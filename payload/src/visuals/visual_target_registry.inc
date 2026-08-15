// Frame-stage-owned typed target registry consumed by the mesh renderer.
// A new target list is built off to the side and published as one short locked
// snapshot. The previous generation remains readable while schema/entity
// scanning is in progress, so DrawObject never observes a half-built list.

extern "C" LONG _InterlockedCompareExchange(volatile LONG*, LONG, LONG);
#pragma intrinsic(_InterlockedCompareExchange)

enum VisualTargetKind : unsigned int {
    VISUAL_TARGET_NONE = 0,
    VISUAL_TARGET_ENEMY_PAWN = 1,
    VISUAL_TARGET_ENEMY_WEAPON = 2,
    VISUAL_TARGET_ALLY_PAWN = 3,
    VISUAL_TARGET_ALLY_WEAPON = 4,
    VISUAL_TARGET_LOCAL_PAWN = 5,
    VISUAL_TARGET_LOCAL_WEAPON = 6,
    VISUAL_TARGET_LOCAL_ARMS = 7,
    VISUAL_TARGET_DROPPED_WEAPON = 8,
    VISUAL_TARGET_GRENADE = 9,
    VISUAL_TARGET_BOMB = 10
};

struct VisualTargetEntry {
    unsigned int handle;
    unsigned int kind;
};

static constexpr int kMaxVisualTargets = 256;

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

static int VisualTargetPriority(VisualTargetKind kind)
{
    switch (kind)
    {
    case VISUAL_TARGET_LOCAL_ARMS: return 100;
    case VISUAL_TARGET_LOCAL_PAWN: return 95;
    case VISUAL_TARGET_ENEMY_PAWN: return 90;
    case VISUAL_TARGET_ALLY_PAWN: return 85;
    case VISUAL_TARGET_LOCAL_WEAPON: return 80;
    case VISUAL_TARGET_ENEMY_WEAPON: return 75;
    case VISUAL_TARGET_ALLY_WEAPON: return 70;
    case VISUAL_TARGET_BOMB: return 60;
    case VISUAL_TARGET_GRENADE: return 50;
    case VISUAL_TARGET_DROPPED_WEAPON: return 40;
    default: return 0;
    }
}

static bool IsPlayerPawnTargetKind(VisualTargetKind kind)
{
    return kind == VISUAL_TARGET_ENEMY_PAWN ||
        kind == VISUAL_TARGET_ALLY_PAWN ||
        kind == VISUAL_TARGET_LOCAL_PAWN;
}

static bool IsPlayerWeaponTargetKind(VisualTargetKind kind)
{
    return kind == VISUAL_TARGET_ENEMY_WEAPON ||
        kind == VISUAL_TARGET_ALLY_WEAPON ||
        kind == VISUAL_TARGET_LOCAL_WEAPON;
}

static bool IsLocalVisualTargetKind(VisualTargetKind kind)
{
    return kind == VISUAL_TARGET_LOCAL_PAWN ||
        kind == VISUAL_TARGET_LOCAL_WEAPON ||
        kind == VISUAL_TARGET_LOCAL_ARMS;
}

static bool VisualTargetKindEnabled(VisualTargetKind kind)
{
    switch (kind)
    {
    case VISUAL_TARGET_ENEMY_PAWN:
        return g_espConfig.chamsEnemies;
    case VISUAL_TARGET_ENEMY_WEAPON:
        return g_espConfig.chamsEnemies && g_espConfig.chamsWeapons;
    case VISUAL_TARGET_ALLY_PAWN:
        return g_espConfig.chamsAllies;
    case VISUAL_TARGET_ALLY_WEAPON:
        return g_espConfig.chamsAllies && g_espConfig.chamsWeapons;
    case VISUAL_TARGET_LOCAL_PAWN:
        return g_espConfig.chamsLocal;
    case VISUAL_TARGET_LOCAL_WEAPON:
        return g_espConfig.chamsWeapons;
    case VISUAL_TARGET_LOCAL_ARMS:
        return g_espConfig.chamsHands;
    case VISUAL_TARGET_DROPPED_WEAPON:
        return g_espConfig.chamsDroppedWeapons;
    case VISUAL_TARGET_GRENADE:
        return g_espConfig.chamsGrenades;
    case VISUAL_TARGET_BOMB:
        return g_espConfig.chamsBomb;
    default:
        return false;
    }
}

static bool VisualTargetAllowsOccludedPass(VisualTargetKind kind)
{
    // Local/viewmodel geometry is rendered for cosmetic differentiation only.
    // Running Ignore-Z on the local pawn, held weapon or HUD arms produces
    // visually confusing overlays without adding useful wall information.
    return !IsLocalVisualTargetKind(kind) && kind != VISUAL_TARGET_NONE;
}

static void AddVisualTarget(unsigned int handle, VisualTargetKind kind)
{
    if (handle == 0 || handle == 0xFFFFFFFFu || kind == VISUAL_TARGET_NONE ||
        !VisualTargetKindEnabled(kind))
        return;
    for (int i = 0; i < g_visualTargetBuildCount; ++i)
    {
        if (g_visualTargetBuild[i].handle != handle)
            continue;
        const VisualTargetKind existing = static_cast<VisualTargetKind>(
            g_visualTargetBuild[i].kind);
        if (VisualTargetPriority(kind) > VisualTargetPriority(existing))
            g_visualTargetBuild[i].kind = kind;
        return;
    }
    if (g_visualTargetBuildCount >= kMaxVisualTargets)
        return;
    VisualTargetEntry& entry = g_visualTargetBuild[g_visualTargetBuildCount++];
    entry.handle = handle;
    entry.kind = kind;
}

// Compatibility entry used by the legacy monolithic enemy-controller call site.
static void AddEnemyVisualTarget(unsigned int handle)
{
    AddVisualTarget(handle, VISUAL_TARGET_ENEMY_PAWN);
}

static VisualTargetKind VisualTargetKindBeingBuilt(unsigned int handle)
{
    if (handle == 0 || handle == 0xFFFFFFFFu)
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

// The render hook takes one immutable copy per DrawObject call. Returning -1
// means the publication lock was momentarily busy; callers must fail closed and
// render the original batch unchanged rather than mixing target generations.
static int CopyVisualTargetSnapshot(VisualTargetEntry* destination,
    int capacity, unsigned int* generation)
{
    if (!destination || capacity <= 0 || !TryLockVisualTargets())
        return -1;
    int count = g_visualTargets.count;
    if (count < 0)
        count = 0;
    if (count > kMaxVisualTargets)
        count = kMaxVisualTargets;
    if (count > capacity)
        count = capacity;
    for (int i = 0; i < count; ++i)
    {
        destination[i].handle = g_visualTargets.entries[i].handle;
        destination[i].kind = g_visualTargets.entries[i].kind;
    }
    if (generation)
        *generation = g_visualTargets.generation;
    UnlockVisualTargets();
    return count;
}

static VisualTargetKind VisualTargetKindFromSnapshot(unsigned int handle,
    const VisualTargetEntry* snapshot, int count)
{
    if (handle == 0 || handle == 0xFFFFFFFFu || !snapshot || count <= 0)
        return VISUAL_TARGET_NONE;
    const int boundedCount = count > kMaxVisualTargets ? kMaxVisualTargets : count;
    for (int i = 0; i < boundedCount; ++i)
    {
        if (snapshot[i].handle == handle)
            return static_cast<VisualTargetKind>(snapshot[i].kind);
    }
    return VISUAL_TARGET_NONE;
}

static VisualTargetKind VisualTargetKindFor(unsigned int handle)
{
    if (handle == 0 || handle == 0xFFFFFFFFu || !TryLockVisualTargets())
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

static bool IsVisualTarget(unsigned int handle)
{
    const VisualTargetKind kind = VisualTargetKindFor(handle);
    return kind != VISUAL_TARGET_NONE && VisualTargetKindEnabled(kind);
}

static bool IsEnemyVisualTarget(unsigned int handle)
{
    const VisualTargetKind kind = VisualTargetKindFor(handle);
    return (kind == VISUAL_TARGET_ENEMY_PAWN ||
        kind == VISUAL_TARGET_ENEMY_WEAPON) && VisualTargetKindEnabled(kind);
}
