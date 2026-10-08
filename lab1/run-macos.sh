#!/bin/bash
set -e
cd "$(dirname "$0")"
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"
if command -v brew >/dev/null 2>&1; then
    if brew --prefix molten-vk >/dev/null 2>&1; then
        export VK_ICD_FILENAMES="$(brew --prefix molten-vk)/etc/vulkan/icd.d/MoltenVK_icd.json"
    fi
    if brew --prefix vulkan-validationlayers >/dev/null 2>&1; then
        export VK_LAYER_PATH="$(brew --prefix vulkan-validationlayers)/share/vulkan/explicit_layer.d"
        export DYLD_LIBRARY_PATH="$(brew --prefix vulkan-validationlayers)/lib:${DYLD_LIBRARY_PATH:-}"
    fi
fi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build --parallel 6
exec ./build/vulkan-starter-app
