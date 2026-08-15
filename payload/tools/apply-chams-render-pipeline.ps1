param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$sourceDirectory = Split-Path -Parent $InputPath
$noCrtMemoryPath = Join-Path $sourceDirectory 'core\no_crt_memory.inc'
$pipelinePath = Join-Path $sourceDirectory 'visuals\chams_render_pipeline.inc'
$targetRegistryPath = Join-Path $sourceDirectory 'visuals\visual_target_registry.inc'
$materialManagerPath = Join-Path $sourceDirectory 'visuals\material_manager.inc'
$materialUiPath = Join-Path $sourceDirectory 'visuals\chams_material_ui.inc'
$meshBackendPath = Join-Path $sourceDirectory 'visuals\mesh_render_probe.inc'
foreach ($requiredPath in @($noCrtMemoryPath, $pipelinePath, $targetRegistryPath, $materialManagerPath, $materialUiPath, $meshBackendPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Visual renderer module was not found: $requiredPath"
    }
}
$noCrtMemory = Get-Content -LiteralPath $noCrtMemoryPath -Raw -Encoding UTF8
$newPipeline = Get-Content -LiteralPath $pipelinePath -Raw -Encoding UTF8
$targetRegistry = Get-Content -LiteralPath $targetRegistryPath -Raw -Encoding UTF8
$materialManager = Get-Content -LiteralPath $materialManagerPath -Raw -Encoding UTF8
$materialUi = Get-Content -LiteralPath $materialUiPath -Raw -Encoding UTF8
$meshBackend = Get-Content -LiteralPath $meshBackendPath -Raw -Encoding UTF8

$oldConfig = @'
    bool chams = true;
    RGBVal chamsColorOccluded = { 239, 68, 68 }; // through-wall render pass
    RGBVal chamsColorVisible = { 132, 204, 22 };  // normal depth-tested model pass
    int chamsStyle = 0; // 0: Dual Pass, 1: Visible Only, 2: Through Wall Only, 3: Glow Pass
'@

$newConfig = @'
    bool chams = true;
    // The scene-system renderer owns enemy mesh passes whenever its guarded
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

$startAnchor = 'static bool ApplyModelPasses(const BotHighlightRuntime& runtime,'
$endAnchor = 'static bool ApplyBotHighlight(const BotHighlightRuntime& runtime,'
$start = $source.IndexOf($startAnchor)
$end = $source.IndexOf($endAnchor)
if ($start -lt 0 -or $end -le $start) {
    throw 'ApplyModelPasses anchors were not found. The payload source changed; refusing to patch blindly.'
}
$generatedVisualBackend = $noCrtMemory + "`r`n" + $targetRegistry + "`r`n" +
    $materialManager + "`r`n" + $materialUi + "`r`n" + $meshBackend + "`r`n" +
    $newPipeline + "`r`n"
$source = $source.Substring(0, $start) + $generatedVisualBackend + $source.Substring($end)

# Keep target selection in FrameStageNotify. DrawObject consumes a previously
# published complete snapshot while a fresh list is built off to the side.
# Material creation also runs here so KeyValues3/MaterialSystem2 calls stay on
# the CS2 game/render lifecycle rather than the manual-map worker thread.
$updateStartAnchor = @'
static int UpdateBotHighlights(BotHighlightStats* stats)
{
    if (stats)
'@
$updateStartReplacement = @'
static int UpdateBotHighlights(BotHighlightStats* stats)
{
    EnsureMaterialManagerReady();
    BeginVisualTargetUpdate();
    if (stats)
'@
if (-not $source.Contains($updateStartAnchor)) {
    throw 'Visual-target update anchor was not found. Refusing to patch blindly.'
}
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
if (-not $source.Contains($runtimeAnchor)) {
    throw 'Entity-runtime failure anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($runtimeAnchor, $runtimeReplacement)

$schemaAnchor = @'
        if (!clientModule ||
            !ResolveBotHighlightRuntime(clientModule, &resolved))
            return BOT_HIGHLIGHT_ERR_SCHEMA;
'@
$schemaReplacement = @'
        if (!clientModule ||
            !ResolveBotHighlightRuntime(clientModule, &resolved))
        {
            ResetVisualTargets();
            return BOT_HIGHLIGHT_ERR_SCHEMA;
        }
'@
if (-not $source.Contains($schemaAnchor)) {
    throw 'Schema failure anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($schemaAnchor, $schemaReplacement)

$mapAnchor = @'
    void* entitySystem = CurrentEntitySystem(g_botHighlightRuntime.entity);
    if (!entitySystem)
        return BOT_HIGHLIGHT_WAITING_MAP;
'@
$mapReplacement = @'
    void* entitySystem = CurrentEntitySystem(g_botHighlightRuntime.entity);
    if (!entitySystem)
    {
        ResetVisualTargets();
        return BOT_HIGHLIGHT_WAITING_MAP;
    }
'@
if (-not $source.Contains($mapAnchor)) {
    throw 'Map wait anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($mapAnchor, $mapReplacement)

$targetApplyAnchor = @'
        if (stats)
            ++stats->botCandidates;
        if (ApplyBotHighlight(g_botHighlightRuntime, *pawnHandle, pawn) &&
            stats)
            ++stats->highlighted;
'@
$targetApplyReplacement = @'
        if (stats)
            ++stats->botCandidates;
        AddEnemyVisualTarget(*pawnHandle);
        if (ApplyBotHighlight(g_botHighlightRuntime, *pawnHandle, pawn) &&
            stats)
            ++stats->highlighted;
'@
if (-not $source.Contains($targetApplyAnchor)) {
    throw 'Enemy visual-target publication anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($targetApplyAnchor, $targetApplyReplacement)

$publishAnchor = @'
    ApplyActiveWeaponChams(g_botHighlightRuntime, entitySystem, localPawn);

    int destination = 0;
'@
$publishReplacement = @'
    ApplyActiveWeaponChams(g_botHighlightRuntime, entitySystem, localPawn);
    PublishVisualTargets();

    int destination = 0;
'@
if (-not $source.Contains($publishAnchor)) {
    throw 'Visual-target publish anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($publishAnchor, $publishReplacement)

$oldTeamFallback = @'
    if (localTeam < 2 || localTeam > 3)
    {
        localTeam = 3; // Fallback CT team so T enemies (team 2) are highlighted
    }
'@
$newTeamFallback = @'
    if (localTeam < 2 || localTeam > 3)
    {
        ResetVisualTargets();
        RestoreAllBotHighlights(stats);
        return BOT_HIGHLIGHT_WAITING_LOCAL;
    }
'@
if (-not $source.Contains($oldTeamFallback)) {
    throw 'Local-team fallback anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($oldTeamFallback, $newTeamFallback)

$localIdentityAnchor = @'
    if (!localIdentity || localHandle == 0xFFFFFFFFu)
    {
        RestoreAllBotHighlights(stats);
        return BOT_HIGHLIGHT_WAITING_LOCAL;
    }
'@
$localIdentityReplacement = @'
    if (!localIdentity || localHandle == 0xFFFFFFFFu)
    {
        ResetVisualTargets();
        RestoreAllBotHighlights(stats);
        return BOT_HIGHLIGHT_WAITING_LOCAL;
    }
'@
if (-not $source.Contains($localIdentityAnchor)) {
    throw 'Local identity wait anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($localIdentityAnchor, $localIdentityReplacement)

$oldBotBookkeeping = @'
    unsigned int humanControlledPawns[kControllerSlotLimit];
    ZeroBytes(humanControlledPawns, sizeof(humanControlledPawns));
    int humanControlledPawnCount = 0;
    for (int i = 0; i < controllerCount; ++i)
    {
        BYTE* base = reinterpret_cast<BYTE*>(controllers[i]);
        BYTE* controllingBot = base +
            g_botHighlightRuntime.controllingBotOffset;
        unsigned int* pawnHandle = reinterpret_cast<unsigned int*>(
            base + g_botHighlightRuntime.playerPawnHandleOffset);
        if (IsAccessible(controllingBot, 1, false) &&
            IsAccessible(pawnHandle, sizeof(unsigned int), false) &&
            !IsBotController(g_botHighlightRuntime, base) &&
            *controllingBot != 0)
            humanControlledPawns[humanControlledPawnCount++] = *pawnHandle;
    }

'@
if (-not $source.Contains($oldBotBookkeeping)) {
    throw 'Stale bot bookkeeping anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($oldBotBookkeeping, '')

# Add independent material selectors to the existing Chams modal without moving
# the rest of the Win32 UI into the renderer modules yet.
$materialDrawAnchor = @'
    DrawToggleSwitch(hdc, 390, 235, cfg.chamsWeapons);
    RECT wRc = { 435, 235, 540, 255 };
    DrawTextW(hdc, L"Weapons", -1, &wRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    DrawRoundedCard(hdc, 235, 335, 310, 32, RGB_COLOR(39, 39, 45), RGB_COLOR(60, 60, 70), 6);
'@
$materialDrawReplacement = @'
    DrawToggleSwitch(hdc, 390, 235, cfg.chamsWeapons);
    RECT wRc = { 435, 235, 540, 255 };
    DrawTextW(hdc, L"Weapons", -1, &wRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    DrawChamsMaterialPresetControls(hdc);

    DrawRoundedCard(hdc, 235, 335, 310, 32, RGB_COLOR(39, 39, 45), RGB_COLOR(60, 60, 70), 6);
'@
if (-not $source.Contains($materialDrawAnchor)) {
    throw 'Chams material UI draw anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($materialDrawAnchor, $materialDrawReplacement)

$materialClickAnchor = @'
            if ((mouseX >= 235 && mouseX <= 545 && mouseY >= 335 && mouseY <= 367) ||
                mouseX < 220 || mouseX > 560 || mouseY < 110 || mouseY > 380)
'@
$materialClickReplacement = @'
            if (HandleChamsMaterialPresetClick(mouseX, mouseY))
            {
                InvalidateRect(wnd, nullptr, FALSE);
                return 0;
            }
            if ((mouseX >= 235 && mouseX <= 545 && mouseY >= 335 && mouseY <= 367) ||
                mouseX < 220 || mouseX > 560 || mouseY < 110 || mouseY > 380)
'@
if (-not $source.Contains($materialClickAnchor)) {
    throw 'Chams material UI click anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($materialClickAnchor, $materialClickReplacement)

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
        InstallMeshRenderBackend();
        const bool modelEffects = g_espConfig.enable &&
            (g_espConfig.chams || g_espConfig.glow);
        g_botHighlightEnabled = modelEffects;
        QueueBotHighlight(modelEffects);
    }
    PositionMenuOverGame();
'@
if (-not $source.Contains($startupAnchor)) {
    throw 'Frame-stage startup anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($startupAnchor, $startupReplacement)

$shutdownAnchor = @'
    RemoveFrameStageBridge();
    return 0;
'@
$shutdownReplacement = @'
    if (g_botHighlightRuntimeReady && g_originalBotHighlightCount > 0)
        RestoreAllBotHighlights(nullptr);
    RemoveMeshRenderBackend();
    ResetMaterialManagerState();
    ResetVisualTargets();
    g_meshRenderBackend.drawObjectTarget = nullptr;
    g_meshRenderBackend.resolved = false;
    RemoveFrameStageBridge();
    return 0;
'@
if (-not $source.Contains($shutdownAnchor)) {
    throw 'Payload shutdown anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($shutdownAnchor, $shutdownReplacement)

$source = $source.Replace(
    'const wchar_t* styles[] = { L"Dual Pass", L"Visible Only", L"Through Wall", L"Glow Pass" };',
    'const wchar_t* styles[] = { L"Visible + Occluded", L"Visible Only", L"Occluded Only", L"Glow Only" };')
$source = $source.Replace(
    'L"Chams: model + through-wall passes active"',
    'L"Chams: scene mesh backend + compatibility fallback"')

$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}
Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Generated modular visual render-pipeline source: $OutputPath"
