# Tooling Reference

EDP-Command currently contains **no substantive maintained compiler, generator, validator, decompiler, schema processor or CLI tooling** requiring a first-class tooling API reference.

`tests/run_tests.py` is the maintained test runner rather than product/toolchain functionality. It verifies host lifecycle and F4 scope contracts, compile-fail contracts and deterministic resource measurement against coherent sibling EDP repositories. `CXX` selects the compiler and `CXXFLAGS` supplies optional additional compiler/linker flags, including sanitizer configurations.

`.github/workflows/validation.yml` is the maintained representative CI definition. It describes GCC/Clang host validation, ASan/UBSan host validation, and PlatformIO Arduino/ESP-IDF demo builds. GitHub Actions execution is not authoritative under current Policy; the same materially relevant executable surfaces are run on RPI400 for acceptance.

The PlatformIO and Arduino IDE projects under `demos/` are demonstration/build assets and are documented by [Testing and Resources](Testing-and-Resources).

If future maintained tooling is introduced, this page and the Reference Index must be expanded in the same tranche.