param(
    [Parameter(Mandatory = $true)]
    [string]$ItemsGamePath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $ItemsGamePath)) {
    throw "items_game.txt was not found: $ItemsGamePath"
}

$lines = [System.IO.File]::ReadAllLines((Resolve-Path -LiteralPath $ItemsGamePath))

function Find-SectionRanges {
    param([string]$Name)

    $result = New-Object System.Collections.Generic.List[object]
    $quoted = '"' + $Name + '"'
    for ($i = 0; $i -lt $lines.Length; ++$i) {
        if ($lines[$i].Trim() -ne $quoted) { continue }

        $open = $i + 1
        while ($open -lt $lines.Length -and $lines[$open].Trim() -eq '') { ++$open }
        if ($open -ge $lines.Length -or $lines[$open].Trim() -ne '{') { continue }

        $depth = 1
        $close = -1
        for ($j = $open + 1; $j -lt $lines.Length; ++$j) {
            $trim = $lines[$j].Trim()
            if ($trim -eq '{') { ++$depth }
            elseif ($trim -eq '}') {
                --$depth
                if ($depth -eq 0) {
                    $close = $j
                    break
                }
            }
        }
        if ($close -lt 0) { throw "Section '$Name' was malformed." }
        $result.Add([pscustomobject]@{ Start = $open + 1; End = $close - 1 })
        $i = $close
    }
    return $result
}

function Find-SectionRange {
    param([string]$Name)
    $ranges = Find-SectionRanges $Name
    if ($ranges.Count -eq 0) {
        throw "Section '$Name' was not found or was malformed."
    }
    return $ranges[0]
}

function Get-NumericBlocks {
    param($Range)

    $result = New-Object System.Collections.Generic.List[object]
    $i = $Range.Start
    while ($i -le $Range.End) {
        $trim = $lines[$i].Trim()
        $match = [regex]::Match($trim, '^"([0-9]+)"$')
        if (-not $match.Success) { ++$i; continue }

        $id = [int]$match.Groups[1].Value
        $open = $i + 1
        while ($open -le $Range.End -and $lines[$open].Trim() -eq '') { ++$open }
        if ($open -gt $Range.End -or $lines[$open].Trim() -ne '{') { ++$i; continue }

        $depth = 1
        $close = $open
        for ($j = $open + 1; $j -le $Range.End; ++$j) {
            $t = $lines[$j].Trim()
            if ($t -eq '{') { ++$depth }
            elseif ($t -eq '}') {
                --$depth
                if ($depth -eq 0) { $close = $j; break }
            }
        }
        if ($depth -ne 0) { throw "Unterminated block for numeric key $id." }

        $result.Add([pscustomobject]@{ Id = $id; Start = $open + 1; End = $close - 1 })
        $i = $close + 1
    }
    return $result
}

function Read-ScalarFromBlock {
    param($Block, [string]$Key)

    $pattern = '^\s*"' + [regex]::Escape($Key) + '"\s+"([^"]*)"'
    for ($i = $Block.Start; $i -le $Block.End; ++$i) {
        $match = [regex]::Match($lines[$i], $pattern)
        if ($match.Success) { return $match.Groups[1].Value }
    }
    return $null
}

function Escape-CppWide {
    param([string]$Value)
    if ($null -eq $Value) { return '' }
    return $Value.Replace('\', '\\').Replace('"', '\"')
}

$itemsRange = Find-SectionRange 'items'
$paintRange = Find-SectionRange 'paint_kits'
$iconsRange = Find-SectionRange 'alternate_icons2'
$musicRanges = Find-SectionRanges 'music_definitions'

$weapons = New-Object System.Collections.Generic.List[object]
foreach ($block in (Get-NumericBlocks $itemsRange)) {
    if ($block.Id -lt 1 -or $block.Id -gt 4095) { continue }
    $name = Read-ScalarFromBlock $block 'name'
    if ([string]::IsNullOrWhiteSpace($name) -or -not $name.StartsWith('weapon_')) { continue }
    $itemName = Read-ScalarFromBlock $block 'item_name'
    $weapons.Add([pscustomobject]@{
        Definition = $block.Id
        Internal = $name
        Token = $itemName
    })
}

$paintByInternal = @{}
foreach ($block in (Get-NumericBlocks $paintRange)) {
    if ($block.Id -lt 0) { continue }
    $name = Read-ScalarFromBlock $block 'name'
    if ([string]::IsNullOrWhiteSpace($name)) { continue }
    $description = Read-ScalarFromBlock $block 'description_tag'
    $paintByInternal[$name] = [pscustomobject]@{
        PaintKit = $block.Id
        Internal = $name
        Token = $description
    }
}

# Some current items_game snapshots contain more than one music_definitions
# section. Merge them by numeric music-kit ID instead of assuming one block.
$musicById = @{}
foreach ($musicRange in $musicRanges) {
    foreach ($block in (Get-NumericBlocks $musicRange)) {
        if ($block.Id -lt 1 -or $block.Id -gt 65535) { continue }
        $name = Read-ScalarFromBlock $block 'name'
        if ([string]::IsNullOrWhiteSpace($name)) { continue }
        $locName = Read-ScalarFromBlock $block 'loc_name'
        $musicById[$block.Id] = [pscustomobject]@{
            MusicKit = $block.Id
            Internal = $name
            Token = $locName
        }
    }
}

# alternate_icons2 contains generated icon paths of the form
# econ/default_generated/<weapon internal>_<paint internal>_light.
# Those paths let us derive weapon/paint compatibility without guessing offsets
# or relying on a third-party skin list.
$weaponOrder = $weapons | Sort-Object { $_.Internal.Length } -Descending
$compatibility = @{}
$iconPattern = '"icon_path"\s+"econ/default_generated/([^"]+)"'
for ($i = $iconsRange.Start; $i -le $iconsRange.End; ++$i) {
    $match = [regex]::Match($lines[$i], $iconPattern)
    if (-not $match.Success) { continue }
    $leaf = $match.Groups[1].Value
    $leaf = [regex]::Replace($leaf, '_(light|medium|large)$', '')

    foreach ($weapon in $weaponOrder) {
        $prefix = $weapon.Internal + '_'
        if (-not $leaf.StartsWith($prefix, [System.StringComparison]::Ordinal)) { continue }
        $paintInternal = $leaf.Substring($prefix.Length)
        if ($paintByInternal.ContainsKey($paintInternal)) {
            $paint = $paintByInternal[$paintInternal]
            $key = "{0}:{1}" -f $weapon.Definition, $paint.PaintKit
            if (-not $compatibility.ContainsKey($key)) {
                $compatibility[$key] = [pscustomobject]@{
                    Definition = $weapon.Definition
                    PaintKit = $paint.PaintKit
                    Internal = $paint.Internal
                    Token = $paint.Token
                }
            }
        }
        break
    }
}

$out = New-Object System.Text.StringBuilder
[void]$out.AppendLine('// Generated from the supplied CS2 scripts/items/items_game.txt.')
[void]$out.AppendLine('// Do not edit by hand; regenerate with payload/tools/generate-inventory-catalog.ps1.')
[void]$out.AppendLine('')
[void]$out.AppendLine('struct InventoryGeneratedWeaponEntry {')
[void]$out.AppendLine('    unsigned short definitionIndex;')
[void]$out.AppendLine('    const char* internalName;')
[void]$out.AppendLine('    const wchar_t* localizationToken;')
[void]$out.AppendLine('};')
[void]$out.AppendLine('')
[void]$out.AppendLine('static const InventoryGeneratedWeaponEntry kGeneratedInventoryWeapons[] = {')
foreach ($weapon in ($weapons | Sort-Object Definition)) {
    $internal = $weapon.Internal.Replace('\', '\\').Replace('"', '\"')
    $token = Escape-CppWide $weapon.Token
    [void]$out.AppendLine(('    {{ {0}, "{1}", L"{2}" }},' -f $weapon.Definition, $internal, $token))
}
[void]$out.AppendLine('};')
[void]$out.AppendLine('')
[void]$out.AppendLine('struct InventoryGeneratedPaintCompatibility {')
[void]$out.AppendLine('    unsigned short definitionIndex;')
[void]$out.AppendLine('    int paintKit;')
[void]$out.AppendLine('    const wchar_t* internalName;')
[void]$out.AppendLine('    const wchar_t* localizationToken;')
[void]$out.AppendLine('};')
[void]$out.AppendLine('')
[void]$out.AppendLine('static const InventoryGeneratedPaintCompatibility kGeneratedInventoryPaints[] = {')
foreach ($entry in ($compatibility.Values | Sort-Object Definition, PaintKit)) {
    $internal = Escape-CppWide $entry.Internal
    $token = Escape-CppWide $entry.Token
    [void]$out.AppendLine(('    {{ {0}, {1}, L"{2}", L"{3}" }},' -f $entry.Definition, $entry.PaintKit, $internal, $token))
}
[void]$out.AppendLine('};')
[void]$out.AppendLine('')
[void]$out.AppendLine('struct InventoryGeneratedMusicEntry {')
[void]$out.AppendLine('    unsigned short musicKitId;')
[void]$out.AppendLine('    const wchar_t* internalName;')
[void]$out.AppendLine('    const wchar_t* localizationToken;')
[void]$out.AppendLine('};')
[void]$out.AppendLine('')
[void]$out.AppendLine('static const InventoryGeneratedMusicEntry kGeneratedInventoryMusic[] = {')
foreach ($entry in ($musicById.Values | Sort-Object MusicKit)) {
    $internal = Escape-CppWide $entry.Internal
    $token = Escape-CppWide $entry.Token
    [void]$out.AppendLine(('    {{ {0}, L"{1}", L"{2}" }},' -f $entry.MusicKit, $internal, $token))
}
[void]$out.AppendLine('};')
[void]$out.AppendLine('')
[void]$out.AppendLine(('// weapons={0}, compatible weapon/paint pairs={1}, music kits={2}' -f $weapons.Count, $compatibility.Count, $musicById.Count))

$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory) { New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null }
[System.IO.File]::WriteAllText($OutputPath, $out.ToString(), [System.Text.UTF8Encoding]::new($false))
Write-Host "Generated inventory catalog candidate: $OutputPath"
Write-Host "Weapons: $($weapons.Count); compatible weapon/paint pairs: $($compatibility.Count); music kits: $($musicById.Count)"
