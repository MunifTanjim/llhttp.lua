#!/usr/bin/env bash

declare -r output_file="llhttp/enum.lua"

declare -a enum_names=(
  "llhttp_errno"
  "llhttp_type"
  "llhttp_method"
  "llhttp_status"
)

declare -A enum_constant_prefix=()
enum_constant_prefix[llhttp_errno]="HPE"
enum_constant_prefix[llhttp_type]="HTTP"
enum_constant_prefix[llhttp_method]="HTTP"
enum_constant_prefix[llhttp_status]="HTTP_STATUS"

declare -A enum_constant_file=()
enum_constant_file[llhttp_errno]="./llhttp/core/llhttp.h"
enum_constant_file[llhttp_type]="./llhttp/core/llhttp.h"
enum_constant_file[llhttp_method]="./llhttp/core/llhttp.h"
enum_constant_file[llhttp_status]="./llhttp/core/llhttp.h"

declare -A enum_group_name=()
enum_group_name[llhttp_errno]="errno"
enum_group_name[llhttp_type]="type"
enum_group_name[llhttp_method]="method"
enum_group_name[llhttp_status]="status"

generate_enum_tables() {
  echo "local enum = {"
  for enum_name in "${enum_names[@]}"; do
    local lines
    lines="$(sed -n "/enum ${enum_name} {/,/};/{/enum ${enum_name} {/!{/};/!p;};}" ${enum_constant_file[$enum_name]})"

    echo ""
    echo "  --[[ ${enum_name} ]]"
    echo ""
    echo "${lines[*]},"
  done
  echo "}"
  echo ""

  for enum_name in "${enum_names[@]}"; do
    local lines
    lines="$(sed -n "/enum ${enum_name} {/,/};/{/enum ${enum_name} {/!{/};/!p;};}" ${enum_constant_file[$enum_name]})"

    echo "enum.${enum_group_name[$enum_name]} = {"
      echo "${lines[*]//${enum_constant_prefix[${enum_name}]}_/},"
    echo "}"
    echo ""
  done
}

generate_enum_tables > ${output_file}

declare -A enum_map_data_name=()
enum_map_data_name[llhttp_errno]="HTTP_ERRNO_MAP"
enum_map_data_name[llhttp_method]="HTTP_ALL_METHOD_MAP"
enum_map_data_name[llhttp_status]="HTTP_STATUS_MAP"

generate_name_tables() {
  for enum_name in "${enum_names[@]}"; do
    if test -z "${enum_map_data_name[$enum_name]}"; then
      continue
    fi

    local lines
    lines="$(sed -n "/#define ${enum_map_data_name[$enum_name]}/,/^$/{/#define ${enum_map_data_name[$enum_name]}/!{/^$/!p;};}" ${enum_constant_file[$enum_name]})"


    echo "enum.${enum_group_name[$enum_name]}_name = {"
    echo "${lines[*]}" | awk -F"[()]" '{print $2}' | awk -F', ' '{printf "[%d] = \"%s\",\n", $1, $2}'
    echo "}"
    echo ""
  done
}

generate_name_tables >> ${output_file}

echo "return enum" >> ${output_file}

stylua ${output_file}
