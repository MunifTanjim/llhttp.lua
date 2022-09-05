#!/usr/bin/env bash

set -euo pipefail

declare ROOT_DIR
ROOT_DIR="$(dirname "$(dirname "$(realpath "${BASH_SOURCE[0]}")")")"

source "${ROOT_DIR}/scripts/_helper.sh"

declare rockspec_version version
rockspec_version="$(get_rockspec_version "${1:-}")"
version="$(get_version "${rockspec_version}")"

declare rockspec
rockspec="$(get_rockspec "${rockspec_version}")"

if [[ "${version}" = "dev" ]]; then
  "${ROOT_DIR}/scripts/generate-rockspec.sh" "${rockspec_version}"
fi

echo "generating compile_flags.txt..."
echo "-Illhttp" > compile_flags.txt
echo "-I$(luarocks config variables.LUA_INCDIR)" >> compile_flags.txt
echo

echo "luarocks make..."
luarocks make --no-install "${rockspec}"
