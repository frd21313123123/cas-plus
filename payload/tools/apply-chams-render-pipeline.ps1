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
if (-not (Test-Path -LiteralPath $pipelinePath)) {
    throw "Chams render pipeline module was not found: $pipelinePath"
}
$newPipeline = Get-Content -LiteralPath $pipelinePath -Raw -Encoding UTF8

$oldConfig = @'
    bool chams = true;
    RGBVal chamsColorOccluded = { 239, 68, 68 }; // through-wall render pass
    RGBVal chamsColorVisible = { 132, 204, 22 };  // normal depth-tested model pass
    int chamsStyle = 0; // 0: Dual Pass, 1: Visible Only, 2: Through Wall Only, 3: Glow Pass
'@

$newConfig = @'
    bool chams = true;
    // Visible material pass stays depth-tested and therefore follows the model
    // geometry only where the scene depth buffer allows it to be seen.
    RGBVal chamsColorVisible = { 132, 204, 22 };
    // The current compatibility backend uses Source 2 screen highlight here.
    // It is isolated behind ChamsPassPlan so a real Ignore-Z mesh backend can
    // replace it without changing target selection or standalone Glow.
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
$source = $source.Substring(0, $start) + $newPipeline + "`r`n" + $source.Substring($end)

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

# ESPConfig defaults must become active immediately after the bridge is ready;
# previously Chams could appear enabled in the UI while the backend stayed off
# until the user clicked a visual setting.
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
        SetBotStatus(L"Bots: failed to install the frame-stage bridge.");
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
if (-not $source.Contains($startupAnchor)) {
    throw 'Frame-stage startup anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($startupAnchor, $startupReplacement)

# Restore entity render/glow state before detaching the frame-stage hook.
$shutdownAnchor = @'
    RemoveFrameStageBridge();
    return 0;
'@
$shutdownReplacement = @'
    if (g_botHighlightRuntimeReady && g_originalBotHighlightCount > 0)
        RestoreAllBotHighlights(nullptr);
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
    'L"Chams: visible pass + occluded backend active"')

$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}

Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Generated modular Chams render-pipeline source: $OutputPath"
