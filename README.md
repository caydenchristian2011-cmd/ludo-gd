# Ludo for Geometry Dash

A [Geode](https://geode-sdk.org) mod: a stats and collections hub for Geometry Dash, matching the Ludo launcher.

## Build

Needs the Geode CLI, Git, CMake and Visual Studio Build Tools (C++ with the Clang tools), with the Geode SDK matching your loader (currently 5.11.2):

    git clone https://github.com/caydenchristian2011-cmd/ludo-gd
    cd ludo-gd
    geode build

`geode build` also installs the mod into your selected Geode profile. The `.geode` file is written to `build/`.
