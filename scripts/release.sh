#!/usr/bin/env bash

set -euo pipefail

declare ROOT_DIR
ROOT_DIR="$(dirname "$(dirname "$(realpath "${BASH_SOURCE[0]}")")")"

source "${ROOT_DIR}/scripts/_helper.sh"

declare rockspec_version version
rockspec_version="$(get_rockspec_version "${1:-}")"
version="$(get_version "${rockspec_version}")"

if test -n "$(git tag -l "${rockspec_version}")"; then
  echo "rockspec version already exists: ${rockspec_version}" >&2
  exit 1
fi

declare repo_rockspec rockspec
repo_rockspec="$(get_repo_rockspec)"
rockspec="$(get_rockspec "${rockspec_version}")"

"${ROOT_DIR}/scripts/generate-rockspec.sh" "${rockspec_version}"

luarocks make --no-install "${rockspec}"

cp "${rockspec}" "${repo_rockspec}"

git add "${repo_rockspec}"

git commit -m "chore: release ${rockspec_version}"

if test -n "$(git tag -l "${version}")"; then
  git tag --delete "${version}"
fi
git tag "${version}" -m "${version}"
git tag "${rockspec_version}" -m "${rockspec_version}"
