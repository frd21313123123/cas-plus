#pragma once

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cas_catalog
{
namespace fs = std::filesystem;

constexpr std::uint32_t kCatalogMagic = 0x31434743u; // CGC1
constexpr std::uint16_t kCatalogVersion = 1;
constexpr std::size_t kMaxCatalogRecords = 8192;

#pragma pack(push, 1)
struct GameCatalogRecord
{
    std::uint16_t definitionIndex;
    std::uint8_t category; // 1 pistol, 2 rifle, 3 smg, 4 heavy, 5 sniper, 6 shotgun, 7 knife, 8 glove, 9 agent
    std::uint8_t teamMask; // 1 T, 2 CT, 3 both
    std::int32_t paintKit;
    wchar_t displayName[80];
    wchar_t weaponName[48];
    wchar_t finishName[64];
    char iconResource[160];
    char modelPlayer[160];
    char modelWorld[160];
};

struct GameCatalogHeader
{
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t recordSize;
    std::uint32_t count;
    std::uint32_t checksum;
};
#pragma pack(pop)

struct CatalogBuildStats
{
    std::size_t itemDefinitions = 0;
    std::size_t paintKits = 0;
    std::size_t iconDefinitions = 0;
    std::size_t records = 0;
    std::size_t duplicatesRemoved = 0;
    std::size_t missingIconAssets = 0;
    bool usedVpk = false;
    bool localized = false;
    fs::path gameRoot;
    fs::path outputPath;
};

inline std::string lowerAscii(std::string value)
{
    for (char& c : value)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

inline std::string normalizePath(std::string value)
{
    for (char& c : value)
    {
        if (c == '\\')
            c = '/';
        else
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    while (!value.empty() && value.front() == '/')
        value.erase(value.begin());
    return value;
}

inline bool readWholeFile(const fs::path& path, std::vector<std::uint8_t>& out)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        return false;
    stream.seekg(0, std::ios::end);
    const std::streamoff size = stream.tellg();
    if (size <= 0 || size > static_cast<std::streamoff>(128ull * 1024ull * 1024ull))
        return false;
    stream.seekg(0, std::ios::beg);
    out.resize(static_cast<std::size_t>(size));
    stream.read(reinterpret_cast<char*>(out.data()), size);
    return stream.good() || stream.eof();
}

inline std::uint16_t readU16(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    if (offset + 2 > bytes.size()) return 0;
    return static_cast<std::uint16_t>(bytes[offset]) |
        static_cast<std::uint16_t>(bytes[offset + 1] << 8);
}

inline std::uint32_t readU32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    if (offset + 4 > bytes.size()) return 0;
    return static_cast<std::uint32_t>(bytes[offset]) |
        (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
        (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
        (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

class VpkDirectory
{
public:
    struct Entry
    {
        std::uint16_t archiveIndex = 0;
        std::uint32_t offset = 0;
        std::uint32_t length = 0;
        std::uint16_t preloadBytes = 0;
        std::size_t preloadOffset = 0;
    };

    bool open(const fs::path& directoryVpk)
    {
        entries_.clear();
        directoryBytes_.clear();
        directoryPath_ = directoryVpk;
        if (!readWholeFile(directoryVpk, directoryBytes_) || directoryBytes_.size() < 12)
            return false;
        if (readU32(directoryBytes_, 0) != 0x55AA1234u)
            return false;
        const std::uint32_t version = readU32(directoryBytes_, 4);
        if (version != 1 && version != 2)
            return false;
        headerSize_ = version == 2 ? 28u : 12u;
        treeSize_ = readU32(directoryBytes_, 8);
        if (treeSize_ == 0 || headerSize_ + treeSize_ > directoryBytes_.size())
            return false;

        std::size_t cursor = headerSize_;
        const std::size_t treeEnd = headerSize_ + treeSize_;
        while (cursor < treeEnd)
        {
            std::string extension;
            if (!readCString(cursor, treeEnd, extension)) return false;
            if (extension.empty()) break;
            if (extension == " ") extension.clear();
            while (cursor < treeEnd)
            {
                std::string directory;
                if (!readCString(cursor, treeEnd, directory)) return false;
                if (directory.empty()) break;
                if (directory == " ") directory.clear();
                while (cursor < treeEnd)
                {
                    std::string filename;
                    if (!readCString(cursor, treeEnd, filename)) return false;
                    if (filename.empty()) break;
                    if (cursor + 18 > treeEnd) return false;

                    Entry entry{};
                    // CRC occupies +0..3 and is intentionally not needed here.
                    entry.preloadBytes = readU16(directoryBytes_, cursor + 4);
                    entry.archiveIndex = readU16(directoryBytes_, cursor + 6);
                    entry.offset = readU32(directoryBytes_, cursor + 8);
                    entry.length = readU32(directoryBytes_, cursor + 12);
                    const std::uint16_t terminator = readU16(directoryBytes_, cursor + 16);
                    cursor += 18;
                    if (terminator != 0xFFFFu || cursor + entry.preloadBytes > treeEnd)
                        return false;
                    entry.preloadOffset = cursor;
                    cursor += entry.preloadBytes;

                    std::string full;
                    if (!directory.empty()) full = directory + "/";
                    full += filename;
                    if (!extension.empty()) full += "." + extension;
                    entries_.emplace(normalizePath(full), entry);
                }
            }
        }
        return !entries_.empty();
    }

    bool valid() const { return !entries_.empty(); }

    bool hasFile(const std::string& path) const
    {
        return entries_.find(normalizePath(path)) != entries_.end();
    }

    bool readFile(const std::string& path, std::vector<std::uint8_t>& out) const
    {
        const auto found = entries_.find(normalizePath(path));
        if (found == entries_.end())
            return false;
        const Entry& entry = found->second;
        out.clear();
        out.reserve(static_cast<std::size_t>(entry.preloadBytes) + entry.length);
        if (entry.preloadBytes)
        {
            if (entry.preloadOffset + entry.preloadBytes > directoryBytes_.size())
                return false;
            out.insert(out.end(),
                directoryBytes_.begin() + static_cast<std::ptrdiff_t>(entry.preloadOffset),
                directoryBytes_.begin() + static_cast<std::ptrdiff_t>(entry.preloadOffset + entry.preloadBytes));
        }
        if (!entry.length)
            return true;

        fs::path dataPath = directoryPath_;
        std::uint64_t absoluteOffset = entry.offset;
        if (entry.archiveIndex == 0x7FFFu)
            absoluteOffset += static_cast<std::uint64_t>(headerSize_) + treeSize_;
        else
            dataPath = archivePath(entry.archiveIndex);

        std::ifstream stream(dataPath, std::ios::binary);
        if (!stream)
            return false;
        stream.seekg(static_cast<std::streamoff>(absoluteOffset), std::ios::beg);
        if (!stream)
            return false;
        const std::size_t oldSize = out.size();
        out.resize(oldSize + entry.length);
        stream.read(reinterpret_cast<char*>(out.data() + oldSize), entry.length);
        return static_cast<std::size_t>(stream.gcount()) == entry.length;
    }

private:
    bool readCString(std::size_t& cursor, std::size_t limit, std::string& out) const
    {
        out.clear();
        while (cursor < limit)
        {
            const char c = static_cast<char>(directoryBytes_[cursor++]);
            if (!c) return true;
            out.push_back(c);
            if (out.size() > 1024) return false;
        }
        return false;
    }

    fs::path archivePath(std::uint16_t index) const
    {
        std::wstring name = directoryPath_.filename().wstring();
        const std::wstring marker = L"_dir.vpk";
        const std::size_t pos = name.rfind(marker);
        std::wostringstream suffix;
        suffix << L"_" << std::setw(3) << std::setfill(L'0') << index << L".vpk";
        if (pos != std::wstring::npos)
            name.replace(pos, marker.size(), suffix.str());
        else
            name = directoryPath_.stem().wstring() + suffix.str();
        return directoryPath_.parent_path() / name;
    }

    fs::path directoryPath_;
    std::vector<std::uint8_t> directoryBytes_;
    std::unordered_map<std::string, Entry> entries_;
    std::size_t headerSize_ = 0;
    std::size_t treeSize_ = 0;
};

struct KvNode
{
    std::string key;
    std::string value;
    std::vector<KvNode> children;
};

class KvTokenizer
{
public:
    explicit KvTokenizer(const std::string& text) : text_(text) {}

    bool next(std::string& token)
    {
        token.clear();
        skipSpaceAndComments();
        if (pos_ >= text_.size()) return false;
        const char c = text_[pos_];
        if (c == '{' || c == '}')
        {
            token.push_back(c);
            ++pos_;
            return true;
        }
        if (c == '"')
        {
            ++pos_;
            while (pos_ < text_.size())
            {
                char ch = text_[pos_++];
                if (ch == '"') return true;
                if (ch == '\\' && pos_ < text_.size())
                {
                    const char escaped = text_[pos_++];
                    if (escaped == 'n') token.push_back('\n');
                    else if (escaped == 't') token.push_back('\t');
                    else token.push_back(escaped);
                }
                else token.push_back(ch);
            }
            return !token.empty();
        }
        while (pos_ < text_.size())
        {
            const char ch = text_[pos_];
            if (std::isspace(static_cast<unsigned char>(ch)) || ch == '{' || ch == '}')
                break;
            token.push_back(ch);
            ++pos_;
        }
        return !token.empty();
    }

private:
    void skipSpaceAndComments()
    {
        for (;;)
        {
            while (pos_ < text_.size() &&
                std::isspace(static_cast<unsigned char>(text_[pos_])))
                ++pos_;
            if (pos_ + 1 < text_.size() && text_[pos_] == '/' && text_[pos_ + 1] == '/')
            {
                pos_ += 2;
                while (pos_ < text_.size() && text_[pos_] != '\n') ++pos_;
                continue;
            }
            break;
        }
    }

    const std::string& text_;
    std::size_t pos_ = 0;
};

inline bool parseKvObject(KvTokenizer& tokenizer, std::vector<KvNode>& output, bool stopAtBrace)
{
    std::string key;
    while (tokenizer.next(key))
    {
        if (key == "}") return stopAtBrace;
        if (key == "{") continue;
        std::string next;
        if (!tokenizer.next(next)) return false;
        KvNode node{};
        node.key = key;
        if (next == "{")
        {
            if (!parseKvObject(tokenizer, node.children, true)) return false;
        }
        else if (next != "}")
            node.value = next;
        else
            return false;
        output.push_back(std::move(node));
    }
    return !stopAtBrace;
}

inline bool parseKeyValues(const std::vector<std::uint8_t>& bytes, KvNode& root)
{
    std::string text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF)
        text.erase(0, 3);
    KvTokenizer tokenizer(text);
    root = {};
    root.key = "<root>";
    return parseKvObject(tokenizer, root.children, false);
}

inline const KvNode* findChild(const KvNode& node, const std::string& key)
{
    const std::string wanted = lowerAscii(key);
    for (const KvNode& child : node.children)
        if (lowerAscii(child.key) == wanted)
            return &child;
    return nullptr;
}

inline const KvNode* findRecursive(const KvNode& node, const std::string& key)
{
    if (lowerAscii(node.key) == lowerAscii(key)) return &node;
    for (const KvNode& child : node.children)
        if (const KvNode* result = findRecursive(child, key)) return result;
    return nullptr;
}

inline std::string childValue(const KvNode& node, const std::string& key)
{
    const KvNode* child = findChild(node, key);
    return child ? child->value : std::string{};
}

inline int parseInteger(const std::string& text, int fallback = -1)
{
    if (text.empty()) return fallback;
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (!end || end == text.c_str()) return fallback;
    return static_cast<int>(value);
}

inline std::wstring utf8ToWide(const std::string& value)
{
    if (value.empty()) return {};
    const int needed = MultiByteToWideChar(CP_UTF8, 0, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (needed <= 0)
        return std::wstring(value.begin(), value.end());
    std::wstring output(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        output.data(), needed);
    return output;
}

inline std::wstring decodeWideText(const std::vector<std::uint8_t>& bytes)
{
    if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE)
    {
        std::wstring result;
        result.reserve((bytes.size() - 2) / 2);
        for (std::size_t i = 2; i + 1 < bytes.size(); i += 2)
            result.push_back(static_cast<wchar_t>(bytes[i] | (bytes[i + 1] << 8)));
        return result;
    }
    return utf8ToWide(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

inline std::wstring lowerWide(std::wstring value)
{
    for (wchar_t& c : value) c = static_cast<wchar_t>(std::towlower(c));
    return value;
}

inline std::wstring normalizeToken(const std::wstring& token)
{
    std::wstring result = token;
    while (!result.empty() && result.front() == L'#') result.erase(result.begin());
    return lowerWide(result);
}

inline std::unordered_map<std::wstring, std::wstring> parseLocalization(
    const std::vector<std::uint8_t>& bytes)
{
    std::unordered_map<std::wstring, std::wstring> result;
    const std::wstring text = decodeWideText(bytes);
    std::size_t lineStart = 0;
    while (lineStart < text.size())
    {
        std::size_t lineEnd = text.find(L'\n', lineStart);
        if (lineEnd == std::wstring::npos) lineEnd = text.size();
        const std::wstring line = text.substr(lineStart, lineEnd - lineStart);
        std::size_t q1 = line.find(L'"');
        if (q1 != std::wstring::npos)
        {
            std::size_t q2 = line.find(L'"', q1 + 1);
            std::size_t q3 = q2 == std::wstring::npos ? q2 : line.find(L'"', q2 + 1);
            std::size_t q4 = q3 == std::wstring::npos ? q3 : line.find_last_of(L'"');
            if (q2 != std::wstring::npos && q3 != std::wstring::npos &&
                q4 != std::wstring::npos && q4 > q3)
            {
                std::wstring key = normalizeToken(line.substr(q1 + 1, q2 - q1 - 1));
                std::wstring value = line.substr(q3 + 1, q4 - q3 - 1);
                if (!key.empty() && !value.empty()) result.emplace(std::move(key), std::move(value));
            }
        }
        lineStart = lineEnd + 1;
    }
    return result;
}

inline std::wstring localize(const std::string& token,
    const std::unordered_map<std::wstring, std::wstring>& localization,
    const std::string& fallback)
{
    if (!token.empty())
    {
        const std::wstring key = normalizeToken(utf8ToWide(token));
        const auto found = localization.find(key);
        if (found != localization.end() && !found->second.empty())
            return found->second;
    }
    return utf8ToWide(fallback);
}

inline void copyWide(wchar_t* destination, std::size_t capacity, const std::wstring& value)
{
    if (!destination || capacity == 0) return;
    const std::size_t count = (std::min)(capacity - 1, value.size());
    std::wmemcpy(destination, value.data(), count);
    destination[count] = L'\0';
}

inline void copyAscii(char* destination, std::size_t capacity, const std::string& value)
{
    if (!destination || capacity == 0) return;
    const std::size_t count = (std::min)(capacity - 1, value.size());
    std::memcpy(destination, value.data(), count);
    destination[count] = '\0';
}

inline std::uint8_t weaponCategory(const std::string& internal, int definition)
{
    const std::string name = lowerAscii(internal);
    if ((definition >= 500 && definition < 600) || name.find("knife") != std::string::npos)
        return 7;
    if (name.find("glove") != std::string::npos)
        return 8;
    static const std::unordered_set<std::string> pistols = {
        "weapon_deagle","weapon_elite","weapon_fiveseven","weapon_glock",
        "weapon_hkp2000","weapon_p250","weapon_usp_silencer","weapon_tec9",
        "weapon_cz75a","weapon_revolver"
    };
    static const std::unordered_set<std::string> rifles = {
        "weapon_ak47","weapon_aug","weapon_famas","weapon_galilar",
        "weapon_m4a1","weapon_m4a1_silencer","weapon_sg556"
    };
    static const std::unordered_set<std::string> smgs = {
        "weapon_mac10","weapon_mp5sd","weapon_mp7","weapon_mp9",
        "weapon_p90","weapon_bizon","weapon_ump45"
    };
    static const std::unordered_set<std::string> heavy = { "weapon_m249","weapon_negev" };
    static const std::unordered_set<std::string> snipers = { "weapon_awp","weapon_g3sg1","weapon_scar20","weapon_ssg08" };
    static const std::unordered_set<std::string> shotguns = { "weapon_nova","weapon_xm1014","weapon_mag7","weapon_sawedoff" };
    if (pistols.count(name)) return 1;
    if (rifles.count(name)) return 2;
    if (smgs.count(name)) return 3;
    if (heavy.count(name)) return 4;
    if (snipers.count(name)) return 5;
    if (shotguns.count(name)) return 6;
    return 0;
}

inline std::uint8_t teamMaskFromItem(const KvNode& item)
{
    const KvNode* used = findChild(item, "used_by_classes");
    if (!used) return 3;
    std::uint8_t mask = 0;
    for (const KvNode& child : used->children)
    {
        if (child.value == "0") continue;
        const std::string key = lowerAscii(child.key);
        if (key == "terrorists" || key == "terrorist") mask |= 1;
        if (key == "counter-terrorists" || key == "counter-terrorist") mask |= 2;
    }
    return mask ? mask : 3;
}

inline fs::path gameRootFromProcess(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return {};
    std::wstring path(32768, L'\0');
    DWORD length = static_cast<DWORD>(path.size());
    const BOOL ok = QueryFullProcessImageNameW(process, 0, path.data(), &length);
    CloseHandle(process);
    if (!ok || length == 0) return {};
    path.resize(length);
    fs::path exe(path);
    fs::path root = exe.parent_path(); // win64
    root = root.parent_path(); // bin
    root = root.parent_path(); // game
    return root;
}

inline bool readGameFile(const fs::path& csgoRoot, const VpkDirectory& vpk,
    const std::string& relativePath, std::vector<std::uint8_t>& out)
{
    const fs::path loose = csgoRoot / fs::path(relativePath);
    if (fs::exists(loose) && readWholeFile(loose, out))
        return true;
    return vpk.valid() && vpk.readFile(relativePath, out);
}

inline std::string chooseIconResource(const VpkDirectory& vpk, const std::string& logical,
    std::size_t* missingCounter)
{
    const std::string first = "panorama/images/" + logical + "_png.vtex_c";
    if (!vpk.valid() || vpk.hasFile(first)) return first;
    const std::string second = "panorama/images/" + logical + ".vtex_c";
    if (vpk.hasFile(second)) return second;
    if (missingCounter) ++(*missingCounter);
    // Keep the exact game logical icon path rather than inventing a CDN/image.
    return logical;
}

inline std::uint32_t checksumRecords(const GameCatalogRecord* records, std::size_t count)
{
    const std::uint8_t* bytes = reinterpret_cast<const std::uint8_t*>(records);
    const std::size_t size = count * sizeof(GameCatalogRecord);
    std::uint32_t hash = 2166136261u;
    for (std::size_t i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

inline bool writeCatalog(const std::vector<GameCatalogRecord>& records, fs::path& outputPath)
{
    wchar_t tempPath[MAX_PATH]{};
    const DWORD length = GetTempPathW(MAX_PATH, tempPath);
    if (!length || length >= MAX_PATH) return false;
    outputPath = fs::path(tempPath) / L"cas_plus_game_catalog_v1.bin";
    const fs::path staging = outputPath.wstring() + L".tmp";

    GameCatalogHeader header{};
    header.magic = kCatalogMagic;
    header.version = kCatalogVersion;
    header.recordSize = static_cast<std::uint16_t>(sizeof(GameCatalogRecord));
    header.count = static_cast<std::uint32_t>(records.size());
    header.checksum = checksumRecords(records.data(), records.size());

    std::ofstream stream(staging, std::ios::binary | std::ios::trunc);
    if (!stream) return false;
    stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!records.empty())
        stream.write(reinterpret_cast<const char*>(records.data()),
            static_cast<std::streamsize>(records.size() * sizeof(GameCatalogRecord)));
    stream.close();
    if (!stream) return false;
    return MoveFileExW(staging.c_str(), outputPath.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

inline bool buildFromRunningGame(DWORD pid, CatalogBuildStats* stats = nullptr)
{
    CatalogBuildStats localStats{};
    localStats.gameRoot = gameRootFromProcess(pid);
    if (localStats.gameRoot.empty())
        return false;
    const fs::path csgoRoot = localStats.gameRoot / L"csgo";
    if (!fs::exists(csgoRoot))
        return false;

    VpkDirectory vpk;
    localStats.usedVpk = vpk.open(csgoRoot / L"pak01_dir.vpk");

    std::vector<std::uint8_t> itemsBytes;
    if (!readGameFile(csgoRoot, vpk, "scripts/items/items_game.txt", itemsBytes))
        return false;
    KvNode root{};
    if (!parseKeyValues(itemsBytes, root))
        return false;
    const KvNode* itemsGame = findRecursive(root, "items_game");
    if (!itemsGame) itemsGame = &root;
    const KvNode* items = findChild(*itemsGame, "items");
    const KvNode* paintKits = findChild(*itemsGame, "paint_kits");
    const KvNode* alternateIcons = findChild(*itemsGame, "alternate_icons2");
    if (!items || !paintKits || !alternateIcons)
        return false;

    std::vector<std::uint8_t> localizationBytes;
    const LANGID language = GetUserDefaultUILanguage();
    const bool wantsRussian = PRIMARYLANGID(language) == LANG_RUSSIAN;
    std::unordered_map<std::wstring, std::wstring> localization;
    if (readGameFile(csgoRoot, vpk,
        wantsRussian ? "resource/csgo_russian.txt" : "resource/csgo_english.txt",
        localizationBytes) ||
        readGameFile(csgoRoot, vpk, "resource/csgo_english.txt", localizationBytes))
    {
        localization = parseLocalization(localizationBytes);
        localStats.localized = !localization.empty();
    }

    struct ItemInfo
    {
        int definition = 0;
        std::string internal;
        std::string nameToken;
        std::string prefab;
        std::string modelPlayer;
        std::string modelWorld;
        std::string imageInventory;
        std::uint8_t category = 0;
        std::uint8_t teamMask = 3;
        const KvNode* source = nullptr;
    };
    struct PaintInfo
    {
        int id = 0;
        std::string internal;
        std::string descriptionToken;
    };

    std::vector<ItemInfo> itemInfos;
    std::unordered_map<std::string, PaintInfo> paints;
    itemInfos.reserve(items->children.size());
    for (const KvNode& block : items->children)
    {
        const int definition = parseInteger(block.key, -1);
        if (definition <= 0 || definition > 65535) continue;
        ItemInfo item{};
        item.definition = definition;
        item.internal = childValue(block, "name");
        item.nameToken = childValue(block, "item_name");
        item.prefab = childValue(block, "prefab");
        item.modelPlayer = childValue(block, "model_player");
        item.modelWorld = childValue(block, "model_world");
        item.imageInventory = childValue(block, "image_inventory");
        item.category = weaponCategory(item.internal, definition);
        const std::string prefabLower = lowerAscii(item.prefab);
        const std::string internalLower = lowerAscii(item.internal);
        if (item.category == 0 &&
            (prefabLower.find("customplayer") != std::string::npos ||
             internalLower.find("customplayer") != std::string::npos) &&
            !item.modelPlayer.empty())
            item.category = 9;
        if (item.category == 0) continue;
        item.teamMask = teamMaskFromItem(block);
        item.source = &block;
        itemInfos.push_back(std::move(item));
    }
    localStats.itemDefinitions = itemInfos.size();

    for (const KvNode& block : paintKits->children)
    {
        const int id = parseInteger(block.key, -1);
        if (id <= 0) continue;
        PaintInfo paint{};
        paint.id = id;
        paint.internal = lowerAscii(childValue(block, "name"));
        paint.descriptionToken = childValue(block, "description_tag");
        if (!paint.internal.empty()) paints[paint.internal] = std::move(paint);
    }
    localStats.paintKits = paints.size();

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
    localStats.iconDefinitions = iconPaths.size();

    std::vector<const ItemInfo*> paintable;
    for (const ItemInfo& item : itemInfos)
        if (item.category >= 1 && item.category <= 8 && !item.internal.empty())
            paintable.push_back(&item);
    std::sort(paintable.begin(), paintable.end(), [](const ItemInfo* a, const ItemInfo* b) {
        return a->internal.size() > b->internal.size();
    });

    std::vector<GameCatalogRecord> records;
    records.reserve((std::min)(iconPaths.size() + itemInfos.size(), kMaxCatalogRecords));
    std::unordered_set<std::uint64_t> seen;

    auto appendRecord = [&](const ItemInfo& item, int paintKit,
        const std::wstring& finish, const std::string& iconLogical) {
        if (records.size() >= kMaxCatalogRecords) return;
        const std::uint64_t key = (static_cast<std::uint64_t>(
            static_cast<std::uint16_t>(item.definition)) << 32) |
            static_cast<std::uint32_t>(paintKit);
        if (!seen.insert(key).second)
        {
            ++localStats.duplicatesRemoved;
            return;
        }
        // A skin without a model is not useful to the in-match projection path.
        if (item.modelPlayer.empty() && item.modelWorld.empty()) return;

        GameCatalogRecord record{};
        record.definitionIndex = static_cast<std::uint16_t>(item.definition);
        record.category = item.category;
        record.teamMask = item.teamMask;
        record.paintKit = paintKit;
        const std::wstring weapon = localize(item.nameToken, localization, item.internal);
        std::wstring display = weapon;
        if (!finish.empty())
        {
            display += L" | ";
            display += finish;
        }
        copyWide(record.displayName, std::size(record.displayName), display);
        copyWide(record.weaponName, std::size(record.weaponName), weapon);
        copyWide(record.finishName, std::size(record.finishName), finish);
        copyAscii(record.iconResource, std::size(record.iconResource),
            chooseIconResource(vpk, iconLogical, &localStats.missingIconAssets));
        copyAscii(record.modelPlayer, std::size(record.modelPlayer), item.modelPlayer);
        copyAscii(record.modelWorld, std::size(record.modelWorld), item.modelWorld);
        records.push_back(record);
    };

    for (const std::string& iconOriginal : iconPaths)
    {
        std::string logical = normalizePath(iconOriginal);
        const std::string prefix = "econ/default_generated/";
        if (logical.rfind(prefix, 0) != 0) continue;
        std::string core = logical.substr(prefix.size());
        const std::string suffix = "_light";
        if (core.size() <= suffix.size() ||
            core.compare(core.size() - suffix.size(), suffix.size(), suffix) != 0)
            continue;
        core.resize(core.size() - suffix.size());

        const ItemInfo* item = nullptr;
        std::string paintInternal;
        for (const ItemInfo* candidate : paintable)
        {
            const std::string weaponPrefix = lowerAscii(candidate->internal) + "_";
            if (core.rfind(weaponPrefix, 0) == 0)
            {
                item = candidate;
                paintInternal = core.substr(weaponPrefix.size());
                break;
            }
        }
        if (!item) continue;
        const auto paintFound = paints.find(lowerAscii(paintInternal));
        if (paintFound == paints.end()) continue;
        const PaintInfo& paint = paintFound->second;
        const std::wstring finish = localize(paint.descriptionToken,
            localization, paint.internal);
        appendRecord(*item, paint.id, finish, logical);
    }

    // Agents are standalone item definitions rather than paint-kit pairs.
    for (const ItemInfo& item : itemInfos)
    {
        if (item.category != 9) continue;
        std::string icon = normalizePath(item.imageInventory);
        if (!icon.empty() && icon.rfind("econ/", 0) != 0)
            icon = "econ/characters/" + icon;
        appendRecord(item, 0, L"", icon);
    }

    std::sort(records.begin(), records.end(), [](const GameCatalogRecord& a,
        const GameCatalogRecord& b) {
        if (a.category != b.category) return a.category < b.category;
        const int weaponCompare = _wcsicmp(a.weaponName, b.weaponName);
        if (weaponCompare != 0) return weaponCompare < 0;
        const int finishCompare = _wcsicmp(a.finishName, b.finishName);
        if (finishCompare != 0) return finishCompare < 0;
        if (a.definitionIndex != b.definitionIndex)
            return a.definitionIndex < b.definitionIndex;
        return a.paintKit < b.paintKit;
    });

    localStats.records = records.size();
    if (records.empty() || !writeCatalog(records, localStats.outputPath))
        return false;
    if (stats) *stats = localStats;
    return true;
}

} // namespace cas_catalog
