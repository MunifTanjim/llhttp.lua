#!/usr/bin/env bash

set -euo pipefail

declare ROOT_DIR
ROOT_DIR="$(dirname "$(dirname "$(realpath "${BASH_SOURCE[0]}")")")"

source "${ROOT_DIR}/scripts/_helper.sh"

declare rockspec_version version
rockspec_version="$(get_rockspec_version "${1:-}")"
version="$(get_version "${rockspec_version}")"

declare repo_rockspec rockspec
repo_rockspec="$(get_repo_rockspec)"
rockspec="$(get_rockspec "${rockspec_version}")"

cp "${repo_rockspec}" "${rockspec}"

declare sed_script
sed_script="/^version/s|\"[^\"]\\+\"|\"${rockspec_version}\"|"
sed -e "${sed_script}" -i "${rockspec}"

if [[ "${version}" = "dev" ]]; then
  sed_script="/^ \\+tag/s|\"[^\"]\\+\"|nil|"
else
  sed_script="/^ \\+tag/s|\"[^\"]\\+\"|\"${version}\"|"
fi
sed -e "${sed_script}" -i "${rockspec}"
