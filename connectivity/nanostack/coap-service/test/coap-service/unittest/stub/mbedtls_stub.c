/*
 * Copyright (c) 2015-2020, Pelion and affiliates.
 * SPDX-License-Identifier: Apache-2.0
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

#include <string.h> // memset
#include "mbedtls_stub.h"

mbedtls_stub_def mbedtls_stub;

//From ssl.h
int mbedtls_ssl_handshake_step(mbedtls_ssl_context *ssl)
{
    if (mbedtls_stub.useCounter) {

        if (mbedtls_stub.retArray[mbedtls_stub.counter] == HANDSHAKE_FINISHED_VALUE ||
                mbedtls_stub.retArray[mbedtls_stub.counter] == HANDSHAKE_FINISHED_VALUE_RETURN_ZERO) {

            ssl->state = MBEDTLS_SSL_HANDSHAKE_OVER;
            if (mbedtls_stub.retArray[mbedtls_stub.counter] == HANDSHAKE_FINISHED_VALUE_RETURN_ZERO) {
                return 0;
            }
        }
        return mbedtls_stub.retArray[mbedtls_stub.counter++];
    }
    return mbedtls_stub.expected_int;
}

int mbedtls_ssl_close_notify(mbedtls_ssl_context *a)
{
    if (mbedtls_stub.useCounter) {
        return mbedtls_stub.retArray[mbedtls_stub.counter++];
    }
    return mbedtls_stub.expected_int;
}

void mbedtls_ssl_init(mbedtls_ssl_context *a)
{

}
void mbedtls_ssl_conf_min_version(mbedtls_ssl_config *conf, int major, int minor)
{

}

void mbedtls_ssl_conf_max_version(mbedtls_ssl_config *conf, int major, int minor)
{

}

void mbedtls_ssl_conf_transport(mbedtls_ssl_config *conf, int transport)
{

}

void mbedtls_ssl_config_init(mbedtls_ssl_config *a)
{

}

void mbedtls_ssl_conf_handshake_timeout(mbedtls_ssl_config *a, uint32_t b, uint32_t c)
{

}

void mbedtls_ssl_free(mbedtls_ssl_context *a)
{

}

int mbedtls_ssl_conf_own_cert(mbedtls_ssl_config *a,
                              mbedtls_x509_crt *b,
                              mbedtls_pk_context *c)
{
    if (mbedtls_stub.useCounter) {
        return mbedtls_stub.retArray[mbedtls_stub.counter++];
    }
    return mbedtls_stub.expected_int;
}

void mbedtls_ssl_conf_authmode(mbedtls_ssl_config *a, int c)
{

}

void mbedtls_ssl_conf_ca_chain(mbedtls_ssl_config *a,
                               mbedtls_x509_crt *b,
                               mbedtls_x509_crl *c)
{

}

int mbedtls_ssl_conf_psk(mbedtls_ssl_config *a,
                         const unsigned char *b, size_t c,
                         const unsigned char *d, size_t e)
{
    if (mbedtls_stub.useCounter) {
        return mbedtls_stub.retArray[mbedtls_stub.counter++];
    }
    return mbedtls_stub.expected_int;
}

int mbedtls_ssl_config_defaults(mbedtls_ssl_config *a,
                                int b, int c, int d)
{
    if (mbedtls_stub.useCounter) {
        return mbedtls_stub.retArray[mbedtls_stub.counter++];
    }
    return mbedtls_stub.expected_int;
}

void mbedtls_ssl_conf_rng(mbedtls_ssl_config *a,
                          int (*f_rng)(void *, unsigned char *, size_t),
                          void *b)
{

}

void mbedtls_ssl_conf_ciphersuites(mbedtls_ssl_config *a,
                                   const int *b)
{

}

int mbedtls_ssl_setup(mbedtls_ssl_context *a,
                      const mbedtls_ssl_config *b)
{
    if (mbedtls_stub.useCounter) {
        return mbedtls_stub.retArray[mbedtls_stub.counter++];
    }
    return mbedtls_stub.expected_int;
}

void mbedtls_ssl_set_bio(mbedtls_ssl_context *ssl,
                         void *p_bio,
                         int (*f_send)(void *, const unsigned char *, size_t),
                         int (*f_recv)(void *, unsigned char *, size_t),
                         int (*f_recv_timeout)(void *, unsigned char *, size_t, uint32_t))
{
    if (p_bio != NULL) {
        if (f_send) {
            f_send(p_bio, NULL, 0);
        }
        if (f_recv) {
            f_recv(p_bio, NULL, 0);
        }
        if (f_recv_timeout) {
            f_recv_timeout(p_bio, NULL, 0, 0);
        }
    }
}

void mbedtls_ssl_set_timer_cb(mbedtls_ssl_context *a,
                              void *ctx,
                              void (*f_set_timer)(void *, uint32_t int_ms, uint32_t fin_ms),
                              int (*f_get_timer)(void *))
{
    f_set_timer(ctx, 1, 2);
    f_get_timer(ctx);
    if (mbedtls_stub.invalidate_timer) {
        f_set_timer(ctx, 0, 0);
    }
    f_get_timer(ctx);
}

int mbedtls_ssl_handshake(mbedtls_ssl_context *ssl)
{
    if (mbedtls_stub.useCounter) {
        return mbedtls_stub.retArray[mbedtls_stub.counter++];
    }
    return mbedtls_stub.expected_int;
}

uint32_t mbedtls_ssl_get_verify_result(const mbedtls_ssl_context *a)
{
    return mbedtls_stub.uint32_value;
}

int mbedtls_ssl_read(mbedtls_ssl_context *a, unsigned char *b, size_t c)
{
    if (mbedtls_stub.useCounter) {
        return mbedtls_stub.retArray[mbedtls_stub.counter++];
    }
    return mbedtls_stub.expected_int;
}

int mbedtls_ssl_write(mbedtls_ssl_context *a, const unsigned char *b, size_t c)
{
    if (mbedtls_stub.useCounter) {
        return mbedtls_stub.retArray[mbedtls_stub.counter++];
    }
    return mbedtls_stub.expected_int;
}

int mbedtls_ssl_set_hostname(mbedtls_ssl_context *ssl, const char *hostname)
{
    return 0;
}

const mbedtls_x509_crt *mbedtls_ssl_get_peer_cert(const mbedtls_ssl_context *ssl)
{
    return NULL;
}



//From crt_drbg.h
int mbedtls_ctr_drbg_seed(mbedtls_ctr_drbg_context *a,
                          int (*f_entropy)(void *, unsigned char *, size_t),
                          void *b,
                          const unsigned char *c,
                          size_t d)
{
    return mbedtls_stub.crt_expected_int;
}

void mbedtls_ctr_drbg_init(mbedtls_ctr_drbg_context *a)
{

}

void mbedtls_ctr_drbg_free(mbedtls_ctr_drbg_context *a)
{

}

int mbedtls_ctr_drbg_random_with_add(void *a,
                                     unsigned char *b, size_t c,
                                     const unsigned char *d, size_t e)
{
    return mbedtls_stub.crt_expected_int;
}

int mbedtls_ctr_drbg_random(void *p_rng,
                            unsigned char *output, size_t output_len)
{
    return mbedtls_stub.crt_expected_int;
}

// from hmac_drbg.h
void mbedtls_hmac_drbg_init(mbedtls_hmac_drbg_context *ctx)
{

}

void mbedtls_hmac_drbg_free(mbedtls_hmac_drbg_context *ctx)
{

}

int mbedtls_hmac_drbg_seed(mbedtls_hmac_drbg_context *ctx,
                           const mbedtls_md_info_t *md_info,
                           int (*f_entropy)(void *, unsigned char *, size_t),
                           void *p_entropy,
                           const unsigned char *custom,
                           size_t len)
{
    return mbedtls_stub.crt_expected_int;
}

int mbedtls_hmac_drbg_random(void *p_rng, unsigned char *output, size_t out_len)
{
    return mbedtls_stub.crt_expected_int;
}

// from md.h
const mbedtls_md_info_t *mbedtls_md_info_from_type(mbedtls_md_type_t md_type)
{
    return 0;
}

//From x509_crt.h
void mbedtls_x509_crt_init(mbedtls_x509_crt *a)
{

}

void mbedtls_x509_crt_free(mbedtls_x509_crt *a)
{

}

int mbedtls_x509_crt_parse(mbedtls_x509_crt *a, const unsigned char *b, size_t c)
{
    if (mbedtls_stub.useCounter) {
        return mbedtls_stub.retArray[mbedtls_stub.counter++];
    }
    return mbedtls_stub.expected_int;
}

int mbedtls_x509_crt_parse_path(mbedtls_x509_crt *a, const char *b)
{
    // means 5 valid certificates found
    return 5;
}

int mbedtls_x509_crt_info(char *buf, size_t size, const char *prefix,
                          const mbedtls_x509_crt *crt)
{
    return 0;
}

int mbedtls_x509_crt_verify_info(char *buf, size_t size, const char *prefix,
                                 uint32_t flags)
{
    return 0;
}

//From entropy.h
void mbedtls_entropy_init(mbedtls_entropy_context *a)
{

}

void mbedtls_entropy_free(mbedtls_entropy_context *ctx)
{

}

int mbedtls_entropy_func(void *a, unsigned char *b, size_t c)
{
    if (mbedtls_stub.useCounter) {
        return mbedtls_stub.retArray[mbedtls_stub.counter++];
    }
    return mbedtls_stub.expected_int;
}

int mbedtls_entropy_add_source(mbedtls_entropy_context *a,
                               mbedtls_entropy_f_source_ptr f_source, void *b,
                               size_t c, int d)
{
    unsigned char buf[2];
    size_t len;
    f_source(NULL, buf, 1, &len);

    if (mbedtls_stub.useCounter) {
        return mbedtls_stub.retArray[mbedtls_stub.counter++];
    }
    return mbedtls_stub.expected_int;
}

//From pk.h
int mbedtls_pk_parse_key(mbedtls_pk_context *a,
                         const unsigned char *b, size_t c,
                         const unsigned char *d, size_t e)
{
    if (mbedtls_stub.useCounter) {
        return mbedtls_stub.retArray[mbedtls_stub.counter++];
    }
    return mbedtls_stub.expected_int;
}

void mbedtls_pk_init(mbedtls_pk_context *ctx)
{

}

void mbedtls_pk_free(mbedtls_pk_context *ctx)
{

}

void mbedtls_ssl_config_free(mbedtls_ssl_config *a)
{

}

int mbedtls_ssl_set_hs_ecjpake_password(mbedtls_ssl_context *ssl,
                                        const unsigned char *pw,
                                        size_t pw_len)
{
    if (mbedtls_stub.useCounter) {
        return mbedtls_stub.retArray[mbedtls_stub.counter++];
    }
    return mbedtls_stub.expected_int;
}

void mbedtls_ssl_conf_dtls_cookies(mbedtls_ssl_config *conf,
                                   mbedtls_ssl_cookie_write_t *f_cookie_write,
                                   mbedtls_ssl_cookie_check_t *f_cookie_check,
                                   void *p_cookie)
{
    if (mbedtls_stub.cookie_obj && f_cookie_check && mbedtls_stub.cookie_len > 0) {
        f_cookie_check(mbedtls_stub.cookie_obj, &mbedtls_stub.cookie_value, mbedtls_stub.cookie_len, NULL, 0);
    }
    if (mbedtls_stub.cookie_obj && f_cookie_write && mbedtls_stub.cookie_len > 0) {
        unsigned char out[16];
        unsigned char *ptr = &out;
        f_cookie_write(mbedtls_stub.cookie_obj, &ptr, ptr + mbedtls_stub.cookie_len, NULL, 0);
    }
}

void mbedtls_ssl_conf_export_keys_cb(mbedtls_ssl_config *conf,
                                     mbedtls_ssl_export_keys_t *f_export_keys,
                                     void *p_export_keys)
{
    if (f_export_keys && p_export_keys) {
        unsigned char value[40];
        memset(&value, 1, 40);
        f_export_keys(p_export_keys, &value, "", 0, 0, 0); //failure case

        f_export_keys(p_export_keys, &value, "", 0, 20, 0); //success case
    }
}

int mbedtls_ssl_session_reset(mbedtls_ssl_context *ssl)
{
    if (mbedtls_stub.useCounter) {
        return mbedtls_stub.retArray[mbedtls_stub.counter++];
    }
    return mbedtls_stub.expected_int;
}

void mbedtls_strerror(int ret, char *buf, size_t buflen)
{
}

int mbedtls_platform_setup(mbedtls_platform_context *ctx)
{
    (void)ctx;

    return (0);
}

void mbedtls_platform_teardown(mbedtls_platform_context *ctx)
{
    (void)ctx;
}
