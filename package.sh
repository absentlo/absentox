#!/bin/sh
# Builds the downloads into dist/. Run by the release workflow.
# The Windows plugins need mingw-w64, and are skipped when it is missing.
set -eu
cd "$(dirname "$0")"

rm -rf dist
mkdir -p dist/ABSENToX
cp -r absentox.jsfx README.md LICENSE docs dist/ABSENToX/

cp install.sh dist/ABSENToX/
tar -czf dist/ABSENToX-Linux.tar.gz -C dist ABSENToX
rm dist/ABSENToX/install.sh

cp install.bat dist/ABSENToX/
(cd dist && zip -qr ABSENToX-Windows.zip ABSENToX)

rm -rf dist/ABSENToX

# Starts each folder with its files, so the two plugin builds cannot mix.
native_folder() {
  rm -rf dist/ABSENToX
  mkdir -p dist/ABSENToX
  cp -r README.md LICENSE THIRD-PARTY-NOTICES docs native/build/ABSENToX.vst3 native/build/ABSENToX.clap dist/ABSENToX/
}

rm -rf native/build native/dpf/build
make -C native -j"$(nproc)"
native_folder
cp native/install.sh dist/ABSENToX/
tar -czf dist/ABSENToX-VST3-CLAP-Linux.tar.gz -C dist ABSENToX

if command -v x86_64-w64-mingw32-g++ >/dev/null; then
  rm -rf native/build native/dpf/build
  make -C native -j"$(nproc)" CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++ AR=x86_64-w64-mingw32-ar
  native_folder
  cp native/install.bat dist/ABSENToX/
  (cd dist && zip -qr ABSENToX-VST3-CLAP-Windows.zip ABSENToX)
  rm -rf native/build native/dpf/build
else
  echo "mingw-w64 not found: skipping the Windows plugins" >&2
fi

rm -rf dist/ABSENToX
