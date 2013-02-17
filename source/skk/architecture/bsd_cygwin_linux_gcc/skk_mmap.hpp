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
#ifndef SKK_MMAP_HPP
#define SKK_MMAP_HPP

#include "skk_gcc.hpp"

namespace YaSkkServ
{
class SkkMmap
{
        SkkMmap(SkkMmap &source);
        SkkMmap& operator=(SkkMmap &source);

public:
        virtual ~SkkMmap()
        {
                if (buffer_)
                {
                        int result = munmap(buffer_, filesize_);
                        if (result < 0)
                        {
                                DEBUG_PRINTF("munmap failed\n");
                        }
                }

                if (file_descriptor_ >= 0)
                {
                        close(file_descriptor_);
                }
        }

        SkkMmap() :
                file_descriptor_(-1),
                filesize_(0),
                buffer_(0)
        {
        }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
/// ファイルをメモリ領域にマップします。成功した場合はメモリ領域へのポインタを、失敗した場合は 0 を返します。対象が通常のファイルの場合のみ成功します。
        void *map(const char *filename)
        {
                file_descriptor_ = open(filename, O_RDONLY);
                if (file_descriptor_ < 0)
                {
                        DEBUG_PRINTF("file \"%s\" open failed.\n", filename);
                }
                else
                {
                        struct stat stat_buffer;
                        fstat(file_descriptor_, &stat_buffer);
                        if (!S_ISREG(stat_buffer.st_mode))
                        {
                                DEBUG_PRINTF("S_ISREG() failed.\n");
                        }
                        else
                        {
                                filesize_ = static_cast<int>(stat_buffer.st_size);
                                buffer_ = mmap(0, filesize_, PROT_READ, MAP_PRIVATE, file_descriptor_, 0);
                                if (buffer_ == MAP_FAILED)
                                {
                                        DEBUG_PRINTF("mmap failed.\n");
                                        buffer_ = 0;
                                }
                        }
                }
                return buffer_;
        }
#pragma GCC diagnostic pop

/// マップしたメモリ領域へのポインタを返します。マップに失敗したか、 map() を実行していなかった場合は 0 を返します。このときデバッグビルドならばアサートします。
        void *getBuffer()
        {
                DEBUG_ASSERT(file_descriptor_ != -1);
                DEBUG_ASSERT_POINTER(buffer_);
                return buffer_;
        }

/// マップしたファイルのサイズを返します。マップに失敗したか、 map() を実行していなかった場合は 0 を返します。このときデバッグビルドならばアサートします。
        int getFilesize()
        {
                DEBUG_ASSERT(file_descriptor_ != -1);
                DEBUG_ASSERT_POINTER(buffer_);
                return filesize_;
        }

private:
        int file_descriptor_;
        int filesize_;
        void *buffer_;
};
}

#endif  // SKK_MMAP_HPP
