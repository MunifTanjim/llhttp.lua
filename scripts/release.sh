#!/usr/bin/env bash

set -eu

declare -r package="llhttp"

declare version="${1:-}"
declare rockspec_version="${version}"

if [[ -z "${version}" ]]; then
  echo "missing version" >&2
  exit 1
fi

if [[ "${version}" != *"-"* ]]; then
  rockspec_version="${version}-1"
else
  version="${version%%-*}"
fi

if [[ ! "${version}" =~ ^[0-9]+.[0-9]+.[0-9]+$ ]]; then
  echo "invalid version: ${version}" >&2
  exit 1
fi

if [[ ! "${rockspec_version}" =~ ^${version}-[1-9]{1,}$ ]]; then
  echo "invalid rockspec version: ${rockspec_version}" >&2
  exit 1
fi

if test -n "$(git tag -l "${rockspec_version}")"; then
  echo "rockspec version already exists: ${rockspec_version}" >&2
  exit 1
fi

declare -r repo_rockspec="${package}.rockspec"
declare -r rockspec="${package}-${rockspec_version}.rockspec"

./scripts/make-rockspec.sh "${rockspec_version}"

luarocks make --no-install "${rockspec}"

cp "${rockspec}" "${repo_rockspec}"

git add ${repo_rockspec}

git commit -m "chore: release ${rockspec_version}"

if test -n "$(git tag -l "${version}")"; then
  git tag --delete "${version}"
fi
git tag "${version}" -m "${version}"
git tag "${rockspec_version}" -m "${rockspec_version}"
