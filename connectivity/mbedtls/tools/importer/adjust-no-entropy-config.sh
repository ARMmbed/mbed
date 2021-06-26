#!/bin/sh
#
# This file is part of mbed TLS (https://tls.mbed.org)
#
# Copyright (c) 2018, ARM Limited, All Rights Reserved
#
# Purpose
#
# Comments and uncomments #define lines in the given configuration header file
# to configure the file for use in mbed OS.
#
# Usage: adjust-no-entropy-config.sh [path to config script] [path to no-entropy config file]
#
set -eu

if [ $# -ne 2 ]; then
    echo "Usage: $0 path/to/config.pl path/to/config.h" >&2
    exit 1
fi

SCRIPT=$1
FILE=$2

conf() {
    $SCRIPT -o -f $FILE $@
}

add_code() {
    MATCH_PATTERN="$1"
    shift
    CODE=$(IFS=""; printf "%s" "$*")

    perl -i -pe                                    \
        "s/$MATCH_PATTERN/$MATCH_PATTERN$CODE/igs" \
        "$FILE"
}

conf set MBEDTLS_CMAC_C
conf unset MBEDTLS_CIPHER_MODE_XTS

# potentially save flash space by not enabling self-tests by default
conf unset MBEDTLS_SELF_TEST
