// Source 2 scene-system mesh renderer backend.
//
// Current backend target: CAnimatableSceneObjectDesc::Render in
// scenesystem.dll. Runtime writes are guarded by one confirmed scene-object
// owner layout, per-element batch classification and a suspended-thread patch
// barrier. Any layout/signature mismatch falls back to the original renderer.

using SceneDrawObjectFn = bool (__fastcall*)(void*, void*, void*, int,
    void*, void*, void*, void*);
using FlushInstructionCacheFn = BOOL (WINAPI*)(HANDLE, const void*, SIZE_T);
using CreateToolhelp32SnapshotFn = HANDLE (WINAPI*)(DWORD, DWORD);
using Thread32FirstFn = BOOL (WINAPI*)(HANDLE, void*);
using Thread32NextFn = BOOL (WINAPI*)(HANDLE, void*);
using OpenThreadFn = HANDLE (WINAPI*)(DWORD, BOOL, DWORD);
using SuspendThreadFn = DWORD (WINAPI*)(HANDLE);
using ResumeThreadFn = DWORD (WINAPI*)(HANDLE);
using GetCurrentThreadIdFn = DWORD (WINAPI*)();

static constexpr unsigned int kMeshDataStride = 0x68;
static constexpr unsigned int kMeshSceneObjectOffset = 0x18;
static constexpr unsigned int kMeshMaterialOffset = 0x20;
static constexpr unsigned int kMeshMaterial2Offset = 0x28;
static constexpr unsigned int kMeshColorOffset = 0x50;
// Confirmed by independent current CS2 scene/material implementations. Do not
// scan neighboring fields: a changed layout disables classification instead.
static constexpr unsigned int kSceneObjectOwnerHandleOffset = 0xB8;
static constexpr int kMaxMeshBatch = 128;
static constexpr unsigned int kDrawPatchSize = 14;
static constexpr int kMaxPatchThreads = 256;
static constexpr DWORD kTh32csSnapThread = 0x00000004UL;
static constexpr DWORD kThreadSuspendResume = 0x0002UL;
static constexpr DWORD kSuspendFailed = 0xFFFFFFFFUL;

struct ThreadEntry32Local {
    DWORD dwSize;
    DWORD cntUsage;
    DWORD th32ThreadID;
    DWORD th32OwnerProcessID;
    LONG tpBasePri;
    LONG tpDeltaPri;
    DWORD dwFlags;
};

struct PatchKernelApi {
    FlushInstructionCacheFn flushInstructionCache;
    CreateToolhelp32SnapshotFn createToolhelp32Snapshot;
    Thread32FirstFn thread32First;
    Thread32NextFn thread32Next;
    OpenThreadFn openThread;
    SuspendThreadFn suspendThread;
    ResumeThreadFn resumeThread;
    GetCurrentThreadIdFn getCurrentThreadId;
    bool ready;
};

struct MeshRenderBackendState {
    BYTE* drawObjectTarget;
    SceneDrawObjectFn originalDrawObject;
    BYTE originalPrologue[kDrawPatchSize];
    bool resolved;
    bool installed;
    bool installAttempted;
    unsigned int installAttempts;
    unsigned int drawCalls;
    unsigned int targetDraws;
    unsigned int pawnDraws;
    unsigned int weaponDraws;
    unsigned int armsDraws;
    unsigned int worldDraws;
    unsigned int visiblePasses;
    unsigned int occludedPasses;
    unsigned int stateRestores;
    unsigned int layoutRejects;
    unsigned int materialFallbacks;
    unsigned int mixedBatches;
    unsigned int patchFailures;
};

static MeshRenderBackendState g_meshRenderBackend{};
static PatchKernelApi g_patchKernelApi{};
static volatile LONG g_meshHookBypass = 0;

#pragma section(".cpxtrm", read, execute)
__declspec(allocate(".cpxtrm")) static BYTE g_drawObjectTrampoline[64] = { 0xCC };

static bool ResolvePatchKernelApi()
{
    if (g_patchKernelApi.ready)
        return true;
    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    if (!kernel)
        return false;
    g_patchKernelApi.flushInstructionCache =
        reinterpret_cast<FlushInstructionCacheFn>(
            GetProcAddress(kernel, "FlushInstructionCache"));
    g_patchKernelApi.createToolhelp32Snapshot =
        reinterpret_cast<CreateToolhelp32SnapshotFn>(
            GetProcAddress(kernel, "CreateToolhelp32Snapshot"));
    g_patchKernelApi.thread32First = reinterpret_cast<Thread32FirstFn>(
        GetProcAddress(kernel, "Thread32First"));
    g_patchKernelApi.thread32Next = reinterpret_cast<Thread32NextFn>(
        GetProcAddress(kernel, "Thread32Next"));
    g_patchKernelApi.openThread = reinterpret_cast<OpenThreadFn>(
        GetProcAddress(kernel, "OpenThread"));
    g_patchKernelApi.suspendThread = reinterpret_cast<SuspendThreadFn>(
        GetProcAddress(kernel, "SuspendThread"));
    g_patchKernelApi.resumeThread = reinterpret_cast<ResumeThreadFn>(
        GetProcAddress(kernel, "ResumeThread"));
    g_patchKernelApi.getCurrentThreadId = reinterpret_cast<GetCurrentThreadIdFn>(
        GetProcAddress(kernel, "GetCurrentThreadId"));
    g_patchKernelApi.ready = g_patchKernelApi.flushInstructionCache &&
        g_patchKernelApi.createToolhelp32Snapshot &&
        g_patchKernelApi.thread32First && g_patchKernelApi.thread32Next &&
        g_patchKernelApi.openThread && g_patchKernelApi.suspendThread &&
        g_patchKernelApi.resumeThread && g_patchKernelApi.getCurrentThreadId;
    return g_patchKernelApi.ready;
}

static bool FlushExecutableRange(const void* address, SIZE_T size)
{
    if (!ResolvePatchKernelApi())
        return false;
    return g_patchKernelApi.flushInstructionCache(
        reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1)), address, size) != 0;
}

static void WriteAbsoluteIndirectJump(BYTE* destination, const void* target)
{
    // jmp qword ptr [rip+0]; dq target -- clobbers no GPR.
    destination[0] = 0xFF;
    destination[1] = 0x25;
    destination[2] = 0x00;
    destination[3] = 0x00;
    destination[4] = 0x00;
    destination[5] = 0x00;
    *reinterpret_cast<unsigned long long*>(destination + 6) =
        reinterpret_cast<unsigned long long>(target);
}

struct SuspendedThreadSet {
    HANDLE handles[kMaxPatchThreads];
    DWORD ids[kMaxPatchThreads];
    int count;
};

static bool PatchThreadAlreadySuspended(const SuspendedThreadSet& threads,
    DWORD threadId)
{
    for (int i = 0; i < threads.count; ++i)
        if (threads.ids[i] == threadId)
            return true;
    return false;
}

static void ResumePatchThreads(SuspendedThreadSet* threads)
{
    if (!threads || !ResolvePatchKernelApi())
        return;
    for (int i = threads->count - 1; i >= 0; --i)
    {
        if (threads->handles[i])
        {
            g_patchKernelApi.resumeThread(threads->handles[i]);
            CloseHandle(threads->handles[i]);
            threads->handles[i] = nullptr;
        }
    }
    threads->count = 0;
}

static bool SuspendOtherProcessThreads(SuspendedThreadSet* threads)
{
    if (!threads || !ResolvePatchKernelApi())
        return false;
    threads->count = 0;
    const DWORD processId = GetCurrentProcessId();
    const DWORD currentThreadId = g_patchKernelApi.getCurrentThreadId();

    // Repeat enumeration until one complete pass discovers no new threads.
    // This closes the ordinary snapshot race without keeping threads suspended
    // longer than the tiny VirtualProtect/write/flush patch window.
    for (int pass = 0; pass < 4; ++pass)
    {
        HANDLE snapshot = g_patchKernelApi.createToolhelp32Snapshot(
            kTh32csSnapThread, 0);
        if (!snapshot || snapshot ==
            reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1)))
        {
            ResumePatchThreads(threads);
            return false;
        }

        ThreadEntry32Local entry{};
        entry.dwSize = sizeof(entry);
        bool foundNewThread = false;
        BOOL hasEntry = g_patchKernelApi.thread32First(snapshot, &entry);
        while (hasEntry)
        {
            if (entry.th32OwnerProcessID == processId &&
                entry.th32ThreadID != currentThreadId &&
                !PatchThreadAlreadySuspended(*threads, entry.th32ThreadID))
            {
                if (threads->count >= kMaxPatchThreads)
                {
                    CloseHandle(snapshot);
                    ResumePatchThreads(threads);
                    return false;
                }
                HANDLE thread = g_patchKernelApi.openThread(
                    kThreadSuspendResume, 0, entry.th32ThreadID);
                if (!thread)
                {
                    CloseHandle(snapshot);
                    ResumePatchThreads(threads);
                    return false;
                }
                if (g_patchKernelApi.suspendThread(thread) == kSuspendFailed)
                {
                    CloseHandle(thread);
                    CloseHandle(snapshot);
                    ResumePatchThreads(threads);
                    return false;
                }
                threads->handles[threads->count] = thread;
                threads->ids[threads->count] = entry.th32ThreadID;
                ++threads->count;
                foundNewThread = true;
            }
            entry.dwSize = sizeof(entry);
            hasEntry = g_patchKernelApi.thread32Next(snapshot, &entry);
        }
        CloseHandle(snapshot);
        if (!foundNewThread)
            return true;
    }

    ResumePatchThreads(threads);
    return false;
}

static bool BytesEqual(const BYTE* left, const BYTE* right, SIZE_T count)
{
    if (!left || !right)
        return false;
    for (SIZE_T i = 0; i < count; ++i)
        if (left[i] != right[i])
            return false;
    return true;
}

static bool PatchExecutableBytesSafely(BYTE* target, const BYTE* expected,
    const BYTE* replacement, SIZE_T count)
{
    if (!target || !expected || !replacement || count == 0 ||
        !IsExecutable(target) || !ResolvePatchKernelApi())
        return false;

    SuspendedThreadSet threads{};
    if (!SuspendOtherProcessThreads(&threads))
        return false;

    bool success = false;
    DWORD oldProtection = 0;
    if (BytesEqual(target, expected, count) &&
        VirtualProtect(target, count, PAGE_EXECUTE_READWRITE, &oldProtection))
    {
        for (SIZE_T i = 0; i < count; ++i)
            target[i] = replacement[i];
        const bool flushed = g_patchKernelApi.flushInstructionCache(
            reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1)),
            target, count) != 0;
        DWORD ignored = 0;
        const bool protectionRestored =
            VirtualProtect(target, count, oldProtection, &ignored) != 0;
        success = flushed && protectionRestored;
        if (!success)
        {
            DWORD rollbackProtection = 0;
            if (VirtualProtect(target, count, PAGE_EXECUTE_READWRITE,
                &rollbackProtection))
            {
                for (SIZE_T i = 0; i < count; ++i)
                    target[i] = expected[i];
                g_patchKernelApi.flushInstructionCache(
                    reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1)),
                    target, count);
                VirtualProtect(target, count, rollbackProtection, &ignored);
            }
        }
    }

    ResumePatchThreads(&threads);
    return success;
}

static bool ResolveMeshRenderBackend()
{
    g_meshRenderBackend.drawObjectTarget = nullptr;
    g_meshRenderBackend.resolved = false;

    HMODULE sceneSystem = GetModuleHandleW(L"scenesystem.dll");
    if (!sceneSystem)
        return false;

    static const int kDrawObjectPattern[] = {
        0x48, 0x8B, 0xC4, 0x53, 0x57, 0x41, 0x54,
        0x48, 0x81, 0xEC, 0xD0, 0x00, 0x00, 0x00, 0x49, 0x63
    };
    BYTE* target = FindUniquePattern(sceneSystem, kDrawObjectPattern,
        sizeof(kDrawObjectPattern) / sizeof(kDrawObjectPattern[0]));
    if (!target || !IsExecutable(target))
        return false;

    static const int kSafePrologue[] = {
        0x48, 0x8B, 0xC4, 0x53, 0x57, 0x41, 0x54,
        0x48, 0x81, 0xEC, 0xD0, 0x00, 0x00, 0x00
    };
    if (!PatternMatchesAt(target, kSafePrologue,
        sizeof(kSafePrologue) / sizeof(kSafePrologue[0])))
        return false;

    g_meshRenderBackend.drawObjectTarget = target;
    g_meshRenderBackend.resolved = true;
    return true;
}

static bool VisualOwnerFromSceneObject(void* sceneObject,
    const VisualTargetEntry* snapshot, int snapshotCount,
    unsigned int* ownerHandle, VisualTargetKind* targetKind)
{
    if (!sceneObject || !snapshot || snapshotCount <= 0 || !ownerHandle ||
        !targetKind || !IsAccessible(sceneObject,
            kSceneObjectOwnerHandleOffset + sizeof(unsigned int), false))
        return false;

    const unsigned int handle = *reinterpret_cast<unsigned int*>(
        reinterpret_cast<BYTE*>(sceneObject) + kSceneObjectOwnerHandleOffset);
    const VisualTargetKind kind = VisualTargetKindFromSnapshot(handle,
        snapshot, snapshotCount);
    if (kind == VISUAL_TARGET_NONE || !VisualTargetKindEnabled(kind))
        return false;
    *ownerHandle = handle;
    *targetKind = kind;
    return true;
}

struct MeshElementClassification {
    unsigned int ownerHandle;
    VisualTargetKind kind;
};

static bool ClassifyMeshBatch(void* meshDraw, int dataCount,
    const VisualTargetEntry* snapshot, int snapshotCount,
    MeshElementClassification* classifications, bool* anyTarget)
{
    if (!meshDraw || dataCount <= 0 || dataCount > kMaxMeshBatch ||
        !snapshot || snapshotCount < 0 || !classifications || !anyTarget)
        return false;
    *anyTarget = false;
    BYTE* base = reinterpret_cast<BYTE*>(meshDraw);
    for (int i = 0; i < dataCount; ++i)
    {
        classifications[i].ownerHandle = 0xFFFFFFFFu;
        classifications[i].kind = VISUAL_TARGET_NONE;
        BYTE* mesh = base + static_cast<SIZE_T>(i) * kMeshDataStride;
        if (!IsAccessible(mesh, kMeshDataStride, false))
            return false;
        void** sceneSlot = reinterpret_cast<void**>(
            mesh + kMeshSceneObjectOffset);
        if (!IsAccessible(sceneSlot, sizeof(void*), false) || !*sceneSlot)
            continue;
        if (VisualOwnerFromSceneObject(*sceneSlot, snapshot, snapshotCount,
            &classifications[i].ownerHandle, &classifications[i].kind))
            *anyTarget = true;
    }
    return true;
}

static void CountTargetDraw(VisualTargetKind kind)
{
    ++g_meshRenderBackend.targetDraws;
    if (IsPlayerPawnTargetKind(kind))
        ++g_meshRenderBackend.pawnDraws;
    else if (IsPlayerWeaponTargetKind(kind))
        ++g_meshRenderBackend.weaponDraws;
    else if (kind == VISUAL_TARGET_LOCAL_ARMS)
        ++g_meshRenderBackend.armsDraws;
    else
        ++g_meshRenderBackend.worldDraws;
}

struct SavedMeshState {
    void* material;
    void* material2;
    BYTE color[4];
};

static bool CaptureMeshBatch(void* meshDraw, int dataCount,
    SavedMeshState* saved)
{
    if (!meshDraw || !saved || dataCount <= 0 || dataCount > kMaxMeshBatch)
        return false;
    BYTE* base = reinterpret_cast<BYTE*>(meshDraw);
    for (int i = 0; i < dataCount; ++i)
    {
        BYTE* mesh = base + static_cast<SIZE_T>(i) * kMeshDataStride;
        if (!IsAccessible(mesh, kMeshDataStride, true))
            return false;
        void** material = reinterpret_cast<void**>(mesh + kMeshMaterialOffset);
        void** material2 = reinterpret_cast<void**>(mesh + kMeshMaterial2Offset);
        BYTE* color = mesh + kMeshColorOffset;
        if (!IsAccessible(material, sizeof(void*), true) ||
            !IsAccessible(material2, sizeof(void*), true) ||
            !IsAccessible(color, 4, true))
            return false;
        saved[i].material = *material;
        saved[i].material2 = *material2;
        saved[i].color[0] = color[0];
        saved[i].color[1] = color[1];
        saved[i].color[2] = color[2];
        saved[i].color[3] = color[3];
    }
    return true;
}

static void ApplyMeshBatchMaterial(void* meshDraw, int dataCount,
    void* materialValue, const RGBVal& colorValue)
{
    BYTE* base = reinterpret_cast<BYTE*>(meshDraw);
    for (int i = 0; i < dataCount; ++i)
    {
        BYTE* mesh = base + static_cast<SIZE_T>(i) * kMeshDataStride;
        *reinterpret_cast<void**>(mesh + kMeshMaterialOffset) = materialValue;
        *reinterpret_cast<void**>(mesh + kMeshMaterial2Offset) = materialValue;
        BYTE* color = mesh + kMeshColorOffset;
        color[0] = colorValue.r;
        color[1] = colorValue.g;
        color[2] = colorValue.b;
        color[3] = 255;
    }
}

static void ApplyMeshBatchColor(void* meshDraw, int dataCount,
    const RGBVal& colorValue)
{
    BYTE* base = reinterpret_cast<BYTE*>(meshDraw);
    for (int i = 0; i < dataCount; ++i)
    {
        BYTE* color = base + static_cast<SIZE_T>(i) *
            kMeshDataStride + kMeshColorOffset;
        color[0] = colorValue.r;
        color[1] = colorValue.g;
        color[2] = colorValue.b;
        color[3] = 255;
    }
}

static void RestoreMeshBatch(void* meshDraw, int dataCount,
    const SavedMeshState* saved)
{
    if (!meshDraw || !saved || dataCount <= 0 || dataCount > kMaxMeshBatch)
        return;
    BYTE* base = reinterpret_cast<BYTE*>(meshDraw);
    for (int i = 0; i < dataCount; ++i)
    {
        BYTE* mesh = base + static_cast<SIZE_T>(i) * kMeshDataStride;
        if (!IsAccessible(mesh, kMeshDataStride, true))
            continue;
        *reinterpret_cast<void**>(mesh + kMeshMaterialOffset) = saved[i].material;
        *reinterpret_cast<void**>(mesh + kMeshMaterial2Offset) = saved[i].material2;
        BYTE* color = mesh + kMeshColorOffset;
        color[0] = saved[i].color[0];
        color[1] = saved[i].color[1];
        color[2] = saved[i].color[2];
        color[3] = saved[i].color[3];
    }
    ++g_meshRenderBackend.stateRestores;
}

struct MeshRenderStateGuard {
    void* meshDraw;
    int dataCount;
    bool captured;
    bool dirty;
    SavedMeshState saved[kMaxMeshBatch];

    bool Capture(void* draw, int count)
    {
        meshDraw = draw;
        dataCount = count;
        dirty = false;
        captured = CaptureMeshBatch(draw, count, saved);
        return captured;
    }

    void ApplyMaterial(void* materialValue, const RGBVal& colorValue)
    {
        if (!captured)
            return;
        ApplyMeshBatchMaterial(meshDraw, dataCount, materialValue, colorValue);
        dirty = true;
    }

    void ApplyColor(const RGBVal& colorValue)
    {
        if (!captured)
            return;
        ApplyMeshBatchColor(meshDraw, dataCount, colorValue);
        dirty = true;
    }

    ~MeshRenderStateGuard()
    {
        if (captured && dirty)
            RestoreMeshBatch(meshDraw, dataCount, saved);
    }
};

static bool RuntimeMaterialPointerUsable(void* material)
{
    if (!material || !IsAccessible(material, sizeof(void*), false))
        return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(material, &mbi, sizeof(mbi)) != sizeof(mbi) ||
        mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
        return false;
    void* vtable = *reinterpret_cast<void**>(material);
    if (!IsAccessible(vtable, sizeof(void*), false))
        return false;
    void* firstVirtual = *reinterpret_cast<void**>(vtable);
    return IsExecutable(firstVirtual);
}

static bool SelectedVisibleChamsMaterialUsable()
{
    return g_materialManager.materialsReady &&
        RuntimeMaterialPointerUsable(g_materialManager.flatVisible);
}

static bool SelectedOccludedChamsMaterialUsable()
{
    return g_materialManager.materialsReady &&
        RuntimeMaterialPointerUsable(g_materialManager.flatOccluded);
}

static bool RenderTargetMeshRun(SceneDrawObjectFn original, void* a1, void* a2,
    void* meshRun, int runCount, VisualTargetKind targetKind,
    void* a5, void* a6, void* a7, void* a8, bool* resultOut)
{
    if (!original || !meshRun || runCount <= 0 || !resultOut)
        return false;

    MeshRenderStateGuard stateGuard;
    stateGuard.captured = false;
    stateGuard.dirty = false;
    if (!stateGuard.Capture(meshRun, runCount))
        return false;

    const bool wantVisible = g_espConfig.chamsStyle == 0 ||
        g_espConfig.chamsStyle == 1;
    const bool wantOccluded = (g_espConfig.chamsStyle == 0 ||
        g_espConfig.chamsStyle == 2) &&
        VisualTargetAllowsOccludedPass(targetKind);
    const bool visibleMaterialReady = SelectedVisibleChamsMaterialUsable();
    const bool occludedMaterialReady = SelectedOccludedChamsMaterialUsable();

    bool result = false;
    bool drew = false;
    if (wantOccluded && occludedMaterialReady)
    {
        stateGuard.ApplyMaterial(g_materialManager.flatOccluded,
            g_espConfig.chamsColorOccluded);
        ++g_meshRenderBackend.occludedPasses;
        result = original(a1, a2, meshRun, runCount, a5, a6, a7, a8);
        drew = true;
    }
    else if (wantOccluded)
    {
        ++g_meshRenderBackend.materialFallbacks;
    }

    if (wantVisible)
    {
        if (visibleMaterialReady)
            stateGuard.ApplyMaterial(g_materialManager.flatVisible,
                g_espConfig.chamsColorVisible);
        else
        {
            stateGuard.ApplyColor(g_espConfig.chamsColorVisible);
            ++g_meshRenderBackend.materialFallbacks;
        }
        ++g_meshRenderBackend.visiblePasses;
        result = original(a1, a2, meshRun, runCount, a5, a6, a7, a8);
        drew = true;
    }

    if (!drew)
        result = original(a1, a2, meshRun, runCount, a5, a6, a7, a8);
    *resultOut = result;
    return true;
}

static bool __fastcall MeshDrawObjectHook(void* a1, void* a2, void* meshDraw,
    int dataCount, void* a5, void* a6, void* a7, void* a8)
{
    SceneDrawObjectFn original = g_meshRenderBackend.originalDrawObject;
    if (!original)
        return false;

    ++g_meshRenderBackend.drawCalls;
    if (g_meshHookBypass != 0 || !meshDraw || dataCount <= 0 ||
        dataCount > kMaxMeshBatch || !g_espConfig.enable ||
        !g_espConfig.chams || g_espConfig.chamsStyle == 3)
        return original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);

    VisualTargetEntry snapshot[kMaxVisualTargets];
    unsigned int snapshotGeneration = 0;
    const int snapshotCount = CopyVisualTargetSnapshot(snapshot,
        kMaxVisualTargets, &snapshotGeneration);
    if (snapshotCount <= 0)
        return original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);

    MeshElementClassification classifications[kMaxMeshBatch];
    bool anyTarget = false;
    if (!ClassifyMeshBatch(meshDraw, dataCount, snapshot, snapshotCount,
        classifications, &anyTarget))
    {
        ++g_meshRenderBackend.layoutRejects;
        return original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);
    }
    if (!anyTarget)
        return original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);

    bool mixed = false;
    for (int i = 1; i < dataCount; ++i)
    {
        if (classifications[i].kind != classifications[0].kind)
        {
            mixed = true;
            break;
        }
    }
    if (mixed)
        ++g_meshRenderBackend.mixedBatches;

    BYTE* base = reinterpret_cast<BYTE*>(meshDraw);
    bool result = false;
    int start = 0;
    while (start < dataCount)
    {
        const VisualTargetKind runKind = classifications[start].kind;
        int end = start + 1;
        while (end < dataCount && classifications[end].kind == runKind)
            ++end;
        const int runCount = end - start;
        void* run = base + static_cast<SIZE_T>(start) * kMeshDataStride;

        if (runKind == VISUAL_TARGET_NONE)
        {
            result = original(a1, a2, run, runCount, a5, a6, a7, a8);
        }
        else
        {
            CountTargetDraw(runKind);
            if (!RenderTargetMeshRun(original, a1, a2, run, runCount,
                runKind, a5, a6, a7, a8, &result))
            {
                ++g_meshRenderBackend.layoutRejects;
                result = original(a1, a2, run, runCount,
                    a5, a6, a7, a8);
            }
        }
        start = end;
    }
    (void)snapshotGeneration;
    return result;
}

static bool InstallMeshRenderBackend()
{
    if (g_meshRenderBackend.installed)
        return true;

    if (!g_meshRenderBackend.resolved)
    {
        if (g_meshRenderBackend.installAttempted)
            return false;
        // Missing scenesystem.dll is transient and does not consume the single
        // structural signature attempt. Once the module exists, a mismatch is
        // treated as a build/layout change and remains fail-closed.
        if (!GetModuleHandleW(L"scenesystem.dll"))
            return false;
        g_meshRenderBackend.installAttempted = true;
        if (!ResolveMeshRenderBackend())
            return false;
    }

    if (g_meshRenderBackend.installAttempts >= 3)
        return false;
    ++g_meshRenderBackend.installAttempts;

    BYTE* target = g_meshRenderBackend.drawObjectTarget;
    if (!target || !IsExecutable(target) || !ResolvePatchKernelApi())
        return false;

    for (unsigned int i = 0; i < kDrawPatchSize; ++i)
        g_meshRenderBackend.originalPrologue[i] = target[i];

    DWORD trampolineProtection = 0;
    if (!VirtualProtect(g_drawObjectTrampoline, sizeof(g_drawObjectTrampoline),
        PAGE_EXECUTE_READWRITE, &trampolineProtection))
        return false;
    for (unsigned int i = 0; i < kDrawPatchSize; ++i)
        g_drawObjectTrampoline[i] = target[i];
    WriteAbsoluteIndirectJump(g_drawObjectTrampoline + kDrawPatchSize,
        target + kDrawPatchSize);
    const bool trampolineFlushed = FlushExecutableRange(
        g_drawObjectTrampoline, kDrawPatchSize * 2);
    DWORD ignored = 0;
    VirtualProtect(g_drawObjectTrampoline, sizeof(g_drawObjectTrampoline),
        trampolineProtection, &ignored);
    if (!trampolineFlushed)
        return false;

    g_meshRenderBackend.originalDrawObject = reinterpret_cast<SceneDrawObjectFn>(
        reinterpret_cast<void*>(&g_drawObjectTrampoline[0]));

    BYTE patch[kDrawPatchSize];
    WriteAbsoluteIndirectJump(patch,
        reinterpret_cast<const void*>(&MeshDrawObjectHook));
    if (!PatchExecutableBytesSafely(target,
        g_meshRenderBackend.originalPrologue, patch, kDrawPatchSize))
    {
        ++g_meshRenderBackend.patchFailures;
        return false;
    }

    AtomicExchange(&g_meshHookBypass, 0);
    g_meshRenderBackend.installed = true;
    return true;
}

static bool RemoveMeshRenderBackend()
{
    AtomicExchange(&g_meshHookBypass, 1);
    if (!g_meshRenderBackend.installed ||
        !g_meshRenderBackend.drawObjectTarget)
    {
        g_meshRenderBackend.installed = false;
        return true;
    }

    BYTE patch[kDrawPatchSize];
    WriteAbsoluteIndirectJump(patch,
        reinterpret_cast<const void*>(&MeshDrawObjectHook));
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        if (PatchExecutableBytesSafely(g_meshRenderBackend.drawObjectTarget,
            patch, g_meshRenderBackend.originalPrologue, kDrawPatchSize))
        {
            g_meshRenderBackend.installed = false;
            return true;
        }
        ++g_meshRenderBackend.patchFailures;
        Sleep(1);
    }

    // Leave the trampoline/original pointer intact. If unpatching could not get
    // a safe patch window, the installed detour remains in bypass-only mode and
    // forwards every call unchanged instead of risking a partial restore.
    return false;
}
