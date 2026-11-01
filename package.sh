#!/bin/sh
# Builds the two downloads into dist/. Run by the release workflow.
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
