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

#include "skk_architecture.hpp"
#include "skk_mmap.hpp"
#include "skk_jisyo.hpp"
#include "skk_dictionary.hpp"
#include "skk_server.hpp"
#include "skk_utility.hpp"
#include "skk_command_line.hpp"

namespace YaSkkServ
{
namespace
{
void print_debug_information(const char *destination)
{
        SkkUtility::printf("DICTIONARY INFORMATION\n");

        SkkJisyo::Information information;
        if (!SkkJisyo::getInformation(destination, information))
        {
                DEBUG_PRINTF("getInformation() failed \n");
        }
        else
        {
                for (int i = 0; i != SkkJisyo::Information::ID_LENGTH; ++i)
                {
                        const char * const table[] =
                        {
                                "ID_BIT_FLAG",

                                "ID_RESERVE_1",
                                "ID_RESERVE_2",
                                "ID_RESERVE_3",
                                "ID_RESERVE_4",
                                "ID_RESERVE_5",

                                "ID_INDEX_DATA_OFFSET",
                                "ID_INDEX_DATA_SIZE",
                                "ID_SPECIAL_LINES",
                                "ID_SPECIAL_SIZE",
                                "ID_NORMAL_LINES",
                                "ID_NORMAL_SIZE",
                                "ID_BLOCK_ALIGNMENT_SIZE",
                                "ID_VERSION",
                                "ID_SIZE",
                                "ID_IDENTIFIER",
                        };
                        SkkUtility::printf("id = %s  v = %d\n",
                                           table[i],
                                           information.get(static_cast<SkkJisyo::Information::Id>(i)));
                }
        }

        SkkUtility::printf("INDEX DATA HEADER\n");

        SkkJisyo::IndexDataHeader index_data_header;
        if (!SkkJisyo::getIndexDataHeader(destination, index_data_header))
        {
                DEBUG_PRINTF("getIndexDataHeader() failed \n");
        }
        else
        {
                for (int i = 0; i != SkkJisyo::IndexDataHeader::ID_LENGTH; ++i)
                {
                        const char * const table[] =
                        {
                                "ID_BIT_FLAG",

                                "ID_SIZE",
                                "ID_BLOCK_SIZE",
                                "ID_NORMAL_BLOCK_LENGTH",
                                "ID_SPECIAL_BLOCK_LENGTH",
                                "ID_NORMAL_STRING_SIZE",
                                "ID_SPECIAL_STRING_SIZE",
                                "ID_SPECIAL_ENTRY_OFFSET",
                        };
                        SkkUtility::printf("id = %s  v = %d\n",
                                           table[i],
                                           index_data_header.get(static_cast<SkkJisyo::IndexDataHeader::Id>(i)));
                }
        }
}

int print_usage()
{
        SkkUtility::printf("Usage: yaskkserv_make_dictionary [OPTION] skk-dictionary output-dictionary\n"
                           "  -a, --alignment          enable alignment (default disable)\n"
                           "  -b, --block-size=SIZE    set block size (default 8192)\n"
                           "  -d, --debug              print debug information\n"
                           "  -h, --help               print this help and exit\n"
                           "  -s, --short-block        enable short block (must set --alignment) (default disable)\n"
                           "  -v, --version            print version\n");
        return -1;
}

int print_version()
{
        SkkUtility::printf("yaskkserv_make_dictionary version " YASKKSERV_VERSION "\n");
        SkkUtility::printf("Copyright (C) 2005, 2006, 2007, 2008, 2011, 2012 Tadashi Watanabe\n");
        SkkUtility::printf("http://umiushi.org/~wac/yaskkserv/\n");
        return -1;
}
}

int local_main(int argc, char *argv[])
{
        enum
        {
                OPTION_TABLE_ALIGNMENT,
                OPTION_TABLE_BLOCK_SIZE,
                OPTION_TABLE_DEBUG,
                OPTION_TABLE_HELP,
                OPTION_TABLE_SHORT_BLOCK,
                OPTION_TABLE_VERSION,

                OPTION_TABLE_LENGTH
        };

        const SkkCommandLine::Option option_table[] =
        {
                {
                        "a", "alignment",
                        SkkCommandLine::OPTION_ARGUMENT_NONE,
                },
                {
                        "b", "block-size",
                        SkkCommandLine::OPTION_ARGUMENT_INTEGER,
                },
                {
                        "d", "debug",
                        SkkCommandLine::OPTION_ARGUMENT_NONE,
                },
                {
                        "h", "help",
                        SkkCommandLine::OPTION_ARGUMENT_NONE,
                },
                {
                        "s", "short-block",
                        SkkCommandLine::OPTION_ARGUMENT_NONE,
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
                int block_size;
                bool alignment_flag;
                bool block_short_flag;
                bool debug_flag;
        }
        option =
        {
                8 * 1024,
                false,
                false,
                false,
        };
        const char *filename_input_skk_jisyo = 0;
        const char *filename_output_dictionary = 0;
        {
                SkkCommandLine command_line;
                if (command_line.parse(argc, argv, option_table))
                {
                        if (command_line.isOptionDefined(OPTION_TABLE_HELP))
                        {
                                return print_usage();
                        }
                        if (command_line.isOptionDefined(OPTION_TABLE_VERSION))
                        {
                                return print_version();
                        }
                        if (command_line.getArgumentLength() != 2)
                        {
                                return print_usage();
                        }

                        filename_input_skk_jisyo = command_line.getArgumentPointer(0);
                        filename_output_dictionary = command_line.getArgumentPointer(1);

                        if (command_line.isOptionDefined(OPTION_TABLE_ALIGNMENT))
                        {
                                option.alignment_flag = true;
                        }
                        if (command_line.isOptionDefined(OPTION_TABLE_BLOCK_SIZE))
                        {
                                option.block_size = command_line.getOptionArgumentInteger(OPTION_TABLE_BLOCK_SIZE);
                        }
                        if (command_line.isOptionDefined(OPTION_TABLE_SHORT_BLOCK))
                        {
                                if (!option.alignment_flag)
                                {
                                        return print_usage();
                                }
                                option.block_short_flag = true;
                        }
                        if (command_line.isOptionDefined(OPTION_TABLE_DEBUG))
                        {
                                option.debug_flag = true;
                        }
                }
                else
                {
                        SkkUtility::printf("error \"%s\"\n\n",
                                           command_line.getErrorString());
                        return print_usage();
                }
        }

        if (!SkkJisyo::createDictionaryForClassSkkDictionary(filename_input_skk_jisyo,
                                                             filename_output_dictionary,
                                                             option.block_size,
                                                             option.alignment_flag,
                                                             option.block_short_flag))
        {
                SkkUtility::printf("createDictionary() failed\n");
        }

        if (option.debug_flag)
        {
                print_debug_information(filename_output_dictionary);
        }

        return EXIT_SUCCESS;
}
}
