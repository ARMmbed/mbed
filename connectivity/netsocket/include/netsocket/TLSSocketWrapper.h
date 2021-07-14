/** @file TLSSocketWrapper.h TLSSocketWrapper */
/*
 * Copyright (c) 2018 ARM Limited
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
/** @addtogroup netsocket
* @{
*/

#ifndef _MBED_HTTPS_TLS_SOCKET_WRAPPER_H_
#define _MBED_HTTPS_TLS_SOCKET_WRAPPER_H_

#include "netsocket/Socket.h"
#include "rtos/EventFlags.h"
#include "platform/Callback.h"
#include "mbedtls/platform.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/hmac_drbg.h"
#include "mbedtls/error.h"

// This class requires Mbed TLS SSL/TLS client code
#if defined(MBEDTLS_SSL_CLI_C) || defined(DOXYGEN_ONLY)

#if defined(MBEDTLS_CTR_DRBG_C)
#define DRBG_CTX mbedtls_ctr_drbg_context
#define DRBG_INIT mbedtls_ctr_drbg_init
#define DRBG_RANDOM mbedtls_ctr_drbg_random
#define DRBG_FREE mbedtls_ctr_drbg_free
#elif defined(MBEDTLS_HMAC_DRBG_C)
#define DRBG_CTX mbedtls_hmac_drbg_context
#define DRBG_INIT mbedtls_hmac_drbg_init
#define DRBG_RANDOM mbedtls_hmac_drbg_random
#define DRBG_FREE mbedtls_hmac_drbg_free
#else
#error "CTR or HMAC must be defined for TLSSocketWrapper!"
#endif

/**
 * TLSSocket is a wrapper around Socket for interacting with TLS servers.
 *
 * TLSSocketWrapper can use any Socket as a transport. After
 * completing the TLS handshake, it can be used as any Socket would be used.
 *
 */
class TLSSocketWrapper : public Socket {
public:
    /** Transport modes */
    enum control_transport {
        TRANSPORT_KEEP,                 /**< Doesn't call connect() or close() on transport socket */
        TRANSPORT_CONNECT_AND_CLOSE,    /**< Does call connect() and close() on transport socket */
        TRANSPORT_CONNECT,              /**< Does call only connect() on transport socket */
        TRANSPORT_CLOSE,                /**< Does call close() on transport socket */
    };

    /** Create a TLSSocketWrapper.
     *
     * @param transport    Underlying transport socket to wrap.
     * @param hostname     Hostname of the remote host, used for certificate checking.
     * @param control      Transport control mode. See @ref control_transport.
     */
    TLSSocketWrapper(Socket *transport, const char *hostname = NULL, control_transport control = TRANSPORT_CONNECT_AND_CLOSE);

    /** Destroy a socket wrapper.
     *
     *  Closes socket wrapper if the socket wrapper is still open.
     */
    ~TLSSocketWrapper() override;

    /** Set hostname.
     *
     * @note Implementation is inside following defines:
     * #if defined(MBEDTLS_X509_CRT_PARSE_C) && !defined(MBEDTLS_X509_REMOVE_HOSTNAME_VERIFICATION)
     *
     * TLSSocket requires hostname used to verify the certificate.
     * If hostname is not given in constructor, this function must be used before
     * starting the TLS handshake.
     *
     * @param hostname     Hostname of the remote host, used for certificate checking.
     */
    void set_hostname(const char *hostname);

    /** Sets the certification of Root CA.
     *
     * @note Must be called before calling connect()
     *
     * @param root_ca Root CA Certificate in any Mbed TLS-supported format.
     * @param len     Length of certificate (including terminating 0 for PEM).
     * @retval NSAPI_ERROR_OK on success.
     * @retval NSAPI_ERROR_NO_MEMORY in case there is not enough memory to allocate certificate.
     * @retval NSAPI_ERROR_PARAMETER in case the provided root_ca parameter failed parsing.
     */
    nsapi_error_t set_root_ca_cert(const void *root_ca, size_t len);

    /** Sets the certification of Root CA.
     *
     * @note Must be called before calling connect()
     *
     * @param root_ca_pem Root CA Certificate in PEM format.
     * @retval NSAPI_ERROR_OK on success.
     * @retval NSAPI_ERROR_NO_MEMORY in case there is not enough memory to allocate certificate.
     * @retval NSAPI_ERROR_PARAMETER in case the provided root_ca parameter failed parsing.
     */
    nsapi_error_t set_root_ca_cert(const char *root_ca_pem);

    /** Sets the certification of Root CA.
     *
     * @note Must be called before calling connect()
     *
     * @param root_ca Path containing Root CA Certificate files in any Mbed TLS-supported format.
     * @retval NSAPI_ERROR_OK on success.
     * @retval NSAPI_ERROR_NO_MEMORY in case there is not enough memory to allocate certificate.
     * @retval NSAPI_ERROR_PARAMETER in case the provided root_ca parameter failed parsing.
     *
     */
    nsapi_error_t set_root_ca_cert_path(const char *root_ca);

    /** Sets client certificate, and client private key.
     *
     * @param client_cert Client certification in PEM or DER format.
     * @param client_cert_len Certificate size including the terminating null byte for PEM data.
     * @param client_private_key_pem Client private key in PEM or DER format.
     * @param client_private_key_len Key size including the terminating null byte for PEM data
     * @retval NSAPI_ERROR_OK on success.
     * @retval NSAPI_ERROR_PARAMETER in case the provided root_ca parameter failed parsing.
     */
    nsapi_error_t set_client_cert_key(const void *client_cert, size_t client_cert_len,
                                      const void *client_private_key_pem, size_t client_private_key_len);

    /** Sets client certificate, and client private key.
     *
     * @param client_cert_pem Client certification in PEM format.
     * @param client_private_key_pem Client private key in PEM format.
     * @retval NSAPI_ERROR_OK on success.
     * @retval NSAPI_ERROR_PARAMETER in case the provided root_ca parameter failed parsing.
     */
    nsapi_error_t set_client_cert_key(const char *client_cert_pem, const char *client_private_key_pem);

    /** Send data over a TLS socket.
     *
     *  The socket must be connected to a remote host. Returns the number of
     *  bytes sent from the buffer.
     *
     *  @param data     Buffer of data to send to the host.
     *  @param size     Size of the buffer in bytes.
     *  @retval         int Number of sent bytes on success
     *  @retval         NSAPI_ERROR_NO_SOCKET in case socket was not created correctly.
     *  @retval         NSAPI_ERROR_WOULD_BLOCK in case non-blocking mode is enabled
     *                  and send cannot be performed immediately.
     *  @retval         NSAPI_ERROR_DEVICE_ERROR in case of tls-related errors.
     *                  See @ref mbedtls_ssl_write.
     */
    nsapi_error_t send(const void *data, nsapi_size_t size) override;

    /** Receive data over a TLS socket.
     *
     *  The socket must be connected to a remote host. Returns the number of
     *  bytes received into the buffer.
     *
     *  @param data     Destination buffer for data received from the host.
     *  @param size     Size of the buffer in bytes.
     *  @retval         int Number of sent bytes on success
     *  @retval         NSAPI_ERROR_NO_SOCKET in case socket was not created correctly.
     *  @retval         NSAPI_ERROR_WOULD_BLOCK in case non-blocking mode is enabled
     *                  and send cannot be performed immediately.
     *  @retval         NSAPI_ERROR_DEVICE_ERROR in case of tls-related errors.
     *                  See @ref mbedtls_ssl_read.
     *  @return         0 if no data is available to be received
     *                  and the peer has performed an orderly shutdown.
     */
    nsapi_size_or_error_t recv(void *data, nsapi_size_t size) override;

    /* = Functions inherited from Socket = */
    nsapi_error_t close() override;
    /**
     *  Connect the transport socket and start handshake.
     *
     *  @note: In case connect() returns an error, the state of the socket is
     *  unspecified. A new socket should be created before reconnecting.
     *
     *  See @ref Socket::connect and @ref start_handshake
     */
    nsapi_error_t connect(const SocketAddress &address = SocketAddress()) override;
    nsapi_size_or_error_t sendto(const SocketAddress &address, const void *data, nsapi_size_t size) override;
    nsapi_size_or_error_t recvfrom(SocketAddress *address,
                                   void *data, nsapi_size_t size) override;
    nsapi_error_t bind(const SocketAddress &address) override;
    void set_blocking(bool blocking) override;
    void set_timeout(int timeout) override;
    void sigio(mbed::Callback<void()> func) override;
    nsapi_error_t setsockopt(int level, int optname, const void *optval, unsigned optlen) override;
    nsapi_error_t getsockopt(int level, int optname, void *optval, unsigned *optlen) override;
    Socket *accept(nsapi_error_t *error = NULL) override;
    nsapi_error_t listen(int backlog = 1) override;
    nsapi_error_t getpeername(SocketAddress *address) override;

#if defined(MBEDTLS_X509_CRT_PARSE_C) || defined(DOXYGEN_ONLY)
    /** Get own certificate directly from Mbed TLS.
     *
     * @return Internal Mbed TLS X509 structure.
     */
    mbedtls_x509_crt *get_own_cert();

    /** Set own certificate directly to Mbed TLS.
     *
     * @param crt Mbed TLS X509 certificate chain.
     * @return error code from mbedtls_ssl_conf_own_cert().
     */
    int set_own_cert(mbedtls_x509_crt *crt);

    /** Get CA chain structure.
     *
     * @return Mbed TLS X509 certificate chain.
     */
    mbedtls_x509_crt *get_ca_chain();

    /** Set CA chain directly to Mbed TLS.
     *
     * @param crt Mbed TLS X509 certificate chain.
     */
    void set_ca_chain(mbedtls_x509_crt *crt);
#endif

    /** Get internal Mbed TLS configuration structure.
     *
     * @return Mbed TLS SSL config.
     */
    mbedtls_ssl_config *get_ssl_config();

    /** Override Mbed TLS configuration.
     *
     * @param conf Mbed TLS SSL configuration structure.
     */
    void set_ssl_config(mbedtls_ssl_config *conf);

    /** Get internal Mbed TLS context structure.
     *
     * @return SSL context.
     */
    mbedtls_ssl_context *get_ssl_context();

protected:
#ifndef DOXYGEN_ONLY
    /** Initiates TLS Handshake.
     *
     *  Initiates a TLS handshake to a remote peer.
     *  Underlying transport socket should already be connected.
     *
     *  Root CA certification must be set by set_ssl_ca_pem() before
     *  calling this function.
     *
     *  For non-blocking purposes, this functions needs to know whether this
     *  was a first call to Socket::connect() API so that NSAPI_ERROR_INPROGRESS
     *  does not happen twice.
     *
     *  @param        first_call is this a first call to Socket::connect() API.
     *  @retval       NSAPI_ERROR_OK if we happen to complete the request on the first call.
     *  @retval       NSAPI_ERROR_IN_PROGRESS if the first call did not complete the request.
     *  @retval       NSAPI_ERROR_NO_SOCKET in case the transport socket was not created correctly.
     *  @retval       NSAPI_ERROR_AUTH_FAILURE in case of tls-related authentication errors.
     *                See @ref mbedtls_ctr_drbg_seed or @ref mbedtls_hmac_drbg_seed, @ref mbedtls_ssl_setup. @ref mbedtls_ssl_handshake.
     */
    nsapi_error_t start_handshake(bool first_call);

    bool is_handshake_started() const;

    void event();
#endif



private:
    /** Continue already initialized handshake */
    nsapi_error_t continue_handshake();
    /**
     * Helper for pretty-printing Mbed TLS error codes
     */
    static void print_mbedtls_error(const char *name, int err);

#if MBED_CONF_TLS_SOCKET_DEBUG_LEVEL > 0
    /**
     * Debug callback for Mbed TLS
     * Just prints on the USB serial port
     */
    static void my_debug(void *ctx, int level, const char *file, int line,
                         const char *str);

    /**
     * Certificate verification callback for Mbed TLS
     * Here we only use it to display information on each cert in the chain
     */
    static int my_verify(void *data, mbedtls_x509_crt *crt, int depth, uint32_t *flags);

#endif /* MBED_CONF_TLS_SOCKET_DEBUG_LEVEL > 0 */

    /**
     * Receive callback for Mbed TLS
     */
    static int ssl_recv(void *ctx, unsigned char *buf, size_t len);

    /**
     * Send callback for Mbed TLS
     */
    static int ssl_send(void *ctx, const unsigned char *buf, size_t len);

    mbedtls_ssl_context _ssl;
#ifdef MBEDTLS_X509_CRT_PARSE_C
    mbedtls_pk_context _pkctx;
#endif

    DRBG_CTX _drbg;

    mbedtls_entropy_context _entropy;

    rtos::EventFlags _event_flag;
    mbed::Callback<void()> _sigio;
    Socket *_transport;
    int _timeout = -1;

#ifdef MBEDTLS_X509_CRT_PARSE_C
    mbedtls_x509_crt *_cacert = nullptr;
    mbedtls_x509_crt *_clicert = nullptr;
#endif
    mbedtls_ssl_config *_ssl_conf = nullptr;

    bool _connect_transport: 1;
    bool _close_transport: 1;
    bool _tls_initialized: 1;
    bool _handshake_completed: 1;
    bool _cacert_allocated: 1;
    bool _clicert_allocated: 1;
    bool _ssl_conf_allocated: 1;

};

#endif /* MBEDTLS_SSL_CLI_C */
#endif // _MBED_HTTPS_TLS_SOCKET_WRAPPER_H_
/** @} */
