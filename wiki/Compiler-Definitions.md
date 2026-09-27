# Compiler Definitions

EDP-Command currently defines **no EDP-owned production compiler/build definitions or conditional-compilation switches**.

The maintained PlatformIO demonstrations select C++20 through build configuration (`-std=gnu++20` while removing older language-standard flags). ESP-IDF demos also request `cxx_std_20` from their component CMake file. These are build-language settings, not runtime Command feature switches.

SDK/framework/platform macros may exist transitively while compiling Arduino-ESP32 or ESP-IDF, but the current Command production headers do not use them to conditionally alter the Command contract.

Test/resource measurement source uses template plan choices rather than preprocessor capacity definitions.