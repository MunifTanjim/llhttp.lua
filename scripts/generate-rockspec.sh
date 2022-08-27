#!/usr/bin/env bash

set -euo pipefail

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

declare -r repo_rockspec="${package}.rockspec"
declare -r rockspec="${package}-${rockspec_version}.rockspec"

cp "${repo_rockspec}" "${rockspec}"

script="/^version/s|\"[^\"]\\+\"|\"${rockspec_version}\"|"
sed -e "${script}" -i "${rockspec}"

if [[ "${version}" = "dev" ]]; then
  script="/^ \\+tag/s|\"[^\"]\\+\"|nil|"
else
  script="/^ \\+tag/s|\"[^\"]\\+\"|\"${version}\"|"
fi
sed -e "${script}" -i "${rockspec}"
