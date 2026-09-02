param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$inputFullPath = [IO.Path]::GetFullPath($InputPath)
$outputFullPath = [IO.Path]::GetFullPath($OutputPath)
if ($inputFullPath.Equals($outputFullPath, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The generated payload must not overwrite its source file.'
}
if (-not (Test-Path -LiteralPath $inputFullPath -PathType Leaf)) {
    throw "Payload source was not found: $inputFullPath"
}

$outputDirectory = Split-Path -Parent $outputFullPath
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$stagingPath = $outputFullPath + '.staging.cpp'

# This is the one ordered build manifest. Every stage transforms only the
# build-local translation unit. Old compatibility normalizers that rewrote
# other patcher scripts are deliberately not part of a build.
$stages = @(
    @{ Name = 'normalize-inventory-injection-points.ps1'; InPlace = $true },
    @{ Name = 'apply-inventory-changer.ps1'; InPlace = $false },
    @{ Name = 'apply-inventory-catalog.ps1'; InPlace = $false },
    @{ Name = 'apply-inventory-operations.ps1'; InPlace = $false },
    @{ Name = 'apply-inventory-visual-refresh.ps1'; InPlace = $false },
    @{ Name = 'apply-inventory-gloves.ps1'; InPlace = $false },
    @{ Name = 'apply-inventory-weapon-factory.ps1'; InPlace = $false },
    @{ Name = 'apply-inventory-music.ps1'; InPlace = $false },
    @{ Name = 'apply-inventory-full.ps1'; InPlace = $false },
    @{ Name = 'normalize-inventory-no-seh.ps1'; InPlace = $true },
    @{ Name = 'apply-inventory-local-ops.ps1'; InPlace = $false },
    @{ Name = 'normalize-inventory-econ-lifecycle.ps1'; InPlace = $true },
    @{ Name = 'apply-inventory-runtime-diagnostics.ps1'; InPlace = $true },
    @{ Name = 'fix-inventory-econ-native.ps1'; InPlace = $true },
    @{ Name = 'fix-inventory-econ-view-fallback.ps1'; InPlace = $true },
    @{ Name = 'apply-ui-redesign.ps1'; InPlace = $true },
    @{ Name = 'apply-ui-sticker-v2.ps1'; InPlace = $true },
    @{ Name = 'apply-ui-interaction-visuals-v2.ps1'; InPlace = $true },
    @{ Name = 'apply-ui-inventory-tooltips-v3.ps1'; InPlace = $true },
    @{ Name = 'apply-inventory-game-catalog.ps1'; InPlace = $true },
    @{ Name = 'normalize-inventory-game-catalog-order.ps1'; InPlace = $true },
    @{ Name = 'fix-inventory-game-catalog-runtime.ps1'; InPlace = $true },
    @{ Name = 'apply-inventory-attachments-v2.ps1'; InPlace = $true },
    @{ Name = 'apply-inventory-runtime-stability.ps1'; InPlace = $true },
    @{ Name = 'apply-ui-inventory-selector-v3.ps1'; InPlace = $true },
    @{ Name = 'apply-inventory-worker-service.ps1'; InPlace = $true }
)

foreach ($stage in $stages) {
    $stagePath = Join-Path $PSScriptRoot $stage.Name
    if (-not (Test-Path -LiteralPath $stagePath -PathType Leaf)) {
        throw "Required payload stage was not found: $($stage.Name)"
    }
}

$rendererPath = Join-Path $PSScriptRoot 'apply-chams-render-pipeline.ps1'
& $rendererPath -InputPath $inputFullPath -OutputPath $stagingPath
foreach ($stage in $stages) {
    $stagePath = Join-Path $PSScriptRoot $stage.Name
    Write-Host "Payload stage: $($stage.Name)"
    try {
        if ($stage.InPlace) {
            & $stagePath -InputPath $stagingPath
        } else {
            & $stagePath -InputPath $stagingPath -OutputPath $stagingPath
        }
    } catch {
        throw "Payload stage '$($stage.Name)' failed: $($_.Exception.Message)"
    }
}

# Publish only a fully prepared translation unit. A failed stage leaves its
# diagnostic intermediate, not a half-patched source for the compiler.
Move-Item -LiteralPath $stagingPath -Destination $outputFullPath -Force
Write-Host "Prepared inventory payload: $outputFullPath"
