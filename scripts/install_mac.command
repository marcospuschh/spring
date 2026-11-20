#!/bin/bash
# Instala o PTZR Spring: VST3 em ~/Library/Audio/Plug-Ins/VST3/PTZR.PLUGINS e AU em Components
cd "$(dirname "$0")"

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3/PTZR.PLUGINS"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"

mkdir -p "$VST3_DIR" "$AU_DIR"
rm -rf "$VST3_DIR/PTZR Spring.vst3" "$AU_DIR/PTZR Spring.component"
cp -R "VST3/PTZR.PLUGINS/PTZR Spring.vst3" "$VST3_DIR/"
cp -R "Components/PTZR Spring.component" "$AU_DIR/"

# remove a quarentena do macOS (plugin sem assinatura da Apple)
xattr -dr com.apple.quarantine "$VST3_DIR/PTZR Spring.vst3" 2>/dev/null
xattr -dr com.apple.quarantine "$AU_DIR/PTZR Spring.component" 2>/dev/null

echo ""
echo "PTZR Spring instalado:"
echo "  VST3: $VST3_DIR"
echo "  AU:   $AU_DIR"
echo "Reescaneie os plugins na sua DAW."
