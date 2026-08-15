// Runtime Source 2 material manager for mesh Chams.
//
// Creates independent visible/Ignore-Z material pairs through KeyValues3 and
// MaterialSystem2. Module-loading failures are transient; signature/layout
// resolution is attempted only after all required modules are present.

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

struct MaterialResourceBinding { void* data; };
struct MaterialStrongHandle { MaterialResourceBinding* binding; };

using LoadKv3Fn = bool (__fastcall*)(void*, void*, const char*,
    const KV3IdView*, const KV3IdView*, unsigned int);
using CreateRuntimeMaterialFn = void (__fastcall*)(void*, MaterialStrongHandle*,
    const char*, void*, unsigned int, unsigned int);
using SetTypeKv3Fn = void* (__fastcall*)(void*, unsigned int, unsigned int);

struct RuntimeMaterialPair { void* visible; void* occluded; };

struct MaterialManagerState {
    void* materialSystem;
    LoadKv3Fn loadKv3;
    CreateRuntimeMaterialFn createMaterial;
    SetTypeKv3Fn setTypeKv3;
    RuntimeMaterialPair presets[CHAMS_MATERIAL_COUNT];
    // Selected material pointers consumed by the mesh backend.
    void* flatVisible;
    void* flatOccluded;
    bool resolveAttempted;
    bool materialCreationAttempted;
    bool functionsReady;
    bool materialsReady;
    unsigned int resolveFailures;
    unsigned int createFailures;
    unsigned int presetFallbacks;
};

static MaterialManagerState g_materialManager{};
static int g_chamsVisibleMaterialPreset = CHAMS_MATERIAL_SHADED;
static int g_chamsOccludedMaterialPreset = CHAMS_MATERIAL_FLAT;
alignas(16) static BYTE g_materialKv3Storage[CHAMS_MATERIAL_COUNT * 2][0x200]{};

static void ZeroMaterialStorage(BYTE* destination, SIZE_T size)
{
    volatile BYTE* cursor = destination;
    for (SIZE_T i = 0; i < size; ++i)
        cursor[i] = 0;
}

static const char kFlatVisibleVmat[] = R"CASMAT(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
 shader = "csgo_unlitgeneric.vfx"
 F_PAINT_VERTEX_COLORS = 1
 F_TRANSLUCENT = 1
 F_BLEND_MODE = 1
 g_vColorTint = [1.0, 1.0, 1.0, 1.0]
 g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
 g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
 g_tTintMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
}
)CASMAT";

static const char kFlatOccludedVmat[] = R"CASMAT(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
 shader = "csgo_unlitgeneric.vfx"
 F_PAINT_VERTEX_COLORS = 1
 F_TRANSLUCENT = 1
 F_BLEND_MODE = 1
 F_DISABLE_Z_BUFFERING = 1
 F_DISABLE_Z_PREPASS = 1
 F_DISABLE_Z_WRITE = 1
 g_vColorTint = [1.0, 1.0, 1.0, 1.0]
 g_tColor = resource:"materials/default/default_mask_tga_fde710a5.vtex"
 g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
 g_tTintMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
}
)CASMAT";

static const char kShadedVisibleVmat[] = R"CASMAT(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
 shader = "csgo_character.vfx"
 F_PAINT_VERTEX_COLORS = 1
 F_TRANSLUCENT = 1
 F_BLEND_MODE = 1
 g_vColorTint = [1.0, 1.0, 1.0, 1.0]
 g_bFogEnabled = 0
 g_flMetalness = 0.0
 g_flRoughness = 0.28
 g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
 g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
 g_tMetalness = resource:"materials/default/default_metal_tga_8fbc2820.vtex"
}
)CASMAT";

static const char kShadedOccludedVmat[] = R"CASMAT(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
 shader = "csgo_character.vfx"
 F_PAINT_VERTEX_COLORS = 1
 F_TRANSLUCENT = 1
 F_BLEND_MODE = 1
 F_DISABLE_Z_BUFFERING = 1
 F_DISABLE_Z_PREPASS = 1
 F_DISABLE_Z_WRITE = 1
 g_vColorTint = [1.0, 1.0, 1.0, 1.0]
 g_bFogEnabled = 0
 g_flMetalness = 0.0
 g_flRoughness = 0.28
 g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
 g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
 g_tMetalness = resource:"materials/default/default_metal_tga_8fbc2820.vtex"
}
)CASMAT";

static const char kMetallicVisibleVmat[] = R"CASMAT(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
 shader = "csgo_complex.vfx"
 F_PAINT_VERTEX_COLORS = 1
 F_TRANSLUCENT = 1
 F_BLEND_MODE = 1
 g_vColorTint = [1.0, 1.0, 1.0, 1.0]
 g_flMetalness = 1.0
 g_flRoughness = 0.18
 g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
 g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
 g_tMetalness = resource:"materials/default/default_metal_tga_8fbc2820.vtex"
}
)CASMAT";

static const char kMetallicOccludedVmat[] = R"CASMAT(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
 shader = "csgo_complex.vfx"
 F_PAINT_VERTEX_COLORS = 1
 F_TRANSLUCENT = 1
 F_BLEND_MODE = 1
 F_DISABLE_Z_BUFFERING = 1
 F_DISABLE_Z_PREPASS = 1
 F_DISABLE_Z_WRITE = 1
 g_vColorTint = [1.0, 1.0, 1.0, 1.0]
 g_flMetalness = 1.0
 g_flRoughness = 0.18
 g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
 g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
 g_tMetalness = resource:"materials/default/default_metal_tga_8fbc2820.vtex"
}
)CASMAT";

static const char kGlowVisibleVmat[] = R"CASMAT(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
 shader = "csgo_complex.vfx"
 F_SELF_ILLUM = 1
 F_PAINT_VERTEX_COLORS = 1
 F_TRANSLUCENT = 1
 F_BLEND_MODE = 1
 g_vColorTint = [1.0, 1.0, 1.0, 1.0]
 g_vSelfIllumTint = [1.0, 1.0, 1.0, 1.0]
 g_flSelfIllumScale = [4.0, 4.0, 4.0, 4.0]
 g_flSelfIllumBrightness = [3.0, 3.0, 3.0, 3.0]
 g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
 g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
 g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
}
)CASMAT";

static const char kGlowOccludedVmat[] = R"CASMAT(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
 shader = "csgo_complex.vfx"
 F_SELF_ILLUM = 1
 F_PAINT_VERTEX_COLORS = 1
 F_TRANSLUCENT = 1
 F_BLEND_MODE = 1
 F_DISABLE_Z_BUFFERING = 1
 F_DISABLE_Z_PREPASS = 1
 F_DISABLE_Z_WRITE = 1
 g_vColorTint = [1.0, 1.0, 1.0, 1.0]
 g_vSelfIllumTint = [1.0, 1.0, 1.0, 1.0]
 g_flSelfIllumScale = [4.0, 4.0, 4.0, 4.0]
 g_flSelfIllumBrightness = [3.0, 3.0, 3.0, 3.0]
 g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
 g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
 g_tSelfIllumMask = resource:"materials/default/default_mask_tga_fde710a5.vtex"
}
)CASMAT";

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

static int ClampChamsMaterialPreset(int preset)
{
    if (preset < 0) return 0;
    if (preset >= CHAMS_MATERIAL_COUNT) return CHAMS_MATERIAL_COUNT - 1;
    return preset;
}

static bool MaterialRangeCommittedReadable(const void* address, SIZE_T bytes)
{
    if (!address || bytes == 0)
        return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(address, &mbi, sizeof(mbi)) != sizeof(mbi) ||
        mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
        return false;
    const ULONG_PTR start = reinterpret_cast<ULONG_PTR>(address);
    const ULONG_PTR region = reinterpret_cast<ULONG_PTR>(mbi.BaseAddress);
    if (start < region)
        return false;
    const SIZE_T offset = static_cast<SIZE_T>(start - region);
    return offset <= mbi.RegionSize && bytes <= mbi.RegionSize - offset;
}

static bool RuntimeMaterialPointerUsable(void* material)
{
    if (!MaterialRangeCommittedReadable(material, sizeof(void*)))
        return false;
    void* vtable = *reinterpret_cast<void**>(material);
    if (!MaterialRangeCommittedReadable(vtable, sizeof(void*)))
        return false;
    void* firstVirtual = *reinterpret_cast<void**>(vtable);
    return IsExecutable(firstVirtual);
}

static bool SelectedVisibleChamsMaterialUsable()
{
    return g_materialManager.materialsReady &&
        RuntimeMaterialPointerUsable(g_materialManager.flatVisible);
}

static bool SelectedOccludedChamsMaterialUsable()
{
    return g_materialManager.materialsReady &&
        RuntimeMaterialPointerUsable(g_materialManager.flatOccluded);
}

static void RefreshSelectedChamsMaterials()
{
    RuntimeMaterialPair& flat = g_materialManager.presets[CHAMS_MATERIAL_FLAT];
    const int visiblePreset = ClampChamsMaterialPreset(
        g_chamsVisibleMaterialPreset);
    const int occludedPreset = ClampChamsMaterialPreset(
        g_chamsOccludedMaterialPreset);
    void* visible = g_materialManager.presets[visiblePreset].visible;
    void* occluded = g_materialManager.presets[occludedPreset].occluded;

    if (!RuntimeMaterialPointerUsable(visible))
    {
        if (visiblePreset != CHAMS_MATERIAL_FLAT &&
            RuntimeMaterialPointerUsable(flat.visible))
        {
            visible = flat.visible;
            ++g_materialManager.presetFallbacks;
        }
        else
            visible = nullptr;
    }
    if (!RuntimeMaterialPointerUsable(occluded))
    {
        if (occludedPreset != CHAMS_MATERIAL_FLAT &&
            RuntimeMaterialPointerUsable(flat.occluded))
        {
            occluded = flat.occluded;
            ++g_materialManager.presetFallbacks;
        }
        else
            occluded = nullptr;
    }
    g_materialManager.flatVisible = visible;
    g_materialManager.flatOccluded = occluded;
}

static bool ResolveMaterialManagerFunctions()
{
    if (g_materialManager.functionsReady)
        return true;

    HMODULE tier0 = GetModuleHandleW(L"tier0.dll");
    HMODULE materialSystemModule = GetModuleHandleW(L"materialsystem2.dll");
    HMODULE clientModule = GetModuleHandleW(L"client.dll");
    // Module absence is a transient lifecycle condition and must not consume
    // the structural one-shot signature attempt.
    if (!tier0 || !materialSystemModule || !clientModule)
        return false;
    if (g_materialManager.resolveAttempted)
        return false;
    g_materialManager.resolveAttempted = true;

    static const char kLoadKv3Export[] =
        "?LoadKV3@@YA_NPEAVKeyValues3@@PEAVCUtlString@@PEBDAEBUKV3ID_t@@2I@Z";
    g_materialManager.loadKv3 = reinterpret_cast<LoadKv3Fn>(
        GetProcAddress(tier0, kLoadKv3Export));
    if (!g_materialManager.loadKv3)
    {
        static const int kLoadKv3Pattern[] = {
            0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x70,
            0x4C,0x8B,0xD1,0x48,0xC7,0xC0,0xFF,0xFF,0xFF,0xFF,
            0x48,0xFF,0xC0,0x41,0x80,0x3C,0x00,0x00,0x75,0xF6
        };
        BYTE* p = FindUniquePattern(tier0, kLoadKv3Pattern,
            sizeof(kLoadKv3Pattern)/sizeof(kLoadKv3Pattern[0]));
        if (p && IsExecutable(p))
            g_materialManager.loadKv3 = reinterpret_cast<LoadKv3Fn>(p);
    }

    static const int kSetTypeKv3Pattern[] = {
        0x40,0x53,0x48,0x83,0xEC,-1,0x80,0xFA
    };
    BYTE* setType = FindUniquePattern(clientModule, kSetTypeKv3Pattern,
        sizeof(kSetTypeKv3Pattern)/sizeof(kSetTypeKv3Pattern[0]));
    if (setType && IsExecutable(setType))
        g_materialManager.setTypeKv3 = reinterpret_cast<SetTypeKv3Fn>(setType);

    static const int kCreateMaterialPattern[] = {
        0x48,0x89,0x5C,0x24,-1,0x48,0x89,0x6C,0x24,-1,
        0x48,0x89,0x74,0x24,-1,0x48,0x89,0x7C,0x24,-1,
        0x41,0x56,0x48,0x81,0xEC,-1,-1,-1,-1,
        0x48,0x8B,0x05,-1,-1,-1,-1,0x48,0x8B,0xF2
    };
    BYTE* create = FindUniquePattern(materialSystemModule,
        kCreateMaterialPattern,
        sizeof(kCreateMaterialPattern)/sizeof(kCreateMaterialPattern[0]));
    if (create && IsExecutable(create))
        g_materialManager.createMaterial =
            reinterpret_cast<CreateRuntimeMaterialFn>(create);

    auto createInterface = reinterpret_cast<CreateInterfaceFn>(
        GetProcAddress(materialSystemModule, "CreateInterface"));
    if (createInterface)
        g_materialManager.materialSystem =
            createInterface("VMaterialSystem2_001", nullptr);

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
    if (!g_materialManager.loadKv3(kv3, nullptr, vmat,
        &genericId, nullptr, 0U))
        return nullptr;
    MaterialStrongHandle handle;
    handle.binding = nullptr;
    g_materialManager.createMaterial(g_materialManager.materialSystem, &handle,
        name, kv3, 0U, 1U);
    if (!handle.binding || !MaterialRangeCommittedReadable(handle.binding,
        sizeof(MaterialResourceBinding)) ||
        !RuntimeMaterialPointerUsable(handle.binding->data))
        return nullptr;
    return handle.binding->data;
}

static bool EnsureMaterialManagerReady()
{
    if (g_materialManager.materialsReady)
    {
        RuntimeMaterialPair& flat =
            g_materialManager.presets[CHAMS_MATERIAL_FLAT];
        g_materialManager.materialsReady =
            RuntimeMaterialPointerUsable(flat.visible) &&
            RuntimeMaterialPointerUsable(flat.occluded);
        RefreshSelectedChamsMaterials();
        if (g_materialManager.materialsReady)
            return true;
    }

    if (!ResolveMaterialManagerFunctions())
        return false;
    if (g_materialManager.materialCreationAttempted)
    {
        RefreshSelectedChamsMaterials();
        return false;
    }
    g_materialManager.materialCreationAttempted = true;

    for (int i = 0; i < CHAMS_MATERIAL_COUNT; ++i)
    {
        const MaterialPresetSpec& spec = kMaterialPresetSpecs[i];
        g_materialManager.presets[i].visible = CreateRuntimeMaterial(
            spec.visibleName, spec.visibleVmat,
            g_materialKv3Storage[i * 2], 0x200);
        g_materialManager.presets[i].occluded = CreateRuntimeMaterial(
            spec.occludedName, spec.occludedVmat,
            g_materialKv3Storage[i * 2 + 1], 0x200);
        if (!g_materialManager.presets[i].visible ||
            !g_materialManager.presets[i].occluded)
            ++g_materialManager.createFailures;
    }

    RuntimeMaterialPair& flat = g_materialManager.presets[CHAMS_MATERIAL_FLAT];
    g_materialManager.materialsReady =
        RuntimeMaterialPointerUsable(flat.visible) &&
        RuntimeMaterialPointerUsable(flat.occluded);
    RefreshSelectedChamsMaterials();
    return g_materialManager.materialsReady;
}

static void ResetMaterialManagerState()
{
    for (int i = 0; i < CHAMS_MATERIAL_COUNT; ++i)
    {
        g_materialManager.presets[i].visible = nullptr;
        g_materialManager.presets[i].occluded = nullptr;
    }
    g_materialManager.flatVisible = nullptr;
    g_materialManager.flatOccluded = nullptr;
    g_materialManager.resolveAttempted = false;
    g_materialManager.materialCreationAttempted = false;
    g_materialManager.materialsReady = false;
    g_materialManager.functionsReady = false;
    g_materialManager.materialSystem = nullptr;
    g_materialManager.loadKv3 = nullptr;
    g_materialManager.createMaterial = nullptr;
    g_materialManager.setTypeKv3 = nullptr;
}
