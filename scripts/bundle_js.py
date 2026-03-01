"""PlatformIO pre-build script: concatenate JS source files into bundle.js."""
import os

Import("env")  # noqa: F821 — provided by PlatformIO SCons

# JS files in dependency order
JS_FILES = [
    "protocol_manager.js",
    "can_analyzer.js",
    "dev_tools.js",
    "app.js",
]

project_dir = env.subst("$PROJECT_DIR")  # noqa: F821
src_dir = os.path.join(project_dir, "web_src")
out_file = os.path.join(project_dir, "data", "web", "bundle.js")


def bundle():
    parts = []
    for name in JS_FILES:
        path = os.path.join(src_dir, name)
        if not os.path.isfile(path):
            print(f"  WARNING: {path} not found, skipping")
            continue
        with open(path, "r") as f:
            parts.append(f"// === {name} ===\n{f.read()}")
    os.makedirs(os.path.dirname(out_file), exist_ok=True)
    with open(out_file, "w") as f:
        f.write("\n".join(parts) + "\n")
    total = os.path.getsize(out_file)
    print(f"  Bundled {len(parts)} JS files -> bundle.js ({total} bytes)")


bundle()
