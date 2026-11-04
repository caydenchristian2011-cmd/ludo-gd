# Ludo for Geometry Dash

A [Geode](https://geode-sdk.org) mod: a stats and collections hub for Geometry Dash, matching the Ludo launcher.

## Build

Needs the Geode CLI, Git, CMake and Visual Studio Build Tools (C++ with the Clang tools), with the Geode SDK matching your loader (currently 5.11.2):

    git clone https://github.com/caydenchristian2011-cmd/ludo-gd
    cd ludo-gd
    geode build

`geode build` also installs the mod into your selected Geode profile. The `.geode` file is written to `build/`.

## Art

All images in `resources/` are generated: `python3 tools/extract_art.py` pulls the creatures and the blossom branch out of the launcher, and `python3 tools/make_art.py` redraws every image (needs Python with Playwright). `python3 tools/mock_hub.py` lays the real images out like the hub does, to check proportions without the game. The heading font is Fraunces (SIL Open Font License).
