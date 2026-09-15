param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $InputPath)) {
    throw "Velocity stage bridge input does not exist: $InputPath"
}

$content = Get-Content -LiteralPath $InputPath -Raw
if ($content.Contains('CasVelocityDispatchFrameStagePre(stage);')) {
    Write-Host "Velocity frame-stage dispatch already present: $InputPath"
    exit 0
}

$declAnchor = 'static FrameStageNotifyFn g_originalFrameStageNotify = nullptr;'
if (-not $content.Contains($declAnchor)) {
    throw 'Velocity stage bridge declaration anchor not found.'
}

$declarations = @'
static FrameStageNotifyFn g_originalFrameStageNotify = nullptr;
extern "C" void CasVelocityDispatchFrameStagePre(int stage);
extern "C" void CasVelocityDispatchFrameStagePost(int stage);
'@
$content = $content.Replace($declAnchor, $declarations.TrimEnd())

$callAnchor = '    g_originalFrameStageNotify(client, stage);'
if (-not $content.Contains($callAnchor)) {
    throw 'Velocity stage bridge call anchor not found.'
}

$dispatch = @'
    CasVelocityDispatchFrameStagePre(stage);
    g_originalFrameStageNotify(client, stage);
    CasVelocityDispatchFrameStagePost(stage);
'@
$content = $content.Replace($callAnchor, $dispatch.TrimEnd())

Set-Content -LiteralPath $InputPath -Value $content -Encoding UTF8
Write-Host "Injected Velocity frame-stage dispatch: $InputPath"
