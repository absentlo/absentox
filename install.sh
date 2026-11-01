#!/bin/sh
# Installs ABSENToX. Usage: ./install.sh [REAPER resource folder]
set -eu

here=$(dirname "$0")
reaper=${1:-${XDG_CONFIG_HOME:-$HOME/.config}/REAPER}

if [ ! -d "$reaper/Effects" ]; then
  echo "REAPER's Effects folder was not found at: $reaper/Effects" >&2
  echo "Open REAPER once, or pass its folder: ./install.sh /path/to/REAPER" >&2
  exit 1
fi

install -m 644 "$here/absentox.jsfx" "$reaper/Effects/absentox.jsfx"
echo "Installed to $reaper/Effects. Restart REAPER, or press F5 in the FX browser."
