param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

function Replace-Required([string]$Old, [string]$New, [string]$Name) {
    if (-not $script:source.Contains($Old)) {
        throw "Aim-assist simulator anchor '$Name' was not found. Refusing to patch blindly."
    }
    $script:source = $script:source.Replace($Old, $New)
}

# This module is intentionally a preview-only trainer. It never writes view
# angles, input state, fire commands, network state, or live-player targeting.
Replace-Required @'
    BYTE diagnosticsPanel;
    RGBVal accentColor;
};
'@ @'
    BYTE diagnosticsPanel;
    RGBVal accentColor;
    BYTE aimAssistSimulator;
    int aimAssistSmoothing;
};
'@ 'persisted aim assist fields'

Replace-Required @'
    RGBVal accentColor = { 56, 189, 248 };
    int targetHits = 0;
'@ @'
    RGBVal accentColor = { 56, 189, 248 };
    bool aimAssistSimulator = false;
    int aimAssistSmoothing = 6;
    int assistX = 555;
    int assistY = 285;
    int targetHits = 0;
'@ 'aim assist runtime state'

Replace-Required @'
    out.diagnosticsPanel = g_safeFeatures.diagnosticsPanel ? 1 : 0;
    out.accentColor = g_safeFeatures.accentColor;
    return out;
'@ @'
    out.diagnosticsPanel = g_safeFeatures.diagnosticsPanel ? 1 : 0;
    out.accentColor = g_safeFeatures.accentColor;
    out.aimAssistSimulator = g_safeFeatures.aimAssistSimulator ? 1 : 0;
    out.aimAssistSmoothing = g_safeFeatures.aimAssistSmoothing;
    return out;
'@ 'save aim assist settings'

Replace-Required @'
    g_safeFeatures.diagnosticsPanel = in.diagnosticsPanel != 0;
    g_safeFeatures.accentColor = in.accentColor;
}
'@ @'
    g_safeFeatures.diagnosticsPanel = in.diagnosticsPanel != 0;
    g_safeFeatures.accentColor = in.accentColor;
    g_safeFeatures.aimAssistSimulator = in.aimAssistSimulator != 0;
    g_safeFeatures.aimAssistSmoothing = in.aimAssistSmoothing < 2 ? 2 :
        (in.aimAssistSmoothing > 14 ? 14 : in.aimAssistSmoothing);
    g_safeFeatures.assistX = 555;
    g_safeFeatures.assistY = 285;
}
'@ 'load aim assist settings'

Replace-Required @'
    DrawTrainingToggleRow(hdc, 225, L"Target trainer", g_safeFeatures.targetTrainer);

    DrawRoundedCard(hdc, 45, 285, 300, 42, RGB_COLOR(32, 32, 35), RGB_COLOR(63, 63, 70), 6);
'@ @'
    DrawTrainingToggleRow(hdc, 225, L"Target trainer", g_safeFeatures.targetTrainer);
    DrawTrainingToggleRow(hdc, 265, L"Aim assist simulator", g_safeFeatures.aimAssistSimulator);

    DrawRoundedCard(hdc, 45, 315, 145, 42, RGB_COLOR(32, 32, 35), RGB_COLOR(63, 63, 70), 6);
    SetTextColor(hdc, RGB_COLOR(228, 228, 231));
    RECT smoothRc = { 45, 315, 190, 357 };
    const wchar_t* smoothText = g_safeFeatures.aimAssistSmoothing <= 4 ?
        L"Smoothing: Fast" : (g_safeFeatures.aimAssistSmoothing <= 8 ?
        L"Smoothing: Medium" : L"Smoothing: Slow");
    DrawTextW(hdc, smoothText, -1, &smoothRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    DrawRoundedCard(hdc, 200, 315, 145, 42, RGB_COLOR(32, 32, 35), RGB_COLOR(63, 63, 70), 6);
'@ 'aim assist controls'

Replace-Required @'
    RECT radiusRc = { 45, 285, 345, 327 };
'@ @'
    RECT radiusRc = { 200, 315, 345, 357 };
'@ 'move radius label'

Replace-Required @'
    DrawTextW(hdc, radiusText, -1, &radiusRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    DrawRoundedCard(hdc, 390, 130, 330, 310, RGB_COLOR(18, 18, 20), RGB_COLOR(45, 45, 50), 8);
'@ @'
    DrawTextW(hdc, radiusText, -1, &radiusRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    DrawRoundedCard(hdc, 390, 130, 330, 310, RGB_COLOR(18, 18, 20), RGB_COLOR(45, 45, 50), 8);
'@ 'radius card preservation'

# Draw and advance a separate simulated aim cursor. Integer interpolation keeps
# the no-CRT payload simple and deterministic.
Replace-Required @'
    if (g_safeFeatures.targetTrainer)
    {
        HPEN targetPen = CreatePen(PS_SOLID, 2, RGB_COLOR(239, 68, 68));
'@ @'
    if (g_safeFeatures.aimAssistSimulator && g_safeFeatures.targetTrainer)
    {
        const int smoothing = g_safeFeatures.aimAssistSmoothing < 2 ? 2 : g_safeFeatures.aimAssistSmoothing;
        const int dx = g_safeFeatures.targetX - g_safeFeatures.assistX;
        const int dy = g_safeFeatures.targetY - g_safeFeatures.assistY;
        if (dx != 0)
        {
            int stepX = dx / smoothing;
            if (stepX == 0) stepX = dx > 0 ? 1 : -1;
            g_safeFeatures.assistX += stepX;
        }
        if (dy != 0)
        {
            int stepY = dy / smoothing;
            if (stepY == 0) stepY = dy > 0 ? 1 : -1;
            g_safeFeatures.assistY += stepY;
        }

        HPEN assistPen = CreatePen(PS_SOLID, 2, g_safeFeatures.accentColor.ToRef());
        HGDIOBJ oldAssistPen = SelectObject(hdc, assistPen);
        MoveToEx(hdc, g_safeFeatures.assistX - 9, g_safeFeatures.assistY, nullptr);
        LineTo(hdc, g_safeFeatures.assistX + 10, g_safeFeatures.assistY);
        MoveToEx(hdc, g_safeFeatures.assistX, g_safeFeatures.assistY - 9, nullptr);
        LineTo(hdc, g_safeFeatures.assistX, g_safeFeatures.assistY + 10);
        SelectObject(hdc, oldAssistPen);
        DeleteObject(assistPen);
    }

    if (g_safeFeatures.targetTrainer)
    {
        HPEN targetPen = CreatePen(PS_SOLID, 2, RGB_COLOR(239, 68, 68));
'@ 'simulated aim cursor'

Replace-Required @'
    DrawTextW(hdc, L"Click the red target to move it", -1, &hint, DT_CENTER | DT_SINGLELINE);
'@ @'
    DrawTextW(hdc, g_safeFeatures.aimAssistSimulator ?
        L"Blue cursor demonstrates smoothing toward the target" :
        L"Click the red target to move it", -1, &hint, DT_CENTER | DT_SINGLELINE);
'@ 'aim assist hint'

Replace-Required @'
                else if (mouseY >= 220 && mouseY <= 255) g_safeFeatures.targetTrainer = !g_safeFeatures.targetTrainer;
                else goto aim_page_no_toggle;
'@ @'
                else if (mouseY >= 220 && mouseY <= 255) g_safeFeatures.targetTrainer = !g_safeFeatures.targetTrainer;
                else if (mouseY >= 260 && mouseY <= 295)
                {
                    g_safeFeatures.aimAssistSimulator = !g_safeFeatures.aimAssistSimulator;
                    g_safeFeatures.assistX = 555;
                    g_safeFeatures.assistY = 285;
                }
                else goto aim_page_no_toggle;
'@ 'aim assist toggle click'

Replace-Required @'
            if (mouseX >= 45 && mouseX <= 345 && mouseY >= 285 && mouseY <= 327)
            {
                if (g_safeFeatures.aimGuideRadius <= 60) g_safeFeatures.aimGuideRadius = 90;
                else if (g_safeFeatures.aimGuideRadius <= 100) g_safeFeatures.aimGuideRadius = 120;
                else g_safeFeatures.aimGuideRadius = 60;
                InvalidateRect(wnd, nullptr, FALSE);
                return 0;
            }
'@ @'
            if (mouseX >= 45 && mouseX <= 190 && mouseY >= 315 && mouseY <= 357)
            {
                if (g_safeFeatures.aimAssistSmoothing <= 4) g_safeFeatures.aimAssistSmoothing = 6;
                else if (g_safeFeatures.aimAssistSmoothing <= 8) g_safeFeatures.aimAssistSmoothing = 12;
                else g_safeFeatures.aimAssistSmoothing = 3;
                InvalidateRect(wnd, nullptr, FALSE);
                return 0;
            }
            if (mouseX >= 200 && mouseX <= 345 && mouseY >= 315 && mouseY <= 357)
            {
                if (g_safeFeatures.aimGuideRadius <= 60) g_safeFeatures.aimGuideRadius = 90;
                else if (g_safeFeatures.aimGuideRadius <= 100) g_safeFeatures.aimGuideRadius = 120;
                else g_safeFeatures.aimGuideRadius = 60;
                InvalidateRect(wnd, nullptr, FALSE);
                return 0;
            }
'@ 'smoothing and radius buttons'

Replace-Required @'
                    g_safeFeatures.targetX = 420 + ((g_safeFeatures.targetHits * 73) % 270);
                    g_safeFeatures.targetY = 175 + ((g_safeFeatures.targetHits * 47) % 215);
'@ @'
                    g_safeFeatures.targetX = 420 + ((g_safeFeatures.targetHits * 73) % 270);
                    g_safeFeatures.targetY = 175 + ((g_safeFeatures.targetHits * 47) % 215);
                    if (!g_safeFeatures.aimAssistSimulator)
                    {
                        g_safeFeatures.assistX = 555;
                        g_safeFeatures.assistY = 285;
                    }
'@ 'target relocation sync'

Replace-Required @'
                g_safeFeatures.targetX = 565;
                g_safeFeatures.targetY = 255;
'@ @'
                g_safeFeatures.targetX = 565;
                g_safeFeatures.targetY = 255;
                g_safeFeatures.assistX = 555;
                g_safeFeatures.assistY = 285;
'@ 'reset assist cursor'

$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}
Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Generated Aim Lab aim-assist simulator source: $OutputPath"
