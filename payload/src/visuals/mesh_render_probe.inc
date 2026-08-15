// Source 2 scene-system mesh renderer backend.
//
// The hook is intentionally fail-closed:
//  * DrawObject must match one unique executable signature.
//  * the overwritten 12-byte prologue must be relocation-safe.
//  * mesh/scene-object slots are range checked before every access.
//  * only handles already published by the frame-stage target registry are
//    modified, keeping schema/entity selection out of the render hot path.

using SceneDrawObjectFn = void (__fastcall*)(void*, void*, void*, int,
    void*, void*, void*, void*);
using FlushInstructionCacheFn = BOOL (WINAPI*)(HANDLE, const void*, SIZE_T);

struct MeshRenderBackendState {
    BYTE* drawObjectTarget;
    SceneDrawObjectFn originalDrawObject;
    BYTE originalPrologue[12];
    bool resolved;
    bool installed;
    unsigned int drawCalls;
    unsigned int targetDraws;
    unsigned int visiblePasses;
    unsigned int layoutRejects;
};

static MeshRenderBackendState g_meshRenderBackend{};

#pragma section(".cpxtrm", read, execute)
__declspec(allocate(".cpxtrm")) static BYTE g_drawObjectTrampoline[32] = { 0xCC };

static bool FlushExecutableRange(const void* address, SIZE_T size)
{
    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    if (!kernel)
        return false;
    auto flush = reinterpret_cast<FlushInstructionCacheFn>(
        GetProcAddress(kernel, "FlushInstructionCache"));
    if (!flush)
        return false;
    // -1 is the current-process pseudo handle on Windows.
    return flush(reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1)),
        address, size) != 0;
}

static void WriteAbsoluteJump(BYTE* destination, const void* target)
{
    // mov rax, imm64; jmp rax
    destination[0] = 0x48;
    destination[1] = 0xB8;
    *reinterpret_cast<unsigned long long*>(destination + 2) =
        reinterpret_cast<unsigned long long>(target);
    destination[10] = 0xFF;
    destination[11] = 0xE0;
}

static bool ResolveMeshRenderBackend()
{
    g_meshRenderBackend.drawObjectTarget = nullptr;
    g_meshRenderBackend.resolved = false;

    HMODULE sceneSystem = GetModuleHandleW(L"scenesystem.dll");
    if (!sceneSystem)
        return false;

    // The same DrawObject entry shape is used by several public CS2
    // scene-system render implementations. FindUniquePattern rejects stale or
    // ambiguous matches instead of guessing an offset.
    static const int kDrawObjectPattern[] = {
        0x48, 0x83, 0xEC, 0x48,
        0x48, 0x8B, 0x84, 0x24, -1, -1, -1, -1,
        0x48, 0x8D, 0x0D, -1, -1, -1, -1
    };

    BYTE* target = FindUniquePattern(sceneSystem, kDrawObjectPattern,
        sizeof(kDrawObjectPattern) / sizeof(kDrawObjectPattern[0]));
    if (!target || !IsExecutable(target))
        return false;

    // Exactly two complete, stack-relative instructions are overwritten.
    // Neither instruction is RIP-relative, so both can be copied verbatim into
    // the trampoline without a relocator.
    static const int kSafePrologue[] = {
        0x48, 0x83, 0xEC, 0x48,
        0x48, 0x8B, 0x84, 0x24, -1, -1, -1, -1
    };
    if (!PatternMatchesAt(target, kSafePrologue,
        sizeof(kSafePrologue) / sizeof(kSafePrologue[0])))
        return false;

    g_meshRenderBackend.drawObjectTarget = target;
    g_meshRenderBackend.resolved = true;
    return true;
}

static bool MeshDrawSlots(void* meshDraw, unsigned int* ownerHandle,
    void*** materialSlot, BYTE** colorSlot)
{
    if (!meshDraw || !ownerHandle || !materialSlot || !colorSlot ||
        !IsAccessible(meshDraw, 0x54, false))
        return false;

    BYTE* mesh = reinterpret_cast<BYTE*>(meshDraw);
    void** sceneObjectSlot = reinterpret_cast<void**>(mesh + 0x18);
    if (!IsAccessible(sceneObjectSlot, sizeof(void*), false))
        return false;
    void* sceneObject = *sceneObjectSlot;
    if (!IsAccessible(sceneObject, 0xBC, false))
        return false;

    auto* handle = reinterpret_cast<unsigned int*>(
        reinterpret_cast<BYTE*>(sceneObject) + 0xB8);
    auto** material = reinterpret_cast<void**>(mesh + 0x20);
    BYTE* color = mesh + 0x50;
    if (!IsAccessible(handle, sizeof(unsigned int), false) ||
        !IsAccessible(material, sizeof(void*), true) ||
        !IsAccessible(color, 4, true))
        return false;

    *ownerHandle = *handle;
    *materialSlot = material;
    *colorSlot = color;
    return true;
}

static void __fastcall MeshDrawObjectHook(void* a1, void* a2, void* meshDraw,
    int dataCount, void* a5, void* a6, void* a7, void* a8)
{
    SceneDrawObjectFn original = g_meshRenderBackend.originalDrawObject;
    if (!original)
        return;

    ++g_meshRenderBackend.drawCalls;
    if (!meshDraw || dataCount <= 0 || dataCount > 4096 ||
        !g_espConfig.enable || !g_espConfig.chams ||
        !g_espConfig.chamsEnemies ||
        g_espConfig.chamsStyle == 2 || g_espConfig.chamsStyle == 3)
    {
        original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);
        return;
    }

    unsigned int ownerHandle = 0xFFFFFFFFu;
    void** materialSlot = nullptr;
    BYTE* color = nullptr;
    if (!MeshDrawSlots(meshDraw, &ownerHandle, &materialSlot, &color))
    {
        ++g_meshRenderBackend.layoutRejects;
        original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);
        return;
    }

    if (!IsEnemyVisualTarget(ownerHandle))
    {
        original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);
        return;
    }

    ++g_meshRenderBackend.targetDraws;
    BYTE savedColor[4] = { color[0], color[1], color[2], color[3] };
    color[0] = g_espConfig.chamsColorVisible.r;
    color[1] = g_espConfig.chamsColorVisible.g;
    color[2] = g_espConfig.chamsColorVisible.b;
    color[3] = 255;

    ++g_meshRenderBackend.visiblePasses;
    original(a1, a2, meshDraw, dataCount, a5, a6, a7, a8);

    color[0] = savedColor[0];
    color[1] = savedColor[1];
    color[2] = savedColor[2];
    color[3] = savedColor[3];
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

    for (unsigned int i = 0; i < sizeof(g_meshRenderBackend.originalPrologue); ++i)
        g_meshRenderBackend.originalPrologue[i] = target[i];

    DWORD trampolineProtection = 0;
    if (!VirtualProtect(g_drawObjectTrampoline, sizeof(g_drawObjectTrampoline),
        PAGE_EXECUTE_READWRITE, &trampolineProtection))
        return false;
    for (unsigned int i = 0; i < 12; ++i)
        g_drawObjectTrampoline[i] = target[i];
    WriteAbsoluteJump(g_drawObjectTrampoline + 12, target + 12);
    const bool trampolineFlushed = FlushExecutableRange(
        g_drawObjectTrampoline, 24);
    DWORD ignored = 0;
    VirtualProtect(g_drawObjectTrampoline, sizeof(g_drawObjectTrampoline),
        trampolineProtection, &ignored);
    if (!trampolineFlushed)
        return false;

    g_meshRenderBackend.originalDrawObject =
        reinterpret_cast<SceneDrawObjectFn>(g_drawObjectTrampoline);

    DWORD targetProtection = 0;
    if (!VirtualProtect(target, 12, PAGE_EXECUTE_READWRITE,
        &targetProtection))
    {
        g_meshRenderBackend.originalDrawObject = nullptr;
        return false;
    }
    WriteAbsoluteJump(target, reinterpret_cast<const void*>(&MeshDrawObjectHook));
    const bool targetFlushed = FlushExecutableRange(target, 12);
    VirtualProtect(target, 12, targetProtection, &ignored);
    if (!targetFlushed)
    {
        DWORD restoreProtection = 0;
        if (VirtualProtect(target, 12, PAGE_EXECUTE_READWRITE,
            &restoreProtection))
        {
            for (unsigned int i = 0; i < 12; ++i)
                target[i] = g_meshRenderBackend.originalPrologue[i];
            FlushExecutableRange(target, 12);
            VirtualProtect(target, 12, restoreProtection, &ignored);
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
    if (VirtualProtect(target, 12, PAGE_EXECUTE_READWRITE, &protection))
    {
        for (unsigned int i = 0; i < 12; ++i)
            target[i] = g_meshRenderBackend.originalPrologue[i];
        FlushExecutableRange(target, 12);
        DWORD ignored = 0;
        VirtualProtect(target, 12, protection, &ignored);
    }
    g_meshRenderBackend.originalDrawObject = nullptr;
    g_meshRenderBackend.installed = false;
}
