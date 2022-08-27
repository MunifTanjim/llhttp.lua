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

shift

if test -z "$(git tag -l "${rockspec_version}")"; then
  echo "missing tag: ${rockspec_version}"
  exit 1
fi

if [[ "$(git rev-parse "${rockspec_version}")" != "$(git rev-parse HEAD)" ]]; then
  echo "tag not checked out: ${rockspec_version}"
  exit 1
fi

declare -r repo_rockspec="${package}.rockspec"
declare -r rockspec="${package}-${rockspec_version}.rockspec"

cp "${repo_rockspec}" "${rockspec}"

declare force_upload=""

if [[ "${LUAROCKS_UPLOAD_FORCE:-"false"}" = "true" ]]; then
  force_upload="--force"
fi

luarocks upload --api-key="${LUAROCKS_API_KEY}" ${force_upload} "${rockspec}"
