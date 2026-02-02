#!/bin/sh

filepath=$1
text=$2

if [ $# -lt 2 ]; then
    printf "Usage: writer.sh FILEPATH TEXT\n\n" >&2
    printf "\033[31mMissing arguments. Provided $#, while expected 2\033[0m\n" >&2
    exit 1
fi

if [ -d "$filepath" ]; then
    printf "\033[31mInvalid path: $filepath is a directory\033[0m\n" >&2
    exit 1
fi

dirpath=$(dirname "$filepath")
mkdir -p "$dirpath"

output=$(echo "$text" > $filepath) 2>/dev/null
if [ $? -ne 0 ]; then
    printf "\033[31m%s\033[0m\n" "Permission error: $filepath could not be written"  >&2
fi
