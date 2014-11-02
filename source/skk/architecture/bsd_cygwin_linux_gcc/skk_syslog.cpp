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

#include "skk_syslog.hpp"

namespace YaSkkServ
{
#ifdef YASKKSERV_CONFIG_ENABLE_SYSLOG
void SkkSyslog::printf(int log_level, Level level, const char *p, ...)
{
        if (log_level <= log_level_)
        {
                va_list ap;
                va_start(ap, p);
                vsyslog(static_cast<int>(level), p, ap);
                va_end(ap);
        }
}
#else   // YASKKSERV_CONFIG_ENABLE_SYSLOG
void SkkSyslog::printf(int log_level, Level level, const char *p, ...)
{
}
#endif  // YASKKSERV_CONFIG_ENABLE_SYSLOG
}
