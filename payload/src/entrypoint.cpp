using BOOL = int;
using DWORD = unsigned long;
using LPVOID = void*;
using HMODULE = void*;
using LPCWSTR = const wchar_t*;

#ifndef WINAPI
#define WINAPI __stdcall
#endif

#define DLL_PROCESS_ATTACH 1

extern "C" {
__declspec(dllimport) HMODULE WINAPI GetModuleHandleW(LPCWSTR);
__declspec(dllimport) void WINAPI OutputDebugStringW(LPCWSTR);
BOOL WINAPI DllMain(HMODULE, DWORD, LPVOID);
}

// BlackBone calls the PE entry point with the manually mapped image base as
// hModule. That is the correct value for the mapper, but the payload also used
// it as the HINSTANCE for RegisterClassExW/CreateWindowExW. Use the process
// executable instance for the UI bootstrap while preserving the original
// mapped-module value for detach and every non-attach reason.
extern "C" BOOL WINAPI CasPayloadEntryPoint(
    HMODULE mappedModule, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        OutputDebugStringW(L"[cas+] payload entry point reached\n");
        HMODULE processInstance = GetModuleHandleW(nullptr);
        if (processInstance)
        {
            OutputDebugStringW(L"[cas+] starting UI with process HINSTANCE\n");
            return DllMain(processInstance, reason, reserved);
        }
        OutputDebugStringW(L"[cas+] process HINSTANCE lookup failed; using mapped base\n");
    }

    return DllMain(mappedModule, reason, reserved);
}
