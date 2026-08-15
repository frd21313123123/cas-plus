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
        throw "Runtime-sync anchor '$Name' was not found. Refusing to patch blindly."
    }
    $script:source = $script:source.Replace($Old, $New)
}

Replace-Required @'
    g_espConfig.flagPin = in.flags[7] != 0;

    const bool modelEffects = g_espConfig.enable &&
        (g_espConfig.chams || g_espConfig.glow);
    g_botHighlightEnabled = modelEffects;
    QueueBotHighlight(modelEffects);
}
'@ @'
    g_espConfig.flagPin = in.flags[7] != 0;
}
'@ 'load-time premature runtime sync'

Replace-Required @'
    g_espConfig = ESPConfig{};
    g_espConfig.selectedTab = tab;
    const bool modelEffects = g_espConfig.enable &&
        (g_espConfig.chams || g_espConfig.glow);
    g_botHighlightEnabled = modelEffects;
    QueueBotHighlight(modelEffects);
}
'@ @'
    g_espConfig = ESPConfig{};
    g_espConfig.selectedTab = tab;
}
'@ 'reset-time premature runtime sync'

Replace-Required @'
                if (mouseY >= 265 && mouseY <= 307)
                {
                    g_configStatus = LoadCasProfile() ? L"Profile loaded" : L"Load failed or profile is incompatible";
                    InvalidateRect(wnd, nullptr, FALSE);
                    return 0;
                }
'@ @'
                if (mouseY >= 265 && mouseY <= 307)
                {
                    const bool loaded = LoadCasProfile();
                    g_configStatus = loaded ? L"Profile loaded" : L"Load failed or profile is incompatible";
                    if (loaded)
                    {
                        const bool modelEffects = g_espConfig.enable &&
                            (g_espConfig.chams || g_espConfig.glow);
                        g_botHighlightEnabled = modelEffects;
                        QueueBotHighlight(modelEffects);
                    }
                    InvalidateRect(wnd, nullptr, FALSE);
                    return 0;
                }
'@ 'load runtime sync'

Replace-Required @'
                if (mouseY >= 325 && mouseY <= 367)
                {
                    ResetCasProfile();
                    g_configStatus = L"Defaults restored";
                    InvalidateRect(wnd, nullptr, FALSE);
                    return 0;
                }
'@ @'
                if (mouseY >= 325 && mouseY <= 367)
                {
                    ResetCasProfile();
                    const bool modelEffects = g_espConfig.enable &&
                        (g_espConfig.chams || g_espConfig.glow);
                    g_botHighlightEnabled = modelEffects;
                    QueueBotHighlight(modelEffects);
                    g_configStatus = L"Defaults restored";
                    InvalidateRect(wnd, nullptr, FALSE);
                    return 0;
                }
'@ 'reset runtime sync'

$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}
Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Generated runtime-synchronized source: $OutputPath"
