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
#ifndef SKK_UTILITY_HPP
#define SKK_UTILITY_HPP

#include "skk_architecture.hpp"
#include "skk_utility_architecture.hpp"
#include "skk_simple_string.hpp"

namespace YaSkkServ
{
namespace SkkUtility
{
inline int getStringOkuriAriLength()
{
        const char data[] = ";; okuri-ari entries.\n";
        return sizeof(data);
}

inline int getStringOkuriNasiLength()
{
        const char data[] = ";; okuri-nasi entries.\n";
        return sizeof(data);
}

/// ";; okuri-ari entries.\n" �Ȥ���ʸ����˰��פ���п����֤��ޤ���
inline bool isStringOkuriAri(const char *p)
{
        DEBUG_ASSERT_POINTER(p);
        const char data[] = ";; okuri-ari entries.\n";
        for (int i = 0; i != sizeof(data) - 1; ++i)
        {
                if (*(p + i) != data[i])
                {
                        return false;
                }
        }
        return true;
}

/// ";; okuri-nasi entries.\n" �Ȥ���ʸ����˰��פ���п����֤��ޤ���
inline bool isStringOkuriNasi(const char *p)
{
        DEBUG_ASSERT_POINTER(p);
        const char data[] = ";; okuri-nasi entries.\n";
        for (int i = 0; i != sizeof(data) - 1; ++i)
        {
                if (*(p + i) != data[i])
                {
                        return false;
                }
        }
        return true;
}

enum
{
// ���Ф��Ϻ���Ǥ� 510 + �� (+�� �� \\1 �����ڡ����佪ü�����ɤʤ�) ��
// ���ȤǤ��뤳�Ȥ��θ���Ƥ��ޤ���
        ENCODED_MIDASI_BUFFER_SIZE = 512 + 16,
        MIDASI_DECODE_HIRAGANA_BUFFER_SIZE = 1024
};

/// ���ڡ����ޤ��� \\0 �ǥ����ߥ͡��Ȥ��줿 source �򥨥󥳡��ɤ��ޤ������󥳡��ɸ�ΥХ��ȥ��������֤��ޤ������󥳡��ɤ��줿ʸ����ϥ����ߥ͡��Ȥ���ޤ��󡣥��󥳡��ɤ˼��Ԥ������� 0 ���֤��ޤ���
inline int encodeHiragana(const char *source, char *destination, int size)
{
        DEBUG_ASSERT_POINTER(source);
        DEBUG_ASSERT_POINTER(destination);
        int result = 0;
        for (int i = 0; i < size;)
        {
                int c = *(source + i) & 0xff;
                if ((c == ' ') || (c == '\0'))
                {
                        return result;
                }
                if (c == 0xa4)
                {
                        if (i + 1 >= size)
                        {
                                return 0;
                        }
                        *(destination + result) = *(source + i + 1);
                        i += 2;
                        ++result;
                }
                else if ((c >= 0x21) && (c <= 0x7e))
                {
                        *(destination + result) = static_cast<char>(c);
                        ++i;
                        ++result;
                }
                else
                {
                        return 0;
                }
        }
        return 0;
}

/// ' ' (���ڡ���) �ޤ��� '\\0' �ǥ����ߥ͡��Ȥ��줿���󥳡��ɤ��줿ʸ���� source ��ǥ����ɤ��ޤ����ǥ����ɸ�ΥХ��ȥ��������֤��ޤ����ǥ����ɸ��ʸ����ϥ����ߥ͡��Ȥ���ޤ������Ϥ����󥳡��ɤ���Ƥ��ʤ�ʸ�����������ʸ�����ޤޤ�Ƥ������� 0 ���֤��ޤ���
inline int decodeHiragana(const char *source, char *destination, int size)
{
        DEBUG_ASSERT_POINTER(source);
        DEBUG_ASSERT_POINTER(destination);
        if (*source == '\1')
        {
                return 0;
        }
        int result = 0;
        for (int i = 0; i < size; ++i)
        {
                int c = *(source + i) & 0xff;
                if ((c == ' ') || (c == '\0'))
                {
                        return result;
                }
                if (c & 0x80)
                {
                        *(destination + result++) = static_cast<char>(0xa4);
                        *(destination + result++) = static_cast<char>(c);
                }
                else if ((c >= 0x21) && (c <= 0x7e))
                {
                        *(destination + result++) = static_cast<char>(c);
                }
                else
                {
                        return 0;
                }
        }
        return 0;
}

/// ʸ���󤬡����ꤢ��פʤ�п����֤��ޤ���ʸ����� '\\0' �� ' ' �ǽ�ü�����ΤȤ��ޤ���������ʸ����ξ�絶���֤��ޤ��� DEBUG_ASSERT ��ͭ���ʤ�Х������Ȥ��ޤ���
/**
 * �����ꤢ��פȤʤ���ϰʲ��� (1.1. || 1.2.) && (2.) �����ξ��Ǥ���
 *
 *    1.1. 1 ʸ���ܤ� '>' or '#' �� 2 ʸ���ܤ����ܸ�
 *    1.2. 1 ʸ���ܤ����ܸ�
 *
 *    2.   ������ a-z
 */
inline bool isOkuriAri(const char *p, int size)
{
        DEBUG_ASSERT_POINTER(p);
        DEBUG_ASSERT(size >= 1);
        int i = 1;
        int before_c = *reinterpret_cast<const unsigned char*>(p);
        if ((before_c == '>') || (before_c == '#'))
        {
                ++i;
                before_c = *reinterpret_cast<const unsigned char*>(p + 1);
        }
        if ((before_c & 0x80) == 0)
        {
                return false;
        }
        for (; i != size; ++i)
        {
                int c = *reinterpret_cast<const unsigned char*>(p + i);
                if ((c == '\0') || (c == ' '))
                {
                        break;
                }
                before_c = c;
        }
        if (before_c & 0x80)
        {
                return false;
        }
        else
        {
                if ((before_c >= 'a') && (before_c <= 'z'))
                {
                        return true;
                }
                else
                {
                        return false;
                }
        }
}

/// ʸ���󤬡�����ʤ��פޤ��ϡ�abbrev�פʤ�п����֤��ޤ���ʸ����� '\\0' �� ' ' �ǽ�ü�����ΤȤ��ޤ���������ʸ����ξ�絶���֤��ޤ����� DEBUG_ASSERT ��ͭ���ʤ�Х������Ȥ��ޤ���
inline bool isOkuriNasiOrAbbrev(const char *p, int size)
{
        DEBUG_ASSERT_POINTER(p);
        DEBUG_ASSERT(size >= 1);
        return !isOkuriAri(p, size);
}


/// 1 �Ը�ι�Ƭ�ؤΥ���ǥå������֤��ޤ����Хåե���ü�˺����ݤ������ -1 ���֤��ޤ���â�������Ԥˤ��б����Ƥ��餺���Ԥ�ɬ�� 1 ʸ���ʾ��ʸ����ޤ�Ǥ���ɬ�פ����뤳�Ȥ���դ�ɬ�פǤ���
inline int getNextLineIndex(const char *p, int index, int size)
{
        DEBUG_ASSERT_POINTER(p);
        DEBUG_ASSERT(size > 0);
        while (*(p + index) != '\n')
        {
                ++index;
                if (index >= size)
                {
                        return -1;
                }
        }
        ++index;
        if (index >= size)
        {
                return -1;
        }
        return index;
}

/// 1 �����ι�Ƭ�ؤΥ���ǥå������֤��ޤ����Хåե���ü�˺����ݤ������ -1 ���֤��ޤ����Хåե���Ƭ�˺����ݤ������ 0 ���֤��ޤ���â�������Ԥˤ��б����Ƥ��餺���Ԥ�ɬ�� 1 ʸ���ʾ��ʸ����ޤ�Ǥ���ɬ�פ����뤳�Ȥ���դ�ɬ�פǤ���
inline int getPreviousLineIndex(const char *p, int index, int size)
{
        DEBUG_ASSERT_POINTER(p);
        DEBUG_ASSERT(size > 0);
        while (*(p + index) != '\n')
        {
                --index;
                if (index <= 0)
                {
                        return 0;
                }
        }
        --index;
        if (index <= 0)
        {
                return 0;
        }
        while (*(p + index) != '\n')
        {
                --index;
                if (index <= 0)
                {
                        return 0;
                }
        }
        ++index;
        if (index >= size)
        {
                return -1;
        }
        return index;
}

/// ��Ƭ�ؤΥ���ǥå������֤��ޤ����Хåե���ü�˺����ݤ������ -1 ���֤��ޤ����Хåե���Ƭ�˺����ݤ������ 0 ���֤��ޤ���â�������Ԥˤ��б����Ƥ��餺���Ԥ�ɬ�� 1 ʸ���ʾ��ʸ����ޤ�Ǥ���ɬ�פ����뤳�Ȥ���դ�ɬ�פǤ���
inline int getBeginningOfLineIndex(const char *p, int index, int size)
{
        DEBUG_ASSERT_POINTER(p);
        DEBUG_ASSERT(size > 0);
        if  (*(p + index) == '\n')
        {
                --index;
                if (index <= 0)
                {
                        return 0;
                }
        }
        while (*(p + index) != '\n')
        {
                --index;
                if (index <= 0)
                {
                        return 0;
                }
        }
        ++index;
        if (index >= size)
        {
                return -1;
        }
        return index;
}

/// index ����Ƭ���֤ˤ���Ȥ��ơ� destination �ءָ��Ф��פ򥳥ԡ����ޤ������ԡ�����ʸ����� \\0 �ǥ����ߥ͡��Ȥ���ޤ��������ߥ͡�����ޤޤʤ����ԡ������Х��ȿ����֤��ޤ������ԡ��˼��Ԥ������� 0 ���֤��ޤ���
/**
 * ���ԡ��˼��Ԥ������ DEBUG_ASSERT ��ͭ���ʤ�Х������Ȥ��ޤ���
 */
inline int copyMidasi(const char *p, int index, int size, char *destination, int destination_size)
{
        DEBUG_ASSERT_POINTER(p);
        DEBUG_ASSERT_POINTER(destination);
        DEBUG_ASSERT(size > 0);
        DEBUG_ASSERT(destination_size > 0);
        const int margin = 8;
        for (int i = 0; i < destination_size - margin; ++i)
        {
                if  ((index + i >= size) || (i >= destination_size - margin))
                {
                        DEBUG_PRINTF("index=%d  size=%d  destination_size=%d\n",
                                     index,
                                     size,
                                     destination_size);
                        DEBUG_ASSERT(0);
                        break;
                }
                if (*(p + index + i) == ' ')
                {
                        *(destination + i) = '\0';
                        return i;
                }
                *(destination + i) = *(p + index + i);
        }
        DEBUG_ASSERT(0);
        return 0;
}

/// index ����Ƭ���֤ˤ���Ȥ��ơ� destination �ء��Ѵ�ʸ����פ򥳥ԡ����ޤ������ԡ�����ʸ����� \\0 �ǥ����ߥ͡��Ȥ���ޤ�������ʸ���ϥ��ԡ�����ޤ��󡣥����ߥ͡�����ޤޤʤ����ԡ������Х��ȿ����֤��ޤ������ԡ��˼��Ԥ������� 0 ���֤��ޤ���
/**
 * ž����Хåե���ۤ�������ž��������Ƥ�����Ǥ��� DEBUG_ASSERT ��
 * ͭ���ʤ�Х������Ȥ��ޤ���
 */
inline int copyHenkanmojiretsu(const char *p, int index, int size, char *destination, int destination_size)
{
        DEBUG_ASSERT_POINTER(p);
        DEBUG_ASSERT_POINTER(destination);
        DEBUG_ASSERT(size > 0);
        DEBUG_ASSERT(destination_size > 0);
        const int margin = 8;
        for (;;)
        {
                if (index >= size)
                {
                        DEBUG_ASSERT(0);
                        return 0;
                }
                if (*(p + index) == ' ')
                {
                        ++index;
                        break;
                }
                ++index;
        }
        for (int i = 0; i < destination_size - margin; ++i)
        {
                if  (index + i >= size)
                {
                        DEBUG_ASSERT(0);
                        break;
                }
                if (*(p + index + i) == '\n')
                {
                        *(destination + i) = '\0';
                        return i;
                }
                *(destination + i) = *(p + index + i);
        }
        DEBUG_ASSERT(0);
        return 0;
}

/// index ����Ƭ���֤ˤ���Ȥ��ơ����Ѵ�ʸ����פΥݥ��󥿤��֤��ޤ���
inline const char *getHenkanmojiretsuPointer(const char *p, int index, int size)
{
        for (;;)
        {
                if (index >= size)
                {
                        DEBUG_ASSERT(0);
                        return 0;
                }
                if (*(p + index) == ' ')
                {
                        ++index;
                        break;
                }
                ++index;
        }
        return p + index;
}

/// ���Ѵ�ʸ�������� candidate �θĿ����֤��ޤ������Ѵ�ʸ����פν�ü�����ɤȤ��� '\\0' �ޤ��� '\\n' ��ǧ�����ޤ���
/**
 *
 * �ߤ��� /candidate0/candidate1/
 *        ^
 *        |
 *        p �ϡ��Ѵ�ʸ����פ���Ƭ����ꤷ�ޤ���
 */
inline int getCandidateLength(const char *henkanmojiretsu)
{
        DEBUG_ASSERT_POINTER(henkanmojiretsu);
        DEBUG_ASSERT(*(henkanmojiretsu + 0) == '/');
        int entries = 0;
// /entry0/entry1/entry2/
        for (;;)
        {
                char c = *henkanmojiretsu++;
                if ((c == '\n') || (c == '\0'))
                {
                        if (entries > 0)
                        {
                                --entries; // ��üʬ����
                        }
                        break;
                }
                if (c == '/')
                {
                        ++entries;
                }
        }
        return entries;
}

/// index ���ܤ� candidate �ؤΥݥ��󥿤ȥХ��ȥ�������������ޤ������Ѵ�ʸ����פν�ü�����ɤȤ��� '\\0' �ޤ��� '\n' ��ǧ�����ޤ���
/**
 * �����˼��Ԥ�����硢 start �� size �ˤϿ���ޤ���
 */
inline bool getCandidateInformation(const char *henkanmojiretsu, int index, const char *&start, int &size)
{
        DEBUG_ASSERT_POINTER(henkanmojiretsu);
        DEBUG_ASSERT(*(henkanmojiretsu + 0) == '/');
        DEBUG_ASSERT(index >= 0);
        int count = 0;
        char c;
        for (;;)
        {
                c = *henkanmojiretsu++;
                if ((c == '\n') || (c == '\0'))
                {
                        return false;
                }
                if (c == '/')
                {
                        if (count == index)
                        {
                                if ((*henkanmojiretsu == '\n') || (*henkanmojiretsu == '\0'))
                                {
                                        DEBUG_ASSERT(0);
                                        return false;
                                }
                                else
                                {
                                        const char *tmp = henkanmojiretsu;
                                        int tmp_size = 0;
                                        for (;;)
                                        {
                                                c = *henkanmojiretsu++;
                                                if ((c == '\n') || (c == '\0'))
                                                {
                                                        DEBUG_ASSERT(0);
                                                        return false;
                                                }
                                                if (c == '/')
                                                {
                                                        start = tmp;
                                                        size = tmp_size;
                                                        return true;
                                                }
                                                ++tmp_size;
                                        }
                                }
                        }
                        ++count;
                }
        }
        return false;           // NOTREACHED
}

/// �ָ��Ф���ʸ���� p + index �����ʸ���� compare ����Ӥ��ޤ��� strncmp(3) ��Ʊ�ͤ��ͤ��֤��ޤ���
/**
 * �ָ��Ф���ʸ���� p + index �Ȼ���ʸ���� compare �ϡ֤Ҥ餬�ʥ��󥳡�
 * �ɡפ���Ƥ��뤳�Ȥ���դ�ɬ�פǤ���
 *
 * ��ü�����ɤȤ��� ' ' �ޤ��� '\0' ��ǧ�����ޤ���
 *
 * ����ͤ� strncmp(3) ��Ʊ�͡����ʤ�� p + index �� compare ����٤ơ�
 * ��������� -1 �ʲ������������������ 0 �������礭����� 1 �ʾ������
 * ���֤��ޤ���
 */
inline int compareMidasi(const char *p, int index, int size, const char *compare)
{
        DEBUG_ASSERT_POINTER(p);
        DEBUG_ASSERT(size > 0);
        DEBUG_ASSERT_POINTER(compare);
        if (index < 0)
        {
                return -1;
        }
        if (index >= size)
        {
                return 1;
        }
        if (((*(p + index) != '\1') && (*compare != '\1')) ||
            ((*(p + index) == '\1') && (*compare == '\1')))
        {
                for (int i = 0; ; ++i)
                {
                        if ((index + i) >= size)
                        {
                                return 1;
                        }
                        int s_1 = *(reinterpret_cast<const unsigned char*>(p + index + i));
                        int s_2 = *(reinterpret_cast<const unsigned char*>(compare + i));
                        if (s_1 == ' ')
                        {
                                s_1 = '\0';
                        }
                        if (s_2 == ' ')
                        {
                                s_2 = '\0';
                        }
                        if (s_1 > s_2)
                        {
                                return i + 1;
                        }
                        else if (s_1 < s_2)
                        {
                                return -(i + 1);
                        }
                        if (s_1 == '\0')
                        {
                                return 0;
                        }
                }
        }
        else if (*(p + index) == '\1')
        {
                ++p;
                int index_2 = 0;
                for (int i = 0; ; ++i)
                {
                        if ((index + i) >= size)
                        {
                                return 1;
                        }
                        int s_1 = *(reinterpret_cast<const unsigned char*>(p + index + i));
                        int s_2;
                        if ((i & 0x1) == 0)
                        {
                                s_2 = 0xa4;
                        }
                        else
                        {
                                s_2 = *(reinterpret_cast<const unsigned char*>(compare + index_2++));
                        }
                        if (s_1 == ' ')
                        {
                                s_1 = '\0';
                        }
                        if (s_2 == ' ')
                        {
                                s_2 = '\0';
                        }
                        if (s_1 > s_2)
                        {
                                return i + 1;
                        }
                        else if (s_1 < s_2)
                        {
                                return -(i + 1);
                        }
                        if (s_1 == '\0')
                        {
                                return 0;
                        }
                }
        }
        else
        {
                int index_1 = 0;
                ++compare;
                for (int i = 0; ; ++i)
                {
                        if ((index + i) >= size)
                        {
                                return 1;
                        }
                        int s_1;
                        int s_2 = *(reinterpret_cast<const unsigned char*>(compare + i));
                        if ((i & 0x1) == 0)
                        {
                                s_1 = 0xa4;
                        }
                        else
                        {
                                s_1 = *(reinterpret_cast<const unsigned char*>(p + index + index_1++));
                        }
                        if (s_1 == ' ')
                        {
                                s_1 = '\0';
                        }
                        if (s_2 == ' ')
                        {
                                s_2 = '\0';
                        }
                        if (s_1 > s_2)
                        {
                                return i + 1;
                        }
                        else if (s_1 < s_2)
                        {
                                return -(i + 1);
                        }
                        if (s_1 == '\0')
                        {
                                return 0;
                        }
                }
        }
        DEBUG_ASSERT(0);
        return -1;      // NOTREACHED
}

/// �ָ��Ф���ʸ���� (p + index) �ΥХ��ȥ��������֤��ޤ�����üʸ���Ȥ��� ' ' ��ǧ�����ޤ����Х��ȥ������˽�üʸ���ϴޤޤ�ޤ��󡣼����˼��Ԥ������� 0 ���֤��ޤ���
/**
 * �����˼��Ԥ������ DEBUG_ASSERT ��ͭ���ʤ�Х������Ȥ��ޤ���
 */
inline int getMidasiSize(const char *p, int index, int size)
{
        DEBUG_ASSERT_POINTER(p);
        DEBUG_ASSERT(index >= 0);
        DEBUG_ASSERT(size > 0);
        for (int midasi_size = 0;; ++midasi_size, ++index)
        {
                if (index >= size)
                {
                        DEBUG_ASSERT(0);
                        return 0;
                }
                if (*(p + index) == ' ')
                {
                        return midasi_size;
                }
        }
        return 0;       // NOTREACHED
}

/// index ����Ƭ���֤ˤ���Ȥ��ơ��Ѵ�ʸ����ΥХ��ȥ��������֤��ޤ����Х��ȥ������˲���ʸ���ϴޤߤޤ���
inline int getHenkanmojiretsuSize(const char *p, int index, int size)
{
        DEBUG_ASSERT_POINTER(p);
        DEBUG_ASSERT(index >= 0);
        DEBUG_ASSERT(size > 0);
        for (;;)
        {
                if (index >= size)
                {
                        DEBUG_ASSERT(0);
                        return 0;
                }
                if (*(p + index) == ' ')
                {
                        ++index;
                        break;
                }
                ++index;
        }
        int result = 0;
        for (;;)
        {
                if (index >= size)
                {
                        DEBUG_ASSERT(0);
                        return 0;
                }
                if (*(p + index) == '\n')
                {
                        return result;
                }
                ++result;
                ++index;
        }
        return 0;       // NOTREACHED
}

/// index ����Ƭ���֤ˤ���Ȥ��ơ� 1 �ԤΥХ��ȥ��������֤��ޤ����Х��ȥ������˲���ʸ���ϴޤߤޤ���
inline int getLineSize(const char *p, int index, int size)
{
        DEBUG_ASSERT_POINTER(p);
        DEBUG_ASSERT(index >= 0);
        DEBUG_ASSERT(size > 0);
        int result = 0;
        for (;;)
        {
                if (index >= size)
                {
                        DEBUG_ASSERT(0);
                        return 0;
                }
                if (*(p + index) == '\n')
                {
                        return result;
                }
                ++result;
                ++index;
        }
        return 0;       // NOTREACHED
}

/// 1 ʸ���ܤ��б�����ե��å����ɥ��쥤����ǥå������֤��ޤ���
/**
 * �Ҥ餬�ʤޤ��� ASCII �ʤ�Хե��å����ɥ��쥤����ǥå������֤��ޤ���
 * ����ʳ���ʸ����-1 ���֤��ޤ���
 */
inline int getFixedArrayIndex(const char *p)
{
        DEBUG_ASSERT_POINTER(p);
        int index;
        int c = *(p + 0) & 0xff;
        if (c == '\1')
        {
                c = *(p + 1) & 0xff;
                if (c == 0xa4)
                {
                        // �֤Ҥ餬�ʡ�
                        index = *(p + 2) & 0xff;
                }
                else if (c <= 0x7e)
                {
#ifdef YASKKSERV_DEBUG
                        if ((c < 0x21) || (c > 0x7e))
                        {
                                DEBUG_PRINTF("illegal character c=0x%02x\n", c);
                        }
#endif  // YASKKSERV_DEBUG
                        // ��ASCII��
                        index = c;
                }
                else
                {
                        return -1;
                }
        }
        else
        {
                if (c <= 0x7e)
                {
#ifdef YASKKSERV_DEBUG
                        if ((c < 0x21) || (c > 0x7e))
                        {
                                DEBUG_PRINTF("illegal character c=0x%02x\n", c);
                        }
#endif  // YASKKSERV_DEBUG
                        // ��ASCII��
                }
                else
                {
                        // �֤Ҥ餬�ʡ�
                }
                index = c;
        }
        DEBUG_ASSERT(index <= 0xff);
        return index;
}

/// �Х��ʥꥵ������ search ��õ�����ޤ������դ���п����֤��ޤ��� result_index �ˤϸ��դ�������ǥå����������դ���ʤ��ä����μ�������õ������Τ�Ŭ��������ǥå������֤��ޤ���
inline bool searchBinary(const char *p, int size, const char *search, int &result_index)
{
        DEBUG_ASSERT_POINTER(p);
        DEBUG_ASSERT_POINTER(search);
        DEBUG_ASSERT(size > 0);
        int index = getNextLineIndex(p, size / 2, size);
        int before_index = index;
        int diff = index / 2;
        for (;;)
        {
                int tmp = compareMidasi(p, index, size, search);
                if (tmp < 0)
                {
                        index += diff;
                        if (index >= size)
                        {
// index ��ǽ��Ԥ���Ƭ�˰�ư���ޤ���
                                result_index = getBeginningOfLineIndex(p, size, size);
                                return false;
                        }
                }
                else if (tmp > 0)
                {
                        index -= diff;
                        if (index < 0)
                        {
                                result_index = 0;
                                return false;
                        }
                }
                else
                {
                        result_index = index;
                        return true;
                }
                index = getNextLineIndex(p, index, size);
                if (index == -1)
                {
                        result_index = 0;
                        return false;
                }
                diff /= 2;
                if (index == before_index)
                {
                        result_index = index;

                        return false;
                }
                before_index = index;
        }
        return false;   // NOTREACHED
}

/// ����õ���� search ��õ�����ޤ������դ���п����֤��ޤ��� result_index �ˤϸ��դ�������ǥå������֤��ޤ������դ���ʤ����� result_index ������Ǥ���
inline bool searchLinear(const char *p, int size, const char *search, int &result_index)
{
        DEBUG_ASSERT_POINTER(p);
        DEBUG_ASSERT_POINTER(search);
        DEBUG_ASSERT(size > 0);
        int direction = 0;
        for (;;)
        {
                int tmp = compareMidasi(p, result_index, size, search);
                if (tmp == 0)
                {
                        return true;
                }
                else if (tmp < 0)
                {
                        if (direction == 0)
                        {
                                direction = -1;
                        }
                        else if (direction != -1)
                        {
                                return false;
                        }
                        result_index = getNextLineIndex(p, result_index, size);
                        if (result_index < 0)
                        {
                                return false;
                        }
                }
                else
                {
                        if (direction == 0)
                        {
                                direction = 1;
                        }
                        else if (direction != 1)
                        {
                                return false;
                        }
                        int before_index = result_index;
                        result_index = getPreviousLineIndex(p, result_index, size);
                        if ((before_index == 0) && (result_index <= 0))
                        {
                                return false;
                        }
                }
        }
        return false;   // NOTREACHED
}

inline void clearMemory(void *p, int size)
{
        DEBUG_ASSERT_POINTER(p);
        DEBUG_ASSERT(size > 0);
        memset(p, 0, static_cast<size_t>(size));
}

inline void copyMemory(const void *source, void *destination, int size)
{
        DEBUG_ASSERT_POINTER(source);
        DEBUG_ASSERT_POINTER(destination);
        DEBUG_ASSERT(size > 0);
        memcpy(destination, source, static_cast<size_t>(size));
}

inline int getStringLength(const void * const string)
{
        DEBUG_ASSERT_POINTER(string);
        return static_cast<int>(strlen(static_cast<const char * const>(string)));
}

/// ʸ�� C �μ���ʸ���ؤΥݥ��󥿤��֤��ޤ���
template<char c> const char *getNextPointer(const char *p)
{
        DEBUG_ASSERT_POINTER(p);
        while (*p++ != c)
        {
        }
        return p;
}

inline bool getInteger(const void *p, int &result)
{
        char *end;
        result = static_cast<int>(strtol(reinterpret_cast<const char*>(p), &end, 10));
        if (result == 0)
        {
                if ((*end != '\0') || (errno == EINVAL) || (errno == ERANGE))
                {
                        return false;
                }
        }
        return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
inline bool getFloat(const void *p, float &result)
{
        char *end;
        result = static_cast<float>(strtof(reinterpret_cast<const char*>(p), &end));
        if ((result == 0.0f) && (p == end))
        {
                return false;
        }
        if (errno == ERANGE)
        {
                return false;
        }
        return true;
}
#pragma GCC diagnostic pop

/// SortKey �򥽡��Ȥ��ޤ���
/**
 * ������ͥ����ȺѤߤΥǡ����ʾ塢����Ǥ���ɬ�פϤʤ��Τǥ��르�ꥺ��
 * �ϥ����륽���Ȥ���Ѥ��Ƥ��ޤ���
 */
struct SortKey
{
        int index;
        int i;
};
inline void sortMidasi(const char *buffer, int buffer_size, SortKey *p, int length)
{
        SortKey tmp;
        int h;
        for (h = 1; h < length / 9; h = h * 3 + 1)
        {
        }
        for (; h > 0; h /= 3)
        {
                for (int i = h; i < length; ++i)
                {
                        int j;
                        j = i;
                        while ((j >= h) &&
                               (compareMidasi(buffer, (p + (j - h))->index, buffer_size, buffer + (p + j)->index) > 0))
                        {
                                tmp = *(p + j);
                                *(p + j) = *(p + j - h);
                                *(p + j - h) = tmp;
                                j -= h;
                        }
                }
        }
}

enum HashType
{
        HASH_TYPE_CANDIDATE,
        HASH_TYPE_MIDASI
};

template<HashType U> struct HashWrapper_
{
};

template<> struct HashWrapper_<HASH_TYPE_CANDIDATE>
{
        static bool is_terminator(char c)
        {
                return (c == '\0');
        }

        static bool is_right(char c)
        {
                return (c == '/');
        }
};

template<> struct HashWrapper_<HASH_TYPE_MIDASI>
{
        static bool is_terminator(char c)
        {
                return ((c == ' ') || (c == '\0'));
        }

        static bool is_right(char c)
        {
                return ((c == ' ') || (c == '/'));
        }
};

/// candidate or �ָ��Ф������ѥϥå���Ǥ�������ʸ����ν�ü�� HashType �˱����� '/' �ޤ��� ' ' �Ȥʤ�ޤ���ǰ�Τ��� '\\0' �Ǥ⽪ü���ޤ�����������ϥǥХå��ӥ�ɤǤϥ������Ȥ��ޤ���
/**
 * �����Ȥʤ�ʸ����� add() �ʹ��˲����ƤϤʤ�ޤ��󡣤��Τ���񤭴���
 * ��ȯ������ƥ�ݥ��Хåե��ʤɤؤλ��ѤϤǤ��ʤ����Ȥ���դ�ɬ�פ�
 * ����
 */
template<HashType T> class Hash
{
        Hash(Hash &source);
        Hash& operator=(Hash &source);

public:
        enum
        {
                PRIME_2 = 2,
                PRIME_4 = 5,
                PRIME_8 = 11,
                PRIME_16 = 17,
                PRIME_32 = 37,
                PRIME_64 = 67,
                PRIME_128 = 131,
                PRIME_256 = 257,
                PRIME_512 = 547,
                PRIME_1024 = 1031,
                PRIME_2048 = 2053,
                PRIME_4096 = 4099,
                PRIME_8192 = 8209,
                PRIME_16384 = 16411,
                PRIME_32768 = 32771,
                PRIME_65536 = 65539
        };

/// ������ hash_table_length ����ϥå���ơ��֥륵�����˺�Ŭ���� (�礭�ܤ��ǿ�) ���֤��ޤ��������˼��Ԥ������� 0 ���֤��ޤ���
        static int getPrimeHashTableLength(int hash_table_length)
        {
// candidate_length �Ϲ�®���Τ��ᡢ�礭�ܤ˳��ݤ��ޤ���
                if (hash_table_length <= 512)
                {
                        hash_table_length = PRIME_1024;
                }
                else if (hash_table_length <= 1024)
                {
                        hash_table_length = PRIME_2048;
                }
                else if (hash_table_length <= 2048)
                {
                        hash_table_length = PRIME_4096;
                }
                else if (hash_table_length <= 4096)
                {
                        hash_table_length = PRIME_8192;
                }
                else if (hash_table_length <= 8192)
                {
                        hash_table_length = PRIME_16384;
                }
                else if (hash_table_length <= 16384)
                {
                        hash_table_length = PRIME_32768;
                }
                else if (hash_table_length <= 32768)
                {
                        hash_table_length = PRIME_65536;
                }
                else
                {
                        hash_table_length = 0;
                        DEBUG_ASSERT(0);
// candidate ��¿�᤮�ޤ���
//
// 2500 �Ĥ� candidate ����ĥ���ȥ꤬ 26 �Ĥμ���ǥҥåȤ����褦�ʾ�
// ��ʤΤǡ��ޤ�����Ϥʤ��Ȼפ��ޤ���
                }
                return hash_table_length;
        }

        virtual ~Hash()
        {
                delete[] hash_table_;
        }

/// hash_table_length �Υϥå���ơ��֥�Ĺ����ĥ��֥������Ȥ򥳥��ȥ饯�Ȥ��ޤ��� hash_table_length �� getPrimeHashTableLength() ������������Τ���Ѥ������ǽ�����夷�ޤ���
        Hash(int hash_table_length) :
                hash_table_(new Unit[hash_table_length]),
                hash_table_length_(hash_table_length),
                add_counter_(0)
        {
                DEBUG_PRINTF("hash_table_length = %d  size = %d\n",
                             hash_table_length_,
                             hash_table_length_ * static_cast<int>(sizeof(Unit)));
        }

        int getHashTableLength()
        {
                return hash_table_length_;
        }

/// candidate ʸ���� key ��ϥå������Ͽ���ޤ�����Ͽ����������п��򡢥Хåե������դ���Ͽ�˼��Ԥ��뤫��������Ͽ����Ƥ���е����֤��ޤ���
/**
 * \attention
 * key �� '/' �ǽ�ü����뤳�Ȥ���դ�ɬ�פǤ���
 */
        bool add(const char *key, int size)
        {
                DEBUG_ASSERT_POINTER(key);
                DEBUG_ASSERT(size >= 1); // ���� 1 ʸ��
                if (add_counter_ >= hash_table_length_)
                {
                        return false;
                }
                int hash_root = get_hash_root(key, size);
                for (int i = 0; i != hash_table_length_; ++i)
                {
                        int index = hash_root + i;
                        index %= hash_table_length_;
                        if ((hash_table_ + index)->key)
                        {
                                if (compare(key, (hash_table_ + index)->key, size))
                                {
                                        return false;
                                }
                        }
                        else
                        {
                                (hash_table_ + index)->key = key;
#ifdef YASKKSERV_DEBUG
                                (hash_table_ + index)->debug_offset = i;
#endif  // YASKKSERV_DEBUG
                                ++add_counter_;
                                return true;
                        }
                }
                return false;
        }

/// size �Х��Ȥ� candidate ʸ���� key ���ϥå������Ͽ����Ƥ���п����֤��ޤ���
/**
 * \attention
 * key �� '/' �ǽ�ü����뤳�Ȥ���դ�ɬ�פǤ���
 */
        bool contain(const char *key, int size) const
        {
                DEBUG_ASSERT_POINTER(key);
                DEBUG_ASSERT(size >= 1); // ���� 1 ʸ��
                int hash_root = get_hash_root(key, size);
                int first_index = hash_root % hash_table_length_;
                if ((hash_table_ + first_index)->key)
                {
                        for (int i = 0; i != hash_table_length_; ++i)
                        {
                                int index = hash_root + i;
                                index %= hash_table_length_;
                                if ((hash_table_ + index)->key)
                                {
                                        if (compare(key, (hash_table_ + index)->key, size))
                                        {
                                                return true;
                                        }
                                }
                                else
                                {
                                        return false;
                                }
                        }
                }
                return false;
        }

        int getEmptyHashTableLength() const
        {
                return hash_table_length_ - add_counter_;
        }

/// �ǥХå��ӥ�ɤǼ����Ǥ���ǥХå������������ޤ�����꡼���ӥ�ɤǤϼ��������������� 0 �ˤʤ�ޤ���
        void getDebugBuildInformation(int &counter, int &offset_max, double &average)
        {
                counter = 0;
                offset_max = 0;
                average = 0.0;
#ifdef YASKKSERV_DEBUG
                for (int i = 0; i != hash_table_length_; ++i)
                {
                        if ((hash_table_ + i)->key)
                        {
                                ++counter;
                                int tmp = (hash_table_ + i)->debug_offset;
                                average += tmp;
                                if (tmp > offset_max)
                                {
                                        offset_max = tmp;
                                }
                        }
                }
                if (counter)
                {
                        average /= counter;
                }
#endif  // YASKKSERV_DEBUG
        }

private:
        bool compare(const char *key_0, const char *key_1, int size) const
        {
                DEBUG_ASSERT_POINTER(key_0);
                DEBUG_ASSERT_POINTER(key_1);
                DEBUG_ASSERT(size >= 1);
                for (int i = 0; i != size; ++i)
                {
                        char tmp_0 = *key_0;
                        char tmp_1 = *key_1;
                        if (HashWrapper_<T>::is_terminator(tmp_0))
                        {
                                tmp_0 = '/';
                        }
                        if (HashWrapper_<T>::is_terminator(tmp_1))
                        {
                                tmp_1 = '/';
                        }
                        if (tmp_0 != tmp_1)
                        {
                                return false;
                        }
                        else
                        {
                                if (tmp_0 == '/')
                                {
                                        DEBUG_ASSERT(HashWrapper_<T>::is_right(*key_0));
                                        return true;
                                }
                                ++key_0;
                                ++key_1;
                        }
                }
                return true;
        }

        int get_hash_root(const char *key, int size) const
        {
                DEBUG_ASSERT_POINTER(key);
// ���� get_hash_root() �Ͻ�ü��ޤޤʤ��Τ� size >= 1 �ʤ��Ȥ���դ�ɬ
// �פǤ���
                DEBUG_ASSERT(size >= 1); // ���� 1 ʸ��

// result �λ��м��ϼºݤ�¬�ꤷ�ơ��ʤ�Ȥʤ��ɤ��ͤ��Ф���Τ���Ѥ�
// �Ƥ��ޤ���
//
// time perl sandbox/send_server.pl --timeout_second=30.0 --port=10013 '40 ' > /dev/null 
//
// factor = 23
// real	0m0.104s
// offset_max=87
// average=1.485140
//
// factor = 24
// real	0m0.103s
// offset_max=51
// average=0.814867
//
// factor = 27
// real	0m0.104s
// offset_max=41
// average=0.713104
//
// factor = 33
// real	0m0.101s
// offset_max=41
// average=1.112136
//
// factor = 61
// real	0m0.107s
// offset_max=23
// average=0.662954
//
// factor = 1999
// real	0m0.103s
// offset_max=33
// average=1.025329
//
// factor = 2203
// real	0m0.106s
// offset_max=25
// average=0.791383
                unsigned int unsigned_tmp = 0;
                const int factor = 61;
                for (int i = 0; i != size; ++i)
                {
                        unsigned_tmp = unsigned_tmp * factor + *reinterpret_cast<const unsigned char*>(key + i);
                }
                const unsigned int unsigned_mask = 0x7fffffffU;
                int result = static_cast<int>(unsigned_tmp & unsigned_mask);
                return result;
        }

private:
        struct Unit
        {
        private:
                Unit(Unit &source);
                Unit& operator=(Unit &source);

        public:
                Unit() :
#ifdef YASKKSERV_DEBUG
                        key(0),
                        debug_offset(0)
#else  // YASKKSERV_DEBUG
                        key(0)
#endif  // YASKKSERV_DEBUG
                {
                }

                const char *key;
#ifdef YASKKSERV_DEBUG
                int debug_offset;
#endif  // YASKKSERV_DEBUG
        };

        Unit *hash_table_;
        int hash_table_length_;
        int add_counter_;
};
}
}

#endif // SKK_UTILITY_HPP
