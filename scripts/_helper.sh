#!/usr/bin/env bash

declare -r package="llhttp"

get_rockspec_version() {
  local version="${1}"
  local rockspec_version="${version}"

  if [[ -z "${version}" ]]; then
    echo "missing version" >&2
    return 1
  fi

  if [[ "${version}" != *"-"* ]]; then
    rockspec_version="${version}-1"
  else
    version="${version%%-*}"
  fi

  if [[ "${version}" != "dev" ]] && [[ ! "${version}" =~ ^[0-9]+.[0-9]+.[0-9]+$ ]]; then
    echo "invalid version: ${version}" >&2
    return 1
  fi

  if [[ ! "${rockspec_version}" =~ ^${version}-[1-9]{1,}$ ]]; then
    echo "invalid rockspec version: ${rockspec_version}" >&2
    return 1
  fi

  echo "${rockspec_version}"
}

get_version() {
  local rockspec_version="${1}"

  if [[ "${rockspec_version}" != *"-"* ]]; then
    echo "invalid rockspec version: ${rockspec_version}" >&2
    return 1
  fi

  local version="${rockspec_version%%-*}"

  echo "${version}"
}

get_repo_rockspec() {
  echo "${package}.rockspec"
}

get_rockspec() {
  local rockspec_version="${1}"
  echo "${package}-${rockspec_version}.rockspec"
}
