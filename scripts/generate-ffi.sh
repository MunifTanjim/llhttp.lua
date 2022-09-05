#!/usr/bin/env bash

declare -r output_file="llhttp/ffi.lua"

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

generate_cdef_enum() {
  echo ""
  for enum_name in "${enum_names[@]}"; do
    local lines
    lines="$(sed -n "/enum ${enum_name} {/,/};/{/enum ${enum_name} {/!{/};/!p;};}" ${enum_constant_file[$enum_name]})"

    echo "enum ${enum_name} {"
      echo "${lines[*]}"
    echo "};"
    echo "typedef enum ${enum_name} ${enum_name}_t;"
    echo ""
  done
}

declare content_before_cdef_enum content_after_cdef_enum
content_before_cdef_enum="$(sed '/\/\/ START:CDEF:ENUM/q' ${output_file})"
content_after_cdef_enum="$(sed -ne '/\/\/ END:CDEF:ENUM/,$ p' ${output_file})"

echo "${content_before_cdef_enum}" > ${output_file}
generate_cdef_enum >> ${output_file}
echo "${content_after_cdef_enum}" >>${output_file}

generate_enum() {
  echo ""
  echo "local enum = {"
  for enum_name in "${enum_names[@]}"; do
    local lines
    lines="$(sed -n "/enum ${enum_name} {/,/};/{/enum ${enum_name} {/!{/};/!p;};}" ${enum_constant_file[$enum_name]})"

    local -a constant_names
    mapfile -t constant_names < <(echo "${lines[@]}" | grep -o "[A-Z][^ =]\+")

    echo ""
    echo "  --[[ ${enum_name} ]]"
    echo ""
    for constant_name in "${constant_names[@]}"; do
      echo "  ${constant_name} = ffi.C.${constant_name},"
    done
  done
  echo "}"

  for enum_name in "${enum_names[@]}"; do
    local lines
    lines="$(sed -n "/enum ${enum_name} {/,/};/{/enum ${enum_name} {/!{/};/!p;};}" ${enum_constant_file[$enum_name]})"

    local -a constant_names
    mapfile -t constant_names < <(echo "${lines[@]}" | grep -o "[A-Z][^ =]\+")

    echo ""
    echo "enum.${enum_group_name[$enum_name]} = {"
      for constant_name in "${constant_names[@]}"; do
        echo "  ${constant_name//${enum_constant_prefix[${enum_name}]}_/} = ffi.C.${constant_name},"
      done
    echo "}"
  done
  echo ""
}

declare content_before_enum content_after_enum
content_before_enum="$(sed '/-- START:ENUM/q' ${output_file})"
content_after_enum="$(sed -ne '/-- END:ENUM/,$ p' ${output_file})"

echo "${content_before_enum}" > ${output_file}
generate_enum >> ${output_file}
echo "${content_after_enum}" >> ${output_file}

stylua ${output_file}
