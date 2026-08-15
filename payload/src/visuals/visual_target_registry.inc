// Frame-stage-owned target registry consumed by the mesh renderer.
// The builder is written only by UpdateBotHighlights. A short interlocked
// publication lock protects the published snapshot from a concurrent scene
// render callback without keeping the lock during entity/schema scanning.

extern "C" LONG _InterlockedCompareExchange(volatile LONG*, LONG, LONG);
#pragma intrinsic(_InterlockedCompareExchange)

struct VisualTargetRegistry {
    unsigned int enemyHandles[64];
    int enemyCount;
    unsigned int generation;
};

static VisualTargetRegistry g_visualTargets{};
static unsigned int g_visualTargetBuild[64]{};
static int g_visualTargetBuildCount = 0;
static volatile LONG g_visualTargetLock = 0;

static bool TryLockVisualTargets()
{
    return _InterlockedCompareExchange(&g_visualTargetLock, 1, 0) == 0;
}

static void UnlockVisualTargets()
{
    AtomicExchange(&g_visualTargetLock, 0);
}

static void BeginVisualTargetUpdate()
{
    g_visualTargetBuildCount = 0;
}

static void AddEnemyVisualTarget(unsigned int handle)
{
    if (handle == 0xFFFFFFFFu)
        return;
    for (int i = 0; i < g_visualTargetBuildCount; ++i)
    {
        if (g_visualTargetBuild[i] == handle)
            return;
    }
    if (g_visualTargetBuildCount >= static_cast<int>(
        sizeof(g_visualTargetBuild) / sizeof(g_visualTargetBuild[0])))
        return;
    g_visualTargetBuild[g_visualTargetBuildCount++] = handle;
}

static void PublishVisualTargets()
{
    // Publication is tiny (at most 64 dwords). If a scene callback is reading
    // the snapshot at this exact instant, skip this publication rather than
    // stalling the render path or exposing a partially-written list.
    if (!TryLockVisualTargets())
        return;
    g_visualTargets.enemyCount = g_visualTargetBuildCount;
    for (int i = 0; i < g_visualTargetBuildCount; ++i)
        g_visualTargets.enemyHandles[i] = g_visualTargetBuild[i];
    ++g_visualTargets.generation;
    UnlockVisualTargets();
}

static void ResetVisualTargets()
{
    g_visualTargetBuildCount = 0;
    if (!TryLockVisualTargets())
        return;
    g_visualTargets.enemyCount = 0;
    ++g_visualTargets.generation;
    UnlockVisualTargets();
}

static bool IsEnemyVisualTarget(unsigned int handle)
{
    if (handle == 0xFFFFFFFFu || !TryLockVisualTargets())
        return false;
    const int count = g_visualTargets.enemyCount < 0 ? 0 :
        (g_visualTargets.enemyCount > 64 ? 64 : g_visualTargets.enemyCount);
    bool found = false;
    for (int i = 0; i < count; ++i)
    {
        if (g_visualTargets.enemyHandles[i] == handle)
        {
            found = true;
            break;
        }
    }
    UnlockVisualTargets();
    return found;
}
