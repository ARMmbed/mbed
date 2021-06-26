#!/bin/sh
#
# This file is part of mbed TLS (https://tls.mbed.org)
#
# Copyright (c) 2015-2016, ARM Limited, All Rights Reserved
#
# Purpose
#
# Comments and uncomments #define lines in the given configuration header file
# to configure the file for use in mbed OS.
#
# Usage: adjust-config.sh [path to config script] [path to config file]
#
set -eu

if [ $# -ne 2 ]; then
    echo "Usage: $0 path/to/config.pl path/to/config.h" >&2
    exit 1
fi

SCRIPT=$1
FILE=$2

conf() {
    $SCRIPT -f $FILE --force $@
}

# Add code before the matching line
prepend_code() {
    MATCH_PATTERN="$1"
    shift
    CODE=$(IFS=""; printf "%s" "$*")

    perl -i -pe                                    \
        "s/$MATCH_PATTERN/$CODE$MATCH_PATTERN/igs" \
        "$FILE"
}

# Add code after the matching line
append_code() {
    MATCH_PATTERN="$1"
    shift
    CODE=$(IFS=""; printf "%s" "$*")

    perl -i -pe                                    \
        "s/$MATCH_PATTERN/$MATCH_PATTERN$CODE/igs" \
        "$FILE"
}

# not supported on mbed OS, nor used by mbed Client
conf unset MBEDTLS_NET_C
conf unset MBEDTLS_TIMING_C

# not supported on all targets with mbed OS, nor used by mbed Client
conf unset MBEDTLS_HAVE_TIME_DATE
conf unset MBEDTLS_FS_IO
conf unset MBEDTLS_PSA_ITS_FILE_C
conf unset MBEDTLS_PSA_CRYPTO_STORAGE_C
conf set MBEDTLS_NO_PLATFORM_ENTROPY

conf unset MBEDTLS_CIPHER_MODE_CFB
conf unset MBEDTLS_CIPHER_MODE_OFB
conf unset MBEDTLS_CIPHER_MODE_CTR
conf unset MBEDTLS_CIPHER_PADDING_ONE_AND_ZEROS
conf unset MBEDTLS_CIPHER_PADDING_ZEROS_AND_LEN
conf unset MBEDTLS_CIPHER_PADDING_ZEROS
conf unset MBEDTLS_CIPHER_MODE_XTS
conf unset MBEDTLS_ECP_DP_SECP192R1_ENABLED
conf unset MBEDTLS_ECP_DP_SECP224R1_ENABLED
conf unset MBEDTLS_ECP_DP_SECP521R1_ENABLED
conf unset MBEDTLS_ECP_DP_SECP192K1_ENABLED
conf unset MBEDTLS_ECP_DP_SECP224K1_ENABLED
conf unset MBEDTLS_ECP_DP_SECP256K1_ENABLED
conf unset MBEDTLS_ECP_DP_BP256R1_ENABLED
conf unset MBEDTLS_ECP_DP_BP384R1_ENABLED
conf unset MBEDTLS_ECP_DP_BP512R1_ENABLED
conf unset MBEDTLS_PK_PARSE_EC_EXTENDED

conf unset MBEDTLS_AESNI_C
conf unset MBEDTLS_ARC4_C
conf unset MBEDTLS_BLOWFISH_C
conf unset MBEDTLS_CAMELLIA_C
conf unset MBEDTLS_DES_C
conf unset MBEDTLS_DHM_C
conf unset MBEDTLS_GENPRIME
conf unset MBEDTLS_MD5_C
conf unset MBEDTLS_PADLOCK_C
conf unset MBEDTLS_PEM_WRITE_C
conf unset MBEDTLS_PKCS5_C
conf unset MBEDTLS_PKCS12_C
conf unset MBEDTLS_RIPEMD160_C
conf unset MBEDTLS_SHA1_C
conf unset MBEDTLS_XTEA_C

conf set MBEDTLS_CMAC_C

conf set MBEDTLS_AES_ROM_TABLES

conf unset MBEDTLS_X509_RSASSA_PSS_SUPPORT

conf unset MBEDTLS_X509_CSR_PARSE_C
conf unset MBEDTLS_X509_CREATE_C
conf unset MBEDTLS_X509_CRT_WRITE_C
conf unset MBEDTLS_X509_CSR_WRITE_C

conf unset MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED
conf unset MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED
conf unset MBEDTLS_KEY_EXCHANGE_RSA_ENABLED
conf unset MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED
conf unset MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED
conf unset MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED
conf unset MBEDTLS_SSL_FALLBACK_SCSV
conf unset MBEDTLS_SSL_CBC_RECORD_SPLITTING
conf unset MBEDTLS_SSL_PROTO_TLS1
conf unset MBEDTLS_SSL_PROTO_TLS1_1
conf unset MBEDTLS_SSL_TRUNCATED_HMAC

conf unset MBEDTLS_PLATFORM_TIME_TYPE_MACRO

# potentially save flash space by not enabling self-tests by default
conf unset MBEDTLS_SELF_TEST

# The default size of MBEDTLS_MPI_MAX_SIZE is 1024 bytes.
# In some cases, this value is set to stack buffers.
# Reduce the maximal MBEDTLS_MPI_MAX_SIZE to 512 bytes,
# which should fit RSA 4096 bit keys.
conf set MBEDTLS_MPI_MAX_SIZE     512

# Explicitly unset MBEDTLS_USE_PSA_CRYPTO as this will be set based on the
# FEATURE_PSA flag in Mbed OS
conf unset MBEDTLS_USE_PSA_CRYPTO

# add an #ifndef to include config-no-entropy.h when the target does not have
# an entropy source we can use.
append_code                                                                                       \
    "#ifndef MBEDTLS_CONFIG_H\n"                                                                  \
    "\n"                                                                                          \
    "#include \"platform\/inc\/platform_mbed.h\"\n"                                               \
    "\n"                                                                                          \
    "\/*\n"                                                                                       \
    " * Only use features that do not require an entropy source when\n"                           \
    " * DEVICE_ENTROPY_SOURCE is not defined in mbed OS.\n"                                       \
    " *\/\n"                                                                                      \
    "#if !defined(MBEDTLS_ENTROPY_HARDWARE_ALT) && !defined(MBEDTLS_TEST_NULL_ENTROPY) && \\\\\n" \
    "    !defined(MBEDTLS_ENTROPY_NV_SEED)\n"                                                     \
    "#include \"mbedtls\/config-no-entropy.h\"\n"                                                 \
    "\n"                                                                                          \
    "#if defined(MBEDTLS_USER_CONFIG_FILE)\n"                                                     \
    "#include MBEDTLS_USER_CONFIG_FILE\n"                                                         \
    "#endif\n"                                                                                    \
    "\n"                                                                                          \
    "#else\n"

prepend_code                                                                                                   \
    "#endif \/\* MBEDTLS_CONFIG_H \*\/"                                                                        \
    "\n"                                                                                                       \
    "#endif \/* !MBEDTLS_ENTROPY_HARDWARE_ALT && !MBEDTLS_TEST_NULL_ENTROPY && !MBEDTLS_ENTROPY_NV_SEED *\/\n" \
    "\n"                                                                                                       \
    "#if defined(MBEDTLS_TEST_NULL_ENTROPY)\n"                                                                 \
    "#warning \"MBEDTLS_TEST_NULL_ENTROPY has been enabled. This \" \\\\\n"                                    \
    "    \"configuration is not secure and is not suitable for production use\"\n"                             \
    "#endif\n"                                                                                                 \
    "\n"                                                                                                       \
    "#if defined(MBEDTLS_SSL_TLS_C) && !defined(MBEDTLS_TEST_NULL_ENTROPY) && \\\\\n"                          \
    "    !defined(MBEDTLS_ENTROPY_HARDWARE_ALT) && !defined(MBEDTLS_ENTROPY_NV_SEED)\n"                        \
    "#error \"No entropy source was found at build time, so TLS \" \\\\\n"                                     \
    "    \"functionality is not available\"\n"                                                                 \
    "#endif\n"                                                                                                 \
    "\n"                                                                                                       \
    "#if defined(FEATURE_EXPERIMENTAL_API) && defined(FEATURE_PSA)\n"                                          \
    "    #define MBEDTLS_PSA_HAS_ITS_IO\n"                                                                     \
    "    #define MBEDTLS_USE_PSA_CRYPTO\n"                                                                     \
    "#endif\n"                                                                                                 \
    "\n"
