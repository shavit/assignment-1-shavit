#!/bin/sh

filedir=$1
text=$2

if [ $# -lt 2 ]; then
    printf "Usage: finder.sh TEXT DIR\n\n" >&2
    printf "\033[31mMissing arguments. Provided $#, while expected 2\033[0m\n" >&2
    exit 1
fi

if [ ! -d $filedir ]; then
    printf "\033[31m%s\033[0m\n", "Invalid input: $filedir is not a directory"  >&2
    exit 1;
fi

output=$(grep -r $text $filedir 2>/dev/null)
nlines=$(echo "$output" | wc -l)
nfiles=$(echo "$output" | cut -d ':' -f 1 | uniq | wc -l)

echo "The number of files are $nfiles and the number of matching lines are $nlines"
