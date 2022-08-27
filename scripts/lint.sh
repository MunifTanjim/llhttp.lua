#!/usr/bin/env sh

echo "[luacheck]"
echo

set -x
luacheck "$@" .

echo
echo "[luarocks lint]"
echo

echo "Checking llhttp-dev-1.rockspec"
./scripts/generate-rockspec.sh dev-1
luarocks lint "llhttp-dev-1.rockspec"
