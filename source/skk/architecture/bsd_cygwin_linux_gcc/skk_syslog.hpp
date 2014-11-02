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
#ifndef SKK_SYSLOG_HPP
#define SKK_SYSLOG_HPP

#include "skk_gcc.hpp"

namespace YaSkkServ
{
/*
  int log_level は --log-level による表示制御用、 enum Level は
  syslog(3) のログレベルであることに注意が必要です。
 */
class SkkSyslog
{
        SkkSyslog(SkkSyslog &source);
        SkkSyslog& operator=(SkkSyslog &source);

public:
        enum Level
        {
                LEVEL_EMERG = LOG_EMERG,
//                 LEVEL_ALART = LOG_ALART,
                LEVEL_CRIT = LOG_CRIT,
                LEVEL_ERR = LOG_ERR,
                LEVEL_WARNING = LOG_WARNING,
                LEVEL_NOTICE = LOG_NOTICE,
                LEVEL_INFO = LOG_INFO,
                LEVEL_DEBUG = LOG_DEBUG
        };

        virtual ~SkkSyslog()
        {
#ifdef YASKKSERV_CONFIG_ENABLE_SYSLOG
                closelog();
#endif  // YASKKSERV_CONFIG_ENABLE_SYSLOG
        }

#ifdef YASKKSERV_CONFIG_ENABLE_SYSLOG
        SkkSyslog(const char *identifier, int log_level) :
                log_level_(log_level)
        {
                openlog(identifier, LOG_PID, LOG_DAEMON);
        }
#else   // YASKKSERV_CONFIG_ENABLE_SYSLOG
        SkkSyslog(const char *identifier, int log_level)
        {
        }
#endif  // YASKKSERV_CONFIG_ENABLE_SYSLOG

        void setLogLevel(int log_level)
        {
#ifdef YASKKSERV_CONFIG_ENABLE_SYSLOG
                log_level_ = log_level;
#endif  // YASKKSERV_CONFIG_ENABLE_SYSLOG
        }

        void printf(int log_level, Level level, const char *p, ...);

private:
#ifdef YASKKSERV_CONFIG_ENABLE_SYSLOG
        int log_level_;
#endif  // YASKKSERV_CONFIG_ENABLE_SYSLOG
};
}

#endif  // SKK_SYSLOG_HPP
