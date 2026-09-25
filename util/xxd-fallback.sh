#!/usr/bin/env bash
#
# Minimal replacement for `xxd -i [-n name] <input> <output>`, used by
# util/build-libvmaf.sh to embed libvmaf's built-in models on systems without
# xxd. Only the C include output is supported.

set -e

name=""
include=false
files=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    -i)
      include=true
      ;;
    -n)
      shift
      name="$1"
      ;;
    *)
      files+=("$1")
      ;;
  esac
  shift
done

# meson checks for -n support by running `xxd -n test`
if [[ "$include" = false ]]; then
  exit 0
fi

input="${files[0]}"
output="${files[1]:-/dev/stdout}"

# Variable name as xxd builds it: every character that is not alphanumeric
# becomes an underscore, and a leading digit gets a "__" prefix
name="${name:-${input}}"
name="$(printf '%s' "${name}" | LC_ALL=C sed 's/[^A-Za-z0-9]/_/g; s/^\([0-9]\)/__\1/')"

{
  echo "unsigned char ${name}[] = {"
  od -An -v -tx1 "${input}" | LC_ALL=C awk '
    { for (i = 1; i <= NF; i++) bytes[n++] = "0x" $i }
    END {
      for (i = 0; i < n; i++) {
        if (i % 12 == 0) printf "  "
        printf "%s", bytes[i]
        if (i < n - 1) printf ", "
        if (i % 12 == 11 || i == n - 1) printf "\n"
      }
    }'
  echo "};"
  echo "unsigned int ${name}_len = $(wc -c < "${input}" | tr -d ' ');"
} > "${output}"
