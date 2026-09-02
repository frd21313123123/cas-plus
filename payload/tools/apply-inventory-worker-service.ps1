param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$anchor = '        FlushInventoryPersistenceIfNeeded();'
$count = ([regex]::Matches($source, [regex]::Escape($anchor))).Count
if ($count -ne 1) {
    throw "Inventory worker service anchor expected once, found $count."
}
$replacement = @'
        ProcessInventoryGameCatalogLoadRequests();
        FlushInventoryPersistenceIfNeeded();
'@
$source = $source.Replace($anchor, $replacement.TrimEnd())
Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host 'Connected deferred catalog loading to the payload worker.'
