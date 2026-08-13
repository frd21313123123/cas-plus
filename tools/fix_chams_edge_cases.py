from pathlib import Path

path = Path("payload/src/dllmain.cpp")
text = path.read_text(encoding="utf-8")

replacements = [
    (
'''    const bool visiblePass = g_espConfig.enable && g_espConfig.chams &&
        allowVisiblePass && g_espConfig.chamsStyle != 2 &&
        g_espConfig.chamsStyle != 3;
''',
'''    const bool visiblePass = g_espConfig.enable && g_espConfig.chams &&
        allowVisiblePass && g_espConfig.chamsStyle != 2 &&
        (!allowThroughWallPass || g_espConfig.chamsStyle != 3);
'''),
    (
'''    const bool glowPass = g_espConfig.enable &&
        (g_espConfig.glow || g_espConfig.chamsStyle == 3) &&
        allowThroughWallPass;
''',
'''    const bool glowPass = g_espConfig.enable &&
        (g_espConfig.glow ||
            (g_espConfig.chams && g_espConfig.chamsStyle == 3)) &&
        allowThroughWallPass;
'''),
]

for old, new in replacements:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected exactly one match, found {count}")
    text = text.replace(old, new, 1)

path.write_text(text, encoding="utf-8")
print("fixed chams edge cases")
