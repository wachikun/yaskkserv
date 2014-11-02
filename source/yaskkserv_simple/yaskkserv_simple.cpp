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

#include "skk_dictionary.hpp"
#include "skk_server.hpp"
#include "skk_utility.hpp"
#include "skk_command_line.hpp"

namespace YaSkkServ
{
namespace
{

#define SERVER_IDENTIFIER "simple"
char version_string[] = YASKKSERV_VERSION ":yaskkserv_" SERVER_IDENTIFIER " ";

class LocalSkkServer :
        private SkkServer
{
        LocalSkkServer(LocalSkkServer &source);
        LocalSkkServer& operator=(LocalSkkServer &source);

public:
        virtual ~LocalSkkServer()
        {
        }

        LocalSkkServer(int port = 1178, int log_level = 0) :
                SkkServer("yaskkserv_" SERVER_IDENTIFIER, port, log_level),

                skk_dictionary_(0),
                max_connection_(0),
                listen_queue_(0)
        {
        }

        void initialize(SkkDictionary *skk_dictionary, int max_connection, int listen_queue)
        {
                skk_dictionary_ = skk_dictionary;
                max_connection_ = max_connection;
                listen_queue_ = listen_queue;
        }

        bool mainLoop()
        {
                bool result = main_loop_initialize(max_connection_, listen_queue_);
                if (result)
                {
#ifndef YASKKSERV_DEBUG
                        if (fork() != 0)
                        {
                                exit(0);
                        }
                        if (chdir("/") != 0)
                        {
                                // why?
                        }
                        close(2);
                        close(1);
                        close(0);

                        printFirstSyslog();
#endif  // YASKKSERV_DEBUG
                        result = local_main_loop();
                        if (result)
                        {
                                result = main_loop_finalize();
                        }
                }
                return result;
        }

private:
        bool local_main_loop();
        bool local_main_loop_1_search(int work_index);
/// バッファをリセットすべきならば真を返します。
        bool local_main_loop_1(int work_index, int recv_result);

private:
        SkkDictionary *skk_dictionary_;

        int max_connection_;
        int listen_queue_;
};

bool LocalSkkServer::local_main_loop_1_search(int work_index)
{
        if (skk_dictionary_->search((work_ + work_index)->read_buffer + 1))
        {
                main_loop_send_found(work_index, skk_dictionary_);

                return true;
        }
        else
        {
                return false;
        }
}

bool LocalSkkServer::local_main_loop_1(int work_index, int recv_result)
{
        bool illegal_protocol_flag;
        bool result = main_loop_check_buffer(work_index, recv_result, illegal_protocol_flag);
        if (result)
        {
                bool found_flag = false;
                if (!illegal_protocol_flag)
                {
                        found_flag = local_main_loop_1_search(work_index);
                }
                if (!found_flag)
                {
                        main_loop_send_not_found(work_index, recv_result);
                }
        }
        return result;
}

bool LocalSkkServer::local_main_loop()
{
        bool result = true;
        for (;;)
        {
                fd_set fd_set_read;
                int select_result = main_loop_select(fd_set_read);
                if (select_result == -1)
                {
                        if (errno == EINTR)
                        {
                                continue;
                        }
                }
                if (!main_loop_accept(fd_set_read, select_result))
                {
                        goto ERROR_BREAK;
                }
                for (int i = 0; i != max_connection_; ++i)
                {
                        if (main_loop_is_recv(i, fd_set_read))
                        {
                                int recv_result;
                                bool error_break_flag;

                                if (main_loop_recv(i, recv_result, error_break_flag))
                                {
                                        if (error_break_flag)
                                        {
                                                goto ERROR_BREAK;
                                        }
                                }
                                else
                                {
                                        bool buffer_reset_flag;
                                        if ((work_ + i)->read_process_index == 0)
                                        {
                                                switch (*(work_ + i)->read_buffer)
                                                {
                                                default:
                                                        buffer_reset_flag = true;
                                                        main_loop_illegal_command(i);
                                                        break;
                                                case '0':
                                                        buffer_reset_flag = true;
                                                        main_loop_0(i);
                                                        break;
                                                case '1':
                                                        buffer_reset_flag = local_main_loop_1(i, recv_result);
                                                        break;
                                                case '2':
                                                        buffer_reset_flag = true;
                                                        main_loop_2(i, version_string, sizeof(version_string));
                                                        break;
                                                case '3':
                                                        buffer_reset_flag = true;
                                                        main_loop_3(i);
                                                        break;
                                                }
                                        }
                                        else
                                        {
                                                buffer_reset_flag = local_main_loop_1(i, recv_result);
                                        }

                                        main_loop_check_buffer_reset(i, recv_result, buffer_reset_flag);
                                }
                        }
                }
        }
ERROR_BREAK:
        result = false;

//SUCCESS_BREAK:
        return result;
}

int print_usage()
{
        SkkUtility::printf("Usage: yaskkserv [OPTION] dictionary\n"
                           "  -d, --debug              enable debug mode (default disable)\n"
                           "  -h, --help               print this help and exit\n"
                           "  -l, --log-level=LEVEL    loglevel (range [0 - 9]  default 1)\n"
                           "  -m, --max-connection=N   max connection (default 8)\n"
                           "  -p, --port=PORT          set port (default 1178)\n"
                           "  -v, --version            print version\n");
        return EXIT_FAILURE;
}

int print_version()
{
        SkkUtility::printf("yaskkserv_" SERVER_IDENTIFIER " version " YASKKSERV_VERSION "\n");
        SkkUtility::printf("Copyright (C) 2005, 2006, 2007, 2008, 2011, 2012, 2013, 2014 Tadashi Watanabe\n");
        SkkUtility::printf("http://umiushi.org/~wac/yaskkserv/\n");
        return EXIT_FAILURE;
}

enum
{
        OPTION_TABLE_DEBUG,
        OPTION_TABLE_HELP,
        OPTION_TABLE_LOG_LEVEL,
        OPTION_TABLE_MAX_CONNECTION,
        OPTION_TABLE_PORT,
        OPTION_TABLE_VERSION,

        OPTION_TABLE_LENGTH
};

const SkkCommandLine::Option option_table[] =
{
        {
                "d", "debug",
                SkkCommandLine::OPTION_ARGUMENT_NONE,
        },
        {
                "h", "help",
                SkkCommandLine::OPTION_ARGUMENT_NONE,
        },
        {
                "l", "log-level",
                SkkCommandLine::OPTION_ARGUMENT_INTEGER,
        },
        {
                "m", "max-connection",
                SkkCommandLine::OPTION_ARGUMENT_INTEGER,
        },
        {
                "p", "port",
                SkkCommandLine::OPTION_ARGUMENT_INTEGER,
        },
        {
                "v", "version",
                SkkCommandLine::OPTION_ARGUMENT_NONE,
        },
        {
                0, 0,
                SkkCommandLine::OPTION_ARGUMENT_TERMINATOR,
        },
};

struct Option
{
        int log_level;
        int max_connection;
        int port;
        bool debug_flag;
}
option =
{
        1,
        8,
        1178,
        false,
};

// 戻り値が真ならば呼び出し側で return すべきです。このとき result に戻り値を返します。戻り値が偽の場合 result には触れません。
bool local_main_core_command_line(SkkCommandLine &command_line, int &result, int argc, char *argv[])
{
        if (command_line.parse(argc, argv, option_table))
        {
                if (command_line.isOptionDefined(OPTION_TABLE_HELP))
                {
                        result = print_usage();
                        return true;
                }
                if (command_line.isOptionDefined(OPTION_TABLE_VERSION))
                {
                        result = print_version();
                        return true;
                }
// 辞書指定が無い場合と、念のため非常識な値 (64) より多い指定をはじいておく。
                if ((command_line.getArgumentLength() < 1) || (command_line.getArgumentLength() > 64))
                {
                        result = print_usage();
                        return true;
                }
                if (command_line.isOptionDefined(OPTION_TABLE_DEBUG))
                {
                        option.debug_flag = true;
                }
                if (command_line.isOptionDefined(OPTION_TABLE_LOG_LEVEL))
                {
                        option.log_level = command_line.getOptionArgumentInteger(OPTION_TABLE_LOG_LEVEL);
                        if ((option.log_level < 0) || (option.log_level > 9))
                        {
                                SkkUtility::printf("Illegal log-level %d (0 - 9)\n", option.log_level);
                                result = print_usage();
                                return true;
                        }
                }
                if (command_line.isOptionDefined(OPTION_TABLE_MAX_CONNECTION))
                {
                        option.max_connection = command_line.getOptionArgumentInteger(OPTION_TABLE_MAX_CONNECTION);
                        if ((option.max_connection < 1) || (option.max_connection > 1024))
                        {
                                SkkUtility::printf("Illegal max-connection %d (1 - 1024)\n", option.max_connection);
                                result = print_usage();
                                return true;
                        }
                }
                if (command_line.isOptionDefined(OPTION_TABLE_PORT))
                {
                        option.port = command_line.getOptionArgumentInteger(OPTION_TABLE_PORT);
                        if ((option.port < 1) || (option.port > 65535))
                        {
                                SkkUtility::printf("Illegal port number %d (1 - 65535)\n", option.port);
                                result = print_usage();
                                return true;
                        }
                }
        }
        else
        {
                SkkUtility::printf("error \"%s\"\n\n", command_line.getErrorString());
                result = print_usage();
                return true;
        }
        return false;
}

// 呼び出し元で返すべき戻り値 EXIT_SUCCESS または EXIT_FAILURE を返します。
int local_main_core_setup_dictionary(const SkkCommandLine &command_line, SkkDictionary *skk_dictionary)
{
        int result = EXIT_SUCCESS;
        int skk_dictionary_length = command_line.getArgumentLength();
        for (int i = 0; i != skk_dictionary_length; ++i)
        {
                SkkUtility::DictionaryPermission dictionary_permission(command_line.getArgumentPointer(i));
                if (!dictionary_permission.isExist())
                {
                        SkkUtility::printf("dictionary file \"%s\" (index = %d) not found\n",
                                           command_line.getArgumentPointer(i),
                                           i);
                        result = EXIT_FAILURE;
                        break;
                }
                if (!dictionary_permission.checkAndPrintPermission())
                {
                        result = EXIT_FAILURE;
                        break;
                }

                if (!(skk_dictionary + i)->open(command_line.getArgumentPointer(i)))
                {
                        SkkUtility::printf("dictionary file \"%s\" (index = %d) open failed\n",
                                           command_line.getArgumentPointer(i),
                                           i);
                        result = EXIT_FAILURE;
                        break;
                }
        }
        return result;
}

int local_main_core(int argc, char *argv[])
{
        int result = EXIT_SUCCESS;
        SkkCommandLine command_line;
        if (local_main_core_command_line(command_line, result, argc, argv))
        {
                return result;
        }

        int skk_dictionary_length = command_line.getArgumentLength();
        if (skk_dictionary_length != 1)
        {
                return print_usage();
        }

        SkkDictionary *skk_dictionary = new SkkDictionary[skk_dictionary_length];
        result = local_main_core_setup_dictionary(command_line, skk_dictionary);

        if (result == EXIT_SUCCESS)
        {
                LocalSkkServer *skk_server = new LocalSkkServer(option.port, option.log_level);
                const int listen_queue = 5;
                skk_server->initialize(skk_dictionary, option.max_connection, listen_queue);
                if (!skk_server->mainLoop())
                {
                        result = EXIT_FAILURE;
                }
                delete skk_server;
        }

        delete[] skk_dictionary;

        return result;
}
}

int local_main(int argc, char *argv[])
{
        return local_main_core(argc, argv);
}
}
