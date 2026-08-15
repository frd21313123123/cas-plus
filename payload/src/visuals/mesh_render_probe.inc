// Source 2 scene-system mesh renderer backend.
//
// Current backend target: CAnimatableSceneObjectDesc::Render in
// scenesystem.dll. The entry signature and CMeshData layout mirror current
// public CS2 render implementations (July 2026), but every runtime access stays
// fail-closed so a game update falls back instead of writing guessed memory.

using SceneDrawObjectFn = bool (__fastcall*)(void*, void*, void*, int,
    void*, void*, void*, void*);
using FlushInstructionCacheFn = BOOL (WINAPI*)(HANDLE, const void*, SIZE_T);

static constexpr unsigned int kMeshDataStride = 0x68;
static constexpr unsigned int kMeshSceneObjectOffset = 0x18;
static constexpr unsigned int kMeshMaterialOffset = 0x20;
static constexpr unsigned int kMeshMaterial2Offset = 0x28;
static constexpr unsigned int kMeshColorOffset = 0x50;
static constexpr int kMaxMeshBatch = 128;
static constexpr unsigned int kDrawPatchSize = 14;

struct MeshRenderBackendState {
    BYTE* drawObjectTarget;
    SceneDrawObjectFn originalDrawObject;
    BYTE originalPrologue[kDrawPatchSize];
    bool resolved;
    bool installed;
    unsigned int drawCalls;
    unsigned int targetDraws;
    unsigned int visiblePasses;
    unsigned int occludedPasses;
    unsigned int stateRestores;
    unsigned int layoutRejects;
    unsigned int materialFallbacks;
};

static MeshRenderBackendState g_meshRenderBackend{};

#pragma section(".cpxtrm", read, execute)
__declspec(allocate(".cpxtrm")) static BYTE g_drawObjectTrampoline[64] = { 0xCC };

static bool FlushExecutableRange(const void* address, SIZE_T size)
{
    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    if (!kernel)
        return false;
    auto flush = reinterpret_cast<FlushInstructionCacheFn>(
        GetProcAddress(kernel, "FlushInstructionCache"));
    if (!flush)
        return false;
    return flush(reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1)),
        address, size) != 0;
}

static void WriteAbsoluteIndirectJump(BYTE* destination, const void* target)
{
    // jmp qword ptr [rip+0]; dq target
    // 14 bytes, clobbers no GPR and is therefore suitable for a function entry.
    destination[0] = 0xFF;
    destination[1] = 0x25;
    destination[2] = 0x00;
    destination[3] = 0x00;
    destination[4] = 0x00;
    destination[5] = 0x00;
    *reinterpret_cast<unsigned long long*>(destination + 6) =
        reinterpret_cast<unsigned long long>(target);
}

static bool ResolveMeshRenderBackend()
{
    g_meshRenderBackend.drawObjectTarget = nullptr;
    g_meshRenderBackend.resolved = false;

    HMODULE sceneSystem = GetModuleHandleW(L"scenesystem.dll");
    if (!sceneSystem)
        return false;

    // Canonical CAnimatableSceneObjectDesc::Render prologue used by current
    // CS2 signature sets. The first 14 bytes are five complete instructions:
    // mov rax,rsp; push rbx; push rdi; push r12; sub rsp,0D0h.
    static const int kDrawObjectPattern[] = {
        0x48, 0x8B, 0xC4, 0x53, 0x57, 0x41, 0x54,
        0x48, 0x81, 0xEC, 0xD0, 0x00, 0x00, 0x00, 0x49, 0x63
    };
    BYTE* target = FindUniquePattern(sceneSystem, kDrawObjectPattern,
        sizeof(kDrawObjectPattern) / sizeof(kDrawObjectPattern[0]));
    if (!target || !IsExecutable(target))
        return false;

    static const int kSafePrologue[] = {
        0x48, 0x8B, 0xC4, 0x53, 0x57, 0x41, 0x54,
        0x48, 0x81, 0xEC, 0xD0, 0x00, 0x00, 0x00
    };
    if (!PatternMatchesAt(target, kSafePrologue,
        sizeof(kSafePrologue) / sizeof(kSafePrologue[0])))
        return false;

    g_meshRenderBackend.drawObjectTarget = target;
    g_meshRenderBackend.resolved = true;
    return true;
}

static unsigned int EnemyOwnerHandleFromSceneObject(void* sceneObject)
{
    if (!sceneObject || !IsAccessible(sceneObject, 0xF4, false))
        return 0xFFFFFFFFu;

    static const unsigned int kOwnerOffsets[] = {
        0x98, 0xA0, 0xA8, 0xB0, 0xB8, 0xC0,
        0xC8, 0xD0, 0xD8, 0xE0, 0xE8, 0xF0
    };
    BYTE* base = reinterpret_cast<BYTE*>(sceneObject);
    for (unsigned int i = 0;
        i < sizeof(kOwnerOffsets) / sizeof(kOwnerOffsets[0]); ++i)
    {
        auto* handle = reinterpret_cast<unsigned int*>(base + kOwnerOffsets[i]);
        if (!IsAccessible(handle, sizeof(unsigned int), false))
            continue;
        if (IsEnemyVisualTarget(*handle))
            return *handle;
    }
    return 0xFFFFFFFFu;
}

static bool MeshBatchBelongsToEnemy(void* meshDraw, int dataCount,
    unsigned int* ownerHandle)
{
    if (!meshDraw || dataCount <= 0 || dataCount > kMaxMeshBatch ||
        !ownerHandle)
        return false;
    BYTE* first = reinterpret_cast<BYTE*>(meshDraw);
    if (!IsAccessible(first, kMeshDataStride, false))
        return false;
    auto** sceneSlot = reinterpret_cast<void**>(first + kMeshSceneObjectOffset);
    if (!IsAccessible(sceneSlot, sizeof(void*), false) || !*sceneSlot)
        return false;
    const unsigned int handle = EnemyOwnerHandleFromSceneObject(*sceneSlot);
    if (handle == 0xFFFFFFFFu)
        return false;
    *ownerHandle = handle;
    return true;
}

struct SavedMeshState {
    void* material;
    void* material2;
    BYTE color[4];
};

static bool CaptureMeshBatch(void* meshDraw, int dataCount,
    SavedMeshState* saved)
{
    if (!meshDraw || !saved || dataCount <= 0 || dataCount > kMaxMeshBatch)
        return false;
    BYTE* base = reinterpret_cast<BYTE*>(meshDraw);
    for (int i = 0; i < dataCount; ++i)
    {
        BYTE* mesh = base + static_cast<SIZE_T>(i) * kMeshDataStride;
        if (!IsAccessible(mesh, kMeshDataStride, true))
            return false;
        void** material = reinterpret_cast<void**>(mesh + kMeshMaterialOffset);
        void** material2 = reinterpret_cast<void**>(mesh + kMeshMaterial2Offset);
        BYTE* color = mesh + kMeshColorOffset;
        if (!IsAccessible(material, sizeof(void*), true) ||
            !IsAccessible(material2, sizeof(void*), true) ||
            !IsAccessible(color, 4, true))
            return false;
        saved[i].material = *material;
        saved[i].material2 = *material2;
        saved[i].color[0] = color[0];
        saved[i].color[1] = color[1];
        saved[i].color[2] = color[2];
        saved[i].color[3] = color[3];
    }
    return true;
}

static void ApplyMeshBatchMaterial(void* meshDraw, int dataCount,
    void* materialValue, const RGBVal& colorValue)
{
    BYTE* base = reinterpret_cast<BYTE*>(meshDraw);
    for (int i = 0; i < dataCount; ++i)
    {
        BYTE* mesh = base + static_cast<SIZE_T>(i) * kMeshDataStride;
        *reinterpret_cast<void**>(mesh + kMeshMaterialOffset) = materialValue;
        *reinterpret_cast<void**>(mesh + kMeshMaterial2Offset) = materialValue;
        BYTE* color = mesh + kMeshColorOffset;
        color[0] = colorValue.r;
        color[1] = colorValue.g;
        color[2] = colorValue.b;
        color[3] = 255;
    }
}

static void RestoreMeshBatch(void* meshDraw, int dataCount,
    const SavedMeshState* saved)
{
    if (!meshDraw || !saved || dataCount <= 0 || dataCount > kMaxMeshBatch)
        return;
    BYTE* base = reinterpret_cast<BYTE*>(meshDraw);
    for (int i = 0; i < dataCount; ++i)
    {
        BYTE* mesh = base + static_cast<SIZE_T>(i) * kMeshDataStride;
        if (!IsAccessible(mesh, kMeshDataStride, true))
            continue;
        *reinterpret_cast<void**>(mesh + kMeshMaterialOffset) = saved[i].material;
        *reinterpret_cast<void**>(mesh + kMeshMaterial2Offset) = saved[i].material2;
        BYTE* color = mesh + kMeshColorOffset;
        color[0] = saved[i].color[0];
        color[1] = saved[i].color[1];
        color[2] = saved[i].color[2];
        color[3] = saved[i].color[3];
    }
    ++g_meshRenderBackend.stateRestores;
}

static bool __fastcall MeshDrawObjectHook(void* a1, void* a2, void* meshDraw,
    int dataCount, void* a5, void* a6, void* a7, void* a8)
{
    SceneDrawObjectFn original = g_meshRenderBackend.originalDrawObject;
    if (!original)
        return false;

    ++g_meshRenderBackend.drawCalls;
    if (!meshDraw || dataCount <= 0 || dataCount > kMaxMeshBatch ||
        !g_espConfig.enable || !g_espConfig.chams ||
        !g_espConfig.chamsEnemies || g_espConfig.chamsStyle == 3)
        return original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);

    unsigned int ownerHandle = 0xFFFFFFFFu;
    if (!MeshBatchBelongsToEnemy(meshDraw, dataCount, &ownerHandle))
        return original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);

    ++g_meshRenderBackend.targetDraws;
    SavedMeshState saved[kMaxMeshBatch]{};
    if (!CaptureMeshBatch(meshDraw, dataCount, saved))
    {
        ++g_meshRenderBackend.layoutRejects;
        return original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);
    }

    const bool wantVisible = g_espConfig.chamsStyle == 0 ||
        g_espConfig.chamsStyle == 1;
    const bool wantOccluded = g_espConfig.chamsStyle == 0 ||
        g_espConfig.chamsStyle == 2;

    const bool materialsReady = g_materialManager.materialsReady &&
        g_materialManager.flatVisible && g_materialManager.flatOccluded &&
        IsAccessible(g_materialManager.flatVisible, sizeof(void*), false) &&
        IsAccessible(g_materialManager.flatOccluded, sizeof(void*), false);

    if (!materialsReady)
    {
        ++g_meshRenderBackend.materialFallbacks;
        if (wantVisible)
        {
            BYTE* base = reinterpret_cast<BYTE*>(meshDraw);
            for (int i = 0; i < dataCount; ++i)
            {
                BYTE* color = base + static_cast<SIZE_T>(i) *
                    kMeshDataStride + kMeshColorOffset;
                color[0] = g_espConfig.chamsColorVisible.r;
                color[1] = g_espConfig.chamsColorVisible.g;
                color[2] = g_espConfig.chamsColorVisible.b;
                color[3] = 255;
            }
            ++g_meshRenderBackend.visiblePasses;
            const bool result = original(a1, a2, meshDraw, dataCount,
                a5, a6, a7, a8);
            RestoreMeshBatch(meshDraw, dataCount, saved);
            return result;
        }
        RestoreMeshBatch(meshDraw, dataCount, saved);
        return original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);
    }

    bool result = false;
    bool drew = false;
    if (wantOccluded)
    {
        ApplyMeshBatchMaterial(meshDraw, dataCount,
            g_materialManager.flatOccluded, g_espConfig.chamsColorOccluded);
        ++g_meshRenderBackend.occludedPasses;
        result = original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);
        drew = true;
    }

    if (wantVisible)
    {
        ApplyMeshBatchMaterial(meshDraw, dataCount,
            g_materialManager.flatVisible, g_espConfig.chamsColorVisible);
        ++g_meshRenderBackend.visiblePasses;
        result = original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);
        drew = true;
    }

    RestoreMeshBatch(meshDraw, dataCount, saved);
    if (!drew)
        return original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);
    return result;
}

static bool InstallMeshRenderBackend()
{
    if (g_meshRenderBackend.installed)
        return true;
    if (!g_meshRenderBackend.resolved && !ResolveMeshRenderBackend())
        return false;

    BYTE* target = g_meshRenderBackend.drawObjectTarget;
    if (!target || !IsExecutable(target))
        return false;

    for (unsigned int i = 0; i < kDrawPatchSize; ++i)
        g_meshRenderBackend.originalPrologue[i] = target[i];

    DWORD trampolineProtection = 0;
    if (!VirtualProtect(g_drawObjectTrampoline, sizeof(g_drawObjectTrampoline),
        PAGE_EXECUTE_READWRITE, &trampolineProtection))
        return false;
    for (unsigned int i = 0; i < kDrawPatchSize; ++i)
        g_drawObjectTrampoline[i] = target[i];
    WriteAbsoluteIndirectJump(g_drawObjectTrampoline + kDrawPatchSize,
        target + kDrawPatchSize);
    const bool trampolineFlushed = FlushExecutableRange(
        g_drawObjectTrampoline, kDrawPatchSize * 2);
    DWORD ignored = 0;
    VirtualProtect(g_drawObjectTrampoline, sizeof(g_drawObjectTrampoline),
        trampolineProtection, &ignored);
    if (!trampolineFlushed)
        return false;

    // Spell out the first byte address; MSVC does not permit a direct
    // reinterpret_cast from an array type to a function pointer.
    g_meshRenderBackend.originalDrawObject = reinterpret_cast<SceneDrawObjectFn>(
        reinterpret_cast<void*>(&g_drawObjectTrampoline[0]));

    DWORD targetProtection = 0;
    if (!VirtualProtect(target, kDrawPatchSize, PAGE_EXECUTE_READWRITE,
        &targetProtection))
    {
        g_meshRenderBackend.originalDrawObject = nullptr;
        return false;
    }
    WriteAbsoluteIndirectJump(target,
        reinterpret_cast<const void*>(&MeshDrawObjectHook));
    const bool targetFlushed = FlushExecutableRange(target, kDrawPatchSize);
    VirtualProtect(target, kDrawPatchSize, targetProtection, &ignored);
    if (!targetFlushed)
    {
        DWORD restoreProtection = 0;
        if (VirtualProtect(target, kDrawPatchSize, PAGE_EXECUTE_READWRITE,
            &restoreProtection))
        {
            for (unsigned int i = 0; i < kDrawPatchSize; ++i)
                target[i] = g_meshRenderBackend.originalPrologue[i];
            FlushExecutableRange(target, kDrawPatchSize);
            VirtualProtect(target, kDrawPatchSize, restoreProtection, &ignored);
        }
        g_meshRenderBackend.originalDrawObject = nullptr;
        return false;
    }

    g_meshRenderBackend.installed = true;
    return true;
}

static void RemoveMeshRenderBackend()
{
    if (!g_meshRenderBackend.installed ||
        !g_meshRenderBackend.drawObjectTarget)
    {
        g_meshRenderBackend.originalDrawObject = nullptr;
        g_meshRenderBackend.installed = false;
        return;
    }

    BYTE* target = g_meshRenderBackend.drawObjectTarget;
    DWORD protection = 0;
    if (VirtualProtect(target, kDrawPatchSize, PAGE_EXECUTE_READWRITE,
        &protection))
    {
        for (unsigned int i = 0; i < kDrawPatchSize; ++i)
            target[i] = g_meshRenderBackend.originalPrologue[i];
        FlushExecutableRange(target, kDrawPatchSize);
        DWORD ignored = 0;
        VirtualProtect(target, kDrawPatchSize, protection, &ignored);
    }
    g_meshRenderBackend.originalDrawObject = nullptr;
    g_meshRenderBackend.installed = false;
}
