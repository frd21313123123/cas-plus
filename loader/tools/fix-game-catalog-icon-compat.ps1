param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

function Replace-Required([string]$Needle, [string]$Replacement, [string]$Name) {
    $count = ([regex]::Matches($script:source, [regex]::Escape($Needle))).Count
    if ($count -ne 1) {
        throw "Game-catalog icon compatibility anchor '$Name' expected exactly once, found $count."
    }
    $script:source = $script:source.Replace($Needle, $Replacement)
}

# Valve has used both logical econ paths and Panorama/compiled-texture wrappers
# around the same generated icon. Preserve only strings that actually originate
# in alternate_icons2; later pair validation still requires a real item and a
# real paint-kit internal name.
$collectAnchor = @'
    std::vector<std::string> iconPaths;
    std::vector<const KvNode*> stack;
    stack.push_back(alternateIcons);
    while (!stack.empty())
    {
        const KvNode* node = stack.back();
        stack.pop_back();
        for (const KvNode& child : node->children)
        {
            if (lowerAscii(child.key) == "icon_path" && !child.value.empty())
                iconPaths.push_back(child.value);
            for (const KvNode& nested : child.children) stack.push_back(&nested);
        }
    }
'@
$collectReplacement = @'
    std::vector<std::string> iconPaths;
    std::vector<const KvNode*> stack;
    stack.push_back(alternateIcons);
    while (!stack.empty())
    {
        const KvNode* node = stack.back();
        stack.pop_back();
        const std::string keyLower = lowerAscii(node->key);
        if (keyLower == "icon_path" && !node->value.empty())
            iconPaths.push_back(node->value);
        else
        {
            if (normalizePath(node->key).find("econ/default_generated/") != std::string::npos)
                iconPaths.push_back(node->key);
            if (normalizePath(node->value).find("econ/default_generated/") != std::string::npos)
                iconPaths.push_back(node->value);
        }
        // Visit every depth; the old grandchild-only traversal skipped icon
        // objects when Valve introduced an additional grouping level.
        for (const KvNode& child : node->children)
            stack.push_back(&child);
    }
'@
Replace-Required $collectAnchor $collectReplacement 'alternate_icons2 candidate collection'

$parseAnchor = @'
        std::string logical = normalizePath(iconOriginal);
        const std::string prefix = "econ/default_generated/";
        if (logical.rfind(prefix, 0) != 0) continue;
        std::string core = logical.substr(prefix.size());
        const std::string suffix = "_light";
'@
$parseReplacement = @'
        std::string logical = normalizePath(iconOriginal);
        const std::string prefix = "econ/default_generated/";
        const std::string panoramaPrefix = "panorama/images/";
        if (logical.rfind(panoramaPrefix, 0) == 0)
            logical.erase(0, panoramaPrefix.size());
        if (logical.rfind(prefix, 0) != 0)
            continue;

        const std::string compiledSuffix = ".vtex_c";
        if (logical.size() > compiledSuffix.size() &&
            logical.compare(logical.size() - compiledSuffix.size(),
                compiledSuffix.size(), compiledSuffix) == 0)
            logical.resize(logical.size() - compiledSuffix.size());
        const std::string pngToken = "_png";
        if (logical.size() > pngToken.size() &&
            logical.compare(logical.size() - pngToken.size(),
                pngToken.size(), pngToken) == 0)
            logical.resize(logical.size() - pngToken.size());
        const std::string pngSuffix = ".png";
        if (logical.size() > pngSuffix.size() &&
            logical.compare(logical.size() - pngSuffix.size(),
                pngSuffix.size(), pngSuffix) == 0)
            logical.resize(logical.size() - pngSuffix.size());

        std::string core = logical.substr(prefix.size());
        const std::string suffix = "_light";
'@
Replace-Required $parseAnchor $parseReplacement 'generated icon path canonicalization'

Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Normalized alternate_icons2 generated icon paths: $InputPath"
