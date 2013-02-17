/*
  Copyright (C) 2005, 2006, 2007, 2008, 2011, 2012 Tadashi Watanabe <wac@umiushi.org>

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
        }

        SkkSocket() :
                timer_clear_value_(),
                getaddrinfo_result_(EAI_SYSTEM),
                internal_addrinfo_(0)
        {
                timer_clear_value_.it_interval.tv_sec = 0;
                timer_clear_value_.it_interval.tv_nsec = 0;
                timer_clear_value_.it_value.tv_sec = 0;
                timer_clear_value_.it_value.tv_nsec = 0;
        }

        static int send(int fd, const void *buffer, int size)
        {
#ifdef YASKKSERV_CONFIG_MACRO_HAVE_SYMBOL_MSG_NOSIGNAL
                return static_cast<int>(::send(fd, buffer, size, MSG_NOSIGNAL));
#else  // YASKKSERV_CONFIG_MACRO_HAVE_SYMBOL_MSG_NOSIGNAL
                return static_cast<int>(::send(fd, buffer, size, 0));
#endif  // YASKKSERV_CONFIG_MACRO_HAVE_SYMBOL_MSG_NOSIGNAL
        }

        static int receive(int fd, void *buffer, int size)
        {
                return static_cast<int>(recv(fd, buffer, size, 0));
        }

        // socket file descriptor を返します。失敗ならば -1 を返します。 timeout 秒でタイムアウトします。
        int prepareASyncSocket(const char *server, const char *service, float timeout)
        {
                DEBUG_PRINTF("prepareASyncSocket(%s, %s, %6.2f)\n", server, service, timeout);
                struct addrinfo *addrinfo = prepareASyncSocketGetAddrinfo(server, service, timeout);
                if (addrinfo == 0)
                {
                        return -1;
                }
                int socket_fd = prepareASyncSocketGetSocketFd(addrinfo, timeout);
                freeInternalAddrinfo();
                return socket_fd;
        }

private:
        static void sighandler(int signum)
        {
                DEBUG_PRINTF("sighandler(%d)\n", signum);
                siglongjmp(jump_buffer_, -1);
        }

        // タイムアウト付きで addrinfo を返します。 addrinfo が 0 以外の場合は必ず freeaddrinfo() で解放する必要があることに注意が必要です。失敗ならば 0 を返します。
        struct addrinfo *prepareASyncSocketGetAddrinfo(const char *server, const char *service, float timeout)
        {
                DEBUG_ASSERT_POINTER(server);
                signal(SIGALRM, sighandler);
                if (sigsetjmp(jump_buffer_, 1) == 0)
                {
                        struct itimerspec set_value;
                        {
                                int64_t tmp = static_cast<int64_t>(timeout) * 1000 * 1000 * 1000;
                                time_t second = tmp / 1000 / 1000 / 1000;
                                long nsecond = tmp - second * 1000 * 1000 * 1000;
                                set_value.it_interval.tv_sec = 0;
                                set_value.it_interval.tv_nsec = 0;
                                set_value.it_value.tv_sec = second;
                                set_value.it_value.tv_nsec = nsecond;
                        }
                        if (timer_create(CLOCK_REALTIME, 0, &timer_id_) == -1)
                        {
                                DEBUG_PRINTF("    timer_create() failed\n");
                                return 0;
                        }
                        DEBUG_PRINTF("    timer_id_=%d\n", timer_id_);
                        if (timer_settime(timer_id_, 0, &set_value, 0) == -1)
                        {
                                DEBUG_PRINTF("    timer_settime() failed timer_id_=%d\n", timer_id_);
                                timer_delete(timer_id_);
                                return 0;
                        }
                        DEBUG_PRINTF("    try getaddrinfo\n");
                        struct addrinfo hints;
                        memset(&hints, 0, sizeof(hints));
                        hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;
                        hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
                        hints.ai_socktype = SOCK_STREAM;
                        hints.ai_protocol = 0;
                        hints.ai_addr = 0;
                        hints.ai_canonname = 0;
                        hints.ai_next = 0;
                        getaddrinfo_result_ = EAI_SYSTEM;
                        internal_addrinfo_ = 0;
                        getaddrinfo_result_ = getaddrinfo(server, service, &hints, &internal_addrinfo_);
                        int clear_timer_settime_result = timer_settime(timer_id_, 0, &timer_clear_value_, 0);
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
                                DEBUG_PRINTF("    timer_settime() clear failed timer_id_=%d\n", timer_id_);
                                freeInternalAddrinfo();
                        }
                        if (timer_delete(timer_id_) == -1)
                        {
                                DEBUG_PRINTF("    timer_delete() failed timer_id_=%d\n", timer_id_);
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
                        timer_settime(timer_id_, 0, &timer_clear_value_, 0);
                        timer_delete(timer_id_);
                        DEBUG_PRINTF("signal jmp\n");
                }
                return internal_addrinfo_;
        }

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
        static timer_t timer_id_;

        struct itimerspec timer_clear_value_;
        int getaddrinfo_result_;
        struct addrinfo *internal_addrinfo_;
};
}

#endif  // SKK_SOCKET_HPP
