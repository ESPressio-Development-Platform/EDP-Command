#!/usr/bin/env python3
import os
import pathlib
import shlex
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
workspace = root.parent
compiler = os.environ.get("CXX", "c++")
extra_cxxflags = shlex.split(os.environ.get("CXXFLAGS", ""))

print("EDP-Command host contract and resource suite")

repositories = {
    "EDP-Command": root,
    "EDP-Clock": workspace / "EDP-Clock",
    "EDP-Primitives": workspace / "EDP-Primitives",
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


def compile_command(source, output):
    cmd = [compiler, "-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-Werror"]
    cmd.extend(extra_cxxflags)
    cmd.append(str(source))
    for include_path in include_paths:
        cmd.extend(["-I", str(include_path)])
    cmd.extend(["-o", str(output)])
    return cmd


def compile_and_run(source_name, binary_name):
    source = root / "tests" / "host" / source_name
    with tempfile.TemporaryDirectory() as td:
        binary = pathlib.Path(td) / binary_name
        subprocess.run(compile_command(source, binary), check=True)
        subprocess.run([str(binary)], check=True)


def require_compile_failure(source_name, label):
    source = root / "tests" / "host" / source_name
    with tempfile.TemporaryDirectory() as td:
        binary = pathlib.Path(td) / "must-not-compile"
        result = subprocess.run(
            compile_command(source, binary),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if result.returncode == 0:
            raise SystemExit(f"FAIL: {label} unexpectedly compiled")
    print(f"PASS: {label}")


compile_and_run("command_runtime.cpp", "command-runtime")
print("PASS: host lifecycle contract")

compile_and_run("execution_scope.cpp", "command-execution-scope")
print("PASS: F4 execution-domain scope contract")

print("Compile-time invalid-plan contracts are enforced by ResourcePlan static_asserts.")

require_compile_failure("invalid_missing_handler.cpp", "missing handler rejected at compile time")
require_compile_failure("invalid_duplicate_handler.cpp", "duplicate handler rejected at compile time")
require_compile_failure("invalid_scope_void_operation.cpp", "void scoped operation rejected at compile time")
require_compile_failure("invalid_scope_throwing_operation.cpp", "throwing scoped operation rejected at compile time")
require_compile_failure("invalid_command_missing_schema.cpp", "anonymous Command rejected at compile time")
require_compile_failure("invalid_request_missing_schema.cpp", "non-schema Request rejected at compile time")

compile_and_run("resource_measurement.cpp", "command-resource-measurement")
print("PASS: deterministic host resource measurement")
