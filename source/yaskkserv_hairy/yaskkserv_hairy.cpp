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
#include "skk_simple_string.hpp"

// #define YASKKSERV_HAIRY_TEST

namespace YaSkkServ
{
namespace
{
#define SERVER_IDENTIFIER "hairy"
char version_string[] = YASKKSERV_VERSION ":yaskkserv_" SERVER_IDENTIFIER " ";

#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
class SimpleStringForHairy : public SkkSimpleString
{
public:
        virtual ~SimpleStringForHairy()
        {
        }
        SimpleStringForHairy(int buffer_size) :
                SkkSimpleString(buffer_size),
                read_index_(0)
        {
        }

        void reset()
        {
                SkkSimpleString::reset();
                read_index_ = 0;
        }

        bool appendOctet(int code)
        {
                if (!isAppendSize(4))
                {
                        return false;
                }
                int c_3 = code / 64;
                code -= c_3 * 64;
                int c_2 = code / 8;
                code -= c_2 * 8;
                c_3 += static_cast<int>(static_cast<unsigned char>('0'));
                c_2 += static_cast<int>(static_cast<unsigned char>('0'));
                code += static_cast<int>(static_cast<unsigned char>('0'));
                appendFast('\\');
                appendFast(static_cast<char>(c_3));
                appendFast(static_cast<char>(c_2));
                appendFast(static_cast<char>(code));
                return true;
        }

        void setReadIndex(int index)
        {
                read_index_ = index;
        }
        int getReadIndex() const
        {
                return read_index_;
        }
        bool isReadIndex(int index) const
        {
                int margin = 4;
                int valid_buffer_size = getValidBufferSize() - margin;
                DEBUG_ASSERT(index >= 0);
                if ((index < valid_buffer_size) && (index >= 0))
                {
                        return true;
                }
                return false;
        }
#if 0
        char readCharacter(char &result) const
        {
                if (isReadIndex(read_index_))
                {
                        result = getCharacter(read_index_);
                        return true;
                }
                return false;
        }
#endif
        bool readCharacterAndAddReadIndex(char &result)
        {
                if (isReadIndex(read_index_))
                {
                        result = getCharacter(read_index_);
                        ++read_index_;
                        return true;
                }
                return false;
        }

protected:
        int read_index_;
};


class SimpleCache
{
private:
        SimpleCache(SimpleCache &source);
        SimpleCache& operator=(SimpleCache &source);

        enum
        {
                FAST_KEY_LENGTH = 28,
                FAST_VALUE_LENGTH = 256 - FAST_KEY_LENGTH,
                LARGE_KEY_LENGTH = 128,
                LARGE_VALUE_LENGTH = 2 * 1024 - LARGE_KEY_LENGTH,
        };
        struct FastKey
        {
                FastKey() :
                        hash_binary_size(0)
                {
                }
                bool isValidKeySize(int key_binary_size) const
                {
                        return key_binary_size < FAST_KEY_LENGTH;
                }
                uint32_t hash_binary_size; // 上位 16bit が hash で下位 16bit がサイズ。 getHashBinarySize() で作成します。
        };
        struct FastValue
        {
                FastValue() :
                        binary_size(0),
                        key(),
                        value()
                {
                        memset(key, 0, FAST_KEY_LENGTH);
                        memset(value, 0, FAST_VALUE_LENGTH);
                }
                int binary_size;
                char key[FAST_KEY_LENGTH];
                char value[FAST_VALUE_LENGTH];
        };
        struct LargeKey
        {
                LargeKey() :
                        hash_binary_size(0)
                {
                }
                bool isValidKeySize(int key_binary_size) const
                {
                        return key_binary_size < LARGE_KEY_LENGTH;
                }
                uint32_t hash_binary_size; // 上位 16bit が hash で下位 16bit がサイズ。 getHashBinarySize() で作成します。
        };
        struct LargeValue
        {
                LargeValue() :
                        binary_size(0),
                        key(),
                        value()
                {
                        memset(key, 0, LARGE_KEY_LENGTH);
                        memset(value, 0, LARGE_VALUE_LENGTH);
                }
                int binary_size;
                char key[LARGE_KEY_LENGTH];
                char value[LARGE_VALUE_LENGTH];
        };

public:
        virtual ~SimpleCache()
        {
                delete[] fast_key_;
                delete[] fast_value_;
                delete[] large_key_;
                delete[] large_value_;
        }
        SimpleCache() :
                fast_entries_(0),
                fast_index_(0),
                large_entries_(0),
                large_index_(0),
                fast_key_(0),
                fast_value_(0),
                large_key_(0),
                large_value_(0)
        {
        }

        void create(int fast_entries, int large_entries)
        {
                if ((fast_entries_ != 0) || (large_entries_ != 0))
                {
                        DEBUG_ASSERT(0);
                }
                else
                {
                        fast_entries_ = fast_entries;
                        fast_index_ = 0;
                        large_entries_ = large_entries;
                        large_index_ = 0;
                        if (fast_entries_ > 0)
                        {
                                fast_key_ = new FastKey[fast_entries_];
                                fast_value_ = new FastValue[fast_entries_];
                        }
                        if (large_entries_ > 0)
                        {
                                large_key_ = new LargeKey[large_entries_];
                                large_value_ = new LargeValue[large_entries_];
                        }
                        DEBUG_PRINTF("fast_entries=%d  large_entries=%d  fast size=%d  large size=%d\n",
                                     fast_entries_,
                                     large_entries_,
                                     fast_entries_ * (sizeof(FastKey) + sizeof(FastValue)),
                                     large_entries_ * (sizeof(LargeKey) + sizeof(LargeValue)));
                }
        }

        static bool isFast(int key_binary_size, int value_binary_size)
        {
                return ((key_binary_size < FAST_KEY_LENGTH) &&
                        (value_binary_size < FAST_VALUE_LENGTH));
        }
        static bool isLarge(int key_binary_size, int value_binary_size)
        {
                return ((key_binary_size < LARGE_KEY_LENGTH) &&
                        (value_binary_size < LARGE_VALUE_LENGTH));
        }

        void appendInformation(SkkSimpleString &string)
        {
                int fast_use_count = 0;
                for (int i = 0; i != fast_entries_; ++i)
                {
                        if ((fast_key_ + i)->hash_binary_size != 0)
                        {
                                ++fast_use_count;
                        }
                }
                int large_use_count = 0;
                for (int i = 0; i != large_entries_; ++i)
                {
                        if ((large_key_ + i)->hash_binary_size != 0)
                        {
                                ++large_use_count;
                        }
                }
                const char fast[] = "fast=";
                const char large[] = " large=";
                string.append(fast, sizeof(fast) - 1);
                string.append(fast_index_);
                string.append('/');
                string.append(fast_use_count);
                string.append('/');
                string.append(fast_entries_);

                string.append(large, sizeof(large) - 1);
                string.append(large_index_);
                string.append('/');
                string.append(large_use_count);
                string.append('/');
                string.append(large_entries_);
        }

        // キャッシュを出力するのに必要なバッファサイズを返します。
        int getCacheBufferSize() const
        {
                const int entries_size = 2 * 4;
                const int index_size = 2 * 4;
                return (entries_size +
                        index_size +
                        fast_entries_ * static_cast<int>(sizeof(FastKey)) +
                        fast_entries_ * static_cast<int>(sizeof(FastValue)) +
                        large_entries_ * static_cast<int>(sizeof(LargeKey)) +
                        large_entries_ * static_cast<int>(sizeof(LargeValue)));
        }

        void getCacheBufferInformation(int &fast_entries, int &fast_index, int &large_entries, int &large_index) const
        {
                fast_entries = fast_entries_;
                fast_index = fast_index_;
                large_entries = large_entries_;
                large_index = large_index_;
        }

        // キャッシュの内容を buffer に書き出します。書き出した次のバッファポインタを返します。
        char *getCacheForSave(char *buffer) const
        {
                SkkUtility::copyMemory(&fast_entries_, buffer, 4);
                buffer += 4;
                SkkUtility::copyMemory(&fast_index_, buffer, 4);
                buffer += 4;
                SkkUtility::copyMemory(&large_entries_, buffer, 4);
                buffer += 4;
                SkkUtility::copyMemory(&large_index_, buffer, 4);
                buffer += 4;
                SkkUtility::copyMemory(fast_key_, buffer, fast_entries_ * static_cast<int>(sizeof(FastKey)));
                buffer += fast_entries_ * sizeof(FastKey);
                SkkUtility::copyMemory(fast_value_, buffer, fast_entries_ * static_cast<int>(sizeof(FastValue)));
                buffer += fast_entries_ * sizeof(FastValue);
                SkkUtility::copyMemory(large_key_, buffer, large_entries_ * static_cast<int>(sizeof(LargeKey)));
                buffer += large_entries_ * sizeof(LargeKey);
                SkkUtility::copyMemory(large_value_, buffer, large_entries_ * static_cast<int>(sizeof(LargeValue)));
                buffer += large_entries_ * sizeof(LargeValue);
                return buffer;
        }

        // buffer の内容が妥当かどうか調べます。内容がおかしい場合は 0 を返します。正しい場合は次のバッファポインタを返します。
        const char *verifyBufferForLoad(const char *buffer)
        {
                int fast_entries;
                int fast_index;
                int large_entries;
                int large_index;
                SkkUtility::copyMemory(buffer, &fast_entries, 4);
                buffer += 4;
                SkkUtility::copyMemory(buffer, &fast_index, 4);
                buffer += 4;
                SkkUtility::copyMemory(buffer, &large_entries, 4);
                buffer += 4;
                SkkUtility::copyMemory(buffer, &large_index, 4);
                buffer += 4;
                if ((fast_index >= fast_entries) || (fast_index < 0) || (fast_entries < 0))
                {
                        return 0;
                }
                if ((large_index >= large_entries) || (large_index < 0) || (large_entries < 0))
                {
                        return 0;
                }
                buffer += fast_entries * sizeof(FastKey);
                buffer += fast_entries * sizeof(FastValue);
                buffer += large_entries * sizeof(LargeKey);
                buffer += large_entries * sizeof(LargeValue);
                return buffer;
        }

        // buffer をキャッシュに書き込みます。次のバッファポインタを返します。正しいデータであることを前提としているので、あらかじめ verifyBufferForLoad() で、妥当性をチェックしておく必要があります。
        const char *setCacheForLoad(const char *buffer)
        {
                int file_fast_entries;
                int file_fast_index;
                int file_large_entries;
                int file_large_index;
                SkkUtility::copyMemory(buffer, &file_fast_entries, 4);
                buffer += 4;
                SkkUtility::copyMemory(buffer, &file_fast_index, 4);
                buffer += 4;
                SkkUtility::copyMemory(buffer, &file_large_entries, 4);
                buffer += 4;
                SkkUtility::copyMemory(buffer, &file_large_index, 4);
                buffer += 4;
                int copy_fast_entries = file_fast_entries;
                int copy_large_entries = file_large_entries;
                if (file_fast_index >= fast_entries_)
                {
                        fast_index_ = 0;
                }
                else
                {
                        fast_index_ = file_fast_index;
                }
                if (file_large_index >= large_entries_)
                {
                        large_index_ = 0;
                }
                else
                {
                        large_index_ = file_large_index;
                }
                if (file_fast_entries > fast_entries_)
                {
                        copy_fast_entries = fast_entries_;
                }
                if (file_large_entries > large_entries_)
                {
                        copy_large_entries = large_entries_;
                }
                DEBUG_PRINTF("copy_fast_entries=%d    fast_entries_=%d  fast_index_=%d    file_fast_entries=%d  file_fast_index=%d\n",
                             copy_fast_entries, fast_entries_, fast_index_, file_fast_entries, file_fast_index);
                DEBUG_PRINTF("copy_large_entries=%d    large_entries_=%d  large_index_=%d    file_large_entries=%d  file_large_index=%d\n",
                             copy_large_entries, large_entries_, large_index_, file_large_entries, file_large_index);
                SkkUtility::copyMemory(buffer, fast_key_, copy_fast_entries * static_cast<int>(sizeof(FastKey)));
                buffer += file_fast_entries * sizeof(FastKey);
                SkkUtility::copyMemory(buffer, fast_value_, copy_fast_entries * static_cast<int>(sizeof(FastValue)));
                buffer += file_fast_entries * sizeof(FastValue);
                SkkUtility::copyMemory(buffer, large_key_, copy_large_entries * static_cast<int>(sizeof(LargeKey)));
                buffer += file_large_entries * sizeof(LargeKey);
                SkkUtility::copyMemory(buffer, large_value_, copy_large_entries * static_cast<int>(sizeof(LargeValue)));
                buffer += file_large_entries * sizeof(LargeValue);
                return buffer;
        }

protected:
        uint32_t getHashBinarySize(const SimpleStringForHairy &key) const
        {
                uint32_t hash = 0;
                uint32_t shift = 0;
                uint32_t size = static_cast<uint32_t>(key.getSize());
                const uint8_t * const p = reinterpret_cast<const uint8_t * const>(key.getBuffer());
                for (uint32_t i = 0; i != size; ++i)
                {
                        uint32_t tmp = static_cast<uint32_t>(*(p + i));
                        hash += tmp << shift;
                        if (++shift >= 9)
                        {
                                shift = 0;
                        }
                }
                const int terminator_size = 1;
                return (hash << 16) | (size + terminator_size);
        }
        template<typename T_1, typename T_2> bool getBase(const SimpleStringForHairy &key,
                                                          SimpleStringForHairy &value,
                                                          int entries,
                                                          const T_1 &key_table,
                                                          T_2 &value_table) const
        {
                if (entries > 0)
                {
                        const char * const key_pointer = key.getBuffer();
                        const int terminator_size = 1;
                        int key_compare_binary_size = key.getSize() + terminator_size;
                        uint32_t key_compare_hash = getHashBinarySize(key);
                        DEBUG_PRINTF("getBase() key_compare_hash=0x%08x\n", key_compare_hash);
                        DEBUG_ASSERT(key_compare_binary_size > 0);
                        DEBUG_ASSERT(key_table->isValidKeySize(key_compare_binary_size));
                        if ((key_compare_binary_size > 0) && key_table->isValidKeySize(key_compare_binary_size))
                        {
                                for (int i = 0; i != entries; ++i)
                                {
                                        if (((key_table + i)->hash_binary_size == key_compare_hash) &&
                                            (memcmp(key_pointer, (value_table + i)->key, static_cast<size_t>(key_compare_binary_size)) == 0))
                                        {
                                                DEBUG_ASSERT(value.getValidBufferSize() > (value_table + i)->binary_size);
                                                if (value.getValidBufferSize() > (value_table + i)->binary_size)
                                                {
                                                        value.reset();
                                                        value.appendFast((value_table + i)->value);
                                                        DEBUG_PRINTF("getBase() value=%s size=%d cur=%p\n", value.getBuffer(), value.getSize(), value.getCurrentBuffer());
                                                        return true;
                                                }
                                        }
                                }
                        }
                }
                return false;
        }
        template<typename T_1, typename T_2> bool setBase(const SimpleStringForHairy &key,
                                                          const SimpleStringForHairy &value,
                                                          int entries,
                                                          const T_1 &key_table,
                                                          T_2 &value_table,
                                                          int &index) const
        {
                if (entries > 0)
                {
                        const int terminator_size = 1;
                        int key_copy_size = key.getSize() + terminator_size;
                        (key_table + index)->hash_binary_size = getHashBinarySize(key);
                        DEBUG_PRINTF("setBase() key_compare_hash=0x%08x  value size=%d\n", (key_table + index)->hash_binary_size, value.getSize());
                        memcpy((value_table + index)->key, key.getBuffer(), static_cast<size_t>(key_copy_size));
                        int value_copy_size = value.getSize() + terminator_size;
                        (value_table + index)->binary_size = value_copy_size;
                        memcpy((value_table + index)->value, value.getBuffer(), static_cast<size_t>(value_copy_size));
                        ++index;
                        if (index >= entries)
                        {
                                index = 0;
                        }
                        return true;
                }
                return false;
        }

public:
        bool getFast(const SimpleStringForHairy &key, SimpleStringForHairy &value) const
        {
                return getBase(key, value, fast_entries_, fast_key_, fast_value_);
        }
        bool setFast(const SimpleStringForHairy &key, const SimpleStringForHairy &value)
        {
                return setBase(key, value, fast_entries_, fast_key_, fast_value_, fast_index_);
        }

        bool getLarge(const SimpleStringForHairy &key, SimpleStringForHairy &value) const
        {
                return getBase(key, value, large_entries_, large_key_, large_value_);
        }
        bool setLarge(const SimpleStringForHairy &key, const SimpleStringForHairy &value)
        {
                return setBase(key, value, large_entries_, large_key_, large_value_, large_index_);
        }

        bool set(const SimpleStringForHairy &key, const SimpleStringForHairy &value)
        {
                int terminator_size = 1;
                if (isFast(key.getSize() + terminator_size, value.getSize()))
                {
                        return setFast(key, value);
                }
                else if (isLarge(key.getSize() + terminator_size, value.getSize()))
                {
                        return setLarge(key, value);
                }
                return false;
        }

        bool get(const SimpleStringForHairy &key, SimpleStringForHairy &value) const
        {
                int terminator_size = 1;
                if (isFast(key.getSize() + terminator_size, 0))
                {
                        return getFast(key, value);
                }
                else if (isLarge(key.getSize() + terminator_size, 0))
                {
                        return getLarge(key, value);
                }
                return false;
        }

private:
        int fast_entries_;
        int fast_index_;
        int large_entries_;
        int large_index_;
        FastKey *fast_key_;
        FastValue *fast_value_;
        LargeKey *large_key_;
        LargeValue *large_value_;
};


class GoogleJapaneseInput
{
        GoogleJapaneseInput(GoogleJapaneseInput &source);
        GoogleJapaneseInput& operator=(GoogleJapaneseInput &source);

public:
        virtual ~GoogleJapaneseInput()
        {
        }

        GoogleJapaneseInput() :
                skk_socket_(),
                work_a_buffer_(4 * 1024),
                work_b_buffer_(4 * 1024),
                midashi_buffer_(1 * 1024),
                http_send_buffer_(2 * 1024),
                http_receive_buffer_(4 * 1024),
                skk_output_buffer_(4 * 1024),
                http_send_string_(1 * 1024),
                cache_(),
#if defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                is_https_(true),
#else  // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                is_https_(false),
#endif  // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                is_ipv6_(false)
        {
        }

        void createCache(int cache_entries)
        {
                int fast_cache_entries = cache_entries;
                int large_cache_entries = cache_entries / 16;
                cache_.create(fast_cache_entries, large_cache_entries);
        }

        void appendCacheInformation(SkkSimpleString &string)
        {
                cache_.appendInformation(string);
        }

        int getCacheBufferSize() const
        {
                return cache_.getCacheBufferSize();
        }

        void getCacheBufferInformation(int &fast_entries, int &fast_index, int &large_entries, int &large_index) const
        {
                cache_.getCacheBufferInformation(fast_entries, fast_index, large_entries, large_index);
        }

        char *getCacheForSave(char *buffer) const
        {
                return cache_.getCacheForSave(buffer);
        }

        const char *verifyBufferForLoad(const char *buffer)
        {
                return cache_.verifyBufferForLoad(buffer);
        }

        const char *setCacheForLoad(const char *buffer)
        {
                return cache_.setCacheForLoad(buffer);
        }

        bool isHttps() const
        {
                return is_https_;
        }
        void setHttpsFlag(bool is_https)
        {
                is_https_ = is_https;
        }
        void setIpv6Flag(bool is_ipv6)
        {
                is_ipv6_ = is_ipv6;
        }

        // 成功すれば、内部バッファのポインタを、失敗した場合は 0 を返します。 search_word の終端コードはスペースである必要があります。
        const char *getSkkCandidatesEuc(const char *search_word, float timeout)
        {
                int search_word_size = 0;
                for (search_word_size = 0; ; ++search_word_size)
                {
                        int c = static_cast<int>(static_cast<unsigned char>(*(search_word + search_word_size)));
                        if (c == '\0')
                        {
                                return 0;
                        }
                        if (c == ' ')
                        {
                                break;
                        }
                }
                // euc  2bytes
                // utf8 3bytes
                // url  9bytes
                // なので、多目にみて 5 倍で計算。元々バッファには扱う
                // サイズに対して非常に大きなサイズを確保指定しているの
                // で、このくらいおおまかで問題ない。
                if (search_word_size >= work_b_buffer_.getValidBufferSize() / 5)
                {
                        return 0;
                }
                work_a_buffer_.reset();
                midashi_buffer_.reset();
                skk_output_buffer_.reset();
                DEBUG_PRINTF("google search_word_size=\"%d\"\n", search_word_size);
                for (int i = 0; ; ++i)
                {
                        int c = static_cast<int>(static_cast<unsigned char>(*(search_word + i)));
                        if (c == '\0')
                        {
                                return 0;
                        }
                        if (c == ' ')
                        {
                                if (SkkUtility::isOkuriAri(search_word, search_word_size))
                                {
                                        // 送りありの場合は最後を削る
                                        if (i <= 0)
                                        {
                                                DEBUG_ASSERT(0);
                                        }
                                        else
                                        {
                                                midashi_buffer_.terminate(i - 1);
                                                break;
                                        }
                                }
                                break;
                        }
                        if (!midashi_buffer_.append(static_cast<char>(c)))
                        {
                                return 0;
                        }
                }
                if (cache_.get(midashi_buffer_, skk_output_buffer_))
                {
                        DEBUG_PRINTF("cache found:%s %s\n", midashi_buffer_.getBuffer(), skk_output_buffer_.getBuffer());
                }
                else
                {
                        if (!convertIconv(midashi_buffer_, work_a_buffer_, "euc-jp", "utf-8"))
                        {
                                return 0;
                        }
                        work_b_buffer_.reset();
                        if (!convertUtf8ToUrl(work_a_buffer_, work_b_buffer_))
                        {
                                return 0;
                        }
                        if (!getHttp(work_b_buffer_, timeout))
                        {
                                return 0;
                        }
                        if (!convertJsonToSkk())
                        {
                                return 0;
                        }
                        if (!confirmSkk(skk_output_buffer_, search_word, search_word_size))
                        {
                                return 0;
                        }
                        cache_.set(midashi_buffer_, skk_output_buffer_);
                }
                return skk_output_buffer_.getBuffer();
        }

        const char *getSkkCandidatesEucSuggest(const char *search_word, float timeout)
        {
                int search_word_size = 0;
                for (search_word_size = 0; ; ++search_word_size)
                {
                        int c = static_cast<int>(static_cast<unsigned char>(*(search_word + search_word_size)));
                        if (c == '\0')
                        {
                                return 0;
                        }
                        if (c == ' ')
                        {
                                break;
                        }
                }
                // euc  2bytes
                // utf8 3bytes
                // url  9bytes
                // なので、多目にみて 5 倍で計算。元々バッファには扱う
                // サイズに対して非常に大きなサイズを確保指定しているの
                // で、このくらいおおまかで問題ない。
                if (search_word_size >= work_b_buffer_.getValidBufferSize() / 5)
                {
                        return 0;
                }
                DEBUG_PRINTF("google search_word_size=\"%d\"\n", search_word_size);
                work_a_buffer_.reset();
                midashi_buffer_.reset();
                skk_output_buffer_.reset();
                for (int i = 0; ; ++i)
                {
                        int c = static_cast<int>(static_cast<unsigned char>(*(search_word + i)));
                        if (c == '\0')
                        {
                                return 0;
                        }
                        if (c == ' ')
                        {
                                if (SkkUtility::isOkuriAri(search_word, search_word_size))
                                {
                                        // 送りありの場合は最後を削る
                                        if (i <= 0)
                                        {
                                                DEBUG_ASSERT(0);
                                        }
                                        else
                                        {
                                                midashi_buffer_.terminate(i - 1);
                                                break;
                                        }
                                }
                                break;
                        }
                        if (!midashi_buffer_.append(static_cast<char>(c)))
                        {
                                return 0;
                        }
                }
                if (cache_.get(midashi_buffer_, skk_output_buffer_))
                {
                        DEBUG_PRINTF("cache found:%s %s\n", midashi_buffer_.getBuffer(), skk_output_buffer_.getBuffer());
                }
                else
                {
                        if (!convertIconv(midashi_buffer_, work_a_buffer_, "euc-jp", "utf-8"))
                        {
                                return 0;
                        }
                        work_b_buffer_.reset();
                        if (!convertUtf8ToUrl(work_a_buffer_, work_b_buffer_))
                        {
                                return 0;
                        }
                        if (!getHttpSuggest(work_b_buffer_, timeout))
                        {
                                return 0;
                        }
                        if (!convertXMLToSkk())
                        {
                                return 0;
                        }
                        if (!confirmSkk(skk_output_buffer_, search_word, search_word_size))
                        {
                                return 0;
                        }
                        cache_.set(midashi_buffer_, skk_output_buffer_);
                }
                return skk_output_buffer_.getBuffer();
        }

        static int getByteSize(const char *p)
        {
                int byte_size = 0;
                while (*(p + byte_size++)) {}
                return byte_size - 1;
        }

private:
        // from_code の source を to_code に変換して destination に書き込みます。成功すれば真を返します。失敗した場合の destination の内容は不定です。
        static bool convertIconv(const SimpleStringForHairy &source, SimpleStringForHairy &destination, const char *from_code, const char *to_code)
        {
                iconv_t icd = iconv_open(to_code, from_code);
                if (icd == reinterpret_cast<iconv_t>(-1))
                {
                        return false;
                }
                else
                {
                        const int nul_size = 1;
                        int byte_size = source.getSize() + nul_size;
#ifdef YASKKSERV_CONFIG_ICONV_ARGUMENT_CONST_CHAR
                        const char *in_buffer = source.getBuffer();
#else  // YASKKSERV_CONFIG_ICONV_ARGUMENT_CONST_CHAR
                        char *in_buffer = const_cast<char*>(source.getBuffer());
#endif  // YASKKSERV_CONFIG_ICONV_ARGUMENT_CONST_CHAR
                        char *out_buffer = const_cast<char*>(destination.getBuffer());
                        size_t in_size = static_cast<size_t>(byte_size);
                        size_t out_size = static_cast<size_t>(destination.getValidBufferSize());
                        for (;;)
                        {
                                size_t result = static_cast<size_t>(-1);
                                if (destination.beginWriteBuffer(const_cast<char*>(out_buffer), static_cast<int>(out_size)))
                                {
                                        result = iconv(icd, &in_buffer, &in_size, &out_buffer, &out_size);
                                }
                                destination.endWriteBuffer(static_cast<int>(out_size));
                                if (result == static_cast<size_t>(-1))
                                {
                                        // error
                                        DEBUG_PRINTF("iconv error\n");
                                        return false;
                                }
                                if (in_size == 0)
                                {
                                        break;
                                }
                        }
                        if (iconv_close(icd) != 0)
                        {
                                // error
                                return false;
                        }
                        destination.setAppendIndex(getByteSize(destination.getBuffer()));
                }
                return true;
        }

        // character の ASCII 0-9 A-F a-f をバイナリの 0x0-0xf に変換して result に返します。成功すれば真を返します。失敗した場合は result に触れません。
        static bool getEscapeBinary(int character, int &result)
        {
                if ((character >= '0') && (character <= '9'))
                {
                        result = character - '0';
                        return true;
                }
                if ((character >= 'A') && (character <= 'F'))
                {
                        result = 0xa + character - 'A';
                        return true;
                }
                if ((character >= 'a') && (character <= 'f'))
                {
                        result = 0xa + character - 'a';
                        return true;
                }
                return false;
        }

        // escape のエスケープ 4 桁 を UTF8 に変換して utf8 に書き込みます。成功すれば真を返します。失敗した場合の utf8 の内容は不定です。
        static bool convertEscape4ToUtf8(SimpleStringForHairy &escape, SimpleStringForHairy &utf8)
        {
                int previous_c = 0;
                for (;;)
                {
                        const int largish_margin = 6;
                        if (!escape.isAppendSize(largish_margin) || !utf8.isAppendSize(largish_margin))
                        {
                                return false;
                        }
                        char c;
                        if (!escape.readCharacterAndAddReadIndex(c))
                        {
                                return false;
                        }
                        if (c == 0)
                        {
                                utf8.appendFast(static_cast<char>(c));
                                break;
                        }
                        else if (c == '\\')
                        {
                        }
                        else if (previous_c == '\\')
                        {
                                if (c == 'u')
                                {
                                        int bin_0;
                                        int bin_1;
                                        int bin_2;
                                        int bin_3;
                                        char tmp;
                                        if (!escape.readCharacterAndAddReadIndex(tmp))
                                        {
                                                return false;
                                        }
                                        if (!getEscapeBinary(tmp, bin_0))
                                        {
                                                return false;
                                        }
                                        if (!escape.readCharacterAndAddReadIndex(tmp))
                                        {
                                                return false;
                                        }
                                        if (!getEscapeBinary(tmp, bin_1))
                                        {
                                                return false;
                                        }
                                        if (!escape.readCharacterAndAddReadIndex(tmp))
                                        {
                                                return false;
                                        }
                                        if (!getEscapeBinary(tmp, bin_2))
                                        {
                                                return false;
                                        }
                                        if (!escape.readCharacterAndAddReadIndex(tmp))
                                        {
                                                return false;
                                        }
                                        if (!getEscapeBinary(tmp, bin_3))
                                        {
                                                return false;
                                        }
                                        int unicode = (bin_0 << 12) | (bin_1 << 8) | (bin_2 << 4) | bin_3;
                                        int utf8_data = (((unicode & 0xf000) << 4) | 0xe00000) | (((unicode & 0xfc0) << 2) | 0x8000) | ((unicode & 0x3f) | 0x80);
                                        utf8.appendFast(static_cast<char>((utf8_data >> 16) & 0xff));
                                        utf8.appendFast(static_cast<char>((utf8_data >> 8) & 0xff));
                                        utf8.appendFast(static_cast<char>(utf8_data & 0xff));
                                }
                                else
                                {
                                        utf8.appendFast(static_cast<char>(c));
                                }
                        }
                        else
                        {
                                utf8.appendFast(static_cast<char>(c));
                        }
                        previous_c = c;
                }
                return true;
        }

        static bool convertUtf8ToUrl(SimpleStringForHairy &source, SimpleStringForHairy &destination)
        {
                // URL encode が 1byte -> 3byte なので、
                // SkkSimpleString のマージンなどを多目にみて 4 倍で計
                // 算。
                if (source.getSize() >= destination.getValidBufferSize() / 4)
                {
                        return false;
                }
                for (;;)
                {
                        char c;
                        if (!source.readCharacterAndAddReadIndex(c))
                        {
                                return false;
                        }
                        if (c == 0)
                        {
                                break;
                        }
                        static const char table[] = "0123456789ABCDEF";
                        if (!destination.isAppendSize(3))
                        {
                                return false;
                        }
                        destination.appendFast('%');
                        destination.appendFast(table[(c >> 4) & 0xf]);
                        destination.appendFast(table[c & 0xf]);
                }
                return true;
        }

        bool isBusySocket()
        {
                bool result = false;
                switch (errno)
                {
                default:
                        break;
                case EAGAIN:
                case EINTR:
                case ECONNABORTED:
                case ECONNREFUSED:
                case ECONNRESET:
                        result = true;
                        break;
                }
                return result;
        }

        int send(int socket_fd)
        {
                int send_result;
                for (;;)
                {
#if defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                        if (isHttps())
                        {
                                send_result = skk_socket_.writeSsl(http_send_string_.getBuffer(), http_send_string_.getSize());
                                DEBUG_PRINTF("send_result=%d  data size=%d\n", send_result, http_send_string_.getSize());
                        }
                        else
                        {
                                send_result = SkkSocket::send(socket_fd, http_send_string_.getBuffer(), http_send_string_.getSize());
                        }
#else  // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                        send_result = SkkSocket::send(socket_fd, http_send_string_.getBuffer(), http_send_string_.getSize());
#endif  // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                        if (send_result == http_send_string_.getSize())
                        {
                                break;
                        }
#if defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                        if (isHttps())
                        {
                                if (!skk_socket_.isBusySsl(send_result))
                                {
                                        DEBUG_PRINTF("busy break send_result=%d\n", send_result);
                                        break;
                                }
                        }
                        else
                        {
                                if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
                                {
                                        break;
                                }
                        }
#endif  // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                        usleep(10 * 1000);
                }
                return send_result;
        }

        int receive(int socket_fd, float timeout)
        {
                bool result = true;
                int receive_buffer_index = 0;
                SkkUtility::WaitAndCheckForGoogleJapaneseInput wait_and_check_for_google_japanese_input;
                for (;;)
                {
                        // select() ではなくポーリングでループ処理していることに注意が必要です。
                        if (!wait_and_check_for_google_japanese_input.waitAndCheckLoopHead(timeout))
                        {
                                result = false;
                                break;
                        }
                        const int margin = 256;
                        int receive_size = -2;
                        int unit_receive_size = http_receive_buffer_.getValidBufferSize() - receive_buffer_index - margin;
                        if (unit_receive_size <= 0)
                        {
                                DEBUG_ASSERT(0);
                                result = false;
                                break;
                        }
#if defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                        if (isHttps())
                        {
                                if (http_receive_buffer_.beginWriteBuffer(const_cast<char*>(http_receive_buffer_.getBuffer()) + receive_buffer_index,
                                                                          unit_receive_size))
                                {
                                        receive_size = skk_socket_.readSsl(const_cast<char*>(http_receive_buffer_.getBuffer()) + receive_buffer_index,
                                                                           unit_receive_size);
                                }
                                http_receive_buffer_.endWriteBuffer(receive_size);
                                DEBUG_PRINTF("receive_size=%d\n", receive_size);
                        }
                        else
                        {
                                if (http_receive_buffer_.beginWriteBuffer(const_cast<char*>(http_receive_buffer_.getBuffer()) + receive_buffer_index,
                                                                          unit_receive_size))
                                {
                                        receive_size = SkkSocket::receive(socket_fd,
                                                                          const_cast<char*>(http_receive_buffer_.getBuffer()) + receive_buffer_index,
                                                                          unit_receive_size);
                                }
                                http_receive_buffer_.endWriteBuffer(receive_size);
                        }
#else  // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                        if (http_receive_buffer_.beginWriteBuffer(const_cast<char*>(http_receive_buffer_.getBuffer()) + receive_buffer_index,
                                                                  unit_receive_size))
                        {
                                receive_size = SkkSocket::receive(socket_fd, const_cast<char*>(http_receive_buffer_.getBuffer()) + receive_buffer_index, unit_receive_size);
                        }
                        http_receive_buffer_.endWriteBuffer(receive_size);
#endif  // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                        if (receive_size == 0)
                        {
#if defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                                if ((receive_buffer_index == 0) && skk_socket_.isBusySsl(receive_size))
                                {
                                        DEBUG_PRINTF("receive_size=0 busy!\n");
                                }
                                else
#endif  // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                                {
                                        break;
                                }
                        }
                        else if (receive_size == -2)
                        {
                                // error on beginWriteBuffer()
                                result = false;
                                break;
                        }
                        else if (receive_size == -1)
                        {
                                // error/busy on receive()/readSsl()
#if defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                                if (isHttps())
                                {
                                        if (!skk_socket_.isBusySsl(receive_size))
                                        {
                                                result = false;
                                                break;
                                        }
                                }
                                else
                                {
                                        if (!isBusySocket())
                                        {
                                                result = false;
                                                break;
                                        }
                                }
#else  // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                                if (!isBusySocket())
                                {
                                        result = false;
                                        break;
                                }
#endif  // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                        }
                        else
                        {
                                receive_buffer_index += receive_size;
                                http_receive_buffer_.setAppendIndex(receive_buffer_index);
                        }
                        usleep(1 * 1000);
                }
                if (!http_receive_buffer_.append('\0'))
                {
                        return false;
                }
                return result;
        }

        bool getHttp(SimpleStringForHairy &url_encode_word, float timeout)
        {
                http_receive_buffer_.reset();
                http_send_string_.reset();
                if (isHttps())
                {
                        http_send_string_.appendFast("GET https://www.google.com/transliterate?langpair=ja-Hira|ja&text=");
                }
                else
                {
                        http_send_string_.appendFast("GET http://www.google.com/transliterate?langpair=ja-Hira|ja&text=");
                }
                {
                        const int cr_lf_and_margin = 16;
                        if (http_send_string_.getRemainSize() < url_encode_word.getSize() + cr_lf_and_margin)
                        {
                                return false;
                        }
                }
                http_send_string_.appendFast(url_encode_word);
                http_send_string_.appendFast(",\x0d\x0a\x0d\x0a");

                int socket_fd;
                if (isHttps())
                {
#if defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                        socket_fd = skk_socket_.prepareASyncSocketSsl("www.google.com", "https", timeout, is_ipv6_);
#else  // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                        socket_fd = -1;
#endif  // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                }
                else
                {
                        socket_fd = skk_socket_.prepareASyncSocket("www.google.com", "http", timeout, is_ipv6_);
                }
                bool result = socket_fd >= 0;
                if (result)
                {
                        int send_result = send(socket_fd);
                        if (send_result == -1)
                        {
                                result = false;
                        }
                        if (result)
                        {
                                result = receive(socket_fd, timeout);
                        }
#if defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                        if (isHttps())
                        {
                                skk_socket_.shutdownASyncSocketSsl();
                        }
#endif  // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                        close(socket_fd);
                }
                return result;
        }

        bool getHttpSuggest(SimpleStringForHairy &url_encode_word, float timeout)
        {
                http_receive_buffer_.reset();
                http_send_string_.reset();
                if (isHttps())
                {
                        http_send_string_.appendFast("GET https://www.google.com/complete/search?hl=ja&output=toolbar&q=");
                }
                else
                {
                        http_send_string_.appendFast("GET http://www.google.com/complete/search?hl=ja&output=toolbar&q=");
                }
                {
                        const int cr_lf_and_margin = 16;
                        if (http_send_string_.getRemainSize() < url_encode_word.getSize() + cr_lf_and_margin)
                        {
                                return false;
                        }
                }
                http_send_string_.appendFast(url_encode_word);
                http_send_string_.appendFast(",\x0d\x0a\x0d\x0a");

                int socket_fd;
                if (isHttps())
                {
#if defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                        socket_fd = skk_socket_.prepareASyncSocketSsl("www.google.com", "https", timeout, is_ipv6_);
#else  // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                        socket_fd = -1;
#endif  // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                }
                else
                {
                        socket_fd = skk_socket_.prepareASyncSocket("www.google.com", "http", timeout, is_ipv6_);
                }
                bool result = socket_fd >= 0;
                if (result)
                {
                        int send_result = send(socket_fd);
                        if (send_result == -1)
                        {
                                result = false;
                        }
                        if (result)
                        {
                                result = receive(socket_fd, timeout);
                        }
#if defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                        if (isHttps())
                        {
                                skk_socket_.shutdownASyncSocketSsl();
                        }
#endif  // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                        close(socket_fd);
                }
                return result;
        }

        // 念の為 skk バッファを確認します。不正な内容、または search_word と同じもののみが返されているならば偽を返します。 Lisp 的なコードがある場合は(問題はないとは思いますが)念の為に偽を返します。現在は必要以上にきつく見ていることに注意が必要です。
        static bool confirmSkk(SimpleStringForHairy &skk, const char *search_word, int search_word_size)
        {
                {
                        const int offset_1_slash = 2;
                        const int offset_1_slash_slash_crlf = offset_1_slash + 2;
                        // search_word_size = sizeof("WORD")
                        // search_word="WORD \n"
                        //         skk="1/WORD/\n"
                        if (search_word_size == skk.getSize() - offset_1_slash_slash_crlf)
                        {
                                const char *skk_buffer = skk.getBuffer() + offset_1_slash;
                                bool same_flag = true;
                                for (int i = 0; i < search_word_size; ++i)
                                {
                                        char c_search_word = *(search_word + i);
                                        char c_skk = *(skk_buffer + i);
                                        if (c_search_word != c_skk)
                                        {
                                                same_flag = false;
                                                break;
                                        }
                                }
                                if (same_flag)
                                {
                                        return false;
                                }
                        }
                }
                skk.setReadIndex(0);
                for (;;)
                {
                        char c;
                        if (!skk.readCharacterAndAddReadIndex(c))
                        {
                                DEBUG_PRINTF("failure readindex=%d\n", skk.getReadIndex());
                                return false;
                        }
                        if (c == 0)
                        {
                                break;
                        }
                        else if (c == '(')
                        {
                                const char concat[] = "concat ";
                                const int nul_size = 1;
                                for (int i = 0; i != sizeof(concat) - nul_size; ++i)
                                {
                                        char concat_c = concat[i];
                                        char skk_c;
                                        if (!skk.readCharacterAndAddReadIndex(skk_c))
                                        {
                                                return false;
                                        }
                                        if (concat_c != skk_c)
                                        {
                                                return false;
                                        }
                                }
                        }
                }
                return true;
        }

        // HTTP ヘッダのステータスをチェックしつつ、スキップします。不正なヘッダまたは 200 OK でないならば偽を返します。
        static bool checkStatusAndSkipHttpHeader(SimpleStringForHairy &buffer)
        {
                int cr_lf_counter = 0;
                int previous_c = 0;
                bool first_line = true;
                bool first_space = false;
                bool second_space = false;
                SkkSimpleString status_code_string(256);
                for (;;)
                {
                        if (!buffer.isAppendSize(1))
                        {
                                return false;
                        }
                        char c;
                        if (!buffer.readCharacterAndAddReadIndex(c))
                        {
                                return false;
                        }
                        if (c == 0)
                        {
                                return false;
                        }
                        if (c == 0x0a)
                        {
                                if (first_line)
                                {
                                        first_line = false;
                                        if (status_code_string.compare("200") != 0)
                                        {
                                                DEBUG_PRINTF("illegal status code=\"%s\"\n", status_code_string.getBuffer());
                                                return false;
                                        }
                                }
                                if (previous_c == 0x0d)
                                {
                                        ++cr_lf_counter;
                                        if (cr_lf_counter == 2)
                                        {
                                                break;
                                        }
                                }
                        }
                        if ((c != 0x0d) && (c != 0x0a))
                        {
                                cr_lf_counter = 0;
                        }
                        if (first_line)
                        {
                                if (c == ' ')
                                {
                                        if (!second_space)
                                        {
                                                if (first_space)
                                                {
                                                        second_space = true;
                                                }
                                                else
                                                {
                                                        first_space = true;
                                                }
                                        }
                                }
                                else if (first_space && !second_space)
                                {
                                        status_code_string.append(c);
                                }
                        }
                        previous_c = c;
                }
                return true;
        }

        static bool isAppendGoogleJapanesInput(SimpleStringForHairy &string)
        {
                int length = string.getSize();
                const char *p = string.getBuffer();
                DEBUG_PRINTF("isAppendGoogleJapanesInput() %s length=%d\n", string.getBuffer(), string.getSize());
                for (int i = 0; i < length; ++i)
                {
                        int c_1 = static_cast<int>(static_cast<unsigned char>(*p++));
                        if (c_1 >= 0x80)
                        {
                                ++i;
                                if (i >= length)
                                {
                                        DEBUG_PRINTF("illegal string %s\n", string.getBuffer());
                                        return false;
                                }
                                int c_2 = static_cast<int>(static_cast<unsigned char>(*p++));
                                if (c_1 == 0xa4) // ひらがな
                                {
                                        if ((c_2 < 0xa1) || (c_2 > 0xf3))
                                        {
                                                return true;
                                        }
                                }
                                else if (c_1 == 0xa5) // カタカナ
                                {
                                        if ((c_2 < 0xa1) || (c_2 > 0xf6))
                                        {
                                                return true;
                                        }
                                }
                                else if (c_1 == 0x8e) // いわゆる半角カタカナ
                                {
                                        if ((c_2 < 0xa6) || (c_2 > 0xdf))
                                        {
                                                return true;
                                        }
                                }
                                else if (c_1 == 0xa1) // 長音ー
                                {
                                        if (c_2 != 0xbc)
                                        {
                                                return true;
                                        }
                                }
                                else
                                {
                                        return true;
                                }
                        }
                        else
                        {
                                return true;
                        }
                }
                return false;
        }

        // json から skk に必要な最初の候補リストを SKK の「変換文字列」形式で書き込みます。ひらがなまたは、いわゆる半角を含むカタカナのみで構成される要素は削除します。成功すれば真を返します。失敗した場合の skk の内容は不定です。
        static bool convertJsonToSkkCore(SimpleStringForHairy &json, SimpleStringForHairy &skk, SimpleStringForHairy &candidate_buffer, SimpleStringForHairy &iconv_buffer)
        {
                json.setReadIndex(0);
                skk.reset();
                candidate_buffer.reset();
                static const char protocol[] = "1/";
                if (!skk.isAppendSize(sizeof(protocol)))
                {
                        return false;
                }
                skk.appendFast(protocol);
                bool skk_write_flag = false;
                int kakko_counter = 0;
                bool double_quote_flag = false;
                bool concat_flag = false;
                int candidate_counter = 0;
                DEBUG_PRINTF("json:\"%s\"\n", json.getBuffer());
                for (;;)
                {
                        char c;
                        if (!json.readCharacterAndAddReadIndex(c))
                        {
                                return false;
                        }
                        if (c == 0)
                        {
                                if (skk_write_flag && !double_quote_flag)
                                {
                                        break;
                                }
                                else
                                {
                                        return false;
                                }
                        }
                        else if (!skk_write_flag)
                        {
                                if (c == '[')
                                {
                                        ++kakko_counter;
                                        if (kakko_counter >= 3)
                                        {
                                                skk_write_flag = true;
                                        }
                                }
                        }
                        else if (!double_quote_flag)
                        {
                                if (c == '"')
                                {
                                        double_quote_flag = true;
                                        concat_flag = false;
                                        candidate_buffer.reset();
                                }
                                else if (c == ']')
                                {
                                        if (skk_write_flag && !double_quote_flag)
                                        {
                                                break;
                                        }
                                        else
                                        {
                                                return false;
                                        }
                                }
                        }
                        else if (c == '\\')
                        {
                                if (skk_write_flag)
                                {
                                        char force;
                                        if (!json.readCharacterAndAddReadIndex(force))
                                        {
                                                return false;
                                        }
                                        if (!skk.isAppendSize(2))
                                        {
                                                return false;
                                        }
                                        skk.appendFast(c);
                                        skk.appendFast(force);
                                }
                        }
                        else if (c == '"')
                        {
                                double_quote_flag = false;
                                DEBUG_PRINTF("candidate_buffer:\"%s\"\n", candidate_buffer.getBuffer());
                                iconv_buffer.reset();
                                if (!convertIconv(candidate_buffer, iconv_buffer, "utf-8", "euc-jp"))
                                {
                                        // convert fail skip
                                }
                                else
                                {
                                        if (!isAppendGoogleJapanesInput(iconv_buffer))
                                        {
                                                // 追加すべきではない(ひらがなカタカナのみな)ので skip
                                                DEBUG_PRINTF("isAppendGoogleJapanesInput() skip=%s\n", iconv_buffer.getBuffer());
                                        }
                                        else
                                        {
                                                if (concat_flag)
                                                {
                                                        static const char concat[] = "(concat \"";
                                                        if (!skk.isAppendSize(sizeof(concat)))
                                                        {
                                                                return false;
                                                        }
                                                        skk.appendFast(concat);
                                                }
                                                if (!skk.isAppend(iconv_buffer))
                                                {
                                                        return false;
                                                }
                                                skk.appendFast(iconv_buffer);
                                                if (concat_flag)
                                                {
                                                        static const char kokka[] = "\")";
                                                        if (!skk.isAppendSize(sizeof(kokka)))
                                                        {
                                                                return false;
                                                        }
                                                        skk.appendFast(kokka);
                                                }
                                                if (!skk.append('/'))
                                                {
                                                        return false;
                                                }
                                                ++candidate_counter;
                                        }
                                }
                        }
                        else
                        {
                                DEBUG_ASSERT(double_quote_flag);
                                // \057 と \073 を含む candidate concat に。
                                if ((c == '/') || (c == ';'))
                                {
                                        concat_flag = true;
                                        if (!candidate_buffer.appendOctet(c))
                                        {
                                                return false;
                                        }
                                }
                                else
                                {
                                        if (!candidate_buffer.append(c))
                                        {
                                                return false;
                                        }
                                }
                        }
                }
                if (!skk.append('\n'))
                {
                        return false;
                }
                if (candidate_counter == 0)
                {
                        return false;
                }
                return true;
        }

        // http_receive_buffer_ 内の JSON データの必要な要素リスト (候補の最初のリスト) のみを SKK 形式に変換して skk_output_buffer_ に出力します。失敗した場合に偽を返します。失敗した場合の skk_output_buffer_ の内容は不定です。
        bool convertJsonToSkk()
        {
                if (!checkStatusAndSkipHttpHeader(http_receive_buffer_))
                {
                        return false;
                }
                if (!convertEscape4ToUtf8(http_receive_buffer_, work_a_buffer_))
                {
                        return false;
                }
                // この時点で http_receive_buffer_ は破壊しても問題ないので、 candidate buffer として再利用。 http_send_buffer_ も同様に iconv buffer として再利用。
                if (!convertJsonToSkkCore(work_a_buffer_,
                                          skk_output_buffer_,
                                          http_receive_buffer_, // candidate buffer
                                          http_send_buffer_))   // iconv buffer
                {
                        DEBUG_PRINTF("FAILED convertJsonToSkkCore()\n");
                        return false;
                }
                DEBUG_PRINTF("success convertJsonToSkkCore()\n");
                return true;
        }

        // json から skk に必要な最初の候補リストを SKK の「変換文字列」形式で書き込みます。成功すれば真を返します。失敗した場合の skk の内容は不定です。
        static bool convertXMLToSkkCore(SimpleStringForHairy &xml, SimpleStringForHairy &skk, SimpleStringForHairy &candidate_buffer, SimpleStringForHairy &iconv_buffer)
        {
                xml.setReadIndex(0);
                skk.reset();
                candidate_buffer.reset();
                static const char protocol[] = "1/";
                if (!skk.isAppendSize(sizeof(protocol)))
                {
                        return false;
                }
                skk.appendFast(protocol);
                bool skk_write_flag = false;
                int skip_double_quote_counter = 0;
                bool double_quote_flag = false;
                bool concat_flag = false;
                int candidate_counter = 0;
                DEBUG_PRINTF("xml:\"%s\"\n", xml.getBuffer());
                for (;;)
                {
                        char c;
                        if (!xml.readCharacterAndAddReadIndex(c))
                        {
                                return false;
                        }
                        if (c == 0)
                        {
                                if (skk_write_flag && !double_quote_flag)
                                {
                                        break;
                                }
                                else
                                {
                                        return false;
                                }
                        }
                        else if (!skk_write_flag)
                        {
                                if (c == '"')
                                {
                                        if (++skip_double_quote_counter >= 2)
                                        {
                                                skk_write_flag = true;
                                        }
                                }
                        }
                        else if (!double_quote_flag)
                        {
                                if (c == '"')
                                {
                                        double_quote_flag = true;
                                        concat_flag = false;
                                        candidate_buffer.reset();
                                }
                        }
                        else if (c == '\\')
                        {
                                if (skk_write_flag)
                                {
                                        char force;
                                        if (!xml.readCharacterAndAddReadIndex(force))
                                        {
                                                return false;
                                        }
                                        if (!skk.isAppendSize(2))
                                        {
                                                return false;
                                        }
                                        skk.appendFast(c);
                                        skk.appendFast(force);
                                }
                        }
                        else if (c == '"')
                        {
                                double_quote_flag = false;
                                DEBUG_PRINTF("candidate_buffer:\"%s\"\n", candidate_buffer.getBuffer());
                                iconv_buffer.reset();
                                if (!convertIconv(candidate_buffer, iconv_buffer, "Shift_JIS", "euc-jp"))
                                {
                                        // convert fail skip
                                }
                                else
                                {
                                        if (concat_flag)
                                        {
                                                static const char concat[] = "(concat \"";
                                                if (!skk.isAppendSize(sizeof(concat)))
                                                {
                                                        return false;
                                                }
                                                skk.appendFast(concat);
                                        }
                                        if (!skk.isAppend(iconv_buffer))
                                        {
                                                return false;
                                        }
                                        skk.appendFast(iconv_buffer);
                                        if (concat_flag)
                                        {
                                                static const char kokka[] = "\")";
                                                if (!skk.isAppendSize(sizeof(kokka)))
                                                {
                                                        return false;
                                                }
                                                skk.appendFast(kokka);
                                        }
                                        if (!skk.append('/'))
                                        {
                                                return false;
                                        }
                                        ++candidate_counter;
                                        // とりあえず最初の一個だけ返す
                                        break;
                                }
                        }
                        else
                        {
                                DEBUG_ASSERT(double_quote_flag);
                                // \057 と \073 を含む candidate concat に。
                                if ((c == '/') || (c == ';'))
                                {
                                        concat_flag = true;
                                        if (!candidate_buffer.appendOctet(c))
                                        {
                                                return false;
                                        }
                                }
                                else
                                {
                                        if (!candidate_buffer.append(c))
                                        {
                                                return false;
                                        }
                                }
                        }
                }
                if (!skk.append('\n'))
                {
                        return false;
                }
                if (candidate_counter == 0)
                {
                        return false;
                }
                return true;
        }

        // http_receive_buffer_ 内の XML データの必要な要素リスト (候補の最初のリスト) のみを SKK 形式に変換して skk_output_buffer_ に出力します。失敗した場合に偽を返します。失敗した場合の skk_output_buffer_ の内容は不定です。
        bool convertXMLToSkk()
        {
                if (!checkStatusAndSkipHttpHeader(http_receive_buffer_))
                {
                        return false;
                }
                if (!convertEscape4ToUtf8(http_receive_buffer_, work_a_buffer_))
                {
                        return false;
                }
                // この時点で http_receive_buffer_ は破壊しても問題ないので、 candidate buffer として再利用。 http_send_buffer_ も同様に iconv buffer として再利用。
                if (!convertXMLToSkkCore(work_a_buffer_,
                                         skk_output_buffer_,
                                         http_receive_buffer_, // candidate buffer
                                         http_send_buffer_))   // iconv buffer
                {
                        return false;
                }
                return true;
        }

private:
        SkkSocket skk_socket_;
        SimpleStringForHairy work_a_buffer_;
        SimpleStringForHairy work_b_buffer_;
        SimpleStringForHairy midashi_buffer_;
        SimpleStringForHairy http_send_buffer_;
        SimpleStringForHairy http_receive_buffer_;
        SimpleStringForHairy skk_output_buffer_;
        SimpleStringForHairy http_send_string_;
        SimpleCache cache_;
        bool is_https_;
        bool is_ipv6_;
};
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT

class LocalSkkDictionary :
        public SkkDictionary
{
        LocalSkkDictionary(LocalSkkDictionary &source);
        LocalSkkDictionary& operator=(LocalSkkDictionary &source);

public:
        ~LocalSkkDictionary()
        {
        }
        LocalSkkDictionary() :
                SkkDictionary(),
                google_japanese_input_flag_(false),
                google_suggest_flag_(false),
                is_https_(true)
        {
        }

        bool isGoogleJapaneseInput() const
        {
                return google_japanese_input_flag_;
        }
        void setGoogleJapaneseInputFlag(bool flag, bool is_https)
        {
                google_japanese_input_flag_ = flag;
                is_https_ = is_https;
        }

        bool isGoogleSuggest() const
        {
                return google_suggest_flag_;
        }
        void setGoogleSuggestFlag(bool flag, bool is_https)
        {
                google_suggest_flag_ = flag;
                is_https_ = is_https;
        }

        bool isHttps() const
        {
                return is_https_;
        }

private:
        bool google_japanese_input_flag_;
        bool google_suggest_flag_;
        bool is_https_;
};

class LocalSkkServer :
        private SkkServer
{
        LocalSkkServer(LocalSkkServer &source);
        LocalSkkServer& operator=(LocalSkkServer &source);

public:
        enum GoogleJapaneseInputType
        {
                GOOGLE_JAPANESE_INPUT_TYPE_DISABLE,
                GOOGLE_JAPANESE_INPUT_TYPE_NOTFOUND,
                GOOGLE_JAPANESE_INPUT_TYPE_NOTFOUND_SUGGEST_INPUT,
                GOOGLE_JAPANESE_INPUT_TYPE_NOTFOUND_INPUT_SUGGEST,
                GOOGLE_JAPANESE_INPUT_TYPE_DICTIONARY
        };

        virtual ~LocalSkkServer()
        {
        }

        LocalSkkServer(int port = 1178, int log_level = 0) :
                SkkServer("yaskkserv_" SERVER_IDENTIFIER, port, log_level),

                skk_dictionary_(0),
                dictionary_filename_table_(0),

#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                google_japanese_input_(),
                google_japanese_input_type_(GOOGLE_JAPANESE_INPUT_TYPE_DISABLE),
                google_japanese_input_timeout_(2.5f),
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                google_suggest_(),
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                google_suggest_flag_(false),
                google_cache_file_(0),
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                skk_dictionary_length_(0),
                max_connection_(0),
                listen_queue_(0),
                server_completion_midasi_length_(0),
                server_completion_midasi_string_size_(0),
                server_completion_test_(1),
                server_completion_test_protocol_('4'),

                dictionary_check_update_flag_(false),
                no_daemonize_flag_(false),
                use_http_flag_(false)
        {
        }

        void initialize(LocalSkkDictionary *skk_dictionary,
                        const char * const *dictionary_filename_table,
                        int skk_dictionary_length,
                        int max_connection,
                        int listen_queue,
                        int server_completion_midasi_length,
                        int server_completion_midasi_string_size,
                        int server_completion_test,
                        int google_cache_entries,
                        const char *google_cache_file,
                        bool dictionary_check_update_flag,
                        bool no_daemonize_flag,
                        bool use_http_flag,
                        bool use_ipv6_flag)
        {
#ifndef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                (void)use_ipv6_flag;
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                skk_dictionary_ = skk_dictionary;
                dictionary_filename_table_ = dictionary_filename_table;

                skk_dictionary_length_ = skk_dictionary_length;
                max_connection_ = max_connection;
                listen_queue_ = listen_queue;
                server_completion_midasi_length_ = server_completion_midasi_length;
                server_completion_midasi_string_size_ = server_completion_midasi_string_size;
                server_completion_test_ = server_completion_test;

                dictionary_check_update_flag_ = dictionary_check_update_flag;
                no_daemonize_flag_ = no_daemonize_flag;
                use_http_flag_ = use_http_flag;
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                google_japanese_input_.createCache(google_cache_entries);
                google_japanese_input_.setIpv6Flag(use_ipv6_flag);
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                google_suggest_.createCache(google_cache_entries);
                google_suggest_.setIpv6Flag(use_ipv6_flag);
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                google_cache_file_ = google_cache_file;

                if (use_http_flag)
                {
                        google_japanese_input_.setHttpsFlag(false);
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                        google_suggest_.setHttpsFlag(false);
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                }

                // google japanese input と google suggest は、それぞれ 1 つしか指定できないことに注意。
                for (int i = 0; i != skk_dictionary_length; ++i)
                {
                        if ((skk_dictionary_ + i)->isGoogleJapaneseInput())
                        {
                                google_japanese_input_.setHttpsFlag((skk_dictionary_ + i)->isHttps());
                                syslog_.printf(1,
                                               SkkSyslog::LEVEL_INFO,
                                               "google japanese input dictionary found (index=%d)",
                                               i);
                        }
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                        else if ((skk_dictionary_ + i)->isGoogleSuggest())
                        {
                                google_suggest_.setHttpsFlag((skk_dictionary_ + i)->isHttps());
                                syslog_.printf(1,
                                               SkkSyslog::LEVEL_INFO,
                                               "google suggest dictionary found (index=%d)",
                                               i);
                        }
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                }

                syslog_.printf(1,
                               SkkSyslog::LEVEL_INFO,
                               "google japanese input (protocol=%s)",
                               google_japanese_input_.isHttps() ? "https" : "http");
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                syslog_.printf(1,
                               SkkSyslog::LEVEL_INFO,
                               "google suggest (protocol=%s)",
                               google_suggest_.isHttps() ? "https" : "http");
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST

#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        }

#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        void setGoogleJapaneseInputParameter(GoogleJapaneseInputType type, float timeout)
        {
                google_japanese_input_type_ = type;
                google_japanese_input_timeout_ = timeout;
        }
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        void setGoogleSuggestParameter(bool flag)
        {
                google_suggest_flag_ = flag;
        }
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT

        bool mainLoop()
        {
                bool result = main_loop_initialize(max_connection_, listen_queue_);
                if (result)
                {
#ifndef YASKKSERV_DEBUG
                        if (!no_daemonize_flag_)
                        {
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
                        }
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
        bool local_main_loop_loop(fd_set &fd_set_read);
        void local_main_loop_sighup();
        bool local_main_loop();

#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        bool local_main_loop_1_google_japanese_input(int work_index);
        bool local_main_loop_1_google_suggest(int work_index);
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        bool local_main_loop_1_search_single_dictionary(int work_index);
        void main_loop_get_plural_dictionary_information(int work_index,
                                                         LocalSkkDictionary *skk_dictionary,
                                                         int skk_dictionary_length,
                                                         int &found_times,
                                                         int &candidate_length,
                                                         int &total_henkanmojiretsu_size,
                                                         const char *&google_japanese_input_candidates,
                                                         const char *&google_suggest_candidates);
        static bool add_candidates_string(SkkUtility::Hash<SkkUtility::HASH_TYPE_CANDIDATE> &hash,
                                          SkkSimpleString &candidates_string,
                                          const char *add_c_string);
        bool local_main_loop_1_search_plural_dictionary(int work_index,
                                                        int candidate_length,
                                                        int total_henkanmojiretsu_size,
                                                        const char *google_japanese_input_candidates,
                                                        const char *google_suggest_candidates);
        bool local_main_loop_1_search(int work_index);
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        bool local_main_loop_1_notfound(int work_index);
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        bool local_main_loop_1_notfound_first_second(int work_index, const char *&first, const char *&second);
        bool local_main_loop_1_notfound_suggest_input(int work_index);
        bool local_main_loop_1_notfound_input_suggest(int work_index);
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
/// バッファをリセットすべきならば真を返します。
        bool local_main_loop_1(int work_index, int recv_result);
        bool local_main_loop_4_search_core(int work_index,
                                           int recv_result,
                                           SkkUtility::Hash<SkkUtility::HASH_TYPE_CANDIDATE> *hash,
                                           SkkSimpleString &string);
        bool local_main_loop_4_search(int work_index, int recv_result);
/// バッファをリセットすべきならば真を返します。
        bool local_main_loop_4(int work_index, int recv_result);

        void print_syslog_cache_information();
        void save_cache_file(const char *filename);
        void load_cache_file_read_all(const char header[64], int fd, int file_size, int bitflags);
        void load_cache_file(const char *filename);

private:
        LocalSkkDictionary *skk_dictionary_;
        const char * const *dictionary_filename_table_;

#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        GoogleJapaneseInput google_japanese_input_;
        GoogleJapaneseInputType google_japanese_input_type_;
        float google_japanese_input_timeout_;
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        GoogleJapaneseInput google_suggest_;
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        bool google_suggest_flag_;
        const char *google_cache_file_;
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        int skk_dictionary_length_;
        int max_connection_;
        int listen_queue_;
        int server_completion_midasi_length_;
        int server_completion_midasi_string_size_;
        int server_completion_test_;
        char server_completion_test_protocol_;

        bool dictionary_check_update_flag_;
        bool no_daemonize_flag_;
        bool use_http_flag_;
};

#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
bool LocalSkkServer::local_main_loop_1_google_japanese_input(int work_index)
{
        bool result = false;
        const char *candidates = google_japanese_input_.getSkkCandidatesEuc((work_ + work_index)->read_buffer + 1, google_japanese_input_timeout_);
        if (candidates)
        {
                if (!send((work_ + work_index)->file_descriptor, candidates, GoogleJapaneseInput::getByteSize(candidates)))
                {
                        (work_ + work_index)->closeAndReset();
                }
                result = true;
        }
        return result;
}
bool LocalSkkServer::local_main_loop_1_google_suggest(int work_index)
{
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        bool result = false;
        const char *candidates = google_suggest_.getSkkCandidatesEucSuggest((work_ + work_index)->read_buffer + 1, google_japanese_input_timeout_);
        if (candidates)
        {
                if (!send((work_ + work_index)->file_descriptor, candidates, GoogleJapaneseInput::getByteSize(candidates)))
                {
                        (work_ + work_index)->closeAndReset();
                }
                result = true;
        }
        return result;
#else  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        (void)work_index;
        return false;
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
}
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT

bool LocalSkkServer::local_main_loop_1_search_single_dictionary(int work_index)
{
        bool result = false;
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        if (skk_dictionary_->isGoogleJapaneseInput())
        {
                const char *candidates = google_japanese_input_.getSkkCandidatesEuc((work_ + work_index)->read_buffer + 1,
                                                                                    google_japanese_input_timeout_);
                if (candidates)
                {
                        if (!send((work_ + work_index)->file_descriptor, candidates, GoogleJapaneseInput::getByteSize(candidates)))
                        {
                                (work_ + work_index)->closeAndReset();
                        }
                        result = true;
                }
        }
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        else if (skk_dictionary_->isGoogleSuggest() && google_suggest_flag_)
        {
                const char *candidates = google_suggest_.getSkkCandidatesEucSuggest((work_ + work_index)->read_buffer + 1,
                                                                                    google_japanese_input_timeout_);
                if (candidates)
                {
                        if (!send((work_ + work_index)->file_descriptor, candidates, GoogleJapaneseInput::getByteSize(candidates)))
                        {
                                (work_ + work_index)->closeAndReset();
                        }
                        result = true;
                }
        }
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        else
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        {
                if (skk_dictionary_->search((work_ + work_index)->read_buffer + 1))
                {
                        main_loop_send_found(work_index, skk_dictionary_);
                        result = true;
                }
        }
        return result;
}

void LocalSkkServer::main_loop_get_plural_dictionary_information(int work_index,
                                                                 LocalSkkDictionary *skk_dictionary,
                                                                 int skk_dictionary_length,
                                                                 int &found_times,
                                                                 int &candidate_length,
                                                                 int &total_henkanmojiretsu_size,
                                                                 const char *&google_japanese_input_candidates,
                                                                 const char *&google_suggest_candidates)
{
        found_times = 0;
        candidate_length = 0;
        total_henkanmojiretsu_size = 0;
        google_japanese_input_candidates = 0;
        google_suggest_candidates = 0;
        for (int h = 0; h != skk_dictionary_length; ++h)
        {
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                if ((skk_dictionary + h)->isGoogleJapaneseInput())
                {
                        google_japanese_input_candidates = google_japanese_input_.getSkkCandidatesEuc((work_ + work_index)->read_buffer + 1, google_japanese_input_timeout_);
                        if (google_japanese_input_candidates)
                        {
                                const int protocol_1_offset = 1;
                                DEBUG_PRINTF("henkanmojiretsu_size=%d\n", SkkUtility::getStringLength(google_japanese_input_candidates + protocol_1_offset));
                                DEBUG_PRINTF("candidate_length=%d\n", SkkUtility::getCandidateLength(google_japanese_input_candidates + protocol_1_offset));
                                total_henkanmojiretsu_size += SkkUtility::getStringLength(google_japanese_input_candidates + protocol_1_offset);
                                candidate_length += SkkUtility::getCandidateLength(google_japanese_input_candidates + protocol_1_offset);
                                ++found_times;
                        }
                }
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                else if ((skk_dictionary + h)->isGoogleSuggest() && google_suggest_flag_)
                {
                        google_suggest_candidates = google_suggest_.getSkkCandidatesEucSuggest((work_ + work_index)->read_buffer + 1, google_japanese_input_timeout_);
                        if (google_suggest_candidates)
                        {
                                const int protocol_1_offset = 1;
                                DEBUG_PRINTF("henkanmojiretsu_size=%d\n", SkkUtility::getStringLength(google_suggest_candidates + protocol_1_offset));
                                DEBUG_PRINTF("candidate_length=%d\n", SkkUtility::getCandidateLength(google_suggest_candidates + protocol_1_offset));
                                total_henkanmojiretsu_size += SkkUtility::getStringLength(google_suggest_candidates + protocol_1_offset);
                                candidate_length += SkkUtility::getCandidateLength(google_suggest_candidates + protocol_1_offset);
                                ++found_times;
                        }
                }
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                else
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                {
                        if ((skk_dictionary + h)->search((work_ + work_index)->read_buffer + 1))
                        {
                                const int cr_size = 1;
                                total_henkanmojiretsu_size += (skk_dictionary + h)->getHenkanmojiretsuSize() + cr_size;
                                candidate_length += SkkUtility::getCandidateLength((skk_dictionary + h)->getHenkanmojiretsuPointer());
                                ++found_times;
                        }
                }
        }
}

#pragma GCC diagnostic push
#ifndef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
bool LocalSkkServer::add_candidates_string(SkkUtility::Hash<SkkUtility::HASH_TYPE_CANDIDATE> &hash,
                                           SkkSimpleString &candidates_string,
                                           const char *add_c_string)
{
        DEBUG_ASSERT_POINTER(add_c_string);
        int length = SkkUtility::getCandidateLength(add_c_string);
        for (int i = 0; i != length; ++i)
        {
                const char *start;
                int size;
                if (SkkUtility::getCandidateInformation(add_c_string, i, start, size))
                {
                        if (!hash.contain(start, size))
                        {
                                hash.add(start, size);
                                const int tail_slash_size = 1;
                                if (!candidates_string.append(start, size + tail_slash_size))
                                {
                                        return false;
                                }
                        }
                }
                else
                {
                        return false;
                }
        }
        return true;
}

bool LocalSkkServer::local_main_loop_1_search_plural_dictionary(int work_index,
                                                                int candidate_length,
                                                                int total_henkanmojiretsu_size,
                                                                const char *google_japanese_input_candidates,
                                                                const char *google_suggest_candidates)
{
#ifndef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        (void)google_suggest_candidates;
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
// candidate_length を最適な hash_table_length に変換します。
        int hash_table_length = SkkUtility::Hash<SkkUtility::HASH_TYPE_CANDIDATE>::getPrimeHashTableLength(candidate_length);
        if (hash_table_length == 0)
        {
                return false;
        }

// 見付かった複数の辞書の candidate を分解、合成します。

// 文字列は string に追加されます。重複チェックは hash でおこないます。
// 変換文字列サイズにマージンを加えたものを temporary_buffer_size とし
// ます。
        int temporary_buffer_size = total_henkanmojiretsu_size;
        {
                const int protocol_header_margin_size = 8;
                const int terminator_size = 1;
                const int margin_size = 8;
                temporary_buffer_size += protocol_header_margin_size;
                temporary_buffer_size += terminator_size;
                temporary_buffer_size += margin_size;
        }

        SkkSimpleString string(temporary_buffer_size);
        SkkUtility::Hash<SkkUtility::HASH_TYPE_CANDIDATE> hash(hash_table_length);

// protocol header + first slash
        string.appendFast("1/");

        for (int h = 0; h != skk_dictionary_length_; ++h)
        {
                const char *p = 0;
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                if ((skk_dictionary_ + h)->isGoogleJapaneseInput() && google_japanese_input_candidates)
                {
                        const int protocol_1_offset = 1;
                        p = google_japanese_input_candidates + protocol_1_offset;
                }
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                else if ((skk_dictionary_ + h)->isGoogleSuggest() && google_suggest_candidates && google_suggest_flag_)
                {
                        const int protocol_1_offset = 1;
                        p = google_suggest_candidates + protocol_1_offset;
                }
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                else if ((skk_dictionary_ + h)->isSuccess())
                {
                        p = (skk_dictionary_ + h)->getHenkanmojiretsuPointer();
                }
#else  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                if ((skk_dictionary_ + h)->isSuccess())
                {
                        p = (skk_dictionary_ + h)->getHenkanmojiretsuPointer();
                }
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                if (p)
                {
                        if (!add_candidates_string(hash, string, p))
                        {
                                return false;
                        }
                }
        }

        if (!string.append('\n'))
        {
                return false;
        }

        if (!send((work_ + work_index)->file_descriptor, string.getBuffer(), string.getSize()))
        {
                (work_ + work_index)->closeAndReset();
        }
        return true;
}
#pragma GCC diagnostic pop

bool LocalSkkServer::local_main_loop_1_search(int work_index)
{
//
// 探索処理は、おおまかに以下のように分けられます。
//
//    - 指定された辞書が 1 つの場合
//
//    - 指定された辞書が複数の場合
//
//        = エントリが 1 つの辞書でしか見付からなかった場合
//
//        = エントリが複数の辞書で見付かった場合
//
        if (skk_dictionary_length_ == 1)
        {
// 指定された辞書が 1 つ。
                return local_main_loop_1_search_single_dictionary(work_index);
        }

// 指定された辞書が複数。
        int found_times;
        int candidate_length;
        int total_henkanmojiretsu_size;
        const char *google_japanese_input_candidates;
        const char *google_suggest_candidates;
        main_loop_get_plural_dictionary_information(work_index,
                                                    skk_dictionary_,
                                                    skk_dictionary_length_,
                                                    found_times,
                                                    candidate_length,
                                                    total_henkanmojiretsu_size,
                                                    google_japanese_input_candidates,
                                                    google_suggest_candidates);
        if (found_times == 0)
        {
// 見付からなかった。
                return false;
        }

        if (found_times == 1)
        {
// エントリが 1 つの辞書でしか見付からなかった。
                for (int h = 0; h != skk_dictionary_length_; ++h)
                {
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                        if ((skk_dictionary_ + h)->isGoogleJapaneseInput() && google_japanese_input_candidates)
                        {
                                if (!send((work_ + work_index)->file_descriptor, google_japanese_input_candidates, GoogleJapaneseInput::getByteSize(google_japanese_input_candidates)))
                                {
                                        (work_ + work_index)->closeAndReset();
                                }
                        }
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                        else if ((skk_dictionary_ + h)->isGoogleSuggest() && google_suggest_candidates && google_suggest_flag_)
                        {
                                if (!send((work_ + work_index)->file_descriptor, google_suggest_candidates, GoogleJapaneseInput::getByteSize(google_suggest_candidates)))
                                {
                                        (work_ + work_index)->closeAndReset();
                                }
                        }
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                        else if ((skk_dictionary_ + h)->isSuccess())
                        {
                                main_loop_send_found(work_index, skk_dictionary_ + h);
                        }
#else  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                        if ((skk_dictionary_ + h)->isSuccess())
                        {
                                main_loop_send_found(work_index, skk_dictionary_ + h);
                        }
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                }
                return true;
        }

// エントリが複数の辞書で見付かった。
        return local_main_loop_1_search_plural_dictionary(work_index, candidate_length, total_henkanmojiretsu_size, google_japanese_input_candidates, google_suggest_candidates);
}

#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
bool LocalSkkServer::local_main_loop_1_notfound(int work_index)
{
        bool found_flag = false;
        if (google_suggest_flag_)
        {
                if (google_japanese_input_type_ == GOOGLE_JAPANESE_INPUT_TYPE_NOTFOUND)
                {
                        found_flag = local_main_loop_1_google_suggest(work_index);
                }
        }
        else
        {
                if (google_japanese_input_type_ == GOOGLE_JAPANESE_INPUT_TYPE_NOTFOUND)
                {
                        found_flag = local_main_loop_1_google_japanese_input(work_index);
                }
        }
        return found_flag;
}

#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
bool LocalSkkServer::local_main_loop_1_notfound_first_second(int work_index, const char *&first, const char *&second)
{
        int found_times = 0;
        int candidate_length = 0;
        int total_henkanmojiretsu_size = 0;
        const int protocol_1_offset = 1;
        if (first)
        {
                total_henkanmojiretsu_size += SkkUtility::getStringLength(first + protocol_1_offset);
                candidate_length += SkkUtility::getCandidateLength(first + protocol_1_offset);
                ++found_times;
        }
        if (second)
        {
                total_henkanmojiretsu_size += SkkUtility::getStringLength(second + protocol_1_offset);
                candidate_length += SkkUtility::getCandidateLength(second + protocol_1_offset);
                ++found_times;
        }
        if (found_times == 0)
        {
                return false;
        }
        else if (found_times == 1)
        {
                bool result = false;
                const char *candidates = first ? first : second;
                DEBUG_ASSERT_POINTER(candidates);
                if (candidates)
                {
                        if (!send((work_ + work_index)->file_descriptor, candidates, GoogleJapaneseInput::getByteSize(candidates)))
                        {
                                (work_ + work_index)->closeAndReset();
                        }
                        result = true;
                }
                return result;
        }
        else
        {
                int temporary_buffer_size = total_henkanmojiretsu_size;
                {
                        const int protocol_header_margin_size = 8;
                        const int terminator_size = 1;
                        const int margin_size = 8;
                        temporary_buffer_size += protocol_header_margin_size;
                        temporary_buffer_size += terminator_size;
                        temporary_buffer_size += margin_size;
                }
                SkkSimpleString string(temporary_buffer_size);
                string.appendFast("1/");
// candidate_length を最適な hash_table_length に変換します。
                int hash_table_length = SkkUtility::Hash<SkkUtility::HASH_TYPE_CANDIDATE>::getPrimeHashTableLength(candidate_length);
                if (hash_table_length == 0)
                {
                        return false;
                }
                SkkUtility::Hash<SkkUtility::HASH_TYPE_CANDIDATE> hash(hash_table_length);
                if (!add_candidates_string(hash, string, first + protocol_1_offset))
                {
                        return false;
                }
                if (!add_candidates_string(hash, string, second + protocol_1_offset))
                {
                        return false;
                }
                if (!string.append('\n'))
                {
                        return false;
                }
                if (!send((work_ + work_index)->file_descriptor, string.getBuffer(), string.getSize()))
                {
                        (work_ + work_index)->closeAndReset();
                }
        }
        return true;
}

bool LocalSkkServer::local_main_loop_1_notfound_suggest_input(int work_index)
{
        const char *google_suggest_candidates = google_suggest_.getSkkCandidatesEucSuggest((work_ + work_index)->read_buffer + 1, google_japanese_input_timeout_);
        const char *google_japanese_input_candidates = google_japanese_input_.getSkkCandidatesEuc((work_ + work_index)->read_buffer + 1, google_japanese_input_timeout_);
        return local_main_loop_1_notfound_first_second(work_index, google_suggest_candidates, google_japanese_input_candidates);
}

bool LocalSkkServer::local_main_loop_1_notfound_input_suggest(int work_index)
{
        const char *google_japanese_input_candidates = google_japanese_input_.getSkkCandidatesEuc((work_ + work_index)->read_buffer + 1, google_japanese_input_timeout_);
        const char *google_suggest_candidates = google_suggest_.getSkkCandidatesEucSuggest((work_ + work_index)->read_buffer + 1, google_japanese_input_timeout_);
        return local_main_loop_1_notfound_first_second(work_index, google_japanese_input_candidates, google_suggest_candidates);
}
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT

bool LocalSkkServer::local_main_loop_1(int work_index, int recv_result)
{
        bool illegal_protocol_flag;
        bool result = main_loop_check_buffer(work_index, recv_result, illegal_protocol_flag);
        if (result)
        {
                bool found_flag = false;
                if (!illegal_protocol_flag)
                {
#ifdef YASKKSERV_DEBUG
                        struct timeval time_begin;
                        struct timeval time_end;
                        gettimeofday(&time_begin, 0);
#endif  // YASKKSERV_DEBUG

                        found_flag = local_main_loop_1_search(work_index);
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                        if (!found_flag)
                        {
                                switch (google_japanese_input_type_)
                                {
                                default:
                                        DEBUG_ASSERT(0);
                                        break;
                                case GOOGLE_JAPANESE_INPUT_TYPE_DISABLE: // FALLTHROUGH
                                case GOOGLE_JAPANESE_INPUT_TYPE_DICTIONARY:
                                        break;
                                case GOOGLE_JAPANESE_INPUT_TYPE_NOTFOUND:
                                        found_flag = local_main_loop_1_notfound(work_index);
                                        break;
                                case GOOGLE_JAPANESE_INPUT_TYPE_NOTFOUND_SUGGEST_INPUT:
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                                        found_flag = local_main_loop_1_notfound_suggest_input(work_index);
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                                        break;
                                case GOOGLE_JAPANESE_INPUT_TYPE_NOTFOUND_INPUT_SUGGEST:
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                                        found_flag = local_main_loop_1_notfound_input_suggest(work_index);
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                                        break;
                                }
                        }
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT

#ifdef YASKKSERV_DEBUG
                        gettimeofday(&time_end, 0);
                        unsigned long long usec_begin = static_cast<unsigned long long>(time_begin.tv_sec * 1000 * 1000 + time_begin.tv_usec);
                        unsigned long long usec_end = static_cast<unsigned long long>(time_end.tv_sec * 1000 * 1000 + time_end.tv_usec);
                        DEBUG_PRINTF("ms.=%f\n", static_cast<float>(usec_end - usec_begin) / 1000.0f);
#endif  // YASKKSERV_DEBUG
                }
                if (!found_flag)
                {
                        main_loop_send_not_found(work_index, recv_result);
                }
        }
        return result;
}

bool LocalSkkServer::local_main_loop_4_search_core(int work_index,
                                                   int recv_result,
                                                   SkkUtility::Hash<SkkUtility::HASH_TYPE_CANDIDATE> *hash,
                                                   SkkSimpleString &string)
{
// 見付かった辞書の「見出し」をまとめます。
// 「見出し」は「ひらがなエンコード」されていることに注意が必要です。
        char decode_buffer[SkkUtility::MIDASI_DECODE_HIRAGANA_BUFFER_SIZE];
        switch (server_completion_test_)
        {
        default:
                // FALLTHROUGH
                DEBUG_ASSERT(0);
        case 2:
                // FALLTHROUGH
        case 1:
                string.appendFast("1/");
                break;

        case 3:
                string.appendFast("1 ");
                break;

        case 4:
                if (server_completion_test_protocol_ == '4')
                {
                        string.appendFast("1/");
                }
                else
                {
                        string.appendFast("1 ");
                }
                break;
        }

        int add_counter = 0;
        for (int h = 0; h != skk_dictionary_length_; ++h)
        {
                if (!(skk_dictionary_ + h)->isGoogleJapaneseInput() &&
                    !(skk_dictionary_ + h)->isGoogleSuggest() &&
                    (skk_dictionary_ + h)->isSuccess())
                {
                        bool found_flag = false;
                        do
                        {
                                const char *p = (skk_dictionary_ + h)->getMidasiPointer();
// DecodeHiragana() 後はターミネータが存在しないことに注意が必要です。
// size もターミネータを含まないサイズです。
                                int size = SkkUtility::decodeHiragana(p, decode_buffer, sizeof(decode_buffer));
                                if (size == 0)
                                {
                                        const int raw_code = 1; // \1 の分
                                        p += raw_code;
                                        size = (skk_dictionary_ + h)->getMidasiSize() - raw_code;
                                }
                                else
                                {
                                        p = decode_buffer;
                                }

                                if (SkkSimpleString::startWith(p,
                                                               (work_ + work_index)->read_buffer + 1,
                                                               size,
                                                               (work_ + work_index)->read_process_index + recv_result - 1))
                                {
                                        found_flag = true;
                                }
                                else
                                {
                                        if (found_flag)
                                        {
                                                break;
                                        }
                                        else
                                        {
                                                continue;
                                        }
                                }

                                if (SkkUtility::isOkuriNasiOrAbbrev(p, size))
                                {
                                        if ((hash == 0) || !hash->contain(p, size))
                                        {
                                                if ((server_completion_test_ == 2) && SkkSimpleString::search(p, '/', size))
                                                {
                                                        // ignore slash
                                                }
                                                else
                                                {
                                                        const int separator_size = 1;
                                                        if (!string.isAppendSize(size + separator_size))
                                                        {
                                                                return false;
                                                        }
                                                        const char *current = string.getCurrentBuffer();
                                                        string.append(p, size);
                                                        switch (server_completion_test_)
                                                        {
                                                        default:
                                                                DEBUG_ASSERT(0);
                                                                // FALLTHROUGH
                                                        case 1:
                                                                string.appendFast('/'); // + separator_size
                                                                break;
                                                        case 3:
                                                                string.appendFast(' '); // + separator_size
                                                                break;
                                                        case 4:
                                                                if (server_completion_test_protocol_ == '4')
                                                                {
                                                                        string.appendFast('/'); // + separator_size
                                                                }
                                                                else
                                                                {
                                                                        string.appendFast(' '); // + separator_size
                                                                }
                                                                break;
                                                        }
// プロトコル "4" ではリードバッファが動的に書き換えられるため、直接リー
// ドバッファを参照してはなりません。この実装では string をバッファとし
// て利用しています。
                                                        if (hash)
                                                        {
                                                                if (!hash->add(current, size))
                                                                {
                                                                        return false;
                                                                }
                                                        }

                                                        if (++add_counter >= server_completion_midasi_length_)
                                                        {
                                                                return false;
                                                        }
                                                }
                                        }
                                }
                        }
                        while ((skk_dictionary_ + h)->searchNextEntry());
                }
        }

#ifdef YASKKSERV_DEBUG
        if (hash)
        {
                int counter;
                int offset_max;
                double average;
                hash->getDebugBuildInformation(counter, offset_max, average);
                DEBUG_PRINTF("counter=%d\n"
                             "offset_max=%d\n"
                             "average=%f\n"
                             ,
                             counter,
                             offset_max,
                             average);
        }
#endif  // YASKKSERV_DEBUG

        if (add_counter == 0)
        {
                return false;
        }

        if (!string.append('\n'))
        {
                return false;
        }

        return true;
}

bool LocalSkkServer::local_main_loop_4_search(int work_index, int recv_result)
{
//
// 1. 「見出し」が「送りあり」ならば即座に偽を返します。
//
// 2. 「見出し」を探索します。
//
// 3. 「見出し」が見付かった場合、そのエントリから SearchNextEntry() を
//    開始します。
//
// 4. 「見出し」が見付からなかった場合、そのエントリが含まれてるであろ
//    うブロックの先頭から SearchNextEntry() を開始します。
//
        if (SkkUtility::isOkuriAri((work_ + work_index)->read_buffer + 1, recv_result - 1))
        {
                return false;
        }

        int found_times = 0;
        for (int h = 0; h != skk_dictionary_length_; ++h)
        {
                if (!(skk_dictionary_ + h)->isGoogleJapaneseInput() &&
                    !(skk_dictionary_ + h)->isGoogleSuggest())
                {
                        if ((skk_dictionary_ + h)->search((work_ + work_index)->read_buffer + 1) ||
                            (skk_dictionary_ + h)->searchForFirstCharacter((work_ + work_index)->read_buffer + 1))
                        {
                                ++found_times;
                        }
                }
        }

        if (found_times == 0)
        {
// 見付からなかった。
                return false;
        }

        SkkSimpleString string(server_completion_midasi_string_size_);
        {
                SkkUtility::Hash<SkkUtility::HASH_TYPE_CANDIDATE> *hash = 0;
                if (found_times > 1)
                {
// 重複チェックが必要なのは複数の辞書を操作する場合だけです。
                        int prime_length = SkkUtility::Hash<SkkUtility::HASH_TYPE_CANDIDATE>::getPrimeHashTableLength(server_completion_midasi_length_);
                        hash = new SkkUtility::Hash<SkkUtility::HASH_TYPE_CANDIDATE>(prime_length);
                }
                bool core_result = local_main_loop_4_search_core(work_index, recv_result, hash, string);
                delete hash;

                if (!core_result)
                {
                        syslog_.printf(1, SkkSyslog::LEVEL_WARNING, "server completion failed");
                        return false;
                }
        }

        if (!send((work_ + work_index)->file_descriptor, string.getBuffer(), string.getSize()))
        {
                (work_ + work_index)->closeAndReset();
        }
        return true;
}

bool LocalSkkServer::local_main_loop_4(int work_index, int recv_result)
{
        bool illegal_protocol_flag;
        bool result = main_loop_check_buffer(work_index, recv_result, illegal_protocol_flag);
        if (result)
        {
                bool found_flag = false;
                if (!illegal_protocol_flag)
                {
                        found_flag = local_main_loop_4_search(work_index, recv_result);
                }
                if (!found_flag)
                {
                        main_loop_send_not_found(work_index, recv_result);
                }
        }
        return result;
}

void LocalSkkServer::print_syslog_cache_information()
{
        int fast_entries;
        int fast_index;
        int large_entries;
        int large_index;
        google_japanese_input_.getCacheBufferInformation(fast_entries, fast_index, large_entries, large_index);
        syslog_.printf(1,
                       SkkSyslog::LEVEL_INFO,
                       "google japanese input:");
        syslog_.printf(1,
                       SkkSyslog::LEVEL_INFO,
                       "    fast entries=%d  fast index=%d    large entries=%d  large index=%d",
                       fast_entries,
                       fast_index,
                       large_entries,
                       large_index);
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        google_suggest_.getCacheBufferInformation(fast_entries, fast_index, large_entries, large_index);
        syslog_.printf(1,
                       SkkSyslog::LEVEL_INFO,
                       "google suggest:");
        syslog_.printf(1,
                       SkkSyslog::LEVEL_INFO,
                       "    fast entries=%d  fast index=%d    large entries=%d  large index=%d",
                       fast_entries,
                       fast_index,
                       large_entries,
                       large_index);
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
}

// ** cache file format
//     0 : "yaskkserv cache" (16bytes)
//    16 : version (4bytes)
//    20 : filesize (4bytes)
//    24 : bitflags (bit0: google japanese input Cache  bit1: google suggest Cache)
//    28 : reserved
// 32-47 : md5 (16bytes md5 や reserved が 0 の状態での全領域の md5)
// 48-63 : reserved
//    64 : google japanese input CacheUnit
//     N : google suggest CacheUnit
// 
// *** CacheUnit format
// 
//  0 fast_entries_
//  4 fast_index_
//  8 large_entries_
// 12 large_index_
//    FastKey
//    FastValue
//    LargeKey
//    LargeValue
// 
// *** load 時の挙動
// 
// バージョンが異なれば失敗。
// 
// md5 が異なれば失敗。
// 
// entries が同じか yaskkserv の entries が大きくなっていればそのまま
// ロード。 index は load した値。不正な値を検出したら、その時点で失敗
// して Cache へは書き込まない。
// 
// yaskkserv の entries が小さくなっていれば読み込める部分だけロードし、
// index は 0 に。
void LocalSkkServer::save_cache_file(const char *filename)
{
        int cache_buffer_size = google_japanese_input_.getCacheBufferSize();
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        cache_buffer_size += google_suggest_.getCacheBufferSize();
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        if (cache_buffer_size <= 0)
        {
                syslog_.printf(1, SkkSyslog::LEVEL_WARNING, "save cache failed (illegal cache_buffer_size)");
                return;
        }
        const int header_size = 64;
        const int margin_footer_size = 64;
        const int file_size = header_size + cache_buffer_size;
        const int buffer_size = file_size + margin_footer_size;
        DEBUG_PRINTF("file_size:%d  buffer_size:%d\n", file_size, buffer_size);
        char *buffer_top = new char[buffer_size];
        DEBUG_PRINTF("buffer_top=%p\n", buffer_top);
        if (buffer_top == 0)
        {
                syslog_.printf(1, SkkSyslog::LEVEL_WARNING, "save cache failed (memory allocation error)");
                return;
        }
        SkkUtility::clearMemory(buffer_top, buffer_size);
        const char header_keyword[16] = "yaskkserv cache";
        const int version = 1;
        int bitflags = 0x1 << 0;
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        bitflags |= 0x1 << 1;
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        SkkUtility::copyMemory(header_keyword, buffer_top + 0, sizeof(header_keyword));
        SkkUtility::copyMemory(&version, buffer_top + 16, sizeof(version));
        SkkUtility::copyMemory(&file_size, buffer_top + 20, sizeof(file_size));
        SkkUtility::copyMemory(&bitflags, buffer_top + 24, sizeof(bitflags));
        char *cache_unit_buffer = buffer_top + 64;
        DEBUG_PRINTF("cache_unit_buffer=%p  buffer_top=%p  diff=%d\n", cache_unit_buffer, buffer_top, cache_unit_buffer - buffer_top);
        cache_unit_buffer = google_japanese_input_.getCacheForSave(cache_unit_buffer);
        DEBUG_PRINTF("cache_unit_buffer=%p  buffer_top=%p  diff=%d\n", cache_unit_buffer, buffer_top, cache_unit_buffer - buffer_top);
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        cache_unit_buffer = google_suggest_.getCacheForSave(cache_unit_buffer);
        DEBUG_PRINTF("cache_unit_buffer=%p  buffer_top=%p  diff=%d\n", cache_unit_buffer, buffer_top, cache_unit_buffer - buffer_top);
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST

        if (cache_unit_buffer - buffer_top != file_size)
        {
                syslog_.printf(1, SkkSyslog::LEVEL_WARNING, "save cache failed (illegal file size)");
                DEBUG_ASSERT(0);
        }
        else
        {
                unsigned char md5[MD5_DIGEST_LENGTH];
                MD5_CTX ctx;
                MD5_Init(&ctx);
                MD5_Update(&ctx, buffer_top, file_size);
                MD5_Final(md5, &ctx);
                SkkUtility::copyMemory(md5, buffer_top + 32, sizeof(md5));
                DEBUG_PRINTF("md5:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                             md5[0], md5[1], md5[2], md5[3],
                             md5[4], md5[5], md5[6], md5[7],
                             md5[8], md5[9], md5[10], md5[11],
                             md5[12], md5[13], md5[14], md5[15]);

                int fd = open(filename, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
                if (fd < 0)
                {
                        syslog_.printf(1, SkkSyslog::LEVEL_WARNING, "save cache failed (file open error)");
                }
                else
                {
                        if (write(fd, buffer_top, file_size) != file_size)
                        {
                                syslog_.printf(1, SkkSyslog::LEVEL_WARNING, "save cache failed (file write error)");
                        }
                        else
                        {
                                print_syslog_cache_information();
                        }
                        close(fd);
                }
        }
        delete[] buffer_top;
}

void LocalSkkServer::load_cache_file_read_all(const char header[64], int fd, int file_size, int bitflags)
{
        unsigned char header_md5[MD5_DIGEST_LENGTH];
        SkkUtility::copyMemory(&header[32], header_md5, sizeof(header_md5));
        const int margin_footer_size = 64;
        const int buffer_size = file_size + margin_footer_size;
        char *buffer_top = new char[buffer_size];
        if (buffer_top == 0)
        {
                syslog_.printf(1, SkkSyslog::LEVEL_WARNING, "load cache failed (memory allocation error size=%d)", buffer_size);
                return;
        }

        lseek(fd, 0, SEEK_SET);
        if (read(fd, buffer_top, file_size) != file_size)
        {
                syslog_.printf(1, SkkSyslog::LEVEL_WARNING, "load cache failed (file read error)");
        }
        else
        {
                unsigned char test_md5[MD5_DIGEST_LENGTH];
                // md5 の領域が 0 で md5 を取るので、クリアしていることに注意。
                SkkUtility::clearMemory(buffer_top + 32, sizeof(header_md5));
                MD5_CTX ctx;
                MD5_Init(&ctx);
                MD5_Update(&ctx, buffer_top, file_size);
                MD5_Final(test_md5, &ctx);
                DEBUG_PRINTF("header_md5:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                             header_md5[0], header_md5[1], header_md5[2], header_md5[3],
                             header_md5[4], header_md5[5], header_md5[6], header_md5[7],
                             header_md5[8], header_md5[9], header_md5[10], header_md5[11],
                             header_md5[12], header_md5[13], header_md5[14], header_md5[15]);
                DEBUG_PRINTF("test_md5:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                             test_md5[0], test_md5[1], test_md5[2], test_md5[3],
                             test_md5[4], test_md5[5], test_md5[6], test_md5[7],
                             test_md5[8], test_md5[9], test_md5[10], test_md5[11],
                             test_md5[12], test_md5[13], test_md5[14], test_md5[15]);

                if (memcmp(header_md5, test_md5, sizeof(header_md5)) != 0)
                {
                        syslog_.printf(1, SkkSyslog::LEVEL_WARNING, "load cache failed (md5 error)");
                }
                else
                {
                        const char *cache_unit_buffer = google_japanese_input_.verifyBufferForLoad(buffer_top + 64);
                        if (cache_unit_buffer == 0)
                        {
                                syslog_.printf(1, SkkSyslog::LEVEL_WARNING, "load cache failed (google japanese input verify error)");
                        }
                        else
                        {
                                DEBUG_PRINTF("bitflags:%08x\n", bitflags);
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                                if (bitflags & (0x1 << 1))
                                {
                                        cache_unit_buffer = google_suggest_.verifyBufferForLoad(cache_unit_buffer);
                                }
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                                if (cache_unit_buffer == 0)
                                {
                                        syslog_.printf(1, SkkSyslog::LEVEL_WARNING, "load cache failed (google suggest verify error)");
                                }
                                else
                                {
                                        cache_unit_buffer = google_japanese_input_.setCacheForLoad(buffer_top + 64);
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                                        if (bitflags & (0x1 << 1))
                                        {
                                                cache_unit_buffer = google_suggest_.setCacheForLoad(cache_unit_buffer);
                                        }
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                                }
                        }
                }
        }

        delete[] buffer_top;
}

void LocalSkkServer::load_cache_file(const char *filename)
{
        char header[64];
        int fd = open(filename, O_RDONLY);
        if (fd < 0)
        {
                syslog_.printf(1, SkkSyslog::LEVEL_WARNING, "load cache failed (file open error)");
                return;
        }
        int read_size = static_cast<int>(read(fd, header, sizeof(header)));
        if (read_size != static_cast<int>(sizeof(header)))
        {
                syslog_.printf(1, SkkSyslog::LEVEL_WARNING, "load cache failed (header read error)");
        }
        else
        {
                const char header_keyword[16] = "yaskkserv cache";
                if (memcmp(header_keyword, &header[0], sizeof(header_keyword)) != 0)
                {
                        syslog_.printf(1, SkkSyslog::LEVEL_WARNING, "load cache failed (file format error)");
                }
                else
                {
                        const int file_size_minimum = sizeof(header) + 256;
                        const int file_size_maximum = 8 * 1024 * 1024;
                        int version;
                        int file_size;
                        int bitflags;
                        SkkUtility::copyMemory(&header[16], &version, 4);
                        SkkUtility::copyMemory(&header[20], &file_size, 4);
                        SkkUtility::copyMemory(&header[24], &bitflags, 4);
                        if ((version != 1) ||
                            (file_size < file_size_minimum) || (file_size > file_size_maximum) ||
                            (bitflags == 0))
                        {
                                syslog_.printf(1, SkkSyslog::LEVEL_WARNING, "load cache failed (file header error)");
                        }
                        else
                        {
                                load_cache_file_read_all(header, fd, file_size, bitflags);
                                print_syslog_cache_information();
                        }
                }
        }
        close(fd);
}

// main_loop_recv() に失敗して error break すべきならば偽を返します。
bool LocalSkkServer::local_main_loop_loop(fd_set &fd_set_read)
{
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
                                        // goto ERROR_BREAK;
                                        return false;
                                }
                        }
                        else
                        {
                                bool buffer_reset_flag;
                                if ((work_ + i)->read_process_index == 0)
                                {
                                        server_completion_test_protocol_ = *(work_ + i)->read_buffer;
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
                                        case '4':
                                                buffer_reset_flag = local_main_loop_4(i, recv_result);
                                                break;
                                        case 'c':
                                                if (server_completion_test_ == 4)
                                                {
                                                        buffer_reset_flag = local_main_loop_4(i, recv_result);
                                                }
                                                else
                                                {
                                                        buffer_reset_flag = true;
                                                        main_loop_illegal_command(i);
                                                }
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
        return true;
}

void LocalSkkServer::local_main_loop_sighup()
{
        char buffer[1024];
        SkkSimpleString string(buffer);
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT

        if (google_cache_file_)
        {
                syslog_.printf(1, SkkSyslog::LEVEL_INFO, "save cache file \"%s\"", google_cache_file_);
                save_cache_file(google_cache_file_);
        }

        const char google_japanese_input[] = "cache status    google japanese Input : ";
        string.append(google_japanese_input, sizeof(google_japanese_input) - 1);
        google_japanese_input_.appendCacheInformation(string);
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        const char google_suggest[] = "    google suggest : ";
        string.append(google_suggest, sizeof(google_suggest) - 1);
        google_suggest_.appendCacheInformation(string);
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        syslog_.printf(1, SkkSyslog::LEVEL_INFO, string.getBuffer());
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        global_sighup_flag = 0;
}

#ifdef YASKKSERV_CONFIG_HAVE_PTHREAD
void *signal_thread(void *args)
{
        (void)args;
        sigset_t sigset;
        sigfillset(&sigset);
        sigdelset(&sigset, SIGTERM);
        sigdelset(&sigset, SIGINT);
        pthread_sigmask(SIG_SETMASK, &sigset, 0);
        for (;;)
        {
                siginfo_t siginfo;
                int result = sigwaitinfo(&sigset, &siginfo);
                // DEBUG_PRINTF("thread result=%d  errno=%d  siginfo.si_signo=%d\n", result, errno, siginfo.si_signo);
                if (result < 0)
                {
                        if (errno != EINTR)
                        {
                                break;
                        }
                }
                else
                {
                        switch (siginfo.si_signo)
                        {
                        default:
                                break;

                        case SIGHUP:
                                global_sighup_flag = 2;
                                break;
                        }
                }
        }
        return 0;
}
#endif  // YASKKSERV_CONFIG_HAVE_PTHREAD

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
void setup_signal()
{
        signal(SIGHUP, SIG_IGN);
}
#pragma GCC diagnostic pop

bool LocalSkkServer::local_main_loop()
{
#ifdef YASKKSERV_CONFIG_HAVE_PTHREAD
        sigset_t sigset;
        sigfillset(&sigset);
        sigdelset(&sigset, SIGTERM);
        sigdelset(&sigset, SIGINT);
        pthread_sigmask(SIG_SETMASK, &sigset, 0);
        pthread_t pthread;
        pthread_create(&pthread, 0, signal_thread, 0);
#endif  // YASKKSERV_CONFIG_HAVE_PTHREAD

        if (google_cache_file_)
        {
                syslog_.printf(1, SkkSyslog::LEVEL_INFO, "load cache file \"%s\"", google_cache_file_);
                load_cache_file(google_cache_file_);
        }

        bool result = true;
        for (;;)
        {
                fd_set fd_set_read;
                int select_result = main_loop_select_polling(fd_set_read);
                DEBUG_PRINTF("select_result=%d\n", select_result);
                if (global_sighup_flag)
                {
                        local_main_loop_sighup();
                }
                if (select_result == 0)
                {
                }
                else
                {
                        main_loop_check_reload_dictionary(skk_dictionary_, skk_dictionary_length_, dictionary_filename_table_, dictionary_check_update_flag_);
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
                        if (!local_main_loop_loop(fd_set_read))
                        {
                                goto ERROR_BREAK;
                        }
                }
        }
ERROR_BREAK:
        result = false;

#ifdef YASKKSERV_CONFIG_HAVE_PTHREAD
        pthread_join(pthread, 0);
#endif  // YASKKSERV_CONFIG_HAVE_PTHREAD

        return result;
}

int print_usage()
{
        SkkUtility::printf("Usage: yaskkserv [OPTION] dictionary [dictionary...]\n"
                           "  -c, --check-update       check update dictionary (default disable)\n"
                           "  -d, --debug              enable debug mode (default disable)\n"
                           "  -h, --help               print this help and exit\n"
                           "  -l, --log-level=LEVEL    loglevel (range [0 - 9]  default 1)\n"
                           "  -m, --max-connection=N   max connection (default 8)\n"
                           "  -p, --port=PORT          set port (default 1178)\n"
                           "  -f, --no-daemonize       not daemonize\n"
                           "      --server-completion-midasi-length=LENGTH\n"
                           "                           set midasi length (range [256 - 32768]  default 2048)\n"
                           "      --server-completion-midasi-string-size=SIZE\n"
                           "                           set midasi string size (range [16384 - 1048576]  default 262144)\n"
                           "      --server-completion-test=type\n"
                           "                           1:default  2:ignore slash  3:space  4:space and protocol 'c'\n"
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                           "      --google-cache=N     use google cache(default 0(disable))\n"
                           "      --use-http           force use http(default https or http)\n"
                           "      --use-ipv6           use ipv6(default ipv4)\n"
                           "      --google-japanese-input=TYPE\n"
                           "                           enable google japanese input\n"
                           "                             disable                : disable\n"
                           "                             notfound               : use japanese input or suggest if not found in dictionary\n"
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                           "                             notfound-suggest-input : Suggest -> JapaneseInput\n"
                           "                             notfound-input-suggest : JapaneseInput -> Suggest\n"
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                           "                             dictionary             : JapaneseInput = https://www.google.com\n"
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                           "                                                      Suggest       = https://suggest.google.com\n"
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                           "          ex.)\n"
                           "              https\n"
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                           "              # yaskkserv --google-japanese-input=dictionary --google-suggest DIC https://suggest.google.com https://www.google.com\n"
                           "              # yaskkserv --google-japanese-input=notfound-suggest-input --google-suggest DIC\n"
                           "              # yaskkserv --google-japanese-input=notfound --google-suggest DIC\n"
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                           "              # yaskkserv --google-japanese-input=dictionary DIC https://www.google.com\n"
                           "              # yaskkserv --google-japanese-input=notfound DIC\n"
                           "              # yaskkserv --google-japanese-input=notfound DIC\n"
                           "              http\n"
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                           "              # yaskkserv --google-japanese-input=dictionary --google-suggest DIC http://suggest.google.com http://www.google.com\n"
                           "              # yaskkserv --google-japanese-input=dictionary --google-suggest --use-http DIC http://suggest.google.com http://www.google.com\n"
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                           "              # yaskkserv --google-japanese-input=dictionary DIC http://www.google.com\n"
                           "              # yaskkserv --google-japanese-input=dictionary --use-http DIC http://www.google.com\n"
                           "              # yaskkserv --google-japanese-input=notfound --use-http DIC\n"
                           "      --google-japanese-input-timeout=SECOND\n"
                           "                           set enable google japanese input timeout (default 2.5)\n"
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                           "      --google-suggest\n"
                           "                           enable google suggest\n"
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                           "      --google-cache-file=filename\n"
                           "                           save/load cache filename\n"
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
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
        OPTION_TABLE_CHECK_UPDATE,
        OPTION_TABLE_DEBUG,
        OPTION_TABLE_HELP,
        OPTION_TABLE_LOG_LEVEL,
        OPTION_TABLE_MAX_CONNECTION,
        OPTION_TABLE_PORT,
        OPTION_TABLE_NO_DAEMONIZE,
        OPTION_TABLE_SERVER_COMPLETION_MIDASI_LENGTH,
        OPTION_TABLE_SERVER_COMPLETION_MIDASI_STRING_SIZE,
        OPTION_TABLE_SERVER_COMPLETION_TEST,
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        OPTION_TABLE_GOOGLE_CACHE,
        OPTION_TABLE_USE_HTTP,
        OPTION_TABLE_USE_IPV6,
        OPTION_TABLE_GOOGLE_JAPANESE_INPUT,
        OPTION_TABLE_GOOGLE_JAPANESE_INPUT_TIMEOUT,
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        OPTION_TABLE_GOOGLE_SUGGEST,
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        OPTION_TABLE_GOOGLE_CACHE_FILE,
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        OPTION_TABLE_VERSION,

        OPTION_TABLE_LENGTH
};

const SkkCommandLine::Option option_table[] =
{
        {
                "c", "check-update",
                SkkCommandLine::OPTION_ARGUMENT_NONE,
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
                "f", "no-daemonize",
                SkkCommandLine::OPTION_ARGUMENT_NONE,
        },
        {
                0, "server-completion-midasi-length",
                SkkCommandLine::OPTION_ARGUMENT_INTEGER,
        },
        {
                0, "server-completion-midasi-string-size",
                SkkCommandLine::OPTION_ARGUMENT_INTEGER,
        },
        {
                0, "server-completion-test",
                SkkCommandLine::OPTION_ARGUMENT_INTEGER,
        },
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        {
                0, "google-cache",
                SkkCommandLine::OPTION_ARGUMENT_INTEGER,
        },
        {
                0, "use-http",
                SkkCommandLine::OPTION_ARGUMENT_NONE,
        },
        {
                0, "use-ipv6",
                SkkCommandLine::OPTION_ARGUMENT_NONE,
        },
        {
                0, "google-japanese-input",
                SkkCommandLine::OPTION_ARGUMENT_STRING,
        },
        {
                0, "google-japanese-input-timeout",
                SkkCommandLine::OPTION_ARGUMENT_FLOAT,
        },
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        {
                0, "google-suggest",
                SkkCommandLine::OPTION_ARGUMENT_NONE,
        },
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        {
                0, "google-cache-file",
                SkkCommandLine::OPTION_ARGUMENT_STRING,
        },
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
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
        int server_completion_midasi_length;
        int server_completion_midasi_string_size;
        int server_completion_test;
        int google_cache;
        bool use_http_flag;
        bool use_ipv6_flag;
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        LocalSkkServer::GoogleJapaneseInputType google_japanese_input_type;
        float google_japanese_input_timeout;
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        bool google_suggest_flag;
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        const char *google_cache_file;
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        bool no_daemonize_flag;
        bool check_update_flag;
        bool debug_flag;
}
option =
{
        1,
        8,
        1178,
        2048,
        262144,
        1,
        false,
        false,
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        0,
        LocalSkkServer::GOOGLE_JAPANESE_INPUT_TYPE_DISABLE,
        2.5f,
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        false,
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        0,
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        false,
        false,
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
                if (command_line.isOptionDefined(OPTION_TABLE_CHECK_UPDATE))
                {
                        option.check_update_flag = true;
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
                                SkkUtility::printf("Illegal log-level %d (0 - 9)\n\n", option.log_level);
                                result = print_usage();
                                return true;
                        }
                }
                if (command_line.isOptionDefined(OPTION_TABLE_MAX_CONNECTION))
                {
                        option.max_connection = command_line.getOptionArgumentInteger(OPTION_TABLE_MAX_CONNECTION);
                        if ((option.max_connection < 1) || (option.max_connection > 1024))
                        {
                                SkkUtility::printf("Illegal max-connection %d (1 - 1024)\n\n", option.max_connection);
                                result = print_usage();
                                return true;
                        }
                }
                if (command_line.isOptionDefined(OPTION_TABLE_PORT))
                {
                        option.port = command_line.getOptionArgumentInteger(OPTION_TABLE_PORT);
                        if ((option.port < 1) || (option.port > 65535))
                        {
                                SkkUtility::printf("Illegal port number %d (1 - 65535)\n\n", option.port);
                                result = print_usage();
                                return true;
                        }
                }
                if (command_line.isOptionDefined(OPTION_TABLE_NO_DAEMONIZE))
                {
                        option.no_daemonize_flag = true;
                }
                if (command_line.isOptionDefined(OPTION_TABLE_SERVER_COMPLETION_MIDASI_LENGTH))
                {
                        option.server_completion_midasi_length = command_line.getOptionArgumentInteger(OPTION_TABLE_SERVER_COMPLETION_MIDASI_LENGTH);
                        if ((option.server_completion_midasi_length < 256) || (option.server_completion_midasi_length > 32768))
                        {
                                SkkUtility::printf("Illegal midasi length %d (256 - 32768)\n\n", option.server_completion_midasi_length);
                                result = print_usage();
                                return true;
                        }
                }
                if (command_line.isOptionDefined(OPTION_TABLE_SERVER_COMPLETION_MIDASI_STRING_SIZE))
                {
                        option.server_completion_midasi_string_size = command_line.getOptionArgumentInteger(OPTION_TABLE_SERVER_COMPLETION_MIDASI_STRING_SIZE);
                        if ((option.server_completion_midasi_string_size < 16 * 1024) || (option.server_completion_midasi_string_size > 1024 * 1024))
                        {
                                SkkUtility::printf("Illegal string size %d (16384 - 1048576)\n\n", option.server_completion_midasi_string_size);
                                result = print_usage();
                                return true;
                        }
                }
                if (command_line.isOptionDefined(OPTION_TABLE_SERVER_COMPLETION_TEST))
                {
                        option.server_completion_test = command_line.getOptionArgumentInteger(OPTION_TABLE_SERVER_COMPLETION_TEST);
                        if ((option.server_completion_test < 1) || (option.server_completion_test > 4))
                        {
                                SkkUtility::printf("Illegal argument %d (1 - 4)\n\n", option.server_completion_test);
                                result = print_usage();
                                return true;
                        }
                }
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                if (command_line.isOptionDefined(OPTION_TABLE_GOOGLE_CACHE))
                {
                        option.google_cache = command_line.getOptionArgumentInteger(OPTION_TABLE_GOOGLE_CACHE);
                        if (option.google_cache > 32768)
                        {
                                SkkUtility::printf("Illegal google cache entry size %d (0 - 32768)\n\n", option.google_cache);
                                result = print_usage();
                                return true;
                        }
                }
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                if (command_line.isOptionDefined(OPTION_TABLE_GOOGLE_SUGGEST))
                {
                        option.google_suggest_flag = true;
                }
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                if (command_line.isOptionDefined(OPTION_TABLE_USE_HTTP))
                {
                        option.use_http_flag = true;
                }
                if (command_line.isOptionDefined(OPTION_TABLE_USE_IPV6))
                {
                        option.use_ipv6_flag = true;
                }
                if (command_line.isOptionDefined(OPTION_TABLE_GOOGLE_JAPANESE_INPUT))
                {
                        const char *p = command_line.getOptionArgumentString(OPTION_TABLE_GOOGLE_JAPANESE_INPUT);
                        if (SkkSimpleString::compare("disable", p) == 0)
                        {
                                option.google_japanese_input_type = LocalSkkServer::GOOGLE_JAPANESE_INPUT_TYPE_DISABLE;
                        }
                        else if (SkkSimpleString::compare("notfound", p) == 0)
                        {
                                option.google_japanese_input_type = LocalSkkServer::GOOGLE_JAPANESE_INPUT_TYPE_NOTFOUND;
                        }
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                        else if (SkkSimpleString::compare("notfound-suggest-input", p) == 0)
                        {
                                if (!option.google_suggest_flag)
                                {
                                        SkkUtility::printf("need --google-suggest\n\n");
                                        result = print_usage();
                                        return true;
                                }
                                option.google_japanese_input_type = LocalSkkServer::GOOGLE_JAPANESE_INPUT_TYPE_NOTFOUND_SUGGEST_INPUT;
                        }
                        else if (SkkSimpleString::compare("notfound-input-suggest", p) == 0)
                        {
                                if (!option.google_suggest_flag)
                                {
                                        SkkUtility::printf("need --google-suggest\n\n");
                                        result = print_usage();
                                        return true;
                                }
                                option.google_japanese_input_type = LocalSkkServer::GOOGLE_JAPANESE_INPUT_TYPE_NOTFOUND_INPUT_SUGGEST;
                        }
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                        else if (SkkSimpleString::compare("dictionary", p) == 0)
                        {
                                option.google_japanese_input_type = LocalSkkServer::GOOGLE_JAPANESE_INPUT_TYPE_DICTIONARY;
                        }
                        else
                        {
                                SkkUtility::printf("Illegal argument %s (disable, notfound or dictionary)\n\n", p);
                                result = print_usage();
                                return true;
                        }
                }
                if (command_line.isOptionDefined(OPTION_TABLE_GOOGLE_JAPANESE_INPUT_TIMEOUT))
                {
                        option.google_japanese_input_timeout = command_line.getOptionArgumentFloat(OPTION_TABLE_GOOGLE_JAPANESE_INPUT_TIMEOUT);
                        if ((option.google_japanese_input_timeout <= 0.0f) || (option.google_japanese_input_timeout > 60.0f))
                        {
                                SkkUtility::printf("Illegal second %f (0.0 - 60.0)\n\n", option.google_japanese_input_timeout);
                                result = print_usage();
                                return true;
                        }
                }
                if (command_line.isOptionDefined(OPTION_TABLE_GOOGLE_CACHE_FILE))
                {
                        option.google_cache_file = command_line.getOptionArgumentString(OPTION_TABLE_GOOGLE_CACHE_FILE);
                }
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
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
int local_main_core_setup_dictionary(const SkkCommandLine &command_line, LocalSkkDictionary *skk_dictionary)
{
        int result = EXIT_SUCCESS;
        int skk_dictionary_length = command_line.getArgumentLength();
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        int google_japanese_input_count = 0;
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
        int google_suggest_count = 0;
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        for (int i = 0; i != skk_dictionary_length; ++i)
        {
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                if (SkkSimpleString::compare("http://www.google.com", command_line.getArgumentPointer(i)) == 0)
                {
                        if (option.google_japanese_input_type != LocalSkkServer::GOOGLE_JAPANESE_INPUT_TYPE_DICTIONARY)
                        {
                                SkkUtility::printf("google dictionary need --google-japanese-input=dictionary\n");
                                result = EXIT_FAILURE;
                                break;
                        }
                        ++google_japanese_input_count;
                        if (google_japanese_input_count >= 2)
                        {
                                SkkUtility::printf("google japanese input dictionary must less than 2\n",
                                                   command_line.getArgumentPointer(i),
                                                   i);
                                result = EXIT_FAILURE;
                                break;
                        }
                        (skk_dictionary + i)->setGoogleJapaneseInputFlag(true, false);
                }
#if defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                else if (SkkSimpleString::compare("https://www.google.com", command_line.getArgumentPointer(i)) == 0)
                {
                        if (option.google_japanese_input_type != LocalSkkServer::GOOGLE_JAPANESE_INPUT_TYPE_DICTIONARY)
                        {
                                SkkUtility::printf("google dictionary need --google-japanese-input=dictionary\n");
                                result = EXIT_FAILURE;
                                break;
                        }
                        ++google_japanese_input_count;
                        if (google_japanese_input_count >= 2)
                        {
                                SkkUtility::printf("google japanese input dictionary must less than 2\n",
                                                   command_line.getArgumentPointer(i),
                                                   i);
                                result = EXIT_FAILURE;
                                break;
                        }
                        (skk_dictionary + i)->setGoogleJapaneseInputFlag(true, true);
                }
#endif  // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                else if (SkkSimpleString::compare("http://suggest.google.com", command_line.getArgumentPointer(i)) == 0)
                {
                        if (option.google_japanese_input_type != LocalSkkServer::GOOGLE_JAPANESE_INPUT_TYPE_DICTIONARY)
                        {
                                SkkUtility::printf("google dictionary need --google-japanese-input=dictionary\n");
                                result = EXIT_FAILURE;
                                break;
                        }
                        if (!option.google_suggest_flag)
                        {
                                SkkUtility::printf("need --google-suggest\n");
                                result = EXIT_FAILURE;
                                break;
                        }
                        ++google_suggest_count;
                        if (google_suggest_count >= 2)
                        {
                                SkkUtility::printf("google suggest dictionary must less than 2\n",
                                                   command_line.getArgumentPointer(i),
                                                   i);
                                result = EXIT_FAILURE;
                                break;
                        }
                        (skk_dictionary + i)->setGoogleSuggestFlag(true, false);
                }
#if defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
                else if (SkkSimpleString::compare("https://suggest.google.com", command_line.getArgumentPointer(i)) == 0)
                {
                        if (option.google_japanese_input_type != LocalSkkServer::GOOGLE_JAPANESE_INPUT_TYPE_DICTIONARY)
                        {
                                SkkUtility::printf("google dictionary need --google-japanese-input=dictionary\n");
                                result = EXIT_FAILURE;
                                break;
                        }
                        if (!option.google_suggest_flag)
                        {
                                SkkUtility::printf("need --google-suggest\n");
                                result = EXIT_FAILURE;
                                break;
                        }
                        ++google_suggest_count;
                        if (google_suggest_count >= 2)
                        {
                                SkkUtility::printf("google suggest dictionary must less than 2\n",
                                                   command_line.getArgumentPointer(i),
                                                   i);
                                result = EXIT_FAILURE;
                                break;
                        }
                        (skk_dictionary + i)->setGoogleSuggestFlag(true, true);
                }
#endif  // defined(YASKKSERV_CONFIG_HEADER_HAVE_GNUTLS_OPENSSL) || defined(YASKKSERV_CONFIG_HEADER_HAVE_OPENSSL)
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                else
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
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
        LocalSkkDictionary *skk_dictionary = new LocalSkkDictionary[skk_dictionary_length];
        result = local_main_core_setup_dictionary(command_line, skk_dictionary);

        if (result == EXIT_SUCCESS)
        {
                LocalSkkServer *skk_server = new LocalSkkServer(option.port, option.log_level);
                const int listen_queue = 5;
                skk_server->initialize(skk_dictionary,
                                       &argv[command_line.getArgumentArgvIndex()],
                                       skk_dictionary_length,
                                       option.max_connection,
                                       listen_queue,
                                       option.server_completion_midasi_length,
                                       option.server_completion_midasi_string_size,
                                       option.server_completion_test,
                                       option.google_cache,
                                       option.google_cache_file,
                                       option.check_update_flag,
                                       option.no_daemonize_flag,
                                       option.use_http_flag,
                                       option.use_ipv6_flag);
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                skk_server->setGoogleJapaneseInputParameter(option.google_japanese_input_type, option.google_japanese_input_timeout);
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
                skk_server->setGoogleSuggestParameter(option.google_suggest_flag);
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_SUGGEST
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                if (!skk_server->mainLoop())
                {
                        result = EXIT_FAILURE;
                }
                delete skk_server;
        }

        delete[] skk_dictionary;

        return result;
}

#ifdef YASKKSERV_HAIRY_TEST

int local_main_test(int argc, char *argv[])
{
        const int allocate_size = 16;
        SkkSimpleString string_a(allocate_size);
        printf("* try append char\n");
        for (int i = 0; i != 8; ++i)
        {
                printf("i = %d\n", i);
                string_a.append('a');
                printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());
        }


        printf("* try appendFast char\n");
        string_a.reset();
        for (int i = 0; i != 8; ++i)
        {
                printf("i = %d\n", i);
                printf("    A %s\n", string_a.isAppendSize(1) ? "ok" : "ng");
                if (string_a.isAppendSize(1))
                {
                        string_a.appendFast('a');
                        printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());
                }
        }


        printf("* try append char pointer 1\n");
        string_a.reset();
        for (int i = 0; i != 8; ++i)
        {
                printf("i = %d\n", i);
                string_a.append("a", 1);
                printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());
        }


        printf("* try append char pointer 2\n");
        string_a.reset();
        for (int i = 0; i != 8; ++i)
        {
                printf("i = %d\n", i);
                string_a.append("ab", 2);
                printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());
        }


        printf("* try appendFast char pointer 1\n");
        string_a.reset();
        for (int i = 0; i != 8; ++i)
        {
                printf("i = %d\n", i);
                printf("    A %s\n", string_a.isAppendSize(1) ? "ok" : "ng");
                if (string_a.isAppendSize(1))
                {
                        string_a.appendFast("A");
                        printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());
                }
        }


        printf("* try appendFast char pointer 2\n");
        string_a.reset();
        for (int i = 0; i != 8; ++i)
        {
                printf("i = %d\n", i);
                printf("    A %s\n", string_a.isAppendSize(2) ? "ok" : "ng");
                if (string_a.isAppendSize(2))
                {
                        string_a.appendFast("AB");
                        printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());
                }
        }


        printf("* try terminate\n");
        string_a.terminate(2);
        printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());

        printf("* try reset\n");
        string_a.reset();
        printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());

        printf("* try fillStringBuffer\n");
        string_a.fillStringBuffer('x');
        printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());

        printf("* try setAppendIndex\n");
        string_a.setAppendIndex(2);
        printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());

        printf("* try terminator append\n");
        string_a.append('\0');
        printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());

        printf("* try fillStringBuffer\n");
        string_a.fillStringBuffer('X');
        printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());

        printf("* try setAppendIndex\n");
        string_a.setAppendIndex(2);
        printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());

        printf("* try terminator appendFast\n");
        string_a.append('\0');
        printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());

        printf("* try fillStringBuffer\n");
        string_a.fillStringBuffer('Y');
        printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());

        printf("* try setAppendIndex\n");
        string_a.setAppendIndex(2);
        printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());

        printf("* try string terminator append\n");
        string_a.append("\0", 1);
        printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());

        printf("* try fillStringBuffer\n");
        string_a.fillStringBuffer('y');
        printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());

        printf("* try setAppendIndex\n");
        string_a.setAppendIndex(2);
        printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());

        printf("* try string terminator appendFast\n");
        string_a.appendFast("\0");
        printf("    A %s size=%d\n", string_a.getBuffer(), string_a.getSize());
}

int local_main_test_3(int argc, char *argv[])
{
        const int allocate_size = 16;
        SimpleStringForHairy string_a(allocate_size);
        printf("allocate_size=%5d  getValidBufferSize()=%5d\n", allocate_size, string_a.getValidBufferSize());
        string_a.appendFast("0123456");
        // for (int i = 0; i != 7; ++i)
        // {
        //         string_a.appendFast('a' + i);
        // }
        printf("%s size=%d\n", string_a.getBuffer(), string_a.getSize());
        return 0;
}

int local_main_test_2(int argc, char *argv[])
{
        const int allocate_size = 16;
        SimpleStringForHairy string_a(allocate_size);
        SimpleStringForHairy string_b(allocate_size);
        SimpleStringForHairy string_c(allocate_size);
        printf("allocate_size=%5d  getValidBufferSize()=%5d\n", allocate_size, string_a.getValidBufferSize());
        int length = 2;
        for (int i = 0; i != allocate_size; ++i)
        {
                printf("i=%3d\n", i);

                printf("  A  getSize()=%5d   getRemainSize()=%5d \"%s\"\n", string_a.getSize(), string_a.getRemainSize(), string_a.getBuffer());
                printf("     isAppendSize(%d) %s\n", length, string_a.isAppendSize(length) ? "ok" : "NG");
                for (int h = 1; h <= length; ++h)
                {
                        printf("     append(char) %s\n", string_a.append('a') ? "ok" : "NG");
                }

                printf("  B  getSize()=%5d   getRemainSize()=%5d \"%s\"\n", string_b.getSize(), string_b.getRemainSize(), string_b.getBuffer());
                printf("     isAppendSize(%d) %s\n", length, string_b.isAppendSize(length) ? "ok" : "NG");
                for (int h = 1; h <= length; ++h)
                {
                        printf("     try appendFast(char)\n");
                        string_b.appendFast('a');
                }

                printf("  C  getSize()=%5d   getRemainSize()=%5d \"%s\"\n", string_c.getSize(), string_c.getRemainSize(), string_c.getBuffer());
                printf("     isAppendSize(%d) %s\n", length, string_c.isAppendSize(length) ? "ok" : "NG");
                printf("     append(void *p, int size) %s\n", string_c.append("aaaa", length) ? "ok" : "NG");
        }
        return 0;
}

#endif  // YASKKSERV_HAIRY_TEST
}

int local_main(int argc, char *argv[])
{
#ifdef YASKKSERV_HAIRY_TEST
        return local_main_test(argc, argv);
#else  // YASKKSERV_HAIRY_TEST
        return local_main_core(argc, argv);
#endif  // YASKKSERV_HAIRY_TEST
}
}
