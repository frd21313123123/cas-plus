#include <windows.h>
#include <tlhelp32.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <conio.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <thread>
#include <BlackBone/Process/Process.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;

namespace colors
{
    const char* reset = "\033[0m";
    const char* bold = "\033[1m";
    const char* cyan = "\033[36m";
    const char* green = "\033[32m";
    const char* yellow = "\033[33m";
    const char* red = "\033[31m";
    const char* magenta = "\033[35m";
    const char* gray = "\033[90m";
    const char* bright_cyan = "\033[96m";
    const char* bright_white = "\033[97m";
}

void enableAnsiColors()
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD mode = 0;
    if (GetConsoleMode(hOut, &mode))
    {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, mode);
    }
    SetConsoleOutputCP(CP_UTF8);
}

bool enableDebugPrivilege()
{
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return false;

    LUID luid{};
    if (!LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &luid))
    {
        CloseHandle(hToken);
        return false;
    }

    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    BOOL ok = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    CloseHandle(hToken);
    return ok && (GetLastError() != ERROR_NOT_ALL_ASSIGNED);
}

void printBanner()
{
    std::cout << colors::cyan << colors::bold << R"(
  ================================================================
     ____   ___ _____ _  _____ ___    _     ___   _    ____  _____ ____  
    |  _ \ / _ \_   _/ \|_   _/ _ \  | |   / _ \ / \  |  _ \| ____|  _ \ 
    | |_) | | | || |/ _ \ | || | | | | |  | | | / _ \ | | | |  _| | |_) |
    |  __/| |_| || / ___ \| || |_| | | |__| |_|/ ___ \| |_| | |___|  _ < 
    |_|    \___/ |_/_/   \_\_| \___/  |_____\___/_/   \_\____/|_____|_| \_\
  ================================================================
)" << colors::reset;
    std::cout << colors::gray << "   Counter-Strike 2 Auto-Launcher & Payload Injector\n"
              << "   Flags: " << colors::yellow << "-dx11 -insecure -allow_third_party_software -novid -nojoy\n" << colors::reset << "\n";
}

DWORD getProcessIdByName(const std::wstring& processName)
{
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(PROCESSENTRY32W);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    DWORD pid = 0;
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (_wcsicmp(entry.szExeFile, processName.c_str()) == 0)
            {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return pid;
}

std::wstring getSteamExecutablePath()
{
    HKEY hKey;
    std::wstring steamPath;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        wchar_t buffer[MAX_PATH];
        DWORD bufferSize = sizeof(buffer);
        if (RegQueryValueExW(hKey, L"SteamExe", nullptr, nullptr, reinterpret_cast<LPBYTE>(buffer), &bufferSize) == ERROR_SUCCESS)
        {
            steamPath = buffer;
        }
        RegCloseKey(hKey);
    }

    if (steamPath.empty() || !PathFileExistsW(steamPath.c_str()))
    {
        const wchar_t* defaultPaths[] = {
            L"C:\\Program Files (x86)\\Steam\\steam.exe",
            L"C:\\Program Files\\Steam\\steam.exe",
            L"D:\\Steam\\steam.exe",
            L"E:\\Steam\\steam.exe"
        };
        for (const auto* path : defaultPaths)
        {
            if (PathFileExistsW(path))
            {
                steamPath = path;
                break;
            }
        }
    }
    return steamPath;
}

bool launchCS2WithFlags(const std::wstring& steamExe, const std::wstring& launchArgs)
{
    if (!steamExe.empty() && PathFileExistsW(steamExe.c_str()))
    {
        std::wstring commandLine = L"\"" + steamExe + L"\" -applaunch 730 " + launchArgs;
        STARTUPINFOW si{ sizeof(si) };
        PROCESS_INFORMATION pi{};
        std::vector<wchar_t> cmdBuffer(commandLine.begin(), commandLine.end());
        cmdBuffer.push_back(L'\0');

        if (CreateProcessW(nullptr, cmdBuffer.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
        {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return true;
        }
    }

    // Fallback: ShellExecute with steam:// protocol
    std::wstring uri = L"steam://run/730//" + launchArgs;
    auto res = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    return res > 32;
}

fs::path findPayloadDll(const fs::path& baseDir)
{
    std::vector<fs::path> searchDirs = {
        baseDir / "dlls",
        baseDir,
        fs::current_path() / "dlls",
        fs::current_path()
    };

    std::vector<std::string> priorityNames = {
        "cas-plus-payload.dll",
        "PotatoPayload.dll"
    };

    for (const auto& dir : searchDirs)
    {
        if (!fs::exists(dir) || !fs::is_directory(dir))
            continue;

        for (const auto& name : priorityNames)
        {
            fs::path p = dir / name;
            if (fs::exists(p) && fs::is_regular_file(p))
                return p;
        }

        // Return any first dll if priority not found
        for (const auto& entry : fs::directory_iterator(dir))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".dll")
                return entry.path();
        }
    }

    return {};
}

bool readFileBytes(const fs::path& filePath, std::vector<BYTE>& buffer)
{
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    DWORD size = GetFileSize(hFile, nullptr);
    if (size == INVALID_FILE_SIZE || size == 0)
    {
        CloseHandle(hFile);
        return false;
    }

    buffer.resize(size);
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hFile, buffer.data(), size, &bytesRead, nullptr);
    CloseHandle(hFile);
    return ok && bytesRead == size;
}

int main(int argc, char* argv[])
{
    SetConsoleTitleW(L"CAS+ / PotatoLoader - CS2 Auto-Launcher & Injector");
    enableAnsiColors();
    enableDebugPrivilege();
    printBanner();

    wchar_t exePathBuffer[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePathBuffer, MAX_PATH);
    fs::path appDir = fs::path(exePathBuffer).parent_path();

    std::wstring launchArgs = L"-dx11 -insecure -allow_third_party_software -novid -nojoy";

    // Allow custom args if passed
    if (argc > 1)
    {
        launchArgs.clear();
        for (int i = 1; i < argc; ++i)
        {
            std::string argStr = argv[i];
            launchArgs += std::wstring(argStr.begin(), argStr.end()) + L" ";
        }
    }

    std::wcout << colors::cyan << L"[*] Launch parameters: " << colors::bright_white << launchArgs << colors::reset << L"\n";

    // 1. Locate Payload DLL
    std::cout << colors::cyan << "[1/4] " << colors::reset << "Searching for payload DLL in ./dlls ...\n";
    fs::path dllPath = findPayloadDll(appDir);
    if (dllPath.empty())
    {
        std::cout << colors::red << "[ERROR] No payload DLL found in ./dlls directory!\n"
                  << "        Please place cas-plus-payload.dll or PotatoPayload.dll into the dlls folder.\n" << colors::reset;
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    std::wcout << colors::green << L"  [+] Found payload: " << colors::bright_white << dllPath.wstring() << colors::reset << L"\n\n";

    // 2. Check CS2 / Steam Status
    std::cout << colors::cyan << "[2/4] " << colors::reset << "Checking game & Steam status...\n";
    DWORD cs2Pid = getProcessIdByName(L"cs2.exe");
    if (cs2Pid != 0)
    {
        std::cout << colors::green << "  [+] CS2 is already running (PID: " << cs2Pid << "). Skipping launch.\n" << colors::reset;
    }
    else
    {
        std::wstring steamExe = getSteamExecutablePath();
        if (steamExe.empty())
        {
            std::cout << colors::yellow << "  [!] Steam registry path not found. Attempting protocol launch...\n" << colors::reset;
        }
        else
        {
            std::wcout << colors::green << L"  [+] Found Steam: " << steamExe << colors::reset << L"\n";
        }

        std::cout << colors::cyan << "  [*] Launching Counter-Strike 2 with flags (-dx11 -insecure ...)\n" << colors::reset;
        if (!launchCS2WithFlags(steamExe, launchArgs))
        {
            std::cout << colors::red << "[ERROR] Failed to send launch request to Steam.\n" << colors::reset;
            std::cout << "\nPress Enter to exit...";
            std::cin.get();
            return 1;
        }
        std::cout << colors::green << "  [+] Launch command issued successfully.\n" << colors::reset;
    }

    // 3. Wait for cs2.exe and client.dll
    std::cout << "\n" << colors::cyan << "[3/4] " << colors::reset << "Waiting for cs2.exe and client.dll to initialize...\n";

    auto startWait = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(90);

    while (cs2Pid == 0)
    {
        if (std::chrono::steady_clock::now() - startWait > timeout)
        {
            std::cout << colors::red << "\n[ERROR] Timed out waiting for cs2.exe to start.\n" << colors::reset;
            std::cout << "\nPress Enter to exit...";
            std::cin.get();
            return 1;
        }
        std::cout << colors::gray << "." << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        cs2Pid = getProcessIdByName(L"cs2.exe");
    }

    std::cout << "\n" << colors::green << "  [+] cs2.exe active (PID: " << cs2Pid << ")\n" << colors::reset;

    blackbone::Process proc;
    NTSTATUS attachStatus = STATUS_UNSUCCESSFUL;
    auto attachDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(25);

    std::cout << colors::cyan << "  [*] Attaching to cs2.exe..." << colors::reset;
    while (std::chrono::steady_clock::now() < attachDeadline)
    {
        attachStatus = proc.Attach(cs2Pid, DEFAULT_ACCESS_P);
        if (NT_SUCCESS(attachStatus))
            break;

        std::cout << colors::gray << "." << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (!NT_SUCCESS(attachStatus))
    {
        std::cout << "\n" << colors::red << "[ERROR] Failed to attach to cs2.exe (Status: 0x" << std::hex << attachStatus << std::dec << ")\n"
                  << "        Make sure to run loader.exe as Administrator if prompted.\n" << colors::reset;
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }
    std::cout << "\n" << colors::green << "  [+] Successfully attached to cs2.exe\n" << colors::reset;

    bool clientLoaded = false;
    std::cout << colors::cyan << "  [*] Waiting for client.dll..." << colors::reset;

    auto clientWaitStart = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - clientWaitStart < timeout)
    {
        const auto& modules = proc.modules().GetAllModules();
        for (const auto& mod : modules)
        {
            std::wstring modName = mod.first.first;
            std::transform(modName.begin(), modName.end(), modName.begin(), ::towlower);
            if (modName == L"client.dll")
            {
                clientLoaded = true;
                std::cout << "\n" << colors::green << "  [+] client.dll detected at base 0x" << std::hex << mod.second->baseAddress << std::dec << colors::reset << "\n";
                break;
            }
        }
        if (clientLoaded)
            break;

        std::cout << colors::gray << "." << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    if (!clientLoaded)
    {
        std::cout << "\n" << colors::red << "[ERROR] Timed out waiting for client.dll in CS2 process.\n" << colors::reset;
        proc.Detach();
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    std::cout << colors::gray << "  [*] Waiting 2.5s for engine surfaces to settle...\n" << colors::reset;
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    // 4. Inject Payload DLL
    std::cout << "\n" << colors::cyan << "[4/4] " << colors::reset << "Reading and injecting payload...\n";
    std::vector<BYTE> dllBuffer;
    if (!readFileBytes(dllPath, dllBuffer))
    {
        std::cout << colors::red << "[ERROR] Failed to read payload DLL file from disk.\n" << colors::reset;
        proc.Detach();
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    const auto modCallback = [](blackbone::CallbackType type, void*, blackbone::Process&, const blackbone::ModuleData& modInfo)
    {
        if (type == blackbone::PreCallback && modInfo.name == L"user32.dll")
            return blackbone::LoadData(blackbone::MT_Native, blackbone::Ldr_Ignore);
        return blackbone::LoadData(blackbone::MT_Default, blackbone::Ldr_Ignore);
    };

    auto mapResult = proc.mmap().MapImage(dllBuffer.size(), dllBuffer.data(), false, blackbone::NoFlags, modCallback);
    if (!mapResult.success())
    {
        std::cout << colors::red << "[ERROR] MapImage failed with NTSTATUS: 0x" << std::hex << mapResult.status << std::dec << "\n" << colors::reset;
        proc.Detach();
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    proc.Detach();
    MessageBeep(MB_ICONINFORMATION);

    std::cout << "\n" << colors::green << colors::bold << R"(
  ================================================================
    [SUCCESS] Payload injected successfully into Counter-Strike 2!
    - Target Process: cs2.exe (PID: )" << cs2Pid << R"()
    - Injected File : )" << dllPath.filename().string() << R"(
    
    >> Switch to CS2 to use the menu. Press INSERT to toggle! <<
  ================================================================
)" << colors::reset;

    std::cout << colors::gray << "\nAuto-closing in 5 seconds (or press any key to close now)..." << colors::reset << "\n";
    for (int i = 5; i > 0; --i)
    {
        if (_kbhit())
            break;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
