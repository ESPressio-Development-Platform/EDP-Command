#!/usr/bin/env python3
import pathlib, subprocess, sys, tempfile

root = pathlib.Path(__file__).resolve().parents[1]
include = root / "src"
source = root / "tests" / "host" / "command_runtime.cpp"
compiler = "c++"

print("EDP-Command host contract suite")
print("NOTE: sibling EDP-Clock and EDP-Memory include paths are required.")
clock = root.parent / "EDP-Clock" / "src"
memory = root.parent / "EDP-Memory" / "src"

with tempfile.TemporaryDirectory() as td:
    binary = pathlib.Path(td) / "command-runtime"
    cmd = [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror", str(source),
           "-I", str(include), "-I", str(clock), "-I", str(memory), "-o", str(binary)]
    subprocess.run(cmd, check=True)
    subprocess.run([str(binary)], check=True)

print("PASS: host lifecycle contract")
print("Compile-time invalid-plan contracts are enforced by ResourcePlan static_asserts.")
