/* mbed Microcontroller Library
 * Copyright (c) 2018 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/* Mbed TLS includes */
#include "mbedtls/asn1.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/pk_info.h"

#include "ATCAFactory.h"
#include "mbedtls_atca_engine.h"

#include "assert.h"

#define UNUSED(x) ((void)(x))


/** Tell if can do the operation given by type.
 *
 *
 * @param ctx       Key context.
 * @param type      Target type
 *
 * @return          0 if context can't do the operations,
 *                  1 otherwise.
 */
static int atca_can_do_func(const void *ctx, mbedtls_pk_type_t type)
{
    UNUSED(ctx);
    return (MBEDTLS_PK_ECDSA == type);
}

/** Sign message.
 *
 *
 * @param ctx       Key context
 * @param md_alg    Algorithm that was used to hash the message
 * @param hash      Message hash
 * @param hash_len  Length of hash
 * @param sig       Buffer that will hold the signature
 * @param sig_len   Length of the signature written
 * @param f_rng     RNG function
 * @param p_rng     RNG parameter
 *
 * @retval          0 if successful, or appropriate Mbed TLS error code.
 */
static int atca_sign_func(void *ctx, mbedtls_md_type_t md_alg,
                            const unsigned char *hash, size_t hash_len,
                            unsigned char *sig, size_t *sig_len,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    ATCAKey * key = (ATCAKey *)ctx;
    uint8_t rs[ATCA_ECC_SIG_LEN];
    size_t rs_len;
    int ret = -1;
    ATCAError err = ATCA_SUCCESS;
    printf("atca_sign_func called \r\n");

    if ( md_alg != MBEDTLS_MD_SHA256 )
    {
        ret = MBEDTLS_ERR_MD_FEATURE_UNAVAILABLE;
        goto exit;
    }

    mbedtls_mpi r, s;
    mbedtls_mpi_init( &r );
    mbedtls_mpi_init( &s );
    err = key->Sign( (const uint8_t *)hash,
                               hash_len, rs, sizeof(rs), &rs_len);
    if (err != ATCA_SUCCESS)
    {
        printf("Sign failed %02x!\r\n", err );
        ret = MBEDTLS_ERR_PK_HW_ACCEL_FAILED;
        goto exit;
    }
    /* import r & s from buffer */
    mbedtls_mpi_read_binary(&r, rs, rs_len/2);
    mbedtls_mpi_read_binary(&s, rs + rs_len/2, rs_len/2);
    /* create asn1 from r & s */
    mbedtls_ecdsa_signature_to_asn1( &r, &s, sig, sig_len, 100 );
    printf("Signature:\r\n");
    for (size_t i = 0; i < *sig_len; i++)
    {
        if (i && i % 4 == 0)
            printf ("\r\n");
        printf ("0x%02x ", sig[i]);
    }
    printf("\r\n");
    ret = 0; /* Reaching here means success */
exit:
    mbedtls_mpi_free( &r );
    mbedtls_mpi_free( &s );
    return ret;
}

/** Extract ECDSA signature components R & S from ASN.1
 *
 * @param sig       Input signature
 * @param sig_len   Input signature length
 * @param R         Buffer to hold the R
 * @param R_len     Length of the R buffer
 * @param S         Buffer to hold the S
 * @param S_len     Length of the S buffer
 *
 * @retval          0 if successful, or appropriate Mbed TLS error code.
 */
static int mbedtls_ecdsa_asn1_to_signature(const unsigned char *sig, size_t slen,
                                    uint8_t * R, size_t R_len, uint8_t * S, size_t S_len)
{
    int ret;
    unsigned char *p = (unsigned char *) sig;
    const unsigned char *end = sig + slen;
    size_t len;
    mbedtls_mpi r, s;

    mbedtls_mpi_init( &r );
    mbedtls_mpi_init( &s );

    if( ( ret = mbedtls_asn1_get_tag( &p, end, &len,
                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE ) ) != 0 )
    {
        ret += MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto cleanup;
    }

    if( p + len != end )
    {
        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA +
              MBEDTLS_ERR_ASN1_LENGTH_MISMATCH;
        goto cleanup;
    }

    if( ( ret = mbedtls_asn1_get_mpi( &p, end, &r ) ) != 0 ||
        ( ret = mbedtls_asn1_get_mpi( &p, end, &s ) ) != 0 )
    {
        ret += MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto cleanup;
    }

    if( ( ret = mbedtls_mpi_write_binary( &r, R, R_len ) ) != 0 ||
        ( ret = mbedtls_mpi_write_binary( &s, S, S_len ) ) != 0 )
    {
        ret += MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto cleanup;
    }

    if( p != end )
        ret = MBEDTLS_ERR_ECP_SIG_LEN_MISMATCH;

cleanup:
    mbedtls_mpi_free( &r );
    mbedtls_mpi_free( &s );

    return( ret );
}

/** Verify signature.
 *
 *
 * @param ctx       Key context
 * @param md_alg    Algorithm that was used to hash the message
 * @param hash      Message hash
 * @param hash_len  Length of hash
 * @param sig       Signature buffer
 * @param sig_len   Length of the signature buffer
 *
 * @retval          0 if successful, or appropriate Mbed TLS error code.
 */
static int atca_verify_func( void *ctx, mbedtls_md_type_t md_alg,
                        const unsigned char *hash, size_t hash_len,
                        const unsigned char *sig, size_t sig_len )
{
    ATCAKey * key = (ATCAKey *)ctx;
    uint8_t rs[64];
    int ret = 0;

    if ( md_alg != MBEDTLS_MD_SHA256 )
        return MBEDTLS_ERR_MD_FEATURE_UNAVAILABLE;

    /* Get R & S concatenantion from signature */
    if ( (ret = mbedtls_ecdsa_asn1_to_signature(sig, sig_len, rs, sizeof(rs)/2,
                    rs + sizeof(rs)/2, sizeof(rs)/2)) != 0)
    {
        printf("R & S import failed\r\n");
        return ret;
    }
    /* Verify the signature */
    ATCAError err = key->Verify( hash, hash_len, rs, sizeof(rs) );
    if (err == ATCA_ERR_CHECK_MAC_OR_VERIFY_FAIL)
    {
        return MBEDTLS_ERR_PK_INVALID_SIGNATURE;
    }
    else if (err != ATCA_SUCCESS)
    {
        printf("Verify failed = 0x%x\r\n", err);
        return MBEDTLS_ERR_PK_HW_ACCEL_FAILED;
    }
    return 0;
}

/** Cleanup Key context.
 *
 * @param ctx       Key context
 */
static void atca_ctx_free( void * ctx )
{
    ATCAKey * key = (ATCAKey *)ctx;
    delete key;
}

#ifdef __cplusplus
extern "C" {
#endif

/** PK Setup function for ATCA crypto engine.
 *
 * @param ctx       Key context
 * @param keyId     Private key Id representing a hardware Key.
 *
 * @retval          0 if successful, or appropriate Mbed TLS error code.
 */
int mbedtls_atca_pk_setup( mbedtls_pk_context * ctx, ATCAKeyID keyId )
{
    ATCAKey * key = NULL;
    ATCAError err = ATCA_SUCCESS;
    
    if ( ctx == NULL )
        return( MBEDTLS_ERR_PK_BAD_INPUT_DATA );

    ATCADevice * device = ATCAFactory::GetDevice( err );
    if ( err != ATCA_SUCCESS )
    {
        assert( device == NULL );
        return( MBEDTLS_ERR_PK_ALLOC_FAILED );
    }
    err = device->GetKeyToken( keyId, key );
    if ( err != ATCA_SUCCESS )
    {
        assert( key == NULL );
        return( MBEDTLS_ERR_PK_ALLOC_FAILED );
    }

    static const mbedtls_pk_info_t atca_pk_info =
    {
        MBEDTLS_PK_OPAQUE,
        "ATCA",
        NULL,
        atca_can_do_func,
        NULL,
        atca_verify_func,
        atca_sign_func,
        NULL,
        NULL,
        NULL,
        NULL,
        atca_ctx_free,
        NULL
    };


    mbedtls_pk_setup( ctx, &atca_pk_info );
    ctx->pk_ctx = (void *)key;

    return( 0 );
}

/** Setup transparent PK context using Public key from ATCA crypto engine.
 *
 * @param ctx       Key context
 * @param keyId     Private key Id representing a hardware Key.
 *
 * @retval          0 if successful, or appropriate Mbed TLS error code.
 */
int mbedtls_atca_transparent_pk_setup( mbedtls_pk_context * ctx, ATCAKeyID keyId )
{
    ATCAKey * key = NULL;
    ATCAError err = ATCA_SUCCESS;
    ATCADevice * device = ATCAFactory::GetDevice( err );
    if ( err != ATCA_SUCCESS )
    {
        assert( device == NULL );
        return MBEDTLS_ERR_PK_HW_ACCEL_FAILED;
    }

    /* Import public key from binary */
    static mbedtls_ecp_keypair ecp_key;
    mbedtls_ecp_keypair_init(&ecp_key);
    int ret = mbedtls_ecp_group_load(&ecp_key.grp, MBEDTLS_ECP_DP_SECP256R1);
    if ( ret != 0 )
        return( ret );
    err = device->GetKeyToken( keyId, key );
    if ( err != ATCA_SUCCESS )
    {
        assert( key == NULL );
        printf(" failed\n  !  to retrieve Public key from ATCA508A\n\n" );
        return MBEDTLS_ERR_PK_HW_ACCEL_FAILED;
    }
    uint8_t pubkey_octet_string[ATCA_ECC_ECC_PK_LEN + 10];
    size_t str_len = 0;
    ret = ecc_key_to_octet_string( key->GetPubKey(), pubkey_octet_string, sizeof(pubkey_octet_string), &str_len);
    delete key;
    if ( ret != 0 )
    {
        printf( " failed\n  !  EC key to octet string conversion\n\n" );
        return( ret );
    }
    ret = mbedtls_ecp_point_read_binary(&ecp_key.grp, &ecp_key.Q, pubkey_octet_string, str_len );
    if ( ret != 0 )
    {
        printf( " failed\n  !  Failed to read ecp key from binary\n\n" );
        return( ret );
    }
    mbedtls_pk_setup( ctx, mbedtls_pk_info_from_type( MBEDTLS_PK_ECKEY ) );
    ctx->pk_ctx = &ecp_key;
    return( 0 );
}

/** Convert ECC Public key in octet string as defined in SEC1 §2.3.3–2.3.4
 *
 *  @param ecc_pk       ECC Public key input.
 *  @param str_out      EC octet string out buffer.
 *  @param str_len      Out buffer length.
 *  @param str_out_len  EC octet string out length.
 *  @return             0 on success, -1 on failure
 */
int ecc_key_to_octet_string( uint8_t * ecc_pk, uint8_t * str_out, size_t str_len,
        size_t * str_out_len )
{
    int ret = -1;
    mbedtls_ecp_keypair ecp_key;
    mbedtls_ecp_keypair_init(&ecp_key);

    if ( ( ret = mbedtls_ecp_group_load(&ecp_key.grp,
                                        MBEDTLS_ECP_DP_SECP256R1) ) != 0)
    {
        goto cleanup;
    }
    MBEDTLS_MPI_CHK( mbedtls_mpi_read_binary( &ecp_key.Q.X, ecc_pk, 32 ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_read_binary( &ecp_key.Q.Y, ecc_pk + 32, 32 ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_lset( &ecp_key.Q.Z, 1 ) );
    if( ( ret = mbedtls_ecp_point_write_binary( &ecp_key.grp, &ecp_key.Q,
                MBEDTLS_ECP_PF_UNCOMPRESSED,
                str_out_len, str_out, str_len ) ) != 0 )
    {
        goto cleanup;
    }
    ret = 0;
cleanup:
    return( ret );
}

#ifdef __cplusplus
}
#endif

