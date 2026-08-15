// Runtime Source 2 material manager for mesh Chams.
//
// Creates independent visible/Ignore-Z material pairs through KeyValues3 and
// MaterialSystem2. All engine entry points are resolved fail-closed. Flat is
// the mandatory baseline; optional presets fall back to Flat per pass.

enum ChamsMaterialPreset {
    CHAMS_MATERIAL_FLAT = 0,
    CHAMS_MATERIAL_SHADED = 1,
    CHAMS_MATERIAL_METALLIC = 2,
    CHAMS_MATERIAL_GLOW = 3,
    CHAMS_MATERIAL_COUNT = 4
};

struct KV3IdView {
    const char* name;
    unsigned long long unknown0;
    unsigned long long unknown1;
};

struct MaterialResourceBinding {
    void* data;
};

struct MaterialStrongHandle {
    MaterialResourceBinding* binding;
};

using LoadKv3Fn = bool (__fastcall*)(void*, void*, const char*,
    const KV3IdView*, const KV3IdView*, unsigned int);
using CreateRuntimeMaterialFn = void (__fastcall*)(void*, MaterialStrongHandle*,
    const char*, void*, unsigned int, unsigned int);
using SetTypeKv3Fn = void* (__fastcall*)(void*, unsigned int, unsigned int);

struct RuntimeMaterialPair {
    void* visible;
    void* occluded;
};

struct MaterialManagerState {
    void* materialSystem;
    LoadKv3Fn loadKv3;
    CreateRuntimeMaterialFn createMaterial;
    SetTypeKv3Fn setTypeKv3;
    RuntimeMaterialPair presets[CHAMS_MATERIAL_COUNT];
    bool resolveAttempted;
    bool functionsReady;
    bool materialsReady;
    unsigned int resolveFailures;
    unsigned int createFailures;
    unsigned int presetFallbacks;
};

static MaterialManagerState g_materialManager{};
alignas(16) static BYTE g_materialKv3Storage[CHAMS_MATERIAL_COUNT * 2][0x200]{};

static void ZeroMaterialStorage(BYTE* destination, SIZE_T size)
{
    volatile BYTE* cursor = destination;
    for (SIZE_T i = 0; i < size; ++i)
        cursor[i] = 0;
}

static const char kFlatVisibleVmat[] =
"<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->\n"
"{\n shader = \"csgo_unlitgeneric.vfx\"\n F_PAINT_VERTEX_COLORS = 1\n F_TRANSLUCENT = 1\n F_BLEND_MODE = 1\n g_vColorTint = [1.0, 1.0, 1.0, 1.0]\n g_tColor = resource:\"materials/default/default_mask_tga_fde710a5.vtex\"\n g_tNormal = resource:\"materials/default/default_normal_tga_1b833b2a.vtex\"\n g_tTintMask = resource:\"materials/default/default_mask_tga_fde710a5.vtex\"\n}\n";
static const char kFlatOccludedVmat[] =
"<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->\n"
"{\n shader = \"csgo_unlitgeneric.vfx\"\n F_PAINT_VERTEX_COLORS = 1\n F_TRANSLUCENT = 1\n F_BLEND_MODE = 1\n F_DISABLE_Z_BUFFERING = 1\n F_DISABLE_Z_PREPASS = 1\n F_DISABLE_Z_WRITE = 1\n g_vColorTint = [1.0, 1.0, 1.0, 1.0]\n g_tColor = resource:\"materials/default/default_mask_tga_fde710a5.vtex\"\n g_tNormal = resource:\"materials/default/default_normal_tga_1b833b2a.vtex\"\n g_tTintMask = resource:\"materials/default/default_mask_tga_fde710a5.vtex\"\n}\n";

static const char kShadedVisibleVmat[] =
"<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->\n"
"{\n shader = \"csgo_character.vfx\"\n F_PAINT_VERTEX_COLORS = 1\n F_TRANSLUCENT = 1\n F_BLEND_MODE = 1\n g_vColorTint = [1.0, 1.0, 1.0, 1.0]\n g_bFogEnabled = 0\n g_flMetalness = 0.0\n g_flRoughness = 0.28\n g_tColor = resource:\"materials/dev/primary_white_color_tga_21186c76.vtex\"\n g_tNormal = resource:\"materials/default/default_normal_tga_1b833b2a.vtex\"\n g_tMetalness = resource:\"materials/default/default_metal_tga_8fbc2820.vtex\"\n}\n";
static const char kShadedOccludedVmat[] =
"<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->\n"
"{\n shader = \"csgo_character.vfx\"\n F_PAINT_VERTEX_COLORS = 1\n F_TRANSLUCENT = 1\n F_BLEND_MODE = 1\n F_DISABLE_Z_BUFFERING = 1\n F_DISABLE_Z_PREPASS = 1\n F_DISABLE_Z_WRITE = 1\n g_vColorTint = [1.0, 1.0, 1.0, 1.0]\n g_bFogEnabled = 0\n g_flMetalness = 0.0\n g_flRoughness = 0.28\n g_tColor = resource:\"materials/dev/primary_white_color_tga_21186c76.vtex\"\n g_tNormal = resource:\"materials/default/default_normal_tga_1b833b2a.vtex\"\n g_tMetalness = resource:\"materials/default/default_metal_tga_8fbc2820.vtex\"\n}\n";

static const char kMetallicVisibleVmat[] =
"<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->\n"
"{\n shader = \"csgo_complex.vfx\"\n F_PAINT_VERTEX_COLORS = 1\n F_TRANSLUCENT = 1\n F_BLEND_MODE = 1\n g_vColorTint = [1.0, 1.0, 1.0, 1.0]\n g_flMetalness = 1.0\n g_flRoughness = 0.18\n g_tColor = resource:\"materials/dev/primary_white_color_tga_21186c76.vtex\"\n g_tNormal = resource:\"materials/default/default_normal_tga_1b833b2a.vtex\"\n g_tMetalness = resource:\"materials/default/default_metal_tga_8fbc2820.vtex\"\n}\n";
static const char kMetallicOccludedVmat[] =
"<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->\n"
"{\n shader = \"csgo_complex.vfx\"\n F_PAINT_VERTEX_COLORS = 1\n F_TRANSLUCENT = 1\n F_BLEND_MODE = 1\n F_DISABLE_Z_BUFFERING = 1\n F_DISABLE_Z_PREPASS = 1\n F_DISABLE_Z_WRITE = 1\n g_vColorTint = [1.0, 1.0, 1.0, 1.0]\n g_flMetalness = 1.0\n g_flRoughness = 0.18\n g_tColor = resource:\"materials/dev/primary_white_color_tga_21186c76.vtex\"\n g_tNormal = resource:\"materials/default/default_normal_tga_1b833b2a.vtex\"\n g_tMetalness = resource:\"materials/default/default_metal_tga_8fbc2820.vtex\"\n}\n";

static const char kGlowVisibleVmat[] =
"<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->\n"
"{\n shader = \"csgo_complex.vfx\"\n F_SELF_ILLUM = 1\n F_PAINT_VERTEX_COLORS = 1\n F_TRANSLUCENT = 1\n F_BLEND_MODE = 1\n g_vColorTint = [1.0, 1.0, 1.0, 1.0]\n g_vSelfIllumTint = [1.0, 1.0, 1.0, 1.0]\n g_flSelfIllumScale = [4.0, 4.0, 4.0, 4.0]\n g_flSelfIllumBrightness = [3.0, 3.0, 3.0, 3.0]\n g_tColor = resource:\"materials/dev/primary_white_color_tga_21186c76.vtex\"\n g_tNormal = resource:\"materials/default/default_normal_tga_1b833b2a.vtex\"\n g_tSelfIllumMask = resource:\"materials/default/default_mask_tga_fde710a5.vtex\"\n}\n";
static const char kGlowOccludedVmat[] =
"<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->\n"
"{\n shader = \"csgo_complex.vfx\"\n F_SELF_ILLUM = 1\n F_PAINT_VERTEX_COLORS = 1\n F_TRANSLUCENT = 1\n F_BLEND_MODE = 1\n F_DISABLE_Z_BUFFERING = 1\n F_DISABLE_Z_PREPASS = 1\n F_DISABLE_Z_WRITE = 1\n g_vColorTint = [1.0, 1.0, 1.0, 1.0]\n g_vSelfIllumTint = [1.0, 1.0, 1.0, 1.0]\n g_flSelfIllumScale = [4.0, 4.0, 4.0, 4.0]\n g_flSelfIllumBrightness = [3.0, 3.0, 3.0, 3.0]\n g_tColor = resource:\"materials/dev/primary_white_color_tga_21186c76.vtex\"\n g_tNormal = resource:\"materials/default/default_normal_tga_1b833b2a.vtex\"\n g_tSelfIllumMask = resource:\"materials/default/default_mask_tga_fde710a5.vtex\"\n}\n";

struct MaterialPresetSpec {
    const char* visibleName;
    const char* occludedName;
    const char* visibleVmat;
    const char* occludedVmat;
};

static const MaterialPresetSpec kMaterialPresetSpecs[CHAMS_MATERIAL_COUNT] = {
    { "materials/dev/casplus_flat_visible.vmat", "materials/dev/casplus_flat_occluded.vmat", kFlatVisibleVmat, kFlatOccludedVmat },
    { "materials/dev/casplus_shaded_visible.vmat", "materials/dev/casplus_shaded_occluded.vmat", kShadedVisibleVmat, kShadedOccludedVmat },
    { "materials/dev/casplus_metallic_visible.vmat", "materials/dev/casplus_metallic_occluded.vmat", kMetallicVisibleVmat, kMetallicOccludedVmat },
    { "materials/dev/casplus_glow_visible.vmat", "materials/dev/casplus_glow_occluded.vmat", kGlowVisibleVmat, kGlowOccludedVmat }
};

static bool ResolveMaterialManagerFunctions()
{
    if (g_materialManager.functionsReady)
        return true;
    if (g_materialManager.resolveAttempted)
        return false;
    g_materialManager.resolveAttempted = true;

    HMODULE tier0 = GetModuleHandleW(L"tier0.dll");
    HMODULE materialSystemModule = GetModuleHandleW(L"materialsystem2.dll");
    HMODULE clientModule = GetModuleHandleW(L"client.dll");
    if (!tier0 || !materialSystemModule || !clientModule)
    {
        ++g_materialManager.resolveFailures;
        return false;
    }

    static const char kLoadKv3Export[] =
        "?LoadKV3@@YA_NPEAVKeyValues3@@PEAVCUtlString@@PEBDAEBUKV3ID_t@@2I@Z";
    g_materialManager.loadKv3 = reinterpret_cast<LoadKv3Fn>(
        GetProcAddress(tier0, kLoadKv3Export));
    if (!g_materialManager.loadKv3)
    {
        static const int kLoadKv3Pattern[] = {
            0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x70,
            0x4C, 0x8B, 0xD1, 0x48, 0xC7, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF,
            0x48, 0xFF, 0xC0, 0x41, 0x80, 0x3C, 0x00, 0x00, 0x75, 0xF6
        };
        BYTE* loadKv3 = FindUniquePattern(tier0, kLoadKv3Pattern,
            sizeof(kLoadKv3Pattern) / sizeof(kLoadKv3Pattern[0]));
        if (loadKv3 && IsExecutable(loadKv3))
            g_materialManager.loadKv3 = reinterpret_cast<LoadKv3Fn>(loadKv3);
    }

    static const int kSetTypeKv3Pattern[] = {
        0x40, 0x53, 0x48, 0x83, 0xEC, -1, 0x80, 0xFA
    };
    BYTE* setTypeKv3 = FindUniquePattern(clientModule, kSetTypeKv3Pattern,
        sizeof(kSetTypeKv3Pattern) / sizeof(kSetTypeKv3Pattern[0]));
    if (setTypeKv3 && IsExecutable(setTypeKv3))
        g_materialManager.setTypeKv3 = reinterpret_cast<SetTypeKv3Fn>(setTypeKv3);

    static const int kCreateMaterialPattern[] = {
        0x48, 0x89, 0x5C, 0x24, -1, 0x48, 0x89, 0x6C, 0x24, -1,
        0x48, 0x89, 0x74, 0x24, -1, 0x48, 0x89, 0x7C, 0x24, -1,
        0x41, 0x56, 0x48, 0x81, 0xEC, -1, -1, -1, -1,
        0x48, 0x8B, 0x05, -1, -1, -1, -1, 0x48, 0x8B, 0xF2
    };
    BYTE* createMaterial = FindUniquePattern(materialSystemModule,
        kCreateMaterialPattern,
        sizeof(kCreateMaterialPattern) / sizeof(kCreateMaterialPattern[0]));
    if (createMaterial && IsExecutable(createMaterial))
        g_materialManager.createMaterial = reinterpret_cast<CreateRuntimeMaterialFn>(createMaterial);

    auto createInterface = reinterpret_cast<CreateInterfaceFn>(
        GetProcAddress(materialSystemModule, "CreateInterface"));
    if (createInterface)
        g_materialManager.materialSystem = createInterface("VMaterialSystem2_001", nullptr);

    g_materialManager.functionsReady = g_materialManager.loadKv3 &&
        g_materialManager.createMaterial && g_materialManager.setTypeKv3 &&
        g_materialManager.materialSystem;
    if (!g_materialManager.functionsReady)
        ++g_materialManager.resolveFailures;
    return g_materialManager.functionsReady;
}

static void* CreateRuntimeMaterial(const char* name, const char* vmat,
    BYTE* kv3Storage, SIZE_T storageSize)
{
    if (!name || !vmat || !kv3Storage || storageSize < 0x200 ||
        !ResolveMaterialManagerFunctions())
        return nullptr;
    ZeroMaterialStorage(kv3Storage, storageSize);
    void* kv3 = g_materialManager.setTypeKv3(kv3Storage, 1U, 6U);
    if (!kv3)
        return nullptr;
    const KV3IdView genericId = {
        "generic", 0x41B818518343427Eull, 0xB5F447C23C0CDF8Cull
    };
    if (!g_materialManager.loadKv3(kv3, nullptr, vmat, &genericId, nullptr, 0U))
        return nullptr;
    MaterialStrongHandle handle;
    handle.binding = nullptr;
    g_materialManager.createMaterial(g_materialManager.materialSystem, &handle,
        name, kv3, 0U, 1U);
    if (!handle.binding || !IsAccessible(handle.binding,
        sizeof(MaterialResourceBinding), false) ||
        !IsAccessible(handle.binding->data, sizeof(void*), false))
        return nullptr;
    return handle.binding->data;
}

static bool EnsureMaterialManagerReady()
{
    if (g_materialManager.materialsReady)
        return true;
    if (!ResolveMaterialManagerFunctions())
        return false;

    for (int i = 0; i < CHAMS_MATERIAL_COUNT; ++i)
    {
        const MaterialPresetSpec& spec = kMaterialPresetSpecs[i];
        g_materialManager.presets[i].visible = CreateRuntimeMaterial(
            spec.visibleName, spec.visibleVmat, g_materialKv3Storage[i * 2], 0x200);
        g_materialManager.presets[i].occluded = CreateRuntimeMaterial(
            spec.occludedName, spec.occludedVmat,
            g_materialKv3Storage[i * 2 + 1], 0x200);
        if (!g_materialManager.presets[i].visible ||
            !g_materialManager.presets[i].occluded)
            ++g_materialManager.createFailures;
    }

    g_materialManager.materialsReady =
        g_materialManager.presets[CHAMS_MATERIAL_FLAT].visible &&
        g_materialManager.presets[CHAMS_MATERIAL_FLAT].occluded;
    return g_materialManager.materialsReady;
}

static int ClampChamsMaterialPreset(int preset)
{
    if (preset < 0)
        return 0;
    if (preset >= CHAMS_MATERIAL_COUNT)
        return CHAMS_MATERIAL_COUNT - 1;
    return preset;
}

static void* ChamsMaterialForPass(int preset, bool occluded)
{
    if (!g_materialManager.materialsReady)
        return nullptr;
    const int selected = ClampChamsMaterialPreset(preset);
    RuntimeMaterialPair& pair = g_materialManager.presets[selected];
    void* material = occluded ? pair.occluded : pair.visible;
    if (material)
        return material;
    ++g_materialManager.presetFallbacks;
    RuntimeMaterialPair& flat =
        g_materialManager.presets[CHAMS_MATERIAL_FLAT];
    return occluded ? flat.occluded : flat.visible;
}

static void ResetMaterialManagerState()
{
    for (int i = 0; i < CHAMS_MATERIAL_COUNT; ++i)
    {
        g_materialManager.presets[i].visible = nullptr;
        g_materialManager.presets[i].occluded = nullptr;
    }
    g_materialManager.materialsReady = false;
    g_materialManager.functionsReady = false;
    g_materialManager.materialSystem = nullptr;
    g_materialManager.loadKv3 = nullptr;
    g_materialManager.createMaterial = nullptr;
    g_materialManager.setTypeKv3 = nullptr;
}
