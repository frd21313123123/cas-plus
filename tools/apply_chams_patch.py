from pathlib import Path

path = Path("payload/src/dllmain.cpp")
text = path.read_text(encoding="utf-8")


def replace_once(old: str, new: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one match, found {count}: {old[:80]!r}")
    text = text.replace(old, new, 1)


replace_once(
'''    bool chams = true;
    RGBVal chamsColorOccluded = { 239, 68, 68 }; // Color 1: Red (Behind Wall / Occluded)
    RGBVal chamsColorVisible = { 132, 204, 22 };  // Color 2: Lime Green (In Line of Sight)
    int chamsStyle = 0; // 0: Flat Solid, 1: Textured, 2: Wireframe, 3: Glow/Glass Chams
    bool chamsEnemies = true;
    bool chamsHands = false;
    bool chamsWeapons = false;
''',
'''    bool chams = true;
    RGBVal chamsColorOccluded = { 239, 68, 68 }; // through-wall render pass
    RGBVal chamsColorVisible = { 132, 204, 22 };  // normal depth-tested model pass
    int chamsStyle = 0; // 0: Dual Pass, 1: Visible Only, 2: Through Wall Only, 3: Glow Pass
    bool chamsEnemies = true;
    bool chamsHands = false; // intentionally unavailable until a schema-backed viewmodel handle exists
    bool chamsWeapons = false;
''')

replace_once(
'''    unsigned int botDifficultyOffset;
    unsigned int glowOffset;
''',
'''    unsigned int botDifficultyOffset;
    unsigned int weaponServicesOffset;
    unsigned int activeWeaponOffset;
    unsigned int glowOffset;
''')

replace_once(
'''    SchemaClassInfoView* baseModelEntity = FindSchemaClass(scope,
        "C_BaseModelEntity");
    SchemaClassInfoView* glowProperty = FindSchemaClass(scope,
        "CGlowProperty");
    if (!baseEntity || !baseController || !playerController ||
        !baseModelEntity || !glowProperty)
        return false;
''',
'''    SchemaClassInfoView* basePlayerPawn = FindSchemaClass(scope,
        "C_BasePlayerPawn");
    SchemaClassInfoView* weaponServices = FindSchemaClass(scope,
        "CPlayer_WeaponServices");
    SchemaClassInfoView* baseModelEntity = FindSchemaClass(scope,
        "C_BaseModelEntity");
    SchemaClassInfoView* glowProperty = FindSchemaClass(scope,
        "CGlowProperty");
    if (!baseEntity || !baseController || !playerController ||
        !basePlayerPawn || !weaponServices || !baseModelEntity || !glowProperty)
        return false;
''')

replace_once(
'''        !FindSchemaField(playerController, "m_iPawnBotDifficulty",
            &runtime->botDifficultyOffset) ||
        !FindSchemaField(baseModelEntity, "m_Glow", &runtime->glowOffset) ||
''',
'''        !FindSchemaField(playerController, "m_iPawnBotDifficulty",
            &runtime->botDifficultyOffset) ||
        !FindSchemaField(basePlayerPawn, "m_pWeaponServices",
            &runtime->weaponServicesOffset) ||
        !FindSchemaField(weaponServices, "m_hActiveWeapon",
            &runtime->activeWeaponOffset) ||
        !FindSchemaField(baseModelEntity, "m_Glow", &runtime->glowOffset) ||
''')

replace_once(
'''        runtime->botDifficultyOffset + sizeof(int) >
            static_cast<unsigned int>(playerController->size) ||
        runtime->botDifficultyOffset <= runtime->pawnAliveOffset ||
        runtime->glowOffset + runtime->glowingOffset >=
''',
'''        runtime->botDifficultyOffset + sizeof(int) >
            static_cast<unsigned int>(playerController->size) ||
        runtime->botDifficultyOffset <= runtime->pawnAliveOffset ||
        runtime->weaponServicesOffset + sizeof(void*) >
            static_cast<unsigned int>(basePlayerPawn->size) ||
        runtime->activeWeaponOffset + sizeof(unsigned int) >
            static_cast<unsigned int>(weaponServices->size) ||
        runtime->glowOffset + runtime->glowingOffset >=
''')

old_apply = '''static bool ApplyBotHighlight(const BotHighlightRuntime& runtime,
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

    if (!g_espConfig.enable)
        return false;

    // Check if enemy pawn is spotted / visible or occluded behind wall
    BYTE* spotted = reinterpret_cast<BYTE*>(pawn) + 0x8;
    bool isOccluded = true;
    if (IsAccessible(spotted, 1, false) && *spotted != 0)
        isOccluded = false;

    // Dual-color Chams: Color 1 (Occluded / Behind Wall) vs Color 2 (Visible / In Sight)
    const RGBVal activeChams = isOccluded ? g_espConfig.chamsColorOccluded : g_espConfig.chamsColorVisible;
    const BYTE highlightColor[4] = { activeChams.r, activeChams.g, activeChams.b, 255 };
    const BYTE glowHighlightColor[4] = {
        (g_espConfig.chams || !g_espConfig.glow) ? activeChams.r : g_espConfig.glowColor.r,
        (g_espConfig.chams || !g_espConfig.glow) ? activeChams.g : g_espConfig.glowColor.g,
        (g_espConfig.chams || !g_espConfig.glow) ? activeChams.b : g_espConfig.glowColor.b,
        255
    };

    // Apply Chams model color
    if (g_espConfig.chams)
    {
        CopyFourBytes(renderColor, highlightColor);
        CopyFourBytes(clientTint, highlightColor);
        *useClientTint = 1;
        runtime.setRenderColor(pawn, highlightColor[0], highlightColor[1], highlightColor[2]);
    }

    // Apply Engine Glow & Wall-penetrating Stencil Pass (Type 3 = renders through walls)
    BYTE* glowBase = reinterpret_cast<BYTE*>(pawn) + runtime.glowOffset;
    if (g_espConfig.glow || g_espConfig.chams || g_espConfig.enable)
    {
        runtime.setGlowColor(glowBase, PackRgba(glowHighlightColor));
        *eligible = 1;
        runtime.setGlowType(glowBase, 3, 0.0f); // Type 3 enables engine stencil glow & mesh pass through walls
        *glowing = 1;
    }
    return true;
}
'''

new_apply = '''static bool ApplyModelPasses(const BotHighlightRuntime& runtime,
    unsigned int entityHandle, void* modelEntity, bool allowVisiblePass,
    bool allowThroughWallPass)
{
    if (!RememberBotHighlight(runtime, entityHandle, modelEntity))
        return false;

    BYTE* glowColor = nullptr;
    BYTE* eligible = nullptr;
    BYTE* glowing = nullptr;
    BYTE* renderColor = nullptr;
    BYTE* clientTint = nullptr;
    BYTE* useClientTint = nullptr;
    if (!HighlightSlots(runtime, modelEntity, &glowColor, &eligible, &glowing,
        &renderColor, &clientTint, &useClientTint))
        return false;

    OriginalBotHighlight* original = FindOriginalBotHighlight(entityHandle);
    if (!original)
        return false;
    original->seen = true;

    const bool visiblePass = g_espConfig.enable && g_espConfig.chams &&
        allowVisiblePass && g_espConfig.chamsStyle != 2 &&
        g_espConfig.chamsStyle != 3;
    const bool wallPass = g_espConfig.enable && g_espConfig.chams &&
        allowThroughWallPass && g_espConfig.chamsStyle != 1 &&
        g_espConfig.chamsStyle != 3;
    const bool glowPass = g_espConfig.enable &&
        (g_espConfig.glow || g_espConfig.chamsStyle == 3) &&
        allowThroughWallPass;

    if (visiblePass)
    {
        const BYTE visible[4] = {
            g_espConfig.chamsColorVisible.r,
            g_espConfig.chamsColorVisible.g,
            g_espConfig.chamsColorVisible.b,
            255
        };
        CopyFourBytes(renderColor, visible);
        CopyFourBytes(clientTint, visible);
        *useClientTint = 1;
        runtime.setRenderColor(modelEntity, visible[0], visible[1], visible[2]);
    }
    else
    {
        CopyFourBytes(clientTint, original->clientTint);
        *useClientTint = original->useClientTint;
        CopyFourBytes(renderColor, original->renderColor);
        runtime.setRenderColor(modelEntity, original->renderColor[0],
            original->renderColor[1], original->renderColor[2]);
    }

    BYTE* glowBase = reinterpret_cast<BYTE*>(modelEntity) + runtime.glowOffset;
    if (wallPass || glowPass)
    {
        const RGBVal passColor = glowPass && !wallPass ?
            g_espConfig.glowColor : g_espConfig.chamsColorOccluded;
        const BYTE throughWall[4] = {
            passColor.r, passColor.g, passColor.b, 255
        };
        runtime.setGlowColor(glowBase, PackRgba(throughWall));
        *eligible = 1;
        runtime.setGlowType(glowBase, 3, 0.0f);
        *glowing = 1;
    }
    else
    {
        runtime.setGlowType(glowBase, original->glowType, 0.0f);
        CopyFourBytes(glowBase + runtime.glowTimeOffset, original->glowTime);
        CopyFourBytes(glowBase + runtime.glowStartTimeOffset,
            original->glowStartTime);
        runtime.setGlowColor(glowBase, PackRgba(original->glowColor));
        CopyFourBytes(glowColor, original->glowColor);
        *eligible = original->eligible;
        *glowing = original->glowing;
    }
    return visiblePass || wallPass || glowPass;
}

static bool ApplyBotHighlight(const BotHighlightRuntime& runtime,
    unsigned int pawnHandle, void* pawn)
{
    if (!g_espConfig.chamsEnemies)
        return false;
    // Two simultaneous render paths replace the old guessed pawn+0x8
    // visibility byte: the normal model remains depth-tested (visible color),
    // while Source 2's screen-highlight pass supplies the through-wall color.
    return ApplyModelPasses(runtime, pawnHandle, pawn, true, true);
}

static bool ApplyActiveWeaponChams(const BotHighlightRuntime& runtime,
    void* entitySystem, void* localPawn)
{
    if (!g_espConfig.chamsWeapons || !localPawn || !entitySystem)
        return false;
    BYTE* pawnBase = reinterpret_cast<BYTE*>(localPawn);
    void** services = reinterpret_cast<void**>(
        pawnBase + runtime.weaponServicesOffset);
    if (!IsAccessible(services, sizeof(void*), false) ||
        !IsAccessible(*services, runtime.activeWeaponOffset +
            sizeof(unsigned int), false))
        return false;
    unsigned int weaponHandle = *reinterpret_cast<unsigned int*>(
        reinterpret_cast<BYTE*>(*services) + runtime.activeWeaponOffset);
    void* weapon = EntityFromHandle(runtime.entity, entitySystem, weaponHandle);
    if (!weapon)
        return false;
    // First-person weapon is already depth-tested; do not force a through-wall
    // pass for it. The same model tint path keeps the effect geometry-accurate.
    return ApplyModelPasses(runtime, weaponHandle, weapon, true, false);
}
'''
replace_once(old_apply, new_apply)

replace_once(
'''    void* localIdentity = IsAccessible(localController, 0x18, false) ?
        *reinterpret_cast<void**>(
            reinterpret_cast<BYTE*>(localController) + 0x10) : nullptr;
    const unsigned int localHandle = EntityHandleFor(localController);
''',
'''    void* localIdentity = IsAccessible(localController, 0x18, false) ?
        *reinterpret_cast<void**>(
            reinterpret_cast<BYTE*>(localController) + 0x10) : nullptr;
    const unsigned int localHandle = EntityHandleFor(localController);
    void* localPawn = nullptr;
    if (localController)
    {
        BYTE* localBase = reinterpret_cast<BYTE*>(localController);
        unsigned int* localPawnHandle = reinterpret_cast<unsigned int*>(
            localBase + g_botHighlightRuntime.playerPawnHandleOffset);
        if (IsAccessible(localPawnHandle, sizeof(unsigned int), false))
            localPawn = EntityFromHandle(g_botHighlightRuntime.entity,
                entitySystem, *localPawnHandle);
    }
''')

replace_once(
'''    int destination = 0;
    for (int i = 0; i < g_originalBotHighlightCount; ++i)
''',
'''    ApplyActiveWeaponChams(g_botHighlightRuntime, entitySystem, localPawn);

    int destination = 0;
    for (int i = 0; i < g_originalBotHighlightCount; ++i)
''')

replace_once(
'''    const wchar_t* styles[] = { L"Flat Solid", L"Textured", L"Wireframe", L"Glass Glow" };
''',
'''    const wchar_t* styles[] = { L"Dual Pass", L"Visible Only", L"Through Wall", L"Glow Pass" };
''')

replace_once(
'''    DrawToggleSwitch(hdc, 390, 205, cfg.chamsHands);
    RECT hRc = { 435, 205, 540, 225 };
    DrawTextW(hdc, L"Hands / Arms", -1, &hRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
''',
'''    DrawToggleSwitch(hdc, 390, 205, false);
    RECT hRc = { 435, 205, 550, 225 };
    SetTextColor(hdc, RGB_COLOR(113, 113, 122));
    DrawTextW(hdc, L"Hands (schema N/A)", -1, &hRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(hdc, RGB_COLOR(228, 228, 231));
''')

replace_once(
'''                if (mouseY >= 175 && mouseY <= 195) { g_espConfig.chamsEnemies = !g_espConfig.chamsEnemies; InvalidateRect(wnd, nullptr, FALSE); return 0; }
                if (mouseY >= 205 && mouseY <= 225) { g_espConfig.chamsHands = !g_espConfig.chamsHands; InvalidateRect(wnd, nullptr, FALSE); return 0; }
                if (mouseY >= 235 && mouseY <= 255) { g_espConfig.chamsWeapons = !g_espConfig.chamsWeapons; InvalidateRect(wnd, nullptr, FALSE); return 0; }
''',
'''                if (mouseY >= 175 && mouseY <= 195) { g_espConfig.chamsEnemies = !g_espConfig.chamsEnemies; QueueBotHighlight(g_espConfig.enable && (g_espConfig.chams || g_espConfig.glow)); InvalidateRect(wnd, nullptr, FALSE); return 0; }
                if (mouseY >= 205 && mouseY <= 225) { return 0; }
                if (mouseY >= 235 && mouseY <= 255) { g_espConfig.chamsWeapons = !g_espConfig.chamsWeapons; QueueBotHighlight(g_espConfig.enable && (g_espConfig.chams || g_espConfig.glow)); InvalidateRect(wnd, nullptr, FALSE); return 0; }
''')

replace_once(
'''                    g_botHighlightEnabled = g_espConfig.enable;
                    QueueBotHighlight(g_espConfig.enable);
''',
'''                    const bool modelEffects = g_espConfig.enable &&
                        (g_espConfig.chams || g_espConfig.glow);
                    g_botHighlightEnabled = modelEffects;
                    QueueBotHighlight(modelEffects);
''')

replace_once(
'''        AppendStatusText(&status, L"Enemies: active (glow + red tint)");
''',
'''        AppendStatusText(&status, L"Chams: model + through-wall passes active");
''')

path.write_text(text, encoding="utf-8")
print("patched", path)
