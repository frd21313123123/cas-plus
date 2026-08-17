#pragma once

#include "game_catalog.h"

namespace cas_catalog
{
constexpr std::uint32_t kAttachmentCatalogMagic = 0x31415443u; // CTA1
constexpr std::uint16_t kAttachmentCatalogVersion = 1;
constexpr std::size_t kMaxAttachmentRecords = 8192;

enum AttachmentKind : std::uint8_t
{
    AttachmentSticker = 1,
    AttachmentPatch = 2,
    AttachmentKeychain = 3
};

#pragma pack(push, 1)
struct AttachmentCatalogRecord
{
    std::int32_t id;
    std::uint8_t kind;
    std::uint8_t reserved[3];
    wchar_t displayName[80];
    char internalName[96];
    char iconResource[160];
    char modelResource[160];
};

struct AttachmentCatalogHeader
{
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t recordSize;
    std::uint32_t count;
    std::uint32_t checksum;
    std::uint8_t maxStickers;
    std::uint8_t maxPatches;
    std::uint8_t reserved[2];
};
#pragma pack(pop)

struct AttachmentBuildStats
{
    std::size_t stickers = 0;
    std::size_t patches = 0;
    std::size_t keychains = 0;
    std::size_t duplicatesRemoved = 0;
    std::size_t missingIconAssets = 0;
    std::uint8_t maxStickers = 5;
    std::uint8_t maxPatches = 3;
    fs::path outputPath;
};

inline std::uint32_t checksumAttachmentRecords(
    const AttachmentCatalogRecord* records, std::size_t count)
{
    const std::uint8_t* bytes =
        reinterpret_cast<const std::uint8_t*>(records);
    const std::size_t size = count * sizeof(AttachmentCatalogRecord);
    std::uint32_t hash = 2166136261u;
    for (std::size_t i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

inline bool writeAttachmentCatalog(
    const std::vector<AttachmentCatalogRecord>& records,
    std::uint8_t maxStickers, std::uint8_t maxPatches,
    fs::path& outputPath)
{
    wchar_t tempPath[MAX_PATH]{};
    const DWORD length = GetTempPathW(MAX_PATH, tempPath);
    if (!length || length >= MAX_PATH)
        return false;
    outputPath = fs::path(tempPath) / L"cas_plus_attachment_catalog_v1.bin";
    const fs::path staging = outputPath.wstring() + L".tmp";

    AttachmentCatalogHeader header{};
    header.magic = kAttachmentCatalogMagic;
    header.version = kAttachmentCatalogVersion;
    header.recordSize =
        static_cast<std::uint16_t>(sizeof(AttachmentCatalogRecord));
    header.count = static_cast<std::uint32_t>(records.size());
    header.checksum = checksumAttachmentRecords(records.data(), records.size());
    header.maxStickers = maxStickers;
    header.maxPatches = maxPatches;

    std::ofstream stream(staging, std::ios::binary | std::ios::trunc);
    if (!stream)
        return false;
    stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!records.empty())
        stream.write(reinterpret_cast<const char*>(records.data()),
            static_cast<std::streamsize>(records.size() *
                sizeof(AttachmentCatalogRecord)));
    stream.close();
    if (!stream)
        return false;
    return MoveFileExW(staging.c_str(), outputPath.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

inline std::string attachmentLogicalIcon(std::uint8_t kind,
    const std::string& material, const std::string& explicitInventory)
{
    std::string logical = normalizePath(explicitInventory);
    if (!logical.empty())
        return logical;
    logical = normalizePath(material);
    if (logical.empty())
        return {};
    if (logical.rfind("econ/", 0) == 0)
        return logical;
    if (kind == AttachmentPatch)
        return "econ/patches/" + logical;
    if (kind == AttachmentSticker)
        return "econ/stickers/" + logical;
    return logical;
}

inline bool buildAttachmentCatalogFromRunningGame(DWORD pid,
    AttachmentBuildStats* stats = nullptr)
{
    AttachmentBuildStats localStats{};
    const fs::path gameRoot = gameRootFromProcess(pid);
    if (gameRoot.empty())
        return false;
    const fs::path csgoRoot = gameRoot / L"csgo";
    if (!fs::exists(csgoRoot))
        return false;

    VpkDirectory vpk;
    vpk.open(csgoRoot / L"pak01_dir.vpk");

    std::vector<std::uint8_t> itemsBytes;
    if (!readGameFile(csgoRoot, vpk,
        "scripts/items/items_game.txt", itemsBytes))
        return false;
    KvNode root{};
    if (!parseKeyValues(itemsBytes, root))
        return false;
    const KvNode* itemsGame = findRecursive(root, "items_game");
    if (!itemsGame)
        itemsGame = &root;

    const KvNode* stickerKits = findChild(*itemsGame, "sticker_kits");
    const KvNode* keychains = findChild(*itemsGame, "keychain_definitions");
    if (!stickerKits && !keychains)
        return false;

    if (const KvNode* gameInfo = findChild(*itemsGame, "game_info"))
    {
        const int stickers = parseInteger(
            childValue(*gameInfo, "max_num_stickers"), 5);
        const int patches = parseInteger(
            childValue(*gameInfo, "max_num_patches"), 3);
        localStats.maxStickers = static_cast<std::uint8_t>(
            (std::max)(1, (std::min)(stickers, 5)));
        localStats.maxPatches = static_cast<std::uint8_t>(
            (std::max)(1, (std::min)(patches, 5)));
    }

    std::vector<std::uint8_t> localizationBytes;
    const LANGID language = GetUserDefaultUILanguage();
    const bool wantsRussian = PRIMARYLANGID(language) == LANG_RUSSIAN;
    std::unordered_map<std::wstring, std::wstring> localization;
    if (readGameFile(csgoRoot, vpk,
        wantsRussian ? "resource/csgo_russian.txt" :
            "resource/csgo_english.txt",
        localizationBytes) ||
        readGameFile(csgoRoot, vpk,
            "resource/csgo_english.txt", localizationBytes))
        localization = parseLocalization(localizationBytes);

    std::vector<AttachmentCatalogRecord> records;
    records.reserve(2048);
    std::unordered_set<std::uint64_t> seen;

    auto append = [&](std::uint8_t kind, int id,
        const std::string& internal, const std::string& nameToken,
        const std::string& material, const std::string& explicitInventory,
        const std::string& model) {
        if (id <= 0 || records.size() >= kMaxAttachmentRecords)
            return;
        const std::uint64_t key =
            (static_cast<std::uint64_t>(kind) << 32) |
            static_cast<std::uint32_t>(id);
        if (!seen.insert(key).second)
        {
            ++localStats.duplicatesRemoved;
            return;
        }

        AttachmentCatalogRecord record{};
        record.id = id;
        record.kind = kind;
        const std::wstring display = localize(nameToken, localization,
            internal.empty() ? std::to_string(id) : internal);
        copyWide(record.displayName, std::size(record.displayName), display);
        copyAscii(record.internalName, std::size(record.internalName), internal);
        const std::string logical = attachmentLogicalIcon(
            kind, material, explicitInventory);
        copyAscii(record.iconResource, std::size(record.iconResource),
            logical.empty() ? std::string{} :
                chooseIconResource(vpk, logical,
                    &localStats.missingIconAssets));
        copyAscii(record.modelResource, std::size(record.modelResource), model);
        records.push_back(record);
        if (kind == AttachmentSticker) ++localStats.stickers;
        else if (kind == AttachmentPatch) ++localStats.patches;
        else if (kind == AttachmentKeychain) ++localStats.keychains;
    };

    if (stickerKits)
    {
        for (const KvNode& block : stickerKits->children)
        {
            const int id = parseInteger(block.key, -1);
            if (id <= 0)
                continue;
            const std::string patchMaterial = childValue(block, "patch_material");
            const std::string stickerMaterial = childValue(block, "sticker_material");
            const std::string internal = childValue(block, "name");
            const std::string nameToken = childValue(block, "item_name");
            const std::string imageInventory = childValue(block, "image_inventory");
            if (!patchMaterial.empty())
                append(AttachmentPatch, id, internal, nameToken,
                    patchMaterial, imageInventory, {});
            else if (!stickerMaterial.empty())
                append(AttachmentSticker, id, internal, nameToken,
                    stickerMaterial, imageInventory, {});
        }
    }

    if (keychains)
    {
        for (const KvNode& block : keychains->children)
        {
            const int id = parseInteger(block.key, -1);
            if (id <= 0)
                continue;
            append(AttachmentKeychain, id,
                childValue(block, "name"),
                childValue(block, "loc_name"), {},
                childValue(block, "image_inventory"),
                childValue(block, "pedestal_display_model"));
        }
    }

    std::sort(records.begin(), records.end(),
        [](const AttachmentCatalogRecord& a,
            const AttachmentCatalogRecord& b) {
            if (a.kind != b.kind) return a.kind < b.kind;
            const int nameCompare = _wcsicmp(a.displayName, b.displayName);
            if (nameCompare != 0) return nameCompare < 0;
            return a.id < b.id;
        });

    if (records.empty() || !writeAttachmentCatalog(records,
        localStats.maxStickers, localStats.maxPatches,
        localStats.outputPath))
        return false;
    if (stats)
        *stats = localStats;
    return true;
}

} // namespace cas_catalog
