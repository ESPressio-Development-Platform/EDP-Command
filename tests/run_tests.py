#!/usr/bin/env python3
import os
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
workspace = root.parent
compiler = os.environ.get("CXX", "c++")

print("EDP-Command host contract and resource suite")

repositories = {
    "EDP-Command": root,
    "EDP-Clock": workspace / "EDP-Clock",
    "EDP-Memory": workspace / "EDP-Memory",
    "EDP-System": workspace / "EDP-System",
    "EDP-Platform": workspace / "EDP-Platform",
    "EDP-BoundedTopology": workspace / "EDP-BoundedTopology",
    "EDP-Threading": workspace / "EDP-Threading",
}

missing = [name for name, path in repositories.items() if not (path / "src").is_dir()]
if missing:
    raise SystemExit("Missing sibling repositories: " + ", ".join(missing) + ". Clone them beside EDP-Command.")

include_paths = [path / "src" for path in repositories.values()]


def compile_and_run(source_name, binary_name):
    source = root / "tests" / "host" / source_name
    with tempfile.TemporaryDirectory() as td:
        binary = pathlib.Path(td) / binary_name
        cmd = [compiler, "-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-Werror", str(source)]
        for include_path in include_paths:
            cmd.extend(["-I", str(include_path)])
        cmd.extend(["-o", str(binary)])
        subprocess.run(cmd, check=True)
        subprocess.run([str(binary)], check=True)


compile_and_run("command_runtime.cpp", "command-runtime")
print("PASS: host lifecycle contract")
print("Compile-time invalid-plan contracts are enforced by ResourcePlan static_asserts.")

compile_and_run("resource_measurement.cpp", "command-resource-measurement")
print("PASS: deterministic host resource measurement")
