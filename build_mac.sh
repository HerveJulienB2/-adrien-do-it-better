#!/bin/bash
# Compile ADRIEN DO IT BETTER en VST3 (Apple Silicon + Intel) et l'installe
# dans ~/Library/Audio/Plug-Ins/VST3
set -e
cd "$(dirname "$0")"

if ! xcode-select -p >/dev/null 2>&1; then
  echo "→ Il faut les outils de développement Apple : lance 'xcode-select --install' puis relance ce script."
  exit 1
fi
if ! command -v cmake >/dev/null 2>&1; then
  echo "→ Il faut CMake : 'brew install cmake' (ou https://cmake.org/download) puis relance ce script."
  exit 1
fi

# Si tu as déjà JUCE en local : JUCE_DIR=/chemin/vers/JUCE ./build_mac.sh
EXTRA=""
if [ -n "$JUCE_DIR" ]; then EXTRA="-DJUCE_DIR=$JUCE_DIR"; fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release $EXTRA
cmake --build build --config Release -j "$(sysctl -n hw.ncpu)"

echo ""
echo "✔ ADRIEN DO IT BETTER.vst3 est installé dans ~/Library/Audio/Plug-Ins/VST3"
echo "  Relance ton DAW (ou fais un rescan des plugins)."
