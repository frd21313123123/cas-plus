// Generated into dllmain.cpp's translation unit by apply-chams-render-pipeline.ps1.
// FrameStageNotify owns compatibility entity state; the scene-system hook owns
// per-draw mesh state when its guarded backend is available.

struct ChamsPassPlan {
    bool visibleMaterial;
    bool occludedHighlight;
    bool glowHighlight;
};

struct ChamsRenderStats {
    unsigned int modelUpdates;
    unsigned int visibleFallbackPasses;
    unsigned int occludedFallbackPasses;
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

    // Glow is independent from Chams. Style 3 remains a compatibility alias
    // for configs that historically encoded Glow Only in the Chams selector.
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

static void ApplyVisibleFallbackPass(const BotHighlightRuntime& runtime,
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
    ++g_chamsRenderStats.visibleFallbackPasses;
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
        ++g_chamsRenderStats.occludedFallbackPasses;
}

static bool ApplyModelPasses(const BotHighlightRuntime& runtime,
    unsigned int entityHandle, void* modelEntity, bool allowVisiblePass,
    bool allowOccludedPass)
{
    ++g_chamsRenderStats.modelUpdates;

    // The legacy controller loop publishes the enemy pawn first. Expand that
    // typed entry here while the same Schema-backed pawn/runtime are available,
    // so its active weapon joins the next immutable render snapshot without
    // adding weapon discovery logic to the DrawObject hot path.
    if (VisualTargetKindBeingBuilt(entityHandle) == VISUAL_TARGET_ENEMY_PAWN)
        AddEnemyPawnVisualTargets(runtime, entityHandle, modelEntity);

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
    // During this FrameStage update the new snapshot is not published yet, so
    // use the private builder for ownership decisions. DrawObject itself only
    // reads the previously published immutable snapshot.
    const bool enemyMeshTarget = IsEnemyVisualTargetBeingBuilt(entityHandle);

    const bool meshOwnsVisiblePass = plan.visibleMaterial &&
        g_meshRenderBackend.installed && enemyMeshTarget;
    if (meshOwnsVisiblePass)
    {
        RestoreVisibleMaterialPass(runtime, modelEntity, *original, renderColor,
            clientTint, useClientTint);
    }
    else if (plan.visibleMaterial)
    {
        ApplyVisibleFallbackPass(runtime, modelEntity, renderColor, clientTint,
            useClientTint);
    }
    else
    {
        RestoreVisibleMaterialPass(runtime, modelEntity, *original, renderColor,
            clientTint, useClientTint);
    }

    BYTE* glowBase = reinterpret_cast<BYTE*>(modelEntity) + runtime.glowOffset;
    const bool materialOwnsOccludedPass = plan.occludedHighlight &&
        g_meshRenderBackend.installed && g_materialManager.materialsReady &&
        enemyMeshTarget;

    if (materialOwnsOccludedPass)
    {
        // True Ignore-Z mesh Chams owns the wall pass. Standalone Glow remains
        // independent and may be layered on top only when explicitly enabled.
        if (plan.glowHighlight)
        {
            ApplyHighlightPass(runtime, glowBase, g_espConfig.glowColor,
                eligible, glowing, true);
        }
        else
        {
            RestoreHighlightPass(runtime, glowBase, *original, glowColor,
                eligible, glowing);
        }
    }
    else if (plan.occludedHighlight)
    {
        // Compatibility fallback for builds where the scene/material backend
        // cannot be resolved safely.
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

    return meshOwnsVisiblePass || materialOwnsOccludedPass ||
        plan.visibleMaterial || plan.occludedHighlight || plan.glowHighlight;
}
