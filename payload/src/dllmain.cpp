// cas+ payload 4.4.1 - x64 manual-map friendly CS2 offline visuals UI.
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
// Current MSVC x64 ABI: RCX=scope, RDX=SchemaMetaInfoHandle_t* out,
// R8=class name. The function itself returns void.
using FindDeclaredClassFn = void (*)(void*, void**, const char*);
using SetModelRenderColorFn = void (*)(void*, BYTE, BYTE, BYTE);
using SetGlowColorFn = void (*)(void*, unsigned int);
using SetGlowTypeFn = void (*)(void*, int, float);

// Required by MSVC whenever a no-CRT image emits floating-point call ABI.
extern "C" int _fltused = 0;

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
#define BS_GROUPBOX    0x00000007UL
#define WS_DISABLED    0x08000000UL

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
static HWND g_skyboxStatusLabel = nullptr;
static HWND g_botStatusLabel = nullptr;
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
    IDC_BOT_HIGHLIGHT = 1004,
    IDC_TAB_RAGEBOT = 2001,
    IDC_TAB_ANTIAIM = 2002,
    IDC_TAB_VISUALS = 2003,
    IDC_TAB_MISC = 2004,
    IDC_TAB_CONFIGS = 2005
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
    unsigned int identityStride;
};

struct BotHighlightRuntime {
    EntityRuntime entity;
    unsigned int teamOffset;
    unsigned int flagsOffset;
    unsigned int healthOffset;
    unsigned int lifeStateOffset;
    unsigned int localControllerOffset;
    unsigned int steamIdOffset;
    unsigned int playerPawnHandleOffset;
    unsigned int pawnAliveOffset;
    unsigned int controllingBotOffset;
    unsigned int botDifficultyOffset;
    unsigned int glowOffset;
    unsigned int renderColorOffset;
    unsigned int clientTintOffset;
    unsigned int useClientTintOffset;
    unsigned int glowColorOffset;
    unsigned int glowTypeOffset;
    unsigned int glowTimeOffset;
    unsigned int glowStartTimeOffset;
    unsigned int glowEligibleOffset;
    unsigned int glowingOffset;
    SetModelRenderColorFn setRenderColor;
    SetGlowColorFn setGlowColor;
    SetGlowTypeFn setGlowType;
};

struct BotHighlightStats {
    int controllers;
    int teammateControllers;
    int botCandidates;
    int pawnsResolved;
    int highlighted;
    int restored;
};

struct OriginalBotHighlight {
    unsigned int pawnHandle;
    void* pawnAddress;
    void* identityAddress;
    void* pawnVtable;
    BYTE glowColor[4];
    BYTE renderColor[4];
    BYTE clientTint[4];
    BYTE glowTime[4];
    BYTE glowStartTime[4];
    int glowType;
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
static LPARAM g_lastBotStatsPacked = -1;
static void* g_lastBotEntitySystem = nullptr;
static void* g_lastLocalController = nullptr;
static void* g_lastLocalControllerIdentity = nullptr;
static unsigned int g_lastLocalControllerHandle = 0xFFFFFFFFu;
static EntityRuntime g_preResolvedEntityRuntime{};
static bool g_preResolvedEntityRuntimeReady = false;

struct SkyboxRuntime {
    void* entitySystem;
    unsigned int highestEntityOffset;
    unsigned int identityStride;
    unsigned int mainMaterialOffset;
    unsigned int lightingMaterialOffset;
    unsigned int mainCacheOffset;
    unsigned int lightingCacheOffset;
    ForceSkyboxUpdateFn forceUpdate;
    AssignStrongHandleFn assignStrongHandle;
};

static SkyboxRuntime g_preResolvedSkyboxRuntime{};
static bool g_preResolvedSkyboxRuntimeReady = false;

struct SkyboxApplyStats {
    int highestEntityIndex;
    int identityStride;
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
    const ULONG_PTR ptr = reinterpret_cast<ULONG_PTR>(address);
    if (ptr < 0x10000u || ptr + bytes < ptr || ptr + bytes > 0x7FFFFFFFFFFFULL)
        return false;
    return true;
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

static void SetSkyboxStatus(const wchar_t* text)
{
    if (g_skyboxStatusLabel)
        SetWindowTextW(g_skyboxStatusLabel, text);
}

static void SetBotStatus(const wchar_t* text)
{
    if (g_botStatusLabel)
        SetWindowTextW(g_botStatusLabel, text);
}

struct WideStatusBuilder {
    wchar_t text[256];
    unsigned int length;
};

static void AppendStatusText(WideStatusBuilder* builder,
    const wchar_t* text)
{
    if (!builder || !text)
        return;
    while (*text && builder->length + 1 <
        sizeof(builder->text) / sizeof(builder->text[0]))
        builder->text[builder->length++] = *text++;
    builder->text[builder->length] = 0;
}

static void AppendStatusUnsigned(WideStatusBuilder* builder,
    unsigned int value)
{
    wchar_t digits[10];
    unsigned int count = 0;
    do
    {
        digits[count++] = static_cast<wchar_t>(L'0' + (value % 10));
        value /= 10;
    } while (value && count < sizeof(digits) / sizeof(digits[0]));
    while (count > 0)
    {
        wchar_t one[2] = { digits[--count], 0 };
        AppendStatusText(builder, one);
    }
}

static unsigned int ClampStatusCount(int value)
{
    if (value <= 0)
        return 0;
    return value > 255 ? 255u : static_cast<unsigned int>(value);
}

static LPARAM PackBotStats(const BotHighlightStats& stats)
{
    const unsigned long long packed =
        ClampStatusCount(stats.controllers) |
        (static_cast<unsigned long long>(
            ClampStatusCount(stats.teammateControllers)) << 8) |
        (static_cast<unsigned long long>(
            ClampStatusCount(stats.botCandidates)) << 16) |
        (static_cast<unsigned long long>(
            ClampStatusCount(stats.pawnsResolved)) << 24) |
        (static_cast<unsigned long long>(
            ClampStatusCount(stats.highlighted)) << 32) |
        (static_cast<unsigned long long>(
            ClampStatusCount(stats.restored)) << 40);
    return static_cast<LPARAM>(packed);
}

static LPARAM PackSkyboxStats(const SkyboxApplyStats& stats)
{
    const unsigned long long highest = stats.highestEntityIndex < 0 ? 0 :
        (stats.highestEntityIndex > 65535 ? 65535u :
            static_cast<unsigned int>(stats.highestEntityIndex));
    const unsigned long long packed = highest |
        (static_cast<unsigned long long>(
            ClampStatusCount(stats.identityStride)) << 16) |
        (static_cast<unsigned long long>(
            ClampStatusCount(stats.skyEntities)) << 24) |
        (static_cast<unsigned long long>(
            ClampStatusCount(stats.handleWrites)) << 32) |
        (static_cast<unsigned long long>(
            ClampStatusCount(stats.rendererConfirmed)) << 40);
    return static_cast<LPARAM>(packed);
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

    // vtable[2] writes the one-pointer SchemaMetaInfoHandle_t through its
    // explicit second argument. Treating it as a pointer-returning function
    // shifts className into the wrong register and can crash the client.
    auto findDeclaredClass = reinterpret_cast<FindDeclaredClassFn>(vtable[2]);
    void* classInfoRaw = nullptr;
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

static bool ResolveEntityIdentityStride(HMODULE clientModule,
    unsigned int* stride)
{
    if (!clientModule || !stride)
        return false;
    *stride = 0;
    // Resolve the identity stride without calling SchemaSystem from the
    // manual-map worker. The full-handle resolver multiplies the chunk-local
    // index by the exact CEntityIdentity stride.
    static const int kHandleResolverPattern[] = {
        0x48, 0x8B, 0x87, 0x80, 0x13, 0x00, 0x00,
        0x8B, 0x90, 0x18, 0x15, 0x00, 0x00,
        0x83, 0xFA, 0xFF, 0x74, -1,
        0x4C, 0x8B, 0x05, -1, -1, -1, -1,
        0x4D, 0x85, 0xC0, 0x74, -1,
        0x83, 0xFA, 0xFE, 0x74, -1,
        0x8B, 0xC2, 0x25, 0xFF, 0x7F, 0x00, 0x00,
        0x8B, 0xC8, 0x48, 0xC1, 0xE8, 0x09,
        0x4D, 0x8B, 0x0C, 0xC0, 0x4D, 0x85, 0xC9, 0x74, -1,
        0x81, 0xE1, 0xFF, 0x01, 0x00, 0x00,
        0x48, 0x6B, 0xC1, -1, 0x49, 0x03, 0xC1, 0x74, -1,
        0x33, 0xC9, 0x39, 0x50, 0x10, 0x48, 0x0F, 0x45, 0xC1
    };
    BYTE* resolver = FindUniquePattern(clientModule,
        kHandleResolverPattern,
        sizeof(kHandleResolverPattern) / sizeof(kHandleResolverPattern[0]));
    if (!resolver)
        return false;
    const unsigned int resolved = resolver[66];
    if (resolved < 0x60 || resolved > 0x100 || (resolved & 7u) != 0)
        return false;
    *stride = resolved;
    return true;
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
    unsigned int identityStride = 0;
    if (!entityMatch || !highestMatch ||
        !ResolveEntityIdentityStride(clientModule, &identityStride))
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
    runtime->identityStride = identityStride;
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

static void* EntityAtIndex(const EntityRuntime& runtime, void* entitySystem,
    int index)
{
    if (!entitySystem || index < 0 || index > 32768 ||
        runtime.identityStride < 0x60 || runtime.identityStride > 0x100)
        return nullptr;
    BYTE* chunkAddress = reinterpret_cast<BYTE*>(entitySystem) + 0x10 +
        8ull * static_cast<unsigned int>(index >> 9);
    if (!IsAccessible(chunkAddress, sizeof(void*), false))
        return nullptr;
    void* chunk = *reinterpret_cast<void**>(chunkAddress);
    const SIZE_T entityOffset = static_cast<SIZE_T>(runtime.identityStride) *
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

static void* EntityFromHandle(const EntityRuntime& runtime,
    void* entitySystem, unsigned int handle)
{
    constexpr unsigned int kEntityIndexMask = 0x7FFFu;
    if (!entitySystem || handle == 0 || handle == 0xFFFFFFFFu)
        return nullptr;
    const unsigned int index = handle & kEntityIndexMask;
    if (index == kEntityIndexMask || index > 32768u)
        return nullptr;
    void* entity = EntityAtIndex(runtime, entitySystem,
        static_cast<int>(index));
    if (!entity || EntityHandleFor(entity) != handle)
        return nullptr;
    return entity;
}

enum EntityHandleResolveResult {
    ENTITY_HANDLE_RESOLVED = 1,
    ENTITY_HANDLE_STALE = 0,
    ENTITY_HANDLE_RETRY = -1
};

static int ResolveEntityHandle(const EntityRuntime& runtime,
    void* entitySystem, unsigned int handle, void** resolved)
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
    if (runtime.identityStride < 0x60 || runtime.identityStride > 0x100)
        return ENTITY_HANDLE_RETRY;
    BYTE* entityAddress = reinterpret_cast<BYTE*>(chunk) +
        static_cast<SIZE_T>(runtime.identityStride) *
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
    unsigned int identityStride = 0;
    const int forceRelative = *reinterpret_cast<int*>(forceMatch + 5);
    BYTE* forceTarget = forceMatch + 9 + forceRelative;
    if (!IsExecutable(forceTarget) ||
        !ResolveEntityIdentityStride(clientModule, &identityStride))
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

    if (highestOffset < 0x100 || highestOffset > 0x10000 ||
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
    runtime->identityStride = identityStride;
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
        stats->identityStride = 0;
        stats->skyEntities = 0;
        stats->handleWrites = 0;
        stats->rendererConfirmed = 0;
    }
    if (!g_preResolvedSkyboxRuntimeReady ||
        !g_preResolvedEntityRuntimeReady)
        return SKYBOX_ERR_REFRESH;
    SkyboxRuntime runtime = g_preResolvedSkyboxRuntime;
    runtime.entitySystem = CurrentEntitySystem(g_preResolvedEntityRuntime);
    if (!runtime.entitySystem)
        return SKYBOX_ERR_ENTITY_SYSTEM;
    if (stats)
        stats->identityStride = static_cast<int>(runtime.identityStride);

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
        void* entity = EntityAtIndex(g_preResolvedEntityRuntime,
            runtime.entitySystem, index);
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

static bool ResolveBotHighlightRuntime(HMODULE clientModule,
    BotHighlightRuntime* runtime)
{
    if (!clientModule || !runtime)
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
        !FindSchemaField(baseController, "m_steamID",
            &runtime->steamIdOffset) ||
        !FindSchemaField(playerController, "m_hPlayerPawn",
            &runtime->playerPawnHandleOffset) ||
        !FindSchemaField(playerController, "m_bPawnIsAlive",
            &runtime->pawnAliveOffset) ||
        !FindSchemaField(playerController, "m_bControllingBot",
            &runtime->controllingBotOffset) ||
        !FindSchemaField(playerController, "m_iPawnBotDifficulty",
            &runtime->botDifficultyOffset) ||
        !FindSchemaField(baseModelEntity, "m_Glow", &runtime->glowOffset) ||
        !FindSchemaField(baseModelEntity, "m_clrRender",
            &runtime->renderColorOffset) ||
        !FindSchemaField(baseModelEntity, "m_ClientOverrideTint",
            &runtime->clientTintOffset) ||
        !FindSchemaField(baseModelEntity, "m_bUseClientOverrideTint",
            &runtime->useClientTintOffset) ||
        !FindSchemaField(glowProperty, "m_glowColorOverride",
            &runtime->glowColorOffset) ||
        !FindSchemaField(glowProperty, "m_iGlowType",
            &runtime->glowTypeOffset) ||
        !FindSchemaField(glowProperty, "m_flGlowTime",
            &runtime->glowTimeOffset) ||
        !FindSchemaField(glowProperty, "m_flGlowStartTime",
            &runtime->glowStartTimeOffset) ||
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
        (runtime->steamIdOffset & 7u) != 0 ||
        runtime->steamIdOffset + sizeof(unsigned long long) >
            runtime->localControllerOffset ||
        (runtime->playerPawnHandleOffset & 3u) != 0 ||
        runtime->playerPawnHandleOffset + sizeof(unsigned int) >
            runtime->pawnAliveOffset ||
        runtime->glowTypeOffset + 4 > runtime->glowColorOffset ||
        runtime->glowTimeOffset != runtime->glowColorOffset + 8 ||
        runtime->glowStartTimeOffset != runtime->glowTimeOffset + 4 ||
        runtime->glowEligibleOffset != runtime->glowStartTimeOffset + 4 ||
        runtime->glowingOffset != runtime->glowEligibleOffset + 1 ||
        runtime->clientTintOffset + 4 > runtime->useClientTintOffset ||
        runtime->playerPawnHandleOffset + sizeof(unsigned int) >
            static_cast<unsigned int>(playerController->size) ||
        runtime->pawnAliveOffset >=
            static_cast<unsigned int>(playerController->size) ||
        runtime->controllingBotOffset >=
            static_cast<unsigned int>(playerController->size) ||
        runtime->botDifficultyOffset + sizeof(int) >
            static_cast<unsigned int>(playerController->size) ||
        runtime->botDifficultyOffset <= runtime->pawnAliveOffset ||
        runtime->glowOffset + runtime->glowingOffset >=
            static_cast<unsigned int>(baseModelEntity->size) ||
        runtime->renderColorOffset + 4 >
            static_cast<unsigned int>(baseModelEntity->size) ||
        runtime->clientTintOffset + 4 >
            static_cast<unsigned int>(baseModelEntity->size) ||
        runtime->useClientTintOffset >=
            static_cast<unsigned int>(baseModelEntity->size))
        return false;

    static const int kRenderColorSetterPattern[] = {
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x40,
        0x48, 0x8B, 0xD9,
        0x38, 0x91, -1, -1, -1, -1, 0x74, 0x06,
        0x88, 0x91, -1, -1, -1, -1,
        0x44, 0x38, 0x81, -1, -1, -1, -1, 0x74, 0x07,
        0x44, 0x88, 0x81, -1, -1, -1, -1,
        0x44, 0x38, 0x89, -1, -1, -1, -1, 0x74, 0x07,
        0x44, 0x88, 0x89, -1, -1, -1, -1,
        0x48, 0x8B, 0xB9, -1, -1, -1, -1, 0x48, 0x85, 0xFF
    };
    static const int kGlowColorSetterPattern[] = {
        0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xD9,
        0x48, 0x83, 0xC1, 0x40, 0x38, 0x11, 0x75, 0x1E,
        0x8B, 0xC2, 0xC1, 0xE8, 0x08, 0x38, 0x41, 0x01, 0x75, 0x14,
        0x8B, 0xC2, 0xC1, 0xE8, 0x10, 0x38, 0x41, 0x02, 0x75, 0x0A,
        0x8B, 0xC2, 0xC1, 0xE8, 0x18, 0x38, 0x41, 0x03, 0x74, 0x02,
        0x89, 0x11, 0xE8, -1, -1, -1, -1, 0x48, 0x8B, 0x4B, 0x18
    };
    static const int kGlowTypeSetterPattern[] = {
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20,
        0x48, 0x8B, 0x05, -1, -1, -1, -1, 0x48, 0x8B, 0xD9,
        0xF3, 0x0F, 0x10, 0x41, 0x4C,
        0xF3, 0x0F, 0x10, 0x48, 0x30,
        0x0F, 0x2E, 0xC1, 0x7A, 0x02, 0x74, 0x05,
        0xF3, 0x0F, 0x11, 0x49, 0x4C,
        0xF3, 0x0F, 0x10, 0x41, 0x48,
        0x0F, 0x2E, 0xC2, 0x7A, 0x02, 0x74, 0x05,
        0xF3, 0x0F, 0x11, 0x51, 0x48,
        0x39, 0x51, 0x30
    };
    BYTE* renderSetter = FindUniquePattern(clientModule,
        kRenderColorSetterPattern,
        sizeof(kRenderColorSetterPattern) /
            sizeof(kRenderColorSetterPattern[0]));
    BYTE* glowColorSetter = FindUniquePattern(clientModule,
        kGlowColorSetterPattern,
        sizeof(kGlowColorSetterPattern) /
            sizeof(kGlowColorSetterPattern[0]));
    BYTE* glowTypeSetter = FindUniquePattern(clientModule,
        kGlowTypeSetterPattern,
        sizeof(kGlowTypeSetterPattern) /
            sizeof(kGlowTypeSetterPattern[0]));
    if (!renderSetter || !glowColorSetter || !glowTypeSetter ||
        !IsExecutable(renderSetter) || !IsExecutable(glowColorSetter) ||
        !IsExecutable(glowTypeSetter))
        return false;

    const unsigned int redRead = *reinterpret_cast<unsigned int*>(
        renderSetter + 15);
    const unsigned int redWrite = *reinterpret_cast<unsigned int*>(
        renderSetter + 23);
    const unsigned int greenRead = *reinterpret_cast<unsigned int*>(
        renderSetter + 30);
    const unsigned int greenWrite = *reinterpret_cast<unsigned int*>(
        renderSetter + 39);
    const unsigned int blueRead = *reinterpret_cast<unsigned int*>(
        renderSetter + 46);
    const unsigned int blueWrite = *reinterpret_cast<unsigned int*>(
        renderSetter + 55);
    const unsigned int renderComponentOffset =
        *reinterpret_cast<unsigned int*>(renderSetter + 62);
    if (!IsAccessible(renderSetter, 152, false))
        return false;
    const unsigned int renderGateOffset =
        *reinterpret_cast<unsigned int*>(renderSetter + 78);
    const unsigned int useTintRead =
        *reinterpret_cast<unsigned int*>(renderSetter + 91);
    const unsigned int tintAlphaRead =
        *reinterpret_cast<unsigned int*>(renderSetter + 109);
    const unsigned int tintBlueRead =
        *reinterpret_cast<unsigned int*>(renderSetter + 120);
    const unsigned int tintGreenRead =
        *reinterpret_cast<unsigned int*>(renderSetter + 134);
    const unsigned int tintRedRead =
        *reinterpret_cast<unsigned int*>(renderSetter + 148);
    if (redRead != runtime->renderColorOffset || redWrite != redRead ||
        greenRead != redRead + 1 || greenWrite != greenRead ||
        blueRead != redRead + 2 || blueWrite != blueRead ||
        renderComponentOffset < 0x100 || renderComponentOffset > 0x800 ||
        renderGateOffset + sizeof(void*) != runtime->clientTintOffset ||
        useTintRead != runtime->useClientTintOffset ||
        tintRedRead != runtime->clientTintOffset ||
        tintGreenRead != runtime->clientTintOffset + 1 ||
        tintBlueRead != runtime->clientTintOffset + 2 ||
        tintAlphaRead != runtime->clientTintOffset + 3)
        return false;

    runtime->setRenderColor =
        reinterpret_cast<SetModelRenderColorFn>(renderSetter);
    runtime->setGlowColor =
        reinterpret_cast<SetGlowColorFn>(glowColorSetter);
    runtime->setGlowType =
        reinterpret_cast<SetGlowTypeFn>(glowTypeSetter);
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

static unsigned int PackRgba(const BYTE* color)
{
    if (!color)
        return 0;
    return static_cast<unsigned int>(color[0]) |
        (static_cast<unsigned int>(color[1]) << 8) |
        (static_cast<unsigned int>(color[2]) << 16) |
        (static_cast<unsigned int>(color[3]) << 24);
}

static bool HighlightSlots(const BotHighlightRuntime& runtime, void* pawn,
    BYTE** glowColor, BYTE** eligible, BYTE** glowing, BYTE** renderColor,
    BYTE** clientTint, BYTE** useClientTint)
{
    if (!pawn || !glowColor || !eligible || !glowing || !renderColor ||
        !clientTint || !useClientTint)
        return false;
    BYTE* base = reinterpret_cast<BYTE*>(pawn);
    *glowColor = base + runtime.glowOffset + runtime.glowColorOffset;
    *eligible = base + runtime.glowOffset + runtime.glowEligibleOffset;
    *glowing = base + runtime.glowOffset + runtime.glowingOffset;
    *renderColor = base + runtime.renderColorOffset;
    *clientTint = base + runtime.clientTintOffset;
    *useClientTint = base + runtime.useClientTintOffset;
    BYTE* glowBase = base + runtime.glowOffset;
    return runtime.setRenderColor && runtime.setGlowColor &&
        runtime.setGlowType && IsAccessible(*glowColor, 4, true) &&
        IsAccessible(glowBase + runtime.glowTypeOffset, 4, true) &&
        IsAccessible(glowBase + runtime.glowTimeOffset, 4, true) &&
        IsAccessible(glowBase + runtime.glowStartTimeOffset, 4, true) &&
        IsAccessible(*eligible, 1, true) &&
        IsAccessible(*glowing, 1, true) &&
        IsAccessible(*renderColor, 4, true) &&
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
    BYTE* renderColor = nullptr;
    BYTE* clientTint = nullptr;
    BYTE* useClientTint = nullptr;
    if (!HighlightSlots(runtime, pawn, &glowColor, &eligible, &glowing,
        &renderColor, &clientTint, &useClientTint))
        return false;
    OriginalBotHighlight& original =
        g_originalBotHighlights[g_originalBotHighlightCount++];
    original.pawnHandle = pawnHandle;
    original.pawnAddress = pawn;
    original.identityAddress = *reinterpret_cast<void**>(
        reinterpret_cast<BYTE*>(pawn) + 0x10);
    original.pawnVtable = *reinterpret_cast<void**>(pawn);
    CopyFourBytes(original.glowColor, glowColor);
    CopyFourBytes(original.renderColor, renderColor);
    CopyFourBytes(original.clientTint, clientTint);
    BYTE* glowBase = reinterpret_cast<BYTE*>(pawn) + runtime.glowOffset;
    original.glowType = *reinterpret_cast<int*>(
        glowBase + runtime.glowTypeOffset);
    CopyFourBytes(original.glowTime,
        glowBase + runtime.glowTimeOffset);
    CopyFourBytes(original.glowStartTime,
        glowBase + runtime.glowStartTimeOffset);
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
    BYTE* renderColor = nullptr;
    BYTE* clientTint = nullptr;
    BYTE* useClientTint = nullptr;
    if (!HighlightSlots(runtime, pawn, &glowColor, &eligible, &glowing,
        &renderColor, &clientTint, &useClientTint))
        return false;
    OriginalBotHighlight* original = FindOriginalBotHighlight(pawnHandle);
    if (original)
        original->seen = true;

    // RGBA bright red for enemy team. Glow is requested first; the client tint
    // makes the feature visibly useful across renderer paths.
    const BYTE highlightColor[4] = { 255, 64, 64, 255 };
    const bool stateAlreadyApplied = (*useClientTint == 1) &&
        (*eligible == 1) && (*glowing == 1) &&
        (renderColor[0] == highlightColor[0] && renderColor[1] == highlightColor[1] && renderColor[2] == highlightColor[2]) &&
        (clientTint[0] == highlightColor[0] && clientTint[1] == highlightColor[1] && clientTint[2] == highlightColor[2]);

    if (!stateAlreadyApplied)
    {
        CopyFourBytes(renderColor, highlightColor);
        CopyFourBytes(clientTint, highlightColor);
        *useClientTint = 1;
        runtime.setRenderColor(pawn, highlightColor[0], highlightColor[1],
            highlightColor[2]);

        BYTE* glowBase = reinterpret_cast<BYTE*>(pawn) + runtime.glowOffset;
        runtime.setGlowColor(glowBase, PackRgba(highlightColor));
        *eligible = 1;
        runtime.setGlowType(glowBase, 3, 0.0f);
        *glowing = 1;
    }
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
    const int resolveResult = ResolveEntityHandle(runtime.entity,
        entitySystem, original.pawnHandle, &pawn);
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
    BYTE* renderColor = nullptr;
    BYTE* clientTint = nullptr;
    BYTE* useClientTint = nullptr;
    if (!HighlightSlots(runtime, pawn, &glowColor, &eligible, &glowing,
        &renderColor, &clientTint, &useClientTint))
        return BOT_RESTORE_RETRY;
    CopyFourBytes(clientTint, original.clientTint);
    *useClientTint = original.useClientTint;
    CopyFourBytes(renderColor, original.renderColor);
    runtime.setRenderColor(pawn, original.renderColor[0],
        original.renderColor[1], original.renderColor[2]);

    BYTE* glowBase = reinterpret_cast<BYTE*>(pawn) + runtime.glowOffset;
    runtime.setGlowType(glowBase, original.glowType, 0.0f);
    CopyFourBytes(glowBase + runtime.glowTimeOffset,
        original.glowTime);
    CopyFourBytes(glowBase + runtime.glowStartTimeOffset,
        original.glowStartTime);
    runtime.setGlowColor(glowBase, PackRgba(original.glowColor));
    CopyFourBytes(glowColor, original.glowColor);
    *eligible = original.eligible;
    *glowing = original.glowing;
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

static bool IsBotController(const BotHighlightRuntime& runtime,
    BYTE* controller)
{
    constexpr unsigned int kFakeClientFlag = 1u << 8;
    if (!controller)
        return false;
    unsigned int* flags = reinterpret_cast<unsigned int*>(
        controller + runtime.flagsOffset);
    unsigned long long* steamId = reinterpret_cast<unsigned long long*>(
        controller + runtime.steamIdOffset);
    int* difficulty = reinterpret_cast<int*>(
        controller + runtime.botDifficultyOffset);
    if (!IsAccessible(flags, sizeof(unsigned int), false) ||
        !IsAccessible(steamId, sizeof(unsigned long long), false) ||
        !IsAccessible(difficulty, sizeof(int), false))
        return false;
    if ((*flags & kFakeClientFlag) != 0)
        return true;
    // Client controller flags have not been stable across updates. A live
    // zero-XUID controller with Valve's bounded pawn bot difficulty is the
    // replicated fallback used only after team/alive/pawn validation.
    return *steamId == 0 && *difficulty >= 0 && *difficulty <= 3;
}

static int UpdateBotHighlights(BotHighlightStats* stats)
{
    if (stats)
    {
        stats->controllers = 0;
        stats->teammateControllers = 0;
        stats->botCandidates = 0;
        stats->pawnsResolved = 0;
        stats->highlighted = 0;
        stats->restored = 0;
    }
    if (!g_preResolvedEntityRuntimeReady)
        return BOT_HIGHLIGHT_ERR_RUNTIME;
    if (!g_botHighlightRuntimeReady)
    {
        HMODULE clientModule = GetModuleHandleW(L"client.dll");
        BotHighlightRuntime resolved{};
        if (!clientModule ||
            !ResolveBotHighlightRuntime(clientModule, &resolved))
            return BOT_HIGHLIGHT_ERR_SCHEMA;
        g_botHighlightRuntime = resolved;
        g_botHighlightRuntimeReady = true;
    }

    void* entitySystem = CurrentEntitySystem(g_botHighlightRuntime.entity);
    if (!entitySystem)
        return BOT_HIGHLIGHT_WAITING_MAP;
    if (g_lastBotEntitySystem && g_lastBotEntitySystem != entitySystem)
    {
        g_originalBotHighlightCount = 0;
        g_botHighlightRestorePending = false;
        g_lastLocalController = nullptr;
        g_lastLocalControllerIdentity = nullptr;
        g_lastLocalControllerHandle = 0xFFFFFFFFu;
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
        void* controller = EntityAtIndex(g_botHighlightRuntime.entity,
            entitySystem, index);
        if (!HasDesignerName(controller, "cs_player_controller"))
            continue;
        controllers[controllerCount++] = controller;
        if (stats)
            ++stats->controllers;
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

    void* localIdentity = IsAccessible(localController, 0x18, false) ?
        *reinterpret_cast<void**>(
            reinterpret_cast<BYTE*>(localController) + 0x10) : nullptr;
    const unsigned int localHandle = EntityHandleFor(localController);
    if (!localIdentity || localHandle == 0xFFFFFFFFu)
    {
        RestoreAllBotHighlights(stats);
        return BOT_HIGHLIGHT_WAITING_LOCAL;
    }
    if (g_lastLocalController &&
        (g_lastLocalController != localController ||
         g_lastLocalControllerIdentity != localIdentity ||
         g_lastLocalControllerHandle != localHandle))
    {
        // A controller identity change is the strongest lifecycle boundary
        // available without a second engine hook. Try a serial-checked restore,
        // then discard any transient leftovers rather than carrying snapshots
        // into a new map/session where pool addresses may be reused.
        RestoreAllBotHighlights(stats);
        g_originalBotHighlightCount = 0;
        g_botHighlightRestorePending = false;
    }
    g_lastLocalController = localController;
    g_lastLocalControllerIdentity = localIdentity;
    g_lastLocalControllerHandle = localHandle;

    unsigned int humanControlledPawns[kControllerSlotLimit];
    ZeroBytes(humanControlledPawns, sizeof(humanControlledPawns));
    int humanControlledPawnCount = 0;
    for (int i = 0; i < controllerCount; ++i)
    {
        BYTE* base = reinterpret_cast<BYTE*>(controllers[i]);
        BYTE* controllingBot = base +
            g_botHighlightRuntime.controllingBotOffset;
        unsigned int* pawnHandle = reinterpret_cast<unsigned int*>(
            base + g_botHighlightRuntime.playerPawnHandleOffset);
        if (IsAccessible(controllingBot, 1, false) &&
            IsAccessible(pawnHandle, sizeof(unsigned int), false) &&
            !IsBotController(g_botHighlightRuntime, base) &&
            *controllingBot != 0)
            humanControlledPawns[humanControlledPawnCount++] = *pawnHandle;
    }

    for (int i = 0; i < controllerCount; ++i)
    {
        void* controller = controllers[i];
        if (controller == localController)
            continue;
        BYTE* base = reinterpret_cast<BYTE*>(controller);
        BYTE* team = base + g_botHighlightRuntime.teamOffset;
        BYTE* pawnAlive = base + g_botHighlightRuntime.pawnAliveOffset;
        unsigned int* pawnHandle = reinterpret_cast<unsigned int*>(
            base + g_botHighlightRuntime.playerPawnHandleOffset);
        if (!IsAccessible(team, 1, false) ||
            !IsAccessible(pawnAlive, 1, false) ||
            !IsAccessible(pawnHandle, sizeof(unsigned int), false) ||
            *team == localTeam || *team < 2 || *team > 3 || *pawnAlive == 0)
            continue;
        if (stats)
            ++stats->teammateControllers;

        void* pawn = EntityFromHandle(g_botHighlightRuntime.entity,
            entitySystem, *pawnHandle);
        if (!pawn)
            continue;
        if (stats)
            ++stats->pawnsResolved;
        BYTE* pawnBase = reinterpret_cast<BYTE*>(pawn);
        BYTE* pawnTeam = pawnBase + g_botHighlightRuntime.teamOffset;
        unsigned int* pawnFlags = reinterpret_cast<unsigned int*>(
            pawnBase + g_botHighlightRuntime.flagsOffset);
        int* health = reinterpret_cast<int*>(
            pawnBase + g_botHighlightRuntime.healthOffset);
        BYTE* lifeState = pawnBase + g_botHighlightRuntime.lifeStateOffset;
        if (!IsAccessible(pawnTeam, 1, false) ||
            !IsAccessible(pawnFlags, sizeof(unsigned int), false) ||
            !IsAccessible(health, sizeof(int), false) ||
            !IsAccessible(lifeState, 1, false) ||
            *pawnTeam == localTeam || *pawnTeam < 2 || *pawnTeam > 3 || *health <= 0 || *health > 1000 ||
            *lifeState != 0)
            continue;
        if (stats)
            ++stats->botCandidates;
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
        SetBotStatus(L"Bots: CS2 frame-stage bridge is unavailable.");
        return;
    }
    AtomicExchange(&g_pendingBotHighlightRequest, enabled ? 1 : -1);
    SetBotStatus(enabled ?
        L"Bots: highlight queued for the next CS2 render frame..." :
        L"Bots: restore queued for the next CS2 render frame...");
}

static void QueueSelectedSkybox()
{
    if (!g_skyboxCombo || !g_frameStageVtableSlot ||
        !g_originalFrameStageNotify)
    {
        SetSkyboxStatus(L"Sky: CS2 frame-stage bridge is unavailable.");
        return;
    }
    const LRESULT selected = SendMessageW(g_skyboxCombo, CB_GETCURSEL, 0, 0);
    if (selected == CB_ERR || selected < 0 ||
        selected >= static_cast<LRESULT>(sizeof(kSkyboxes) / sizeof(kSkyboxes[0])))
    {
        SetSkyboxStatus(L"Sky: select a preset first.");
        return;
    }
    AtomicExchange(&g_pendingSkyboxRequest,
        static_cast<LONG>(selected) + 1);
    SetSkyboxStatus(L"Sky: queued for the next CS2 render frame...");
}

static void QueueRestoreSkybox()
{
    if (!g_frameStageVtableSlot || !g_originalFrameStageNotify)
    {
        SetSkyboxStatus(L"Sky: CS2 frame-stage bridge is unavailable.");
        return;
    }
    AtomicExchange(&g_pendingSkyboxRequest, -1);
    SetSkyboxStatus(L"Sky: restore queued for the next CS2 render frame...");
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
                    PackSkyboxStats(stats));
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
            g_lastBotStatsPacked = PackBotStats(stats);
            if (g_menuWindow)
                PostMessageW(g_menuWindow, WM_CAS_BOT_STATUS,
                    static_cast<WPARAM>(result),
                    PackBotStats(stats));
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
            g_lastBotStatsPacked = PackBotStats(stats);
            if (g_menuWindow)
                PostMessageW(g_menuWindow, WM_CAS_BOT_STATUS,
                    static_cast<WPARAM>(static_cast<LONG_PTR>(result)),
                    PackBotStats(stats));
        }
        else if (g_botHighlightEnabled &&
            ++g_botHighlightFrameCounter >= 4)
        {
            g_botHighlightFrameCounter = 0;
            BotHighlightStats stats{};
            const int result = UpdateBotHighlights(&stats);
            const LPARAM packedStats = PackBotStats(stats);
            if ((result != g_lastBotHighlightResult ||
                packedStats != g_lastBotStatsPacked) && g_menuWindow)
                PostMessageW(g_menuWindow, WM_CAS_BOT_STATUS,
                    static_cast<WPARAM>(static_cast<LONG_PTR>(result)),
                    packedStats);
            g_lastBotHighlightResult = result;
            g_lastBotStatsPacked = packedStats;
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
                    PackBotStats(stats));
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
    g_preResolvedSkyboxRuntimeReady = ResolveSkyboxRuntime(clientModule,
        &g_preResolvedSkyboxRuntime);
    BotHighlightRuntime resolvedBot{};
    if (ResolveBotHighlightRuntime(clientModule, &resolvedBot))
    {
        g_botHighlightRuntime = resolvedBot;
        g_botHighlightRuntimeReady = true;
    }
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

static unsigned int SkyStatAt(LPARAM packed, unsigned int shift,
    unsigned int mask)
{
    return static_cast<unsigned int>(
        (static_cast<unsigned long long>(packed) >> shift) & mask);
}

static void ShowSkyboxResult(int result, LPARAM packed)
{
    if (result == SKYBOX_APPLIED)
    {
        WideStatusBuilder status;
        status.length = 0;
        status.text[0] = 0;
        AppendStatusText(&status, L"Sky: applied | entities ");
        AppendStatusUnsigned(&status, SkyStatAt(packed, 24, 0xFFu));
        AppendStatusText(&status, L", writes ");
        AppendStatusUnsigned(&status, SkyStatAt(packed, 32, 0xFFu));
        AppendStatusText(&status, L", renderer ");
        AppendStatusUnsigned(&status, SkyStatAt(packed, 40, 0xFFu));
        SetSkyboxStatus(status.text);
    }
    else if (result == SKYBOX_RESTORED)
        SetSkyboxStatus(L"Sky: original material restored and confirmed.");
    else if (result == SKYBOX_ERR_MATERIAL)
        SetSkyboxStatus(L"Sky: VMaterialSystem2 could not load that .vmat.");
    else if (result == SKYBOX_ERR_ENTITY_SYSTEM)
        SetSkyboxStatus(L"Sky: CS2 entity system is not ready.");
    else if (result == SKYBOX_ERR_REFRESH)
        SetSkyboxStatus(L"Sky: runtime signature/layout validation failed.");
    else if (result == SKYBOX_ERR_NO_ENTITY)
    {
        WideStatusBuilder status;
        status.length = 0;
        status.text[0] = 0;
        AppendStatusText(&status, L"Sky: no env_sky | highest ");
        AppendStatusUnsigned(&status, SkyStatAt(packed, 0, 0xFFFFu));
        AppendStatusText(&status, L", identity stride ");
        AppendStatusUnsigned(&status, SkyStatAt(packed, 16, 0xFFu));
        SetSkyboxStatus(status.text);
    }
    else if (result == SKYBOX_ERR_WRITE)
        SetSkyboxStatus(L"Sky: handle write was not confirmed by renderer.");
    else if (result == SKYBOX_ERR_NOTHING_TO_RESTORE)
        SetSkyboxStatus(L"Sky: nothing captured on this map to restore.");
    else
        SetSkyboxStatus(L"Sky: request failed validation.");
}

static unsigned int BotStatAt(LPARAM packed, unsigned int shift)
{
    return static_cast<unsigned int>(
        (static_cast<unsigned long long>(packed) >> shift) & 0xFFu);
}

static void ShowBotHighlightResult(int result, LPARAM packed)
{
    if (result == BOT_HIGHLIGHT_ERR_SCHEMA)
    {
        SetBotStatus(L"Bots: CS2 schema validation failed; stayed off.");
        return;
    }
    if (result < 0)
    {
        SetBotStatus(L"Bots: runtime validation failed.");
        return;
    }

    WideStatusBuilder status;
    status.length = 0;
    status.text[0] = 0;
    if (result == BOT_HIGHLIGHT_ACTIVE)
        AppendStatusText(&status, L"Enemies: active (glow + red tint)");
    else if (result == BOT_HIGHLIGHT_DISABLED)
        AppendStatusText(&status, L"Enemies: disabled");
    else if (result == BOT_HIGHLIGHT_WAITING_LOCAL)
        AppendStatusText(&status, L"Enemies: waiting for your live team");
    else if (result == BOT_HIGHLIGHT_WAITING_BOTS)
        AppendStatusText(&status, L"Enemies: no live enemy players found");
    else if (result == BOT_HIGHLIGHT_WAITING_MAP)
        AppendStatusText(&status, L"Enemies: waiting for a loaded map");
    else if (result == BOT_HIGHLIGHT_RESTORE_PENDING)
        AppendStatusText(&status, L"Enemies: waiting to restore pawn state");
    else
        AppendStatusText(&status, L"Enemies: unknown state");

    if (result == BOT_HIGHLIGHT_DISABLED ||
        result == BOT_HIGHLIGHT_RESTORE_PENDING)
    {
        AppendStatusText(&status, L" | restored ");
        AppendStatusUnsigned(&status, BotStatAt(packed, 40));
    }
    else
    {
        AppendStatusText(&status, L" | controllers ");
        AppendStatusUnsigned(&status, BotStatAt(packed, 0));
        AppendStatusText(&status, L", team ");
        AppendStatusUnsigned(&status, BotStatAt(packed, 8));
        AppendStatusText(&status, L", bots ");
        AppendStatusUnsigned(&status, BotStatAt(packed, 16));
        AppendStatusText(&status, L", pawns ");
        AppendStatusUnsigned(&status, BotStatAt(packed, 24));
        AppendStatusText(&status, L", colored ");
        AppendStatusUnsigned(&status, BotStatAt(packed, 32));
    }
    SetBotStatus(status.text);
}

static LRESULT CALLBACK MenuWindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_POTATO_STATUS)
    {
        ShowSkyboxResult(static_cast<int>(static_cast<LONG_PTR>(wParam)),
            lParam);
        return 0;
    }
    if (msg == WM_CAS_BOT_STATUS)
    {
        const int result = static_cast<int>(static_cast<LONG_PTR>(wParam));
        if (result < 0 && g_botHighlightCheck)
            SendMessageW(g_botHighlightCheck, BM_SETCHECK,
                BST_UNCHECKED, 0);
        ShowBotHighlightResult(result, lParam);
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
    static const wchar_t kClassName[] = L"CasPlusOfflineVisualsMenu441";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = MenuWindowProc;
    wc.hInstance = instance;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<ULONG_PTR>(COLOR_BTNFACE + 1));
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    constexpr int kWidth = 540;
    constexpr int kHeight = 420;
    HWND wnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kClassName, L"CAS v2.3 - CS2 Visuals & Skybox",
        WS_POPUP | WS_BORDER, 0, 0, kWidth, kHeight, owner, nullptr, instance, nullptr);
    if (!wnd)
        return nullptr;

    // Header
    CreateWindowExW(0, L"STATIC", L"CAS v2.3 | Nixware Interface Replica (Injected DLL)", WS_CHILD | WS_VISIBLE | SS_CENTER,
        15, 12, 510, 22, wnd, nullptr, instance, nullptr);

    // Navigation Tabs (CLICKABLE)
    CreateWindowExW(0, L"BUTTON", L"Ragebot", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        15, 40, 96, 28, wnd, reinterpret_cast<HMENU>(static_cast<ULONG_PTR>(IDC_TAB_RAGEBOT)), instance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Anti-Aim", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        118, 40, 96, 28, wnd, reinterpret_cast<HMENU>(static_cast<ULONG_PTR>(IDC_TAB_ANTIAIM)), instance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Visuals", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        221, 40, 96, 28, wnd, reinterpret_cast<HMENU>(static_cast<ULONG_PTR>(IDC_TAB_VISUALS)), instance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Misc", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        324, 40, 96, 28, wnd, reinterpret_cast<HMENU>(static_cast<ULONG_PTR>(IDC_TAB_MISC)), instance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Configs", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        427, 40, 96, 28, wnd, reinterpret_cast<HMENU>(static_cast<ULONG_PTR>(IDC_TAB_CONFIGS)), instance, nullptr);

    // Section 1: World Customization
    CreateWindowExW(0, L"BUTTON", L"World Customization", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        15, 78, 510, 145, wnd, nullptr, instance, nullptr);
    CreateWindowExW(0, L"STATIC", L"Skybox Preset:", WS_CHILD | WS_VISIBLE,
        30, 102, 100, 22, wnd, nullptr, instance, nullptr);
    g_skyboxCombo = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
        135, 98, 370, 200, wnd, reinterpret_cast<HMENU>(static_cast<ULONG_PTR>(IDC_SKYBOX_COMBO)), instance, nullptr);
    if (g_skyboxCombo)
    {
        for (unsigned int i = 0; i < sizeof(kSkyboxes) / sizeof(kSkyboxes[0]); ++i)
            SendMessageW(g_skyboxCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kSkyboxes[i].name));
        SendMessageW(g_skyboxCombo, CB_SETCURSEL, 0, 0);
    }
    CreateWindowExW(0, L"BUTTON", L"Apply Skybox", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        135, 132, 175, 32, wnd, reinterpret_cast<HMENU>(static_cast<ULONG_PTR>(IDC_APPLY_SKYBOX)), instance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Restore Skybox", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        330, 132, 175, 32, wnd, reinterpret_cast<HMENU>(static_cast<ULONG_PTR>(IDC_RESTORE_SKYBOX)), instance, nullptr);
    g_skyboxStatusLabel = CreateWindowExW(0, L"STATIC",
        L"Sky: ready. Load a map, then choose a preset.",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        30, 172, 475, 40, wnd, nullptr, instance, nullptr);

    // Section 2: Enemy Highlighting & Features
    CreateWindowExW(0, L"BUTTON", L"Enemy Highlighting & Penetration", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        15, 230, 510, 145, wnd, nullptr, instance, nullptr);
    g_botHighlightCheck = CreateWindowExW(0, L"BUTTON",
        L"Highlight enemy players (glow + red tint)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        30, 254, 475, 26, wnd,
        reinterpret_cast<HMENU>(static_cast<ULONG_PTR>(IDC_BOT_HIGHLIGHT)),
        instance, nullptr);
    g_botStatusLabel = CreateWindowExW(0, L"STATIC",
        L"Enemies: disabled.",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        30, 284, 475, 45, wnd, nullptr, instance, nullptr);

    // Decorative Read-Only Checkboxes (Disabled)
    CreateWindowExW(0, L"BUTTON", L"Highlight penetrable surfaces", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_DISABLED,
        30, 335, 230, 24, wnd, nullptr, instance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Show penetration damage", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_DISABLED,
        270, 335, 230, 24, wnd, nullptr, instance, nullptr);

    // Footer
    CreateWindowExW(0, L"STATIC",
        L"Press Insert to toggle menu overlay",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        15, 386, 510, 20, wnd, nullptr, instance, nullptr);
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
    constexpr int kWidth = 540;
    constexpr int kHeight = 420;
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
    {
        SetSkyboxStatus(L"Sky: failed to install the frame-stage bridge.");
        SetBotStatus(L"Bots: failed to install the frame-stage bridge.");
    }
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
    return 0x00040401u;
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
