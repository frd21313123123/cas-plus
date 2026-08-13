param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

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
    // Occluded pass uses Source 2's screen-highlight path so the same entity can
    // be represented through scene occluders without guessing a visibility byte.
    RGBVal chamsColorOccluded = { 239, 68, 68 };
    // 0: Visible + Occluded, 1: Visible Only, 2: Occluded Only, 3: Glow Only.
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

$newPipeline = @'
struct ChamsPassPlan {
    bool visibleMaterial;
    bool occludedHighlight;
    bool glowHighlight;
};

static ChamsPassPlan BuildChamsPassPlan(bool allowVisiblePass,
    bool allowOccludedPass)
{
    ChamsPassPlan plan{};
    if (!g_espConfig.enable)
        return plan;

    if (g_espConfig.chams)
    {
        if (g_espConfig.chamsStyle == 0) // Visible + Occluded
        {
            plan.visibleMaterial = allowVisiblePass;
            plan.occludedHighlight = allowOccludedPass;
        }
        else if (g_espConfig.chamsStyle == 1) // Visible Only
        {
            plan.visibleMaterial = allowVisiblePass;
        }
        else if (g_espConfig.chamsStyle == 2) // Occluded Only
        {
            plan.occludedHighlight = allowOccludedPass;
        }
        else if (g_espConfig.chamsStyle == 3) // Glow Only
        {
            plan.glowHighlight = allowOccludedPass;
        }
    }

    // The standalone ESP Glow toggle is independent from Chams. It can be
    // combined with the visible material pass, but never steals the occluded
    // Chams color when the dual-pass mode is active.
    if (g_espConfig.glow && allowOccludedPass)
        plan.glowHighlight = true;

    return plan;
}

static void RestoreVisibleMaterialPass(const BotHighlightRuntime& runtime,
    void* modelEntity, const OriginalBotHighlight& original,
    BYTE* renderColor, BYTE* clientTint, BYTE* useClientTint)
{
    CopyFourBytes(clientTint, original.clientTint);
    *useClientTint = original.useClientTint;
    CopyFourBytes(renderColor, original.renderColor);
    runtime.setRenderColor(modelEntity, original.renderColor[0],
        original.renderColor[1], original.renderColor[2]);
}

static void ApplyVisibleMaterialPass(const BotHighlightRuntime& runtime,
    void* modelEntity, BYTE* renderColor, BYTE* clientTint,
    BYTE* useClientTint)
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

static void RestoreHighlightPass(const BotHighlightRuntime& runtime,
    BYTE* glowBase, const OriginalBotHighlight& original, BYTE* glowColor,
    BYTE* eligible, BYTE* glowing)
{
    runtime.setGlowType(glowBase, original.glowType, 0.0f);
    CopyFourBytes(glowBase + runtime.glowTimeOffset, original.glowTime);
    CopyFourBytes(glowBase + runtime.glowStartTimeOffset,
        original.glowStartTime);
    runtime.setGlowColor(glowBase, PackRgba(original.glowColor));
    CopyFourBytes(glowColor, original.glowColor);
    *eligible = original.eligible;
    *glowing = original.glowing;
}

static void ApplyHighlightPass(const BotHighlightRuntime& runtime,
    BYTE* glowBase, const RGBVal& color, BYTE* eligible, BYTE* glowing)
{
    const BYTE rgba[4] = { color.r, color.g, color.b, 255 };
    runtime.setGlowColor(glowBase, PackRgba(rgba));
    *eligible = 1;
    // Source 2 type 3 is the screen-highlight path that survives scene
    // occlusion. It is deliberately separate from the depth-tested model tint.
    runtime.setGlowType(glowBase, 3, 0.0f);
    *glowing = 1;
}

static bool ApplyModelPasses(const BotHighlightRuntime& runtime,
    unsigned int entityHandle, void* modelEntity, bool allowVisiblePass,
    bool allowOccludedPass)
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

    const ChamsPassPlan plan = BuildChamsPassPlan(allowVisiblePass,
        allowOccludedPass);

    if (plan.visibleMaterial)
        ApplyVisibleMaterialPass(runtime, modelEntity, renderColor, clientTint,
            useClientTint);
    else
        RestoreVisibleMaterialPass(runtime, modelEntity, *original, renderColor,
            clientTint, useClientTint);

    BYTE* glowBase = reinterpret_cast<BYTE*>(modelEntity) + runtime.glowOffset;
    if (plan.occludedHighlight)
    {
        // Chams owns the occluded pass color. This is intentionally evaluated
        // before Glow so toggling Glow cannot cause red/green color flicker.
        ApplyHighlightPass(runtime, glowBase, g_espConfig.chamsColorOccluded,
            eligible, glowing);
    }
    else if (plan.glowHighlight)
    {
        ApplyHighlightPass(runtime, glowBase, g_espConfig.glowColor,
            eligible, glowing);
    }
    else
    {
        RestoreHighlightPass(runtime, glowBase, *original, glowColor,
            eligible, glowing);
    }

    return plan.visibleMaterial || plan.occludedHighlight ||
        plan.glowHighlight;
}

'@

$source = $source.Substring(0, $start) + $newPipeline + $source.Substring($end)

$source = $source.Replace(
    'const wchar_t* styles[] = { L"Dual Pass", L"Visible Only", L"Through Wall", L"Glow Pass" };',
    'const wchar_t* styles[] = { L"Visible + Occluded", L"Visible Only", L"Occluded Only", L"Glow Only" };')

$source = $source.Replace(
    'L"Chams: model + through-wall passes active"',
    'L"Chams: visible material + occluded highlight active"')

$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}

Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Generated Chams render-pipeline source: $OutputPath"
