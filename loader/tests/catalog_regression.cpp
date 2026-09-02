#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "game_catalog.prefab.h"

namespace fs = std::filesystem;
static int checks = 0;
static void require(bool condition, const char* description)
{
    ++checks;
    if (!condition)
    {
        std::cerr << "FAILED: " << description << '\n';
        std::exit(1);
    }
}

static cas_catalog::KvNode schema(const std::string& text)
{
    cas_catalog::KvNode root;
    require(cas_catalog::parseKeyValues({text.begin(), text.end()}, root), "fixture parses");
    return root;
}

static std::vector<cas_catalog::GameCatalogRecord> buildFixture()
{
    const auto root = schema(R"kv(
"items_game" {
  "prefabs" {
    "base" { "model_player" "models/weapons/base.vmdl" "used_by_classes" { "counter-terrorists" "1" } }
    "m4" { "prefab" "base" "item_name" "#M4" "image_inventory" "econ/weapons/base_weapons/weapon_m4a1" }
    "silencer" { "prefab" "m4" "item_name" "#M4S" }
    "loop_a" { "prefab" "loop_b" }
    "loop_b" { "prefab" "loop_a" }
  }
  "items" {
    "16" { "name" "weapon_m4a1" "prefab" "m4" }
    "60" { "name" "weapon_m4a1_silencer" "prefab" "silencer" }
    "7" { "name" "weapon_ak47" "item_name" "#AK" "prefab" "loop_a" }
    "500" { "name" "weapon_bayonet" "item_name" "#KNIFE" "model_player" "models/knife.vmdl" }
    "5030" { "name" "sporty_gloves" "item_name" "#GLOVE" "model_player" "models/gloves.vmdl" }
    "4619" { "name" "customplayer_fixture" "item_name" "#AGENT" "model_player" "models/agent.vmdl" }
    "17junk" { "name" "weapon_mac10" }
  }
  "paint_kits" {
    "100" { "name" "red" "description_tag" "#RED" "rarity" "rarity_ancient" }
    "101" { "name" "silencer_red" "description_tag" "#AMBIGUOUS" }
    "102" { "name" "blue" "description_tag" "#BLUE" "rarity" "rarity_mythical" }
    "103" { "name" "duplicate_name" "description_tag" "#RED" }
    "104" { "name" "duplicate_name" "description_tag" "#BLUE" }
    "105junk" { "name" "bad_id" }
  }
  "alternate_icons2" {
    "icon_path" "econ/default_generated/weapon_m4a1_red_light"
    "weapon_icons" {
      "0" { "icon_path" "panorama/images/econ/default_generated/weapon_m4a1_red_light_png.vtex_c" }
      "1" { "icon_path" "econ/default_generated/weapon_m4a1_silencer_blue_light.png" }
      "2" { "extra_group" { "icon_path" "econ/default_generated/weapon_bayonet_blue_light" } }
      "3" { "icon_path" "econ/default_generated/sporty_gloves_red_light" }
      "4" { "icon_path" "econ/default_generated/weapon_m4a1_silencer_red_light" }
      "5" { "icon_path" "econ/default_generated/weapon_ak47_duplicate_name_light" }
      "6" { "icon_path" "bad_prefix/econ/default_generated/weapon_ak47_red_light" }
      "7" { "icon_path" "econ/default_generated/weapon_ak47_bad_id_light" }
    }
  }
})kv");
    const std::unordered_map<std::wstring, std::wstring> localization = {
        {L"m4", L"M4A4"}, {L"m4s", L"M4A1-S"}, {L"ak", L"AK-47"},
        {L"knife", L"Bayonet"}, {L"glove", L"Sport Gloves"}, {L"agent", L"Agent"},
        {L"red", L"Red finish"}, {L"blue", L"Blue finish"}
    };
    cas_catalog::VpkDirectory vpk;
    cas_catalog::CatalogBuildStats stats;
    std::vector<cas_catalog::GameCatalogRecord> records;
    require(cas_catalog::buildCatalogRecords(root, vpk, localization, records, stats), "schema builds");
    const auto pair = [&](int def, int paint) -> const cas_catalog::GameCatalogRecord* {
        for (const auto& record : records)
            if (record.definitionIndex == def && record.paintKit == paint) return &record;
        return nullptr;
    };
    require(pair(16, 100) && pair(60, 102), "exact full weapon/finish matches");
    require(!pair(7, 100), "global paint ID does not fabricate AK pair");
    require(!pair(60, 100) && !pair(16, 101), "ambiguous prefix decomposition rejected");
    require(stats.ambiguousPairs == 1, "ambiguous schema reported");
    require(!pair(7, 103) && !pair(7, 104), "ambiguous paint internal name rejected");
    require(pair(500, 102), "icons at odd nested depth visited");
    require(pair(5030, 100) && !pair(5030, 0), "glove exact paint and no invented default");
    require(pair(16, 0) && pair(60, 0) && pair(7, 0) && pair(500, 0), "explicit weapon/knife defaults");
    require(pair(4619, 0), "standalone agent definition");
    require(std::wstring(pair(16, 0)->displayName) == L"M4A4", "default name is not first painted skin");
    require(std::wstring(pair(16, 100)->displayName) == L"M4A4 | Red finish", "localized weapon and finish");
    require(std::string(pair(16, 100)->rarity) == "rarity_ancient" &&
        std::string(pair(60, 102)->rarity) == "rarity_mythical", "paint rarity is preserved from schema");
    require(std::string(pair(60, 102)->modelPlayer) == "models/weapons/base.vmdl", "nested prefab model inherited");
    require(pair(60, 102)->teamMask == 2, "nested prefab team inherited");
    require(records.size() == 9 && stats.duplicatesRemoved == 1, "pairs deduplicated deterministically");
    require(cas_catalog::parseInteger("7junk") == -1 && cas_catalog::parseInteger("2147483648") == -1,
        "malformed and overflow IDs rejected");
    cas_catalog::KvNode invalid;
    const std::string truncated = "\"items_game\" { \"unterminated";
    require(!cas_catalog::parseKeyValues({truncated.begin(), truncated.end()}, invalid), "truncated quoted schema rejected");
    return records;
}

// Exercise the actual no-CRT consumer with real file/VirtualAlloc APIs. Only the
// temp directory is redirected to the test output directory, not the user's cache.
static fs::path runtimeDirectory;
static unsigned int fileOpenCount = 0;
static void testVpk()
{
    std::vector<unsigned char> tree;
    const auto string = [&](const char* value) {
        do { tree.push_back(static_cast<unsigned char>(*value)); } while (*value++);
    };
    const auto word = [&](unsigned int value) {
        tree.push_back(static_cast<unsigned char>(value));
        tree.push_back(static_cast<unsigned char>(value >> 8));
    };
    const auto dword = [&](unsigned int value) { word(value); word(value >> 16); };
    string("txt"); string("scripts/items"); string("fixture");
    dword(0); word(0); word(0x7FFF); dword(0); dword(4); word(0xFFFF);
    string(""); string(""); string("");
    const auto path = runtimeDirectory / L"fixture_dir.vpk";
    const auto write = [&](bool inlineData) {
        const unsigned int header[] = { 0x55AA1234u, 1u, static_cast<unsigned int>(tree.size()) };
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(header), sizeof(header));
        file.write(reinterpret_cast<const char*>(tree.data()), tree.size());
        if (inlineData) file.write("data", 4);
    };
    write(true);
    cas_catalog::VpkDirectory vpk;
    require(vpk.open(path) && vpk.hasFile("SCRIPTS\\ITEMS\\FIXTURE.TXT"), "VPK header/tree parsed and paths normalized");
    std::vector<unsigned char> data;
    require(vpk.readFile("scripts/items/fixture.txt", data) && std::string(data.begin(), data.end()) == "data",
        "VPK inline resource read at header plus tree offset");
    write(false);
    require(!vpk.readFile("scripts/items/fixture.txt", data), "VPK truncated resource fails bounds check");
    // Entry insertion is transactional: a malformed terminator after a valid
    // prefix must not leave VpkDirectory::valid() true on a failed parse.
    tree[tree.size() - 4] = 0;
    write(true);
    require(!vpk.open(path) && !vpk.valid(), "malformed VPK tree never publishes partial entries");
}
static DWORD WINAPI TestGetTempPathW(DWORD capacity, wchar_t* out)
{
    const std::wstring path = runtimeDirectory.wstring() + L"\\";
    if (path.size() + 1 >= capacity) return static_cast<DWORD>(path.size() + 1);
    std::wmemcpy(out, path.c_str(), path.size() + 1);
    return static_cast<DWORD>(path.size());
}
static HANDLE WINAPI TestCreateFileW(LPCWSTR path, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security,
    DWORD disposition, DWORD attributes, HANDLE templateFile)
{
    ++fileOpenCount;
    return ::CreateFileW(path, access, share, security, disposition, attributes, templateFile);
}
static BOOL WINAPI TestGetProcessTimes(HANDLE process, void* created, void* exited, void* kernel, void* user)
{
    return ::GetProcessTimes(process, static_cast<FILETIME*>(created), static_cast<FILETIME*>(exited),
        static_cast<FILETIME*>(kernel), static_cast<FILETIME*>(user));
}
constexpr DWORD CAS_GENERIC_READ = GENERIC_READ;
constexpr DWORD CAS_FILE_SHARE_READ = FILE_SHARE_READ;
constexpr DWORD CAS_OPEN_EXISTING = OPEN_EXISTING;
constexpr DWORD CAS_FILE_ATTRIBUTE_NORMAL = FILE_ATTRIBUTE_NORMAL;
#define CAS_INVALID_HANDLE_VALUE INVALID_HANDLE_VALUE
constexpr unsigned short INVENTORY_SLOT_KNIFE = 0xFFFEu;
constexpr unsigned short INVENTORY_SLOT_GLOVE = 0xFFFDu;
constexpr unsigned short INVENTORY_SLOT_AGENT = 0xFFFBu;
struct VirtualInventoryItem { unsigned short overrideDefinitionIndex; int paintKit; unsigned short slotDefinitionIndex; };
static LONG AtomicExchange(volatile LONG* destination, LONG value) { return InterlockedExchange(destination, value); }
static void* AtomicCompareExchangePointer(void** destination, void* desired, void* expected)
{ return InterlockedCompareExchangePointer(destination, desired, expected); }
static bool InventoryGameCatalogReady();
#define GetTempPathW TestGetTempPathW
#define CreateFileW TestCreateFileW
#define GetProcessTimes TestGetProcessTimes
#include "../../payload/src/inventory/inventory_game_catalog.inc"
#undef GetTempPathW
#undef CreateFileW
#undef GetProcessTimes

static void writeFixture(cas_catalog::GameCatalogHeader header,
    const std::vector<cas_catalog::GameCatalogRecord>& records, bool trailing = false, bool truncated = false)
{
    std::ofstream file(runtimeDirectory / L"cas_plus_game_catalog_v2.bin", std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    const auto bytes = records.size() * sizeof(records[0]);
    file.write(reinterpret_cast<const char*>(records.data()), static_cast<std::streamsize>(bytes - (truncated ? 1 : 0)));
    if (trailing) file.put('!');
    require(file.good(), "fixture file written");
}

static void testRuntime(const std::vector<cas_catalog::GameCatalogRecord>& records)
{
    static_assert(sizeof(InventoryGameCatalogRecord) == sizeof(cas_catalog::GameCatalogRecord));
    static_assert(sizeof(InventoryGameCatalogHeader) == sizeof(cas_catalog::GameCatalogHeader));
    require(!InventoryGameCatalogReady() && !InventoryGameCatalogValidatePair(16, 100), "missing catalog fail closed");
    require(fileOpenCount == 0, "lookup never performs lazy IO");
    ReloadInventoryGameCatalog();
    require(fileOpenCount == 0, "reload queues IO off calling thread");
    ProcessInventoryGameCatalogLoadRequests();
    require(fileOpenCount == 1 && !InventoryGameCatalogReady(), "worker handles missing file");
    cas_catalog::GameCatalogHeader header{};
    header.magic = cas_catalog::kCatalogMagic;
    header.version = cas_catalog::kCatalogVersion;
    header.recordSize = sizeof(cas_catalog::GameCatalogRecord);
    header.count = static_cast<unsigned int>(records.size());
    header.checksum = cas_catalog::checksumRecords(records.data(), records.size());
    header.processId = GetCurrentProcessId();
    header.processCreated = cas_catalog::processCreationTime(header.processId);
    header.schemaChecksum = 1234;
    const auto failed = [&](unsigned int status, const char* label) {
        ReloadInventoryGameCatalog();
        ProcessInventoryGameCatalogLoadRequests();
        require(!InventoryGameCatalogReady() && InventoryGameCatalogLastLoadStatus() == status, label);
    };
    auto bad = header;
    bad.processId ^= 1;
    writeFixture(bad, records);
    failed(INVENTORY_GAME_CATALOG_STATUS_STALE, "different process cache rejected");
    bad = header;
    ++bad.processCreated;
    writeFixture(bad, records);
    failed(INVENTORY_GAME_CATALOG_STATUS_STALE, "reused PID old process cache rejected");
    bad = header;
    bad.version = 1;
    writeFixture(bad, records);
    failed(INVENTORY_GAME_CATALOG_STATUS_HEADER, "v1 unbound cache rejected");
    bad = header;
    bad.checksum ^= 1;
    writeFixture(bad, records);
    failed(INVENTORY_GAME_CATALOG_STATUS_CHECKSUM, "checksum corruption rejected");
    writeFixture(header, records, true);
    failed(INVENTORY_GAME_CATALOG_STATUS_RECORDS, "trailing bytes rejected");
    writeFixture(header, records, false, true);
    failed(INVENTORY_GAME_CATALOG_STATUS_RECORDS, "truncated file rejected");
    auto invalid = records;
    for (wchar_t& c : invalid[0].weaponName) c = L'X';
    bad = header;
    bad.checksum = cas_catalog::checksumRecords(invalid.data(), invalid.size());
    writeFixture(bad, invalid);
    failed(INVENTORY_GAME_CATALOG_STATUS_CONTENT, "checksummed unterminated strings rejected");
    invalid = records;
    invalid[1] = invalid[0];
    bad.checksum = cas_catalog::checksumRecords(invalid.data(), invalid.size());
    writeFixture(bad, invalid);
    failed(INVENTORY_GAME_CATALOG_STATUS_CONTENT, "checksummed duplicate pairs rejected");
    writeFixture(header, records);
    ReloadInventoryGameCatalog();
    ProcessInventoryGameCatalogLoadRequests();
    require(InventoryGameCatalogReady(), "valid session catalog loads");
    require(InventoryGameCatalogValidateItem({16, 100, 16}) && !InventoryGameCatalogValidateItem({7, 100, 7}), "strict runtime pair validation");
    require(!InventoryGameCatalogValidateItem({16, 100, 7}), "valid pair in wrong weapon slot rejected");
    require(InventoryGameCatalogValidateItem({500, 102, INVENTORY_SLOT_KNIFE}) &&
        !InventoryGameCatalogValidateItem({500, 102, INVENTORY_SLOT_GLOVE}), "knife domain consistency");
    require(std::wstring(InventoryGameCatalogDisplayName({16, 0})) == L"M4A4", "default runtime label exact");
    require(InventoryGameCatalogForItem({5030, 0}) == nullptr, "missing default never aliases painted finish");
    require(InventoryGameCatalogCyclePaint(16, 100, 1) == 0, "finish cycling includes explicit default");
    require(InventoryGameCatalogCyclePaint(65500, 999, 1) == 999, "missing definition does not rewrite saved paint");
    const auto* savedPointer = InventoryGameCatalogAt(0);
    const auto opened = fileOpenCount;
    writeFixture(bad, invalid);
    require(ReloadInventoryGameCatalog(), "loaded session snapshot stays ready");
    ProcessInventoryGameCatalogLoadRequests();
    require(fileOpenCount == opened && savedPointer == InventoryGameCatalogAt(0), "reload cannot free or replace live pointer");
    InventoryGameCatalogSanitizeLoadedStore();
    ShutdownInventoryGameCatalog();
    require(!InventoryGameCatalogReady(), "shutdown clears snapshot after readers stop");
}

int wmain(int argc, wchar_t** argv)
{
    require(argc == 2, "test output directory argument supplied");
    runtimeDirectory = fs::absolute(argv[1]);
    fs::create_directories(runtimeDirectory);
    testVpk();
    testRuntime(buildFixture());
    std::cout << "Catalog regression: " << checks << " checks passed.\n";
}
