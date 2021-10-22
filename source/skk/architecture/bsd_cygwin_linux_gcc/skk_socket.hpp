/*
  Copyright (C) 2005-2021 Tadashi Watanabe <twacc2020@gmail.com>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef SKK_SOCKET_HPP
#define SKK_SOCKET_HPP

#include "skk_gcc.hpp"

namespace YaSkkServ
{
class SkkSocket
{
        SkkSocket(SkkSocket &source);
        SkkSocket& operator=(SkkSocket &source);

public:
        virtual ~SkkSocket()
        {
#if defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                shutdownASyncSocketSsl();
                if (ssl_ctx_)
                {
                        SSL_CTX_free(ssl_ctx_);
                }
#endif  // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
        }

        SkkSocket() :
#if defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                ssl_ctx_(0),
                ssl_(0),
#endif // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                getaddrinfo_result_(EAI_SYSTEM),
                internal_addrinfo_(0)
        {
#if defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
#ifdef YASKKSERV_DEBUG
                SSL_load_error_strings();
#endif  // YASKKSERV_DEBUG
                SSL_library_init();

                ssl_ctx_ = SSL_CTX_new(SSLv23_client_method());
                if (ssl_ctx_ == 0)
                {
#ifdef YASKKSERV_DEBUG
                        char error_buffer[1024];
                        fprintf(stderr, "%s\n", ERR_error_string(ERR_get_error(), error_buffer));
#endif  // YASKKSERV_DEBUG
                        DEBUG_ASSERT(0);
                        exit(1);
                }
#endif // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
        }

        static int send(int fd, const void *buffer, int size)
        {
#ifdef YASKKSERV_CONFIG_MACRO_HAVE_SYMBOL_MSG_NOSIGNAL
                return static_cast<int>(::send(fd, buffer, static_cast<size_t>(size), MSG_NOSIGNAL));
#else  // YASKKSERV_CONFIG_MACRO_HAVE_SYMBOL_MSG_NOSIGNAL
                return static_cast<int>(::send(fd, buffer, size, 0));
#endif  // YASKKSERV_CONFIG_MACRO_HAVE_SYMBOL_MSG_NOSIGNAL
        }

        static int receive(int fd, void *buffer, int size)
        {
                return static_cast<int>(recv(fd, buffer, static_cast<size_t>(size), 0));
        }

#if defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
        int writeSsl(const void *buffer, int size)
        {
                DEBUG_ASSERT_POINTER(ssl_ctx_);
                DEBUG_ASSERT_POINTER(ssl_);
                return static_cast<int>(SSL_write(ssl_, buffer, size));
        }
        int readSsl(void *buffer, int size)
        {
                DEBUG_ASSERT_POINTER(ssl_ctx_);
                DEBUG_ASSERT_POINTER(ssl_);
                return static_cast<int>(SSL_read(ssl_, buffer, size));
        }
        void shutdownASyncSocketSsl()
        {
                DEBUG_ASSERT_POINTER(ssl_ctx_);
                DEBUG_ASSERT_POINTER(ssl_);
                if (ssl_)
                {
                        for (;;)
                        {
                                int shutdown_result = SSL_shutdown(ssl_);
                                if (shutdown_result == 1)
                                {
                                        break;
                                }
                                else
                                {
                                        int ssl_error = SSL_get_error(ssl_, shutdown_result);
                                        if (ssl_error == SSL_ERROR_SYSCALL)
                                        {
                                                DEBUG_PRINTF("SSL_ERROR_SYSCALL\n");
                                                break;
                                        }
                                        else if ((ssl_error == SSL_ERROR_WANT_READ) ||
                                                 (ssl_error == SSL_ERROR_WANT_WRITE))
                                        {
                                                DEBUG_PRINTF("  shutdown ssl_error=%d\n", ssl_error);
                                        }
                                        else
                                        {
                                                DEBUG_PRINTF("SSL_shutdown() failed\n");
                                                DEBUG_ASSERT(0);
                                                break;
                                        }
                                }
                                usleep(10 * 1000);
                        }
                        SSL_free(ssl_);
                        ssl_ = 0;
                }
        }
        bool isBusySsl(int send_receive_result)
        {
                DEBUG_ASSERT_POINTER(ssl_ctx_);
                DEBUG_ASSERT_POINTER(ssl_);
                if (ssl_)
                {
                        int ssl_error = SSL_get_error(ssl_, send_receive_result);
                        DEBUG_PRINTF("send_receive_result=%d  ssl_error=%d\n", send_receive_result, ssl_error);
                        if ((ssl_error == SSL_ERROR_WANT_READ) || (ssl_error == SSL_ERROR_WANT_WRITE) || (ssl_error == SSL_ERROR_ZERO_RETURN))
                        {
                                return true;
                        }
                }
                return false;
        }
#endif // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)

        // socket file descriptor を返します。失敗ならば -1 を返します。 timeout 秒でタイムアウトします。
        int prepareASyncSocket(const char *server, const char *service, float timeout, bool is_ipv6)
        {
                DEBUG_PRINTF("prepareASyncSocket(%s, %s, %6.2f)\n", server, service, timeout);
                struct addrinfo *addrinfo = prepareASyncSocketGetAddrinfo(server, service, timeout, is_ipv6);
                if (addrinfo == 0)
                {
                        return -1;
                }
                int socket_fd = prepareASyncSocketGetSocketFd(addrinfo, timeout);
                freeInternalAddrinfo();
                return socket_fd;
        }

#if defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
        int prepareASyncSocketSsl(const char *server, const char *service, float timeout, bool is_ipv6)
        {
                DEBUG_ASSERT_POINTER(ssl_ctx_);
                DEBUG_PRINTF("prepareASyncSocketSsl(%s, %s, %6.2f)\n", server, service, timeout);
                struct addrinfo *addrinfo = prepareASyncSocketGetAddrinfo(server, service, timeout, is_ipv6);
                if (addrinfo == 0)
                {
                        return -1;
                }
                int socket_fd = prepareASyncSocketGetSocketFd(addrinfo, timeout);
                freeInternalAddrinfo();
                if (socket_fd < 0)
                {
                        return -1;
                }
                ssl_ = SSL_new(ssl_ctx_);
                if (ssl_ == 0)
                {
#ifdef YASKKSERV_DEBUG
                        char error_buffer[1024];
                        fprintf(stderr, "%s\n", ERR_error_string(ERR_get_error(), error_buffer));
#endif  // YASKKSERV_DEBUG
                        close(socket_fd);
                        return -1;
                }
                if (SSL_set_fd(ssl_, socket_fd) == 0)
                {
#ifdef YASKKSERV_DEBUG
                        char error_buffer[1024];
                        fprintf(stderr, "%s\n", ERR_error_string(ERR_get_error(), error_buffer));
#endif  // YASKKSERV_DEBUG
                        // SSL_shutdown(ssl_);
                        // ssl_ = 0;
                        shutdownASyncSocketSsl();
                        close(socket_fd);
                        return -1;
                }
#ifdef YASKKSERV_CONFIG_HEADER_HAVE_RAND_POLL
                RAND_poll();
#endif  // YASKKSERV_CONFIG_HEADER_HAVE_RAND_POLL
                // if (RAND_status() == 0) // FIXME!
                // {
                //         SSL_shutdown(ssl_);
                //         ssl_ = 0;
                //         close(socket_fd);
                //         return -1;
                // }
                for (;;)
                {
                        int ssl_connect_result = SSL_connect(ssl_);
                        if (ssl_connect_result == 1)
                        {
                                break;
                        }
                        else
                        {
                                int ssl_error = SSL_get_error(ssl_, ssl_connect_result);
                                if ((ssl_error == SSL_ERROR_WANT_READ) || (ssl_error == SSL_ERROR_WANT_WRITE) || (ssl_error == SSL_ERROR_ZERO_RETURN))
                                // if (isBusySsl(ssl_connect_result))
                                {
                                        usleep(10 * 1000);
                                }
                                else
                                {
                                        shutdownASyncSocketSsl();
                                        close(socket_fd);
                                        return -1;
                                }
                        }
                }
                return socket_fd;
        }
#endif // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)

private:
        static void sighandler(int signum) __attribute__ ((noreturn))
        {
                (void)signum;
                DEBUG_PRINTF("sighandler(%d)\n", signum);
                siglongjmp(jump_buffer_, -1);
        }

#ifdef YASKKSERV_CONFIG_MACRO_HAVE_SYMBOL_RESOLV_RETRANS_RETRY
        struct addrinfo *prepareASyncSocketGetAddrinfo(const char *server, const char *service, float timeout, bool is_ipv6)
        {
                DEBUG_ASSERT_POINTER(server);
                DEBUG_PRINTF("    try getaddrinfo\n");
                res_state res = &_res;
                int int_timeout = static_cast<int>(timeout);
                if (int_timeout < 1)
                {
                        int_timeout = 1;
                }
                res->retrans = int_timeout;
                res->retry = 1;
                DEBUG_PRINTF("  res->retrans=%d  res->retry=%d\n", res->retrans, res->retry);
                struct addrinfo hints;
                memset(&hints, 0, sizeof(hints));
                hints.ai_flags = 0;
                if (is_ipv6)
                {
                        hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
                }
                else
                {
                        hints.ai_family = AF_INET;
                }
                hints.ai_socktype = SOCK_STREAM;
                hints.ai_protocol = 0;
                hints.ai_addr = 0;
                hints.ai_canonname = 0;
                hints.ai_next = 0;
                getaddrinfo_result_ = EAI_SYSTEM;
                internal_addrinfo_ = 0;
                getaddrinfo_result_ = getaddrinfo(server, service, &hints, &internal_addrinfo_);
                DEBUG_PRINTF("    done getaddrinfo getaddrinfo_result_=%d internal_addrinfo_=%p\n", getaddrinfo_result_, internal_addrinfo_);
                if (getaddrinfo_result_ == 0)
                {
                        DEBUG_PRINTF("    getaddrinfo() success internal_addrinfo_=%p\n", internal_addrinfo_);
                }
                else
                {
                        internal_addrinfo_ = 0;
                }
                return internal_addrinfo_;
        }
#else  // YASKKSERV_CONFIG_MACRO_HAVE_SYMBOL_RESOLV_RETRANS_RETRY
        // タイムアウト付きで addrinfo を返します。 addrinfo が 0 以外の場合は必ず freeaddrinfo() で解放する必要があることに注意が必要です。失敗ならば 0 を返します。
#ifdef YASKKSERV_CONFIG_FUNCTION_HAVE_TIMER_CREATE
        struct addrinfo *prepareASyncSocketGetAddrinfo(const char *server, const char *service, float timeout, bool is_ipv6)
        {
                DEBUG_ASSERT_POINTER(server);
                signal(SIGALRM, sighandler);
                static timer_t timer_id;
                struct itimerspec timer_clear_value;
                timer_clear_value.it_interval.tv_sec = 0;
                timer_clear_value.it_interval.tv_nsec = 0;
                timer_clear_value.it_value.tv_sec = 0;
                timer_clear_value.it_value.tv_nsec = 0;
                if (sigsetjmp(jump_buffer_, 1) == 0)
                {
                        struct itimerspec set_value;
                        {
                                int64_t tmp = static_cast<int64_t>(timeout) * 1000 * 1000 * 1000;
                                time_t second = static_cast<time_t>(tmp / 1000 / 1000 / 1000);
                                long nsecond = static_cast<long>(tmp - second * 1000 * 1000 * 1000);
                                set_value.it_interval.tv_sec = 0;
                                set_value.it_interval.tv_nsec = 0;
                                set_value.it_value.tv_sec = second;
                                set_value.it_value.tv_nsec = nsecond;
                        }
                        if (timer_create(CLOCK_REALTIME, 0, &timer_id) == -1)
                        {
                                DEBUG_PRINTF("    timer_create() failed\n");
                                return 0;
                        }
                        DEBUG_PRINTF("    timer_id=%d\n", timer_id);
                        if (timer_settime(timer_id, 0, &set_value, 0) == -1)
                        {
                                DEBUG_PRINTF("    timer_settime() failed timer_id=%d\n", timer_id);
                                timer_delete(timer_id);
                                return 0;
                        }
                        DEBUG_PRINTF("    try getaddrinfo\n");
                        struct addrinfo hints;
                        memset(&hints, 0, sizeof(hints));
                        hints.ai_flags = 0;
                        if (is_ipv6)
                        {
                                hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
                        }
                        else
                        {
                                hints.ai_family = AF_INET;
                        }
                        hints.ai_socktype = SOCK_STREAM;
                        hints.ai_protocol = 0;
                        hints.ai_addr = 0;
                        hints.ai_canonname = 0;
                        hints.ai_next = 0;
                        getaddrinfo_result_ = EAI_SYSTEM;
                        internal_addrinfo_ = 0;
                        getaddrinfo_result_ = getaddrinfo(server, service, &hints, &internal_addrinfo_);
                        int clear_timer_settime_result = timer_settime(timer_id, 0, &timer_clear_value, 0);
                        DEBUG_PRINTF("    done getaddrinfo getaddrinfo_result_=%d internal_addrinfo_=%p\n", getaddrinfo_result_, internal_addrinfo_);
                        if (getaddrinfo_result_ == 0)
                        {
                                DEBUG_PRINTF("    getaddrinfo() success internal_addrinfo_=%p\n", internal_addrinfo_);
                        }
                        else
                        {
                                internal_addrinfo_ = 0;
                        }
                        if (clear_timer_settime_result == -1)
                        {
                                DEBUG_PRINTF("    timer_settime() clear failed timer_id=%d\n", timer_id);
                                freeInternalAddrinfo();
                        }
                        if (timer_delete(timer_id) == -1)
                        {
                                DEBUG_PRINTF("    timer_delete() failed timer_id=%d\n", timer_id);
                                freeInternalAddrinfo();
                        }
                }
                else
                {
                        if (internal_addrinfo_)
                        {
                                DEBUG_PRINTF("    BAD TIMING freeaddrinfo() getaddrinfo_result_=%d internal_addrinfo_=%p\n", getaddrinfo_result_, internal_addrinfo_);
                                freeInternalAddrinfo();
                        }
                        timer_settime(timer_id, 0, &timer_clear_value, 0);
                        timer_delete(timer_id);
                        DEBUG_PRINTF("signal jmp\n");
                }
                return internal_addrinfo_;
        }
#else  // YASKKSERV_CONFIG_FUNCTION_HAVE_TIMER_CREATE
        struct addrinfo *prepareASyncSocketGetAddrinfo(const char *server, const char *service, float timeout, bool is_ipv6)
        {
                DEBUG_ASSERT_POINTER(server);
                signal(SIGALRM, sighandler);
                if (sigsetjmp(jump_buffer_, 1) == 0)
                {
                        {
                                struct itimerval new_value;
                                {
                                        int64_t tmp = static_cast<int64_t>(timeout) * 1000 * 1000;
                                        time_t second = tmp / 1000 / 1000;
                                        long usecond = tmp - second * 1000 * 1000;
                                        new_value.it_interval.tv_sec = 0;
                                        new_value.it_interval.tv_usec = 0;
                                        new_value.it_value.tv_sec = second;
                                        new_value.it_value.tv_usec = usecond;
                                }
                                if (setitimer(ITIMER_REAL, &new_value, 0) == -1)
                                {
                                        DEBUG_PRINTF("    setitimer() failed\n");
                                        return 0;
                                }
                        }
                        DEBUG_PRINTF("    try getaddrinfo\n");
                        struct addrinfo hints;
                        memset(&hints, 0, sizeof(hints));
                        if (is_ipv6)
                        {
                                hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;
                                hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
                        }
                        else
                        {
                                hints.ai_flags = AI_V4MAPPED;
                                hints.ai_family = AF_INET;
                        }
                        hints.ai_socktype = SOCK_STREAM;
                        hints.ai_protocol = 0;
                        hints.ai_addr = 0;
                        hints.ai_canonname = 0;
                        hints.ai_next = 0;
                        getaddrinfo_result_ = EAI_SYSTEM;
                        internal_addrinfo_ = 0;
                        struct itimerval cancel_value;
                        cancel_value.it_interval.tv_sec = 0;
                        cancel_value.it_interval.tv_usec = 0;
                        cancel_value.it_value.tv_sec = 0;
                        cancel_value.it_value.tv_usec = 0;
                        getaddrinfo_result_ = getaddrinfo(server, service, &hints, &internal_addrinfo_);
                        int clear_timer_result = setitimer(ITIMER_REAL, &cancel_value, 0);
                        DEBUG_PRINTF("    done getaddrinfo getaddrinfo_result_=%d internal_addrinfo_=%p\n", getaddrinfo_result_, internal_addrinfo_);
                        if (getaddrinfo_result_ == 0)
                        {
                                DEBUG_PRINTF("    getaddrinfo() success internal_addrinfo_=%p\n", internal_addrinfo_);
                        }
                        else
                        {
                                internal_addrinfo_ = 0;
                        }
                        if (clear_timer_result == -1)
                        {
                                DEBUG_PRINTF("    clear timer failed\n");
                                freeInternalAddrinfo();
                        }
                }
                else
                {
                        if (internal_addrinfo_)
                        {
                                DEBUG_PRINTF("    BAD TIMING freeaddrinfo() getaddrinfo_result_=%d internal_addrinfo_=%p\n", getaddrinfo_result_, internal_addrinfo_);
                                freeInternalAddrinfo();
                        }
                        DEBUG_PRINTF("signal jmp\n");
                }
                return internal_addrinfo_;
        }
#endif // YASKKSERV_CONFIG_FUNCTION_HAVE_TIMER_CREATE
#endif // YASKKSERV_CONFIG_MACRO_HAVE_SYMBOL_RESOLV_RETRANS_RETRY

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
        // 失敗ならば -1 を返します。
        static int prepareASyncSocketGetSocketFd(struct addrinfo *addrinfo, float timeout)
        {
                DEBUG_ASSERT_POINTER(addrinfo);
                int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
                if (socket_fd < 0)
                {
                        return -1;
                }
                fcntl(socket_fd, F_SETFL, O_NONBLOCK);
                bool socket_fd_fail_close_flag = true;
                int connect_result = connect(socket_fd, addrinfo->ai_addr, addrinfo->ai_addrlen);
                DEBUG_PRINTF("connect_result=%d  errno=%d\n", connect_result);
                if ((connect_result == -1) && (errno == EINPROGRESS))
                {
                        fd_set fds;
                        struct timeval tv;
                        int sec = static_cast<int>(timeout);
                        int usec = static_cast<int>((timeout - static_cast<float>(sec))) * 1000 * 1000;
                        tv.tv_sec = sec;
                        tv.tv_usec = usec;
                        FD_ZERO(&fds);
                        FD_SET(socket_fd, &fds);
                        DEBUG_PRINTF("try select\n");
                        int select_result = select(socket_fd + 1, 0, &fds, 0, &tv);
                        if (select_result == 0)
                        {
                                // timeout
                                DEBUG_PRINTF("select timeout\n");
                        }
                        else if (select_result != -1)
                        {
                                DEBUG_PRINTF("select success\n");
                                int optval;
                                socklen_t optlen = sizeof(optval);
                                if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &optval, &optlen) == 0)
                                {
                                        DEBUG_PRINTF("optval=%d  optlen=%d\n", optval, optlen);
                                        if (optval == 0)
                                        {
                                                socket_fd_fail_close_flag = false;
                                        }
                                }
                        }
                }
                if (socket_fd_fail_close_flag)
                {
                        close(socket_fd);
                        return -1;
                }
                return socket_fd;
        }
#pragma GCC diagnostic pop

        void freeInternalAddrinfo()
        {
                if (internal_addrinfo_)
                {
                        DEBUG_PRINTF("freeInternalAddrinfo() %p\n", internal_addrinfo_);
                        freeaddrinfo(internal_addrinfo_);
                        internal_addrinfo_ = 0;
                }
        }

private:
        static sigjmp_buf jump_buffer_;

#if defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
        SSL_CTX *ssl_ctx_;
        SSL *ssl_;
#endif // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
        int getaddrinfo_result_;
        struct addrinfo *internal_addrinfo_;
};
}

#endif  // SKK_SOCKET_HPP
