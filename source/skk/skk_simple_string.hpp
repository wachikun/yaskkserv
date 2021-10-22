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
/// ñ���ʸ���󥯥饹�Ǥ���
/**
 * ���󥹥ȥ饯���ǻ��ꤷ���Хåե���ʸ������ɲä�����Ǵ������ޤ���
 *
 * �Хåե��ϰʲ��Τ褦�ʹ�¤�ˤʤäƤ��ޤ���
 *
 *<pre>
+--------+----------------+--------+
| MARGIN | ʸ����Хåե� | MARGIN |
+--------+----------------+--------+</pre>
 *
 * MARGIN �ϥǥХå��ʤɤ˻��Ѥ����ΰ�Ǥ������Τ��᰷�����ȤΤǤ���ʸ
 * ����Ĺ�ϡ����󥹥ȥ饯���ǻ��ꤷ���Хåե�����ʬ�����ʤ�ΤȤʤ��
 * ����
 *
 * <h1>append, appendFast() �� overwrite</h1>
 *
 * append ��ʸ�����������ɲä��� \\0 �ǽ�ü���ޤ����Хåե��˼��ޤ뤫
 * �ɤ���������å����ơ��ɲäǤ��ʤ���в��⤻�������֤��ޤ���
 * appendFast �� append ��Ʊ�ͤǤ������ɲò�ǽ���ɤ����򤷤ޤ���
 * isAppendSize() �ʤɤǻ����˸��ڤ���ɬ�פ�����ޤ���
 *
 * overwrite �ϻ�����֤�ʸ������񤭤�������Ǥ������ΤȤ� \\0 ���
 * �񤭤��뤳�ȤϤ���ޤ��󡣤Ĥޤ� \\0 �ǽ�ü����ޤ���
 *
 * �ޤȤ��Ȱʲ��Τ褦�ˤʤ�ޤ���
 *
 * <dl>
 * <dt>append
 * <dd>ʸ������ɲá� \\0 ���ɲá��ɲå����å����ꡣ
 * <dt>appendFast
 * <dd>ʸ������ɲá� \\0 ���ɲá��ɲå����å��ʤ��� isAppendSize() �ʤɤǻ����˥����å���ɬ�ס�
 * <dt>overwrite
 * <dd>ʸ������񤭡� \\0 �˿���ʤ���
 * </dl>
 *
 *
 * <h1>ʸ����Хåե� �� ������ MARGIN</h1>
 *
 * �̾�ʸ����ü�� \\0 ��ʸ����Хåե�����֤���ޤ���
 * DEBUG_PARANOIA_ASSERT() �Ǥ�ʸ����Хåե���˼��ޤ�ʤ���祢������
 * ���ޤ������ξ�ǡ������� MARGIN ����Ƭ 1 �Х��Ȥ� 0 �Ǥ��뤳�Ȥ��ݾ�
 * ���ޤ���
 *
 * �Ĥޤꡢ�̾�Ͻ�ü�Ǥ��� \\0 ��ʸ����Хåե����֤���ʤ���Фʤ��
 * ���󤬡������� \\0 ��ʸ����Хåե��˼��ޤ꤭��ʤ��ä���� (�̾��
 * ���ꤨ�ޤ���) �Ǥ������ MARGIN ����Ƭ�� 0 �Ǥ��뤳�Ȥ��顢ɬ��ʸ
 * ����Ͻ�ü����뤳�Ȥ��ݾڤ���ޤ���
 *
 *
 * <h1>���󥹥ȥ饯��</h1>
 *
 * �Хåե��ؤΥݥ��󥿤��Ϥ���硢�ݥ��󥿤� 0 �ΤȤ��������Ȥ�ͭ����
 * ��Х������Ȥ��ޤ���
 *
 *
 * <h1>����¾�᥽�åɤδ���ư��</h1>
 *
 * �����ʤɤ������ʾ�硢�������Ȥ�ͭ���ʤ�Х������Ȥ��ޤ���
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

                // ʸ������Ƭ�� 0 �ˡ�
                *get_buffer() = '\0';
                // ��ޡ��������Ƭ�� 0 �ˤ���ʸ�����ΰ褬���դξ���
                // ��ü����뤳�Ȥ��ݾڤ��ޤ���
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

/// ʸ����ΥХ��ȥ��������֤��ޤ����������˽�ü�� \\0 �ϴޤߤޤ���
/**
 * \attention
 * limit �ʾ�ε���ʥǡ����ξ�硢����Ƿ�¬������ 0 ���֤��ޤ�������
 * �Ȥ��������Ȥ�ͭ���ʤ�Х������Ȥ��ޤ���
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

/// ʸ���򥳥ԡ����ޤ���
/**
 * \attention
 * c �� \\0 ����ꤷ�ƽ񤭹��ߡ�ʸ��������Ǥ��뤳�Ȥ��Ǥ��ޤ���
 */
        static void overwrite_internal(char c, void *destination)
        {
                char *p = static_cast<char*>(destination);
                *p = c;
        }

/// ʸ����򥳥ԡ����ޤ���ʸ����κǸ� + 1 ��ؤ��ݥ��󥿤��֤��ޤ���
/**
 * \attention
 * \\0 �ϥ��ԡ�����ޤ���
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

/// column ��� 10 ��ʸ������������ޤ���ʸ����κǸ� + 1 ��ؤ��ݥ��󥿤��֤��ޤ���
/**
 * ���� column �˴ޤޤ�ޤ���
 *
 * \attention
 * column ���¿���η��ɽ������ޤ��󡣤��ξ�硢��̤η�Ϻ���ޤ���
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

/// column ��� 10 ��ʸ������������ޤ���ʸ����κǸ� + 1 ��ؤ��ݥ��󥿤��֤��ޤ���
/**
 * ���ȥɥåȤ� column �˴ޤޤ�ޤ���
 *
 * \attention
 * column ���¿���η��ɽ������ޤ��󡣤��ξ�硢��̤η�Ϻ���ޤ���
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

/// column ��� 16 ��ʸ������������ޤ���ʸ����κǸ� + 1 ��ؤ��ݥ��󥿤��֤��ޤ���
/**
 * \attention
 * column ���¿���η��ɽ������ޤ��󡣤��ξ�硢��̤η�Ϻ���ޤ���
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

/// ���� append() �ϥ᥽�åɤΥ���ǥå��������ꤷ�ޤ���
/**
 * \attention
 * �Хåե���Ȥ��ޤ魯���� setAppendIndex(0) �Ȥ����� reset() ���Ѥ�
 * �Ƥ���������
 *
 * \attention
 * ������ index ����ꤷ�����ϲ��⤻����ȴ���ޤ����ǥХå��ӥ�ɤǤ�
 * �������Ȥ��ޤ���
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

/// ���֤�ꥻ�åȤ���ʸ��������ꤷ�ޤ���
/**
 * �Хåե���Ȥ��ޤ路��ʣ����ʸ������ۤ��뤿��˻Ȥ��ޤ���
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

/// ʸ����Хåե��ؤΥݥ��󥿤��֤��ޤ���
/**
 * ��������ʸ����ϡ��ܥ᥽�åɤǼ�������ɬ�פ�����ޤ���
 */
        const char *getBuffer() const
        {
                return get_buffer();
        }

/// ������ʸ����Хåե��ؤΥݥ��󥿤��֤��ޤ���
/**
 * ����� append() �Ϥ��ΰ��֤ؼ¹Ԥ���ޤ���
 */
        const char *getCurrentBuffer() const
        {
                return current_;
        }

/// �����Хåե���ľ�ܽ񤭹���ľ���˽񤭹��ޤ���ΰ��������ޤ���ɬ���ڥ��Ȥʤ� endWriteBuffer() ��Ƥ�ɬ�פ�����ޤ����������������ΰ��ؤ��Ƥ���е����֤��ޤ����ǥХå��ӥ�ɤǤϥǡ�������ڥ��Ѥ˥ǡ�����Хå����åפ��ޤ���
/**
 * \attention
 * �������ΰ��ؤ��Ƥ�����硢�ǥХå��ӥ�ɤǤϥ������Ȥ��ޤ���
 */
        bool beginWriteBuffer(const char *start, int reserve_size)
        {
                const char *p = getBuffer();
#ifdef SKK_SIMPLE_STRING_DEBUG_BUFFER
                DEBUG_ASSERT(debug_write_buffer_start_ == 0);
                // �Хåե���ޤ뤴�ȥ��ԡ�
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

/// �����Хåե���ľ�ܽ񤭹�����ΰ�����ꤷ�ޤ����ǥХå��ӥ�ɤǤϥǡ�������ڥ����������������ä����ϥ������Ȥ��ޤ���
/**
 * \attention
 * ��꡼���ӥ�ɤǤϲ��⤷�ޤ�������ͤ��֤��ޤ��󡣤���ϡ����������оݤ���̿Ū�ʥ��顼�Ǥ��뤿��Ǥ���
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
                // �Хåե����ΰ賰����Ƭ�ն�����
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
                // �Хåե����ΰ賰����ü�ն�����
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

/// ͭ���ʥХåե����������֤��ޤ���
/**
 * MARGIN ��������Хåե����������֤��ޤ���
 */
        int getValidBufferSize() const
        {
                return buffer_size_ - MARGIN_SIZE * 2;
        }

/// ʸ����Ĺ���֤��ޤ���ʸ����Ĺ�˽�ü�� \\0 �ϴޤߤޤ���
        int getSize() const
        {
                return string_size_;
        }

/// �Хåե����ɲäǤ��� \\0 ��������Х��ȿ����֤��ޤ�������ͤϼºݤ����̤��� \\0 ��ʬ������ 1 �Х��Ⱦ��ʤ��ʤ�ޤ���
        int getRemainSize() const
        {
                const int terminator_size = 1;
                int remain_size = static_cast<int>(buffer_ + buffer_size_ - MARGIN_SIZE - current_ - terminator_size);
                DEBUG_ASSERT_RANGE(remain_size, 0, buffer_size_ - MARGIN_SIZE * 2 - 1);
                return remain_size;
        }

/// ���ꤷ���ͤ�ʸ����Хåե��ν�üľ���ޤ����ޤ���
/**
 * \attention
 * ʸ����Хåե��ν�ü�ؤϾ�� \\0 ���񤭹��ޤ�ޤ���
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

/// '\0' ����� size �Х��ȤΥǡ����� append() ��ǽ�ʤ�п����֤��ޤ���
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

/// ������֤�ʸ��������Ǥ��ޤ���
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
/// ������֤�ʸ�����񤭤��ޤ���
/**
 * \attention
 * c �� \\0 ����ꤷ�ƽ񤭹��ߡ�ʸ��������Ǥ��뤳�Ȥ��Ǥ��ޤ��� index
 * �����ߤ�ʸ������˼��ޤ�ʤ����ϲ��⤷�ޤ��󡣥������Ȥ�ͭ���ʤ��
 * �������Ȥ��ޤ���
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

/// ������֤�ʸ������񤭤��ޤ���
/**
 * \attention
 * ����ʸ���� p ¦�� \\0 �ϥ��ԡ�����ޤ���
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

/// ������֤�ʸ������񤭤��ޤ���ʸ���� p �� copy_size �Х���ʬ���ޤ��� \\0 ���դ���ޤ��ɲä���ޤ���
/**
 * \attention
 * ����ʸ���� p ¦�� \\0 �ϥ��ԡ�����ޤ���
 *
 * �̾�� overwrite() �Ȱۤʤꡢ p �� \\0 �ǽ�ü����Ƥ���ɬ�פ�����ޤ���
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
/// ʸ������ɲä��ޤ���
/**
 * \attention
 * ʸ����� \\0 �ǽ�ü����ޤ���
 */
        void appendFast(const SkkSimpleString &object)
        {
                appendFast(object.getBuffer());
        }

/// ʸ������ɲä��ޤ���
/**
 * \attention
 * ʸ����� \\0 �ǽ�ü����ޤ���
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

/// ʸ������ɲä��ޤ���ʸ���� p �� copy_size �Х���ʬ���ޤ��� \\0 ���դ���ޤ��ɲä���ޤ����ɲä˼��Ԥ���ʤ�е����֤��ޤ���
/**
 * \attention
 * �̾�� append() �Ȱۤʤꡢ p �� \\0 �ǽ�ü����Ƥ���ɬ�פ�����ޤ���
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

/// ʸ�����ɲä��ޤ���
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

/// scalar �� 10 ��ʸ������Ѵ������ɲä��ޤ���
/**
 * \attention
 * ʸ����� \\0 �ǽ�ü����ޤ���
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

/// scalar �� 16 ��ʸ������Ѵ������ɲä��ޤ���
/**
 * \attention
 * ʸ����� \\0 �ǽ�ü����ޤ���
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

/// strncmp(3) ��Ʊ�ͤ�ʸ�������Ӥ��ޤ���
/**
 * ��ʬ���ȤΥ��֥������Ȥ�ʸ���󤬰�������٤ơ���������� -1 �ʲ�����
 * ������������� 0 �������礭����� 1 �ʾ���������֤��ޤ���
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

/// ʸ���� 1 ʸ���֤��ޤ���
/**
 * index ���ϰϳ��ʤ�� \\0 ���֤��ޤ���
 *
 * \attention
 * index ���ϰϳ��ξ��ϥ���˿��줺�������ʽ����Ȥ��ư����ޤ�����
 * �����Ȥ��ʤ����Ȥ���դ�ɬ�פǤ���
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

/// ������ʸ���� c ��Ʊ���ʤ�й�����ʸ���������ޤ��� repeat_flag �����ʤ�о�郎��Ω����¤������ޤ����������ʸ�������֤��ޤ���
/**
 * c �� 0 �ξ�硢���ԥ����ɤ������ޤ������ԥ����ɤ� \\r �ޤ��� \\n
 * �Ȥʤ�ޤ��� DOS �β��ԥ����ɤʤɤ����̻뤷�ʤ��Τǡ��μ¤˲��ԥ���
 * �ɤ�������ˤ� repeat_flag �򿿤ˤ���ɬ�פ�����ޤ���
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

/// base_size �Х��Ȥ�ʸ���� base �� search_size �Х��Ȥ� search �ˤ�곫�Ϥ���Ƥ���п����֤��ޤ���
/**
 * ��ü�����ɤȤ��� ' ' �ޤ��� '\0' ��ǧ�����ޤ���
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

/// base_size �Х��Ȥ�ʸ���� base �� c ���ޤޤ�Ƥ���п����֤��ޤ���
/**
 * ��ü�����ɤȤ��� ' ' �ޤ��� '\0' ��ǧ�����ޤ���
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

/// strncmp(3) ��Ʊ�ͤ�ʸ�������Ӥ��ޤ���
/**
 * s_1 �� s_2 ����٤ơ���������� -1 �ʲ������������������ 0 ��������
 * ������� 1 �ʾ���������֤��ޤ���
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
