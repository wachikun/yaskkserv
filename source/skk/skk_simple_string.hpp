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
#ifndef SKK_SIMPLE_STRING_HPP
#define SKK_SIMPLE_STRING_HPP

#include "skk_architecture.hpp"

#ifdef YASKKSERV_DEBUG
#define SKK_SIMPLE_STRING_DEBUG_BUFFER
#endif  // YASKKSERV_DEBUG

namespace YaSkkServ
{
/// 単純な文字列クラスです。
/**
 * コンストラクタで指定したバッファに文字列を追加する形で管理します。
 *
 * バッファは以下のような構造になっています。
 *
 *<pre>
+--------+----------------+--------+
| MARGIN | 文字列バッファ | MARGIN |
+--------+----------------+--------+</pre>
 *
 * MARGIN はデバッグなどに使用する領域です。このため扱うことのできる文
 * 字列長は、コンストラクタで指定したバッファより幾分小さなものとなりま
 * す。
 *
 * <h1>append, appendFast() と overwrite</h1>
 *
 * append は文字列末尾に追加し、 \\0 で終端します。バッファに収まるか
 * どうかをチェックして、追加できなければ何もせず偽を返します。
 * appendFast は append と同様ですが、追加可能かどうかをしません。
 * isAppendSize() などで事前に検証する必要があります。
 *
 * overwrite は指定位置に文字列を上書きするだけです。このとき \\0 を上
 * 書きすることはありません。つまり \\0 で終端されません。
 *
 * まとめると以下のようになります。
 *
 * <dl>
 * <dt>append
 * <dd>文字列を追加。 \\0 を追加。追加チェックあり。
 * <dt>appendFast
 * <dd>文字列を追加。 \\0 を追加。追加チェックなし。 isAppendSize() などで事前にチェックが必要。
 * <dt>overwrite
 * <dd>文字列を上書き。 \\0 に触れない。
 * </dl>
 *
 *
 * <h1>文字列バッファ と 後方の MARGIN</h1>
 *
 * 通常文字列終端の \\0 は文字列バッファ内に置かれます。
 * DEBUG_PARANOIA_ASSERT() では文字列バッファ内に収まらない場合アサート
 * します。その上で、後方の MARGIN の先頭 1 バイトが 0 であることを保証
 * します。
 *
 * つまり、通常は終端である \\0 は文字列バッファに置かれなければなりま
 * せんが、万が一 \\0 が文字列バッファに収まりきらなかった場合 (通常は
 * ありえませんが) でも後方の MARGIN の先頭が 0 であることから、必ず文
 * 字列は終端されることが保証されます。
 *
 *
 * <h1>コンストラクタ</h1>
 *
 * バッファへのポインタを渡す場合、ポインタが 0 のときアサートが有効な
 * らばアサートします。
 *
 *
 * <h1>その他メソッドの基本動作</h1>
 *
 * 引数などが不正な場合、アサートが有効ならばアサートします。
 */
class SkkSimpleString
{
        SkkSimpleString(SkkSimpleString &source);
        SkkSimpleString& operator=(SkkSimpleString &source);

public:
        enum
        {
                MARGIN_SIZE = 4,
                STRING_SIZE_MAXIMUM = 8 * 1024
        };
        enum Flag
        {
                FLAG_RIGHT,
                FLAG_RIGHT_ZERO,
                FLAG_LEFT,
        };

protected:
        void update_string_size_from_current()
        {
#ifdef YASKKSERV_DEBUG
                check_margin_for_debug();
#endif  // YASKKSERV_DEBUG
                string_size_ = static_cast<int>(current_ - get_buffer());
        }

        void update_string_size_by_strlen()
        {
#ifdef YASKKSERV_DEBUG
                check_margin_for_debug();
#endif  // YASKKSERV_DEBUG
                string_size_ = 0;
                const char *p = get_buffer();
                for (int i = 0; i < getValidBufferSize(); ++i)
                {
                        if (*(p + i) == '\0')
                        {
                                string_size_ = i;
                                return;
                        }
                }
                DEBUG_ASSERT(0);
        }

        void fill_margin_for_debug()
        {
#ifdef YASKKSERV_DEBUG
                *(buffer_ + 0) = 0;
                *(buffer_ + buffer_size_ - MARGIN_SIZE + 0) = 0;
                for (int i = 1; i != MARGIN_SIZE; ++i)
                {
                        *(buffer_ + i) = static_cast<char>(1 + i);
                        *(buffer_ + buffer_size_ - MARGIN_SIZE + i) = static_cast<char>(1 + i);
                }
#endif  // YASKKSERV_DEBUG
        }

        void check_margin_for_debug() const
        {
#ifdef YASKKSERV_DEBUG
                DEBUG_ASSERT(*(buffer_ + 0) == 0);
                DEBUG_ASSERT(*(buffer_ + buffer_size_ - MARGIN_SIZE + 0) == 0);
                for (int i = 1; i != MARGIN_SIZE; ++i)
                {
                        DEBUG_ASSERT(*(buffer_ + i) == 1 + i);
                        DEBUG_ASSERT(*(buffer_ + buffer_size_ - MARGIN_SIZE + i) == 1 + i);
                }
#endif  // YASKKSERV_DEBUG
        }

        char *get_buffer() const
        {
                return buffer_ + MARGIN_SIZE;
        }

        void initialize_buffer()
        {
                fill_margin_for_debug();

                // 文字列先頭を 0 に。
                *get_buffer() = '\0';
                // 後マージンの先頭を 0 にして文字列領域が一杯の場合も
                // 終端されることを保証します。
                *(buffer_ + buffer_size_ - MARGIN_SIZE) = '\0';
        }

        static int compare_internal(const void *s_1, const void *s_2)
        {
                DEBUG_ASSERT_POINTER(s_1);
                DEBUG_ASSERT_POINTER(s_2);
                for (int i = 0;; ++i)
                {
                        int tmp_1 = *(static_cast<const char*>(s_1) + i);
                        int tmp_2 = *(static_cast<const char*>(s_2) + i);
                        if (tmp_1 == '\0')
                        {
                                if (tmp_2 == '\0')
                                {
                                        return 0;
                                }
                                else
                                {
                                        return -(i + 1);
                                }
                        }
                        else if (tmp_2 == '\0')
                        {
                                if (tmp_1 == '\0')
                                {
                                        return 0;
                                }
                                else
                                {
                                        return i + 1;
                                }
                        }
                        else if (tmp_1 > tmp_2)
                        {
                                return i + 1;
                        }
                        else if (tmp_1 < tmp_2)
                        {
                                return -(i + 1);
                        }
                }
                return 0;
        }

        static int compare_internal(const void *s_1, const void *s_2, int compare_size)
        {
                DEBUG_ASSERT_POINTER(s_1);
                DEBUG_ASSERT_POINTER(s_2);
                for (int i = 0; i != compare_size; ++i)
                {
                        int tmp_1 = *(static_cast<const char*>(s_1) + i);
                        int tmp_2 = *(static_cast<const char*>(s_2) + i);
                        if (tmp_1 == '\0')
                        {
                                if (tmp_2 == '\0')
                                {
                                        return 0;
                                }
                                else
                                {
                                        return -(i + 1);
                                }
                        }
                        else if (tmp_2 == '\0')
                        {
                                if (tmp_1 == '\0')
                                {
                                        return 0;
                                }
                                else
                                {
                                        return i + 1;
                                }
                        }
                        else if (tmp_1 > tmp_2)
                        {
                                return i + 1;
                        }
                        else if (tmp_1 < tmp_2)
                        {
                                return -(i + 1);
                        }
                }
                return 0;
        }

/// 文字列のバイトサイズを返します。サイズに終端の \\0 は含みません。
/**
 * \attention
 * limit 以上の巨大なデータの場合、途中で計測を諦め 0 を返します。この
 * ときアサートが有効ならばアサートします。
 */
        static int get_size_internal(const void *p, int limit = 64 * 1024)
        {
                const char *tmp = static_cast<const char*>(p);
                for (int i = 0; i != limit; ++i)
                {
                        if (*tmp++ == '\0')
                        {
                                return i;
                        }
                }
                DEBUG_ASSERT(0);
                return 0;
        }

/// 文字をコピーします。
/**
 * \attention
 * c に \\0 を指定して書き込み、文字列を切断することができます。
 */
        static void overwrite_internal(char c, void *destination)
        {
                char *p = static_cast<char*>(destination);
                *p = c;
        }

/// 文字列をコピーします。文字列の最後 + 1 を指すポインタを返します。
/**
 * \attention
 * \\0 はコピーされません。
 */
        static char *overwrite_internal(const void *source, void *destination, int size)
        {
                const char *s = static_cast<const char*>(source);
                char *d = static_cast<char*>(destination);
                for (; size > 0; --size)
                {
                        if (*s == '\0')
                        {
                                break;
                        }
                        *d++ = *s++;
                }
                return d;
        }

/// column 桁の 10 進文字列を生成します。文字列の最後 + 1 を指すポインタを返します。
/**
 * 符号も column に含まれます。
 *
 * \attention
 * column より多くの桁は表示されません。この場合、上位の桁は削られます。
 */
        static char *overwrite_internal(void *destination, int scalar, Flag flag = FLAG_LEFT, int column = 11)
        {
                static const int table[] =
                {
                        1,
                        10,
                        100,
                        1000,
                        10000,

                        100000,
                        1000000,
                        10000000,
                        100000000,
                        1000000000,
                };
                int space = (flag == FLAG_RIGHT_ZERO) ? '0' : ' ';
                int sign;
                int absolute;
                const int table_size = sizeof(table)/sizeof(table[0]);
                char *p = static_cast<char*>(destination);
                bool zero_print_flag = false;
                bool right_flag = ((flag == FLAG_RIGHT_ZERO) || (flag == FLAG_RIGHT)) ? true : false;
                DEBUG_ASSERT_RANGE(column, 1, 11);
                if (scalar < 0)
                {
                        sign = '-';
                        absolute = -scalar;
                }
                else
                {
                        sign = 0;
                        absolute = scalar;
                        if (right_flag && (column == 11))
                        {
                                *p++ = ' ';
                        }
                }
                for (int i = table_size - 1; i >= 0; --i)
                {
                        int d = absolute / table[i];
                        if (d == 0)
                        {
                                if (zero_print_flag || (i == 0))
                                {
                                        if (i < column)
                                        {
                                                *p++ = '0';
                                        }
                                }
                                else
                                {
                                        if (right_flag)
                                        {
                                                if (i < column)
                                                {
                                                        *p++ = static_cast<char>(space);
                                                }
                                        }
                                }
                        }
                        else
                        {
                                if (sign != 0)
                                {
                                        if (i < column - 1)
                                        {
                                                if (column != 11)
                                                {
                                                        --column;
                                                        if (p != destination)
                                                        {
                                                                --p;
                                                        }
                                                }
                                                *p++ = static_cast<char>(sign);
                                        }
                                        sign = 0;
                                }
                                absolute -= d * table[i];
                                zero_print_flag = true;
                                if (i < column)
                                {
                                        *p++ = static_cast<char>('0' + d);
                                }
                        }
                }
                return p;
        }

/// column 桁の 10 進文字列を生成します。文字列の最後 + 1 を指すポインタを返します。
/**
 * 符号とドットも column に含まれます。
 *
 * \attention
 * column より多くの桁は表示されません。この場合、上位の桁は削られます。
 */
        static char *overwrite_internal(void *destination, float scalar, int decimal = 1, Flag flag = FLAG_LEFT, int column = 11)
        {
                DEBUG_ASSERT_RANGE(column, 2, 11);
                DEBUG_ASSERT_RANGE(column - 1 - decimal, 1, 9);
                char *p = static_cast<char*>(destination);
                p = overwrite_internal(p, static_cast<int>(scalar), flag, column - 1 - decimal);
                *p++ = '.';
                int po = 1;
                for (int i = 0; i != decimal; ++i)
                {
                        po *= 10;
                }
                p = overwrite_internal(p, static_cast<int>(scalar * static_cast<float>(po)) % po, FLAG_RIGHT_ZERO, decimal);
                return p;
        }

/// column 桁の 16 進文字列を生成します。文字列の最後 + 1 を指すポインタを返します。
/**
 * \attention
 * column より多くの桁は表示されません。この場合、上位の桁は削られます。
 */
        static char *overwrite_internal_hexadecimal(void *destination, int scalar, int column = 8)
        {
                char *p = static_cast<char*>(destination);
                DEBUG_ASSERT_RANGE(column, 1, 8);
                for (int i = column - 1; i >= 0; --i)
                {
                        int tmp = (scalar >> (i * 4)) & 0xf;
                        if (tmp > 0xa)
                        {
                                *p++ = static_cast<char>('a' - 0xa + tmp);
                        }
                        else
                        {
                                *p++ = static_cast<char>('0' + tmp);
                        }
                }
                return p;
        }

public:
        virtual ~SkkSimpleString()
        {
                check_margin_for_debug();
                DEBUG_ASSERT_POINTER(buffer_);
                DEBUG_ASSERT(buffer_size_ != 0);
                if (dynamic_allocation_flag_)
                {
                        delete[] buffer_;
                }
#ifdef SKK_SIMPLE_STRING_DEBUG_BUFFER
                delete[] debug_buffer_;
#endif  // SKK_SIMPLE_STRING_DEBUG_BUFFER
        }

        SkkSimpleString(int buffer_size) :
#ifdef SKK_SIMPLE_STRING_DEBUG_BUFFER
                debug_buffer_(new char[buffer_size]),
                debug_write_buffer_start_(0),
#endif  // SKK_SIMPLE_STRING_DEBUG_BUFFER
                buffer_(new char[buffer_size]),
                current_(get_buffer()),
                buffer_size_(buffer_size),
                string_size_(0),
                dynamic_allocation_flag_(true)
        {
                DEBUG_ASSERT(buffer_size_ != 0);
                initialize_buffer();
        }

        SkkSimpleString(void *buffer, int buffer_size) :
#ifdef SKK_SIMPLE_STRING_DEBUG_BUFFER
                debug_buffer_(new char[buffer_size]),
                debug_write_buffer_start_(0),
#endif  // SKK_SIMPLE_STRING_DEBUG_BUFFER
                buffer_(static_cast<char*>(buffer)),
                current_(get_buffer()),
                buffer_size_(buffer_size),
                string_size_(0),
                dynamic_allocation_flag_(false)
        {
                DEBUG_ASSERT(buffer_size_ != 0);
                if (buffer_ == 0)
                {
                        DEBUG_ASSERT(0);
                }
                else
                {
                        initialize_buffer();
                }
        }

        template<size_t N> SkkSimpleString(char (&array)[N]) :
#ifdef SKK_SIMPLE_STRING_DEBUG_BUFFER
                debug_buffer_(new char[N]),
                debug_write_buffer_start_(0),
#endif  // SKK_SIMPLE_STRING_DEBUG_BUFFER
                buffer_(array),
                current_(get_buffer()),
                buffer_size_(N),
                string_size_(0),
                dynamic_allocation_flag_(false)
        {
                DEBUG_ASSERT(buffer_size_ != 0);
                if (buffer_ == 0)
                {
                        DEBUG_ASSERT(0);
                }
                else
                {
                        fill_margin_for_debug();
                        *(buffer_ + 0) = '\0';
                        *(buffer_ + buffer_size_ - MARGIN_SIZE) = '\0';
                }
        }

/// 次の append() 系メソッドのインデックスを設定します。
/**
 * \attention
 * バッファを使いまわす場合は setAppendIndex(0) とせず、 reset() を用い
 * てください。
 *
 * \attention
 * 不正な index を指定した場合は何もせずに抜けます。デバッグビルドでは
 * アサートします。
 */
        void setAppendIndex(int index)
        {
                if ((index < 0) || (index >= buffer_size_ - MARGIN_SIZE * 2 - 1))
                {
                        DEBUG_ASSERT(0);
                }
                else
                {
                        current_ = get_buffer() + index;
                        update_string_size_from_current();
                }
        }

/// 状態をリセットし空文字列を設定します。
/**
 * バッファを使いまわして複数の文字列を構築するために使います。
 *
 * \verbatim
ex.)
    char buffer[256];
    SimpleString object(buffer);

    object.append("ABC=");
    object.append(999);
    PRINT(object.getBuffer());

    object.reset();
    object.append("DEF=");
    object.append(999);
    PRINT(object.getBuffer());
\endverbatim
 */
        void reset()
        {
                current_ = get_buffer();
                *current_ = '\0';
                string_size_ = 0;
        }

/// 文字列バッファへのポインタを返します。
/**
 * 生成した文字列は、本メソッドで取得する必要があります。
 */
        const char *getBuffer() const
        {
                return get_buffer();
        }

/// カレント文字列バッファへのポインタを返します。
/**
 * 次回の append() はこの位置へ実行されます。
 */
        const char *getCurrentBuffer() const
        {
                return current_;
        }

/// 内部バッファへ直接書き込む直前に書き込まれる領域を宣言します。必ずペアとなる endWriteBuffer() を呼ぶ必要があります。引数が不正な領域を指していれば偽を返します。デバッグビルドではデータコンペア用にデータをバックアップします。
/**
 * \attention
 * 不正な領域を指していた場合、デバッグビルドではアサートします。
 */
        bool beginWriteBuffer(const char *start, int reserve_size)
        {
                const char *p = getBuffer();
#ifdef SKK_SIMPLE_STRING_DEBUG_BUFFER
                DEBUG_ASSERT(debug_write_buffer_start_ == 0);
                // バッファをまるごとコピー
                debug_write_buffer_start_ = start;
                memcpy(debug_buffer_, buffer_, buffer_size_);
#endif  // SKK_SIMPLE_STRING_DEBUG_BUFFER
                if ((start < p) || (start + reserve_size > p + getValidBufferSize()))
                {
                        DEBUG_PRINTF("start=%p  getBuffer()=%p  (start+reserve_size)=%p  (getBuffer()+getValidBufferSize())=%p\n",
                                     start,
                                     getBuffer(),
                                     start + reserve_size,
                                     p + getValidBufferSize());
                        DEBUG_ASSERT(0);
                        return false;
                }
                return true;
        }

/// 内部バッファへ直接書き込んだ領域を設定します。デバッグビルドではデータコンペアして不正な操作だった場合はアサートします。
/**
 * \attention
 * リリースビルドでは何もしません。戻り値も返しません。これは、検査する対象が致命的なエラーであるためです。
 */
        void endWriteBuffer(int write_size)
        {
#ifdef SKK_SIMPLE_STRING_DEBUG_BUFFER
                const char *p = getBuffer();
                if ((debug_write_buffer_start_ < p) || (debug_write_buffer_start_ + write_size > p + getValidBufferSize()))
                {
                        DEBUG_PRINTF("start=%p  getBuffer()=%p  (start+write_size)=%p  (getBuffer()+getValidBufferSize())=%p\n",
                                     debug_write_buffer_start_,
                                     getBuffer(),
                                     debug_write_buffer_start_ + write_size,
                                     p + getValidBufferSize());
                        DEBUG_ASSERT(0);
                }
                DEBUG_ASSERT_POINTER(debug_write_buffer_start_);
                // バッファの領域外、先頭付近を比較
                const char *before_a = buffer_;
                const char *before_b = debug_buffer_;
                DEBUG_ASSERT(before_a <= debug_write_buffer_start_);
                for (;;)
                {
                        if (before_a >= debug_write_buffer_start_)
                        {
                                break;
                        }
                        if (*before_a++ != *before_b++)
                        {
                                DEBUG_ASSERT(0);
                        }
                }
                // バッファの領域外、終端付近を比較
                size_t offset = debug_write_buffer_start_ - buffer_;
                const char *after_a = debug_write_buffer_start_ + MARGIN_SIZE + write_size;
                const char *after_b = debug_buffer_ + offset + MARGIN_SIZE + write_size;
                DEBUG_ASSERT(after_a <= buffer_ + buffer_size_);
                // DEBUG_PRINTF("a=%p  b()=%p  (buffer_ + buffer_size_)=%p\n",
                //              after_a,
                //              after_b,
                //              buffer_ + buffer_size_);
                // DEBUG_PRINTF("write_size=%d\n", write_size);
                for (;;)
                {
                        if (after_a >= buffer_ + buffer_size_)
                        {
                                break;
                        }
                        if (*after_a++ != *after_b++)
                        {
                                DEBUG_PRINTF("0x%02x : 0x%02x\n", *(after_a - 1) & 0xff, *(after_b - 1) & 0xff);
                                DEBUG_ASSERT(0);
                        }
                }
                debug_write_buffer_start_ = 0;
#else  // SKK_SIMPLE_STRING_DEBUG_BUFFER
                (void)write_size;
#endif // SKK_SIMPLE_STRING_DEBUG_BUFFER
        }

/// 有効なバッファサイズを返します。
/**
 * MARGIN を除いたバッファサイズを返します。
 */
        int getValidBufferSize() const
        {
                return buffer_size_ - MARGIN_SIZE * 2;
        }

/// 文字列長を返します。文字列長に終端の \\0 は含みません。
        int getSize() const
        {
                return string_size_;
        }

/// バッファに追加できる \\0 を除いたバイト数を返します。戻り値は実際の容量から \\0 の分だけ、 1 バイト少なくなります。
        int getRemainSize() const
        {
                const int terminator_size = 1;
                int remain_size = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - current_ - terminator_size);
                DEBUG_ASSERT_RANGE(remain_size, 0, buffer_size_ - MARGIN_SIZE * 2 - 1);
                return remain_size;
        }

/// 指定した値で文字列バッファの終端直前まで埋めます。
/**
 * \attention
 * 文字列バッファの終端へは常に \\0 が書き込まれます。
 */
        void fillStringBuffer(int c = '\0')
        {
                char *p = get_buffer();
                for (int i = 0; i != buffer_size_ - MARGIN_SIZE * 2 - 1; ++i)
                {
                        *p++ = static_cast<char>(c);
                }
                *p = '\0';
                current_ = p;
                update_string_size_from_current();
        }

/// '\0' を除く size バイトのデータが append() 可能ならば真を返します。
        bool isAppendSize(int size) const
        {
                if (getRemainSize() < size)
                {
                        return false;
                }
                else
                {
                        return true;
                }
        }
        bool isAppend(SkkSimpleString &string) const
        {
                return isAppendSize(string.getSize());
        }

/// 指定位置で文字列を切断します。
/**
 * \attention
 */
        void terminate(int index)
        {
                char *d = get_buffer() + index;
#ifdef YASKKSERV_DEBUG_PARANOIA
                const int terminator_size = 1;
                int size = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - d - terminator_size);
                DEBUG_ASSERT_RANGE(size, 1, buffer_size_ - MARGIN_SIZE * 2 - 1);
#endif  // YASKKSERV_DEBUG_PARANOIA
                if (index >= getSize())
                {
                        DEBUG_ASSERT(0);
                }
                else
                {
                        overwrite_internal('\0', d);
                        update_string_size_by_strlen();
                }
        }

#if 0
/// 指定位置に文字を上書きします。
/**
 * \attention
 * c に \\0 を指定して書き込み、文字列を切断することができます。 index
 * が現在の文字列内に収まらない場合は何もしません。アサートが有効ならば
 * アサートします。
 */
        void overwrite(char c, int index)
        {
                char *d = get_buffer() + index;
#ifdef YASKKSERV_DEBUG_PARANOIA
                const int terminator_size = 1;
                int size = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - d - terminator_size);
                DEBUG_ASSERT_RANGE(size, 1, buffer_size_ - MARGIN_SIZE * 2 - 1);
#endif  // YASKKSERV_DEBUG_PARANOIA
                if (index >= getSize())
                {
                        DEBUG_ASSERT(0);
                }
                else
                {
                        overwrite_internal(c, d);
                        update_string_size_by_strlen();
                }
        }

/// 指定位置に文字列を上書きします。
/**
 * \attention
 * 入力文字列 p 側の \\0 はコピーされません。
 */
        void overwrite(const void *p, int index)
        {
                char *d = get_buffer() + index;
                const int terminator_size = 1;
                int size = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - d - terminator_size);
                DEBUG_ASSERT_RANGE(size, 1, buffer_size_ - MARGIN_SIZE * 2 - 1);
#ifdef YASKKSERV_DEBUG_PARANOIA
                int tmp = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - (d + getSize(p) - 1) - terminator_size);
                DEBUG_ASSERT_RANGE(tmp, 1, buffer_size_ - MARGIN_SIZE * 2 - 1);
#endif  // YASKKSERV_DEBUG_PARANOIA
                overwrite_internal(p, d, size);
        }

/// 指定位置に文字列を上書きします。文字列 p は copy_size バイト分、または \\0 を見付けるまで追加されます。
/**
 * \attention
 * 入力文字列 p 側の \\0 はコピーされません。
 *
 * 通常の overwrite() と異なり、 p は \\0 で終端されている必要がありません。
 */
        void overwrite(const void *p, int index, int copy_size)
        {
                char *d = get_buffer() + index;
                const int terminator_size = 1;
                int size = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - d - terminator_size);
                DEBUG_ASSERT_RANGE(size, 1, buffer_size_ - MARGIN_SIZE * 2 - 1);
#ifdef YASKKSERV_DEBUG_PARANOIA
                int debug_size = copy_size;
                int tmp = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - (d + debug_size - 1) - terminator_size);
                DEBUG_ASSERT_RANGE(tmp, 1, buffer_size_ - MARGIN_SIZE * 2 - 1);
#endif  // YASKKSERV_DEBUG_PARANOIA
                copy_size = (copy_size < size) ? copy_size : size;
                overwrite_internal(p, d, size);
        }
#endif
/// 文字列を追加します。
/**
 * \attention
 * 文字列は \\0 で終端されます。
 */
        void appendFast(const SkkSimpleString &object)
        {
                appendFast(object.getBuffer());
        }

/// 文字列を追加します。
/**
 * \attention
 * 文字列は \\0 で終端されます。
 */
        void appendFast(const void *p)
        {
#ifdef YASKKSERV_DEBUG_PARANOIA
                const int terminator_size = 1;
                const int append_terminator_legth = 1;
                int tmp = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - (current_ + get_size_internal(p) + append_terminator_legth - 1) - terminator_size);
                DEBUG_ASSERT_RANGE(tmp, 0, buffer_size_ - MARGIN_SIZE * 2 - 1);
                DEBUG_ASSERT_RANGE(get_size_internal(p), 0, buffer_size_ - MARGIN_SIZE * 2 - 1);
#endif  // YASKKSERV_DEBUG_PARANOIA
                current_ = overwrite_internal(p, current_, getRemainSize());
                *current_ = '\0';
                update_string_size_from_current();
        }

/// 文字列を追加します。文字列 p は copy_size バイト分、または \\0 を見付けるまで追加されます。追加に失敗するならば偽を返します。
/**
 * \attention
 * 通常の append() と異なり、 p は \\0 で終端されている必要がありません。
 */
        bool append(const void *p, int copy_size)
        {
                if (getRemainSize() < copy_size)
                {
                        return false;
                }
                current_ = overwrite_internal(p, current_, copy_size);
                *current_ = '\0';
                update_string_size_from_current();
                return true;
        }

/// 文字を追加します。
        bool append(char c)
        {
                if (getRemainSize() < 1)
                {
                        return false;
                }
                if (c == '\0')
                {
                        *current_ = c;
                }
                else
                {
                        *current_++ = c;
                        *current_ = '\0';
                }
                update_string_size_from_current();
                return true;
        }

        void appendFast(char c)
        {
#ifdef YASKKSERV_DEBUG_PARANOIA
                const int terminator_size = 1;
                const int append_terminator_size = 1;
                int tmp = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - (current_ + 1 + append_terminator_size - 1) - terminator_size);
                DEBUG_ASSERT_RANGE(tmp, 0, buffer_size_ - MARGIN_SIZE * 2 - 1);
#endif  // YASKKSERV_DEBUG_PARANOIA
                if (c == '\0')
                {
                        *current_ = c;
                }
                else
                {
                        *current_++ = c;
                        *current_ = '\0';
                }
                update_string_size_from_current();
        }

/// scalar を 10 進文字列に変換して追加します。
/**
 * \attention
 * 文字列は \\0 で終端されます。
 */
        bool append(int scalar, Flag flag = FLAG_LEFT, int column = 11)
        {
#ifdef YASKKSERV_DEBUG_PARANOIA
                const int append_terminator_size = 1;
                DEBUG_ASSERT_RANGE(getRemainSize(), 11 + append_terminator_size, buffer_size_ - MARGIN_SIZE * 2 - 1);
#endif  // YASKKSERV_DEBUG_PARANOIA
                if (getRemainSize() <= 11 + 1)
                {
                        return false;
                }
                current_ = overwrite_internal(current_, scalar, flag, column);
                *current_ = '\0';
                update_string_size_from_current();
                return true;
        }

        bool append(float scalar, int decimal, Flag flag = FLAG_LEFT, int column = 11)
        {
#ifdef YASKKSERV_DEBUG_PARANOIA
                const int append_terminator_size = 1;
                DEBUG_ASSERT_RANGE(getRemainSize(), 11 + append_terminator_size, buffer_size_ - MARGIN_SIZE * 2 - 1);
#endif  // YASKKSERV_DEBUG_PARANOIA
                if (getRemainSize() <= 11 + 1)
                {
                        return false;
                }
                current_ = overwrite_internal(current_, scalar, decimal, flag, column);
                *current_ = '\0';
                update_string_size_from_current();
                return true;
        }

/// scalar を 16 進文字列に変換して追加します。
/**
 * \attention
 * 文字列は \\0 で終端されます。
 */
        bool appendHexadecimal(int scalar, int column = 8)
        {
#ifdef YASKKSERV_DEBUG_PARANOIA
                const int append_terminator_size = 1;
                DEBUG_ASSERT_RANGE(getRemainSize(), 8 + append_terminator_size, buffer_size_ - MARGIN_SIZE * 2);
#endif  // YASKKSERV_DEBUG_PARANOIA
                if (getRemainSize() <= 8 + 1)
                {
                        return false;
                }
                current_ = overwrite_internal_hexadecimal(current_, scalar, column);
                *current_ = '\0';
                update_string_size_from_current();
                return true;
        }

/// strncmp(3) と同様に文字列を比較します。
/**
 * 自分自身のオブジェクトの文字列が引数に比べて、小さければ -1 以下の整
 * 数、等しければ 0 そして大きければ 1 以上の整数を返します。
 */
        int compare(SkkSimpleString &object)
        {
                return compare_internal(getBuffer(), object.getBuffer());
        }

        int compare(const char *p)
        {
                return compare_internal(getBuffer(), p);
        }

        int compare(const char *p, int size)
        {
                return compare_internal(getBuffer(), p, size);
        }

/// 文字を 1 文字返します。
/**
 * index が範囲外ならば \\0 を返します。
 *
 * \attention
 * index が範囲外の場合はメモリに触れず、正当な処理として扱います。ア
 * サートしないことに注意が必要です。
 */
        char getCharacter(int index) const
        {
                if ((index >= getSize()) || (index < 0))
                {
                        return '\0';
                }
                return *(getBuffer() + index);
        }

        template<int index> char getCharacter() const
        {
                if (index == 0)
                {
                        return *getBuffer();
                }
                else
                {
                        return getCharacter(index);
                }
        }

/// 行末の文字が c と同じならば行末の文字を削除します。 repeat_flag が真ならば条件が成立する限り削除します。削除した文字数を返します。
/**
 * c が 0 の場合、改行コードを削除します。改行コードは \\r または \\n
 * となります。 DOS の改行コードなどを特別視しないので、確実に改行コー
 * ドを削除するには repeat_flag を真にする必要があります。
 */
        int chomp(char c = 0, bool repeat_flag = true)
        {
                int index = getSize() - 1;
                if (index < 0)
                {
                        return 0;
                }
                int result = 0;
                char *p = get_buffer();
                if (c == 0)
                {
                        if (repeat_flag)
                        {
                                while ((*(p + index) == '\r') || (*(p + index) == '\n'))
                                {
                                        *(p + index) = '\0';
                                        ++result;
                                        if (--index <= 0)
                                        {
                                                break;
                                        }
                                }
                        }
                        else
                        {
                                if ((*(p + index) == '\r') || (*(p + index) == '\n'))
                                {
                                        *(p + index) = '\0';
                                        ++result;
                                }
                        }
                }
                else
                {
                        if (repeat_flag)
                        {
                                while (*(p + index) == c)
                                {
                                        *(p + index) = '\0';
                                        ++result;
                                        if (--index <= 0)
                                        {
                                                break;
                                        }
                                }
                        }
                        else
                        {
                                if (*(p + index) == c)
                                {
                                        *(p + index) = '\0';
                                        ++result;
                                }
                        }
                }
                update_string_size_by_strlen();
                return result;
        }

/// base_size バイトの文字列 base が search_size バイトの search により開始されていれば真を返します。
/**
 * 終端コードとして ' ' または '\0' を認識します。
 */
        static bool startWith(const char *base, const char *search, int base_size, int search_size)
        {
                DEBUG_ASSERT_POINTER(base);
                DEBUG_ASSERT_POINTER(search);
                DEBUG_ASSERT(base_size > 0);
                DEBUG_ASSERT(search_size > 0);
                for (int i = 0; ; ++i)
                {
                        if (i >= search_size)
                        {
                                return true;
                        }
                        int s_2 = *(reinterpret_cast<const unsigned char*>(search + i));
                        if ((s_2 == ' ') || (s_2 == '\0'))
                        {
                                return true;
                        }
                        if (i >= base_size)
                        {
                                return false;
                        }
                        int s_1 = *(reinterpret_cast<const unsigned char*>(base + i));
                        if (s_1 != s_2)
                        {
                                return false;
                        }
                        if ((s_1 == ' ') || (s_1 == '\0'))
                        {
                                return false;
                        }
                }
                return false;   // NOTREACHED
        }

/// base_size バイトの文字列 base に c が含まれていれば真を返します。
/**
 * 終端コードとして ' ' または '\0' を認識します。
 */
        static bool search(const char *base, char c, int base_size)
        {
                DEBUG_ASSERT_POINTER(base);
                DEBUG_ASSERT(base_size > 0);
                for (int i = 0; ; ++i)
                {
                        if (i >= base_size)
                        {
                                return false;
                        }
                        int s_1 = *(reinterpret_cast<const unsigned char*>(base + i));
                        if (s_1 == c)
                        {
                                return true;
                        }
                        if ((s_1 == ' ') || (s_1 == '\0'))
                        {
                                return false;
                        }
                }
                return false;   // NOTREACHED
        }

/// strncmp(3) と同様に文字列を比較します。
/**
 * s_1 が s_2 に比べて、小さければ -1 以下の整数、等しければ 0 そして大
 * きければ 1 以上の整数を返します。
 */
        static int compare(const void *s_1, const void *s_2)
        {
                return compare_internal(s_1, s_2);
        }

        static int compare(const void *s_1, const void *s_2, int buffer_size)
        {
                return compare_internal(s_1, s_2, buffer_size);
        }

protected:
#ifdef SKK_SIMPLE_STRING_DEBUG_BUFFER
        char *debug_buffer_;
        const char *debug_write_buffer_start_;
#endif  // SKK_SIMPLE_STRING_DEBUG_BUFFER
        char *buffer_;
        char *current_;
        int buffer_size_;
        int string_size_;
        bool dynamic_allocation_flag_;
};
}

#endif // SKK_SIMPLE_STRING_HPP
