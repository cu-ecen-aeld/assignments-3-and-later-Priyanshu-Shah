#!/bin/bash

if [ $# -lt 2 ]; then
    echo "Usage: $0 <path to file> <string to write>"
    exit 1
fi

writefile=$1
writestr=$2

writedir=$(dirname "$writefile")

if [ ! -d "$writedir" ]; then
    mkdir -p "$writedir"
    if [ $? -ne 0 ]; then
        echo "Error: Could not create directory $writedir"
        exit 1
    fi
fi

echo "$writestr" > "$writefile"