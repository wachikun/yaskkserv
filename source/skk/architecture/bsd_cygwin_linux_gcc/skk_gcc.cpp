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

#include "skk_gcc.hpp"

namespace YaSkkServ
{
int global_sighup_flag = 0;
int local_main(int argc, char *argv[]);

namespace
{
void signal_dictionary_update_handler(int signum)
{
        global_sighup_flag = 1;
        signum = 0;             // KILLWARNING
}
}

#ifdef YASKKSERV_DEBUG

// skk_gcc.hpp で定義されているマクロを削除
#undef new
#undef delete

void Debug::printf_core(const char *filename, int line, const char *p, ...)
{
        char buffer[16 * 1024];
        va_list ap;
        va_start(ap, p);
        vsnprintf(buffer, sizeof(buffer), p, ap);
        va_end(ap);

        FILE *file;
        time_t time_object;
        char tmp_buffer[1024];
        file = fopen("/tmp/yaskkserv.debuglog.tmp", "a");
        time_object = time(0);
        tmp_buffer[0] = '[';
        strcpy(&tmp_buffer[1], ctime(&time_object));
        for (int i = 0; i != 1024 - 8; ++i)
        {
                if (tmp_buffer[i] == '\0')
                {
                        break;
                }
                if (tmp_buffer[i] == '\n')
                {
                        tmp_buffer[i + 0] = ']';
                        tmp_buffer[i + 1] = ':';
                        tmp_buffer[i + 2] = '\0';
                        fputs(tmp_buffer, file);
                        break;
                }
        }
        fprintf(file, "%s:%d ", filename, line);
        fputs(buffer, file);
        fclose(file);
}

void Debug::print_core(const char *filename, int line, const char *p)
{
        FILE *file;
        time_t time_object;
        char tmp_buffer[1024];
        file = fopen("/tmp/yaskkserv.debuglog.tmp", "a");
        time_object = time(0);
        tmp_buffer[0] = '[';
        strcpy(&tmp_buffer[1], ctime(&time_object));
        for (int i = 0; i != 1024 - 8; ++i)
        {
                if (tmp_buffer[i] == '\0')
                {
                        break;
                }
                if (tmp_buffer[i] == '\n')
                {
                        tmp_buffer[i + 0] = ']';
                        tmp_buffer[i + 1] = ':';
                        tmp_buffer[i + 2] = '\0';
                        fputs(tmp_buffer, file);
                        break;
                }
        }
        fprintf(file, "%s:%d ", filename, line);
        fputs(p, file);
        fclose(file);
}

#endif  // YASKKSERV_DEBUG
}




#ifdef YASKKSERV_INTERNAL_DEBUG_NEW

namespace
{
enum
{
        YASKKSERV_INTERNAL_DEBUG_NEW_BUFFER_LENGTH = 16 * 1024
};

struct skk_debug_new_t
{
        void *p;
        size_t size;
        char filename[128];
        int line;
};

// extern size_t skk_debug_new_total_size;
bool skk_debug_new_first_flag = true;
size_t skk_debug_new_total_size = 0;
skk_debug_new_t skk_debug_new_buffer[YASKKSERV_INTERNAL_DEBUG_NEW_BUFFER_LENGTH];
// #pragma GCC diagnostic push
// #pragma GCC diagnostic ignored "-Wlarger-than="
// extern skk_debug_new_t skk_debug_new_buffer[YASKKSERV_INTERNAL_DEBUG_NEW_BUFFER_LENGTH];
// #pragma GCC diagnostic pop

void skk_debug_new_check_leak()
{
        YaSkkServ::Debug::printf_core(__FILE__, __LINE__,
                                      "skk_debug_new_check_leak()  skk_debug_new_total_size:%d\n",
                                      skk_debug_new_total_size);
        for (int i = 0; i != YASKKSERV_INTERNAL_DEBUG_NEW_BUFFER_LENGTH; ++i)
        {
                if (skk_debug_new_buffer[i].p)
                {
                        YaSkkServ::Debug::printf_core(__FILE__, __LINE__,
                                                      "leak:%p  size:%d  %s:%d\n",
                                                      skk_debug_new_buffer[i].p,
                                                      skk_debug_new_buffer[i].size,
                                                      skk_debug_new_buffer[i].filename,
                                                      skk_debug_new_buffer[i].line);
                }
        }
}

void skk_debug_new_initialize_first()
{
        for (int i = 0; i != YASKKSERV_INTERNAL_DEBUG_NEW_BUFFER_LENGTH; ++i)
        {
                skk_debug_new_buffer[i].p = 0;
                skk_debug_new_buffer[i].size = 0;
                skk_debug_new_buffer[i].filename[0] = '\0';
                skk_debug_new_buffer[i].line = 0;
        }
        skk_debug_new_first_flag = false;
}
}

void *operator new(size_t size)
{
        if (skk_debug_new_first_flag)
        {
                skk_debug_new_initialize_first();
        }
        int i;
        for (i = 0; i != YASKKSERV_INTERNAL_DEBUG_NEW_BUFFER_LENGTH; ++i)
        {
                if (skk_debug_new_buffer[i].p == 0)
                {
                        goto FOUND;
                }
        }

#ifdef YASKKSERV_DEBUG_PARANOIA
        YaSkkServ::Debug::paranoia_assert_(__FILE__, __LINE__, 0);
#endif  // YASKKSERV_DEBUG_PARANOIA

FOUND:
        skk_debug_new_total_size += size;

        void *p = malloc(size);
        skk_debug_new_buffer[i].p = p;
        skk_debug_new_buffer[i].size = size;
        strcpy(skk_debug_new_buffer[i].filename, "-");
        skk_debug_new_buffer[i].line = 0;
        YaSkkServ::Debug::printf_core(__FILE__, __LINE__,
                                      "DEBUG      new:%d  total:%d  p:%p  id:%d\n",
                                      size,
                                      skk_debug_new_total_size,
                                      p,
                                      i);
        return p;
}

void *operator new(size_t size, const char *filename, int line)
{
        if (skk_debug_new_first_flag)
        {
                skk_debug_new_initialize_first();
        }
        YaSkkServ::Debug::printf_core(__FILE__, __LINE__,
                                      "filename:%s:%d\n",
                                      filename,
                                      line);
        int i;
        for (i = 0; i != YASKKSERV_INTERNAL_DEBUG_NEW_BUFFER_LENGTH; ++i)
        {
                if (skk_debug_new_buffer[i].p == 0)
                {
                        goto FOUND;
                }
        }

#ifdef YASKKSERV_DEBUG_PARANOIA
        YaSkkServ::Debug::paranoia_assert_(__FILE__, __LINE__, 0);
#endif  // YASKKSERV_DEBUG_PARANOIA

FOUND:
        skk_debug_new_total_size += size;

        void *p = malloc(size);
        skk_debug_new_buffer[i].p = p;
        skk_debug_new_buffer[i].size = size;
        strcpy(skk_debug_new_buffer[i].filename, filename);
        skk_debug_new_buffer[i].line = line;
        YaSkkServ::Debug::printf_core(__FILE__, __LINE__,
                                      "DEBUG      new:%d  total:%d  p:%p  id:%d\n",
                                      size,
                                      skk_debug_new_total_size,
                                      p,
                                      i);
        return p;
}

void operator delete(void *p)
{
        if (p == 0)
        {
                YaSkkServ::Debug::printf_core(__FILE__, __LINE__,
                                              "DEBUG   delete null\n");
                return;
        }

        int i;
        for (i = 0; i != YASKKSERV_INTERNAL_DEBUG_NEW_BUFFER_LENGTH; ++i)
        {
                if (skk_debug_new_buffer[i].p == p)
                {
                        goto FOUND;
                }
        }

#ifdef YASKKSERV_DEBUG_PARANOIA
        YaSkkServ::Debug::paranoia_assert_(__FILE__, __LINE__, 0);
#endif  // YASKKSERV_DEBUG_PARANOIA

FOUND:
        size_t size = skk_debug_new_buffer[i].size;
        skk_debug_new_total_size -= size;
        YaSkkServ::Debug::printf_core(__FILE__, __LINE__,
                                      "DEBUG   delete:%d  total:%d  p:%p  id:%d  filename:%s:%d\n",
                                      size,
                                      skk_debug_new_total_size,
                                      p,
                                      i,
                                      skk_debug_new_buffer[i].filename,
                                      skk_debug_new_buffer[i].line);
        skk_debug_new_buffer[i].p = 0;
        skk_debug_new_buffer[i].size = 0;
        skk_debug_new_buffer[i].filename[0] = '\0';
        skk_debug_new_buffer[i].line = 0;
        free(p);
}

void *operator new[](size_t size)
{
        if (skk_debug_new_first_flag)
        {
                skk_debug_new_initialize_first();
        }

        int i;
        for (i = 0; i != YASKKSERV_INTERNAL_DEBUG_NEW_BUFFER_LENGTH; ++i)
        {
                if (skk_debug_new_buffer[i].p == 0)
                {
                        goto FOUND;
                }
        }

#ifdef YASKKSERV_DEBUG_PARANOIA
        YaSkkServ::Debug::paranoia_assert_(__FILE__, __LINE__, 0);
#endif  // YASKKSERV_DEBUG_PARANOIA

FOUND:
        skk_debug_new_total_size += size;

        void *p = malloc(size);
        skk_debug_new_buffer[i].p = p;
        skk_debug_new_buffer[i].size = size;
        strcpy(skk_debug_new_buffer[i].filename, "-");
        skk_debug_new_buffer[i].line = 0;
        YaSkkServ::Debug::printf_core(__FILE__, __LINE__,
                                      "DEBUG    new[]:%d  total:%d  p:%p  id:%d\n",
                                      size,
                                      skk_debug_new_total_size,
                                      p,
                                      i);
        return p;
}

void *operator new[](size_t size, const char *filename, int line)
{
        if (skk_debug_new_first_flag)
        {
                skk_debug_new_initialize_first();
        }

        YaSkkServ::Debug::printf_core(__FILE__, __LINE__,
                                      "filename:%s:%d\n", filename, line);

        int i;
        for (i = 0; i != YASKKSERV_INTERNAL_DEBUG_NEW_BUFFER_LENGTH; ++i)
        {
                if (skk_debug_new_buffer[i].p == 0)
                {
                        goto FOUND;
                }
        }

#ifdef YASKKSERV_DEBUG_PARANOIA
        YaSkkServ::Debug::paranoia_assert_(__FILE__, __LINE__, 0);
#endif  // YASKKSERV_DEBUG_PARANOIA

FOUND:
        skk_debug_new_total_size += size;

        void *p = malloc(size);
        skk_debug_new_buffer[i].p = p;
        skk_debug_new_buffer[i].size = size;
        strcpy(skk_debug_new_buffer[i].filename, filename);
        skk_debug_new_buffer[i].line = line;
        YaSkkServ::Debug::printf_core(__FILE__, __LINE__,
                                      "DEBUG    new[]:%d  total:%d  p:%p  id:%d\n",
                                      size,
                                      skk_debug_new_total_size,
                                      p,
                                      i);
        return p;
}

void operator delete[](void *p)
{
        if (p == 0)
        {
                YaSkkServ::Debug::printf_core(__FILE__, __LINE__,
                                              "DEBUG   delete[] null\n");
                return;
        }

        int i;
        for (i = 0; i != YASKKSERV_INTERNAL_DEBUG_NEW_BUFFER_LENGTH; ++i)
        {
                if (skk_debug_new_buffer[i].p == p)
                {
                        goto FOUND;
                }
        }

        YaSkkServ::Debug::printf_core(__FILE__, __LINE__,
                                      "DEBUG delete[] %p not found\n",
                                      p);
#ifdef YASKKSERV_DEBUG_PARANOIA
        YaSkkServ::Debug::paranoia_assert_(__FILE__, __LINE__, 0);
#endif  // YASKKSERV_DEBUG_PARANOIA

FOUND:
        size_t size = skk_debug_new_buffer[i].size;
        skk_debug_new_total_size -= size;
        YaSkkServ::Debug::printf_core(__FILE__, __LINE__,
                                      "DEBUG   delete:%d  total:%d  p:%p  id:%d  filename:%s:%d\n",
                                      size,
                                      skk_debug_new_total_size,
                                      p,
                                      i,
                                      skk_debug_new_buffer[i].filename,
                                      skk_debug_new_buffer[i].line);
        skk_debug_new_buffer[i].p = 0;
        skk_debug_new_buffer[i].size = 0;
        skk_debug_new_buffer[i].filename[0] = '\0';
        skk_debug_new_buffer[i].line = 0;
        free(p);
}

#endif  // YASKKSERV_INTERNAL_DEBUG_NEW




int main(int argc, char *argv[])
{
        signal(SIGHUP, YaSkkServ::signal_dictionary_update_handler);
        int result = YaSkkServ::local_main(argc, argv);
#ifdef YASKKSERV_INTERNAL_DEBUG_NEW
        skk_debug_new_check_leak();
#endif  // YASKKSERV_INTERNAL_DEBUG_NEW
        return result;
}
