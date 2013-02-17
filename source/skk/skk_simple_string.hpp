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
#ifndef SKK_SIMPLE_STRING_HPP
#define SKK_SIMPLE_STRING_HPP

#include "skk_architecture.hpp"

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
 * <h1>Append と Overwrite</h1>
 *
 * Append は文字列末尾に追加し、 \\0 で終端します。 Overwrite は指定位
 * 置に文字列を上書きするだけです。このとき \\0 を上書きすることはあり
 * ません。つまり \\0 で終端されません。
 *
 * まとめると以下のようになります。
 *
 * <dl>
 * <dt>Append
 * <dd>文字列を追加。 \\0 を追加。
 * <dt>Overwrite
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

private:
        void update_string()
        {
                string_size_cache_ = 0;
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

public:
        enum Flag
        {
                FLAG_RIGHT,
                FLAG_RIGHT_ZERO,
                FLAG_LEFT,
        };

        virtual ~SkkSimpleString()
        {
                check_margin_for_debug();
                DEBUG_ASSERT_POINTER(buffer_);
                DEBUG_ASSERT(buffer_size_ != 0);
                if (dynamic_allocation_flag_)
                {
                        delete[] buffer_;
                }
        }

        SkkSimpleString(int buffer_size) :
                buffer_(new char[buffer_size]),
                current_(get_buffer()),
                buffer_size_(buffer_size),
                string_size_cache_(0),
                dynamic_allocation_flag_(true)
        {
                DEBUG_ASSERT(buffer_size_ != 0);
                initialize_buffer();
        }

        SkkSimpleString(void *buffer,
                        int buffer_size) :
                buffer_(static_cast<char*>(buffer)),
                current_(get_buffer()),
                buffer_size_(buffer_size),
                string_size_cache_(0),
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
                buffer_(array),
                current_(get_buffer()),
                buffer_size_(N),
                string_size_cache_(0),
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

/// 文字列長を返します。文字列長に終端の \\0 は含みません。
/**
 * \attention
 * 非常識に巨大なデータの場合、途中で計測を諦め 0 を返します。
 */
        int getSize()
        {
                if (string_size_cache_)
                {
                        return string_size_cache_;
                }
                else
                {
                        ptrdiff_t diff = current_ - get_buffer();
                        string_size_cache_ = static_cast<int>(diff) + getSize(current_);
                        DEBUG_ASSERT(getSize(get_buffer()) == string_size_cache_);
                        return string_size_cache_;
                }
        }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
        int getSize(int limit)
        {
                if (string_size_cache_)
                {
                        return string_size_cache_;
                }
                else
                {
                        ptrdiff_t diff = current_ - get_buffer();
                        string_size_cache_ = getSize(current_, static_cast<int>(diff));
                        string_size_cache_ += static_cast<int>(diff);
                        DEBUG_ASSERT(getSize(get_buffer(), limit) == string_size_cache_);
                        return string_size_cache_;
                }
        }
#pragma GCC diagnostic pop

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
                update_string();
        }

/// size バイトのデータが Append() 可能ならば真を返します。
        bool isAppendSize(int size)
        {
                const int terminator_size = 1;
                int remain_size = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - current_ - terminator_size);
                if (remain_size > size)
                {
                        return true;
                }
                else
                {
                        return false;
                }
        }

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
                if (index <= getSize())
                {
                        DEBUG_ASSERT(0);
                }
                else
                {
                        overwrite(c,
                                  d);
                        update_string();
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
                overwrite(p, d, size);
                update_string();
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
                overwrite(p, d, size);
                update_string();
        }

/// 文字列を追加します。
/**
 * \attention
 * 文字列は \\0 で終端されます。
 */
        void append(const SkkSimpleString &object)
        {
                append(object.getBuffer());
        }

/// 文字列を追加します。
/**
 * \attention
 * 文字列は \\0 で終端されます。
 */
        void append(const void *p)
        {
                const int terminator_size = 1;
                int size = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - current_ - terminator_size);
                DEBUG_ASSERT((size >= 1) && (size < buffer_size_ - MARGIN_SIZE * 2));
#ifdef YASKKSERV_DEBUG_PARANOIA
                const int append_terminator_legth = 1;
                int tmp = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - (current_ + getSize(p) + append_terminator_legth - 1) - terminator_size);
                DEBUG_ASSERT_RANGE(tmp, 1, buffer_size_ - MARGIN_SIZE * 2 - 1);
                DEBUG_ASSERT_RANGE(getSize(p), 0, buffer_size_ - MARGIN_SIZE * 2 - 1);
#endif  // YASKKSERV_DEBUG_PARANOIA
                current_ = overwrite(p, current_, size);
                *current_ = '\0';
                update_string();
        }

/// 文字列を追加します。文字列 p は copy_size バイト分、または \\0 を見付けるまで追加されます。
/**
 * \attention
 * 通常の append() と異なり、 p は \\0 で終端されている必要がありません。
 */
        void append(const void *p, int copy_size)
        {
                const int terminator_size = 1;
                int size = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - current_ - terminator_size);
                DEBUG_ASSERT((size >= 1) && (size < buffer_size_ - MARGIN_SIZE * 2));
#ifdef YASKKSERV_DEBUG_PARANOIA
                const int append_terminator_legth = 1;
                int debug_size = copy_size;
                int tmp = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - (current_ + debug_size + append_terminator_legth - 1) - terminator_size);
                DEBUG_ASSERT_RANGE(tmp, 1, buffer_size_ - MARGIN_SIZE * 2 - 1);
                DEBUG_ASSERT_RANGE(debug_size, 0, buffer_size_ - MARGIN_SIZE * 2 - 1);
#endif  // YASKKSERV_DEBUG_PARANOIA
                copy_size = (copy_size < size) ? copy_size : size;
                current_ = overwrite(p, current_, copy_size);
                *current_ = '\0';
                update_string();
        }

/// 文字を追加します。
/**
 * \attention
 * 文字列は \\0 で終端されます。
 */
        void append(char c)
        {
#ifdef YASKKSERV_DEBUG_PARANOIA
                const int terminator_size = 1;
                int size = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - current_ - terminator_size);
                DEBUG_ASSERT((size >= 1) && (size < buffer_size_ - MARGIN_SIZE * 2));
                const int append_terminator_size = 1;
                const int tmp_size = 1;
                int tmp = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - (current_ + 1 + append_terminator_size - 1) - terminator_size);
                DEBUG_ASSERT_RANGE(tmp, 1, buffer_size_ - MARGIN_SIZE * 2 - 1);
                DEBUG_ASSERT_RANGE(tmp_size, 1, buffer_size_ - MARGIN_SIZE * 2 - 1);
#endif  // YASKKSERV_DEBUG_PARANOIA
                *current_++ = c;
                *current_ = '\0';
                update_string();
        }

/// scalar を 10 進文字列に変換して追加します。
/**
 * \attention
 * 文字列は \\0 で終端されます。
 */
        void append(int scalar, Flag flag = FLAG_LEFT, int column = 11)
        {
#ifdef YASKKSERV_DEBUG_PARANOIA
                const int terminator_size = 1;
                int size = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - current_ - terminator_size);
                const int append_terminator_size = 1;
                DEBUG_ASSERT_RANGE(size, 11 + append_terminator_size, buffer_size_ - MARGIN_SIZE * 2 - 1);
#endif  // YASKKSERV_DEBUG_PARANOIA
                current_ = overwrite(current_, scalar, flag, column);
                *current_ = '\0';
                update_string();
        }

        void append(float scalar, int decimal, Flag flag = FLAG_LEFT, int column = 11)
        {
#ifdef YASKKSERV_DEBUG_PARANOIA
                const int terminator_size = 1;
                int size = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - current_ - terminator_size);
                const int append_terminator_size = 1;
                DEBUG_ASSERT_RANGE(size, 11 + append_terminator_size, buffer_size_ - MARGIN_SIZE * 2 - 1);
#endif  // YASKKSERV_DEBUG_PARANOIA
                current_ = overwrite(current_, scalar, decimal, flag, column);
                *current_ = '\0';
                update_string();
        }

/// scalar を 16 進文字列に変換して追加します。
/**
 * \attention
 * 文字列は \\0 で終端されます。
 */
        void appendHexadecimal(int scalar, int column = 8)
        {
#ifdef YASKKSERV_DEBUG_PARANOIA
                const int terminator_size = 1;
                int size = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - current_ - terminator_size);
                const int append_terminator_size = 1;
                DEBUG_ASSERT_RANGE(size, 8 + append_terminator_size, buffer_size_ - MARGIN_SIZE * 2);
#endif  // YASKKSERV_DEBUG_PARANOIA
                current_ = overwriteHexadecimal(current_, scalar, column);
                *current_ = '\0';
                update_string();
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
 * 範囲外アクセスは正当な処理として扱い、アサートしないことに注意が必要
 * です。
 */
        char getCharacter(int index)
        {
                if ((index >= getSize()) || (index < 0))
                {
                        return '\0';
                }
                return *(getBuffer() + index);
        }

        template<int index> char getCharacter()
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
                update_string();
                return result;
        }

/// 文字列のバイトサイズを返します。サイズに終端の \\0 は含みません。
/**
 * \attention
 * limit 以上の巨大なデータの場合、途中で計測を諦め 0 を返します。この
 * ときアサートが有効ならばアサートします。
 */
        static int getSize(const void *p, int limit = 64 * 1024)
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
        static void overwrite(char c, void *destination)
        {
                char *p = static_cast<char*>(destination);
                *p = c;
        }

/// 文字列をコピーします。文字列の最後 + 1 を指すポインタを返します。
/**
 * \attention
 * \\0 はコピーされません。
 */
        static char *overwrite(const void *source, void *destination, int size)
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
        static char *overwrite(void *destination, int scalar, Flag flag = FLAG_LEFT, int column = 11)
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
        static char *overwrite(void *destination, float scalar, int decimal = 1, Flag flag = FLAG_LEFT, int column = 11)
        {
                DEBUG_ASSERT_RANGE(column, 2, 11);
                DEBUG_ASSERT_RANGE(column - 1 - decimal, 1, 9);
                char *p = static_cast<char*>(destination);
                p = overwrite(p, static_cast<int>(scalar), flag, column - 1 - decimal);
                *p++ = '.';
                int po = 1;
                for (int i = 0; i != decimal; ++i)
                {
                        po *= 10;
                }
                p = overwrite(p, static_cast<int>(scalar * static_cast<float>(po)) % po, FLAG_RIGHT_ZERO, decimal);
                return p;
        }

/// column 桁の 16 進文字列を生成します。文字列の最後 + 1 を指すポインタを返します。
/**
 * \attention
 * column より多くの桁は表示されません。この場合、上位の桁は削られます。
 */
        static char *overwriteHexadecimal(void *destination, int scalar, int column = 8)
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

private:
        char *buffer_;
        char *current_;
        int buffer_size_;
        int string_size_cache_;
        bool dynamic_allocation_flag_;
};
}

#endif // SKK_SIMPLE_STRING_HPP
