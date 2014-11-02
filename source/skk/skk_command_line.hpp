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
#ifndef SKK_COMMAND_LINE_HPP
#define SKK_COMMAND_LINE_HPP

#include "skk_architecture.hpp"

namespace YaSkkServ
{
//
/**
 *
 * 言葉の定義
 *
 * "option argument" はオプションの引数
 *
 * "argument" はオプション以外の引数(主にファイル名)
 *
 *
 * 以下のような形式に対応しています。
 *
 *    ./a.out [options] [argument]
 *
 *    ./a.out -a optarg --alpha=optarg argument
 *
 *    ./a.out -- -a b --alpha=beta gamma (-- 以降は全て argument)
 */
class SkkCommandLine
{
        SkkCommandLine(SkkCommandLine &source);
        SkkCommandLine& operator=(SkkCommandLine &source);

public:
        enum OptionArgument
        {
                OPTION_ARGUMENT_TERMINATOR,
                OPTION_ARGUMENT_NONE,
                OPTION_ARGUMENT_INTEGER,
                OPTION_ARGUMENT_FLOAT,
                OPTION_ARGUMENT_STRING
        };

        struct Option
        {
                const char *option_short;
                const char *option_long;
                OptionArgument option_argument;
        };

        virtual ~SkkCommandLine()
        {
                delete[] option_result_;
        }

        SkkCommandLine() :
                argv_(0),
                option_table_(0),
                option_result_(0),
                error_string_("no error."),
                option_table_length_(0),
                argc_(0),
                argument_index_(0),
                argument_length_(0),
                parse_flag_(false)
        {
        }

        bool parse(int argc, const char * const argv[], const Option *option_table)
        {
                argc_ = argc;
                argv_ = argv;
                option_table_ = option_table;

                for (int i = 0;; ++i)
                {
                        if ((option_table + i)->option_argument == OPTION_ARGUMENT_TERMINATOR)
                        {
                                option_table_length_ = i;
                                break;
                        }
                }

                if (option_table_length_ > 0)
                {
                        if (option_result_)
                        {
                                delete[] option_result_;
                        }
                        option_result_ = new OptionResult[option_table_length_];

                        int option_check_length = argc_;

// check "--" (force argument)
                        for (int i = 1; i != argc_; ++i)
                        {
                                const char *p = *(argv_ + i);
                                if ((*(p + 0) == '-') &&
                                    (*(p + 1) == '-') &&
                                    (*(p + 2) == '\0'))
                                {
                                        argument_index_ = i + 1;
                                        argument_length_ = argc_ - argument_index_;
                                        DEBUG_PRINTF("argument_index_=%d  argument_length_=%d\n",
                                                     argument_index_,
                                                     argument_length_);
                                        option_check_length = i;
                                        break;
                                }
                        }

                        if (!parse_loop(option_check_length))
                        {
                                return false;
                        }
                        else
                        {
                                if (argument_index_ != 0)
                                {
                                        for (int i = 0; i != argument_length_; ++i)
                                        {
                                                DEBUG_PRINTF("argument?  %d/%d  %s\n",
                                                             i,
                                                             argument_length_,
                                                             *(argv_ + argument_index_ + i));
                                        }
                                }
                                return true;
                        }
                }
                else
                {
                        error_string_ = "internal error.";
                        return false;
                }
        }

        int getArgumentArgvIndex() const
        {
                return argument_index_;
        }

        int getArgumentLength() const
        {
                return argument_length_;
        }

        bool isArgumentDefined(int n) const
        {
                if (n >= argument_length_)
                {
                        DEBUG_ASSERT(0);
                        return false;
                }
                else
                {
                        return true;
                }
        }

        const char *getArgumentPointer(int n) const
        {
                if (n >= argument_length_)
                {
                        DEBUG_ASSERT(0);
                        return 0;
                }
                else
                {
                        return *(argv_ + argument_index_ + n);
                }
        }

        bool isOptionDefined(int n) const
        {
                if (n >= option_table_length_)
                {
                        DEBUG_ASSERT(0);
                        return false;
                }
                else
                {
                        return (option_result_ + n)->enable_flag;
                }
        }

        const char *getOptionArgumentPointer(int n) const
        {
                if (n >= option_table_length_)
                {
                        DEBUG_ASSERT(0);
                        return 0;
                }
                else
                {
                        return (option_result_ + n)->argument;
                }
        }

        const char *getOptionArgumentString(int n) const
        {
                const char *p = getOptionArgumentPointer(n);
                if (p)
                {
                        if ((option_table_ + n)->option_argument != OPTION_ARGUMENT_STRING)
                        {
                                DEBUG_ASSERT(0);
                                return 0;
                        }
                }
                return p;
        }

        int getOptionArgumentInteger(int n) const
        {
                const char *p = getOptionArgumentPointer(n);
                if (p)
                {
                        if ((option_table_ + n)->option_argument != OPTION_ARGUMENT_INTEGER)
                        {
                                DEBUG_ASSERT(0);
                                return 0;
                        }
                }

                int result;
                if (!SkkUtility::getInteger(p, result))
                {
                        DEBUG_ASSERT(0);
                }
                return result;
        }

        float getOptionArgumentFloat(int n) const
        {
                const char *p = getOptionArgumentPointer(n);
                if (p)
                {
                        if ((option_table_ + n)->option_argument != OPTION_ARGUMENT_FLOAT)
                        {
                                DEBUG_ASSERT(0);
                                return 0;
                        }
                }

                float result;
                if (!SkkUtility::getFloat(p, result))
                {
                        DEBUG_ASSERT(0);
                }
                return result;
        }

        const char *getErrorString() const
        {
                return error_string_;
        }

private:
        static bool is_option(const char *p)
        {
                if (*(p + 0) == '-')
                {
                        return true;
                }
                else
                {
                        return false;
                }
        }

        static bool is_long_option(const char *p)
        {
                if ((*(p + 0) == '-') &&
                    (*(p + 1) == '-'))
                {
                        return true;
                }
                else
                {
                        return false;
                }
        }

        static const char *get_equal_pointer(const char *p)
        {
                for (;;)
                {
                        char c = *p;
                        if (c == '\0')
                        {
                                return 0;
                        }
                        if (c == '=')
                        {
                                return p;
                        }
                        ++p;
                }
        }

        static bool is_integer(const char *p)
        {
                int dummy_result;

                return SkkUtility::getInteger(p, dummy_result);
        }

        static bool is_float(const char *p)
        {
                float dummy_result;

                return SkkUtility::getFloat(p, dummy_result);
        }

/// '=' or '\0' で終端する文字列を比較し、同一ならば真を返します。
        static bool compare_option_string(const char *a, const char *b)
        {
                for (int i = 0; ;++i)
                {
                        if ((*(a + i) == '\0') || (*(a + i) == '='))
                        {
                                if ((*(b + i) == '\0') || (*(b + i) == '='))
                                {
                                        return true;
                                }
                                else
                                {
                                        return false;
                                }
                        }
                        if (*(a + i) != *(b + i))
                        {
                                return false;
                        }
                }
        }

/// option が option_table_ に含まれるか調べ、含まれていれば真を返しその情報を引数に設定します。
        bool search_option_table(const char *option, int &result_option_table_index, bool &result_long_flag) const
        {
                if (is_option(option))
                {
                        bool long_flag = is_long_option(option);
                        const char *p;
                        if (long_flag)
                        {
                                p = option + 2; // skip "--"
                        }
                        else
                        {
                                p = option + 1; // skip "-"
                        }
                        if (long_flag)
                        {
                                for (int i = 0; (option_table_ + i)->option_argument != OPTION_ARGUMENT_TERMINATOR; ++i)
                                {
                                        if ((option_table_ + i)->option_long &&
                                            compare_option_string(p, (option_table_ + i)->option_long))
                                        {
                                                result_option_table_index = i;
                                                result_long_flag = long_flag;
                                                return true;
                                        }
                                }
                        }
                        else
                        {
                                for (int i = 0; (option_table_ + i)->option_argument != OPTION_ARGUMENT_TERMINATOR; ++i)
                                {
                                        if ((option_table_ + i)->option_short &&
                                            compare_option_string(p, (option_table_ + i)->option_short))
                                        {
                                                result_option_table_index = i;
                                                result_long_flag = long_flag;
                                                return true;
                                        }
                                }
                        }
                }
                return false;
        }

        bool parse_loop(int option_check_length)
        {
                enum State
                {
                        STATE_OPTION,
                        STATE_OPTION_ARGUMENT
                };
                State state = STATE_OPTION;
                int option_table_index = -1;

                error_string_ = "no error.";

                for (int i = 1; i != option_check_length; ++i)
                {
                        DEBUG_PRINTF("option?  %d/%d  %s\n",
                                     i,
                                     option_check_length,
                                     *(argv_ + i));
                        switch (state)
                        {
                        default:
                                DEBUG_ASSERT(0);
                                error_string_ = "internal error.";
                                return false;
                        case STATE_OPTION:
                                DEBUG_PRINTF("    state_option\n");
                                {
                                        bool long_flag;
                                        if (search_option_table(*(argv_ + i), option_table_index, long_flag))
                                        {
                                                DEBUG_PRINTF("    search_option_table() found.\n");

                                                switch ((option_table_ + option_table_index)->option_argument)
                                                {
                                                case OPTION_ARGUMENT_TERMINATOR: // FALLTHROUGH
                                                default:
                                                        DEBUG_ASSERT(0);
                                                        error_string_ = "internal error.";
                                                        return false;
                                                case OPTION_ARGUMENT_NONE:
                                                        {
                                                                const char *p = get_equal_pointer(*(argv_ + i));
                                                                if (p)
                                                                {
                                                                        error_string_ = p;
                                                                        return false;
                                                                }
                                                        }
                                                        (option_result_ + option_table_index)->enable_flag = true;
                                                        break;
                                                case OPTION_ARGUMENT_INTEGER:
                                                case OPTION_ARGUMENT_FLOAT:
                                                case OPTION_ARGUMENT_STRING:
                                                        {
                                                                const char *p = get_equal_pointer(*(argv_ + i));
                                                                if (p)
                                                                {
// --foo=ARGUMENT
//      ||
// p __/  \__ p + 1
                                                                        if (*(p + 1) == '\0')
                                                                        {
                                                                                error_string_ = p;
                                                                                return false;
                                                                        }
                                                                        if ((option_table_ + option_table_index)->option_argument == OPTION_ARGUMENT_INTEGER)
                                                                        {
                                                                                if (!is_integer(p + 1))
                                                                                {
                                                                                        error_string_ = p;
                                                                                        return false;
                                                                                }
                                                                        }
                                                                        if ((option_table_ + option_table_index)->option_argument == OPTION_ARGUMENT_FLOAT)
                                                                        {
                                                                                if (!is_float(p + 1))
                                                                                {
                                                                                        error_string_ = p;
                                                                                        return false;
                                                                                }
                                                                        }
                                                                        (option_result_ + option_table_index)->argument = p + 1;
                                                                        (option_result_ + option_table_index)->long_flag = long_flag;
                                                                        (option_result_ + option_table_index)->enable_flag = true;
                                                                }
                                                                else
                                                                {
                                                                        (option_result_ + option_table_index)->long_flag = long_flag;
                                                                        state = STATE_OPTION_ARGUMENT;
                                                                }
                                                        }
                                                        break;
                                                }
                                        }
                                        else
                                        {
                                                DEBUG_PRINTF("    search_option_table() NOT FOUND.\n");
                                                if (is_option(*(argv_ + i)))
                                                {
                                                        error_string_ = *(argv_ + i);

                                                        return false;
                                                }
                                                else
                                                {
                                                        argument_index_ = i;
                                                        argument_length_ = argc_ - argument_index_;

                                                        return true;
                                                }
                                        }
                                }
                                break;
                        case STATE_OPTION_ARGUMENT:
                                DEBUG_PRINTF("    state_option_argument\n");
                                if (option_table_index < 0)
                                {
                                        error_string_ = "internal error.";
                                        return false;
                                }
                                if ((option_table_ + option_table_index)->option_argument == OPTION_ARGUMENT_INTEGER)
                                {
                                        if (!is_integer(*(argv_ + i)))
                                        {
                                                error_string_ = *(argv_ + i);
                                                return false;
                                        }
                                }
                                if ((option_table_ + option_table_index)->option_argument == OPTION_ARGUMENT_FLOAT)
                                {
                                        if (!is_float(*(argv_ + i)))
                                        {
                                                error_string_ = *(argv_ + i);
                                                return false;
                                        }
                                }
                                (option_result_ + option_table_index)->argument = *(argv_ + i);
                                (option_result_ + option_table_index)->enable_flag = true;
                                state = STATE_OPTION;
                                break;
                        }
                }
                return true;
        }

private:
        struct OptionResult
        {
                OptionResult(OptionResult &source);
                OptionResult& operator=(OptionResult &source);

                OptionResult () :
                        argument(0),
                        enable_flag(false),
                        long_flag(false)
                {
                }

                const char *argument;
                bool enable_flag;
                bool long_flag;
        };

        const char * const *argv_;
        const Option *option_table_;
        OptionResult *option_result_;
        const char *error_string_;
        int option_table_length_;
        int argc_;
        int argument_index_;
        int argument_length_;
        bool parse_flag_;
};
}

#endif // SKK_COMMAND_LINE_HPP
