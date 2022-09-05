#!/usr/bin/env bash

set -euo pipefail

declare ROOT_DIR
ROOT_DIR="$(dirname "$(dirname "$(realpath "${BASH_SOURCE[0]}")")")"

declare -r repo_dir="${ROOT_DIR}/.nodejs-llhttp"

if ! test -d "${repo_dir}"; then
  git clone "https://github.com/nodejs/llhttp.git" "${repo_dir}"
else
  git -C "${repo_dir}" pull
fi

pushd "${repo_dir}"

npm ci

make generate

popd

cp "${repo_dir}"/{build/{llhttp.h,c/llhttp.c},src/native/{api.{c,h},http.c}} llhttp/core/

echo
echo "generating 'llhttp.enum'..."
"${ROOT_DIR}/scripts/generate-enum.sh"

echo
echo "generating 'llhttp.ffi'..."
"${ROOT_DIR}/scripts/generate-ffi.sh"
