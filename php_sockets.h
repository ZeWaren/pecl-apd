/*
   +----------------------------------------------------------------------+
   | PHP version 4.0                                                      |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2001 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Chris Vandomelen <chrisv@b0rked.dhs.org>                    |
   |          Sterling Hughes  <sterling@php.net>                         |
   |                                                                      |
   | WinSock: Daniel Beulshausen <daniel@php4win.de>                      |
   +----------------------------------------------------------------------+
 */

#ifndef PHP_SOCKETS_H
#define PHP_SOCKETS_H

/* $Id$ */

#if HAVE_SOCKETS

extern zend_module_entry sockets_module_entry;
#define phpext_sockets_ptr &sockets_module_entry

#ifdef PHP_WIN32
#define PHP_SOCKETS_API __declspec(dllexport)
#include <winsock.h>
#else
#define PHP_SOCKETS_API
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#endif

PHP_MINIT_FUNCTION(sockets);
PHP_MINFO_FUNCTION(sockets);

PHP_FUNCTION(socket_fd_alloc);
PHP_FUNCTION(socket_fd_free);
PHP_FUNCTION(socket_fd_set);
PHP_FUNCTION(socket_fd_isset);
PHP_FUNCTION(socket_fd_clear);
PHP_FUNCTION(socket_fd_zero);
PHP_FUNCTION(socket_iovec_alloc);
PHP_FUNCTION(socket_iovec_free);
PHP_FUNCTION(socket_iovec_set);
PHP_FUNCTION(socket_iovec_fetch);
PHP_FUNCTION(socket_iovec_add);
PHP_FUNCTION(socket_iovec_delete);
PHP_FUNCTION(socket_select);
PHP_FUNCTION(socket_create_listen);
PHP_FUNCTION(socket_create_pair);
PHP_FUNCTION(socket_accept);
PHP_FUNCTION(socket_set_nonblock);
PHP_FUNCTION(socket_listen);
PHP_FUNCTION(socket_close);
PHP_FUNCTION(socket_write);
PHP_FUNCTION(socket_read);
PHP_FUNCTION(socket_getsockname);
PHP_FUNCTION(socket_getpeername);
PHP_FUNCTION(socket_create);
PHP_FUNCTION(socket_connect);
PHP_FUNCTION(socket_strerror);
PHP_FUNCTION(socket_bind);
PHP_FUNCTION(socket_recv);
PHP_FUNCTION(socket_send);
PHP_FUNCTION(socket_recvfrom);
PHP_FUNCTION(socket_sendto);
PHP_FUNCTION(socket_recvmsg);
PHP_FUNCTION(socket_sendmsg);
PHP_FUNCTION(socket_readv);
PHP_FUNCTION(socket_writev);
PHP_FUNCTION(socket_getopt);
PHP_FUNCTION(socket_setopt);
PHP_FUNCTION(socket_shutdown);
PHP_FUNCTION(socket_last_error);

typedef struct php_iovec {
	struct iovec	*iov_array;
	unsigned int	count;
} php_iovec_t;

#ifndef PHP_WIN32
typedef int SOCKET;
#endif

typedef struct {
	SOCKET	bsd_socket;
	int		type;
	int		error;
} php_socket;

typedef struct {
	fd_set	set;
	SOCKET	max_fd;
} php_fd_set;

typedef struct {
	long family;
	char info[256];
} php_sockaddr_storage;

typedef struct {
	zend_bool	use_system_read;
} php_sockets_globals;

/* Prototypes */
int open_listen_sock(php_socket **php_sock, int port, int backlog TSRMLS_DC);
int accept_connect(php_socket *in_sock, php_socket **new_sock, struct sockaddr *la TSRMLS_DC);
int php_read(int bsd_socket, void *buf, int maxlen);

#ifdef ZTS
#define SOCKETSG(v) (sockets_globals->v)
#define SOCKETSLS_FETCH() php_sockets_globals *sockets_globals = ts_resource(sockets_globals_id)
#else
#define SOCKETSG(v) (sockets_globals.v)
#define SOCKETSLS_FETCH()
#endif

#else
#define phpext_sockets_ptr NULL
#endif

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */

