/*
  Copyright (C) 2005, 2006, 2007, 2008, 2011, 2012, 2013, 2014 Tadashi Watanabe <wac@umiushi.org>

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
#ifndef SKK_SERVER_H
#define SKK_SERVER_H

#ifdef YASKKSERV_CONFIG_HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include "skk_architecture.hpp"
#include "skk_socket.hpp"
#include "skk_utility.hpp"
#include "skk_dictionary.hpp"
#include "skk_syslog.hpp"

namespace YaSkkServ
{
#ifdef YASKKSERV_DEBUG
#define SKK_MEMORY_DEBUG
#endif  // YASKKSERV_DEBUG

#ifdef SKK_MEMORY_DEBUG
//
// SKK_MEMORY_DEBUG が定義されている場合、バッファを
// SKK_MEMORY_DEBUG_MARGIN_SIZE * 2 だけ多く確保します。
//
// SKK_MEMORY_DEBUG_MARGIN_SIZE の大きさを持つ領域はバッファの前後に付加さ
// れます。マージンは memory_debug_set() で初期設定され、
// memory_debug_check() でチェックします。
//
// SKK_MEMORY_DEBUG が定義されない場合のバッファ
//    +----------+
//    | バッファ |
//    +----------+
//
// SKK_MEMORY_DEBUG が定義される場合のバッファ
//    +----------+----------+----------+
//    | マージン | バッファ | マージン |
//    +----------+----------+----------+
//
#define SKK_MEMORY_DEBUG_MARGIN_SIZE 4
inline void skk_memory_debug_set_(char *p)
{
        *(p + 0) = static_cast<char>(0xde);
        *(p + 1) = static_cast<char>(0xad);
        *(p + 2) = static_cast<char>(0xbe);
        *(p + 3) = static_cast<char>(0xef);
        DEBUG_PRINTF("skk_memory_debug_set_() %p\n"
                     ,
                     p);
}

inline void skk_memory_debug_check_(const char *p)
{
        if (((*(p + 0) & 0xff) != 0xde) ||
            ((*(p + 1) & 0xff) != 0xad) ||
            ((*(p + 2) & 0xff) != 0xbe) ||
            ((*(p + 3) & 0xff) != 0xef))
        {
                DEBUG_PRINTF("skk_memory_debug_check_() %p failed\n"
                             "0x %02x,%02x,%02x,%02x\n"
                             ,
                             p,
                             *(p + 0) & 0xff,
                             *(p + 1) & 0xff,
                             *(p + 2) & 0xff,
                             *(p + 3) & 0xff);
                DEBUG_ASSERT(0);
        }
}
#endif  // SKK_MEMORY_DEBUG




/// SKK サーバです。
class SkkServer
{
        SkkServer(SkkServer &source);
        SkkServer& operator=(SkkServer &source);

public:
        virtual ~SkkServer()
        {
                for (int i = 0; i != max_connection_; ++i)
                {
#ifdef SKK_MEMORY_DEBUG
                        delete[] ((work_ + i)->read_buffer - SKK_MEMORY_DEBUG_MARGIN_SIZE);
#else  // SKK_MEMORY_DEBUG
                        delete[] (work_ + i)->read_buffer;
#endif  // SKK_MEMORY_DEBUG
                }
                delete[] work_;
                syslog_.printf(1, SkkSyslog::LEVEL_INFO, "terminated");
        }

        SkkServer(const char *identifier, int port, int log_level, const char *address) :
                syslog_(identifier, log_level),
                work_(0),
                port_(port),
                address_(address),
                max_connection_(0),
                listen_queue_(0),
                file_descriptor_(0)
        {
        }

        void printFirstSyslog()
        {
                syslog_.printf(1, SkkSyslog::LEVEL_INFO, "version " YASKKSERV_VERSION " (port=%d)", port_);
        }

        virtual bool mainLoop() = 0;

protected:
/// send() に成功すれば真を返します。偽を返した場合 Work::closeAndReset() すべきです。
        bool send(int file_descriptor, const void *data, int data_size)
        {
                bool result = false;
                int send_size = 0;
                for (;;)
                {
                        int send_result = SkkSocket::send(file_descriptor, data, data_size);
                        if (send_result == -1)
                        {
                                break;
                        }
                        else
                        {
                                send_size += send_result;
                                if (send_size == data_size)
                                {
                                        result = true;
                                        break;
                                }
                        }
                }
                return result;
        }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
/// MainLoop() のイニシャライズ処理です。
        bool main_loop_initialize(int max_connection, int listen_queue)
        {
                max_connection_ = max_connection;
                listen_queue_ = listen_queue;
// delete[] はデストラクタで。
                work_ = new Work[max_connection_];

                for (int i = 0; i != max_connection_; ++i)
                {
#ifdef SKK_MEMORY_DEBUG
                        (work_ + i)->read_buffer = new char[SKK_MEMORY_DEBUG_MARGIN_SIZE + READ_BUFFER_SIZE + SKK_MEMORY_DEBUG_MARGIN_SIZE];
                        (work_ + i)->read_buffer += SKK_MEMORY_DEBUG_MARGIN_SIZE;
                        skk_memory_debug_set_((work_ + i)->read_buffer - SKK_MEMORY_DEBUG_MARGIN_SIZE);
                        skk_memory_debug_set_((work_ + i)->read_buffer - SKK_MEMORY_DEBUG_MARGIN_SIZE + SKK_MEMORY_DEBUG_MARGIN_SIZE + READ_BUFFER_SIZE);
#else  // SKK_MEMORY_DEBUG
                        (work_ + i)->read_buffer = new char[READ_BUFFER_SIZE];
#endif  // SKK_MEMORY_DEBUG
                        *((work_ + i)->read_buffer + 0) = '\0';
                }

#ifdef YASKKSERV_CONFIG_HAVE_SYSTEMD
                int number_of_fds = sd_listen_fds(1);
                if (number_of_fds == 1) {
                        file_descriptor_ = SD_LISTEN_FDS_START;
                        syslog_.printf(1, SkkSyslog::LEVEL_INFO, "Use fd=%d from systemd socket activation", file_descriptor_);
                        return true;
                }
#endif

                file_descriptor_ = socket(AF_INET, SOCK_STREAM, 0);
                if (file_descriptor_ == -1)
                {
                        return false;
                }
                fcntl(file_descriptor_, F_SETFL, O_NONBLOCK);
                {
                        int optval = 1;
                        setsockopt(file_descriptor_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
                }

                struct sockaddr_in socket_connect;
                SkkUtility::clearMemory(&socket_connect, sizeof(socket_connect));
                socket_connect.sin_family = AF_INET;
                if (! inet_aton(address_, &socket_connect.sin_addr)) {
                        SkkUtility::printf("invalid address\n");
                        return false;
                }
                socket_connect.sin_port = htons(static_cast<uint16_t>(port_));
                int retry = 3;
                while (bind(file_descriptor_, reinterpret_cast<struct sockaddr *>(&socket_connect), sizeof(socket_connect)) == -1)
                {
                        if (--retry <= 0)
                        {
                                close(file_descriptor_);
                                SkkUtility::printf("socket bind failed\n");
                                return false;
                        }
                        SkkUtility::sleep(1);
                }
                if ((listen(file_descriptor_, listen_queue_)) == -1)
                {
                        close(file_descriptor_);
                        return false;
                }
                return true;
        }
#pragma GCC diagnostic pop

/// 「見出し」の探索に成功したものとして "1" を付加した現在のバッファの「変換文字列」を send() します。
        void main_loop_send_found(int work_index, SkkDictionary *skk_dictionary)
        {
//
// mmap を使わない場合、メモリコピーを避けるためブロックリードバッファ
// を書き換え、 send() 後に戻しています。一見危なっかしいですが、 '1'
// を書き込む p - 1 は RAM であることが保証され、かつ必ず存在します。具
// 体的にはブロックバッファ中の以下の位置にあたります。
//
// mmap を使う場合、または ROM 上のデータを扱う場合には send() を複数回
// 呼ぶなどの処理が必要になることに注意が必要です。
//
//     p - 1
//       |
//       v
// みだしβ /変換文字列0/変換文字列1/
//        ^
//        |
//        p == getHenkanmojiretsuPointer()
//
                char *p = const_cast<char*>(skk_dictionary->getHenkanmojiretsuPointer());
                const int protocol_size = 1;
                const int cr_size = 1;
                char backup = *(p - 1);
                *(p - 1) = '1';
                if (!send((work_ + work_index)->file_descriptor, p - 1, protocol_size + skk_dictionary->getHenkanmojiretsuSize() + cr_size))
                {
                        (work_ + work_index)->closeAndReset();
                }
                *(p - 1) = backup;
        }

/// 「見出し」の探索に失敗したものとして "4" を付加した現在のバッファの「見出し」を send() します。
/**
 * 現在のバッファの末尾が \\n でない場合は \\n を追加して send() します。
 */
        void main_loop_send_not_found(int work_index, int recv_result)
        {
// 2 回に分けて send() すれば素直になりますが、メモリをいじりまわして
// 1 回の send() にした方が高速なため、以下のようなコードになっています。
                DEBUG_ASSERT(recv_result > 1);
// read_buffer は recv() するサイズより大き目に確保されているので +
// recv_result にアクセスしても問題ありません。
                char backup_2 = *((work_ + work_index)->read_buffer + recv_result);
                int send_size;
                if (*((work_ + work_index)->read_buffer + recv_result - 1) != '\n')
                {
// 終端が  '\n' でなければ '\n' を追加します。
                        *((work_ + work_index)->read_buffer + recv_result) = '\n';
                        send_size = recv_result + 1;
                }
                else
                {
                        send_size = recv_result;
                }

                char backup = *((work_ + work_index)->read_buffer);
                *((work_ + work_index)->read_buffer) = '4';
                if (!send((work_ + work_index)->file_descriptor, (work_ + work_index)->read_buffer, send_size))
                {
                        (work_ + work_index)->closeAndReset();
                }
                *((work_ + work_index)->read_buffer) = backup;
                *((work_ + work_index)->read_buffer + recv_result) = backup_2;
        }

/// mainLoop() で select() します。
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
        int main_loop_select(fd_set &fd_set_read)
        {
                int file_descriptor_maximum = file_descriptor_;
                FD_ZERO(&fd_set_read);
                FD_SET(file_descriptor_, &fd_set_read);
                for (int i = 0; i != max_connection_; ++i)
                {
                        if ((work_ + i)->flag)
                        {
                                FD_SET((work_ + i)->file_descriptor, &fd_set_read);
                                if ((work_ + i)->file_descriptor > file_descriptor_maximum)
                                {
                                        file_descriptor_maximum = (work_ + i)->file_descriptor;
                                }
                        }
#ifdef SKK_MEMORY_DEBUG
                        skk_memory_debug_check_((work_ + i)->read_buffer - SKK_MEMORY_DEBUG_MARGIN_SIZE);
                        skk_memory_debug_check_((work_ + i)->read_buffer - SKK_MEMORY_DEBUG_MARGIN_SIZE + SKK_MEMORY_DEBUG_MARGIN_SIZE + READ_BUFFER_SIZE);
#endif  // SKK_MEMORY_DEBUG
                }
                int n = select(file_descriptor_maximum + 1, &fd_set_read, 0, 0, 0);
                if ((n == -1) && (errno == EINTR))
                {
                        syslog_.printf(1, SkkSyslog::LEVEL_INFO, "caught signal");
                }
                return n;
        }

        int main_loop_select_polling(fd_set &fd_set_read)
        {
                int file_descriptor_maximum = file_descriptor_;
                FD_ZERO(&fd_set_read);
                FD_SET(file_descriptor_, &fd_set_read);
                for (int i = 0; i != max_connection_; ++i)
                {
                        if ((work_ + i)->flag)
                        {
                                FD_SET((work_ + i)->file_descriptor, &fd_set_read);
                                if ((work_ + i)->file_descriptor > file_descriptor_maximum)
                                {
                                        file_descriptor_maximum = (work_ + i)->file_descriptor;
                                }
                        }
#ifdef SKK_MEMORY_DEBUG
                        skk_memory_debug_check_((work_ + i)->read_buffer - SKK_MEMORY_DEBUG_MARGIN_SIZE);
                        skk_memory_debug_check_((work_ + i)->read_buffer - SKK_MEMORY_DEBUG_MARGIN_SIZE + SKK_MEMORY_DEBUG_MARGIN_SIZE + READ_BUFFER_SIZE);
#endif  // SKK_MEMORY_DEBUG
                }
                struct timeval timeout;
                timeout.tv_sec = 3;
                timeout.tv_usec = 0;
                int n = select(file_descriptor_maximum + 1, &fd_set_read, 0, 0, &timeout);
                if ((n == -1) && (errno == EINTR))
                {
                        syslog_.printf(1, SkkSyslog::LEVEL_INFO, "caught signal");
                }
                return n;
        }
#pragma GCC diagnostic pop

/// mainLoop() で辞書の更新チェックをします。
        int main_loop_check_reload_dictionary(SkkDictionary *skk_dictionary,
                                              int skk_dictionary_length,
                                              const char * const *dictionary_filename_table,
                                              bool dictionary_check_update_flag)
        {
                int result = 0;
                if (dictionary_check_update_flag && dictionary_filename_table)
                {
                        int total_read_size = 0;
                        for (int i = 0; i != max_connection_; ++i)
                        {
                                if ((work_ + i)->flag)
                                {
                                        total_read_size += (work_ + i)->read_process_index;
                                }
                        }
                        if (total_read_size == 0)
                        {
                                for (int i = 0; i != skk_dictionary_length; ++i)
                                {
                                        if (*(dictionary_filename_table + i))
                                        {
                                                bool update_flag;
                                                if ((skk_dictionary + i)->isUpdateDictionary(update_flag, *(dictionary_filename_table + i)))
                                                {
                                                        if (update_flag)
                                                        {
                                                                ++result;
                                                                (skk_dictionary + i)->close();
                                                                (skk_dictionary + i)->open(*(dictionary_filename_table + i));
                                                                syslog_.printf(1,
                                                                               SkkSyslog::LEVEL_INFO,
                                                                               "dictionary \"%s\" (index = %d) updated",
                                                                               *(dictionary_filename_table + i),
                                                                               i);
                                                        }
                                                }
                                                else
                                                {
                                                        syslog_.printf(1,
                                                                       SkkSyslog::LEVEL_WARNING,
                                                                       "dictionary \"%s\" (index = %d) check failed",
                                                                       *(dictionary_filename_table + i),
                                                                       i);
                                                }
                                        }
                                }
                        }
                }
                return result;
        }

/// mainLoop() で accept() します。
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
        bool main_loop_accept(fd_set &fd_set_read, int select_result)
        {
                socklen_t length = sizeof(struct sockaddr_in);
                if (FD_ISSET(file_descriptor_, &fd_set_read))
                {
                        int counter = 0;
                        for (int i = 0; i != max_connection_; ++i)
                        {
                                if ((work_ + i)->flag == false)
                                {
                                        int fd = accept(file_descriptor_,
                                                        reinterpret_cast<struct sockaddr*>(&(work_ + i)->socket),
                                                        reinterpret_cast<socklen_t*>(&length));
                                        if (fd == -1)
                                        {
//                                         goto ERROR_BREAK;
//                                         return false;
                                        }
                                        else
                                        {
                                                syslog_.printf(2,
                                                               SkkSyslog::LEVEL_INFO,
                                                               "connected from %s",
                                                               inet_ntoa((work_ + i)->socket.sin_addr));
                                                (work_ + i)->flag = true;
                                                (work_ + i)->file_descriptor = fd;
                                                ++counter;
                                                if (counter >= select_result)
                                                {
                                                        break;
                                                }
                                        }
                                }
                        }
                        if (counter == 0)
                        {
                                struct sockaddr_in dummy_socket;
                                int dummy_fd = accept(file_descriptor_,
                                                      reinterpret_cast<struct sockaddr*>(&dummy_socket),
                                                      reinterpret_cast<socklen_t*>(&length));
                                if (dummy_fd == -1)
                                {
                                        return false;
                                }
                                close(dummy_fd);
                        }
                }
                return true;
        }

/// MainLoop() で recv() すべきかどうかを返します。
        bool main_loop_is_recv(int work_index, fd_set &fd_set_read)
        {
                if ((work_ + work_index)->flag && FD_ISSET((work_ + work_index)->file_descriptor, &fd_set_read))
                {
                        return true;
                }
                else
                {
                        return false;
                }
        }
#pragma GCC diagnostic pop

/// MainLoop() で recv() して read_buffer へ読み込みます。 read_buffer を参照してはならないならば真を返します。
        bool main_loop_recv(int work_index, int &recv_result, bool &error_break_flag)
        {
                bool result = true;
                recv_result = SkkSocket::receive((work_ + work_index)->file_descriptor,
                                                 (work_ + work_index)->read_buffer + (work_ + work_index)->read_process_index,
                                                 MIDASI_SIZE + MIDASI_TERMINATOR_SIZE - (work_ + work_index)->read_process_index);
                error_break_flag = false;
                if (recv_result == -1)
                {
                        switch (errno)
                        {
                        default:
                                error_break_flag = true;
                                goto ERROR_BREAK;
                        case EAGAIN:
                        case EINTR:
                        case ECONNABORTED:
                        case ECONNREFUSED:
                        case ECONNRESET:
                                break;
                        }
                        (work_ + work_index)->reset();
                }
                else if (recv_result == 0)
                {
                        close((work_ + work_index)->file_descriptor);
                        (work_ + work_index)->reset();
                        (work_ + work_index)->file_descriptor = 0;
                        (work_ + work_index)->flag = false;
                }
                else if (recv_result > MIDASI_SIZE + MIDASI_TERMINATOR_SIZE)
                {
// recv() で指定した読み込みサイズは、ここで比較する値より小さいので、
// ここに来ることはありません。
                        DEBUG_ASSERT(0);
                        (work_ + work_index)->reset();
                }
                else
                {
                        result = false;
                }

        ERROR_BREAK:
                return result;
        }

/// mainLoop() で "0" 相当の処理をします。
        void main_loop_0(int work_index)
        {
                close((work_ + work_index)->file_descriptor);
                (work_ + work_index)->file_descriptor = 0;
                (work_ + work_index)->flag = false;
        }

/// mainLoop() で "2" 相当の処理をします。
        void main_loop_2(int work_index, const char *version_string, int version_string_size)
        {
                if (!send((work_ + work_index)->file_descriptor, version_string, version_string_size - 1))
                {
                        (work_ + work_index)->closeAndReset();
                }
        }

/// mainLoop() で "3" 相当の処理をします。
        void main_loop_3(int work_index)
        {
                const char hostname[] = "hostname:addr:...: ";
                if (!send((work_ + work_index)->file_descriptor, hostname, sizeof(hostname) - 1))
                {
                        (work_ + work_index)->closeAndReset();
                }
        }

/// mainLoop() で不正なコマンドの処理をします。
        void main_loop_illegal_command(int work_index)
        {
                const char result[] = "0\n";
                if (!send((work_ + work_index)->file_descriptor, result, sizeof(result) - 1))
                {
                        (work_ + work_index)->closeAndReset();
                }
        }

/// バッファをリセットすべきならば真を返します。
/**
 * 偽を返すのは 1 度で recv() しきれなかった場合だけです。
 *
 * その他不正なプロトコルなどでもバッファはリセットすべきなので、真を返
 * します。
 *
 * recv 文字列に '\n' を含むにもかかわらず ' ' が存在しない場合は不正な
 * プロトコルとして illegal_protocol_flag を真にします。このとき呼び出
 * し側では "4" を返すべきです。
 */
        bool main_loop_check_buffer(int work_index, int recv_result, bool &illegal_protocol_flag)
        {
                DEBUG_ASSERT(recv_result > 0);
                DEBUG_ASSERT(recv_result <= MIDASI_SIZE + MIDASI_TERMINATOR_SIZE);
                bool result = true;
                illegal_protocol_flag = false;
                bool cr_found_flag = false;
                bool space_found_flag = false;
                int body_bytes = 0;
                if (recv_result >= 2)
                {
                        for (int h = 1; h <= recv_result - 1; ++h)
                        {
                                int c = static_cast<int>(static_cast<unsigned char>(*((work_ + work_index)->read_buffer + h)));
                                if ((c == '\n') || (c == ' ') || (c == '\0'))
                                {
                                        if (body_bytes == 0)
                                        {
                                                illegal_protocol_flag = true;
                                        }
                                        break;
                                }
                                else
                                {
                                        ++body_bytes;
                                }
                        }
                }
                if (!illegal_protocol_flag)
                {
                        for (int h = recv_result - 1; h >= 0; --h)
                        {
                                int tmp = (work_ + work_index)->read_process_index + h;
                                int c = static_cast<int>(static_cast<unsigned char>(*((work_ + work_index)->read_buffer + tmp)));
                                if (c == '\n')
                                {
                                        cr_found_flag = true;
                                }
                                if (c == ' ')
                                {
                                        space_found_flag = true;
                                        break;
                                }
                        }
                        if (!space_found_flag)
                        {
                                if (cr_found_flag)
                                {
// '\n' を含むにもかかわらず ' ' が存在しないので不正なプロトコルです。
                                        illegal_protocol_flag = true;
                                }
                                else
                                {
// 終端に '\n' も ' ' も存在しないということは recv() で読み切れていな
// いため、バッファをリセットしません。
                                        result = false;
                                }
                        }
                }
                return result;
        }

/// MainLoop() で必要ならばバッファリセットをおこないます。
        void main_loop_check_buffer_reset(int work_index, int recv_result, bool buffer_reset_flag)
        {
                if (buffer_reset_flag)
                {
                        (work_ + work_index)->reset();
                }
                else
                {
                        (work_ + work_index)->read_process_index += recv_result;
                        if ((work_ + work_index)->read_process_index > MIDASI_SIZE + MIDASI_TERMINATOR_SIZE)
                        {
// 不正なリクエストです。
                                (work_ + work_index)->reset();
                        }
                }
        }

/// MainLoop() のファイナライズ処理です。
        bool main_loop_finalize()
        {
                for (int i = 0; i != max_connection_; ++i)
                {
                        if ((work_ + i)->flag)
                        {
                                close((work_ + i)->file_descriptor);
                                (work_ + i)->file_descriptor = 0;
                                (work_ + i)->flag = false;
                        }
                }
                close(file_descriptor_);
                return true;
        }

protected:
        enum
        {
// でエンコード時は先頭に \\1 が付くため 1 バイト大きくなる可能性があり
// ます。
                MIDASI_SIZE = 1 + 510,
// 見出しのターミネータとなるスペース (0x20) のサイズです。
                MIDASI_TERMINATOR_SIZE = 1,
// main_loop_send_not_found() などでマージン領域を利用するため、絶対に
// 必要です。
                MIDASI_MARGIN_SIZE = 4,
// struct Work の read_buffer のサイズです。
                READ_BUFFER_SIZE = MIDASI_SIZE + MIDASI_TERMINATOR_SIZE + MIDASI_MARGIN_SIZE
        };

        struct Work
        {
        private:
                Work(Work &source);
                Work& operator=(Work &source);

        public:
                Work() :
                        read_buffer(0),
                        file_descriptor(0),
                        read_process_index(0),
                        socket(),
                        flag(false)
                {
                        SkkUtility::clearMemory(&socket, sizeof(socket));
                }

                void reset()
                {
                        read_process_index = 0;
                        *(read_buffer + 0) = '\0';
                }

                void closeAndReset()
                {
                        close(file_descriptor);
                        reset();
                        file_descriptor = 0;
                        flag = false;
                }

                char *read_buffer;
                int file_descriptor;
                int read_process_index;
                struct sockaddr_in socket;
                bool flag;
        };

        SkkSyslog syslog_;
        Work *work_;
        int port_;
        const char *address_;
        int max_connection_;
        int listen_queue_;
        int file_descriptor_;
};
}

#endif  // SKK_SERVER_H
