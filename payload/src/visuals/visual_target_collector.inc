// Schema-backed target discovery for the final mesh visual pipeline.
// FrameStage owns discovery; the DrawObject hook only consumes the published
// full-handle snapshot and never scans controllers or the entity system.

struct VisualTargetCollectorStats {
    unsigned int playerTargets;
    unsigned int weaponTargets;
    unsigned int armsTargets;
    unsigned int worldScans;
    unsigned int droppedWeapons;
    unsigned int grenades;
    unsigned int bombs;
};

static VisualTargetCollectorStats g_visualTargetCollectorStats{};

static const char* VisualDesignerName(void* entity)
{
    if (!entity || !IsAccessible(entity, 0x18, false))
        return nullptr;
    void* identity = *reinterpret_cast<void**>(
        reinterpret_cast<BYTE*>(entity) + 0x10);
    if (!IsAccessible(identity, 0x28, false))
        return nullptr;
    const char* name = *reinterpret_cast<const char**>(
        reinterpret_cast<BYTE*>(identity) + 0x20);
    return IsAccessible(name, 64, false) ? name : nullptr;
}

static bool AsciiStartsWith(const char* text, const char* prefix)
{
    if (!text || !prefix)
        return false;
    for (int i = 0; i < 64; ++i)
    {
        if (prefix[i] == 0)
            return true;
        if (text[i] == 0 || text[i] != prefix[i])
            return false;
    }
    return false;
}

static bool IsGrenadeProjectileName(const char* name)
{
    return AsciiEquals(name, "hegrenade_projectile") ||
        AsciiEquals(name, "flashbang_projectile") ||
        AsciiEquals(name, "smokegrenade_projectile") ||
        AsciiEquals(name, "molotov_projectile") ||
        AsciiEquals(name, "decoy_projectile");
}

static unsigned int ActiveWeaponHandleForPawn(const BotHighlightRuntime& runtime,
    void* pawn)
{
    if (!pawn)
        return 0xFFFFFFFFu;
    BYTE* pawnBase = reinterpret_cast<BYTE*>(pawn);
    void** services = reinterpret_cast<void**>(
        pawnBase + runtime.weaponServicesOffset);
    if (!IsAccessible(services, sizeof(void*), false) || !*services ||
        !IsAccessible(*services, runtime.activeWeaponOffset +
            sizeof(unsigned int), false))
        return 0xFFFFFFFFu;
    return *reinterpret_cast<unsigned int*>(
        reinterpret_cast<BYTE*>(*services) + runtime.activeWeaponOffset);
}

static unsigned int HudArmsHandleForPawn(const BotHighlightRuntime& runtime,
    void* pawn)
{
    if (!pawn || runtime.hudModelArmsOffset == 0)
        return 0xFFFFFFFFu;
    BYTE* slot = reinterpret_cast<BYTE*>(pawn) + runtime.hudModelArmsOffset;
    if (!IsAccessible(slot, sizeof(unsigned int), false))
        return 0xFFFFFFFFu;
    return *reinterpret_cast<unsigned int*>(slot);
}

static void CountPlayerTargetIfAdded(int before)
{
    if (g_visualTargetBuildCount != before)
        ++g_visualTargetCollectorStats.playerTargets;
}

// Scene-system player meshes are not guaranteed to expose the pawn as their
// CSceneAnimatableObject owner. Current CS2 DrawObject implementations also
// observe player geometry owned by the corresponding player controller. Keep
// the registry strict/full-handle based, but publish that controller as an
// explicit alias for the same visual target kind. No index-only matching or
// neighboring memory scan is introduced in the render hook.
static void AddPlayerControllerOwnerAlias(void* controller,
    VisualTargetKind pawnKind, bool enabled)
{
    if (!enabled || !controller)
        return;
    const unsigned int controllerHandle = EntityHandleFor(controller);
    const int before = g_visualTargetBuildCount;
    AddVisualTarget(controllerHandle, pawnKind);
    CountPlayerTargetIfAdded(before);
}

static void AddPlayerVisualTargetWithGear(const BotHighlightRuntime& runtime,
    unsigned int pawnHandle, void* pawn, VisualTargetKind pawnKind,
    VisualTargetKind weaponKind, bool addPawn, bool addWeapon, bool addArms)
{
    if (!pawn || pawnHandle == 0 || pawnHandle == 0xFFFFFFFFu)
        return;
    if (addPawn)
    {
        const int before = g_visualTargetBuildCount;
        AddVisualTarget(pawnHandle, pawnKind);
        CountPlayerTargetIfAdded(before);
    }
    if (addWeapon)
    {
        const unsigned int weaponHandle = ActiveWeaponHandleForPawn(runtime, pawn);
        const int before = g_visualTargetBuildCount;
        AddVisualTarget(weaponHandle, weaponKind);
        if (g_visualTargetBuildCount != before)
            ++g_visualTargetCollectorStats.weaponTargets;
    }
    if (addArms)
    {
        const unsigned int armsHandle = HudArmsHandleForPawn(runtime, pawn);
        const int before = g_visualTargetBuildCount;
        AddVisualTarget(armsHandle, VISUAL_TARGET_LOCAL_ARMS);
        if (g_visualTargetBuildCount != before)
            ++g_visualTargetCollectorStats.armsTargets;
    }
}

static void AddEnemyPawnVisualTargets(const BotHighlightRuntime& runtime,
    unsigned int pawnHandle, void* pawn)
{
    AddPlayerVisualTargetWithGear(runtime, pawnHandle, pawn,
        VISUAL_TARGET_ENEMY_PAWN, VISUAL_TARGET_ENEMY_WEAPON,
        g_espConfig.chamsEnemies,
        g_espConfig.chamsEnemies && g_espConfig.chamsWeapons, false);
}

static bool ValidVisualPawn(const BotHighlightRuntime& runtime, void* pawn,
    BYTE expectedTeam)
{
    if (!pawn)
        return false;
    BYTE* base = reinterpret_cast<BYTE*>(pawn);
    BYTE* team = base + runtime.teamOffset;
    int* health = reinterpret_cast<int*>(base + runtime.healthOffset);
    BYTE* lifeState = base + runtime.lifeStateOffset;
    if (!IsAccessible(team, 1, false) ||
        !IsAccessible(health, sizeof(int), false) ||
        !IsAccessible(lifeState, 1, false))
        return false;
    if (*team < 2 || *team > 3 || *health <= 0 || *health > 1000 ||
        *lifeState != 0)
        return false;
    return expectedTeam < 2 || expectedTeam > 3 || *team == expectedTeam;
}

static void CollectPlayerVisualTargets(const BotHighlightRuntime& runtime,
    void* entitySystem, void** controllers, int controllerCount,
    void* localController, void* localPawn, BYTE localTeam)
{
    if (!entitySystem || !controllers || controllerCount < 0)
        return;

    if (localPawn && ValidVisualPawn(runtime, localPawn, localTeam))
    {
        const unsigned int localPawnHandle = EntityHandleFor(localPawn);
        AddPlayerVisualTargetWithGear(runtime, localPawnHandle, localPawn,
            VISUAL_TARGET_LOCAL_PAWN, VISUAL_TARGET_LOCAL_WEAPON,
            g_espConfig.chamsLocal, g_espConfig.chamsWeapons,
            g_espConfig.chamsHands);
        AddPlayerControllerOwnerAlias(localController,
            VISUAL_TARGET_LOCAL_PAWN, g_espConfig.chamsLocal);
    }

    const int boundedCount = controllerCount > 64 ? 64 : controllerCount;
    for (int i = 0; i < boundedCount; ++i)
    {
        void* controller = controllers[i];
        if (!controller || controller == localController)
            continue;
        BYTE* base = reinterpret_cast<BYTE*>(controller);
        BYTE* team = base + runtime.teamOffset;
        BYTE* pawnAlive = base + runtime.pawnAliveOffset;
        unsigned int* pawnHandle = reinterpret_cast<unsigned int*>(
            base + runtime.playerPawnHandleOffset);
        if (!IsAccessible(team, 1, false) ||
            !IsAccessible(pawnAlive, 1, false) ||
            !IsAccessible(pawnHandle, sizeof(unsigned int), false) ||
            *pawnAlive == 0 || *team < 2 || *team > 3)
            continue;
        void* pawn = EntityFromHandle(runtime.entity, entitySystem, *pawnHandle);
        if (!ValidVisualPawn(runtime, pawn, *team))
            continue;

        if (*team == localTeam)
        {
            AddPlayerVisualTargetWithGear(runtime, *pawnHandle, pawn,
                VISUAL_TARGET_ALLY_PAWN, VISUAL_TARGET_ALLY_WEAPON,
                g_espConfig.chamsAllies,
                g_espConfig.chamsAllies && g_espConfig.chamsWeapons, false);
            AddPlayerControllerOwnerAlias(controller,
                VISUAL_TARGET_ALLY_PAWN, g_espConfig.chamsAllies);
        }
        else
        {
            AddEnemyPawnVisualTargets(runtime, *pawnHandle, pawn);
            AddPlayerControllerOwnerAlias(controller,
                VISUAL_TARGET_ENEMY_PAWN, g_espConfig.chamsEnemies);
        }
    }
}

static bool EntityOwnerIsEmpty(const BotHighlightRuntime& runtime, void* entity)
{
    if (!entity || runtime.ownerEntityOffset == 0)
        return false;
    BYTE* slot = reinterpret_cast<BYTE*>(entity) + runtime.ownerEntityOffset;
    if (!IsAccessible(slot, sizeof(unsigned int), false))
        return false;
    const unsigned int owner = *reinterpret_cast<unsigned int*>(slot);
    return owner == 0 || owner == 0xFFFFFFFFu;
}

static void CollectWorldVisualTargets(const BotHighlightRuntime& runtime,
    void* entitySystem)
{
    if (!entitySystem || (!g_espConfig.chamsDroppedWeapons &&
        !g_espConfig.chamsGrenades && !g_espConfig.chamsBomb))
        return;
    BYTE* highestAddress = reinterpret_cast<BYTE*>(entitySystem) +
        runtime.entity.highestEntityOffset;
    if (!IsAccessible(highestAddress, sizeof(int), false))
        return;
    const int highest = *reinterpret_cast<int*>(highestAddress);
    if (highest < 0 || highest > 32768)
        return;

    ++g_visualTargetCollectorStats.worldScans;
    for (int index = 65; index <= highest; ++index)
    {
        void* entity = EntityAtIndex(runtime.entity, entitySystem, index);
        if (!entity)
            continue;
        const unsigned int handle = EntityHandleFor(entity);
        if (handle == 0 || handle == 0xFFFFFFFFu ||
            VisualTargetKindBeingBuilt(handle) != VISUAL_TARGET_NONE)
            continue;
        const char* name = VisualDesignerName(entity);
        if (!name)
            continue;

        if (g_espConfig.chamsBomb && AsciiEquals(name, "planted_c4"))
        {
            const int before = g_visualTargetBuildCount;
            AddVisualTarget(handle, VISUAL_TARGET_BOMB);
            if (g_visualTargetBuildCount != before)
                ++g_visualTargetCollectorStats.bombs;
            continue;
        }
        if (g_espConfig.chamsGrenades && IsGrenadeProjectileName(name))
        {
            const int before = g_visualTargetBuildCount;
            AddVisualTarget(handle, VISUAL_TARGET_GRENADE);
            if (g_visualTargetBuildCount != before)
                ++g_visualTargetCollectorStats.grenades;
            continue;
        }
        if (g_espConfig.chamsDroppedWeapons && AsciiStartsWith(name, "weapon_") &&
            EntityOwnerIsEmpty(runtime, entity))
        {
            const int before = g_visualTargetBuildCount;
            AddVisualTarget(handle, VISUAL_TARGET_DROPPED_WEAPON);
            if (g_visualTargetBuildCount != before)
                ++g_visualTargetCollectorStats.droppedWeapons;
        }
    }
}

static void CollectSupplementalVisualTargets(const BotHighlightRuntime& runtime,
    void* entitySystem, void** controllers, int controllerCount,
    void* localController, void* localPawn, BYTE localTeam)
{
    CollectPlayerVisualTargets(runtime, entitySystem, controllers,
        controllerCount, localController, localPawn, localTeam);
    CollectWorldVisualTargets(runtime, entitySystem);
}
