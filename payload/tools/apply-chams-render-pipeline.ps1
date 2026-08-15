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
$meshBackendPath = Join-Path $sourceDirectory 'visuals\mesh_render_probe.inc'
foreach ($requiredPath in @($pipelinePath, $targetRegistryPath, $meshBackendPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Visual renderer module was not found: $requiredPath"
    }
}
$newPipeline = Get-Content -LiteralPath $pipelinePath -Raw -Encoding UTF8
$targetRegistry = Get-Content -LiteralPath $targetRegistryPath -Raw -Encoding UTF8
$meshBackend = Get-Content -LiteralPath $meshBackendPath -Raw -Encoding UTF8

$oldConfig = @'
    bool chams = true;
    RGBVal chamsColorOccluded = { 239, 68, 68 }; // through-wall render pass
    RGBVal chamsColorVisible = { 132, 204, 22 };  // normal depth-tested model pass
    int chamsStyle = 0; // 0: Dual Pass, 1: Visible Only, 2: Through Wall Only, 3: Glow Pass
'@

$newConfig = @'
    bool chams = true;
    // The mesh renderer owns the normal depth-tested player pass whenever its
    // guarded DrawObject trampoline is installed. Frame-stage tint remains the
    // compatibility fallback when the scene-system entry cannot be resolved.
    RGBVal chamsColorVisible = { 132, 204, 22 };
    // Occluded rendering remains the screen-highlight compatibility backend
    // until the MaterialManager can supply an Ignore-Z material pair.
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
$generatedVisualBackend = $targetRegistry + "`r`n" + $meshBackend + "`r`n" +
    $newPipeline + "`r`n"
$source = $source.Substring(0, $start) + $generatedVisualBackend + $source.Substring($end)

# Keep target selection in FrameStageNotify. DrawObject consumes only the
# published full entity handles and performs no controller/schema scan itself.
$updateStartAnchor = @'
static int UpdateBotHighlights(BotHighlightStats* stats)
{
    if (stats)
'@
$updateStartReplacement = @'
static int UpdateBotHighlights(BotHighlightStats* stats)
{
    ResetVisualTargets();
    BeginVisualTargetUpdate();
    if (stats)
'@
if (-not $source.Contains($updateStartAnchor)) {
    throw 'Visual-target update anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($updateStartAnchor, $updateStartReplacement)

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

# Do not invent a team when the local controller is not ready. A forced CT
# fallback could classify teammates as enemies during connect/map transitions.
$oldTeamFallback = @'
    if (localTeam < 2 || localTeam > 3)
    {
        localTeam = 3; // Fallback CT team so T enemies (team 2) are highlighted
    }
'@
$newTeamFallback = @'
    if (localTeam < 2 || localTeam > 3)
    {
        RestoreAllBotHighlights(stats);
        return BOT_HIGHLIGHT_WAITING_LOCAL;
    }
'@
if (-not $source.Contains($oldTeamFallback)) {
    throw 'Local-team fallback anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($oldTeamFallback, $newTeamFallback)

# The old bot-control bookkeeping no longer participates in target selection.
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

# Install the mesh backend only after the existing frame-stage/schema runtime is
# ready. Failure is non-fatal: frame-stage tint + screen highlight remain the
# compatibility path.
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

# Restore every state owner in reverse order: model/glow bytes, DrawObject
# detour, target registry, then the frame-stage vtable bridge.
$shutdownAnchor = @'
    RemoveFrameStageBridge();
    return 0;
'@
$shutdownReplacement = @'
    if (g_botHighlightRuntimeReady && g_originalBotHighlightCount > 0)
        RestoreAllBotHighlights(nullptr);
    RemoveMeshRenderBackend();
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
    'L"Chams: mesh-visible + occluded compatibility backend"')

$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}

Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Generated modular visual render-pipeline source: $OutputPath"
