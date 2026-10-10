#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
JUCE_DIR="${JUCE_DIR:-$ROOT/../JUCE}"
cmake -S "$ROOT" -B "$ROOT/build-macos" -G Xcode -DJUCE_DIR="$JUCE_DIR" '-DDUCK_FORMATS=VST3;AU;Standalone' -DRICH_ENABLE_LICENSE=OFF -DDUCK_BUILD_UI_TESTS=ON '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build "$ROOT/build-macos" --config Release --target RichPocket_VST3 RichPocket_AU RichPocket_Standalone RichDSPTest RichIntegrationTest
ctest --test-dir "$ROOT/build-macos" -C Release --output-on-failure
