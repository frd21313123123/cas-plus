// Source 2 scene-system mesh renderer backend.
//
// Current backend target: CAnimatableSceneObjectDesc::Render in
// scenesystem.dll. Runtime writes are guarded by one confirmed scene-object
// owner layout, per-element batch verification, RIP-aware patching and
// per-target render confirmations. Any ambiguity renders the original batch.

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

struct alignas(16) ThreadContext64 {
    unsigned long long P1Home;
    unsigned long long P2Home;
    unsigned long long P3Home;
    unsigned long long P4Home;
    unsigned long long P5Home;
    unsigned long long P6Home;
    DWORD ContextFlags;
    DWORD MxCsr;
    WORD SegCs;
    WORD SegDs;
    WORD SegEs;
    WORD SegFs;
    WORD SegGs;
    WORD SegSs;
    DWORD EFlags;
    unsigned long long Dr0;
    unsigned long long Dr1;
    unsigned long long Dr2;
    unsigned long long Dr3;
    unsigned long long Dr6;
    unsigned long long Dr7;
    unsigned long long Rax;
    unsigned long long Rcx;
    unsigned long long Rdx;
    unsigned long long Rbx;
    unsigned long long Rsp;
    unsigned long long Rbp;
    unsigned long long Rsi;
    unsigned long long Rdi;
    unsigned long long R8;
    unsigned long long R9;
    unsigned long long R10;
    unsigned long long R11;
    unsigned long long R12;
    unsigned long long R13;
    unsigned long long R14;
    unsigned long long R15;
    unsigned long long Rip;
    BYTE extendedState[0x3D0];
};
static_assert(sizeof(ThreadContext64) == 0x4D0,
    "x64 CONTEXT layout used by GetThreadContext changed");

using GetThreadContextFn = BOOL (WINAPI*)(HANDLE, ThreadContext64*);

extern "C" LONG _InterlockedIncrement(volatile LONG*);
extern "C" LONG _InterlockedDecrement(volatile LONG*);
#pragma intrinsic(_InterlockedIncrement)
#pragma intrinsic(_InterlockedDecrement)

static constexpr unsigned int kMeshDataStride = 0x68;
static constexpr unsigned int kMeshSceneObjectOffset = 0x18;
static constexpr unsigned int kMeshMaterialOffset = 0x20;
static constexpr unsigned int kMeshMaterial2Offset = 0x28;
static constexpr unsigned int kMeshColorOffset = 0x50;
static constexpr unsigned int kSceneObjectOwnerHandleOffset = 0xB8;
static constexpr int kMaxMeshBatch = 128;
static constexpr unsigned int kDrawPatchSize = 14;
static constexpr int kMaxPatchThreads = 256;
static constexpr int kMaxMeshConfirmations = 256;
static constexpr DWORD kTh32csSnapThread = 0x00000004UL;
static constexpr DWORD kThreadSuspendResume = 0x0002UL;
static constexpr DWORD kThreadGetContext = 0x0008UL;
static constexpr DWORD kPatchThreadRights =
    kThreadSuspendResume | kThreadGetContext;
static constexpr DWORD kSuspendFailed = 0xFFFFFFFFUL;
static constexpr DWORD kContextAmd64Control = 0x00100001UL;

enum PatchWriteResult {
    PATCH_WRITE_FAILED = -1,
    PATCH_WRITE_BUSY = 0,
    PATCH_WRITE_SUCCESS = 1
};

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
    GetThreadContextFn getThreadContext;
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
    unsigned int ripDeferrals;
    unsigned int snapshotMisses;
    unsigned int ownershipRejects;
    unsigned int quiescenceWaits;
};

struct MeshTargetConfirmation {
    unsigned int handle;
    unsigned int kind;
    unsigned int successGeneration;
    unsigned int rejectGeneration;
    BYTE visibleConfirmed;
    BYTE occludedConfirmed;
    BYTE hasSuccess;
    BYTE hasReject;
};

static MeshRenderBackendState g_meshRenderBackend{};
static PatchKernelApi g_patchKernelApi{};
static volatile LONG g_meshHookBypass = 1;
static volatile LONG g_meshHookInFlight = 0;
static volatile LONG g_meshConfirmationLock = 0;
static MeshTargetConfirmation g_meshConfirmations[kMaxMeshConfirmations]{};

#pragma section(".cpxtrm", read, execute)
__declspec(allocate(".cpxtrm")) static BYTE g_drawObjectTrampoline[64] = { 0xCC };

static bool ReadableCommittedRange(const void* address, SIZE_T bytes)
{
    if (!address || bytes == 0)
        return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(address, &mbi, sizeof(mbi)) != sizeof(mbi) ||
        mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
        return false;
    const ULONG_PTR start = reinterpret_cast<ULONG_PTR>(address);
    const ULONG_PTR region = reinterpret_cast<ULONG_PTR>(mbi.BaseAddress);
    if (start < region)
        return false;
    const SIZE_T offset = static_cast<SIZE_T>(start - region);
    return offset <= mbi.RegionSize && bytes <= mbi.RegionSize - offset;
}

static bool WritableCommittedRange(void* address, SIZE_T bytes)
{
    if (!ReadableCommittedRange(address, bytes))
        return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(address, &mbi, sizeof(mbi)) != sizeof(mbi))
        return false;
    const DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY |
        PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (mbi.Protect & writable) != 0;
}

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
    g_patchKernelApi.getThreadContext = reinterpret_cast<GetThreadContextFn>(
        GetProcAddress(kernel, "GetThreadContext"));
    g_patchKernelApi.ready = g_patchKernelApi.flushInstructionCache &&
        g_patchKernelApi.createToolhelp32Snapshot &&
        g_patchKernelApi.thread32First && g_patchKernelApi.thread32Next &&
        g_patchKernelApi.openThread && g_patchKernelApi.suspendThread &&
        g_patchKernelApi.resumeThread && g_patchKernelApi.getCurrentThreadId &&
        g_patchKernelApi.getThreadContext;
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
        HANDLE thread = threads->handles[i];
        if (!thread)
            continue;
        g_patchKernelApi.resumeThread(thread);
        CloseHandle(thread);
        threads->handles[i] = nullptr;
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
        BOOL hasEntry = g_patchKernelApi.thread32First(snapshot, &entry);
        if (!hasEntry)
        {
            CloseHandle(snapshot);
            ResumePatchThreads(threads);
            return false;
        }

        bool foundNewThread = false;
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
                    kPatchThreadRights, 0, entry.th32ThreadID);
                if (!thread ||
                    g_patchKernelApi.suspendThread(thread) == kSuspendFailed)
                {
                    if (thread)
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

static bool SuspendedThreadsAvoidRange(const SuspendedThreadSet& threads,
    const BYTE* rangeStart, SIZE_T rangeSize, bool* busy)
{
    if (busy)
        *busy = false;
    if (!rangeStart || rangeSize == 0 || !ResolvePatchKernelApi())
        return false;
    const unsigned long long start =
        reinterpret_cast<unsigned long long>(rangeStart);
    const unsigned long long end = start + rangeSize;
    for (int i = 0; i < threads.count; ++i)
    {
        ThreadContext64 context{};
        context.ContextFlags = kContextAmd64Control;
        if (!g_patchKernelApi.getThreadContext(threads.handles[i], &context))
            return false;
        if (context.Rip >= start && context.Rip < end)
        {
            if (busy)
                *busy = true;
            return false;
        }
    }
    return true;
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

static PatchWriteResult PatchExecutableBytesSafely(BYTE* target,
    const BYTE* expected, const BYTE* replacement, SIZE_T count)
{
    if (!target || !expected || !replacement || count == 0 ||
        !IsExecutable(target) || !ResolvePatchKernelApi())
        return PATCH_WRITE_FAILED;

    SuspendedThreadSet threads{};
    if (!SuspendOtherProcessThreads(&threads))
        return PATCH_WRITE_FAILED;

    bool busy = false;
    if (!SuspendedThreadsAvoidRange(threads, target, count, &busy))
    {
        ResumePatchThreads(&threads);
        return busy ? PATCH_WRITE_BUSY : PATCH_WRITE_FAILED;
    }

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
                VirtualProtect(target, count, oldProtection, &ignored);
            }
        }
    }

    ResumePatchThreads(&threads);
    return success ? PATCH_WRITE_SUCCESS : PATCH_WRITE_FAILED;
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

static bool TryLockMeshConfirmations()
{
    return _InterlockedCompareExchange(&g_meshConfirmationLock, 1, 0) == 0;
}

static void UnlockMeshConfirmations()
{
    AtomicExchange(&g_meshConfirmationLock, 0);
}

static int FindMeshConfirmationSlot(unsigned int handle, VisualTargetKind kind,
    bool allowEmpty)
{
    int empty = -1;
    for (int i = 0; i < kMaxMeshConfirmations; ++i)
    {
        if (g_meshConfirmations[i].handle == handle &&
            g_meshConfirmations[i].kind == static_cast<unsigned int>(kind))
            return i;
        if (allowEmpty && empty < 0 && g_meshConfirmations[i].handle == 0)
            empty = i;
    }
    return empty;
}

static void RecordMeshTargetConfirmation(unsigned int handle,
    VisualTargetKind kind, unsigned int generation, bool visible,
    bool occluded)
{
    if (handle == 0 || handle == 0xFFFFFFFFu || kind == VISUAL_TARGET_NONE ||
        (!visible && !occluded) || !TryLockMeshConfirmations())
        return;
    const int slot = FindMeshConfirmationSlot(handle, kind, true);
    if (slot >= 0)
    {
        MeshTargetConfirmation& entry = g_meshConfirmations[slot];
        if (entry.handle != handle ||
            entry.kind != static_cast<unsigned int>(kind) ||
            !entry.hasSuccess || entry.successGeneration != generation)
        {
            entry.handle = handle;
            entry.kind = static_cast<unsigned int>(kind);
            entry.successGeneration = generation;
            entry.visibleConfirmed = 0;
            entry.occludedConfirmed = 0;
            entry.hasSuccess = 1;
        }
        if (visible)
            entry.visibleConfirmed = 1;
        if (occluded)
            entry.occludedConfirmed = 1;
    }
    UnlockMeshConfirmations();
}

static void RejectMeshTargetConfirmation(unsigned int handle,
    VisualTargetKind kind, unsigned int generation)
{
    if (handle == 0 || handle == 0xFFFFFFFFu || kind == VISUAL_TARGET_NONE ||
        !TryLockMeshConfirmations())
        return;
    const int slot = FindMeshConfirmationSlot(handle, kind, true);
    if (slot >= 0)
    {
        MeshTargetConfirmation& entry = g_meshConfirmations[slot];
        entry.handle = handle;
        entry.kind = static_cast<unsigned int>(kind);
        entry.rejectGeneration = generation;
        entry.hasReject = 1;
        ++g_meshRenderBackend.ownershipRejects;
    }
    UnlockMeshConfirmations();
}

static void RejectClassifiedMeshTargets(
    const struct MeshElementClassification* classifications, int dataCount,
    unsigned int generation);

static bool MeshTargetPassRecentlyConfirmed(unsigned int handle,
    VisualTargetKind kind, bool occludedPass)
{
    if (!g_meshRenderBackend.installed || g_meshHookBypass != 0 ||
        handle == 0 || handle == 0xFFFFFFFFu || kind == VISUAL_TARGET_NONE)
        return false;

    unsigned int currentGeneration = 0;
    if (!ReadPublishedVisualGeneration(&currentGeneration) ||
        !TryLockMeshConfirmations())
        return false;

    bool confirmed = false;
    const int slot = FindMeshConfirmationSlot(handle, kind, false);
    if (slot >= 0)
    {
        const MeshTargetConfirmation& entry = g_meshConfirmations[slot];
        const unsigned int successAge =
            currentGeneration - entry.successGeneration;
        const bool recentSuccess = entry.hasSuccess && successAge <= 1u;
        const unsigned int rejectAge =
            currentGeneration - entry.rejectGeneration;
        const bool recentReject = entry.hasReject && rejectAge <= 1u;
        if (recentSuccess && !recentReject)
            confirmed = occludedPass ? entry.occludedConfirmed != 0 :
                entry.visibleConfirmed != 0;
    }
    UnlockMeshConfirmations();
    return confirmed;
}

static bool VisualOwnerFromSceneObject(void* sceneObject,
    const VisualTargetEntry* snapshot, int snapshotCount,
    unsigned int* ownerHandle, VisualTargetKind* targetKind)
{
    if (!sceneObject || !snapshot || snapshotCount <= 0 || !ownerHandle ||
        !targetKind)
        return false;
    BYTE* ownerSlot = reinterpret_cast<BYTE*>(sceneObject) +
        kSceneObjectOwnerHandleOffset;
    if (!ReadableCommittedRange(ownerSlot, sizeof(unsigned int)))
        return false;

    const unsigned int handle = *reinterpret_cast<unsigned int*>(ownerSlot);
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

static void RejectClassifiedMeshTargets(
    const MeshElementClassification* classifications, int dataCount,
    unsigned int generation)
{
    if (!classifications || dataCount <= 0)
        return;
    for (int i = 0; i < dataCount; ++i)
    {
        if (classifications[i].kind == VISUAL_TARGET_NONE)
            continue;
        bool duplicate = false;
        for (int j = 0; j < i; ++j)
        {
            if (classifications[j].ownerHandle == classifications[i].ownerHandle &&
                classifications[j].kind == classifications[i].kind)
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            RejectMeshTargetConfirmation(classifications[i].ownerHandle,
                classifications[i].kind, generation);
    }
}

static bool ClassifyMeshBatch(void* meshDraw, int dataCount,
    const VisualTargetEntry* snapshot, int snapshotCount,
    MeshElementClassification* classifications)
{
    if (!meshDraw || dataCount <= 0 || dataCount > kMaxMeshBatch ||
        !snapshot || snapshotCount <= 0 || !classifications)
        return false;
    BYTE* base = reinterpret_cast<BYTE*>(meshDraw);
    for (int i = 0; i < dataCount; ++i)
    {
        classifications[i].ownerHandle = 0xFFFFFFFFu;
        classifications[i].kind = VISUAL_TARGET_NONE;
        BYTE* mesh = base + static_cast<SIZE_T>(i) * kMeshDataStride;
        if (!ReadableCommittedRange(mesh, kMeshDataStride))
            return false;
        void** sceneSlot = reinterpret_cast<void**>(
            mesh + kMeshSceneObjectOffset);
        if (!ReadableCommittedRange(sceneSlot, sizeof(void*)) || !*sceneSlot)
            continue;
        VisualOwnerFromSceneObject(*sceneSlot, snapshot, snapshotCount,
            &classifications[i].ownerHandle, &classifications[i].kind);
    }
    return true;
}

static bool UniformTargetBatch(const MeshElementClassification* classifications,
    int dataCount, unsigned int* ownerHandle, VisualTargetKind* targetKind)
{
    if (!classifications || dataCount <= 0 || !ownerHandle || !targetKind)
        return false;
    const unsigned int owner = classifications[0].ownerHandle;
    const VisualTargetKind kind = classifications[0].kind;
    if (owner == 0xFFFFFFFFu || kind == VISUAL_TARGET_NONE)
        return false;
    for (int i = 1; i < dataCount; ++i)
    {
        if (classifications[i].ownerHandle != owner ||
            classifications[i].kind != kind)
            return false;
    }
    *ownerHandle = owner;
    *targetKind = kind;
    return true;
}

static bool BatchHasAnyTarget(const MeshElementClassification* classifications,
    int dataCount)
{
    if (!classifications || dataCount <= 0)
        return false;
    for (int i = 0; i < dataCount; ++i)
        if (classifications[i].kind != VISUAL_TARGET_NONE)
            return true;
    return false;
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
        if (!WritableCommittedRange(mesh, kMeshDataStride))
            return false;
        saved[i].material = *reinterpret_cast<void**>(
            mesh + kMeshMaterialOffset);
        saved[i].material2 = *reinterpret_cast<void**>(
            mesh + kMeshMaterial2Offset);
        BYTE* color = mesh + kMeshColorOffset;
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
        if (!WritableCommittedRange(mesh, kMeshDataStride))
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

static bool RenderTargetMeshBatch(SceneDrawObjectFn original, void* a1, void* a2,
    void* meshDraw, int dataCount, VisualTargetKind targetKind,
    void* a5, void* a6, void* a7, void* a8, bool* resultOut,
    bool* visibleApplied, bool* occludedApplied)
{
    if (visibleApplied)
        *visibleApplied = false;
    if (occludedApplied)
        *occludedApplied = false;
    if (!original || !meshDraw || dataCount <= 0 || !resultOut)
        return false;

    MeshRenderStateGuard stateGuard{};
    if (!stateGuard.Capture(meshDraw, dataCount))
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
        result = original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);
        drew = true;
        if (occludedApplied)
            *occludedApplied = true;
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
        result = original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);
        drew = true;
        if (visibleApplied)
            *visibleApplied = true;
    }

    if (!drew)
        result = original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);
    *resultOut = result;
    return true;
}

struct MeshHookActivityGuard {
    MeshHookActivityGuard()
    {
        _InterlockedIncrement(&g_meshHookInFlight);
    }
    ~MeshHookActivityGuard()
    {
        _InterlockedDecrement(&g_meshHookInFlight);
    }
};

static bool __fastcall MeshDrawObjectHook(void* a1, void* a2, void* meshDraw,
    int dataCount, void* a5, void* a6, void* a7, void* a8)
{
    MeshHookActivityGuard activity;
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
    {
        if (snapshotCount < 0)
            ++g_meshRenderBackend.snapshotMisses;
        return original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);
    }

    MeshElementClassification classifications[kMaxMeshBatch];
    if (!ClassifyMeshBatch(meshDraw, dataCount, snapshot, snapshotCount,
        classifications))
    {
        ++g_meshRenderBackend.layoutRejects;
        return original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);
    }

    unsigned int ownerHandle = 0xFFFFFFFFu;
    VisualTargetKind targetKind = VISUAL_TARGET_NONE;
    if (!UniformTargetBatch(classifications, dataCount,
        &ownerHandle, &targetKind))
    {
        if (BatchHasAnyTarget(classifications, dataCount))
        {
            ++g_meshRenderBackend.mixedBatches;
            RejectClassifiedMeshTargets(classifications, dataCount,
                snapshotGeneration);
        }
        return original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);
    }

    bool result = false;
    bool visibleApplied = false;
    bool occludedApplied = false;
    if (!RenderTargetMeshBatch(original, a1, a2, meshDraw, dataCount,
        targetKind, a5, a6, a7, a8, &result,
        &visibleApplied, &occludedApplied))
    {
        ++g_meshRenderBackend.layoutRejects;
        RejectMeshTargetConfirmation(ownerHandle, targetKind,
            snapshotGeneration);
        return original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);
    }

    CountTargetDraw(targetKind);
    RecordMeshTargetConfirmation(ownerHandle, targetKind, snapshotGeneration,
        visibleApplied, occludedApplied);
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
        if (!GetModuleHandleW(L"scenesystem.dll"))
            return false;
        g_meshRenderBackend.installAttempted = true;
        if (!ResolveMeshRenderBackend())
            return false;
    }

    if (g_meshRenderBackend.installAttempts >= 3)
        return false;

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
    const bool trampolineProtectionRestored = VirtualProtect(
        g_drawObjectTrampoline, sizeof(g_drawObjectTrampoline),
        trampolineProtection, &ignored) != 0;
    if (!trampolineFlushed || !trampolineProtectionRestored)
        return false;

    g_meshRenderBackend.originalDrawObject = reinterpret_cast<SceneDrawObjectFn>(
        reinterpret_cast<void*>(&g_drawObjectTrampoline[0]));

    AtomicExchange(&g_meshHookBypass, 1);
    BYTE patch[kDrawPatchSize];
    WriteAbsoluteIndirectJump(patch,
        reinterpret_cast<const void*>(&MeshDrawObjectHook));
    const PatchWriteResult patchResult = PatchExecutableBytesSafely(target,
        g_meshRenderBackend.originalPrologue, patch, kDrawPatchSize);
    if (patchResult == PATCH_WRITE_BUSY)
    {
        ++g_meshRenderBackend.ripDeferrals;
        return false;
    }
    if (patchResult != PATCH_WRITE_SUCCESS)
    {
        ++g_meshRenderBackend.installAttempts;
        ++g_meshRenderBackend.patchFailures;
        return false;
    }

    g_meshRenderBackend.installed = true;
    AtomicExchange(&g_meshHookBypass, 0);
    return true;
}

static void WaitForMeshHookQuiescence()
{
    // Once the target entrypoint has been restored, no new hook calls can
    // enter. Existing calls are allowed to finish before shutdown clears any
    // state that they can still reference.
    while (_InterlockedCompareExchange(&g_meshHookInFlight, 0, 0) != 0)
    {
        ++g_meshRenderBackend.quiescenceWaits;
        Sleep(1);
    }
}

static bool RemoveMeshRenderBackend()
{
    AtomicExchange(&g_meshHookBypass, 1);
    if (!g_meshRenderBackend.installed ||
        !g_meshRenderBackend.drawObjectTarget)
    {
        g_meshRenderBackend.installed = false;
        WaitForMeshHookQuiescence();
        return true;
    }

    BYTE patch[kDrawPatchSize];
    WriteAbsoluteIndirectJump(patch,
        reinterpret_cast<const void*>(&MeshDrawObjectHook));
    for (int attempt = 0; attempt < 16; ++attempt)
    {
        const PatchWriteResult result = PatchExecutableBytesSafely(
            g_meshRenderBackend.drawObjectTarget, patch,
            g_meshRenderBackend.originalPrologue, kDrawPatchSize);
        if (result == PATCH_WRITE_SUCCESS)
        {
            g_meshRenderBackend.installed = false;
            WaitForMeshHookQuiescence();
            return true;
        }
        if (result == PATCH_WRITE_BUSY)
            ++g_meshRenderBackend.ripDeferrals;
        else
            ++g_meshRenderBackend.patchFailures;
        Sleep(1);
    }

    // No raw fallback restore: leaving the detour in bypass mode is safer than
    // rewriting an entrypoint while a suspended thread owns an instruction
    // pointer inside that patch range.
    return false;
}
