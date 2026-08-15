// Frame-stage-owned typed target registry consumed by the mesh renderer.
// A new target list is built off to the side and published as one short locked
// snapshot. The previous generation remains readable while schema/entity
// scanning is in progress, so DrawObject never observes a half-built list.

extern "C" LONG _InterlockedCompareExchange(volatile LONG*, LONG, LONG);
#pragma intrinsic(_InterlockedCompareExchange)

enum VisualTargetKind : unsigned int {
    VISUAL_TARGET_NONE = 0,
    VISUAL_TARGET_ENEMY_PAWN = 1,
    VISUAL_TARGET_ENEMY_WEAPON = 2
};

struct VisualTargetEntry {
    unsigned int handle;
    unsigned int kind;
};

static constexpr int kMaxVisualTargets = 128;

struct VisualTargetRegistry {
    VisualTargetEntry entries[kMaxVisualTargets];
    int count;
    unsigned int generation;
};

static VisualTargetRegistry g_visualTargets{};
static VisualTargetEntry g_visualTargetBuild[kMaxVisualTargets]{};
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

static void AddVisualTarget(unsigned int handle, VisualTargetKind kind)
{
    if (handle == 0xFFFFFFFFu || kind == VISUAL_TARGET_NONE)
        return;
    for (int i = 0; i < g_visualTargetBuildCount; ++i)
    {
        if (g_visualTargetBuild[i].handle == handle)
        {
            // Pawn ownership is stronger than an attached weapon classification
            // if a transient handle ever appears twice in the same generation.
            if (kind == VISUAL_TARGET_ENEMY_PAWN)
                g_visualTargetBuild[i].kind = kind;
            return;
        }
    }
    if (g_visualTargetBuildCount >= kMaxVisualTargets)
        return;
    VisualTargetEntry& entry = g_visualTargetBuild[g_visualTargetBuildCount++];
    entry.handle = handle;
    entry.kind = kind;
}

// Compatibility entry used by the legacy monolithic UpdateBotHighlights call
// site. The richer category expansion happens inside the modular pipeline.
static void AddEnemyVisualTarget(unsigned int handle)
{
    AddVisualTarget(handle, VISUAL_TARGET_ENEMY_PAWN);
}

static void AddEnemyPawnVisualTargets(const BotHighlightRuntime& runtime,
    unsigned int pawnHandle, void* pawn)
{
    AddVisualTarget(pawnHandle, VISUAL_TARGET_ENEMY_PAWN);
    if (!g_espConfig.chamsWeapons || !pawn)
        return;

    BYTE* pawnBase = reinterpret_cast<BYTE*>(pawn);
    void** services = reinterpret_cast<void**>(
        pawnBase + runtime.weaponServicesOffset);
    if (!IsAccessible(services, sizeof(void*), false) || !*services ||
        !IsAccessible(*services, runtime.activeWeaponOffset +
            sizeof(unsigned int), false))
        return;
    const unsigned int weaponHandle = *reinterpret_cast<unsigned int*>(
        reinterpret_cast<BYTE*>(*services) + runtime.activeWeaponOffset);
    AddVisualTarget(weaponHandle, VISUAL_TARGET_ENEMY_WEAPON);
}

static VisualTargetKind VisualTargetKindBeingBuilt(unsigned int handle)
{
    if (handle == 0xFFFFFFFFu)
        return VISUAL_TARGET_NONE;
    for (int i = 0; i < g_visualTargetBuildCount; ++i)
    {
        if (g_visualTargetBuild[i].handle == handle)
            return static_cast<VisualTargetKind>(g_visualTargetBuild[i].kind);
    }
    return VISUAL_TARGET_NONE;
}

static bool IsEnemyVisualTargetBeingBuilt(unsigned int handle)
{
    const VisualTargetKind kind = VisualTargetKindBeingBuilt(handle);
    return kind == VISUAL_TARGET_ENEMY_PAWN ||
        kind == VISUAL_TARGET_ENEMY_WEAPON;
}

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

static void ResetVisualTargets()
{
    g_visualTargetBuildCount = 0;
    if (!TryLockVisualTargets())
        return;
    g_visualTargets.count = 0;
    ++g_visualTargets.generation;
    UnlockVisualTargets();
}

static VisualTargetKind VisualTargetKindFor(unsigned int handle)
{
    if (handle == 0xFFFFFFFFu || !TryLockVisualTargets())
        return VISUAL_TARGET_NONE;
    const int count = g_visualTargets.count < 0 ? 0 :
        (g_visualTargets.count > kMaxVisualTargets ? kMaxVisualTargets :
            g_visualTargets.count);
    VisualTargetKind kind = VISUAL_TARGET_NONE;
    for (int i = 0; i < count; ++i)
    {
        if (g_visualTargets.entries[i].handle == handle)
        {
            kind = static_cast<VisualTargetKind>(g_visualTargets.entries[i].kind);
            break;
        }
    }
    UnlockVisualTargets();
    return kind;
}

static bool IsEnemyVisualTarget(unsigned int handle)
{
    const VisualTargetKind kind = VisualTargetKindFor(handle);
    return kind == VISUAL_TARGET_ENEMY_PAWN ||
        kind == VISUAL_TARGET_ENEMY_WEAPON;
}
