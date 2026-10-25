
#include "wserve.h"

// Enable logic related to HTTPS, using full RSA
// #define ENABLE_SECURE 1
// #if ENABLE_SECURE
#include "../BearSSL/inc/bearssl.h"
#include "../BearSSL/samples/chain-rsa.h"
#include "../BearSSL/samples/key-rsa.h"
#define SKEY RSA
// #endif


// ######################
// # SERVER HTTPS SETUP #
// ######################


typedef struct SERVER_TX_RX
{
    ssize_t (*read)(void *ctx, char *buf, size_t maxrecvsize);
    ssize_t (*write)(void *ctx, char *buf, size_t maxrecvsize);
    void (*close)(int fd, void *ctx);
}
SERVER_TX_RX;


ssize_t read_fd(void *ctx, char *buf, size_t maxrecvsize)
{
    int fd = *(int*)ctx;
    return recv(fd, buf, maxrecvsize, 0);
}

ssize_t read_fd_ssl(void *ctx, char *buf, size_t maxrecvsize)
{
    br_sslio_context *ioc = ctx;
    return br_sslio_read(ioc, buf, maxrecvsize);
}

ssize_t write_fd(void *ctx, char *buf, size_t n)
{
    int fd = *(int*)ctx;
    return _send(fd, buf, n);
}

ssize_t write_fd_ssl(void *ctx, char *buf, size_t n)
{
    br_sslio_context *ioc = ctx;
    return br_sslio_write_all(ioc, buf, n);
}

void close_fd(int fd, void *ctx)
{
    close(fd);
}

void close_fd_ssl(int fd, void *ctx)
{
    br_sslio_context *ioc = ctx;
    br_sslio_close(ioc);
    close(fd);
}

/*
 * Low-level data read callback for the simplified SSL I/O API.
 */
static int
sock_read(void *ctx, unsigned char *buf, size_t len)
{

	for (;;) {
		ssize_t rlen;

		rlen = read(*(int *)ctx, buf, len);
		if (rlen <= 0) {
			if (rlen < 0 && errno == EINTR) {
				continue;
			}
			return -1;
		}
		return (int)rlen;
	}
}

/*
 * Low-level data write callback for the simplified SSL I/O API.
 */
static int
sock_write(void *ctx, const unsigned char *buf, size_t len)
{
	for (;;) {
		ssize_t wlen;

		wlen = write(*(int *)ctx, buf, len);
		if (wlen <= 0) {
			if (wlen < 0 && errno == EINTR) {
				continue;
			}
			return -1;
		}
		return (int)wlen;
	}
}

br_sslio_context init_bearssl()
{
    // To be called at every client connection (child).
    int cfd;
    br_ssl_server_context sc;
	unsigned char iobuf[BR_SSL_BUFSIZE_BIDI];
	br_sslio_context ioc;

	// TODO: Using hardcoded certificates initially.
	br_ssl_server_init_full_rsa(&sc, CHAIN, CHAIN_LEN, &SKEY);

	br_ssl_engine_set_buffer(&sc.eng, iobuf, sizeof iobuf, 1);
	br_ssl_server_reset(&sc);
	br_sslio_init(&ioc, &sc.eng, sock_read, &cfd, sock_write, &cfd);

	// TODO: Basically need to change read functions/provide bearssl alternatives.

	return ioc;
}
