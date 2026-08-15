// Frame-stage-owned target registry consumed by the mesh renderer.
// A new target list is built off to the side and published as one short locked
// snapshot. The previous generation remains readable while schema/entity
// scanning is in progress, so DrawObject never observes a half-built list.

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

static bool IsEnemyVisualTargetBeingBuilt(unsigned int handle)
{
    if (handle == 0xFFFFFFFFu)
        return false;
    for (int i = 0; i < g_visualTargetBuildCount; ++i)
    {
        if (g_visualTargetBuild[i] == handle)
            return true;
    }
    return false;
}

static void PublishVisualTargets()
{
    if (!TryLockVisualTargets())
        return;
    g_visualTargets.enemyCount = g_visualTargetBuildCount;
    // Volatile element-wise publication preserves the no-CRT payload contract;
    // /O2 must not lower this bounded copy into an external memcpy call.
    volatile unsigned int* destination = g_visualTargets.enemyHandles;
    volatile unsigned int* source = g_visualTargetBuild;
    for (int i = 0; i < g_visualTargetBuildCount; ++i)
        destination[i] = source[i];
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
