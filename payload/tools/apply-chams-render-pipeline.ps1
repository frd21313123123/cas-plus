param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$sourceDirectory = Split-Path -Parent $InputPath
$pipelinePath = Join-Path $sourceDirectory 'visuals\chams_render_pipeline.inc'
$targetRegistryPath = Join-Path $sourceDirectory 'visuals\visual_target_registry.inc'
$targetCollectorPath = Join-Path $sourceDirectory 'visuals\visual_target_collector.inc'
$materialManagerPath = Join-Path $sourceDirectory 'visuals\material_manager.inc'
$targetUiPath = Join-Path $sourceDirectory 'visuals\chams_target_ui.inc'
$materialUiPath = Join-Path $sourceDirectory 'visuals\chams_material_ui.inc'
$meshBackendPath = Join-Path $sourceDirectory 'visuals\mesh_render_probe.inc'
$diagnosticsPath = Join-Path $sourceDirectory 'visuals\visual_diagnostics.inc'
foreach ($requiredPath in @($pipelinePath, $targetRegistryPath, $targetCollectorPath, $materialManagerPath, $targetUiPath, $materialUiPath, $meshBackendPath, $diagnosticsPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Visual renderer module was not found: $requiredPath"
    }
}
$newPipeline = Get-Content -LiteralPath $pipelinePath -Raw -Encoding UTF8
$targetRegistry = Get-Content -LiteralPath $targetRegistryPath -Raw -Encoding UTF8
$targetCollector = Get-Content -LiteralPath $targetCollectorPath -Raw -Encoding UTF8
$materialManager = Get-Content -LiteralPath $materialManagerPath -Raw -Encoding UTF8
$targetUi = Get-Content -LiteralPath $targetUiPath -Raw -Encoding UTF8
$materialUi = Get-Content -LiteralPath $materialUiPath -Raw -Encoding UTF8
$meshBackend = Get-Content -LiteralPath $meshBackendPath -Raw -Encoding UTF8
$diagnostics = Get-Content -LiteralPath $diagnosticsPath -Raw -Encoding UTF8

$oldConfig = @'
    bool chams = true;
    RGBVal chamsColorOccluded = { 239, 68, 68 }; // through-wall render pass
    RGBVal chamsColorVisible = { 132, 204, 22 };  // normal depth-tested model pass
    int chamsStyle = 0; // 0: Dual Pass, 1: Visible Only, 2: Through Wall Only, 3: Glow Pass
'@
$newConfig = @'
    bool chams = true;
    // The scene-system renderer owns mesh passes whenever its guarded
    // DrawObject trampoline and runtime MaterialManager are available.
    RGBVal chamsColorVisible = { 132, 204, 22 };
    RGBVal chamsColorOccluded = { 239, 68, 68 };
    // 0: Visible + Occluded, 1: Visible Only, 2: Occluded Only,
    // 3: legacy Glow Only compatibility mode.
    int chamsStyle = 0;
'@
if (-not $source.Contains($oldConfig)) {
    throw 'Chams config anchor was not found. The payload source changed; refusing to patch blindly.'
}
$source = $source.Replace($oldConfig, $newConfig)

$targetConfigAnchor = @'
    bool chamsEnemies = true;
    bool chamsHands = false; // intentionally unavailable until a schema-backed viewmodel handle exists
    bool chamsWeapons = false;
'@
$targetConfigReplacement = @'
    bool chamsEnemies = true;
    bool chamsAllies = false;
    bool chamsLocal = false;
    bool chamsHands = false;
    bool chamsWeapons = false;
    bool chamsDroppedWeapons = false;
    bool chamsGrenades = false;
    bool chamsBomb = false;
'@
if (-not $source.Contains($targetConfigAnchor)) { throw 'Chams target config anchor was not found. Refusing to patch blindly.' }
$source = $source.Replace($targetConfigAnchor, $targetConfigReplacement)

$runtimeStructAnchor = @'
    unsigned int healthOffset;
    unsigned int lifeStateOffset;
    unsigned int localControllerOffset;
    unsigned int steamIdOffset;
    unsigned int playerPawnHandleOffset;
    unsigned int pawnAliveOffset;
    unsigned int controllingBotOffset;
    unsigned int botDifficultyOffset;
    unsigned int weaponServicesOffset;
    unsigned int activeWeaponOffset;
    unsigned int glowOffset;
'@
$runtimeStructReplacement = @'
    unsigned int healthOffset;
    unsigned int lifeStateOffset;
    unsigned int ownerEntityOffset;
    unsigned int localControllerOffset;
    unsigned int steamIdOffset;
    unsigned int playerPawnHandleOffset;
    unsigned int pawnAliveOffset;
    unsigned int controllingBotOffset;
    unsigned int botDifficultyOffset;
    unsigned int weaponServicesOffset;
    unsigned int activeWeaponOffset;
    unsigned int hudModelArmsOffset;
    unsigned int glowOffset;
'@
if (-not $source.Contains($runtimeStructAnchor)) { throw 'Visual runtime struct anchor was not found. Refusing to patch blindly.' }
$source = $source.Replace($runtimeStructAnchor, $runtimeStructReplacement)

$schemaClassAnchor = @'
    SchemaClassInfoView* basePlayerPawn = FindSchemaClass(scope,
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
'@
$schemaClassReplacement = @'
    SchemaClassInfoView* basePlayerPawn = FindSchemaClass(scope,
        "C_BasePlayerPawn");
    SchemaClassInfoView* csPlayerPawn = FindSchemaClass(scope,
        "C_CSPlayerPawn");
    SchemaClassInfoView* weaponServices = FindSchemaClass(scope,
        "CPlayer_WeaponServices");
    SchemaClassInfoView* baseModelEntity = FindSchemaClass(scope,
        "C_BaseModelEntity");
    SchemaClassInfoView* glowProperty = FindSchemaClass(scope,
        "CGlowProperty");
    if (!baseEntity || !baseController || !playerController ||
        !basePlayerPawn || !csPlayerPawn || !weaponServices ||
        !baseModelEntity || !glowProperty)
        return false;
'@
if (-not $source.Contains($schemaClassAnchor)) { throw 'Visual schema class anchor was not found. Refusing to patch blindly.' }
$source = $source.Replace($schemaClassAnchor, $schemaClassReplacement)

$ownerFieldAnchor = @'
        !FindSchemaField(baseEntity, "m_iHealth", &runtime->healthOffset) ||
        !FindSchemaField(baseEntity, "m_lifeState", &runtime->lifeStateOffset) ||
        !FindSchemaField(baseController, "m_bIsLocalPlayerController",
'@
$ownerFieldReplacement = @'
        !FindSchemaField(baseEntity, "m_iHealth", &runtime->healthOffset) ||
        !FindSchemaField(baseEntity, "m_lifeState", &runtime->lifeStateOffset) ||
        !FindSchemaField(baseEntity, "m_hOwnerEntity", &runtime->ownerEntityOffset) ||
        !FindSchemaField(baseController, "m_bIsLocalPlayerController",
'@
if (-not $source.Contains($ownerFieldAnchor)) { throw 'Owner-handle schema anchor was not found. Refusing to patch blindly.' }
$source = $source.Replace($ownerFieldAnchor, $ownerFieldReplacement)

$armsFieldAnchor = @'
        !FindSchemaField(weaponServices, "m_hActiveWeapon",
            &runtime->activeWeaponOffset) ||
        !FindSchemaField(baseModelEntity, "m_Glow", &runtime->glowOffset) ||
'@
$armsFieldReplacement = @'
        !FindSchemaField(weaponServices, "m_hActiveWeapon",
            &runtime->activeWeaponOffset) ||
        !FindSchemaField(csPlayerPawn, "m_hHudModelArms",
            &runtime->hudModelArmsOffset) ||
        !FindSchemaField(baseModelEntity, "m_Glow", &runtime->glowOffset) ||
'@
if (-not $source.Contains($armsFieldAnchor)) { throw 'HUD arms schema anchor was not found. Refusing to patch blindly.' }
$source = $source.Replace($armsFieldAnchor, $armsFieldReplacement)

$runtimeBoundsAnchor = @'
        runtime->weaponServicesOffset + sizeof(void*) >
            static_cast<unsigned int>(basePlayerPawn->size) ||
        runtime->activeWeaponOffset + sizeof(unsigned int) >
            static_cast<unsigned int>(weaponServices->size) ||
        runtime->glowOffset + runtime->glowingOffset >=
'@
$runtimeBoundsReplacement = @'
        runtime->ownerEntityOffset + sizeof(unsigned int) >
            static_cast<unsigned int>(baseEntity->size) ||
        runtime->weaponServicesOffset + sizeof(void*) >
            static_cast<unsigned int>(basePlayerPawn->size) ||
        runtime->activeWeaponOffset + sizeof(unsigned int) >
            static_cast<unsigned int>(weaponServices->size) ||
        runtime->hudModelArmsOffset + sizeof(unsigned int) >
            static_cast<unsigned int>(csPlayerPawn->size) ||
        runtime->glowOffset + runtime->glowingOffset >=
'@
if (-not $source.Contains($runtimeBoundsAnchor)) { throw 'Visual runtime bounds anchor was not found. Refusing to patch blindly.' }
$source = $source.Replace($runtimeBoundsAnchor, $runtimeBoundsReplacement)

$startAnchor = 'static bool ApplyModelPasses(const BotHighlightRuntime& runtime,'
$endAnchor = 'static bool ApplyBotHighlight(const BotHighlightRuntime& runtime,'
$start = $source.IndexOf($startAnchor)
$end = $source.IndexOf($endAnchor)
if ($start -lt 0 -or $end -le $start) {
    throw 'ApplyModelPasses anchors were not found. The payload source changed; refusing to patch blindly.'
}
$generatedVisualBackend = $targetRegistry + "`r`n" + $targetCollector + "`r`n" +
    $materialManager + "`r`n" + $targetUi + "`r`n" + $materialUi + "`r`n" +
    $meshBackend + "`r`n" + $diagnostics + "`r`n" + $newPipeline + "`r`n"
$source = $source.Substring(0, $start) + $generatedVisualBackend + $source.Substring($end)

# Discovery can start early, but expensive/one-shot render backend resolution
# happens only after the entity/schema/local lifecycle is fully validated.
$updateStartAnchor = @'
static int UpdateBotHighlights(BotHighlightStats* stats)
{
    if (stats)
'@
$updateStartReplacement = @'
static int UpdateBotHighlights(BotHighlightStats* stats)
{
    BeginVisualTargetUpdate();
    if (stats)
'@
if (-not $source.Contains($updateStartAnchor)) { throw 'Visual-target update anchor was not found. Refusing to patch blindly.' }
$source = $source.Replace($updateStartAnchor, $updateStartReplacement)

$runtimeAnchor = @'
    if (!g_preResolvedEntityRuntimeReady)
        return BOT_HIGHLIGHT_ERR_RUNTIME;
'@
$runtimeReplacement = @'
    if (!g_preResolvedEntityRuntimeReady)
    {
        ResetVisualTargets();
        return BOT_HIGHLIGHT_ERR_RUNTIME;
    }
'@
if (-not $source.Contains($runtimeAnchor)) { throw 'Visual runtime early-return anchor was not found. Refusing to patch blindly.' }
$source = $source.Replace($runtimeAnchor, $runtimeReplacement)

$entitySystemAnchor = @'
    void* entitySystem = CurrentEntitySystem(g_preResolvedEntityRuntime);
    if (!entitySystem)
        return BOT_HIGHLIGHT_ERR_ENTITY_SYSTEM;
'@
$entitySystemReplacement = @'
    void* entitySystem = CurrentEntitySystem(g_preResolvedEntityRuntime);
    if (!entitySystem)
    {
        ResetVisualTargets();
        return BOT_HIGHLIGHT_ERR_ENTITY_SYSTEM;
    }
'@
if (-not $source.Contains($entitySystemAnchor)) { throw 'Visual entity-system early-return anchor was not found. Refusing to patch blindly.' }
$source = $source.Replace($entitySystemAnchor, $entitySystemReplacement)

# The extended target collector needs the local pawn and local handle that the
# legacy function already resolves. It appends typed targets after enemy pawns.
$enemyLoopTail = @'
        if (ApplyBotHighlight(g_botHighlightRuntime, *pawnHandle, pawn) &&
            stats)
            ++stats->highlighted;
    }

    ApplyActiveWeaponChams(g_botHighlightRuntime, entitySystem, localPawn);
'@
$enemyLoopTailReplacement = @'
        if (ApplyBotHighlight(g_botHighlightRuntime, *pawnHandle, pawn) &&
            stats)
            ++stats->highlighted;
    }

    CollectExtendedVisualTargets(g_botHighlightRuntime, entitySystem,
        localController, localPawn, localHandle, localTeam);
    PublishVisualTargets();

    ApplyActiveWeaponChams(g_botHighlightRuntime, entitySystem, localPawn);
'@
if (-not $source.Contains($enemyLoopTail)) { throw 'Visual target publication anchor was not found. Refusing to patch blindly.' }
$source = $source.Replace($enemyLoopTail, $enemyLoopTailReplacement)

# Publish an empty target generation before all disabled/restoration exits so a
# render thread never holds stale targets from the previous frame.
$disabledAnchor = @'
    if (!g_botHighlightEnabled || !g_espConfig.enable ||
        (!g_espConfig.chams && !g_espConfig.glow))
    {
        RestoreAllBotHighlights(stats);
        return BOT_HIGHLIGHT_OK;
    }
'@
$disabledReplacement = @'
    if (!g_botHighlightEnabled || !g_espConfig.enable ||
        (!g_espConfig.chams && !g_espConfig.glow))
    {
        RestoreAllBotHighlights(stats);
        PublishVisualTargets();
        return BOT_HIGHLIGHT_OK;
    }
'@
if (-not $source.Contains($disabledAnchor)) { throw 'Visual disabled-state publication anchor was not found. Refusing to patch blindly.' }
$source = $source.Replace($disabledAnchor, $disabledReplacement)

# Resolve/install the mesh backend only from FrameStage after the game entity
# lifecycle and schemas are valid. Material creation also stays on this thread.
$localReadyAnchor = @'
    if (!localIdentity || localHandle == 0xFFFFFFFFu)
    {
        RestoreAllBotHighlights(stats);
        return BOT_HIGHLIGHT_ERR_LOCAL;
    }

'@
$localReadyReplacement = @'
    if (!localIdentity || localHandle == 0xFFFFFFFFu)
    {
        RestoreAllBotHighlights(stats);
        PublishVisualTargets();
        return BOT_HIGHLIGHT_ERR_LOCAL;
    }

    if (g_espConfig.chams && g_espConfig.chamsStyle != 3)
    {
        if (InstallMeshRenderBackend())
            EnsureSelectedChamsMaterialsReady();
    }

'@
if (-not $source.Contains($localReadyAnchor)) { throw 'Mesh backend install lifecycle anchor was not found. Refusing to patch blindly.' }
$source = $source.Replace($localReadyAnchor, $localReadyReplacement)

# Replace the legacy chams-modal target controls with the modular target picker.
$oldTargetToggle = @'
    DrawToggleSwitch(hdc, 390, 175, cfg.chamsEnemies);
    RECT eRc = { 435, 175, 540, 195 };
    DrawTextW(hdc, L"Enemy Pawns", -1, &eRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    DrawToggleSwitch(hdc, 390, 205, false);
    RECT hRc = { 435, 205, 550, 225 };
    SetTextColor(hdc, RGB_COLOR(113, 113, 122));
    DrawTextW(hdc, L"Hands (schema N/A)", -1, &hRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(hdc, RGB_COLOR(228, 228, 231));

    DrawToggleSwitch(hdc, 390, 235, cfg.chamsWeapons);
    RECT wRc = { 435, 235, 540, 255 };
    DrawTextW(hdc, L"Weapons", -1, &wRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
'@
# Some revisions no longer contain the first target block after the modal body
# was reorganized. It is safe to remove it only when present; the final draw
# anchor below is mandatory.
if ($source.Contains($oldTargetToggle)) { $source = $source.Replace($oldTargetToggle, '') }

$oldBotBookkeeping = @'
    unsigned int humanControlledPawns[64]{};
    int humanControlledPawnCount = 0;
    for (int index = 1; index <= 64 && humanControlledPawnCount < 64; ++index)
    {
        void* controller = EntityAtIndex(g_preResolvedEntityRuntime,
            entitySystem, index);
        if (!HasDesignerName(controller, "cs_player_controller"))
            continue;
        BYTE* base = reinterpret_cast<BYTE*>(controller);
        unsigned int* pawnHandle = reinterpret_cast<unsigned int*>(
            base + g_botHighlightRuntime.playerPawnHandleOffset);
        BYTE* controllingBot = base + g_botHighlightRuntime.controllingBotOffset;
        if (IsAccessible(controllingBot, 1, false) &&
            IsAccessible(pawnHandle, sizeof(unsigned int), false) &&
            !IsBotController(g_botHighlightRuntime, base) &&
            *controllingBot != 0)
            humanControlledPawns[humanControlledPawnCount++] = *pawnHandle;
    }

'@
if (-not $source.Contains($oldBotBookkeeping)) { throw 'Stale bot bookkeeping anchor was not found. Refusing to patch blindly.' }
$source = $source.Replace($oldBotBookkeeping, '')

$modalSizeAnchor = 'DrawRoundedCard(hdc, 220, 110, 340, 270, RGB_COLOR(28, 28, 34), RGB_COLOR(60, 60, 70), 8);'
$modalSizeReplacement = 'DrawRoundedCard(hdc, 220, 110, 400, 270, RGB_COLOR(28, 28, 34), RGB_COLOR(60, 60, 70), 8);'
if (-not $source.Contains($modalSizeAnchor)) { throw 'Chams modal size anchor was not found. Refusing to patch blindly.' }
$source = $source.Replace($modalSizeAnchor, $modalSizeReplacement)

$targetDrawAnchor = @'
    SetTextColor(hdc, RGB_COLOR(228, 228, 231));
    RECT lblRc = { 390, 150, 540, 170 };
    DrawTextW(hdc, L"Chams Targets:", -1, &lblRc, DT_LEFT | DT_SINGLELINE);

    DrawToggleSwitch(hdc, 390, 175, cfg.chamsEnemies);
    RECT eRc = { 435, 175, 540, 195 };
    DrawTextW(hdc, L"Enemy Pawns", -1, &eRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    DrawToggleSwitch(hdc, 390, 205, false);
    RECT hRc = { 435, 205, 550, 225 };
    SetTextColor(hdc, RGB_COLOR(113, 113, 122));
    DrawTextW(hdc, L"Hands (schema N/A)", -1, &hRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(hdc, RGB_COLOR(228, 228, 231));

    DrawToggleSwitch(hdc, 390, 235, cfg.chamsWeapons);
    RECT wRc = { 435, 235, 540, 255 };
    DrawTextW(hdc, L"Weapons", -1, &wRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    DrawRoundedCard(hdc, 235, 335, 310, 32, RGB_COLOR(39, 39, 45), RGB_COLOR(60, 60, 70), 6);
'@
$targetDrawReplacement = @'
    DrawChamsTargetControls(hdc, cfg);
    DrawChamsMaterialPresetControls(hdc);

    DrawRoundedCard(hdc, 235, 335, 365, 32, RGB_COLOR(39, 39, 45), RGB_COLOR(60, 60, 70), 6);
'@
if (-not $source.Contains($targetDrawAnchor)) { throw 'Chams final target UI draw anchor was not found. Refusing to patch blindly.' }
$source = $source.Replace($targetDrawAnchor, $targetDrawReplacement)
$source = $source.Replace('RECT btnRc = { 235, 335, 545, 367 };', 'RECT btnRc = { 235, 335, 600, 367 };')

$targetClickAnchor = @'
            if (mouseX >= 385 && mouseX <= 430)
            {
                if (mouseY >= 175 && mouseY <= 195) { g_espConfig.chamsEnemies = !g_espConfig.chamsEnemies; QueueBotHighlight(g_espConfig.enable && (g_espConfig.chams || g_espConfig.glow)); InvalidateRect(wnd, nullptr, FALSE); return 0; }
                if (mouseY >= 205 && mouseY <= 225) { return 0; }
                if (mouseY >= 235 && mouseY <= 255) { g_espConfig.chamsWeapons = !g_espConfig.chamsWeapons; QueueBotHighlight(g_espConfig.enable && (g_espConfig.chams || g_espConfig.glow)); InvalidateRect(wnd, nullptr, FALSE); return 0; }
            }
            if ((mouseX >= 235 && mouseX <= 545 && mouseY >= 335 && mouseY <= 367) ||
                mouseX < 220 || mouseX > 560 || mouseY < 110 || mouseY > 380)
'@
$targetClickReplacement = @'
            if (HandleChamsTargetClick(mouseX, mouseY))
            {
                InvalidateRect(wnd, nullptr, FALSE);
                return 0;
            }
            if (HandleChamsMaterialPresetClick(mouseX, mouseY))
            {
                InvalidateRect(wnd, nullptr, FALSE);
                return 0;
            }
            if ((mouseX >= 235 && mouseX <= 600 && mouseY >= 335 && mouseY <= 367) ||
                mouseX < 220 || mouseX > 620 || mouseY < 110 || mouseY > 380)
'@
if (-not $source.Contains($targetClickAnchor)) { throw 'Chams final target UI click anchor was not found. Refusing to patch blindly.' }
$source = $source.Replace($targetClickAnchor, $targetClickReplacement)

$statusAnchor = @'
    SetBotStatus(status.text);
}

static void DrawRoundedCard(HDC hdc, int x, int y, int w, int h, COLORREF bg, COLORREF border, int radius)
'@
$statusReplacement = @'
    AppendVisualRendererDiagnostics(&status);
    SetBotStatus(status.text);
}

static void DrawRoundedCard(HDC hdc, int x, int y, int w, int h, COLORREF bg, COLORREF border, int radius)
'@
if (-not $source.Contains($statusAnchor)) { throw 'Visual diagnostics status anchor was not found. Refusing to patch blindly.' }
$source = $source.Replace($statusAnchor, $statusReplacement)

$startupAnchor = @'
    if (!InstallFrameStageBridge())
    {
        SetSkyboxStatus(L"Sky: failed to install the frame-stage bridge.");
        SetBotStatus(L"Bots: failed to install the frame-stage bridge.");
    }
    PositionMenuOverGame();
'@
$startupReplacement = @'
    if (!InstallFrameStageBridge())
    {
        SetSkyboxStatus(L"Sky: failed to install the frame-stage bridge.");
        SetBotStatus(L"Visuals: failed to install the frame-stage bridge.");
    }
    else
    {
        const bool modelEffects = g_espConfig.enable &&
            (g_espConfig.chams || g_espConfig.glow);
        g_botHighlightEnabled = modelEffects;
        QueueBotHighlight(modelEffects);
    }
    PositionMenuOverGame();
'@
if (-not $source.Contains($startupAnchor)) { throw 'Frame-stage startup anchor was not found. Refusing to patch blindly.' }
$source = $source.Replace($startupAnchor, $startupReplacement)

$shutdownAnchor = @'
    RemoveFrameStageBridge();
    return 0;
'@
$shutdownReplacement = @'
    if (g_botHighlightRuntimeReady && g_originalBotHighlightCount > 0)
        RestoreAllBotHighlights(nullptr);
    const bool meshRemoved = RemoveMeshRenderBackend();
    if (meshRemoved)
    {
        ResetMaterialManagerState();
        ResetVisualTargets();
        g_meshRenderBackend.drawObjectTarget = nullptr;
        g_meshRenderBackend.resolved = false;
    }
    RemoveFrameStageBridge();
    return 0;
'@
if (-not $source.Contains($shutdownAnchor)) { throw 'Payload shutdown anchor was not found. Refusing to patch blindly.' }
$source = $source.Replace($shutdownAnchor, $shutdownReplacement)

$source = $source.Replace(
    'const wchar_t* styles[] = { L"Dual Pass", L"Visible Only", L"Through Wall", L"Glow Pass" };',
    'const wchar_t* styles[] = { L"Visible + Occluded", L"Visible Only", L"Occluded Only", L"Glow Only" };')
$source = $source.Replace(
    'L"Chams: model + through-wall passes active"',
    'L"Chams: scene mesh backend + compatibility fallback"')
$source = $source.Replace('AppendStatusText(&status, L", bots ");', 'AppendStatusText(&status, L", targets ");')

$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory) { New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null }
Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Generated modular visual render-pipeline source: $OutputPath"

# Keep the high-frequency DrawObject optimization isolated from the structural
# renderer generator. It uses exact anchors and fails closed if the scene module
# changes, instead of silently shipping the old O(draws * targets) path.
$hotPathFix = Join-Path $PSScriptRoot 'apply-visual-hotpath-fix.ps1'
& $hotPathFix -InputPath $OutputPath
