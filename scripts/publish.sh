#!/usr/bin/env bash

set -euo pipefail

declare ROOT_DIR
ROOT_DIR="$(dirname "$(realpath "${BASH_SOURCE[0]}")")"

source "${ROOT_DIR}/_helper.sh"

declare rockspec_version
rockspec_version="$(get_rockspec_version "${1:-}")"

shift

if test -z "$(git tag -l "${rockspec_version}")"; then
  echo "missing tag: ${rockspec_version}"
  exit 1
fi

if [[ "$(git rev-parse "${rockspec_version}")" != "$(git rev-parse HEAD)" ]]; then
  echo "tag not checked out: ${rockspec_version}"
  exit 1
fi

declare repo_rockspec rockspec
repo_rockspec="$(get_repo_rockspec)"
rockspec="$(get_rockspec "${rockspec_version}")"

cp "${repo_rockspec}" "${rockspec}"

declare force_upload=""

if [[ "${LUAROCKS_UPLOAD_FORCE:-"false"}" = "true" ]]; then
  force_upload="--force"
fi

luarocks upload --api-key="${LUAROCKS_API_KEY}" ${force_upload} "${rockspec}"
