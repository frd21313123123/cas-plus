param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

function Replace-Required([string]$Needle, [string]$Replacement, [string]$Name) {
    $count = ([regex]::Matches($script:source, [regex]::Escape($Needle))).Count
    if ($count -ne 1) {
        throw "Game-catalog prefab fix anchor '$Name' expected exactly once, found $count."
    }
    $script:source = $script:source.Replace($Needle, $Replacement)
}

$sectionsAnchor = @'
    const KvNode* items = findChild(*itemsGame, "items");
    const KvNode* paintKits = findChild(*itemsGame, "paint_kits");
    const KvNode* alternateIcons = findChild(*itemsGame, "alternate_icons2");
    if (!items || !paintKits || !alternateIcons)
        return false;
'@
$sectionsReplacement = @'
    const KvNode* items = findChild(*itemsGame, "items");
    const KvNode* prefabs = findChild(*itemsGame, "prefabs");
    const KvNode* paintKits = findChild(*itemsGame, "paint_kits");
    const KvNode* alternateIcons = findChild(*itemsGame, "alternate_icons2");
    if (!items || !prefabs || !paintKits || !alternateIcons)
        return false;
'@
Replace-Required $sectionsAnchor $sectionsReplacement 'prefabs section lookup'

$structAnchor = @'
    struct ItemInfo
    {
'@
$resolverBlock = @'
    // Current CS2 item definitions intentionally keep most weapon metadata in
    // named prefabs.  Base items such as weapon_deagle only contain `name` and
    // `prefab`, while item_name/model_player/model_world/image_inventory live in
    // weapon_deagle_prefab (which may itself inherit another prefab). Resolve
    // the chain exactly like Source item-schema inheritance instead of treating
    // missing direct fields as missing game data.
    auto appendPrefabNames = [](std::vector<std::string>& output,
        const std::string& names)
    {
        std::size_t cursor = 0;
        while (cursor < names.size())
        {
            while (cursor < names.size() &&
                std::isspace(static_cast<unsigned char>(names[cursor])))
                ++cursor;
            const std::size_t start = cursor;
            while (cursor < names.size() &&
                !std::isspace(static_cast<unsigned char>(names[cursor])))
                ++cursor;
            if (cursor > start)
                output.push_back(names.substr(start, cursor - start));
        }
    };

    auto inheritedValue = [&](const KvNode& item,
        const std::string& key) -> std::string
    {
        const std::string direct = childValue(item, key);
        if (!direct.empty())
            return direct;

        std::vector<std::string> queue;
        appendPrefabNames(queue, childValue(item, "prefab"));
        std::unordered_set<std::string> visited;
        for (std::size_t index = 0;
            index < queue.size() && index < 64; ++index)
        {
            const std::string normalized = lowerAscii(queue[index]);
            if (normalized.empty() || !visited.insert(normalized).second)
                continue;
            const KvNode* prefab = findChild(*prefabs, queue[index]);
            if (!prefab)
                continue;
            const std::string value = childValue(*prefab, key);
            if (!value.empty())
                return value;
            appendPrefabNames(queue, childValue(*prefab, "prefab"));
        }
        return {};
    };

    auto inheritedNode = [&](const KvNode& item,
        const std::string& key) -> const KvNode*
    {
        if (const KvNode* direct = findChild(item, key))
            return direct;

        std::vector<std::string> queue;
        appendPrefabNames(queue, childValue(item, "prefab"));
        std::unordered_set<std::string> visited;
        for (std::size_t index = 0;
            index < queue.size() && index < 64; ++index)
        {
            const std::string normalized = lowerAscii(queue[index]);
            if (normalized.empty() || !visited.insert(normalized).second)
                continue;
            const KvNode* prefab = findChild(*prefabs, queue[index]);
            if (!prefab)
                continue;
            if (const KvNode* value = findChild(*prefab, key))
                return value;
            appendPrefabNames(queue, childValue(*prefab, "prefab"));
        }
        return nullptr;
    };

    auto inheritedTeamMask = [&](const KvNode& item) -> std::uint8_t
    {
        const KvNode* used = inheritedNode(item, "used_by_classes");
        if (!used)
            return 3;
        std::uint8_t mask = 0;
        for (const KvNode& child : used->children)
        {
            if (child.value == "0")
                continue;
            const std::string name = lowerAscii(child.key);
            if (name == "terrorists" || name == "terrorist")
                mask |= 1;
            if (name == "counter-terrorists" ||
                name == "counter-terrorist")
                mask |= 2;
        }
        return mask ? mask : 3;
    };

    struct ItemInfo
    {
'@
Replace-Required $structAnchor $resolverBlock 'prefab inheritance helpers'

$itemFieldsAnchor = @'
        item.internal = childValue(block, "name");
        item.nameToken = childValue(block, "item_name");
        item.prefab = childValue(block, "prefab");
        item.modelPlayer = childValue(block, "model_player");
        item.modelWorld = childValue(block, "model_world");
        item.imageInventory = childValue(block, "image_inventory");
'@
$itemFieldsReplacement = @'
        item.internal = inheritedValue(block, "name");
        item.nameToken = inheritedValue(block, "item_name");
        item.prefab = childValue(block, "prefab");
        item.modelPlayer = inheritedValue(block, "model_player");
        item.modelWorld = inheritedValue(block, "model_world");
        item.imageInventory = inheritedValue(block, "image_inventory");
'@
Replace-Required $itemFieldsAnchor $itemFieldsReplacement 'inherited weapon metadata'

$teamAnchor = '        item.teamMask = teamMaskFromItem(block);'
$teamReplacement = '        item.teamMask = inheritedTeamMask(block);'
Replace-Required $teamAnchor $teamReplacement 'inherited team ownership'

$modelGateAnchor = @'
        // A skin without a model is not useful to the in-match projection path.
        if (item.modelPlayer.empty() && item.modelWorld.empty()) return;
'@
$modelGateReplacement = @'
        // Existing weapon entities already own their view/world model; paint-kit
        // projection does not require a catalog model path.  Knife/glove/agent
        // definition swaps still require a real game model and remain fail-closed.
        if (item.category >= 7 &&
            item.modelPlayer.empty() && item.modelWorld.empty())
            return;
'@
Replace-Required $modelGateAnchor $modelGateReplacement 'ordinary weapon model gate'

$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}
Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Generated prefab-aware current-game catalog parser: $OutputPath"
