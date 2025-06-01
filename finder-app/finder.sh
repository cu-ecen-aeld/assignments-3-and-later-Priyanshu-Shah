#!/bin/bash

if [ $# -lt 2 ]; then
    echo "Usage: $0 <path to directory> <search string>"
    exit 1
fi

filesdir=$1
searchstr=$2

if [ ! -d "$filesdir" ]; then
    echo "Directory $filesdir does not exist."
    exit 1
fi

count=$(find "$filesdir" -type f | wc -l)
lines=$(grep -r "$searchstr" "$filesdir" | wc -l)

echo "The number of files are $count and the number of matching lines are $lines"