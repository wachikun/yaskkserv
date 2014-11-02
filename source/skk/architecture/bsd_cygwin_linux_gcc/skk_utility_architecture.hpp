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
#ifndef SKK_UTILITY_ARCHITECTURE_HPP
#define SKK_UTILITY_ARCHITECTURE_HPP

#include "skk_gcc.hpp"

namespace YaSkkServ
{
namespace SkkUtility
{
void printf(const char *p, ...);
inline int chmod(const char *path, mode_t mode)
{
        return ::chmod(path, mode);
}
inline void sleep(int second)
{
        ::sleep(static_cast<unsigned int>(second));
}

class DictionaryPermission
{
public:
        DictionaryPermission(const char *filename) :
                filename_(filename),
                stat_(),
                stat_result_(stat(filename_, &stat_)),
                euid_(geteuid())
        {
        }

        bool isExist() const
        {
                if (stat_result_ == -1)
                {
                        return false;
                }
                return true;
        }
        // 真ならば正しいパーミッションです。偽ならばメッセージも表示します。
        bool checkAndPrintPermission() const
        {
                DEBUG_ASSERT(isExist());
                if ((stat_.st_uid == 0) || (stat_.st_uid == euid_))
                {
                        if (stat_.st_mode & 0022)
                        {
                                SkkUtility::printf("illegal permission 0%o \"%s\"\n"
                                                   "Try chmod go-w %s\n"
                                                   ,
                                                   stat_.st_mode & 0777,
                                                   filename_,
                                                   filename_);
                                return false;
                        }
                }
                else
                {
                        SkkUtility::printf("illegal owner \"%s\"\n"
                                           "file owner must be root or same euid\n"
                                           ,
                                           filename_);
                        return false;
                }
                return true;
        }

private:
        const char *filename_;
        struct stat stat_;
        int stat_result_;
        uid_t euid_;
};

class WaitAndCheckForGoogleJapaneseInput
{
public:
        WaitAndCheckForGoogleJapaneseInput() :
                time_start_()
        {
                gettimeofday(&time_start_, 0);
        }
// 偽ならばタイムアウトです。
        bool waitAndCheckLoopHead(float timeout)
        {
                // send の直後なので、ちょっと待つ。
                usleep(10 * 1000);
                // 余計な仕事もこのあたりで。
                struct timeval time_current;
                gettimeofday(&time_current, 0);
                unsigned long long usec_start = static_cast<unsigned long long>(time_start_.tv_sec * 1000 * 1000 + time_start_.tv_usec);
                unsigned long long usec_current = static_cast<unsigned long long>(time_current.tv_sec * 1000 * 1000 + time_current.tv_usec);
                if (usec_current - usec_start >= static_cast<unsigned long long>(timeout * 1000.0f * 1000.0f))
                {
                        return false;
                }
                return true;
        }
private:
        struct timeval time_start_;
};
}
}

#endif  // SKK_UTILITY_ARCHITECTURE_HPP
