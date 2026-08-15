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
        throw "UI-scope anchor '$Name' was not found. Refusing to patch blindly."
    }
    $script:source = $script:source.Replace($Old, $New)
}

# The original single-page renderer declared rowFont inside the ESP body and
# used it later for the flags modal. Once the ESP body became a routed branch,
# that declaration became branch-local. Keep the handle in the WM_PAINT scope.
Replace-Required @'
        if (g_espConfig.selectedTab == 4)
        {
            DrawConfigPage(memDC);
'@ @'
        HFONT rowFont = nullptr;

        if (g_espConfig.selectedTab == 4)
        {
            DrawConfigPage(memDC);
'@ 'paint-scope rowFont declaration'

Replace-Required @'
        HFONT rowFont = CreateFontW(14, 0, 0, 0, 400, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
'@ @'
        rowFont = CreateFontW(14, 0, 0, 0, 400, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
'@ 'visual rowFont assignment'

# Switching away from Visuals closes visual-only modal state, ensuring a null
# rowFont is never selected by a stale modal on another page.
Replace-Required @'
                    g_espConfig.selectedTab = i;
                    InvalidateRect(wnd, nullptr, FALSE);
'@ @'
                    g_espConfig.selectedTab = i;
                    if (i != 2)
                    {
                        g_espConfig.showManageModal = false;
                        g_espConfig.showRgbPickerModal = false;
                        g_espConfig.showChamsModal = false;
                    }
                    InvalidateRect(wnd, nullptr, FALSE);
'@ 'close visual modals on tab switch'

$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}
Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Generated UI-scope-fixed source: $OutputPath"
