// Generated into dllmain.cpp's translation unit by apply-chams-render-pipeline.ps1.
// Keep this file CRT-free and dependent only on types/helpers declared before
// ApplyModelPasses in dllmain.cpp.

struct ChamsPassPlan {
    bool visibleMaterial;
    bool occludedHighlight;
    bool glowHighlight;
};

struct ChamsRenderStats {
    unsigned int modelUpdates;
    unsigned int visiblePasses;
    unsigned int occludedPasses;
    unsigned int glowPasses;
    unsigned int restores;
    unsigned int failures;
};

static ChamsRenderStats g_chamsRenderStats{};

static ChamsPassPlan BuildChamsPassPlan(bool allowVisiblePass,
    bool allowOccludedPass)
{
    ChamsPassPlan plan{};
    if (!g_espConfig.enable)
        return plan;

    if (g_espConfig.chams)
    {
        switch (g_espConfig.chamsStyle)
        {
        case 0: // Visible + Occluded
            plan.visibleMaterial = allowVisiblePass;
            plan.occludedHighlight = allowOccludedPass;
            break;
        case 1: // Visible Only
            plan.visibleMaterial = allowVisiblePass;
            break;
        case 2: // Occluded Only
            plan.occludedHighlight = allowOccludedPass;
            break;
        default:
            break;
        }
    }

    // Glow is intentionally independent from Chams. The legacy style value 3
    // is retained as a compatibility alias for Glow Only configs.
    if ((g_espConfig.glow || (g_espConfig.chams && g_espConfig.chamsStyle == 3)) &&
        allowOccludedPass)
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
    ++g_chamsRenderStats.restores;
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
    ++g_chamsRenderStats.visiblePasses;
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
    ++g_chamsRenderStats.restores;
}

static void ApplyHighlightPass(const BotHighlightRuntime& runtime,
    BYTE* glowBase, const RGBVal& color, BYTE* eligible, BYTE* glowing,
    bool glowOnly)
{
    const BYTE rgba[4] = { color.r, color.g, color.b, 255 };
    runtime.setGlowColor(glowBase, PackRgba(rgba));
    *eligible = 1;
    runtime.setGlowType(glowBase, 3, 0.0f);
    *glowing = 1;
    if (glowOnly)
        ++g_chamsRenderStats.glowPasses;
    else
        ++g_chamsRenderStats.occludedPasses;
}

static bool ApplyModelPasses(const BotHighlightRuntime& runtime,
    unsigned int entityHandle, void* modelEntity, bool allowVisiblePass,
    bool allowOccludedPass)
{
    ++g_chamsRenderStats.modelUpdates;
    if (!RememberBotHighlight(runtime, entityHandle, modelEntity))
    {
        ++g_chamsRenderStats.failures;
        return false;
    }

    BYTE* glowColor = nullptr;
    BYTE* eligible = nullptr;
    BYTE* glowing = nullptr;
    BYTE* renderColor = nullptr;
    BYTE* clientTint = nullptr;
    BYTE* useClientTint = nullptr;
    if (!HighlightSlots(runtime, modelEntity, &glowColor, &eligible, &glowing,
        &renderColor, &clientTint, &useClientTint))
    {
        ++g_chamsRenderStats.failures;
        return false;
    }

    OriginalBotHighlight* original = FindOriginalBotHighlight(entityHandle);
    if (!original)
    {
        ++g_chamsRenderStats.failures;
        return false;
    }
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
        // The current backend still uses Source 2 screen highlight for the
        // occluded leg. Keeping this isolated behind the pass planner makes it
        // replaceable by a true Ignore-Z mesh pass without touching target
        // selection, lifecycle, or standalone Glow.
        ApplyHighlightPass(runtime, glowBase, g_espConfig.chamsColorOccluded,
            eligible, glowing, false);
    }
    else if (plan.glowHighlight)
    {
        ApplyHighlightPass(runtime, glowBase, g_espConfig.glowColor,
            eligible, glowing, true);
    }
    else
    {
        RestoreHighlightPass(runtime, glowBase, *original, glowColor,
            eligible, glowing);
    }

    return plan.visibleMaterial || plan.occludedHighlight ||
        plan.glowHighlight;
}
