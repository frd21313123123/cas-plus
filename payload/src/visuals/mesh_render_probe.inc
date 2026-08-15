// Runtime probe for the Source 2 scene-system mesh draw entry point.
// This deliberately does not detour the function yet. It validates that the
// expected DrawObject prologue is unique and executable on the running build,
// giving the material backend a fail-closed capability gate.

struct MeshRenderBackendState {
    BYTE* drawObjectTarget;
    bool resolved;
};

static MeshRenderBackendState g_meshRenderBackend{};

static bool ResolveMeshRenderBackend()
{
    g_meshRenderBackend.drawObjectTarget = nullptr;
    g_meshRenderBackend.resolved = false;

    HMODULE sceneSystem = GetModuleHandleW(L"scenesystem.dll");
    if (!sceneSystem)
        return false;

    // Source 2 DrawObject entry used by current CS2 mesh-rendering projects.
    // FindUniquePattern makes this fail closed if Valve changes the function or
    // the byte sequence becomes ambiguous.
    static const int kDrawObjectPattern[] = {
        0x48, 0x83, 0xEC, 0x48,
        0x48, 0x8B, 0x84, 0x24, -1, -1, -1, -1,
        0x48, 0x8D, 0x0D, -1, -1, -1, -1
    };

    BYTE* target = FindUniquePattern(sceneSystem, kDrawObjectPattern,
        sizeof(kDrawObjectPattern) / sizeof(kDrawObjectPattern[0]));
    if (!target || !IsExecutable(target))
        return false;

    // The first 12 bytes are two whole instructions (sub rsp,48h and
    // mov rax,[rsp+disp32]). Keeping this invariant explicit is important for
    // the future trampoline implementation: no RIP-relative instruction may be
    // copied into a trampoline without relocation.
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
