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
#ifndef SKK_DICTIONARY_H
#define SKK_DICTIONARY_H

#include "skk_jisyo.hpp"

namespace YaSkkServ
{
/// yaskkserv 用辞書クラスです。
/**
 *
 */
class SkkDictionary
{
        SkkDictionary(SkkDictionary &source);
        SkkDictionary& operator=(SkkDictionary &source);

public:
        virtual ~SkkDictionary()
        {
                close();
        }

        SkkDictionary() :
                mtime_(),
// 以下のメンバは close() 内でも初期化が必要なことに注意が必要です。
                read_buffer_(0),
                index_(0),
                fixed_array_(0),
                block_(0),
                block_short_(0),
                string_(0),
                midasi_(0),
                henkanmojiretsu_(0),
                file_descriptor_(-1),
                before_read_offset_(-1),
                index_size_(0),
                normal_block_length_(0),
                special_block_length_(0),
                normal_string_size_(0),
                special_entry_offset_(0),
                midasi_size_(0),
                henkanmojiretsu_size_(0),
                block_size_(0),
                last_read_offset_start_(0),
                last_read_index_(0),
                last_start_block_(0),
                last_block_length_(0),
                last_block_index_(0),
                last_search_result_(false)
        {
        }

        bool open(const char *filename)
        {
                return open_system_call(filename);
        }

        bool close()
        {
                return close_system_call();
        }

/// 取得に成功すれば真を返します。辞書が更新されていれば update_flag に真を返します。取得に失敗した場合は update_flag に触れません。
/**
 * アップデートチェック機構には、
 *
 * 「辞書ファイル上書き中に実行された場合は破綻する」
 *
 * という大きな問題があることに注意が必要です。
 */
        bool isUpdateDictionary(bool &update_flag, const char *filename)
        {
                bool result;
                struct stat stat_work;
                if (stat(filename, &stat_work) == -1)
                {
                        result = false;
                }
                else
                {
                        result = true;
                        if (mtime_ != stat_work.st_mtime)
                        {
                                mtime_ = stat_work.st_mtime;
                                update_flag = true;
                        }
                        else
                        {
                                update_flag = false;
                        }
                }
                return result;
        }

/// midasi で指定したエントリを探します。見付かれば真を返します。
        bool search(const char *midasi)
        {
                last_search_result_ = search_system_call<false>(midasi);
                return last_search_result_;
        }

/// midasi で指定した文字列の 1 文字目に対応する最初のエントリを探します。見付かれば真を返します。
        bool searchForFirstCharacter(const char *midasi)
        {
                last_search_result_ = search_system_call<true>(midasi);
                return last_search_result_;
        }

/// 次のエントリを探します。見付かれば真を返します。
        bool searchNextEntry()
        {
                last_search_result_ = search_next_entry_system_call();
                return last_search_result_;
        }

/// 前回の search() または searchNextEntry() が成功しているかどうかを返します。成功していれば真を返します。
        bool isSuccess() const
        {
                return last_search_result_;
        }

/// search() または searchNextEntry() で最後に発見した「見出し」のサイズを返します。サイズに改行文字は含みません。
        int getMidasiSize() const
        {
                return midasi_size_;
        }

/// search() または searchNextEntry() で最後に発見した「見出し」のポインタを返します。
        const char *getMidasiPointer() const
        {
                return midasi_;
        }

/// search() または searchNextEntry() で最後に発見した「変換文字列」のサイズを返します。サイズに改行文字は含みません。
        int getHenkanmojiretsuSize() const
        {
                return henkanmojiretsu_size_;
        }

/// search() または searchNextEntry() で最後に発見した「変換文字列」のポインタを返します。
        const char *getHenkanmojiretsuPointer() const
        {
                return henkanmojiretsu_;
        }

private:
        template<bool is_first> bool search_system_call(const char *midasi)
        {
                DEBUG_ASSERT_POINTER(midasi);
                const int margin = 8;
                char encoded_midasi[SkkUtility::ENCODED_MIDASI_BUFFER_SIZE];
                int encoded_size = SkkUtility::encodeHiragana(midasi, encoded_midasi, sizeof(encoded_midasi) - margin);
                if (encoded_size == 0)
                {
                        encoded_midasi[0] = '\1';
                        int i;
                        for (i = 0; sizeof(encoded_midasi) - margin; ++i)
                        {
                                if ((*(midasi + i) == ' ') || (*(midasi + i) == '\0'))
                                {
                                        goto FOUND_TERMINATOR;
                                }
                                encoded_midasi[i + 1] = *(midasi + i);
                        }

                        return false;

                FOUND_TERMINATOR:
                        encoded_midasi[i + 1] = '\0';
                }
                else
                {
                        encoded_midasi[encoded_size] = '\0';
                }

                int fixed_array_index = SkkUtility::getFixedArrayIndex(encoded_midasi);
                DEBUG_ASSERT_RANGE(fixed_array_index, -1, 0xff);
                int start_block;
                int block_length;
                int read_offset_start;
                const char *string;
                if (fixed_array_index == -1)
                {
// special
                        start_block = normal_block_length_;
                        block_length = special_block_length_;
                        read_offset_start = special_entry_offset_;
                        string = string_ + normal_string_size_;
                }
                else
                {
// normal
                        start_block = (fixed_array_ + fixed_array_index)->start_block;
                        block_length = (fixed_array_ + fixed_array_index)->block_length;
                        read_offset_start = 0;
                        string = string_ + (fixed_array_ + fixed_array_index)->string_data_offset;
                }

                for (int i = 0; i != block_length; ++i)
                {
                        int cmp = SkkUtility::compareMidasi(string, 0, 510, encoded_midasi);
                        if (cmp >= 0)
                        {
                                int read_size;
                                int read_offset;
                                if (block_)
                                {
                                        read_size = (block_ + start_block + i)->getDataSize();
                                        read_offset = read_offset_start + (block_ + start_block + i)->getOffset();
                                }
                                else
                                {
                                        read_size = (block_short_ + start_block + i)->getDataSize();
                                        read_offset = (start_block + i) * block_size_;
                                }

                                if (before_read_offset_ == read_offset)
                                {
// cached
                                }
                                else
                                {
                                        if (lseek(file_descriptor_, read_offset, SEEK_SET) == -1)
                                        {
                                                DEBUG_PRINTF("#### FAILED lseek() ERROR!!\n");
                                                return false;
                                        }
                                        int read_result = static_cast<int>(read(file_descriptor_, read_buffer_, read_size));
                                        if (read_result != read_size)
                                        {
                                                DEBUG_PRINTF("#### FAILED read() ERROR!!  read_size = %d  read_result = %d\n",
                                                             read_size,
                                                             read_result);
                                                return false;
                                        }
                                        before_read_offset_ = read_offset;
                                        last_read_index_ = 0;
                                }

                                if (is_first)
                                {
                                        int index = 0;
                                        midasi_ = read_buffer_ + index;
                                        midasi_size_ = SkkUtility::getMidasiSize(read_buffer_, index, read_size);
                                        henkanmojiretsu_ = SkkUtility::getHenkanmojiretsuPointer(read_buffer_, index, read_size);
                                        henkanmojiretsu_size_ = SkkUtility::getHenkanmojiretsuSize(read_buffer_, index, read_size);
                                        last_read_offset_start_ = read_offset_start;
                                        last_read_index_ = index;
                                        last_start_block_ = start_block;
                                        last_block_length_ = block_length;
                                        last_block_index_ = i;
                                        return true;
                                }
                                else
                                {
                                        int index;
                                        if (SkkUtility::searchBinary(read_buffer_, read_size, encoded_midasi, index))
                                        {
                                                DEBUG_ASSERT(index >= 0);
                                                midasi_ = read_buffer_ + index;
                                                midasi_size_ = SkkUtility::getMidasiSize(read_buffer_, index, read_size);
                                                henkanmojiretsu_ = SkkUtility::getHenkanmojiretsuPointer(read_buffer_, index, read_size);
                                                henkanmojiretsu_size_ = SkkUtility::getHenkanmojiretsuSize(read_buffer_, index, read_size);
                                                last_read_offset_start_ = read_offset_start;
                                                last_read_index_ = index;
                                                last_start_block_ = start_block;
                                                last_block_length_ = block_length;
                                                last_block_index_ = i;
                                                return true;
                                        }

                                        if (SkkUtility::searchLinear(read_buffer_, read_size, encoded_midasi, index))
                                        {
                                                DEBUG_ASSERT(index >= 0);
                                                midasi_ = read_buffer_ + index;
                                                midasi_size_ = SkkUtility::getMidasiSize(read_buffer_, index, read_size);
                                                henkanmojiretsu_ = SkkUtility::getHenkanmojiretsuPointer(read_buffer_, index, read_size);
                                                henkanmojiretsu_size_ = SkkUtility::getHenkanmojiretsuSize(read_buffer_, index, read_size);
                                                last_read_offset_start_ = read_offset_start;
                                                last_read_index_ = index;
                                                last_start_block_ = start_block;
                                                last_block_length_ = block_length;
                                                last_block_index_ = i;
                                                return true;
                                        }

                                        return false;
                                }
                        }
                        string = skip_space(string);
                }
                return false;
        }

        bool search_next_entry_system_call()
        {
                if ((!last_search_result_) || (last_block_index_ >= last_block_length_))
                {
                        return false;
                }

                int read_size;
                int read_offset;
                if (block_)
                {
                        read_size = (block_ + last_start_block_ + last_block_index_)->getDataSize();
                        read_offset = last_read_offset_start_ + (block_ + last_start_block_ + last_block_index_)->getOffset();
                }
                else
                {
                        read_size = (block_short_ + last_start_block_ + last_block_index_)->getDataSize();
                        read_offset = (last_start_block_ + last_block_index_) * block_size_;
                }

                last_read_index_ = SkkUtility::getNextLineIndex(read_buffer_, last_read_index_, read_size);
                if (last_read_index_ < 0)
                {
                        ++last_block_index_;
                        if (last_block_index_ >= last_block_length_)
                        {
                                return false;
                        }

                        if (block_)
                        {
                                read_size = (block_ + last_start_block_ + last_block_index_)->getDataSize();
                                read_offset = last_read_offset_start_ + (block_ + last_start_block_ + last_block_index_)->getOffset();
                        }
                        else
                        {
                                read_size = (block_short_ + last_start_block_ + last_block_index_)->getDataSize();
                                read_offset = (last_start_block_ + last_block_index_) * block_size_;
                        }

                        if (lseek(file_descriptor_, read_offset, SEEK_SET) == -1)
                        {
                                DEBUG_PRINTF("#### FAILED lseek() ERROR!!\n");

                                return false;
                        }
                        int read_result = static_cast<int>(read(file_descriptor_, read_buffer_, read_size));
                        if (read_result != read_size)
                        {
                                DEBUG_PRINTF("#### FAILED read() ERROR!!  read_size = %d  read_result = %d\n",
                                             read_size,
                                             read_result);
                                return false;
                        }
                        before_read_offset_ = read_offset;
                        last_read_index_ = 0;
                }

                midasi_ = read_buffer_ + last_read_index_;
                midasi_size_ = SkkUtility::getMidasiSize(read_buffer_, last_read_index_, read_size);
                henkanmojiretsu_ = SkkUtility::getHenkanmojiretsuPointer(read_buffer_, last_read_index_, read_size);
                henkanmojiretsu_size_ = SkkUtility::getHenkanmojiretsuSize(read_buffer_, last_read_index_, read_size);
                return true;
        }

        bool open_system_call(const char *filename)
        {
                bool result;
                file_descriptor_ = ::open(filename, O_RDONLY);
                if (file_descriptor_ == -1)
                {
                        result = false;
                }
                else
                {
                        SkkJisyo::Information information;
                        struct stat stat;
                        if (fstat(file_descriptor_, &stat) == -1)
                        {
                                result = false;
                        }
                        else
                        {
                                result = true;
                                mtime_ = stat.st_mtime;
                        }
                        {
                                off_t lseek_offset = sizeof(information);
                                if (result && (lseek(file_descriptor_, -lseek_offset, SEEK_END) == -1))
                                {
                                        result = false;
                                }
                                else
                                {
                                        result = true;
                                }
                        }

                        if (result && (read(file_descriptor_, &information, sizeof(information)) == -1))
                        {
                                result = false;
                        }

                        if (result)
                        {
                                if (information.get(SkkJisyo::Information::ID_IDENTIFIER) != SkkJisyo::IDENTIFIER)
                                {
                                        result = false;
                                }
                        }

                        if (result)
                        {
                                const int size_limit_minimum = 1 * 1024;
                                const int size_limit_maximum = 256 * 1024;
                                int index_data_offset = information.get(SkkJisyo::Information::ID_INDEX_DATA_OFFSET);
                                int index_data_size = information.get(SkkJisyo::Information::ID_INDEX_DATA_SIZE);
                                if ((index_data_offset <= 0) ||
                                    (index_data_size <= size_limit_minimum) ||
                                    (index_data_size >= size_limit_maximum))
                                {
                                        result = false;
                                }

                                if (result)
                                {
                                        if (lseek(file_descriptor_, index_data_offset, SEEK_SET) == -1)
                                        {
                                                result = false;
                                        }
                                }

                                if (result)
                                {
                                        index_ = new char[index_data_size];
                                        if (read(file_descriptor_, index_, index_data_size) != index_data_size)
                                        {
                                                result = false;
                                        }
                                        else
                                        {
                                                SkkJisyo::IndexDataHeader index_data_header;
                                                index_data_header.initialize(index_);

                                                index_size_ = index_data_size;
                                                const int block_size_limit_minimum = 32;
                                                const int block_size_limit_maximum = 256 * 1024;
                                                block_size_ = index_data_header.get(SkkJisyo::IndexDataHeader::ID_BLOCK_SIZE);
                                                if ((block_size_ < block_size_limit_minimum) ||
                                                    (block_size_ > block_size_limit_maximum))
                                                {
                                                        result = false;
                                                }
                                                else
                                                {
                                                        const int margin = 32;
                                                        read_buffer_ = new char[block_size_ + margin];

                                                        normal_block_length_ = index_data_header.get(SkkJisyo::IndexDataHeader::ID_NORMAL_BLOCK_LENGTH);
                                                        special_block_length_ = index_data_header.get(SkkJisyo::IndexDataHeader::ID_SPECIAL_BLOCK_LENGTH);
                                                        normal_string_size_ = index_data_header.get(SkkJisyo::IndexDataHeader::ID_NORMAL_STRING_SIZE);
                                                        special_entry_offset_ = index_data_header.get(SkkJisyo::IndexDataHeader::ID_SPECIAL_ENTRY_OFFSET);

                                                        int index_data_string_offset = 0;
                                                        index_data_string_offset += normal_block_length_;
                                                        index_data_string_offset += special_block_length_;
                                                        fixed_array_ = reinterpret_cast<SkkJisyo::FixedArray*>(index_ + SkkJisyo::IndexDataHeader::getSize());
                                                        if (index_data_header.get(SkkJisyo::IndexDataHeader::ID_BIT_FLAG) & SkkJisyo::IndexDataHeader::BIT_FLAG_BLOCK_SHORT)
                                                        {
                                                                block_ = 0;
                                                                block_short_ = reinterpret_cast<SkkJisyo::BlockShort*>(index_ + SkkJisyo::IndexDataHeader::getSize() + sizeof(SkkJisyo::FixedArray) * 256);
                                                                index_data_string_offset *= static_cast<int>(sizeof(SkkJisyo::BlockShort));
                                                        }
                                                        else
                                                        {
                                                                block_ = reinterpret_cast<SkkJisyo::Block*>(index_ + SkkJisyo::IndexDataHeader::getSize() + sizeof(SkkJisyo::FixedArray) * 256);
                                                                block_short_ = 0;
                                                                index_data_string_offset *= static_cast<int>(sizeof(SkkJisyo::Block));
                                                        }
                                                        string_ = index_ + SkkJisyo::IndexDataHeader::getSize() + sizeof(SkkJisyo::FixedArray) * 256 + index_data_string_offset;
                                                }
                                        }
                                }
                        }
                }
                return result;
        }

        bool close_system_call()
        {
                delete[] read_buffer_;

                if (file_descriptor_ >= 0)
                {
                        ::close(file_descriptor_);
                }

                delete[] index_;

                read_buffer_ = 0;
                index_ = 0;
                fixed_array_ = 0;
                block_ = 0;
                block_short_ = 0;
                string_ = 0;
                midasi_ = 0;
                henkanmojiretsu_ = 0;
                file_descriptor_ = -1;
                before_read_offset_ = -1;
                index_size_ = 0;
                normal_block_length_ = 0;
                special_block_length_ = 0;
                normal_string_size_ = 0;
                special_entry_offset_ = 0;
                midasi_size_ = 0;
                henkanmojiretsu_size_ = 0;
                block_size_ = 0;
                last_read_offset_start_ = 0;
                last_read_index_ = 0;
                last_start_block_ = 0;
                last_block_length_ = 0;
                last_block_index_ = 0;
                last_search_result_ = false;

                return true;
        }

/// 1 行後の行頭へのポインタを返します。
        static const char *get_next_line(const char *p)
        {
                return SkkUtility::getNextPointer<'\n'>(p);
        }

        static const char *skip_space(const char *p)
        {
                return SkkUtility::getNextPointer<' '>(p);
        }

private:
        time_t mtime_;
        char *read_buffer_;
        char *index_;
        SkkJisyo::FixedArray *fixed_array_;
        SkkJisyo::Block *block_;
        SkkJisyo::BlockShort *block_short_;
        char *string_;
        const char *midasi_;
        const char *henkanmojiretsu_;
        int file_descriptor_;
        int before_read_offset_;
        int index_size_;
        int normal_block_length_;
        int special_block_length_;
        int normal_string_size_;
        int special_entry_offset_;
        int midasi_size_;
        int henkanmojiretsu_size_;
        int block_size_;
        int last_read_offset_start_;
        int last_read_index_;
        int last_start_block_;
        int last_block_length_;
        int last_block_index_;
        bool last_search_result_;
};
}

#endif  // SKK_DICTIONARY_H
