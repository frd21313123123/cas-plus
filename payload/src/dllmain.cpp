// cas+ payload 4.3.0 - x64 manual-map friendly CS2 offline visuals UI.
// INSERT toggles an owned Win32 window. Skybox requests and reversible teammate
// bot highlighting are dispatched to the CS2 game thread; no DirectX hook or
// input automation is used.

using BOOL = int;
using BYTE = unsigned char;
using WORD = unsigned short;
using SHORT = short;
using DWORD = unsigned long;
using LONG = long;
using UINT = unsigned int;
using ULONG_PTR = unsigned long long;
using LONG_PTR = long long;
using SIZE_T = unsigned long long;
using WPARAM = ULONG_PTR;
using LPARAM = LONG_PTR;
using LRESULT = LONG_PTR;
using LPVOID = void*;
using HANDLE = void*;
using HMODULE = void*;
using HINSTANCE = void*;
using HWND = void*;
using HICON = void*;
using HCURSOR = void*;
using HBRUSH = void*;
using HMENU = void*;
using LPCSTR = const char*;
using LPCWSTR = const wchar_t*;
using ATOM = WORD;

#ifndef WINAPI
#define WINAPI __stdcall
#endif
#ifndef CALLBACK
#define CALLBACK __stdcall
#endif

struct POINT { long x; long y; };
struct RECT { long left; long top; long right; long bottom; };
struct MSG {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
    DWORD lPrivate;
};
struct MEMORY_BASIC_INFORMATION {
    LPVOID BaseAddress;
    LPVOID AllocationBase;
    DWORD AllocationProtect;
    WORD PartitionId;
    SIZE_T RegionSize;
    DWORD State;
    DWORD Protect;
    DWORD Type;
};

using WNDPROC = LRESULT (CALLBACK*)(HWND, UINT, WPARAM, LPARAM);
using WNDENUMPROC = BOOL (CALLBACK*)(HWND, LPARAM);
using CreateInterfaceFn = void* (*)(const char*, int*);
using FindMaterialFn = void* (*)(void*, void**, const char*);
using ForceSkyboxUpdateFn = void* (*)(void*);
using AssignStrongHandleFn = void (*)(void*, void*);
using FrameStageNotifyFn = void (*)(void*, int);
using PreCacheFn = void* (*)(void*, void*, const char*);
using FindTypeScopeForModuleFn = void* (*)(void*, const char*, const char**);
using FindDeclaredClassFn = void (*)(void*, void**, const char*);

struct WNDCLASSEXW {
    UINT cbSize;
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCWSTR lpszMenuName;
    LPCWSTR lpszClassName;
    HICON hIconSm;
};

#define DLL_PROCESS_ATTACH 1
#define DLL_PROCESS_DETACH 0
#define VK_INSERT 0x2D

#define WM_CLOSE 0x0010
#define WM_COMMAND 0x0111
#define WM_APP 0x8000
#define WM_POTATO_STATUS (WM_APP + 0x453)
#define WM_CAS_BOT_STATUS (WM_APP + 0x454)
#define PM_REMOVE 0x0001

#define FRAME_RENDER_PASS 12

#define SW_HIDE 0
#define SW_SHOWNA 8

#define WS_POPUP       0x80000000UL
#define WS_CHILD       0x40000000UL
#define WS_VISIBLE     0x10000000UL
#define WS_BORDER      0x00800000UL
#define WS_VSCROLL     0x00200000UL
#define SS_CENTER      0x00000001UL
#define SS_CENTERIMAGE 0x00000200UL
#define CBS_DROPDOWNLIST 0x00000003UL
#define BS_PUSHBUTTON  0x00000000UL
#define BS_AUTOCHECKBOX 0x00000003UL

#define WS_EX_TOOLWINDOW 0x00000080UL
#define WS_EX_NOACTIVATE 0x08000000UL

#define COLOR_BTNFACE 15
#define HWND_TOP ((HWND)0)
#define SWP_NOSIZE       0x0001
#define SWP_NOACTIVATE   0x0010

#define CB_ADDSTRING  0x0143
#define CB_GETCURSEL  0x0147
#define CB_SETCURSEL  0x014E
#define CB_ERR (-1)
#define BN_CLICKED 0
#define BM_GETCHECK 0x00F0
#define BM_SETCHECK 0x00F1
#define BST_UNCHECKED 0
#define BST_CHECKED 1

#define MEM_COMMIT 0x1000
#define PAGE_NOACCESS 0x01
#define PAGE_READWRITE 0x04
#define PAGE_WRITECOPY 0x08
#define PAGE_EXECUTE 0x10
#define PAGE_EXECUTE_READ 0x20
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_EXECUTE_WRITECOPY 0x80
#define PAGE_GUARD 0x100

#ifdef POTATO_DIAGNOSTIC
#define MB_OK 0x00000000UL
#define MB_ICONINFORMATION 0x00000040UL
#endif

extern "C" {
__declspec(dllimport) HANDLE WINAPI CreateThread(LPVOID, unsigned long long, DWORD (WINAPI*)(LPVOID), LPVOID, DWORD, DWORD*);
__declspec(dllimport) BOOL WINAPI CloseHandle(HANDLE);
__declspec(dllimport) BOOL WINAPI DisableThreadLibraryCalls(HMODULE);
__declspec(dllimport) void WINAPI Sleep(DWORD);
__declspec(dllimport) DWORD WINAPI GetCurrentProcessId();
__declspec(dllimport) HMODULE WINAPI GetModuleHandleW(LPCWSTR);
__declspec(dllimport) LPVOID WINAPI GetProcAddress(HMODULE, LPCSTR);
__declspec(dllimport) SIZE_T WINAPI VirtualQuery(const void*, MEMORY_BASIC_INFORMATION*, SIZE_T);
__declspec(dllimport) BOOL WINAPI VirtualProtect(LPVOID, SIZE_T, DWORD, DWORD*);

__declspec(dllimport) ATOM WINAPI RegisterClassExW(const WNDCLASSEXW*);
__declspec(dllimport) HWND WINAPI CreateWindowExW(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
__declspec(dllimport) LRESULT WINAPI DefWindowProcW(HWND, UINT, WPARAM, LPARAM);
__declspec(dllimport) BOOL WINAPI ShowWindow(HWND, int);
__declspec(dllimport) BOOL WINAPI UpdateWindow(HWND);
__declspec(dllimport) SHORT WINAPI GetAsyncKeyState(int);
__declspec(dllimport) BOOL WINAPI PeekMessageW(MSG*, HWND, UINT, UINT, UINT);
__declspec(dllimport) BOOL WINAPI TranslateMessage(const MSG*);
__declspec(dllimport) LRESULT WINAPI DispatchMessageW(const MSG*);
__declspec(dllimport) BOOL WINAPI EnumWindows(WNDENUMPROC, LPARAM);
__declspec(dllimport) DWORD WINAPI GetWindowThreadProcessId(HWND, DWORD*);
__declspec(dllimport) BOOL WINAPI IsWindowVisible(HWND);
__declspec(dllimport) HWND WINAPI GetForegroundWindow();
__declspec(dllimport) BOOL WINAPI GetClientRect(HWND, RECT*);
__declspec(dllimport) BOOL WINAPI ClientToScreen(HWND, POINT*);
__declspec(dllimport) BOOL WINAPI SetWindowPos(HWND, HWND, int, int, int, int, UINT);
__declspec(dllimport) LRESULT WINAPI SendMessageW(HWND, UINT, WPARAM, LPARAM);
__declspec(dllimport) BOOL WINAPI SetWindowTextW(HWND, LPCWSTR);
__declspec(dllimport) BOOL WINAPI PostMessageW(HWND, UINT, WPARAM, LPARAM);
#ifdef POTATO_DIAGNOSTIC
__declspec(dllimport) int WINAPI MessageBoxW(HWND, LPCWSTR, LPCWSTR, UINT);
#endif
}

extern "C" __declspec(dllexport) void* PotatoRelocationAnchor = &PotatoRelocationAnchor;

static volatile bool g_running = true;
static HWND g_gameWindow = nullptr;
static HWND g_menuWindow = nullptr;
static HWND g_skyboxCombo = nullptr;
static HWND g_botHighlightCheck = nullptr;
static HWND g_statusLabel = nullptr;
static bool g_menuRequested = false;
static bool g_menuActuallyShown = false;
static volatile LONG g_pendingSkyboxRequest = 0;
static volatile LONG g_pendingBotHighlightRequest = 0;
static void** g_frameStageVtableSlot = nullptr;
static FrameStageNotifyFn g_originalFrameStageNotify = nullptr;

extern "C" LONG _InterlockedExchange(volatile LONG*, LONG);
#pragma intrinsic(_InterlockedExchange)
extern "C" long long _InterlockedCompareExchange64(
    volatile long long*, long long, long long);
#pragma intrinsic(_InterlockedCompareExchange64)

static LONG AtomicExchange(volatile LONG* destination, LONG value)
{
    return _InterlockedExchange(destination, value);
}

static void* AtomicCompareExchangePointer(void** destination, void* desired,
    void* expected)
{
    const long long previous = _InterlockedCompareExchange64(
        reinterpret_cast<volatile long long*>(destination),
        reinterpret_cast<long long>(desired),
        reinterpret_cast<long long>(expected));
    return reinterpret_cast<void*>(previous);
}

enum ControlId {
    IDC_SKYBOX_COMBO = 1001,
    IDC_APPLY_SKYBOX = 1002,
    IDC_RESTORE_SKYBOX = 1003,
    IDC_BOT_HIGHLIGHT = 1004
};

enum SkyboxResult {
    SKYBOX_APPLIED = 1,
    SKYBOX_RESTORED = 2,
    SKYBOX_ERR_SELECTION = -1,
    SKYBOX_ERR_MATERIAL = -2,
    SKYBOX_ERR_ENTITY_SYSTEM = -3,
    SKYBOX_ERR_REFRESH = -4,
    SKYBOX_ERR_NO_ENTITY = -5,
    SKYBOX_ERR_WRITE = -6,
    SKYBOX_ERR_DISPATCH = -7,
    SKYBOX_ERR_NOTHING_TO_RESTORE = -8
};

enum BotHighlightResult {
    BOT_HIGHLIGHT_ACTIVE = 1,
    BOT_HIGHLIGHT_DISABLED = 2,
    BOT_HIGHLIGHT_WAITING_LOCAL = 3,
    BOT_HIGHLIGHT_WAITING_BOTS = 4,
    BOT_HIGHLIGHT_WAITING_MAP = 5,
    BOT_HIGHLIGHT_RESTORE_PENDING = 6,
    BOT_HIGHLIGHT_ERR_SCHEMA = -1,
    BOT_HIGHLIGHT_ERR_RUNTIME = -2
};

struct EntityRuntime {
    void** entitySystemGlobal;
    unsigned int highestEntityOffset;
};

struct BotHighlightRuntime {
    EntityRuntime entity;
    unsigned int teamOffset;
    unsigned int flagsOffset;
    unsigned int healthOffset;
    unsigned int lifeStateOffset;
    unsigned int localControllerOffset;
    unsigned int playerPawnHandleOffset;
    unsigned int pawnAliveOffset;
    unsigned int controllingBotOffset;
    unsigned int glowOffset;
    unsigned int clientTintOffset;
    unsigned int useClientTintOffset;
    unsigned int glowColorOffset;
    unsigned int glowEligibleOffset;
    unsigned int glowingOffset;
};

struct BotHighlightStats {
    int highlighted;
    int restored;
};

struct OriginalBotHighlight {
    unsigned int pawnHandle;
    void* pawnAddress;
    void* identityAddress;
    void* pawnVtable;
    BYTE glowColor[4];
    BYTE clientTint[4];
    BYTE glowing;
    BYTE eligible;
    BYTE useClientTint;
    bool seen;
};

static BotHighlightRuntime g_botHighlightRuntime{};
static OriginalBotHighlight g_originalBotHighlights[64]{};
static int g_originalBotHighlightCount = 0;
static bool g_botHighlightRuntimeReady = false;
static bool g_botHighlightEnabled = false;
static bool g_botHighlightRestorePending = false;
static unsigned int g_botHighlightFrameCounter = 0;
static int g_lastBotHighlightResult = 0;
static void* g_lastBotEntitySystem = nullptr;
static EntityRuntime g_preResolvedEntityRuntime{};
static bool g_preResolvedEntityRuntimeReady = false;

struct SkyboxRuntime {
    void* entitySystem;
    unsigned int highestEntityOffset;
    unsigned int mainMaterialOffset;
    unsigned int lightingMaterialOffset;
    unsigned int mainCacheOffset;
    unsigned int lightingCacheOffset;
    ForceSkyboxUpdateFn forceUpdate;
    AssignStrongHandleFn assignStrongHandle;
};

struct SkyboxApplyStats {
    int highestEntityIndex;
    int skyEntities;
    int handleWrites;
    int rendererConfirmed;
};

struct SkyboxPreset {
    const wchar_t* name;
    const char* material;
};

static const SkyboxPreset kSkyboxes[] = {
    { L"Cloudy",  "materials/skybox/sky_csgo_cloudy01.vmat" },
    { L"Anubis",  "materials/skybox/sky_de_annubis.vmat" },
    { L"Dust 2",  "materials/skybox/sky_de_dust2.vmat" },
    { L"Mirage",  "materials/skybox/sky_de_mirage.vmat" },
    { L"Nuke",    "materials/skybox/sky_de_nuke.vmat" },
    { L"Overpass","materials/skybox/sky_de_overpass_01.vmat" },
    { L"Train",   "materials/skybox/sky_de_train03.vmat" },
    { L"Vertigo", "materials/skybox/sky_de_vertigo.vmat" },
    { L"Aztec",   "materials/skybox/sky_hr_aztec_02.vmat" },
    { L"Italy",   "materials/skybox/cs_italy_s2_skybox_2.vmat" }
};

struct OriginalSkybox {
    unsigned int entityHandle;
    void* mainMaterial;
    void* lightingMaterial;
    bool seen;
    bool restored;
};

static OriginalSkybox g_originalSkyboxes[32]{};
static int g_originalSkyboxCount = 0;

struct WindowSearchState {
    HWND best;
    unsigned long long bestArea;
    DWORD pid;
};

static bool AsciiEquals(const char* left, const char* right)
{
    if (!left || !right)
        return false;
    for (int i = 0; i < 64; ++i)
    {
        if (left[i] != right[i])
            return false;
        if (left[i] == 0)
            return true;
    }
    return false;
}

static bool IsAccessible(const void* address, SIZE_T bytes, bool write)
{
    if (!address || bytes == 0)
        return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(address, &mbi, sizeof(mbi)) != sizeof(mbi))
        return false;
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0)
        return false;
    const ULONG_PTR start = reinterpret_cast<ULONG_PTR>(address);
    const ULONG_PTR end = reinterpret_cast<ULONG_PTR>(mbi.BaseAddress) + mbi.RegionSize;
    if (start + bytes < start || start + bytes > end)
        return false;
    if (!write)
        return true;
    const DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (mbi.Protect & writable) != 0;
}

static bool IsExecutable(const void* address)
{
    MEMORY_BASIC_INFORMATION mbi{};
    if (!address || VirtualQuery(address, &mbi, sizeof(mbi)) != sizeof(mbi))
        return false;
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0)
        return false;
    const DWORD executable = PAGE_EXECUTE | PAGE_EXECUTE_READ |
        PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (mbi.Protect & executable) != 0;
}

static SIZE_T ModuleImageSize(HMODULE module)
{
    if (!IsAccessible(module, 0x1000, false))
        return 0;
    BYTE* base = reinterpret_cast<BYTE*>(module);
    if (base[0] != 'M' || base[1] != 'Z')
        return 0;
    const unsigned int ntOffset = *reinterpret_cast<unsigned int*>(base + 0x3C);
    if (ntOffset > 0x1000 || !IsAccessible(base + ntOffset, 0x100, false))
        return 0;
    if (*reinterpret_cast<unsigned int*>(base + ntOffset) != 0x00004550u)
        return 0;
    return *reinterpret_cast<unsigned int*>(base + ntOffset + 0x50);
}

static BYTE* FindUniquePattern(HMODULE module, const int* pattern,
    SIZE_T patternLength)
{
    BYTE* base = reinterpret_cast<BYTE*>(module);
    const SIZE_T imageSize = ModuleImageSize(module);
    if (!base || imageSize < patternLength)
        return nullptr;
    BYTE* imageEnd = base + imageSize;
    BYTE* cursor = base;
    BYTE* result = nullptr;
    while (cursor < imageEnd)
    {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(cursor, &mbi, sizeof(mbi)) != sizeof(mbi) ||
            mbi.RegionSize == 0)
            break;
        BYTE* regionStart = cursor;
        BYTE* regionEnd = reinterpret_cast<BYTE*>(mbi.BaseAddress) +
            mbi.RegionSize;
        if (regionEnd > imageEnd)
            regionEnd = imageEnd;
        const bool readable = mbi.State == MEM_COMMIT &&
            (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0;
        if (readable && regionEnd > regionStart &&
            static_cast<SIZE_T>(regionEnd - regionStart) >= patternLength)
        {
            const SIZE_T regionSize =
                static_cast<SIZE_T>(regionEnd - regionStart);
            for (SIZE_T i = 0; i + patternLength <= regionSize; ++i)
            {
                bool matches = true;
                for (SIZE_T j = 0; j < patternLength; ++j)
                {
                    if (pattern[j] >= 0 &&
                        regionStart[i + j] != static_cast<BYTE>(pattern[j]))
                    {
                        matches = false;
                        break;
                    }
                }
                if (!matches)
                    continue;
                if (result)
                    return nullptr;
                result = regionStart + i;
            }
        }
        cursor = regionEnd;
    }
    return result;
}

static BYTE* FindPatternInRange(BYTE* start, SIZE_T rangeLength, const int* pattern, SIZE_T patternLength)
{
    if (!start || rangeLength < patternLength || !IsAccessible(start, rangeLength, false))
        return nullptr;
    for (SIZE_T i = 0; i + patternLength <= rangeLength; ++i)
    {
        bool matches = true;
        for (SIZE_T j = 0; j < patternLength; ++j)
        {
            if (pattern[j] >= 0 && start[i + j] != static_cast<BYTE>(pattern[j]))
            {
                matches = false;
                break;
            }
        }
        if (matches)
            return start + i;
    }
    return nullptr;
}

static bool PatternMatchesAt(BYTE* address, const int* pattern,
    SIZE_T patternLength)
{
    if (!address || !IsAccessible(address, patternLength, false))
        return false;
    for (SIZE_T i = 0; i < patternLength; ++i)
        if (pattern[i] >= 0 && address[i] != static_cast<BYTE>(pattern[i]))
            return false;
    return true;
}

static void SetStatus(const wchar_t* text)
{
    if (g_statusLabel)
        SetWindowTextW(g_statusLabel, text);
}

struct SchemaClassFieldView {
    const char* name;
    void* type;
    int offset;
    int metadataCount;
    void* metadata;
};

struct SchemaClassInfoView {
    void* binding;
    const char* name;
    const char* binaryName;
    const char* moduleName;
    int size;
    short fieldCount;
    short staticMetadataCount;
    BYTE layout[8];
    SchemaClassFieldView* fields;
};

static_assert(sizeof(SchemaClassFieldView) == 0x20,
    "SchemaClassFieldView layout changed");
static_assert(sizeof(SchemaClassInfoView) == 0x38,
    "SchemaClassInfoView layout changed");

static bool ResolveClientSchemaScope(void** scope)
{
    if (!scope)
        return false;
    *scope = nullptr;
    HMODULE schemaModule = GetModuleHandleW(L"schemasystem.dll");
    if (!schemaModule)
        return false;
    auto createInterface = reinterpret_cast<CreateInterfaceFn>(
        GetProcAddress(schemaModule, "CreateInterface"));
    if (!createInterface)
        return false;
    void* schemaSystem = createInterface("SchemaSystem_001", nullptr);
    if (!IsAccessible(schemaSystem, sizeof(void*), false))
        return false;
    void** vtable = *reinterpret_cast<void***>(schemaSystem);
    if (!IsAccessible(vtable, 14 * sizeof(void*), false) ||
        !IsExecutable(vtable[13]))
        return false;
    auto findTypeScope = reinterpret_cast<FindTypeScopeForModuleFn>(vtable[13]);
    void* clientScope = findTypeScope(schemaSystem, "client.dll", nullptr);
    if (!IsAccessible(clientScope, sizeof(void*), false))
        return false;
    void** scopeVtable = *reinterpret_cast<void***>(clientScope);
    if (!IsAccessible(scopeVtable, 3 * sizeof(void*), false) ||
        !IsExecutable(scopeVtable[2]))
        return false;
    *scope = clientScope;
    return true;
}

static SchemaClassInfoView* FindSchemaClass(void* scope,
    const char* className)
{
    if (!scope || !className ||
        !IsAccessible(scope, sizeof(void*), false))
        return nullptr;
    void** vtable = *reinterpret_cast<void***>(scope);
    if (!IsAccessible(vtable, 3 * sizeof(void*), false) ||
        !IsExecutable(vtable[2]))
        return nullptr;

    // The current Source 2 SDK wrapper exposes its one-pointer schema handle
    // through caller-provided return storage on Windows x64.
    void* classInfoRaw = nullptr;
    auto findDeclaredClass = reinterpret_cast<FindDeclaredClassFn>(vtable[2]);
    findDeclaredClass(scope, &classInfoRaw, className);
    auto* classInfo = reinterpret_cast<SchemaClassInfoView*>(classInfoRaw);
    if (!IsAccessible(classInfo, sizeof(SchemaClassInfoView), false) ||
        classInfo->size <= 0 || classInfo->size > 0x20000 ||
        classInfo->fieldCount <= 0 || classInfo->fieldCount > 2048 ||
        !IsAccessible(classInfo->name, 64, false) ||
        !AsciiEquals(classInfo->name, className))
        return nullptr;
    const SIZE_T fieldsSize = static_cast<SIZE_T>(classInfo->fieldCount) *
        sizeof(SchemaClassFieldView);
    if (!IsAccessible(classInfo->fields, fieldsSize, false))
        return nullptr;
    return classInfo;
}

static bool FindSchemaField(const SchemaClassInfoView* classInfo,
    const char* fieldName, unsigned int* offset)
{
    if (!classInfo || !fieldName || !offset)
        return false;
    for (int i = 0; i < classInfo->fieldCount; ++i)
    {
        const SchemaClassFieldView& field = classInfo->fields[i];
        if (!IsAccessible(field.name, 64, false) ||
            !AsciiEquals(field.name, fieldName))
            continue;
        if (field.offset < 0 || field.offset >= classInfo->size)
            return false;
        *offset = static_cast<unsigned int>(field.offset);
        return true;
    }
    return false;
}

static bool ResolveEntityRuntime(HMODULE clientModule, EntityRuntime* runtime)
{
    if (!clientModule || !runtime)
        return false;
    static const int kEntitySystemPattern[] = {
        0x48, 0x8B, 0x1D, -1, -1, -1, -1,
        0x48, 0x89, 0x1D, -1, -1, -1, -1,
        0x4C, 0x63, 0xB3
    };
    static const int kHighestEntityPattern[] = {
        0xFF, 0x81, -1, -1, -1, -1, 0x48, 0x85, 0xD2
    };
    BYTE* entityMatch = FindUniquePattern(clientModule, kEntitySystemPattern,
        sizeof(kEntitySystemPattern) / sizeof(kEntitySystemPattern[0]));
    BYTE* highestMatch = FindUniquePattern(clientModule, kHighestEntityPattern,
        sizeof(kHighestEntityPattern) / sizeof(kHighestEntityPattern[0]));
    if (!entityMatch || !highestMatch)
        return false;
    const int entityRelative = *reinterpret_cast<int*>(entityMatch + 3);
    void** entitySystemGlobal = reinterpret_cast<void**>(
        entityMatch + 7 + entityRelative);
    const unsigned int highestOffset =
        *reinterpret_cast<unsigned int*>(highestMatch + 2);
    if (!IsAccessible(entitySystemGlobal, sizeof(void*), false) ||
        highestOffset < 0x100 || highestOffset > 0x10000)
        return false;
    runtime->entitySystemGlobal = entitySystemGlobal;
    runtime->highestEntityOffset = highestOffset;
    return true;
}

static void* CurrentEntitySystem(const EntityRuntime& runtime)
{
    if (!runtime.entitySystemGlobal ||
        !IsAccessible(runtime.entitySystemGlobal, sizeof(void*), false))
        return nullptr;
    void* entitySystem = *runtime.entitySystemGlobal;
    if (!IsAccessible(reinterpret_cast<BYTE*>(entitySystem) +
        runtime.highestEntityOffset, sizeof(int), false))
        return nullptr;
    return entitySystem;
}

static void* EntityAtIndex(void* entitySystem, int index)
{
    if (!entitySystem || index < 0 || index > 32768)
        return nullptr;
    BYTE* chunkAddress = reinterpret_cast<BYTE*>(entitySystem) + 0x10 +
        8ull * static_cast<unsigned int>(index >> 9);
    if (!IsAccessible(chunkAddress, sizeof(void*), false))
        return nullptr;
    void* chunk = *reinterpret_cast<void**>(chunkAddress);
    const SIZE_T entityOffset = 0x78ull *
        static_cast<unsigned int>(index & 0x1FF);
    BYTE* entityAddress = reinterpret_cast<BYTE*>(chunk) + entityOffset;
    if (!IsAccessible(entityAddress, sizeof(void*), false))
        return nullptr;
    return *reinterpret_cast<void**>(entityAddress);
}

static unsigned int AsciiLength(const char* text, unsigned int limit)
{
    if (!text)
        return 0;
    unsigned int length = 0;
    while (length < limit && text[length])
        ++length;
    return length;
}

static unsigned int LoadU32(const BYTE* bytes)
{
    return static_cast<unsigned int>(bytes[0]) |
        (static_cast<unsigned int>(bytes[1]) << 8) |
        (static_cast<unsigned int>(bytes[2]) << 16) |
        (static_cast<unsigned int>(bytes[3]) << 24);
}

static unsigned long long MurmurHash64B(const void* key, int length,
    unsigned long long seed)
{
    constexpr unsigned int kMultiplier = 0x5BD1E995u;
    constexpr int kShift = 24;
    unsigned int h1 = static_cast<unsigned int>(seed) ^
        static_cast<unsigned int>(length);
    unsigned int h2 = static_cast<unsigned int>(seed >> 32);
    const BYTE* data = reinterpret_cast<const BYTE*>(key);

    while (length >= 8)
    {
        unsigned int k1 = LoadU32(data);
        data += 4;
        k1 *= kMultiplier;
        k1 ^= k1 >> kShift;
        k1 *= kMultiplier;
        h1 *= kMultiplier;
        h1 ^= k1;
        length -= 4;

        unsigned int k2 = LoadU32(data);
        data += 4;
        k2 *= kMultiplier;
        k2 ^= k2 >> kShift;
        k2 *= kMultiplier;
        h2 *= kMultiplier;
        h2 ^= k2;
        length -= 4;
    }

    if (length >= 4)
    {
        unsigned int k1 = LoadU32(data);
        data += 4;
        k1 *= kMultiplier;
        k1 ^= k1 >> kShift;
        k1 *= kMultiplier;
        h1 *= kMultiplier;
        h1 ^= k1;
        length -= 4;
    }

    if (length == 3)
        h2 ^= static_cast<unsigned int>(data[2]) << 16;
    if (length >= 2)
        h2 ^= static_cast<unsigned int>(data[1]) << 8;
    if (length >= 1)
    {
        h2 ^= static_cast<unsigned int>(data[0]);
        h2 *= kMultiplier;
    }

    h1 ^= h2 >> 18;
    h1 *= kMultiplier;
    h2 ^= h1 >> 22;
    h2 *= kMultiplier;
    h1 ^= h2 >> 17;
    h1 *= kMultiplier;
    h2 ^= h1 >> 19;
    h2 *= kMultiplier;
    return (static_cast<unsigned long long>(h1) << 32) | h2;
}

struct ResourceNameWrapper {
    int length;
    int allocatedSize;
    char* data;
    BYTE padding[0xC0];
    unsigned long long hash;
    char extension[8];
};

static_assert(sizeof(ResourceNameWrapper) == 0xE0,
    "ResourceNameWrapper must match Source 2 CBufferStringWrapper");

static void ZeroBytes(void* destination, SIZE_T count)
{
    volatile BYTE* bytes = reinterpret_cast<volatile BYTE*>(destination);
    for (SIZE_T i = 0; i < count; ++i)
        bytes[i] = 0;
}

static bool IsMaterialHandle(void* handle)
{
    if (!IsAccessible(handle, 0x24, true))
        return false;
    void* material = *reinterpret_cast<void**>(handle);
    return IsAccessible(material, sizeof(void*), false);
}

static void* PreCacheSkyMaterial(const char* materialPath)
{
    const unsigned int sourceLength = AsciiLength(materialPath, 127);
    if (!sourceLength || sourceLength >= 127)
        return nullptr;

    char normalized[128];
    ZeroBytes(normalized, sizeof(normalized));
    int lastDot = -1;
    for (unsigned int i = 0; i < sourceLength; ++i)
    {
        char value = materialPath[i];
        if (value >= 'A' && value <= 'Z')
            value = static_cast<char>(value + ('a' - 'A'));
        if (value == '\\')
            value = '/';
        if (value == '.')
            lastDot = static_cast<int>(i);
        normalized[i] = value;
    }
    normalized[sourceLength] = 0;

    ResourceNameWrapper wrapper;
    ZeroBytes(&wrapper, sizeof(wrapper));
    wrapper.length = static_cast<int>(sourceLength);
    wrapper.allocatedSize = static_cast<int>(sizeof(normalized));
    wrapper.data = normalized;
    wrapper.hash = MurmurHash64B(normalized, static_cast<int>(sourceLength),
        0x00000000EDABCDEFULL);
    if (lastDot >= 0)
    {
        unsigned int destination = 0;
        for (unsigned int source = static_cast<unsigned int>(lastDot + 1);
            source < sourceLength && destination < 7; ++source)
            wrapper.extension[destination++] = normalized[source];
    }

    HMODULE resourceModule = GetModuleHandleW(L"resourcesystem.dll");
    if (!resourceModule)
        return nullptr;
    auto createInterface = reinterpret_cast<CreateInterfaceFn>(
        GetProcAddress(resourceModule, "CreateInterface"));
    if (!createInterface)
        return nullptr;
    void* resourceSystem = createInterface("ResourceSystem013", nullptr);
    if (!IsAccessible(resourceSystem, sizeof(void*), false))
        return nullptr;
    void** vtable = *reinterpret_cast<void***>(resourceSystem);
    if (!IsAccessible(vtable, 41 * sizeof(void*), false) ||
        !IsExecutable(vtable[40]))
        return nullptr;

    auto preCache = reinterpret_cast<PreCacheFn>(vtable[40]);
    void* handle = preCache(resourceSystem, &wrapper, "");
    return IsMaterialHandle(handle) ? handle : nullptr;
}

static void* ResolveSkyMaterial(const char* materialPath)
{
    void* precachedHandle = PreCacheSkyMaterial(materialPath);
    if (!precachedHandle)
        return nullptr;

    HMODULE materialModule = GetModuleHandleW(L"materialsystem2.dll");
    if (!materialModule)
        return nullptr;
    auto createInterface = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(materialModule, "CreateInterface"));
    if (!createInterface)
        return nullptr;

    void* materialSystem = createInterface("VMaterialSystem2_001", nullptr);
    if (!IsAccessible(materialSystem, sizeof(void*), false))
        return nullptr;

    void** vtable = *reinterpret_cast<void***>(materialSystem);
    if (!IsAccessible(vtable, 15 * sizeof(void*), false) ||
        !IsExecutable(vtable[14]))
        return nullptr;
    auto findMaterial = reinterpret_cast<FindMaterialFn>(vtable[14]);

    void* outMaterialHandle = nullptr;
    findMaterial(materialSystem, &outMaterialHandle, materialPath);
    if (IsMaterialHandle(outMaterialHandle))
        return outMaterialHandle;
    return precachedHandle;
}

static bool IsEnvSky(void* entity)
{
    if (!IsAccessible(entity, 0x18, false))
        return false;
    void* identity = *reinterpret_cast<void**>(reinterpret_cast<BYTE*>(entity) + 0x10);
    if (!IsAccessible(identity, 0x28, false))
        return false;
    const char* designerName = *reinterpret_cast<const char**>(reinterpret_cast<BYTE*>(identity) + 0x20);
    if (!IsAccessible(designerName, 8, false))
        return false;
    return AsciiEquals(designerName, "env_sky");
}

static unsigned int EntityHandleFor(void* entity)
{
    if (!IsAccessible(entity, 0x18, false))
        return 0xFFFFFFFFu;
    void* identity = *reinterpret_cast<void**>(reinterpret_cast<BYTE*>(entity) + 0x10);
    if (!IsAccessible(identity, 0x14, false))
        return 0xFFFFFFFFu;
    return *reinterpret_cast<unsigned int*>(reinterpret_cast<BYTE*>(identity) + 0x10);
}

static bool HasDesignerName(void* entity, const char* expected)
{
    if (!entity || !expected || !IsAccessible(entity, 0x18, false))
        return false;
    void* identity = *reinterpret_cast<void**>(
        reinterpret_cast<BYTE*>(entity) + 0x10);
    if (!IsAccessible(identity, 0x28, false))
        return false;
    const char* designerName = *reinterpret_cast<const char**>(
        reinterpret_cast<BYTE*>(identity) + 0x20);
    return IsAccessible(designerName, 64, false) &&
        AsciiEquals(designerName, expected);
}

static void* EntityFromHandle(void* entitySystem, unsigned int handle)
{
    constexpr unsigned int kEntityIndexMask = 0x7FFFu;
    if (!entitySystem || handle == 0 || handle == 0xFFFFFFFFu)
        return nullptr;
    const unsigned int index = handle & kEntityIndexMask;
    if (index == kEntityIndexMask || index > 32768u)
        return nullptr;
    void* entity = EntityAtIndex(entitySystem, static_cast<int>(index));
    if (!entity || EntityHandleFor(entity) != handle)
        return nullptr;
    return entity;
}

enum EntityHandleResolveResult {
    ENTITY_HANDLE_RESOLVED = 1,
    ENTITY_HANDLE_STALE = 0,
    ENTITY_HANDLE_RETRY = -1
};

static int ResolveEntityHandle(void* entitySystem, unsigned int handle,
    void** resolved)
{
    constexpr unsigned int kEntityIndexMask = 0x7FFFu;
    if (resolved)
        *resolved = nullptr;
    if (!entitySystem || !resolved)
        return ENTITY_HANDLE_RETRY;
    if (handle == 0 || handle == 0xFFFFFFFFu)
        return ENTITY_HANDLE_STALE;
    const unsigned int index = handle & kEntityIndexMask;
    if (index == kEntityIndexMask || index > 32768u)
        return ENTITY_HANDLE_STALE;
    BYTE* chunkAddress = reinterpret_cast<BYTE*>(entitySystem) + 0x10 +
        8ull * static_cast<unsigned int>(index >> 9);
    if (!IsAccessible(chunkAddress, sizeof(void*), false))
        return ENTITY_HANDLE_RETRY;
    void* chunk = *reinterpret_cast<void**>(chunkAddress);
    if (!chunk)
        return ENTITY_HANDLE_STALE;
    BYTE* entityAddress = reinterpret_cast<BYTE*>(chunk) + 0x78ull *
        static_cast<unsigned int>(index & 0x1FF);
    if (!IsAccessible(entityAddress, sizeof(void*), false))
        return ENTITY_HANDLE_RETRY;
    void* entity = *reinterpret_cast<void**>(entityAddress);
    if (!entity)
        return ENTITY_HANDLE_STALE;
    const unsigned int currentHandle = EntityHandleFor(entity);
    if (currentHandle == 0xFFFFFFFFu)
        return ENTITY_HANDLE_RETRY;
    if (currentHandle != handle)
        return ENTITY_HANDLE_STALE;
    *resolved = entity;
    return ENTITY_HANDLE_RESOLVED;
}

struct StrongHandleAssignmentTarget {
    BYTE padding[0xD0];
    void* handle;
};

static_assert(sizeof(StrongHandleAssignmentTarget) == 0xD8,
    "Strong-handle assignment target layout changed");

static void InitializeAssignmentTarget(StrongHandleAssignmentTarget* target,
    void* handle)
{
    target->handle = handle;
}

static bool AssignStrongHandle(const SkyboxRuntime& runtime, void** destination,
    void* value)
{
    if (!runtime.assignStrongHandle ||
        !IsExecutable(reinterpret_cast<void*>(runtime.assignStrongHandle)) ||
        !IsAccessible(destination, sizeof(void*), true) ||
        (*destination && !IsMaterialHandle(*destination)) ||
        (value && !IsMaterialHandle(value)))
        return false;

    StrongHandleAssignmentTarget target;
    InitializeAssignmentTarget(&target, *destination);
    runtime.assignStrongHandle(&target, value);
    if (target.handle != value)
        return false;
    *destination = target.handle;
    return *destination == value;
}

static bool RetainStrongHandle(const SkyboxRuntime& runtime, void* value)
{
    if (!value)
        return true;
    if (!runtime.assignStrongHandle || !IsMaterialHandle(value))
        return false;
    StrongHandleAssignmentTarget target;
    InitializeAssignmentTarget(&target, nullptr);
    runtime.assignStrongHandle(&target, value);
    return target.handle == value;
}

static void ReleaseStrongHandle(const SkyboxRuntime& runtime, void* value)
{
    if (!value || !runtime.assignStrongHandle)
        return;
    StrongHandleAssignmentTarget target;
    InitializeAssignmentTarget(&target, value);
    runtime.assignStrongHandle(&target, nullptr);
}

static OriginalSkybox* FindOriginalSkybox(unsigned int entityHandle)
{
    for (int i = 0; i < g_originalSkyboxCount; ++i)
        if (g_originalSkyboxes[i].entityHandle == entityHandle)
            return &g_originalSkyboxes[i];
    return nullptr;
}

static bool RememberOriginal(const SkyboxRuntime& runtime,
    unsigned int entityHandle, void* mainMaterial, void* lightingMaterial)
{
    if (FindOriginalSkybox(entityHandle))
        return true;
    if (entityHandle == 0xFFFFFFFFu ||
        g_originalSkyboxCount >= static_cast<int>(
            sizeof(g_originalSkyboxes) / sizeof(g_originalSkyboxes[0])))
        return false;
    if (!RetainStrongHandle(runtime, mainMaterial))
        return false;
    if (!RetainStrongHandle(runtime, lightingMaterial))
    {
        ReleaseStrongHandle(runtime, mainMaterial);
        return false;
    }
    g_originalSkyboxes[g_originalSkyboxCount++] = {
        entityHandle, mainMaterial, lightingMaterial, true, false
    };
    return true;
}

static bool RendererCacheMatches(void** cacheSlot, void* materialHandle)
{
    if (!IsAccessible(cacheSlot, sizeof(void*), false))
        return false;
    void* cache = *cacheSlot;
    if (!materialHandle)
        return cache == nullptr;
    if (!IsAccessible(cache, 0xD8, false))
        return false;
    return *reinterpret_cast<void**>(
        reinterpret_cast<BYTE*>(cache) + 0xD0) == materialHandle;
}

static bool ResolveSkyboxRuntime(HMODULE clientModule, SkyboxRuntime* runtime)
{
    if (!clientModule || !runtime)
        return false;

    // The entity-system signatures are the same relationships used by the
    // current cs2-dumper. Resolving them at runtime avoids a build-specific RVA.
    static const int kEntitySystemPattern[] = {
        0x48, 0x8B, 0x1D, -1, -1, -1, -1,
        0x48, 0x89, 0x1D, -1, -1, -1, -1,
        0x4C, 0x63, 0xB3
    };
    static const int kHighestEntityPattern[] = {
        0xFF, 0x81, -1, -1, -1, -1, 0x48, 0x85, 0xD2
    };
    static const int kForcePattern[] = {
        0x33, 0xDB, 0x48, 0x8D, 0x05, -1, -1, -1, -1,
        0x48, 0x8B, 0xCF, 0x48, 0x89, 0x44, 0x24, -1
    };
    static const int kMainHandlePattern[] = {
        0x48, 0x83, 0xB9, -1, -1, -1, -1, 0x00, 0x48, 0x8B, 0xD9
    };
    static const int kLightingHandlePattern[] = {
        0x48, 0x83, 0xBB, -1, -1, -1, -1, 0x00, 0x0F, 0x84
    };
    static const int kMainCachePattern[] = {
        0x48, 0x8D, 0xB3, -1, -1, -1, -1, 0x48, 0x8B, 0x0E
    };
    static const int kLightingCachePattern[] = {
        0x48, 0x8B, 0x8B, -1, -1, -1, -1, 0x48, 0x85, 0xC9, 0x75, -1
    };
    static const int kAssignStrongHandlePattern[] = {
        0x48, 0x8B, 0x93, -1, -1, -1, -1,
        0xE8, -1, -1, -1, -1, 0x48, 0x8B, 0x3E
    };
    static const int kAssignStrongHandlePrologue[] = {
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
        0xEC, 0x20, 0x48, 0x8B, 0xDA, 0x48, 0x8B, 0xF9,
        0x48, 0x8B, 0x91, 0xD0, 0x00, 0x00, 0x00, 0x48,
        0x85, 0xDB
    };

    BYTE* entityMatch = FindUniquePattern(clientModule, kEntitySystemPattern,
        sizeof(kEntitySystemPattern) / sizeof(kEntitySystemPattern[0]));
    BYTE* highestMatch = FindUniquePattern(clientModule, kHighestEntityPattern,
        sizeof(kHighestEntityPattern) / sizeof(kHighestEntityPattern[0]));
    BYTE* forceMatch = FindUniquePattern(clientModule, kForcePattern,
        sizeof(kForcePattern) / sizeof(kForcePattern[0]));
    if (!entityMatch || !highestMatch || !forceMatch)
        return false;

    const int entityRelative = *reinterpret_cast<int*>(entityMatch + 3);
    void** entitySystemGlobal = reinterpret_cast<void**>(entityMatch + 7 + entityRelative);
    if (!IsAccessible(entitySystemGlobal, sizeof(void*), false))
        return false;
    void* entitySystem = *entitySystemGlobal;

    const unsigned int highestOffset = *reinterpret_cast<unsigned int*>(highestMatch + 2);
    const int forceRelative = *reinterpret_cast<int*>(forceMatch + 5);
    BYTE* forceTarget = forceMatch + 9 + forceRelative;
    if (!IsExecutable(forceTarget))
        return false;

    BYTE* mainHandleMatch = FindPatternInRange(forceTarget, 0x100,
        kMainHandlePattern, sizeof(kMainHandlePattern) / sizeof(kMainHandlePattern[0]));
    BYTE* lightingHandleMatch = FindPatternInRange(forceTarget, 0x900,
        kLightingHandlePattern, sizeof(kLightingHandlePattern) / sizeof(kLightingHandlePattern[0]));
    BYTE* mainCacheMatch = FindPatternInRange(forceTarget, 0x900,
        kMainCachePattern, sizeof(kMainCachePattern) / sizeof(kMainCachePattern[0]));
    BYTE* lightingCacheMatch = FindPatternInRange(forceTarget, 0x900,
        kLightingCachePattern, sizeof(kLightingCachePattern) / sizeof(kLightingCachePattern[0]));
    BYTE* assignStrongHandleMatch = FindPatternInRange(forceTarget, 0x300,
        kAssignStrongHandlePattern,
        sizeof(kAssignStrongHandlePattern) / sizeof(kAssignStrongHandlePattern[0]));
    if (!mainHandleMatch || !lightingHandleMatch || !mainCacheMatch ||
        !lightingCacheMatch || !assignStrongHandleMatch)
        return false;

    const unsigned int mainOffset = *reinterpret_cast<unsigned int*>(mainHandleMatch + 3);
    const unsigned int lightingOffset = *reinterpret_cast<unsigned int*>(lightingHandleMatch + 3);
    const unsigned int mainCacheOffset = *reinterpret_cast<unsigned int*>(mainCacheMatch + 3);
    const unsigned int lightingCacheOffset = *reinterpret_cast<unsigned int*>(lightingCacheMatch + 3);
    const unsigned int assignmentSourceOffset =
        *reinterpret_cast<unsigned int*>(assignStrongHandleMatch + 3);
    const int assignmentRelative = *reinterpret_cast<int*>(assignStrongHandleMatch + 8);
    BYTE* assignmentTarget = assignStrongHandleMatch + 12 + assignmentRelative;

    if (!IsAccessible(entitySystem, highestOffset + sizeof(int), false) ||
        highestOffset < 0x100 || highestOffset > 0x10000 ||
        mainOffset < 0x100 || mainOffset > 0x5000 ||
        lightingOffset != mainOffset + sizeof(void*) ||
        mainCacheOffset <= lightingOffset || mainCacheOffset > 0x5000 ||
        lightingCacheOffset != mainCacheOffset + sizeof(void*) ||
        assignmentSourceOffset != mainOffset || !IsExecutable(assignmentTarget) ||
        !PatternMatchesAt(assignmentTarget, kAssignStrongHandlePrologue,
            sizeof(kAssignStrongHandlePrologue) /
                sizeof(kAssignStrongHandlePrologue[0])))
        return false;

    runtime->entitySystem = entitySystem;
    runtime->highestEntityOffset = highestOffset;
    runtime->mainMaterialOffset = mainOffset;
    runtime->lightingMaterialOffset = lightingOffset;
    runtime->mainCacheOffset = mainCacheOffset;
    runtime->lightingCacheOffset = lightingCacheOffset;
    runtime->forceUpdate = reinterpret_cast<ForceSkyboxUpdateFn>(forceTarget);
    runtime->assignStrongHandle =
        reinterpret_cast<AssignStrongHandleFn>(assignmentTarget);
    return true;
}

static int ApplyToEnvSky(bool restore, void* newMaterial, SkyboxApplyStats* stats)
{
    if (stats)
    {
        stats->highestEntityIndex = 0;
        stats->skyEntities = 0;
        stats->handleWrites = 0;
        stats->rendererConfirmed = 0;
    }
    HMODULE clientModule = GetModuleHandleW(L"client.dll");
    if (!clientModule)
        return SKYBOX_ERR_ENTITY_SYSTEM;

    SkyboxRuntime runtime;
    runtime.entitySystem = nullptr;
    runtime.highestEntityOffset = 0;
    runtime.mainMaterialOffset = 0;
    runtime.lightingMaterialOffset = 0;
    runtime.mainCacheOffset = 0;
    runtime.lightingCacheOffset = 0;
    runtime.forceUpdate = nullptr;
    runtime.assignStrongHandle = nullptr;
    if (!ResolveSkyboxRuntime(clientModule, &runtime))
        return SKYBOX_ERR_REFRESH;

    const int highest = *reinterpret_cast<int*>(
        reinterpret_cast<BYTE*>(runtime.entitySystem) + runtime.highestEntityOffset);
    if (highest < 0 || highest > 32768)
        return SKYBOX_ERR_ENTITY_SYSTEM;
    if (stats)
        stats->highestEntityIndex = highest;

    for (int i = 0; i < g_originalSkyboxCount; ++i)
    {
        g_originalSkyboxes[i].seen = false;
        g_originalSkyboxes[i].restored = false;
    }

    int matchedOriginals = 0;
    for (int index = 0; index <= highest; ++index)
    {
        BYTE* chunkAddress = reinterpret_cast<BYTE*>(runtime.entitySystem) + 0x10 +
            8ull * static_cast<unsigned int>(index >> 9);
        if (!IsAccessible(chunkAddress, sizeof(void*), false))
            continue;
        void* chunk = *reinterpret_cast<void**>(chunkAddress);
        const SIZE_T entityOffset = 0x78ull * static_cast<unsigned int>(index & 0x1FF);
        if (!IsAccessible(reinterpret_cast<BYTE*>(chunk) + entityOffset, sizeof(void*), false))
            continue;
        void* entity = *reinterpret_cast<void**>(reinterpret_cast<BYTE*>(chunk) + entityOffset);
        if (!IsEnvSky(entity))
            continue;
        const unsigned int entityHandle = EntityHandleFor(entity);
        if (entityHandle == 0xFFFFFFFFu)
            continue;
        if (stats)
            ++stats->skyEntities;

        void** mainSlot = reinterpret_cast<void**>(
            reinterpret_cast<BYTE*>(entity) + runtime.mainMaterialOffset);
        void** lightingSlot = reinterpret_cast<void**>(
            reinterpret_cast<BYTE*>(entity) + runtime.lightingMaterialOffset);
        if (!IsAccessible(mainSlot, sizeof(void*), true) ||
            !IsAccessible(lightingSlot, sizeof(void*), true))
            continue;

        OriginalSkybox* original = FindOriginalSkybox(entityHandle);
        if (original)
            original->seen = true;

        void* mainValue = newMaterial;
        void* lightingValue = newMaterial;
        if (restore)
        {
            if (!original)
                continue;
            ++matchedOriginals;
            mainValue = original->mainMaterial;
            lightingValue = original->lightingMaterial;
        }
        else
        {
            if (!original)
            {
                if (!RememberOriginal(runtime, entityHandle,
                    *mainSlot, *lightingSlot))
                    continue;
                original = FindOriginalSkybox(entityHandle);
                if (!original)
                    continue;
                original->seen = true;
            }
        }

        // Use the same ref-counted strong-handle assignment helper that the
        // current renderer uses for its cached material at +0xD0. A raw pointer
        // write can leave CStrongHandle ownership inconsistent across map loads.
        void* previousMain = *mainSlot;
        void* previousLighting = *lightingSlot;
        if (!RetainStrongHandle(runtime, previousMain))
            continue;
        if (!RetainStrongHandle(runtime, previousLighting))
        {
            ReleaseStrongHandle(runtime, previousMain);
            continue;
        }
        if (!AssignStrongHandle(runtime, mainSlot, mainValue))
        {
            ReleaseStrongHandle(runtime, previousMain);
            ReleaseStrongHandle(runtime, previousLighting);
            continue;
        }
        if (!AssignStrongHandle(runtime, lightingSlot, lightingValue))
        {
            AssignStrongHandle(runtime, mainSlot, previousMain);
            ReleaseStrongHandle(runtime, previousMain);
            ReleaseStrongHandle(runtime, previousLighting);
            continue;
        }
        ReleaseStrongHandle(runtime, previousMain);
        ReleaseStrongHandle(runtime, previousLighting);

        // Current ForceUpdateSkybox has an in-place update path for both cache
        // objects. Do not clear either pointer: clearing bypasses the release
        // path and was responsible for the old hitch/freeze-prone behavior.
        runtime.forceUpdate(entity);

        if (*mainSlot == mainValue && *lightingSlot == lightingValue)
        {
            if (stats)
                ++stats->handleWrites;
            void** mainCache = reinterpret_cast<void**>(
                reinterpret_cast<BYTE*>(entity) + runtime.mainCacheOffset);
            void** lightingCache = reinterpret_cast<void**>(
                reinterpret_cast<BYTE*>(entity) + runtime.lightingCacheOffset);
            if (RendererCacheMatches(mainCache, mainValue) &&
                RendererCacheMatches(lightingCache, lightingValue))
            {
                if (stats)
                    ++stats->rendererConfirmed;
                if (restore && original)
                    original->restored = true;
            }
        }
    }

    // Retained original handles survive while an override is active. Release
    // them only after a confirmed restore, or when the full entity handle
    // (index + serial) is no longer present after a map transition.
    int destination = 0;
    for (int i = 0; i < g_originalSkyboxCount; ++i)
    {
        OriginalSkybox& original = g_originalSkyboxes[i];
        if (!original.seen || original.restored)
        {
            ReleaseStrongHandle(runtime, original.mainMaterial);
            ReleaseStrongHandle(runtime, original.lightingMaterial);
            continue;
        }
        g_originalSkyboxes[destination++] = original;
    }
    g_originalSkyboxCount = destination;

    if (!stats || stats->skyEntities == 0)
        return SKYBOX_ERR_NO_ENTITY;
    if (restore && matchedOriginals == 0)
        return SKYBOX_ERR_NOTHING_TO_RESTORE;
    if (stats->handleWrites == 0 || stats->rendererConfirmed == 0)
        return SKYBOX_ERR_WRITE;
    if (restore)
        return SKYBOX_RESTORED;
    return SKYBOX_APPLIED;
}

static int ApplySkyboxOnGameThread(int presetIndex, SkyboxApplyStats* stats)
{
    if (presetIndex < 0 ||
        presetIndex >= static_cast<int>(sizeof(kSkyboxes) / sizeof(kSkyboxes[0])))
        return SKYBOX_ERR_SELECTION;
    void* material = ResolveSkyMaterial(kSkyboxes[presetIndex].material);
    if (!material)
        return SKYBOX_ERR_MATERIAL;
    return ApplyToEnvSky(false, material, stats);
}

static int RestoreSkyboxOnGameThread(SkyboxApplyStats* stats)
{
    if (g_originalSkyboxCount == 0)
        return SKYBOX_ERR_NOTHING_TO_RESTORE;
    return ApplyToEnvSky(true, nullptr, stats);
}

static bool ResolveBotHighlightRuntime(BotHighlightRuntime* runtime)
{
    if (!runtime)
        return false;
    void* scope = nullptr;
    if (!g_preResolvedEntityRuntimeReady ||
        !g_preResolvedEntityRuntime.entitySystemGlobal ||
        !ResolveClientSchemaScope(&scope))
        return false;
    runtime->entity = g_preResolvedEntityRuntime;

    SchemaClassInfoView* baseEntity = FindSchemaClass(scope, "C_BaseEntity");
    SchemaClassInfoView* baseController = FindSchemaClass(scope,
        "CBasePlayerController");
    SchemaClassInfoView* playerController = FindSchemaClass(scope,
        "CCSPlayerController");
    SchemaClassInfoView* baseModelEntity = FindSchemaClass(scope,
        "C_BaseModelEntity");
    SchemaClassInfoView* glowProperty = FindSchemaClass(scope,
        "CGlowProperty");
    if (!baseEntity || !baseController || !playerController ||
        !baseModelEntity || !glowProperty)
        return false;

    if (!FindSchemaField(baseEntity, "m_iTeamNum", &runtime->teamOffset) ||
        !FindSchemaField(baseEntity, "m_fFlags", &runtime->flagsOffset) ||
        !FindSchemaField(baseEntity, "m_iHealth", &runtime->healthOffset) ||
        !FindSchemaField(baseEntity, "m_lifeState", &runtime->lifeStateOffset) ||
        !FindSchemaField(baseController, "m_bIsLocalPlayerController",
            &runtime->localControllerOffset) ||
        !FindSchemaField(playerController, "m_hPlayerPawn",
            &runtime->playerPawnHandleOffset) ||
        !FindSchemaField(playerController, "m_bPawnIsAlive",
            &runtime->pawnAliveOffset) ||
        !FindSchemaField(playerController, "m_bControllingBot",
            &runtime->controllingBotOffset) ||
        !FindSchemaField(baseModelEntity, "m_Glow", &runtime->glowOffset) ||
        !FindSchemaField(baseModelEntity, "m_ClientOverrideTint",
            &runtime->clientTintOffset) ||
        !FindSchemaField(baseModelEntity, "m_bUseClientOverrideTint",
            &runtime->useClientTintOffset) ||
        !FindSchemaField(glowProperty, "m_glowColorOverride",
            &runtime->glowColorOffset) ||
        !FindSchemaField(glowProperty, "m_bEligibleForScreenHighlight",
            &runtime->glowEligibleOffset) ||
        !FindSchemaField(glowProperty, "m_bGlowing",
            &runtime->glowingOffset))
        return false;

    // Cross-check the reflected relationships before any write. The exact
    // offsets may move, but these field sizes/orderings are structural.
    if (runtime->teamOffset >= runtime->flagsOffset ||
        runtime->healthOffset >= runtime->lifeStateOffset ||
        (runtime->healthOffset & 3u) != 0 ||
        (runtime->flagsOffset & 3u) != 0 ||
        runtime->localControllerOffset >= runtime->playerPawnHandleOffset ||
        (runtime->playerPawnHandleOffset & 3u) != 0 ||
        runtime->playerPawnHandleOffset + sizeof(unsigned int) >
            runtime->pawnAliveOffset ||
        runtime->glowColorOffset + 4 > runtime->glowEligibleOffset ||
        runtime->glowingOffset != runtime->glowEligibleOffset + 1 ||
        runtime->clientTintOffset + 4 > runtime->useClientTintOffset ||
        runtime->playerPawnHandleOffset + sizeof(unsigned int) >
            static_cast<unsigned int>(playerController->size) ||
        runtime->pawnAliveOffset >=
            static_cast<unsigned int>(playerController->size) ||
        runtime->controllingBotOffset >=
            static_cast<unsigned int>(playerController->size) ||
        runtime->glowOffset + runtime->glowingOffset >=
            static_cast<unsigned int>(baseModelEntity->size) ||
        runtime->clientTintOffset + 4 >
            static_cast<unsigned int>(baseModelEntity->size) ||
        runtime->useClientTintOffset >=
            static_cast<unsigned int>(baseModelEntity->size))
        return false;
    return true;
}

static OriginalBotHighlight* FindOriginalBotHighlight(
    unsigned int pawnHandle)
{
    for (int i = 0; i < g_originalBotHighlightCount; ++i)
        if (g_originalBotHighlights[i].pawnHandle == pawnHandle)
            return &g_originalBotHighlights[i];
    return nullptr;
}

static void CopyFourBytes(BYTE* destination, const BYTE* source)
{
    for (int i = 0; i < 4; ++i)
        destination[i] = source[i];
}

static bool HighlightSlots(const BotHighlightRuntime& runtime, void* pawn,
    BYTE** glowColor, BYTE** eligible, BYTE** glowing, BYTE** clientTint,
    BYTE** useClientTint)
{
    if (!pawn || !glowColor || !eligible || !glowing || !clientTint ||
        !useClientTint)
        return false;
    BYTE* base = reinterpret_cast<BYTE*>(pawn);
    *glowColor = base + runtime.glowOffset + runtime.glowColorOffset;
    *eligible = base + runtime.glowOffset + runtime.glowEligibleOffset;
    *glowing = base + runtime.glowOffset + runtime.glowingOffset;
    *clientTint = base + runtime.clientTintOffset;
    *useClientTint = base + runtime.useClientTintOffset;
    return IsAccessible(*glowColor, 4, true) &&
        IsAccessible(*eligible, 1, true) &&
        IsAccessible(*glowing, 1, true) &&
        IsAccessible(*clientTint, 4, true) &&
        IsAccessible(*useClientTint, 1, true);
}

static bool RememberBotHighlight(const BotHighlightRuntime& runtime,
    unsigned int pawnHandle, void* pawn)
{
    OriginalBotHighlight* existing = FindOriginalBotHighlight(pawnHandle);
    if (existing)
    {
        void* identity = IsAccessible(pawn, 0x18, false) ?
            *reinterpret_cast<void**>(reinterpret_cast<BYTE*>(pawn) + 0x10) :
            nullptr;
        void* vtable = IsAccessible(pawn, sizeof(void*), false) ?
            *reinterpret_cast<void**>(pawn) : nullptr;
        if (existing->pawnAddress == pawn &&
            existing->identityAddress == identity &&
            existing->pawnVtable == vtable)
            return true;

        // Same numeric handle in a different object/map generation is not the
        // entity whose bytes were captured. Drop that stale snapshot without
        // writing it into the new pawn, then capture the new object below.
        const int staleIndex = static_cast<int>(
            existing - g_originalBotHighlights);
        for (int i = staleIndex + 1; i < g_originalBotHighlightCount; ++i)
            g_originalBotHighlights[i - 1] = g_originalBotHighlights[i];
        --g_originalBotHighlightCount;
    }
    if (g_originalBotHighlightCount >= static_cast<int>(
        sizeof(g_originalBotHighlights) / sizeof(g_originalBotHighlights[0])))
        return false;
    BYTE* glowColor = nullptr;
    BYTE* eligible = nullptr;
    BYTE* glowing = nullptr;
    BYTE* clientTint = nullptr;
    BYTE* useClientTint = nullptr;
    if (!HighlightSlots(runtime, pawn, &glowColor, &eligible, &glowing,
        &clientTint, &useClientTint))
        return false;
    OriginalBotHighlight& original =
        g_originalBotHighlights[g_originalBotHighlightCount++];
    original.pawnHandle = pawnHandle;
    original.pawnAddress = pawn;
    original.identityAddress = *reinterpret_cast<void**>(
        reinterpret_cast<BYTE*>(pawn) + 0x10);
    original.pawnVtable = *reinterpret_cast<void**>(pawn);
    CopyFourBytes(original.glowColor, glowColor);
    CopyFourBytes(original.clientTint, clientTint);
    original.glowing = *glowing;
    original.eligible = *eligible;
    original.useClientTint = *useClientTint;
    original.seen = true;
    if (!IsAccessible(original.identityAddress, 0x18, false) ||
        !IsAccessible(original.pawnVtable, sizeof(void*), false) ||
        !IsExecutable(*reinterpret_cast<void**>(original.pawnVtable)))
    {
        --g_originalBotHighlightCount;
        return false;
    }
    return true;
}

static bool ApplyBotHighlight(const BotHighlightRuntime& runtime,
    unsigned int pawnHandle, void* pawn)
{
    if (!RememberBotHighlight(runtime, pawnHandle, pawn))
        return false;
    BYTE* glowColor = nullptr;
    BYTE* eligible = nullptr;
    BYTE* glowing = nullptr;
    BYTE* clientTint = nullptr;
    BYTE* useClientTint = nullptr;
    if (!HighlightSlots(runtime, pawn, &glowColor, &eligible, &glowing,
        &clientTint, &useClientTint))
        return false;
    OriginalBotHighlight* original = FindOriginalBotHighlight(pawnHandle);
    if (original)
        original->seen = true;

    // RGBA lime-green. Glow is requested first; the client tint makes the
    // feature visibly useful even on renderer paths that ignore raw glow
    // property changes until an internal glow-manager registration occurs.
    const BYTE highlightColor[4] = { 72, 255, 96, 255 };
    CopyFourBytes(glowColor, highlightColor);
    *eligible = 1;
    *glowing = 1;
    CopyFourBytes(clientTint, highlightColor);
    *useClientTint = 1;
    return true;
}

enum BotRestoreResult {
    BOT_RESTORE_DONE = 1,
    BOT_RESTORE_STALE = 0,
    BOT_RESTORE_RETRY = -1
};

static int RestoreBotHighlight(const BotHighlightRuntime& runtime,
    void* entitySystem, const OriginalBotHighlight& original)
{
    void* pawn = nullptr;
    const int resolveResult = ResolveEntityHandle(entitySystem,
        original.pawnHandle, &pawn);
    if (resolveResult == ENTITY_HANDLE_RETRY)
        return BOT_RESTORE_RETRY;
    if (resolveResult != ENTITY_HANDLE_RESOLVED)
        return BOT_RESTORE_STALE;
    if (pawn != original.pawnAddress || !IsAccessible(pawn, 0x18, false) ||
        *reinterpret_cast<void**>(pawn) != original.pawnVtable ||
        *reinterpret_cast<void**>(reinterpret_cast<BYTE*>(pawn) + 0x10) !=
            original.identityAddress)
        return BOT_RESTORE_STALE;
    BYTE* glowColor = nullptr;
    BYTE* eligible = nullptr;
    BYTE* glowing = nullptr;
    BYTE* clientTint = nullptr;
    BYTE* useClientTint = nullptr;
    if (!HighlightSlots(runtime, pawn, &glowColor, &eligible, &glowing,
        &clientTint, &useClientTint))
        return BOT_RESTORE_RETRY;
    CopyFourBytes(glowColor, original.glowColor);
    *eligible = original.eligible;
    *glowing = original.glowing;
    CopyFourBytes(clientTint, original.clientTint);
    *useClientTint = original.useClientTint;
    return BOT_RESTORE_DONE;
}

static int RestoreAllBotHighlights(BotHighlightStats* stats)
{
    if (stats)
    {
        stats->highlighted = 0;
        stats->restored = 0;
    }
    void* entitySystem = CurrentEntitySystem(g_botHighlightRuntime.entity);
    int destination = 0;
    for (int i = 0; i < g_originalBotHighlightCount; ++i)
    {
        OriginalBotHighlight& original = g_originalBotHighlights[i];
        const int restoreResult = entitySystem ?
            RestoreBotHighlight(g_botHighlightRuntime, entitySystem,
                original) : BOT_RESTORE_RETRY;
        if (restoreResult == BOT_RESTORE_DONE)
        {
            if (stats)
                ++stats->restored;
            continue;
        }
        if (restoreResult == BOT_RESTORE_RETRY)
            g_originalBotHighlights[destination++] = original;
    }
    g_originalBotHighlightCount = destination;
    g_botHighlightRestorePending = destination > 0;
    return BOT_HIGHLIGHT_DISABLED;
}

static int UpdateBotHighlights(BotHighlightStats* stats)
{
    constexpr unsigned int kFakeClientFlag = 1u << 8;
    if (stats)
    {
        stats->highlighted = 0;
        stats->restored = 0;
    }
    if (!g_botHighlightRuntimeReady)
    {
        g_botHighlightRuntimeReady =
            ResolveBotHighlightRuntime(&g_botHighlightRuntime);
        if (!g_botHighlightRuntimeReady)
            return BOT_HIGHLIGHT_ERR_SCHEMA;
    }

    void* entitySystem = CurrentEntitySystem(g_botHighlightRuntime.entity);
    if (!entitySystem)
        return BOT_HIGHLIGHT_WAITING_MAP;
    if (g_lastBotEntitySystem && g_lastBotEntitySystem != entitySystem)
    {
        g_originalBotHighlightCount = 0;
        g_botHighlightRestorePending = false;
    }
    g_lastBotEntitySystem = entitySystem;

    for (int i = 0; i < g_originalBotHighlightCount; ++i)
        g_originalBotHighlights[i].seen = false;

    BYTE localTeam = 0;
    void* localController = nullptr;
    constexpr int kControllerSlotLimit = 64;
    void* controllers[kControllerSlotLimit];
    ZeroBytes(controllers, sizeof(controllers));
    int controllerCount = 0;
    for (int index = 1; index <= kControllerSlotLimit; ++index)
    {
        void* controller = EntityAtIndex(entitySystem, index);
        if (!HasDesignerName(controller, "cs_player_controller"))
            continue;
        controllers[controllerCount++] = controller;
        BYTE* localFlag = reinterpret_cast<BYTE*>(controller) +
            g_botHighlightRuntime.localControllerOffset;
        BYTE* team = reinterpret_cast<BYTE*>(controller) +
            g_botHighlightRuntime.teamOffset;
        if (!IsAccessible(localFlag, 1, false) ||
            !IsAccessible(team, 1, false))
            continue;
        if (*localFlag != 0)
        {
            localController = controller;
            localTeam = *team;
        }
    }

    if (!localController || localTeam < 2 || localTeam > 3)
    {
        RestoreAllBotHighlights(stats);
        return BOT_HIGHLIGHT_WAITING_LOCAL;
    }

    unsigned int humanControlledPawns[kControllerSlotLimit];
    ZeroBytes(humanControlledPawns, sizeof(humanControlledPawns));
    int humanControlledPawnCount = 0;
    for (int i = 0; i < controllerCount; ++i)
    {
        BYTE* base = reinterpret_cast<BYTE*>(controllers[i]);
        unsigned int* flags = reinterpret_cast<unsigned int*>(
            base + g_botHighlightRuntime.flagsOffset);
        BYTE* controllingBot = base +
            g_botHighlightRuntime.controllingBotOffset;
        unsigned int* pawnHandle = reinterpret_cast<unsigned int*>(
            base + g_botHighlightRuntime.playerPawnHandleOffset);
        if (IsAccessible(flags, sizeof(unsigned int), false) &&
            IsAccessible(controllingBot, 1, false) &&
            IsAccessible(pawnHandle, sizeof(unsigned int), false) &&
            (*flags & kFakeClientFlag) == 0 && *controllingBot != 0)
            humanControlledPawns[humanControlledPawnCount++] = *pawnHandle;
    }

    for (int i = 0; i < controllerCount; ++i)
    {
        void* controller = controllers[i];
        if (controller == localController)
            continue;
        BYTE* base = reinterpret_cast<BYTE*>(controller);
        BYTE* team = base + g_botHighlightRuntime.teamOffset;
        unsigned int* flags = reinterpret_cast<unsigned int*>(
            base + g_botHighlightRuntime.flagsOffset);
        BYTE* pawnAlive = base + g_botHighlightRuntime.pawnAliveOffset;
        unsigned int* pawnHandle = reinterpret_cast<unsigned int*>(
            base + g_botHighlightRuntime.playerPawnHandleOffset);
        if (!IsAccessible(team, 1, false) ||
            !IsAccessible(flags, sizeof(unsigned int), false) ||
            !IsAccessible(pawnAlive, 1, false) ||
            !IsAccessible(pawnHandle, sizeof(unsigned int), false) ||
            *team != localTeam || (*flags & kFakeClientFlag) == 0 ||
            *pawnAlive == 0)
            continue;

        bool controlledByHuman = false;
        for (int humanIndex = 0;
            humanIndex < humanControlledPawnCount; ++humanIndex)
        {
            if (humanControlledPawns[humanIndex] == *pawnHandle)
            {
                controlledByHuman = true;
                break;
            }
        }
        if (controlledByHuman)
            continue;

        void* pawn = EntityFromHandle(entitySystem, *pawnHandle);
        if (!pawn)
            continue;
        BYTE* pawnBase = reinterpret_cast<BYTE*>(pawn);
        BYTE* pawnTeam = pawnBase + g_botHighlightRuntime.teamOffset;
        int* health = reinterpret_cast<int*>(
            pawnBase + g_botHighlightRuntime.healthOffset);
        BYTE* lifeState = pawnBase + g_botHighlightRuntime.lifeStateOffset;
        if (!IsAccessible(pawnTeam, 1, false) ||
            !IsAccessible(health, sizeof(int), false) ||
            !IsAccessible(lifeState, 1, false) ||
            *pawnTeam != localTeam || *health <= 0 || *health > 1000 ||
            *lifeState != 0)
            continue;
        if (ApplyBotHighlight(g_botHighlightRuntime, *pawnHandle, pawn) &&
            stats)
            ++stats->highlighted;
    }

    int destination = 0;
    for (int i = 0; i < g_originalBotHighlightCount; ++i)
    {
        OriginalBotHighlight& original = g_originalBotHighlights[i];
        if (!original.seen)
        {
            const int restoreResult = RestoreBotHighlight(
                g_botHighlightRuntime, entitySystem, original);
            if (restoreResult == BOT_RESTORE_DONE)
            {
                if (stats)
                    ++stats->restored;
                continue;
            }
            if (restoreResult == BOT_RESTORE_STALE)
                continue;
        }
        g_originalBotHighlights[destination++] = original;
    }
    g_originalBotHighlightCount = destination;
    return stats && stats->highlighted > 0 ? BOT_HIGHLIGHT_ACTIVE :
        BOT_HIGHLIGHT_WAITING_BOTS;
}

static void QueueBotHighlight(bool enabled)
{
    if (!g_frameStageVtableSlot || !g_originalFrameStageNotify)
    {
        if (g_botHighlightCheck)
            SendMessageW(g_botHighlightCheck, BM_SETCHECK,
                BST_UNCHECKED, 0);
        SetStatus(L"Status: CS2 frame-stage bridge is unavailable.");
        return;
    }
    AtomicExchange(&g_pendingBotHighlightRequest, enabled ? 1 : -1);
    SetStatus(enabled ?
        L"Status: teammate-bot highlight queued for CS2 render frame..." :
        L"Status: bot highlight restore queued for CS2 render frame...");
}

static void QueueSelectedSkybox()
{
    if (!g_skyboxCombo || !g_frameStageVtableSlot ||
        !g_originalFrameStageNotify)
    {
        SetStatus(L"Status: CS2 frame-stage bridge is unavailable.");
        return;
    }
    const LRESULT selected = SendMessageW(g_skyboxCombo, CB_GETCURSEL, 0, 0);
    if (selected == CB_ERR || selected < 0 ||
        selected >= static_cast<LRESULT>(sizeof(kSkyboxes) / sizeof(kSkyboxes[0])))
    {
        SetStatus(L"Status: select a skybox first.");
        return;
    }
    AtomicExchange(&g_pendingSkyboxRequest,
        static_cast<LONG>(selected) + 1);
    SetStatus(L"Status: queued for CS2 render frame...");
}

static void QueueRestoreSkybox()
{
    if (!g_frameStageVtableSlot || !g_originalFrameStageNotify)
    {
        SetStatus(L"Status: CS2 frame-stage bridge is unavailable.");
        return;
    }
    AtomicExchange(&g_pendingSkyboxRequest, -1);
    SetStatus(L"Status: restore queued for CS2 render frame...");
}

static BOOL CALLBACK FindLargestProcessWindow(HWND wnd, LPARAM param)
{
    auto* state = reinterpret_cast<WindowSearchState*>(param);
    if (!IsWindowVisible(wnd))
        return 1;
    DWORD pid = 0;
    GetWindowThreadProcessId(wnd, &pid);
    if (pid != state->pid)
        return 1;
    RECT client{};
    if (!GetClientRect(wnd, &client))
        return 1;
    const long width = client.right - client.left;
    const long height = client.bottom - client.top;
    if (width <= 0 || height <= 0)
        return 1;
    const unsigned long long area = static_cast<unsigned long long>(width) * static_cast<unsigned long long>(height);
    if (area > state->bestArea)
    {
        state->best = wnd;
        state->bestArea = area;
    }
    return 1;
}

static HWND FindGameWindow()
{
    WindowSearchState state{};
    state.pid = GetCurrentProcessId();
    EnumWindows(FindLargestProcessWindow, reinterpret_cast<LPARAM>(&state));
    return state.best;
}

static bool IsCurrentProcessForeground()
{
    const HWND foreground = GetForegroundWindow();
    if (!foreground)
        return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(foreground, &pid);
    return pid == GetCurrentProcessId();
}

static void FrameStageNotifyHook(void* client, int stage)
{
    if (stage == FRAME_RENDER_PASS)
    {
        const LONG request = AtomicExchange(&g_pendingSkyboxRequest, 0);
        if (request != 0)
        {
            SkyboxApplyStats stats{};
            int result = SKYBOX_ERR_DISPATCH;
            if (request == -1)
                result = RestoreSkyboxOnGameThread(&stats);
            else if (request > 0)
                result = ApplySkyboxOnGameThread(
                    static_cast<int>(request - 1), &stats);

            if (g_menuWindow)
                PostMessageW(g_menuWindow, WM_POTATO_STATUS,
                    static_cast<WPARAM>(static_cast<LONG_PTR>(result)),
                    static_cast<LPARAM>(stats.rendererConfirmed));
        }
    }
    g_originalFrameStageNotify(client, stage);
    if (stage == FRAME_RENDER_PASS)
    {
        const LONG botRequest = AtomicExchange(
            &g_pendingBotHighlightRequest, 0);
        if (botRequest < 0)
        {
            BotHighlightStats stats{};
            RestoreAllBotHighlights(&stats);
            g_botHighlightEnabled = false;
            g_botHighlightFrameCounter = 0;
            const int result = g_botHighlightRestorePending ?
                BOT_HIGHLIGHT_RESTORE_PENDING : BOT_HIGHLIGHT_DISABLED;
            g_lastBotHighlightResult = result;
            if (g_menuWindow)
                PostMessageW(g_menuWindow, WM_CAS_BOT_STATUS,
                    static_cast<WPARAM>(result),
                    static_cast<LPARAM>(stats.restored));
        }
        else if (botRequest > 0)
        {
            g_botHighlightEnabled = true;
            g_botHighlightRestorePending = false;
            g_botHighlightFrameCounter = 0;
            BotHighlightStats stats{};
            const int result = UpdateBotHighlights(&stats);
            if (result < 0)
                g_botHighlightEnabled = false;
            g_lastBotHighlightResult = result;
            if (g_menuWindow)
                PostMessageW(g_menuWindow, WM_CAS_BOT_STATUS,
                    static_cast<WPARAM>(static_cast<LONG_PTR>(result)),
                    static_cast<LPARAM>(stats.highlighted));
        }
        else if (g_botHighlightEnabled &&
            ++g_botHighlightFrameCounter >= 4)
        {
            g_botHighlightFrameCounter = 0;
            BotHighlightStats stats{};
            const int result = UpdateBotHighlights(&stats);
            if (result != g_lastBotHighlightResult && g_menuWindow)
                PostMessageW(g_menuWindow, WM_CAS_BOT_STATUS,
                    static_cast<WPARAM>(static_cast<LONG_PTR>(result)),
                    static_cast<LPARAM>(stats.highlighted));
            g_lastBotHighlightResult = result;
        }
        else if (g_botHighlightRestorePending &&
            ++g_botHighlightFrameCounter >= 4)
        {
            g_botHighlightFrameCounter = 0;
            BotHighlightStats stats{};
            RestoreAllBotHighlights(&stats);
            if (!g_botHighlightRestorePending && g_menuWindow)
                PostMessageW(g_menuWindow, WM_CAS_BOT_STATUS,
                    static_cast<WPARAM>(BOT_HIGHLIGHT_DISABLED),
                    static_cast<LPARAM>(stats.restored));
        }
    }
}

static bool CompareExchangeVtableSlot(void** slot, void* desired,
    void* expected)
{
    if (!slot || !desired || !expected ||
        !IsAccessible(slot, sizeof(void*), false))
        return false;
    DWORD oldProtection = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &oldProtection))
        return false;
    void* observed = AtomicCompareExchangePointer(slot, desired, expected);
    DWORD ignoredProtection = 0;
    VirtualProtect(slot, sizeof(void*), oldProtection, &ignoredProtection);
    return observed == expected;
}

static bool InstallFrameStageBridge()
{
    HMODULE clientModule = GetModuleHandleW(L"client.dll");
    if (!clientModule)
        return false;
    // The entity signatures scan the full module. Resolve them on the payload
    // worker before installing the callback, never inside a CS2 render frame.
    g_preResolvedEntityRuntimeReady = ResolveEntityRuntime(clientModule,
        &g_preResolvedEntityRuntime);
    auto createInterface = reinterpret_cast<CreateInterfaceFn>(
        GetProcAddress(clientModule, "CreateInterface"));
    if (!createInterface)
        return false;
    void* client = createInterface("Source2Client002", nullptr);
    if (!IsAccessible(client, sizeof(void*), false))
        return false;
    void** vtable = *reinterpret_cast<void***>(client);
    if (!IsAccessible(vtable, 37 * sizeof(void*), false) ||
        !IsExecutable(vtable[36]))
        return false;

    // Current Source2Client002 frame callback. Requiring one unique match and
    // exact equality with vtable[36] makes an ABI-changing game update a clean
    // no-op instead of calling an arbitrary function.
    static const int kFrameStagePattern[] = {
        0x48, 0x89, 0x5C, 0x24, -1,
        0x48, 0x89, 0x6C, 0x24, -1,
        0x57, 0x48, 0x83, 0xEC, -1,
        0x48, 0x8B, 0xF9, 0x33, 0xED
    };
    BYTE* frameStageTarget = FindUniquePattern(clientModule,
        kFrameStagePattern,
        sizeof(kFrameStagePattern) / sizeof(kFrameStagePattern[0]));
    if (!frameStageTarget || vtable[36] != frameStageTarget)
        return false;

    g_frameStageVtableSlot = &vtable[36];
    g_originalFrameStageNotify =
        reinterpret_cast<FrameStageNotifyFn>(vtable[36]);
    if (!CompareExchangeVtableSlot(g_frameStageVtableSlot,
        reinterpret_cast<void*>(FrameStageNotifyHook),
        reinterpret_cast<void*>(g_originalFrameStageNotify)))
    {
        g_frameStageVtableSlot = nullptr;
        g_originalFrameStageNotify = nullptr;
        return false;
    }
    return true;
}

static void RemoveFrameStageBridge()
{
    AtomicExchange(&g_pendingSkyboxRequest, 0);
    AtomicExchange(&g_pendingBotHighlightRequest, 0);
    if (g_frameStageVtableSlot && g_originalFrameStageNotify)
        CompareExchangeVtableSlot(g_frameStageVtableSlot,
            reinterpret_cast<void*>(g_originalFrameStageNotify),
            reinterpret_cast<void*>(FrameStageNotifyHook));
    g_frameStageVtableSlot = nullptr;
    g_originalFrameStageNotify = nullptr;
}

static void ShowSkyboxResult(int result)
{
    if (result == SKYBOX_APPLIED)
        SetStatus(L"Status: applied; both CS2 renderer caches confirmed.");
    else if (result == SKYBOX_RESTORED)
        SetStatus(L"Status: original skybox restored and renderer confirmed.");
    else if (result == SKYBOX_ERR_MATERIAL)
        SetStatus(L"Status: VMaterialSystem2 could not load that .vmat.");
    else if (result == SKYBOX_ERR_ENTITY_SYSTEM)
        SetStatus(L"Status: CS2 entity system is not ready/compatible.");
    else if (result == SKYBOX_ERR_REFRESH)
        SetStatus(L"Status: CS2 runtime signatures or field layout changed.");
    else if (result == SKYBOX_ERR_NO_ENTITY)
        SetStatus(L"Status: this map has no client env_sky entity.");
    else if (result == SKYBOX_ERR_WRITE)
        SetStatus(L"Status: handles changed, but renderer did not confirm them.");
    else if (result == SKYBOX_ERR_NOTHING_TO_RESTORE)
        SetStatus(L"Status: nothing from this map has been captured to restore.");
    else
        SetStatus(L"Status: skybox request failed validation.");
}

static void ShowBotHighlightResult(int result, int count)
{
    if (result == BOT_HIGHLIGHT_ACTIVE)
        SetStatus(count == 1 ?
            L"Status: highlighting 1 teammate bot (glow + green tint)." :
            L"Status: teammate bots highlighted (glow + green tint).");
    else if (result == BOT_HIGHLIGHT_DISABLED)
        SetStatus(count > 0 ?
            L"Status: bot highlight disabled; original colors restored." :
            L"Status: bot highlight disabled.");
    else if (result == BOT_HIGHLIGHT_WAITING_LOCAL)
        SetStatus(L"Status: highlight enabled; waiting for your live team.");
    else if (result == BOT_HIGHLIGHT_WAITING_BOTS)
        SetStatus(L"Status: highlight enabled; no live teammate bots found.");
    else if (result == BOT_HIGHLIGHT_WAITING_MAP)
        SetStatus(L"Status: highlight enabled; waiting for a loaded map.");
    else if (result == BOT_HIGHLIGHT_RESTORE_PENDING)
        SetStatus(L"Status: highlight off; waiting to restore unavailable pawn.");
    else if (result == BOT_HIGHLIGHT_ERR_SCHEMA)
        SetStatus(L"Status: CS2 schema changed; bot highlight stayed off.");
    else
        SetStatus(L"Status: bot highlight runtime validation failed.");
}

static LRESULT CALLBACK MenuWindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_POTATO_STATUS)
    {
        ShowSkyboxResult(static_cast<int>(static_cast<LONG_PTR>(wParam)));
        return 0;
    }
    if (msg == WM_CAS_BOT_STATUS)
    {
        const int result = static_cast<int>(static_cast<LONG_PTR>(wParam));
        if (result < 0 && g_botHighlightCheck)
            SendMessageW(g_botHighlightCheck, BM_SETCHECK,
                BST_UNCHECKED, 0);
        ShowBotHighlightResult(result, static_cast<int>(lParam));
        return 0;
    }
    if (msg == WM_COMMAND)
    {
        const int id = static_cast<int>(wParam & 0xFFFFu);
        const int notification = static_cast<int>((wParam >> 16) & 0xFFFFu);
        if (notification == BN_CLICKED && id == IDC_APPLY_SKYBOX)
        {
            QueueSelectedSkybox();
            return 0;
        }
        if (notification == BN_CLICKED && id == IDC_RESTORE_SKYBOX)
        {
            QueueRestoreSkybox();
            return 0;
        }
        if (notification == BN_CLICKED && id == IDC_BOT_HIGHLIGHT)
        {
            const bool enabled = g_botHighlightCheck &&
                SendMessageW(g_botHighlightCheck, BM_GETCHECK, 0, 0) ==
                    BST_CHECKED;
            QueueBotHighlight(enabled);
            return 0;
        }
    }
    if (msg == WM_CLOSE)
    {
        g_menuRequested = false;
        g_menuActuallyShown = false;
        ShowWindow(wnd, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(wnd, msg, wParam, lParam);
}

static HWND CreateMenuWindow(HINSTANCE instance, HWND owner)
{
    static const wchar_t kClassName[] = L"CasPlusOfflineVisualsMenu430";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = MenuWindowProc;
    wc.hInstance = instance;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<ULONG_PTR>(COLOR_BTNFACE + 1));
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    constexpr int kWidth = 500;
    constexpr int kHeight = 315;
    HWND wnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kClassName, L"cas+ - CS2 Offline Visuals",
        WS_POPUP | WS_BORDER, 0, 0, kWidth, kHeight, owner, nullptr, instance, nullptr);
    if (!wnd)
        return nullptr;

    CreateWindowExW(0, L"STATIC", L"Client-side skybox", WS_CHILD | WS_VISIBLE | SS_CENTER,
        25, 18, 450, 24, wnd, nullptr, instance, nullptr);
    CreateWindowExW(0, L"STATIC", L"Preset:", WS_CHILD | WS_VISIBLE,
        35, 63, 75, 24, wnd, nullptr, instance, nullptr);
    g_skyboxCombo = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
        110, 58, 345, 220, wnd, reinterpret_cast<HMENU>(static_cast<ULONG_PTR>(IDC_SKYBOX_COMBO)), instance, nullptr);
    if (g_skyboxCombo)
    {
        for (unsigned int i = 0; i < sizeof(kSkyboxes) / sizeof(kSkyboxes[0]); ++i)
            SendMessageW(g_skyboxCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kSkyboxes[i].name));
        SendMessageW(g_skyboxCombo, CB_SETCURSEL, 0, 0);
    }
    CreateWindowExW(0, L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        110, 103, 160, 36, wnd, reinterpret_cast<HMENU>(static_cast<ULONG_PTR>(IDC_APPLY_SKYBOX)), instance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Restore", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        295, 103, 160, 36, wnd, reinterpret_cast<HMENU>(static_cast<ULONG_PTR>(IDC_RESTORE_SKYBOX)), instance, nullptr);
    g_botHighlightCheck = CreateWindowExW(0, L"BUTTON",
        L"Highlight teammate bots (glow + green tint)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        55, 158, 390, 30, wnd,
        reinterpret_cast<HMENU>(static_cast<ULONG_PTR>(IDC_BOT_HIGHLIGHT)),
        instance, nullptr);
    g_statusLabel = CreateWindowExW(0, L"STATIC", L"Status: ready. Load a map, then choose a preset.",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE, 30, 210, 440, 65, wnd, nullptr, instance, nullptr);
    return wnd;
}

static bool PositionMenuOverGame()
{
    if (!g_gameWindow || !g_menuWindow)
        return false;
    RECT client{};
    if (!GetClientRect(g_gameWindow, &client))
        return false;
    POINT origin{};
    if (!ClientToScreen(g_gameWindow, &origin))
        return false;
    constexpr int kWidth = 500;
    constexpr int kHeight = 315;
    const int width = static_cast<int>(client.right - client.left);
    const int height = static_cast<int>(client.bottom - client.top);
    const int x = origin.x + ((width > kWidth) ? (width - kWidth) / 2 : 0);
    const int y = origin.y + ((height > kHeight) ? (height - kHeight) / 2 : 0);
    return SetWindowPos(g_menuWindow, HWND_TOP, x, y, kWidth, kHeight, SWP_NOSIZE | SWP_NOACTIVATE) != 0;
}

static DWORD WINAPI PayloadThread(LPVOID parameter)
{
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(parameter);
    while (g_running && !g_gameWindow)
    {
        g_gameWindow = FindGameWindow();
        if (!g_gameWindow)
            Sleep(100);
    }
    if (!g_running || !g_gameWindow)
        return 0;
    g_menuWindow = CreateMenuWindow(instance, g_gameWindow);
    if (!g_menuWindow)
        return 0;
    if (!InstallFrameStageBridge())
        SetStatus(L"Status: failed to install the CS2 frame-stage bridge.");
    PositionMenuOverGame();

#ifdef POTATO_DIAGNOSTIC
    MessageBoxW(g_gameWindow, L"cas+ offline visuals started.\nPress Insert while CS2 is focused.",
        L"cas+ diagnostic", MB_OK | MB_ICONINFORMATION);
#endif

    bool insertWasDown = false;
    unsigned int positionTicks = 0;
    while (g_running)
    {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        const bool processIsForeground = IsCurrentProcessForeground();
        const bool insertDown = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (processIsForeground && insertDown && !insertWasDown)
            g_menuRequested = !g_menuRequested;
        insertWasDown = insertDown;

        const bool shouldShow = g_menuRequested && processIsForeground;
        if (shouldShow)
        {
            if (!g_menuActuallyShown)
            {
                PositionMenuOverGame();
                ShowWindow(g_menuWindow, SW_SHOWNA);
                UpdateWindow(g_menuWindow);
                g_menuActuallyShown = true;
                positionTicks = 0;
            }
            // Following the game every 8 ms creates needless cross-thread
            // window-manager traffic. Re-center roughly four times per second.
            if (++positionTicks >= 31)
            {
                PositionMenuOverGame();
                positionTicks = 0;
            }
        }
        else if (g_menuActuallyShown)
        {
            ShowWindow(g_menuWindow, SW_HIDE);
            g_menuActuallyShown = false;
        }
        Sleep(8);
    }
    RemoveFrameStageBridge();
    return 0;
}

extern "C" __declspec(dllexport) unsigned int WINAPI PotatoPayloadVersion()
{
    return 0x00040300u;
}

extern "C" BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_running = true;
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, PayloadThread, module, 0, nullptr);
        if (thread)
            CloseHandle(thread);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        g_running = false;
    }
    return 1;
}
