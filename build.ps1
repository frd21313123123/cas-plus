param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$artifactsRoot = Join-Path $projectRoot 'artifacts'
$injectorOutput = Join-Path $artifactsRoot 'injector'
$payloadOutput = Join-Path $artifactsRoot 'payload'
$packageRoot = Join-Path $artifactsRoot 'package'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'vswhere.exe was not found. Install Visual Studio with Desktop development with C++.'
}

$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
if (-not $msbuild) {
    throw 'MSBuild was not found.'
}

if (Test-Path -LiteralPath $artifactsRoot) {
    $resolvedProject = [IO.Path]::GetFullPath($projectRoot)
    $resolvedArtifacts = [IO.Path]::GetFullPath($artifactsRoot)
    if (-not $resolvedArtifacts.StartsWith($resolvedProject + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Refusing to clean an artifacts path outside the project.'
    }
    Remove-Item -LiteralPath $resolvedArtifacts -Recurse -Force
}

New-Item -ItemType Directory -Path $injectorOutput, $payloadOutput, (Join-Path $packageRoot 'dlls') -Force | Out-Null

$commonArgs = @(
    '/m',
    '/nr:false',
    '/t:Rebuild',
    "/p:Configuration=$Configuration",
    '/p:Platform=x64',
    '/v:minimal'
)

& $msbuild (Join-Path $projectRoot 'injector\potatoInjector.vcxproj') @commonArgs "/p:OutDir=$injectorOutput\"
if ($LASTEXITCODE -ne 0) { throw "Injector build failed with exit code $LASTEXITCODE." }

& $msbuild (Join-Path $projectRoot 'payload\PotatoPayload.vcxproj') @commonArgs "/p:OutDir=$payloadOutput\"
if ($LASTEXITCODE -ne 0) { throw "Payload build failed with exit code $LASTEXITCODE." }

$injectorName = if ($Configuration -eq 'Release') { 'cas-plus.exe' } else { 'cas-plus-debug.exe' }
$payloadName = if ($Configuration -eq 'Release') { 'cas-plus-payload.dll' } else { 'cas-plus-payload-debug.dll' }

Copy-Item -LiteralPath (Join-Path $injectorOutput $injectorName) -Destination (Join-Path $packageRoot 'cas-plus.exe')
Copy-Item -LiteralPath (Join-Path $payloadOutput $payloadName) -Destination (Join-Path $packageRoot 'dlls\cas-plus-payload.dll')
if (Test-Path (Join-Path $projectRoot 'README.md')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot 'README.md') -Destination $packageRoot
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'THIRD_PARTY_NOTICES.md') -Destination $packageRoot

Write-Host "Package ready: $packageRoot"

