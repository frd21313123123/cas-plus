from pathlib import Path
import runpy

script = Path("tools/apply_chams_patch.py")
source = script.read_text(encoding="utf-8")
old = '''    count = text.count(old)\n    if count != 1:\n        raise RuntimeError(f"expected one match, found {count}: {old[:80]!r}")\n    text = text.replace(old, new, 1)\n'''
new = '''    count = text.count(old)\n    if count == 0:\n        raise RuntimeError(f"expected at least one match, found 0: {old[:80]!r}")\n    if count == 1:\n        text = text.replace(old, new, 1)\n        return\n    # The only intentionally duplicated anchor is the cleanup loop; the\n    # UpdateBotHighlights copy is the last occurrence in dllmain.cpp.\n    if old.startswith("    int destination = 0;\\n"):\n        position = text.rfind(old)\n        text = text[:position] + new + text[position + len(old):]\n        return\n    raise RuntimeError(f"ambiguous patch anchor, found {count}: {old[:80]!r}")\n'''
if old not in source:
    raise RuntimeError("replace_once implementation changed")
script.write_text(source.replace(old, new, 1), encoding="utf-8")
runpy.run_path(str(script), run_name="__main__")
