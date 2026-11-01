#!/bin/sh
# Installs ABSENToX as VST3 and CLAP for the current user.
set -eu

here=$(dirname "$0")

mkdir -p "$HOME/.vst3" "$HOME/.clap"
rm -rf "$HOME/.vst3/ABSENToX.vst3"
cp -r "$here/ABSENToX.vst3" "$HOME/.vst3/"
install -m 755 "$here/ABSENToX.clap" "$HOME/.clap/ABSENToX.clap"

echo "Installed to ~/.vst3 and ~/.clap. Rescan your plugins, or restart your DAW."
