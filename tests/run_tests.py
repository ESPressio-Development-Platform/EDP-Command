#!/usr/bin/env python3
import os
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
workspace = root.parent
source = root / "tests" / "host" / "command_runtime.cpp"
compiler = os.environ.get("CXX", "c++")

print("EDP-Command host contract suite")

repositories = {
    "EDP-Command": root,
    "EDP-Clock": workspace / "EDP-Clock",
    "EDP-Memory": workspace / "EDP-Memory",
    "EDP-System": workspace / "EDP-System",
    "EDP-Platform": workspace / "EDP-Platform",
}

missing = [name for name, path in repositories.items() if not (path / "src").is_dir()]
if missing:
    raise SystemExit(
        "Missing sibling repositories: " + ", ".join(missing) +
        ". Clone them beside EDP-Command."
    )

include_paths = [path / "src" for path in repositories.values()]

with tempfile.TemporaryDirectory() as td:
    binary = pathlib.Path(td) / "command-runtime"
    cmd = [compiler, "-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-Werror", str(source)]
    for include_path in include_paths:
        cmd.extend(["-I", str(include_path)])
    cmd.extend(["-o", str(binary)])
    subprocess.run(cmd, check=True)
    subprocess.run([str(binary)], check=True)

print("PASS: host lifecycle contract")
print("Compile-time invalid-plan contracts are enforced by ResourcePlan static_asserts.")
