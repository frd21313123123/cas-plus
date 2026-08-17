param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

function Replace-Required([string]$Needle, [string]$Replacement, [string]$Name) {
    $count = ([regex]::Matches($script:source, [regex]::Escape($Needle))).Count
    if ($count -ne 1) {
        throw "Visual hot-path anchor '$Name' expected exactly once, found $count. Refusing to patch blindly."
    }
    $script:source = $script:source.Replace($Needle, $Replacement)
}

# DrawObject is one of the hottest functions in scenesystem.dll.  The previous
# implementation copied the whole target registry and linearly searched it for
# every scene draw, even world geometry.  Publish a generation-tagged direct
# table keyed by the entity-handle index instead.  Old slots require no clearing:
# their generation simply stops matching after the next publish.
$registryAnchor = @'
static VisualTargetRegistry g_visualTargets{};
static VisualTargetEntry g_visualTargetBuild[kMaxVisualTargets]{};
static int g_visualTargetBuildCount = 0;
static volatile LONG g_visualTargetLock = 0;
'@
$registryReplacement = @'
static VisualTargetRegistry g_visualTargets{};
static VisualTargetEntry g_visualTargetBuild[kMaxVisualTargets]{};
static int g_visualTargetBuildCount = 0;
static volatile LONG g_visualTargetLock = 0;

static constexpr unsigned int kFastVisualTargetSlots = 0x8000u;
struct FastVisualTargetEntry {
    unsigned int handle;
    unsigned int kind;
    unsigned int generation;
};
static FastVisualTargetEntry g_fastVisualTargets[kFastVisualTargetSlots]{};
static volatile LONG g_fastVisualTargetGeneration = 0;

static unsigned int FastVisualTargetGeneration()
{
    return static_cast<unsigned int>(_InterlockedCompareExchange(
        &g_fastVisualTargetGeneration, 0, 0));
}

static VisualTargetKind FastVisualTargetKindFor(unsigned int handle)
{
    if (handle == 0 || handle == 0xFFFFFFFFu)
        return VISUAL_TARGET_NONE;
    const unsigned int generation = FastVisualTargetGeneration();
    if (generation == 0)
        return VISUAL_TARGET_NONE;
    const FastVisualTargetEntry& entry =
        g_fastVisualTargets[handle & (kFastVisualTargetSlots - 1u)];
    if (entry.generation != generation || entry.handle != handle)
        return VISUAL_TARGET_NONE;
    const VisualTargetKind kind = static_cast<VisualTargetKind>(entry.kind);
    return VisualTargetKindEnabled(kind) ? kind : VISUAL_TARGET_NONE;
}
'@
Replace-Required $registryAnchor $registryReplacement.TrimEnd() 'fast target table declaration'

$publishAnchor = @'
static void PublishVisualTargets()
{
    if (!TryLockVisualTargets())
        return;
    g_visualTargets.count = g_visualTargetBuildCount;
    // Publish fields explicitly so /O2 cannot lower the bounded copy into an
    // external CRT memcpy in this /NODEFAULTLIB payload.
    for (int i = 0; i < g_visualTargetBuildCount; ++i)
    {
        g_visualTargets.entries[i].handle = g_visualTargetBuild[i].handle;
        g_visualTargets.entries[i].kind = g_visualTargetBuild[i].kind;
    }
    ++g_visualTargets.generation;
    UnlockVisualTargets();
}
'@
$publishReplacement = @'
static void PublishVisualTargets()
{
    if (!TryLockVisualTargets())
        return;
    const unsigned int nextGeneration = g_visualTargets.generation + 1u;
    g_visualTargets.count = g_visualTargetBuildCount;
    // Publish fields explicitly so /O2 cannot lower the bounded copy into an
    // external CRT memcpy in this /NODEFAULTLIB payload.
    for (int i = 0; i < g_visualTargetBuildCount; ++i)
    {
        const VisualTargetEntry& source = g_visualTargetBuild[i];
        g_visualTargets.entries[i].handle = source.handle;
        g_visualTargets.entries[i].kind = source.kind;

        FastVisualTargetEntry& fast =
            g_fastVisualTargets[source.handle & (kFastVisualTargetSlots - 1u)];
        fast.handle = source.handle;
        fast.kind = source.kind;
        // Generation is written last; the Interlocked publish below is the
        // release fence seen by the render thread.
        fast.generation = nextGeneration;
    }
    g_visualTargets.generation = nextGeneration;
    UnlockVisualTargets();
    AtomicExchange(&g_fastVisualTargetGeneration,
        static_cast<LONG>(nextGeneration));
}
'@
Replace-Required $publishAnchor $publishReplacement.TrimEnd() 'fast target publish'

$resetAnchor = @'
static void ResetVisualTargets()
{
    g_visualTargetBuildCount = 0;
    if (!TryLockVisualTargets())
        return;
    g_visualTargets.count = 0;
    ++g_visualTargets.generation;
    UnlockVisualTargets();
}
'@
$resetReplacement = @'
static void ResetVisualTargets()
{
    g_visualTargetBuildCount = 0;
    if (!TryLockVisualTargets())
        return;
    g_visualTargets.count = 0;
    const unsigned int nextGeneration = g_visualTargets.generation + 1u;
    g_visualTargets.generation = nextGeneration;
    UnlockVisualTargets();
    AtomicExchange(&g_fastVisualTargetGeneration,
        static_cast<LONG>(nextGeneration));
}
'@
Replace-Required $resetAnchor $resetReplacement.TrimEnd() 'fast target reset'

# Through-wall visibility already has a guarded Source 2 highlight fallback.
# Rendering the same model a second time with Ignore-Z is substantially more
# expensive and is unnecessary for the wall pass.  Keep the mesh renderer for
# the visible material pass only; style 2 therefore never enters the hot hook.
$occludedAnchor = @'
    const bool wantOccluded = (g_espConfig.chamsStyle == 0 ||
        g_espConfig.chamsStyle == 2) &&
        VisualTargetAllowsOccludedPass(targetKind);
'@
$occludedReplacement = @'
    // Occluded wall visibility is owned by the compatibility highlight path.
    // This deliberately avoids a second scenesystem DrawObject call per target.
    const bool wantOccluded = false;
'@
Replace-Required $occludedAnchor $occludedReplacement.TrimEnd() 'single-pass wall rendering'

# Helpers used by the replacement hook.  The first element is a deliberately
# tiny, allocation-free rejection probe.  meshDraw and its scene-object pointer
# are engine-owned arguments of the resolved DrawObject ABI; full VirtualQuery
# validation is deferred until that first owner is actually a registered target.
$hookStart = $source.IndexOf('static bool __fastcall MeshDrawObjectHook(void* a1, void* a2, void* meshDraw,')
$installStart = $source.IndexOf('static bool InstallMeshRenderBackend()', $hookStart)
if ($hookStart -lt 0 -or $installStart -le $hookStart) {
    throw 'Mesh DrawObject hook function anchors were not found. Refusing to patch blindly.'
}

$replacementHook = @'
static bool FastProbeMeshTarget(void* meshDraw, int dataCount,
    unsigned int* ownerHandle, VisualTargetKind* targetKind)
{
    if (!meshDraw || dataCount <= 0 || dataCount > kMaxMeshBatch ||
        !ownerHandle || !targetKind)
        return false;
    BYTE* first = reinterpret_cast<BYTE*>(meshDraw);
    void* sceneObject = *reinterpret_cast<void**>(first + kMeshSceneObjectOffset);
    if (!sceneObject ||
        reinterpret_cast<ULONG_PTR>(sceneObject) < 0x10000ULL)
        return false;
    const unsigned int handle = *reinterpret_cast<unsigned int*>(
        reinterpret_cast<BYTE*>(sceneObject) + kSceneObjectOwnerHandleOffset);
    const VisualTargetKind kind = FastVisualTargetKindFor(handle);
    if (kind == VISUAL_TARGET_NONE)
        return false;
    *ownerHandle = handle;
    *targetKind = kind;
    return true;
}

static bool VisualOwnerFromSceneObjectFast(void* sceneObject,
    unsigned int* ownerHandle, VisualTargetKind* targetKind)
{
    if (!sceneObject || !ownerHandle || !targetKind)
        return false;
    BYTE* ownerSlot = reinterpret_cast<BYTE*>(sceneObject) +
        kSceneObjectOwnerHandleOffset;
    if (!ReadableCommittedRange(ownerSlot, sizeof(unsigned int)))
        return false;
    const unsigned int handle = *reinterpret_cast<unsigned int*>(ownerSlot);
    const VisualTargetKind kind = FastVisualTargetKindFor(handle);
    if (kind == VISUAL_TARGET_NONE)
        return false;
    *ownerHandle = handle;
    *targetKind = kind;
    return true;
}

static bool ClassifyMeshBatchFast(void* meshDraw, int dataCount,
    MeshElementClassification* classifications)
{
    if (!meshDraw || dataCount <= 0 || dataCount > kMaxMeshBatch ||
        !classifications)
        return false;
    BYTE* base = reinterpret_cast<BYTE*>(meshDraw);
    for (int i = 0; i < dataCount; ++i)
    {
        classifications[i].ownerHandle = 0xFFFFFFFFu;
        classifications[i].kind = VISUAL_TARGET_NONE;
        BYTE* mesh = base + static_cast<SIZE_T>(i) * kMeshDataStride;
        if (!ReadableCommittedRange(mesh, kMeshDataStride))
            return false;
        void** sceneSlot = reinterpret_cast<void**>(mesh + kMeshSceneObjectOffset);
        if (!ReadableCommittedRange(sceneSlot, sizeof(void*)) || !*sceneSlot)
            continue;
        VisualOwnerFromSceneObjectFast(*sceneSlot,
            &classifications[i].ownerHandle, &classifications[i].kind);
    }
    return true;
}

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
        !g_espConfig.chams || g_espConfig.chamsStyle == 2 ||
        g_espConfig.chamsStyle == 3)
        return original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);

    // Reject ordinary world/UI/prop batches before taking a lock, copying the
    // target list or running VirtualQuery for every element.  This is the common
    // path for the overwhelming majority of scene draws.
    unsigned int ownerHandle = 0xFFFFFFFFu;
    VisualTargetKind targetKind = VISUAL_TARGET_NONE;
    if (!FastProbeMeshTarget(meshDraw, dataCount, &ownerHandle, &targetKind))
        return original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);

    const unsigned int snapshotGeneration = FastVisualTargetGeneration();
    if (snapshotGeneration == 0)
        return original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);

    MeshElementClassification classifications[kMaxMeshBatch];
    if (!ClassifyMeshBatchFast(meshDraw, dataCount, classifications))
    {
        ++g_meshRenderBackend.layoutRejects;
        return original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);
    }

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
        visibleApplied, false);
    return result;
}

'@
$source = $source.Substring(0, $hookStart) + $replacementHook +
    $source.Substring($installStart)

Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Optimized Visuals DrawObject hot path and moved wall pass to highlight backend: $InputPath"
