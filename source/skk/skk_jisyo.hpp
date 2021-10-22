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
#ifndef SKK_JISYO_H
#define SKK_JISYO_H

#include "skk_architecture.hpp"
#include "skk_utility.hpp"
#include "skk_mmap.hpp"

namespace YaSkkServ
{
/// SKK 辞書を扱うクラスです。
/**
 *
 * \section memo メモ
 *
 * 辞書生成メソッドではバッファリングによる高速化を期待するため I/O に
 * cstdio を使用します。見出しに対するエントリを取得する Search() メソッ
 * ドではバッファリングする必要がないので mmap を使用します。
 *
 * 以上のように、本クラスでは潤沢なリソースを要求します。
 *
 *
 * \section aboutdictionary 辞書について
 *
 * yaskkserv では辞書を SKK の辞書から専用形式に変換して使用します。変
 * 換は内部的に 2 段階ありますが、一般にユーザが 1 段階目の辞書に触れる
 * ことはありません。
 *
 * 1 段階目
 *
 * \li class SkkJisyo 用
 *
 * \li createDictionaryForClassSkkJisyo() を用い作成
 *
 * 2 段階目
 *
 * \li class SkkDictionary 用
 *
 * \li createDictionaryForClassSkkDictionary() を用い作成
 *
 *
 * 変換の内容は以下の通りです。
 *
 * \li コメントは全て取り除かれる
 *
 * \li 「見出し」は「ひらがなエンコード」される
 *
 * \li 「通常エントリ」と「特殊エントリ」に分けられる
 *
 * \li それぞれ「送りあり」と「送りなし」を混ぜた状態で昇順ソートされる
 *
 * \li 2 段階目ではインデックスデータが付加される
 *
 * \li 末尾に辞書情報がバイナリで付加される
 *
 * \subsection special 特殊エントリ
 * 
 * 先頭一文字が「ひらがな(0xa4)」または「ASCII(0x21-0x7e)」でない文字は
 * 特殊エントリとして扱われます。
 *
 *
 * \seciton structure 辞書の構造
 *
 * \verbatim
 +---------------+------------------------------------+ <-- SEEK_TOP (STATE_NORMAL)
 |               | normal                             |
 |               | 通常エントリ                       |
 |               | (昇順ソート。存在しない場合もある) | <-- SEEK_BOTTOM (STATE_NORMAL)
 | dictionary    +------------------------------------+ <-- SEEK_TOP (STATE_SPECIAL)
 |               | special                            |
 |               | 特殊エントリ                       |
 |               | (昇順ソート。存在しない場合もある) | <-- SEEK_BOTTOM (STATE_SPECIAL)
 +---------------+------------------------------------+
 |               | terminator and alignment           |
 | index data    | 1 - 4 バイトの 0 ターミネート領域  |
 |               +------------------------------------+ <-- インデックスデータオフセット
 | (1 段階目では | struct IndexDataHeader             |
 |  存在しない)  |              +                     |
 |               | index data  インデックスデータ     |
 +---------------+------------------------------------+
 |               | terminator and alignment           |
 |               | 1 - 4 バイトの 0 ターミネート領域  |
 | information   +------------------------------------+ <--- ファイルサイズ - sizeof(Information)
 |               |                                    |
 |               | struct Information 辞書情報        |
 |               |                                    |
 +---------------+------------------------------------+
\endverbatim
 *
 * \subsection information 辞書情報
 *
 * \verbatim

struct Information

 object[0]  : ビットフラグ
 object[1]  : リザーブ
 object[2]  : リザーブ
 object[3]  : リザーブ
 object[4]  : リザーブ
 object[5]  : リザーブ
 object[6]  : インデックスデータへのオフセット
 object[7]  : インデックスデータサイズ
 object[8]  : special 行数
 object[9]  : special サイズ
 object[10] : normal 行数
 object[11] : normal サイズ
 object[12] : ブロックアラインメントサイズ
 object[13] : バージョン
 object[14] : object サイズ
 object[15] : identifier

\endverbatim
 *
 * \section aboutdictionaryindex インデックスデータについて
 *
 * インデックスデータは以下の要素から構成されます。(参考数値は 2005年10
 * 月12 日(水) あたりの SKK-JISYO.total+zipcode の値です。)
 *
 * \li struct FixedArray の固定長配列 (256 個)
 *
 * \li struct Block/BlockShort の可変長配列 (block_size/length : 4k/3564, 8k/1840, 16k/971)
 *
 * \li 文字列格納領域 (block_size/length : 4k/28k, 8k/14k, 16k/8k)
 *
 * Block/BlockShort の可変長配列と文字列格納領域は、通常エントリと特殊
 * エントリで共用します。特殊エントリのものは通常エントリの後に追加され
 * ます。 FixedArray は通常エントリだけが使用します。
 *
 * \verbatim
 +----------------------------------------------------+
 |                                                    |
 |  struct IndexDataHeader                            |
 |                                                    |
 |          header[0]  : ビットフラグ                 |
 |          header[1]  : header サイズ                |
 |          header[2]  : ブロックサイズ               |
 |          header[3]  : normal_block_length          |
 |          header[4]  : special_block_length         |
 |          header[5]  : normal_string_length         |
 |          header[6]  : special_string_length        |
 |          header[7]  : 特殊エントリへのオフセット   |
 |                                                    |
 +----------------------------------------------------+
 |                                                    |
 |  FixedArray fixed_array[256]                       |
 |                                                    |
 |     通常エントリの探索ヒント情報です。             |
 |                                                    |
 +----------------------------------------------------+
 |                                                    |
 |  Block block[block_length]                         |
 |  BlockShort block[block_length]                    |
 |                                                    |
 |     ブロック情報を保持します。                     |
 |     アラインメントされた辞書では BlockShort を     |
 |     使用することでサイズを 1/4 にすることが        |
 |     できます。                                     |
 |                                                    |
 +----------------------------------------------------+
 |                                                    |
 |  char string[string_length]                        |
 |                                                    |
 |     可変長文字列です。終端コードとしてスペースを   |
 |     使用します。つまり "AA BBB CCCC DDDDD " と     |
 |     いった文字列になります。                       |
 |                                                    |
 |     文字列は block 配列と同じ数(block_length 個)   |
 |     存在します。                                   |
 |                                                    |
 |     この文字列は対応するブロックの最後の見出し     |
 |     です。これによりブロック内に探索文字列が       |
 |     存在するかどうかを調べます。                   |
 |                                                    |
 +----------------------------------------------------+
\endverbatim
 *
 */
class SkkJisyo
{
        SkkJisyo(SkkJisyo &source);
        SkkJisyo& operator=(SkkJisyo &source);

public:
        enum State
        {
                STATE_NORMAL,
                STATE_SPECIAL,

                STATE_LENGTH
        };
        enum JisyoType
        {
                JISYO_TYPE_UNKNOWN,
                JISYO_TYPE_SKK_RAW,
                JISYO_TYPE_CLASS_SKK_JISYO,
                JISYO_TYPE_CLASS_SKK_DICTIONARY
        };
        enum SeekPosition
        {
                SEEK_POSITION_TOP,
                SEEK_POSITION_BOTTOM,
                SEEK_POSITION_NEXT,
                SEEK_POSITION_PREVIOUS,
                SEEK_POSITION_BEGINNING_OF_LINE
        };
        enum
        {
                IDENTIFIER = 0x7fedc000
        };

/// array[0] は常にビットフラグを示すことに注意が必要です。
        template<int N> struct ArrayInt32
        {
                enum
                {
                        BIT_FLAG_BYTE_ORDER_VAX = 0x1 << 0,
                };

                static int getSize()
                {
                        return sizeof(int32_t) * N;
                }

                ArrayInt32(int bit_flag = BIT_FLAG_BYTE_ORDER_VAX)
                {
                        array[0] = bit_flag;
                        for (int i = 1; i != N; ++i)
                        {
                                array[i] = 0;
                        }
                }

                bool initialize(const void *p)
                {
                        DEBUG_ASSERT_POINTER(p);
                        DEBUG_ASSERT_POINTER_ALIGN(p, 4);
                        const int32_t *tmp = reinterpret_cast<const int32_t*>(p);
                        for (int i = 0; i != N; ++i)
                        {
                                array[i] = *tmp++;
                        }
                        return true;
                }

                int32_t getSwap(int index) const
                {
                        return static_cast<int32_t>((static_cast<uint32_t>(array[index] << 24) & 0xff000000UL) |
                                                    (static_cast<uint32_t>(array[index] << 8) & 0x00ff0000UL) |
                                                    (static_cast<uint32_t>(array[index] >> 8) & 0x0000ff00UL) |
                                                    (static_cast<uint32_t>(array[index] >> 24) & 0x000000ffUL));
                }

                int32_t get(int index) const
                {
#ifdef YASKKSERV_ARCHITECTURE_BYTE_ORDER_NETWORK
                        if (array[0] & BIT_FLAG_BYTE_ORDER_VAX)
                        {
                                return getSwap(index);
                        }
                        else
                        {
                                return array[index];
                        }
#endif  // YASKKSERV_ARCHITECTURE_BYTE_ORDER_NETWORK

#ifdef YASKKSERV_ARCHITECTURE_BYTE_ORDER_VAX
                        if (array[0] & BIT_FLAG_BYTE_ORDER_VAX)
                        {
                                return array[index];
                        }
                        else
                        {
                                return getSwap(index);
                        }
#endif  // YASKKSERV_ARCHITECTURE_BYTE_ORDER_VAX
                }

                void set(int index, int32_t scalar)
                {
#ifdef YASKKSERV_ARCHITECTURE_BYTE_ORDER_NETWORK
                        if (array[0] & BIT_FLAG_BYTE_ORDER_VAX)
                        {
                                array[index] = getSwap(index);
                        }
                        else
                        {
                                array[index] = scalar;
                        }
#endif  // YASKKSERV_ARCHITECTURE_BYTE_ORDER_NETWORK

#ifdef YASKKSERV_ARCHITECTURE_BYTE_ORDER_VAX
                        if (array[0] & BIT_FLAG_BYTE_ORDER_VAX)
                        {
                                array[index] = scalar;
                        }
                        else
                        {
                                array[index] = getSwap(index);
                        }
#endif  // YASKKSERV_ARCHITECTURE_BYTE_ORDER_VAX
                }

                int32_t array[N];
        };

        struct Information
        {
                enum Id
                {
                        ID_BIT_FLAG,

                        ID_RESERVE_1,
                        ID_RESERVE_2,
                        ID_RESERVE_3,
                        ID_RESERVE_4,
                        ID_RESERVE_5,

                        ID_INDEX_DATA_OFFSET,
                        ID_INDEX_DATA_SIZE,
                        ID_SPECIAL_LINES,
                        ID_SPECIAL_SIZE,
                        ID_NORMAL_LINES,
                        ID_NORMAL_SIZE,
                        ID_BLOCK_ALIGNMENT_SIZE,
                        ID_VERSION,
                        ID_SIZE,
                        ID_IDENTIFIER,

                        ID_LENGTH,
                };

                static int getSize()
                {
                        return sizeof(int32_t) * ID_LENGTH;
                }

                Information(int bit_flag = ArrayInt32<ID_LENGTH>::BIT_FLAG_BYTE_ORDER_VAX) :
                        object(bit_flag)
                {
                        set(ID_IDENTIFIER, SkkJisyo::IDENTIFIER);
                        set(ID_SIZE, getSize());
                        set(ID_VERSION,
                            (0 * 100000) +
                            (0 * 10000) +
                            (0 * 1000) +
                            (0 * 100) +
                            (0 * 10) +
                            (1 * 1));
                }

                bool initialize(const void *p)
                {
                        return object.initialize(p);
                }

                int32_t get(Id id)
                {
                        return object.get(id);
                }

                void set(Id id, int32_t scalar)
                {
                        return object.set(id, scalar);
                }

                ArrayInt32<ID_LENGTH> object;
        };

        struct IndexDataHeader
        {
                enum Id
                {
                        ID_BIT_FLAG,

                        ID_SIZE,
                        ID_BLOCK_SIZE,
                        ID_NORMAL_BLOCK_LENGTH,
                        ID_SPECIAL_BLOCK_LENGTH,
                        ID_NORMAL_STRING_SIZE,
                        ID_SPECIAL_STRING_SIZE,
                        ID_SPECIAL_ENTRY_OFFSET,

                        ID_LENGTH
                };
                enum
                {
                        BIT_FLAG_BLOCK_SHORT = 0x1 << 31
                };

                static int getSize()
                {
                        return sizeof(int32_t) * ID_LENGTH;
                }

                IndexDataHeader(int bit_flag = ArrayInt32<ID_LENGTH>::BIT_FLAG_BYTE_ORDER_VAX) :
                        object(bit_flag)
                {
                        set(ID_SIZE, getSize());
                }

                bool initialize(const void *p)
                {
                        return object.initialize(p);
                }

                int32_t get(Id id)
                {
                        return object.get(id);
                }

                void set(Id id, int32_t scalar)
                {
                        return object.set(id, scalar);
                }

                ArrayInt32<ID_LENGTH> object;
        };

        struct FixedArray
        {
                FixedArray () :
                        start_block(0),
                        block_length(0),
                        string_data_offset(0)
                {
                }

                int16_t start_block;
                int16_t block_length;
                int32_t string_data_offset;
        };

        struct Block
        {
                Block () :
                        offset(0),
                        line_length_and_data_size(0)
                {
                }

                int getOffset()
                {
                        return offset;
                }

                void setOffset(int scalar)
                {
                        offset = scalar;
                }

                int getDataSize()
                {
                        return line_length_and_data_size & 0xfffff;
                }

                void setLineLengthAndDataSize(int line_length, int data_size)
                {
                        line_length_and_data_size = (line_length << 20) | data_size;
                }

                int32_t offset;
                int32_t line_length_and_data_size;
        };

        struct BlockShort
        {
                BlockShort () :
                        data_size(0)
                {
                }

                int getDataSize()
                {
                        return data_size;
                }

                void setDataSize(int size)
                {
                        data_size = static_cast<int16_t>(size);
                }

                int16_t data_size;
        };

private:
/// okuri_ari_index と okuri_nasi_index を求めます。失敗した場合は偽を返します。
        static bool get_index(const char *buffer, int filesize, int &okuri_ari_index, int &okuri_nasi_index)
        {
                DEBUG_ASSERT_POINTER(buffer);
                for (;;)
                {
//
// okuri_ari_index と okuri_nasi_index を以下のような流れで求めます。
//
// ================ SKK 辞書の構造とインデックス ==================
// ;; okuri-ari entries.
// うううu /.../
// いいいi /.../
// あああa /.../ ← この行頭が okuri_ari_index
// ;; okuri-nasi entries.
// あああ /.../ ← この行頭が okuri_nasi_index
// いいい /.../
// ううう /.../
// ================================================================
//
// 1. まず ";; okuri-nasi entries.\n" を検索
//
// 2. 見付かったら、その 1 行上が「送りありエントリの先頭候補」、
//    1 行下が「送りなしエントリの先頭候補」
//    (SKK 辞書では送りありエントリは逆方向にソートされていることに注意)
//
// 3. それぞれの先頭候補で空行とコメントをスキップし、真の先頭を設定
//
                        if ((*(buffer + okuri_nasi_index) == ';') &&
                           ((filesize + 1 - okuri_nasi_index) >= SkkUtility::getStringOkuriNasiLength()) &&
                            SkkUtility::isStringOkuriNasi(buffer + okuri_nasi_index))
                        {
// get candidate index
                                okuri_ari_index = SkkUtility::getPreviousLineIndex(buffer, okuri_nasi_index, filesize);
                                okuri_nasi_index = SkkUtility::getNextLineIndex(buffer, okuri_nasi_index, filesize);
// skip empty/comment line
                                while (okuri_ari_index > 0)
                                {
                                        if (*(buffer + okuri_ari_index) == '\n')
                                        {
                                                --okuri_ari_index;
                                                okuri_ari_index = SkkUtility::getBeginningOfLineIndex(buffer, okuri_ari_index, filesize);
                                        }
                                        else if (*(buffer + okuri_ari_index) == ';')
                                        {
                                                okuri_ari_index = SkkUtility::getPreviousLineIndex(buffer, okuri_ari_index, filesize);
                                        }
                                        else
                                        {
                                                break;
                                        }
                                }
                                while (okuri_nasi_index >= 0)
                                {
                                        if (*(buffer + okuri_nasi_index) == '\n')
                                        {
                                                ++okuri_nasi_index;
                                                if (okuri_nasi_index >= filesize)
                                                {
                                                        okuri_nasi_index = -1;
                                                }
                                        }
                                        else if (*(buffer + okuri_nasi_index) == ';')
                                        {
                                                okuri_nasi_index = SkkUtility::getNextLineIndex(buffer, okuri_nasi_index, filesize);
                                        }
                                        else
                                        {
                                                break;
                                        }
                                }
                                break;
                        }
                        okuri_nasi_index = SkkUtility::getNextLineIndex(buffer, okuri_nasi_index, filesize);
                        if (okuri_nasi_index < 0)
                        {
                                return false;
                        }
                }
                return true;
        }

        static bool append_terminator(FILE *file)
        {
                DEBUG_ASSERT_POINTER(file);
// ターミネータ兼アラインメント領域(4 bytesに揃える)
                char tmp[4];
                for (int i = 0; i != 4; ++i)
                {
                        tmp[i] = 0;
                }

// 1 or 2 or 3 or 4 バイトの 0 で埋める(すでに 4 バイトアラインならば 4 バイト)
                long current = ftell(file);
                if (fwrite(tmp, 4 - (current % 4), 1, file) < 1)
                {
                        return false;
                }
                return true;
        }

        static bool append_information(FILE *file,
                                       int block_alignment_size,
                                       int normal_size,
                                       int normal_lines,
                                       int special_size,
                                       int special_lines,
                                       int index_data_size,
                                       int index_data_offset)
        {
                DEBUG_ASSERT_POINTER(file);
                Information information;
                information.set(Information::ID_BLOCK_ALIGNMENT_SIZE,
                                block_alignment_size);
                information.set(Information::ID_NORMAL_SIZE,
                                normal_size);
                information.set(Information::ID_NORMAL_LINES,
                                normal_lines);
                information.set(Information::ID_SPECIAL_SIZE,
                                special_size);
                information.set(Information::ID_SPECIAL_LINES,
                                special_lines);
                information.set(Information::ID_INDEX_DATA_SIZE,
                                index_data_size);
                information.set(Information::ID_INDEX_DATA_OFFSET,
                                index_data_offset);

                append_terminator(file);

                if (fwrite(&information, sizeof(information), 1, file) < 1)
                {
                        return false;
                }
                return true;
        }

        static bool create_dictionary_for_class_skk_jisyo_write_temporary_raw(FILE *file, const char *buffer, int index, int line_size)
        {
                DEBUG_ASSERT_POINTER(file);
                DEBUG_ASSERT_POINTER(buffer);
                DEBUG_ASSERT(index >= 0);
                DEBUG_ASSERT(line_size > 0);
                const int cr_size = 1;
                if (fwrite("\1", 1, 1, file) < 1)
                {
                        return false;
                }
                if (fwrite(buffer + index, static_cast<size_t>(line_size + cr_size), 1, file) < 1)
                {
                        return false;
                }
                return true;
        }


        static bool create_dictionary_for_class_skk_jisyo_write_temporary_encoded(FILE *file,
                                                                                  const char *buffer,
                                                                                  int index,
                                                                                  int filesize,
                                                                                  const char *line_buffer,
                                                                                  int encoded_size)
        {
                DEBUG_ASSERT_POINTER(file);
                DEBUG_ASSERT_POINTER(buffer);
                DEBUG_ASSERT(index >= 0);
                DEBUG_ASSERT(filesize > 0);
                DEBUG_ASSERT_POINTER(line_buffer);
                DEBUG_ASSERT(encoded_size > 0);
                const int cr_size = 1;
                if (fwrite(line_buffer, static_cast<size_t>(encoded_size), 1, file) < 1)
                {
                        return false;
                }
                if (fwrite(" ", 1, 1, file) < 1)
                {
                        return false;
                }
                const char *tmp = SkkUtility::getHenkanmojiretsuPointer(buffer, index, filesize);
                const int tmp_size = SkkUtility::getHenkanmojiretsuSize(buffer, index, filesize);
                if ((tmp == 0) || (tmp_size <= 0))
                {
                        return false;
                }
                if (fwrite(tmp, static_cast<size_t>(tmp_size + cr_size), 1, file) < 1)
                {
                        return false;
                }
                return true;
        }

        static bool create_dictionary_for_class_skk_jisyo_write_temporary(const char *buffer,
                                                                          int filesize,
                                                                          int normal_fd,
                                                                          int special_fd,
                                                                          int okuri_ari_index,
                                                                          int okuri_nasi_index,
                                                                          int &okuri_ari_lines,
                                                                          int &okuri_nasi_lines,
                                                                          int &special_okuri_nasi_lines)
        {
                DEBUG_ASSERT_POINTER(buffer);
                DEBUG_ASSERT(filesize > 0);
                DEBUG_ASSERT(normal_fd > 0);
                DEBUG_ASSERT(special_fd > 0);
                DEBUG_ASSERT(okuri_ari_index >= 0);
                DEBUG_ASSERT(okuri_nasi_index >= 0);
                DEBUG_ASSERT(okuri_ari_lines == 0);
                DEBUG_ASSERT(okuri_nasi_lines == 0);
                DEBUG_ASSERT(special_okuri_nasi_lines == 0);
                bool result;
                FILE *file_normal = fdopen(normal_fd, "wb");
                FILE *file_special = fdopen(special_fd, "wb");
                if ((file_normal == 0) || (file_special == 0))
                {
                        result = false;
                }
                else
                {
                        result = true;
                        const int cr_size = 1;
                        const int line_buffer_margin = 16;
// line_buffer_size は常識的に考えて問題のない値です。万が一この値を越
// えた場合でも、 encodeHiragana() でサイズチェックをしているため安全で
// す。
                        const int line_buffer_size = 64 * 1024;
                        char *line_buffer = new char[line_buffer_size];
                        for (;;)
                        {
                                bool copy_okuri_ari = false;
                                bool copy_okuri_nasi = false;
                                if ((okuri_ari_index > 0) && (okuri_nasi_index >= 0))
                                {
                                        int tmp = SkkUtility::compareMidasi(buffer, okuri_ari_index, filesize, buffer + okuri_nasi_index);
                                        if (tmp == 0)
                                        {
// なぜか ari/nasi で同じ見出しがある (あったらおかしい)
                                                result = false;
                                                break;
                                        }
                                        else if (tmp > 0)
                                        {
// ari -> nasi
                                                copy_okuri_nasi = true;
                                        }
                                        else
                                        {
// nasi -> ari
                                                copy_okuri_ari = true;
                                        }
                                }
                                else if (okuri_ari_index > 0)
                                {
                                        copy_okuri_ari = true;
                                }
                                else if (okuri_nasi_index >= 0)
                                {
                                        copy_okuri_nasi = true;
                                }
                                else
                                {
                                        break;
                                }

                                if (copy_okuri_ari)
                                {
                                        int line_size = SkkUtility::getLineSize(buffer, okuri_ari_index, filesize);
                                        if (line_size == 0)
                                        {
                                                SkkUtility::printf("warning: found empty line\n");
                                                --okuri_ari_index;
                                                if (okuri_ari_index <= 0)
                                                {
                                                        okuri_ari_index = 0;
                                                }
                                        }
                                        else
                                        {
                                                int encoded_size = SkkUtility::encodeHiragana(buffer + okuri_ari_index,
                                                                                              line_buffer,
                                                                                              line_buffer_size - line_buffer_margin);
                                                if (encoded_size == 0)
                                                {
                                                        result = create_dictionary_for_class_skk_jisyo_write_temporary_raw(file_normal,
                                                                                                                           buffer,
                                                                                                                           okuri_ari_index,
                                                                                                                           line_size);
                                                        if (result == false)
                                                        {
                                                                break;
                                                        }
                                                }
                                                else
                                                {
                                                        result = create_dictionary_for_class_skk_jisyo_write_temporary_encoded(file_normal,
                                                                                                                               buffer,
                                                                                                                               okuri_ari_index,
                                                                                                                               filesize,
                                                                                                                               line_buffer,
                                                                                                                               encoded_size);
                                                        if (result == false)
                                                        {
                                                                break;
                                                        }
                                                }
                                                ++okuri_ari_lines;

                                                okuri_ari_index = SkkUtility::getPreviousLineIndex(buffer, okuri_ari_index, filesize);
                                        }
                                }

                                if (copy_okuri_nasi)
                                {
                                        int line_size = SkkUtility::getLineSize(buffer, okuri_nasi_index, filesize);
                                        if (line_size == 0)
                                        {
                                                SkkUtility::printf("warning: found empty line\n");
                                                ++okuri_nasi_index;
                                                if (okuri_nasi_index >= filesize)
                                                {
                                                        okuri_nasi_index = -1;
                                                }
                                        }
                                        else
                                        {
                                                int c = *(buffer + okuri_nasi_index) & 0xff;
                                                if ((c == 0xa4) ||
                                                    ((c >= 0x21) && (c <= 0x7e)))
                                                {
                                                        int encoded_size = SkkUtility::encodeHiragana(buffer + okuri_nasi_index,
                                                                                                      line_buffer,
                                                                                                      line_buffer_size - line_buffer_margin);
                                                        if (encoded_size == 0)
                                                        {
                                                                result = create_dictionary_for_class_skk_jisyo_write_temporary_raw(file_normal,
                                                                                                                                   buffer,
                                                                                                                                   okuri_nasi_index,
                                                                                                                                   line_size);
                                                                if (result == false)
                                                                {
                                                                        break;
                                                                }
                                                        }
                                                        else
                                                        {
                                                                result = create_dictionary_for_class_skk_jisyo_write_temporary_encoded(file_normal,
                                                                                                                                       buffer,
                                                                                                                                       okuri_nasi_index,
                                                                                                                                       filesize,
                                                                                                                                       line_buffer,
                                                                                                                                       encoded_size);
                                                                if (result == false)
                                                                {
                                                                        break;
                                                                }
                                                        }
                                                        ++okuri_nasi_lines;
                                                }
                                                else
                                                {
                                                        if (fwrite("\1", 1, 1, file_special) < 1)
                                                        {
                                                                result = false;
                                                                break;
                                                        }
                                                        if (fwrite(buffer + okuri_nasi_index, static_cast<size_t>(line_size + cr_size), 1, file_special) < 1)
                                                        {
                                                                result = false;
                                                                break;
                                                        }
                                                        ++special_okuri_nasi_lines;
                                                }
                                                okuri_nasi_index = SkkUtility::getNextLineIndex(buffer, okuri_nasi_index, filesize);
                                        }
                                }

// skip comment
                                while ((okuri_ari_index > 0) && (*(buffer + okuri_ari_index) == ';'))
                                {
                                        okuri_ari_index = SkkUtility::getPreviousLineIndex(buffer, okuri_ari_index, filesize);
                                }
                                while ((okuri_nasi_index >= 0) && (*(buffer + okuri_nasi_index) == ';'))
                                {
                                        okuri_nasi_index = SkkUtility::getNextLineIndex(buffer, okuri_nasi_index, filesize);
                                }
                        }
                        delete[] line_buffer;
                }

                if (file_normal)
                {
                        fclose(file_normal);
                }
                if (file_special)
                {
                        fclose(file_special);
                }

                return result;
        }

        static bool create_dictionary_for_class_skk_jisyo_sort_core(FILE *file, char *buffer, int filesize, int lines)
        {
                DEBUG_ASSERT_POINTER(file);
                DEBUG_ASSERT_POINTER(buffer);
                DEBUG_ASSERT(filesize > 0);
                DEBUG_ASSERT(lines > 0);
                const int cr_size = 1;
                SkkUtility::SortKey *sort_key = new SkkUtility::SortKey[lines];
                bool result = true;
                int index = 0;
                for (int i = 0; i != lines; ++i)
                {
                        (sort_key + i)->index = index;
                        (sort_key + i)->i = i;
                        index = SkkUtility::getNextLineIndex(buffer, index, filesize);
                }
                SkkUtility::sortMidasi(buffer, filesize, sort_key, lines);

                for (int i = 0; i != lines; ++i)
                {
                        int line_size = SkkUtility::getLineSize(buffer, (sort_key + i)->index, filesize);
                        if (fwrite(buffer + (sort_key + i)->index, static_cast<size_t>(line_size + cr_size), 1, file) < 1)
                        {
                                result = false;
                                break;
                        }
                }
                delete[] sort_key;
                return result;
        }

        static bool create_dictionary_for_class_skk_jisyo_sort_and_write(const char *filename_destination,
                                                                         const char *filename_normal,
                                                                         const char *filename_special,
                                                                         int okuri_ari_lines,
                                                                         int okuri_nasi_lines,
                                                                         int special_okuri_nasi_lines)
        {
                DEBUG_ASSERT_POINTER(filename_destination);
                DEBUG_ASSERT_POINTER(filename_normal);
                DEBUG_ASSERT_POINTER(filename_special);
                DEBUG_ASSERT(okuri_ari_lines >= 0);
                DEBUG_ASSERT(okuri_nasi_lines >= 0);
                DEBUG_ASSERT(special_okuri_nasi_lines >= 0);
                DEBUG_ASSERT((okuri_ari_lines + okuri_nasi_lines + special_okuri_nasi_lines) > 0);
                bool result = true;
                FILE *file = fopen(filename_destination, "wb");
                if (file == 0)
                {
                        result = false;
                }
                else
                {
                        int normal_size = 0;
                        int special_size = 0;
                        if (result && ((okuri_ari_lines + okuri_nasi_lines) > 0))
                        {
                                SkkMmap mmap;
                                char *buffer = static_cast<char*>(mmap.map(filename_normal));
                                if (buffer == 0)
                                {
                                        result = false;
                                }
                                else
                                {
                                        normal_size = mmap.getFilesize();
                                        result = create_dictionary_for_class_skk_jisyo_sort_core(file,
                                                                                                 buffer,
                                                                                                 normal_size,
                                                                                                 okuri_ari_lines + okuri_nasi_lines);
                                }
                        }

                        if (result && (special_okuri_nasi_lines > 0))
                        {
                                SkkMmap mmap;
                                char *buffer = static_cast<char*>(mmap.map(filename_special));
                                if (buffer == 0)
                                {
                                        result = false;
                                }
                                else
                                {
                                        special_size = mmap.getFilesize();
                                        result = create_dictionary_for_class_skk_jisyo_sort_core(file,
                                                                                                 buffer,
                                                                                                 special_size,
                                                                                                 special_okuri_nasi_lines);
                                }
                        }

                        if (result)
                        {
                                result = append_information(file,
                                                            0,
                                                            normal_size,
                                                            okuri_ari_lines + okuri_nasi_lines,
                                                            special_size,
                                                            special_okuri_nasi_lines,
                                                            0,
                                                            0);
                                if (!result)
                                {
                                        DEBUG_PRINTF("append_information() failed.\n");
                                }
                        }

                        fclose(file);
                }
                return result;
        }

// 辞書を作成します。失敗した場合は偽を返します。
/**
 * コメントを削除し「送りあり」と「送りなし」を混ぜてソート済みの状態で
 * 出力します。但し、「送りなし」の「ひらがな(0xa4)」と「ASCII(0x21 -
 * 0x7e)」以外の文字は special_okuri_nasi_buffer へ格納し、最後に出力し
 * ます。
 *
 * ;; okuri-ari  entries.             いろは /.../
 * いろはa /.../                      いろはa /.../
 * にほへとa /.../            →      にほへと /.../
 * ;; okuri-nasi  entries.            にほへとa /.../
 * いろは /.../                           special_okuri_nasi_buffer
 * にほへと /.../                     ２しんほう /２進法/
 * ２しんほう /２進法/                イ /ヰ/
 * イ /ヰ/
 */
        static bool create_dictionary_for_class_skk_jisyo(const char *filename_destination,
                                                          const char *buffer,
                                                          int filesize,
                                                          int okuri_ari_index,
                                                          int okuri_nasi_index)
        {
                DEBUG_ASSERT_POINTER(filename_destination);
                DEBUG_ASSERT_POINTER(buffer);
                DEBUG_ASSERT(filesize > 0);
                DEBUG_ASSERT(okuri_ari_index >= 0);
                DEBUG_ASSERT(okuri_nasi_index >= 0);
                char tmp_filename_normal[] = "/tmp/skkjisyo_normal.XXXXXX";
                char tmp_filename_special[] = "/tmp/skkjisyo_special.XXXXXX";
                int normal_fd = mkstemp(tmp_filename_normal);
                if (normal_fd == -1)
                {
                        return false;
                }
                int special_fd = mkstemp(tmp_filename_special);
                if (special_fd == -1)
                {
                        return false;
                }
                bool result;
                int okuri_ari_lines = 0;
                int okuri_nasi_lines = 0;
                int special_okuri_nasi_lines = 0;
                result = create_dictionary_for_class_skk_jisyo_write_temporary(buffer,
                                                                               filesize,
                                                                               normal_fd,
                                                                               special_fd,
                                                                               okuri_ari_index,
                                                                               okuri_nasi_index,
                                                                               okuri_ari_lines,
                                                                               okuri_nasi_lines,
                                                                               special_okuri_nasi_lines);
                if (result)
                {
                        result = create_dictionary_for_class_skk_jisyo_sort_and_write(filename_destination,
                                                                                      tmp_filename_normal,
                                                                                      tmp_filename_special,
                                                                                      okuri_ari_lines,
                                                                                      okuri_nasi_lines,
                                                                                      special_okuri_nasi_lines);
                }
                if (unlink(tmp_filename_normal) == -1)
                {
                        result = false;
                }
                if (unlink(tmp_filename_special) == -1)
                {
                        result = false;
                }
                return result;
        }

        static bool create_dictionary_index_and_dictionary_for_class_skk_jisyo_special_core(SkkJisyo &object,
                                                                                            FILE *aligned_block_dictionary,
                                                                                            Block *block,
                                                                                            BlockShort *block_short,
                                                                                            char *string,
                                                                                            int &string_size,
                                                                                            int block_size_maximum,
                                                                                            bool alignment_flag,
                                                                                            int line,
                                                                                            int top_line,
                                                                                            int index,
                                                                                            int top_offset,
                                                                                            int block_length,
                                                                                            int top_file_offset)
        {
                if (block && block_short)
                {
// 両方のポインタが指定されていてはなりません。
                        DEBUG_PRINTF("ILLEGAL argument\n");
                        DEBUG_ASSERT(0);
                        return false;
                }
// 前のブロックの最終行の情報を得ます。
                int backup_index = object.getIndex();

                if (!object.seek(SkkJisyo::SEEK_POSITION_PREVIOUS))
                {
// ここで SEEK_POSITION_PREVIOUS が失敗するなどはありえないので不正な形式とします。
                        DEBUG_PRINTF("ILLEGAL dictionary\n");
                        DEBUG_ASSERT(0);
                        object.setIndex(backup_index);

                        return false;
                }

                if (block)
                {
                        if (aligned_block_dictionary && alignment_flag)
                        {
                                (block + block_length)->setOffset(static_cast<int>(ftell(aligned_block_dictionary)) - top_file_offset);
                        }
                        else
                        {
                                (block + block_length)->setOffset(top_offset);
                        }
                        (block + block_length)->setLineLengthAndDataSize(line - top_line, index - top_offset);
                }
                else if (block_short)
                {
                        (block_short + block_length)->setDataSize(index - top_offset);
                }

                {
                        const int terminator_size = 1;
                        int length = object.getMidasiSize() + terminator_size;
                        if (string)
                        {
                                char *p = object.getMidasiPointer();
                                int i;
                                for (i = 0; i != length - terminator_size; ++i)
                                {
                                        *(string + string_size + i) = *(p + i);
                                }
                                *(string + string_size + i) = ' ';
                        }
                        string_size += length;
                }

                if (aligned_block_dictionary)
                {
                        int size = index - top_offset;
                        if (fwrite(object.getBuffer() + top_offset, static_cast<size_t>(size), 1, aligned_block_dictionary) < 1)
                        {
                                object.setIndex(backup_index);
                                return false;
                        }
                        if (alignment_flag && (size < block_size_maximum))
                        {
                                fseek(aligned_block_dictionary, block_size_maximum - (size % block_size_maximum), SEEK_CUR);
                        }
                }

                object.setIndex(backup_index);

                return true;
        }

        static bool create_dictionary_index_and_dictionary_for_class_skk_jisyo_special(SkkJisyo &object,
                                                                                       FILE *aligned_block_dictionary,
                                                                                       Block *block,
                                                                                       BlockShort *block_short,
                                                                                       char *string,
                                                                                       int &block_length,
                                                                                       int &string_size,
                                                                                       int block_size_maximum,
                                                                                       bool alignment_flag)
        {
                if (block && block_short)
                {
// 両方のポインタが指定されていてはなりません。
                        DEBUG_PRINTF("ILLEGAL argument\n");
                        DEBUG_ASSERT(0);
                        return false;
                }

                {
                        SkkJisyo::Information information;
                        object.getInformation(information);
                        if (information.get(SkkJisyo::Information::ID_SPECIAL_LINES) == 0)
                        {
// special が存在しない場合は長さ 0 で正常終了。
                                block_length = 0;
                                string_size = 0;

                                return true;
                        }
                }

                int backup_index = object.getIndex();
                int top_offset = 0;
                int line = 1;
                int top_line = 1;
                int top_file_offset = 0;
                if (aligned_block_dictionary)
                {
                        top_file_offset = static_cast<int>(ftell(aligned_block_dictionary));
                }

                block_length = 0;
                string_size = 0;

                object.seek(SkkJisyo::SEEK_POSITION_TOP);

                while (object.seek(SkkJisyo::SEEK_POSITION_NEXT))
                {
                        ++line;
                        int index = object.getIndex();
                        if (index - top_offset > block_size_maximum)
                        {
// ブロックサイズを越えたので 1 行戻します。
                                object.seek(SkkJisyo::SEEK_POSITION_PREVIOUS);
                                --line;
                                index = object.getIndex();
                                if (index <= top_offset)
                                {
// 戻したにもかかわらず前と同じか小さいということは 1 要素が
// block_size_maximum より大きいこととなり、扱うことができません。
                                        DEBUG_PRINTF("ILLEGAL dictionary\n");
                                        DEBUG_ASSERT(0);
                                        object.setIndex(backup_index);

                                        return false;
                                }

                                if (!create_dictionary_index_and_dictionary_for_class_skk_jisyo_special_core(object,
                                                                                                             aligned_block_dictionary,
                                                                                                             block,
                                                                                                             block_short,
                                                                                                             string,
                                                                                                             string_size,
                                                                                                             block_size_maximum,
                                                                                                             alignment_flag,
                                                                                                             line,
                                                                                                             top_line,
                                                                                                             index,
                                                                                                             top_offset,
                                                                                                             block_length,
                                                                                                             top_file_offset))
                                {
                                        // FIXME!
                                }

                                top_line = line;
                                top_offset = index;
                                ++block_length;
                        }
                }

// 終端付近の処理です。
                {
                        object.seek(SkkJisyo::SEEK_POSITION_BOTTOM);
                        int index = object.getIndex() + 1;
                        if (index - top_offset > block_size_maximum)
                        {
// ブロックサイズを越えたので 1 行戻します。
                                object.seek(SkkJisyo::SEEK_POSITION_PREVIOUS);
                                --line;
                                index = object.getIndex();
                                if (index <= top_offset)
                                {
// 戻したにもかかわらず前と同じか小さいということは 1 要素が
// block_size_maximum より大きいこととなり、扱うことができません。
                                        DEBUG_PRINTF("ILLEGAL dictionary\n");
                                        DEBUG_ASSERT(0);
                                        object.setIndex(backup_index);

                                        return false;
                                }
                                create_dictionary_index_and_dictionary_for_class_skk_jisyo_special_core(object,
                                                                                                        aligned_block_dictionary,
                                                                                                        block,
                                                                                                        block_short,
                                                                                                        string,
                                                                                                        string_size,
                                                                                                        block_size_maximum,
                                                                                                        alignment_flag,
                                                                                                        line,
                                                                                                        top_line,
                                                                                                        index,
                                                                                                        top_offset,
                                                                                                        block_length,
                                                                                                        top_file_offset);
                                top_line = line;
                                top_offset = index;
                                ++block_length;
                                ++line;
                        }

                        object.seek(SkkJisyo::SEEK_POSITION_BOTTOM);
                        index = object.getIndex() + 1;
                        {
                                create_dictionary_index_and_dictionary_for_class_skk_jisyo_special_core(object,
                                                                                                        aligned_block_dictionary,
                                                                                                        block,
                                                                                                        block_short,
                                                                                                        string,
                                                                                                        string_size,
                                                                                                        block_size_maximum,
                                                                                                        alignment_flag,
                                                                                                        line,
                                                                                                        top_line,
                                                                                                        index,
                                                                                                        top_offset,
                                                                                                        block_length,
                                                                                                        top_file_offset);
                                top_offset = index;
                                ++block_length;
                        }
                }
                object.setIndex(backup_index);
                return true;
        }

        static bool create_dictionary_index_and_dictionary_for_class_skk_jisyo_normal_core(SkkJisyo &object,
                                                                                           FILE *aligned_block_dictionary,
                                                                                           FixedArray *fixed_array,
                                                                                           Block *block,
                                                                                           BlockShort *block_short,
                                                                                           char *string,
                                                                                           int *count_work,
                                                                                           int &string_size,
                                                                                           int block_size_maximum,
                                                                                           bool alignment_flag,
                                                                                           int line,
                                                                                           int top_line,
                                                                                           int index,
                                                                                           int top_offset,
                                                                                           int top_fixed_array,
                                                                                           int block_length)
        {
                if (block && block_short)
                {
// 両方のポインタが指定されていてはなりません。
                        DEBUG_PRINTF("ILLEGAL argument\n");
                        DEBUG_ASSERT(0);

                        return false;
                }

// 前のブロックの最終行の情報を得ます。
                int backup_index = object.getIndex();

                if (!object.seek(SkkJisyo::SEEK_POSITION_PREVIOUS))
                {
// ここで SEEK_POSITION_PREVIOUS が失敗するなどはありえないので不正な形式とします。
                        DEBUG_PRINTF("ILLEGAL dictionary\n");
                        DEBUG_ASSERT(0);
                        object.setIndex(backup_index);

                        return false;
                }

                if (fixed_array)
                {
                        if (*(count_work + top_fixed_array) == 0)
                        {
                                (fixed_array + top_fixed_array)->start_block = static_cast<int16_t>(block_length);
                                (fixed_array + top_fixed_array)->string_data_offset = string_size;
                        }
                        (fixed_array + top_fixed_array)->block_length = static_cast<int16_t>(block_length - (fixed_array + top_fixed_array)->start_block + 1);
                }

                if (block)
                {
                        if (aligned_block_dictionary &&
                            alignment_flag)
                        {
                                (block + block_length)->setOffset(static_cast<int>(ftell(aligned_block_dictionary)));
                        }
                        else
                        {
                                (block + block_length)->setOffset(top_offset);
                        }
                        (block + block_length)->setLineLengthAndDataSize(line - top_line, index - top_offset);
                }
                else if (block_short)
                {
                        (block_short + block_length)->setDataSize(index - top_offset);
                }

                {
                        const int terminator_size = 1;
                        int length = object.getMidasiSize() + terminator_size;
                        if (string)
                        {
                                char *p = object.getMidasiPointer();
                                int i;
                                for (i = 0; i != length - terminator_size; ++i)
                                {
                                        *(string + string_size + i) = *(p + i);
                                }
                                *(string + string_size + i) = ' ';
                        }
                        string_size += length;
                }

                if (aligned_block_dictionary)
                {
                        int size = index - top_offset;
                        if (fwrite(object.getBuffer() + top_offset, static_cast<size_t>(size), 1, aligned_block_dictionary) < 1)
                        {
                                object.setIndex(backup_index);

                                return false;
                        }
                        if (alignment_flag && (size < block_size_maximum))
                        {
                                fseek(aligned_block_dictionary, block_size_maximum - (size % block_size_maximum), SEEK_CUR);
                        }
                }

                ++*(count_work + top_fixed_array);

                object.setIndex(backup_index);

                return true;
        }


/// 辞書インデックスを取得します。同時にアラインメントされた辞書を作成できます。取得に失敗した場合は偽を返します。このとき DEBUG_ASSERT が有効ならばアサートします。
/**
 * block_length と string_size に block と string で確保すべきサイズを
 * 返します。block と string に 0 を渡すと、これらのバッファには触れま
 * せんので、典型的な呼び出し方として以下のように
 * create_block_and_aligned_block_dictionary() を 2 度呼び出すといった
 * 方法があります。
 *
 * \verbatim
 create_dictionary_index_and_dictionary_for_class_skk_jisyo_normal(object,
                                                                   0,
                                                                   fixed_array,
                                                                   0,            // block
                                                                   0,            // string
                                                                   block_length,
                                                                   string_size,
                                                                   block_size_maximum,
                                                                   alignment_flag);

 block = new Block[block_length];
 string = new char[string_size];

 create_dictionary_index_and_dictionary_for_class_skk_jisyo_normal(object,
                                                                   0,
                                                                   fixed_array,
                                                                   block,
                                                                   string
                                                                   block_length,
                                                                   string_size,
                                                                   block_size_maximum,
                                                                   alignment_flag);

\endverbatim
 *
 * もちろん block と string に十分なサイズを与えるか、block_length と
 * string_size が既知ならば 1 度の呼び出しだけでかまいません。
 *
 * aligned_block_dictionary に書き込み可能なファイルポインタを与えると
 * 辞書を出力します。 0 ならば何もしません。
 *
 * alignment_flag が真ならば各ブロックが block_size_maximum でアライン
 * メントされた辞書になります。
 */
        static bool create_dictionary_index_and_dictionary_for_class_skk_jisyo_normal(SkkJisyo &object,
                                                                                      FILE *aligned_block_dictionary,
                                                                                      FixedArray *fixed_array,
                                                                                      Block *block,
                                                                                      BlockShort *block_short,
                                                                                      char *string,
                                                                                      int &block_length,
                                                                                      int &string_size,
                                                                                      int block_size_maximum,
                                                                                      bool alignment_flag)
        {
                if (block && block_short)
                {
// 両方のポインタが指定されていてはなりません。
                        DEBUG_PRINTF("ILLEGAL argument\n");
                        DEBUG_ASSERT(0);
                        return false;
                }

                {
                        SkkJisyo::Information information;
                        object.getInformation(information);
                        if (information.get(SkkJisyo::Information::ID_NORMAL_LINES) == 0)
                        {
// normal が存在しない場合は長さ 0 で正常終了。
                                block_length = 0;
                                string_size = 0;
                                return true;
                        }
                }

                int backup_index = object.getIndex();
                int top_offset = 0;
                int count_work_table[256];
                int top_fixed_array;
                int line = 1;
                int top_line = 1;

                block_length = 0;
                string_size = 0;

                for (int i = 0; i != 256; ++i)
                {
                        count_work_table[i] = 0;
                }

                object.seek(SkkJisyo::SEEK_POSITION_TOP);
                top_fixed_array = object.getFixedArrayIndex();
                if (top_fixed_array < 0)
                {
                        DEBUG_ASSERT(0);
                        object.setIndex(backup_index);
                        return false;
                }

                while (object.seek(SkkJisyo::SEEK_POSITION_NEXT))
                {
                        ++line;
                        int next_fixed_array = object.getFixedArrayIndex();
                        if (next_fixed_array < 0)
                        {
                                DEBUG_PRINTF("ILLEGAL FIXED_ARRAY\n");
                                DEBUG_ASSERT(0);
                                object.setIndex(backup_index);
                                return false;
                        }

                        int index = object.getIndex();
                        if ((top_fixed_array != next_fixed_array) ||
                            (index - top_offset > block_size_maximum))
                        {
                                if (index - top_offset > block_size_maximum)
                                {
// ブロックサイズを越えたので 1 行戻します。
                                        object.seek(SkkJisyo::SEEK_POSITION_PREVIOUS);
                                        --line;
                                        index = object.getIndex();
                                        if (index <= top_offset)
                                        {
// 戻したにもかかわらず前と同じか小さいということは 1 要素が
// block_size_maximum より大きいこととなり、扱うことができません。
                                                SkkUtility::printf("block size error. (block size = %d  line size = %d)\n",
                                                                   block_size_maximum,
                                                                   object.getLineSize());
                                                DEBUG_PRINTF("ILLEGAL dictionary\n");
                                                DEBUG_ASSERT(0);
                                                object.setIndex(backup_index);
                                                return false;
                                        }
// 読み直したので再度 fixed_array を設定します。
                                        next_fixed_array = object.getFixedArrayIndex();
                                        if (next_fixed_array < 0)
                                        {
                                                DEBUG_PRINTF("ILLEGAL FIXED_ARRAY\n");
                                                DEBUG_ASSERT(0);
                                                object.setIndex(backup_index);
                                                return false;
                                        }
                                }

                                if (!create_dictionary_index_and_dictionary_for_class_skk_jisyo_normal_core(object,
                                                                                                            aligned_block_dictionary,
                                                                                                            fixed_array,
                                                                                                            block,
                                                                                                            block_short,
                                                                                                            string,
                                                                                                            count_work_table,
                                                                                                            string_size,
                                                                                                            block_size_maximum,
                                                                                                            alignment_flag,
                                                                                                            line,
                                                                                                            top_line,
                                                                                                            index,
                                                                                                            top_offset,
                                                                                                            top_fixed_array,
                                                                                                            block_length))
                                {
                                        // FIXME!
                                }

                                top_line = line;
                                top_offset = index;
                                top_fixed_array = next_fixed_array;
                                ++block_length;
                        }
                }

// 終端付近の処理です。
                {
                        object.seek(SkkJisyo::SEEK_POSITION_BOTTOM);
                        int index = object.getIndex() + 1;
                        if (index - top_offset > block_size_maximum)
                        {
// ブロックサイズを越えたので 1 行戻します。
                                object.seek(SkkJisyo::SEEK_POSITION_PREVIOUS);
                                --line;
                                index = object.getIndex();
                                if (index <= top_offset)
                                {
// 戻したにもかかわらず前と同じか小さいということは 1 要素が
// block_size_maximum より大きいこととなり、扱うことができません。
                                        DEBUG_PRINTF("ILLEGAL dictionary\n");
                                        DEBUG_ASSERT(0);
                                        object.setIndex(backup_index);

                                        return false;
                                }
                                create_dictionary_index_and_dictionary_for_class_skk_jisyo_normal_core(object,
                                                                                                       aligned_block_dictionary,
                                                                                                       fixed_array,
                                                                                                       block,
                                                                                                       block_short,
                                                                                                       string,
                                                                                                       count_work_table,
                                                                                                       string_size,
                                                                                                       block_size_maximum,
                                                                                                       alignment_flag,
                                                                                                       line,
                                                                                                       top_line,
                                                                                                       index,
                                                                                                       top_offset,
                                                                                                       top_fixed_array,
                                                                                                       block_length);
                                top_line = line;
                                top_offset = index;
                                ++block_length;
                                ++line;
                        }

                        object.seek(SkkJisyo::SEEK_POSITION_BOTTOM);
                        index = object.getIndex() + 1;
                        {
                                create_dictionary_index_and_dictionary_for_class_skk_jisyo_normal_core(object,
                                                                                                       aligned_block_dictionary,
                                                                                                       fixed_array,
                                                                                                       block,
                                                                                                       block_short,
                                                                                                       string,
                                                                                                       count_work_table,
                                                                                                       string_size,
                                                                                                       block_size_maximum,
                                                                                                       alignment_flag,
                                                                                                       line,
                                                                                                       top_line,
                                                                                                       index,
                                                                                                       top_offset,
                                                                                                       top_fixed_array,
                                                                                                       block_length);
                                top_offset = index;
                                ++block_length;
                        }
                }

                object.setIndex(backup_index);

                return true;
        }

public:
        static bool getJisyoType(const char *filename, JisyoType &type)
        {
                DEBUG_ASSERT_POINTER(filename);
                bool result;
                FILE *file = fopen(filename, "rb");
                if (file == 0)
                {
                        type = JISYO_TYPE_UNKNOWN;
                        result = false;
                }
                else
                {
                        result = true;
                        char tmp[1024];
                        if (fread(tmp, 1024, 1, file) < 1)
                        {
                                type = JISYO_TYPE_UNKNOWN;
                                result = false;
                        }
                        else
                        {
                                if (tmp[0] == ';')
                                {
// 1 文字目が ; ならば高確率で SKK 辞書なので、かなりいいかげんですが
// SKK 辞書としてしまいます。
                                        type = JISYO_TYPE_SKK_RAW;
                                }
                                else
                                {
                                        if (fseek(file, -static_cast<long>(sizeof(Information)), SEEK_END) == -1)
                                        {
                                                type = JISYO_TYPE_UNKNOWN;
                                                result = false;
                                        }
                                        else
                                        {
                                                if (ftell(file) & 0x3)
                                                {
// Information があるべき位置が 4 の倍数でなければ知らない形式です。
                                                        type = JISYO_TYPE_UNKNOWN;
                                                }
                                                else
                                                {
                                                        Information tmp_information;

                                                        if (fread(&tmp_information, sizeof(tmp_information), 1, file) < 1)
                                                        {
                                                                type = JISYO_TYPE_UNKNOWN;
                                                                result = false;
                                                        }
                                                        else
                                                        {
                                                                if (tmp_information.get(Information::ID_IDENTIFIER) != IDENTIFIER)
                                                                {
// IDENTIFIER が一致しないので知らない形式です。
                                                                        type = JISYO_TYPE_UNKNOWN;
                                                                }
                                                                else
                                                                {
                                                                        if (tmp_information.get(Information::ID_INDEX_DATA_SIZE) == 0)
                                                                        {
// インデックスデータが存在しないので class SkkJisyo 用です。
                                                                                type = JISYO_TYPE_CLASS_SKK_JISYO;
                                                                        }
                                                                        else
                                                                        {
                                                                                type = JISYO_TYPE_CLASS_SKK_DICTIONARY;
                                                                        }
                                                                }
                                                        }
                                                }
                                        }
                                }
                        }
                        fclose(file);
                }
                return result;
        }

/// SKK 辞書を class SkkJisyo 形式の辞書へ変換します。変換に失敗した場合は偽を返します。
/**
 * class SkkJisyo で扱う形式の辞書にはインデックスデータが存在しません。
 */
        static bool createDictionaryForClassSkkJisyo(const char *filename_source, const char *filename_destination)
        {
                DEBUG_ASSERT_POINTER(filename_source);
                DEBUG_ASSERT_POINTER(filename_destination);
                bool result = true;
                SkkMmap mmap;
                char *buffer = static_cast<char*>(mmap.map(filename_source));
                if (buffer == 0)
                {
                        result = false;
                }
                else
                {
                        int filesize = mmap.getFilesize();
                        if (filesize < SkkUtility::getStringOkuriAriLength() + SkkUtility::getStringOkuriNasiLength())
                        {
                                result = false;
                        }
                        else
                        {
                                int okuri_ari_index = 0;
                                int okuri_nasi_index = 0;
// まず okuri_ari_index と okuri_nasi_index を求めます。
                                if (!get_index(buffer, filesize, okuri_ari_index, okuri_nasi_index))
                                {
                                        result = false;
                                }
                                else
                                {
                                        result = create_dictionary_for_class_skk_jisyo(filename_destination, buffer, filesize, okuri_ari_index, okuri_nasi_index);
                                }
                        }
                }
                return result;
        }

/// SKK 辞書を block_size の class SkkDictionary 形式の辞書へ変換します。変換に失敗した場合は偽を返します。
        static bool createDictionaryForClassSkkDictionary(const char *filename_source,
                                                          const char *filename_destination,
                                                          int block_size,
                                                          bool alignment_flag = false,
                                                          bool block_short_flag = false)
        {
                DEBUG_ASSERT_POINTER(filename_source);
                DEBUG_ASSERT_POINTER(filename_destination);
                bool result;
                SkkJisyo object;
                FixedArray *fixed_array = 0;
                Block *block = 0;
                BlockShort *block_short = 0;
                char *string = 0;
                int normal_block_length = 0;
                int normal_string_size = 0;
                int special_block_length = 0;
                int special_string_size = 0;
                if (alignment_flag)
                {
                        char tmp_filename[] = "/tmp/skkjisyo.XXXXXX";
// 美しくない上に問題のある方法ですが、テンポラリファイル名を得るためだ
// けに mkstemp() を利用しています。厳密にはこの使い方ではテンポラリファ
// イル名がユニークであることは保証されないことに注意が必要です。
                        int tmp_fd = mkstemp(tmp_filename);
                        if (tmp_fd == -1)
                        {
                                return false;
                        }
                        else
                        {
                                ::close(tmp_fd);
                                result = createDictionaryForClassSkkJisyo(filename_source, tmp_filename);
                                if (result)
                                {
                                        result = object.open(tmp_filename);
                                        if (result)
                                        {
                                                result = object.createDictionaryIndex(block_size, alignment_flag, block_short_flag);
                                                if (result)
                                                {
                                                        result = object.getDictionaryIndexInformation(fixed_array,
                                                                                                      block,
                                                                                                      block_short,
                                                                                                      string,
                                                                                                      normal_block_length,
                                                                                                      normal_string_size,
                                                                                                      special_block_length,
                                                                                                      special_string_size);
                                                }
                                                if (result)
                                                {
                                                        if (unlink(tmp_filename) == -1)
                                                        {
                                                                result = false;
                                                        }
                                                        else
                                                        {
                                                                result = object.createDictionaryIndexAndDictionaryForClassSkkJisyo(block_size,
                                                                                                                                   filename_destination,
                                                                                                                                   alignment_flag,
                                                                                                                                   block_short_flag);
                                                        }
                                                }
                                        }
                                }
                        }
                }
                else
                {
                        result = createDictionaryForClassSkkJisyo(filename_source, filename_destination);
                        if (result)
                        {
                                result = object.open(filename_destination);
                                if (result)
                                {
                                        result = object.createDictionaryIndex(block_size);
                                        if (result)
                                        {
                                                result = object.getDictionaryIndexInformation(fixed_array,
                                                                                              block,
                                                                                              block_short,
                                                                                              string,
                                                                                              normal_block_length,
                                                                                              normal_string_size,
                                                                                              special_block_length,
                                                                                              special_string_size);
                                        }
                                }
                        }
                }

                if (!result)
                {
                        unlink(filename_destination);
                }
                else
                {
                        FILE *file = fopen(filename_destination, "r+b");
                        if (file == 0)
                        {
                                result = false;
                        }
                        else
                        {
                                if (fseek(file, -static_cast<long>(sizeof(Information)), SEEK_END) == -1)
                                {
                                        result = false;
                                }

                                Information tmp_information;
                                if (result && (fread(&tmp_information, sizeof(tmp_information), 1, file) < 1))
                                {
                                        result = false;
                                }
                                if (result)
                                {
                                        if (fseek(file, -static_cast<long>(sizeof(Information)), SEEK_END) == -1)
                                        {
                                                result = false;
                                        }
                                        IndexDataHeader index_data_header;
                                        {
                                                int32_t tmp = index_data_header.get(IndexDataHeader::ID_BIT_FLAG);
                                                tmp |= block_short_flag ? IndexDataHeader::BIT_FLAG_BLOCK_SHORT : 0;
                                                index_data_header.set(IndexDataHeader::ID_BIT_FLAG, tmp);
                                        }
                                        index_data_header.set(IndexDataHeader::ID_BLOCK_SIZE, block_size);
                                        index_data_header.set(IndexDataHeader::ID_NORMAL_BLOCK_LENGTH, normal_block_length);
                                        index_data_header.set(IndexDataHeader::ID_SPECIAL_BLOCK_LENGTH, special_block_length);
                                        index_data_header.set(IndexDataHeader::ID_NORMAL_STRING_SIZE, normal_string_size);
                                        index_data_header.set(IndexDataHeader::ID_SPECIAL_STRING_SIZE, special_string_size);
                                        index_data_header.set(IndexDataHeader::ID_SPECIAL_ENTRY_OFFSET, tmp_information.get(Information::ID_NORMAL_SIZE));
                                        if (result)
                                        {
                                                int index_data_offset = static_cast<int>(ftell(file));
                                                if (result)
                                                {
                                                        const size_t size_of_block = block ? sizeof(Block) : sizeof(BlockShort);
                                                        struct
                                                        {
                                                                const void *p;
                                                                size_t size;
                                                        }
                                                        table[] =
                                                        {
                                                                { &index_data_header,
                                                                  sizeof(index_data_header), },
                                                                { fixed_array,
                                                                  sizeof(FixedArray) * 256, },
                                                                { block,
                                                                  sizeof(Block) * static_cast<size_t>(normal_block_length + special_block_length), },
                                                                { block_short,
                                                                  sizeof(BlockShort) * static_cast<size_t>(normal_block_length + special_block_length), },
                                                                { string,
                                                                  static_cast<size_t>(normal_string_size + special_string_size), },
                                                                { 0,
                                                                  0, },
                                                        };

                                                        for (int i = 0;; ++i)
                                                        {
                                                                if ((table[i].p == 0) && (table[i].size == 0))
                                                                {
                                                                        break;
                                                                }
                                                                if (table[i].p)
                                                                {
                                                                        if (fwrite(table[i].p, table[i].size, 1, file) < 1)
                                                                        {
                                                                                result = false;
                                                                                break;
                                                                        }
                                                                }
                                                        }

                                                        if (result)
                                                        {
                                                                result = append_terminator(file);
                                                        }

                                                        if (result)
                                                        {
                                                                int index_data_size = static_cast<int>(sizeof(index_data_header) +
                                                                                                       sizeof(FixedArray) * 256 +
                                                                                                       static_cast<size_t>(size_of_block * static_cast<size_t>(normal_block_length + special_block_length)) +
                                                                                                       static_cast<size_t>(normal_string_size + special_string_size));
                                                                tmp_information.set(Information::ID_INDEX_DATA_SIZE,
                                                                                    index_data_size);
                                                                tmp_information.set(Information::ID_INDEX_DATA_OFFSET,
                                                                                    index_data_offset);
                                                                if (fwrite(&tmp_information, sizeof(tmp_information), 1, file) < 1)
                                                                {
                                                                        result = false;
                                                                }
                                                        }
                                                }
                                        }
                                }

                                fclose(file);
                                SkkUtility::chmod(filename_destination, 0644);
                        }
                }

                return result;
        }

/// 指定した辞書ファイルから Information を取得します。取得に失敗した場合は偽を返します。
        static bool getInformation(const char *filename, Information &information)
        {
                DEBUG_ASSERT_POINTER(filename);
                JisyoType type;
                bool result = getJisyoType(filename, type);
                if ((type == JISYO_TYPE_CLASS_SKK_JISYO) || (type == JISYO_TYPE_CLASS_SKK_DICTIONARY))
                {
                        FILE *file = fopen(filename, "rb");
                        if (file == 0)
                        {
                                result = false;
                        }
                        else
                        {
                                result = true;
                                if (fseek(file, -static_cast<long>(sizeof(Information)), SEEK_END) == -1)
                                {
                                        result = false;
                                }
                                if (result && (fread(&information, sizeof(information), 1, file) < 1))
                                {
                                        result = false;
                                }
                                fclose(file);
                        }
                }
                else
                {
                        result = false;
                }
                return result;
        }

/// 指定した辞書ファイルから IndexDataHeader を取得します。取得に失敗した場合は偽を返します。
        static bool getIndexDataHeader(const char *filename, IndexDataHeader &index_data_header)
        {
                DEBUG_ASSERT_POINTER(filename);
                JisyoType type;
                bool result = getJisyoType(filename, type);
                if (type == JISYO_TYPE_CLASS_SKK_DICTIONARY)
                {
                        FILE *file = fopen(filename, "rb");
                        if (file == 0)
                        {
                                result = false;
                        }
                        else
                        {
                                result = true;
                                if (fseek(file, -static_cast<long>(sizeof(Information)), SEEK_END) == -1)
                                {
                                        result = false;
                                }
                                Information tmp_information;
                                if (result && (fread(&tmp_information, sizeof(tmp_information), 1, file) < 1))
                                {
                                        result = false;
                                }
                                int index_data_offset = tmp_information.get(Information::ID_INDEX_DATA_OFFSET);
                                if (result && (fseek(file, index_data_offset, SEEK_SET) == -1))
                                {
                                        result = false;
                                }
                                if (result && (fread(&index_data_header, sizeof(index_data_header), 1, file) < 1))
                                {
                                        result = false;
                                }
                                fclose(file);
                        }
                }
                else
                {
                        result = false;
                }
                return result;
        }

        virtual ~SkkJisyo()
        {
                close();
        }

        SkkJisyo() :
                mmap_(0),
                filename_buffer_(0),
                state_(STATE_NORMAL),
                information_(),

                fixed_array_(0),
                block_(0),
                block_short_(0),
                string_(0),
                normal_block_length_(0),
                normal_string_size_(0),
                special_block_length_(0),
                special_string_size_(0),

                open_failure_flag_(false)
        {
                for (int i = 0; i != STATE_LENGTH; ++i)
                {
                        buffer_table_[i] = 0;
                        index_table_[i] = 0;
                        size_table_[i] = 0;
                }
        }

/// 辞書インデックスをオブジェクト内に生成し、 filename_destination が指定されていれば class SkkJisyo 形式の辞書を書き出します。変換に失敗した場合は偽を返します。
/**
 * 辞書は block_size でアラインメントされます。
 *
 * 辞書インデックスは filename_destination へ 0 を指定して生成します。
 * createDictionaryIndex() は、まさにそのような実装になっています。
 */
        bool createDictionaryIndexAndDictionaryForClassSkkJisyo(int block_size,
                                                                const char *filename_destination,
                                                                bool alignment_flag = false,
                                                                bool block_short_flag = false)
        {
                int normal_block_length;
                int normal_string_size;
                int special_block_length = 0;
                int special_string_size = 0;
                FILE *file;
                if (filename_destination)
                {
                        file = fopen(filename_destination, "wb");
                        if (file == 0)
                        {
                                return false;
                        }
                }
                else
                {
                        file = 0;
                }
                if (fixed_array_ == 0)
                {
                        fixed_array_ = new FixedArray[256];
                }

                bool result;
                if (normal_block_length_ == 0)
                {
                        setState(SkkJisyo::STATE_NORMAL);
                        result = create_dictionary_index_and_dictionary_for_class_skk_jisyo_normal(*this,
                                                                                                   0,
                                                                                                   fixed_array_,
                                                                                                   0,
                                                                                                   0,
                                                                                                   0,
                                                                                                   normal_block_length,
                                                                                                   normal_string_size,
                                                                                                   block_size,
                                                                                                   alignment_flag);
                        setState(SkkJisyo::STATE_SPECIAL);
                        if (result)
                        {
                                result = create_dictionary_index_and_dictionary_for_class_skk_jisyo_special(*this,
                                                                                                            0,
                                                                                                            0,
                                                                                                            0,
                                                                                                            0,
                                                                                                            special_block_length,
                                                                                                            special_string_size,
                                                                                                            block_size,
                                                                                                            alignment_flag);
                                normal_block_length_ = normal_block_length;
                                normal_string_size_ = normal_string_size;
                                special_block_length_ = special_block_length;
                                special_string_size_ = special_string_size;
                        }
                        setState(SkkJisyo::STATE_NORMAL);
                }
                else
                {
                        result = true;
                }

                if (result)
                {
                        if (string_ == 0)
                        {
                                string_ = new char[normal_string_size_ + special_string_size_];
                                SkkUtility::clearMemory(string_, normal_string_size_ + special_string_size_);
                        }
                        else
                        {
                                // 既に確保されている。
                        }

// block_ と block_short_ は同時に設定してはならないため、片方に確保し
// た場合は、もう片方を解放し 0 を設定します。
                        if (block_short_flag)
                        {
                                if (block_short_ == 0)
                                {
                                        delete[] block_;
                                        block_ = 0;
                                        block_short_ = new BlockShort[normal_block_length_ + special_block_length_];
                                }
                                else
                                {
                                        // 既に確保されています。
                                }
                        }
                        else
                        {
                                if (block_ == 0)
                                {
                                        delete[] block_short_;
                                        block_short_ = 0;

                                        block_ = new Block[normal_block_length_ + special_block_length_];
                                }
                                else
                                {
                                        // 既に確保されています。
                                }
                        }

                        if (result)
                        {
                                setState(SkkJisyo::STATE_NORMAL);
                                result = create_dictionary_index_and_dictionary_for_class_skk_jisyo_normal(*this,
                                                                                                           file,
                                                                                                           fixed_array_,
                                                                                                           block_,
                                                                                                           block_short_,
                                                                                                           string_,
                                                                                                           normal_block_length,
                                                                                                           normal_string_size,
                                                                                                           block_size,
                                                                                                           alignment_flag);
                                int normal_size;
                                if (file)
                                {
                                        normal_size = static_cast<int>(ftell(file));
                                }
                                else
                                {
                                        normal_size = 0;
                                }

                                if (result)
                                {
                                        setState(SkkJisyo::STATE_SPECIAL);
                                        Block *tmp_block = 0;
                                        BlockShort *tmp_block_short = 0;
                                        if (block_)
                                        {
                                                tmp_block = block_ + normal_block_length;
                                        }
                                        if (block_short_)
                                        {
                                                tmp_block_short = block_short_ + normal_block_length;
                                        }
                                        result = create_dictionary_index_and_dictionary_for_class_skk_jisyo_special(*this,
                                                                                                                    file,
                                                                                                                    tmp_block,
                                                                                                                    tmp_block_short,
                                                                                                                    string_ + normal_string_size,
                                                                                                                    special_block_length,
                                                                                                                    special_string_size,
                                                                                                                    block_size,
                                                                                                                    alignment_flag);
                                        setState(SkkJisyo::STATE_NORMAL);
                                }

                                if (result && file)
                                {
                                        int special_size = static_cast<int>(ftell(file)) - normal_size;
                                        append_information(file,
                                                           block_size,
                                                           normal_size,
                                                           information_.get(Information::ID_NORMAL_LINES),
                                                           special_size,
                                                           information_.get(Information::ID_SPECIAL_LINES),
                                                           0,
                                                           0);
                                }
                        }
                }

                if (file)
                {
                        fclose(file);
                }

                return result;
        }

/// 辞書インデックスをオブジェクト内に生成します。生成に失敗した場合は偽を返します。
        bool createDictionaryIndex(int block_size, bool alignment_flag = false, bool block_short_flag = false)
        {
                return createDictionaryIndexAndDictionaryForClassSkkJisyo(block_size,
                                                                          0,
                                                                          alignment_flag,
                                                                          block_short_flag);
        }

/// 辞書インデックスの情報を得ます。取得に失敗した場合は偽を返し、 DEBUG_ASSERT が有効ならばアサートします。
        bool getDictionaryIndexInformation(FixedArray *&fixed_array,
                                           Block *&block,
                                           BlockShort *&block_short,
                                           char *&string,
                                           int &normal_block_length,
                                           int &normal_string_size,
                                           int &special_block_length,
                                           int &special_string_size)
        {
                if ((fixed_array_ == 0) ||
                    ((block_ == 0) && (block_short_ == 0)) ||
                    (string_ == 0))
                {
                        DEBUG_ASSERT(0);
                        return false;
                }
                else
                {
                        fixed_array = fixed_array_;
                        block = block_;
                        block_short = block_short_;
                        string = string_;
                        normal_block_length = normal_block_length_;
                        normal_string_size = normal_string_size_;
                        special_block_length = special_block_length_;
                        special_string_size = special_string_size_;
                        return true;
                }
        }

        bool close()
        {
                delete[] string_;
                delete[] block_;
                delete[] fixed_array_;

                delete mmap_;
                delete[] filename_buffer_;

                mmap_ = 0;
                filename_buffer_ = 0;
                state_ = STATE_NORMAL;
//                 information_;

                fixed_array_ = 0;
                block_ = 0;
                string_ = 0;
                normal_block_length_ = 0;
                normal_string_size_ = 0;
                special_block_length_ = 0;
                special_string_size_ = 0;

                open_failure_flag_ = false;

                for (int i = 0; i != STATE_LENGTH; ++i)
                {
                        buffer_table_[i] = 0;
                        index_table_[i] = 0;
                        size_table_[i] = 0;
                }
                return true;
        }

/// 辞書をオープンします。正常にオープンできれば真を返します。失敗した場合は偽を返し、 DEBUG_ASSERT が有効ならばアサートします。
/**
 * class SkkJisyo で使用できる辞書は
 * 「createDictionaryForClassSkkJisyo()」または
 * 「createDictionaryIndexAndDictionaryForClassSkkJisyo() アラインメン
 * トなし」で生成されたものです。
 *
 * 通常の SKK-JISYO やアラインメントされた辞書は class SkkJisyo では使
 * 用できないため、オープンに失敗します。
 */
        bool open(const char *filename)
        {
                DEBUG_ASSERT_POINTER(filename);

                int length = 0;
                const int limit = 4096;
                const int margin = 16;

                while (*(filename + length) != '\0')
                {
                        ++length;
                }

                if ((length == 0) || (length > limit - margin))
                {
                        DEBUG_ASSERT(0);
                        open_failure_flag_ = true;

                        return false;
                }
                else
                {
                        filename_buffer_ = new char[length + margin];

                        int i = 0;
                        for (i = 0; *(filename + i) != '\0'; ++i)
                        {
                                *(filename_buffer_ + i) = *(filename + i);
                        }
                        *(filename_buffer_ + i) = '\0';
                        mmap_ = new SkkMmap();
                        buffer_table_[STATE_NORMAL] = static_cast<char*>(mmap_->map(filename_buffer_));
                        if (buffer_table_[STATE_NORMAL] == 0)
                        {
                                DEBUG_ASSERT(0);
                                open_failure_flag_ = true;

                                return false;
                        }

                        if (*buffer_table_[STATE_NORMAL] == ';')
                        {
// 先頭 1 バイトが ';' なデータは通常の SKK-JISYO の可能性が高いため
// (少なくとも class SkkJisyo では不正な形式なので) 失敗します。
                                DEBUG_ASSERT(0);
                                open_failure_flag_ = true;

                                return false;
                        }

                        information_.initialize(buffer_table_[STATE_NORMAL] + mmap_->getFilesize() - Information::getSize());

                        if ((information_.get(Information::ID_IDENTIFIER) != IDENTIFIER) ||
                            (information_.get(Information::ID_BLOCK_ALIGNMENT_SIZE) != 0))
                        {
// IDENTIFIER が不正、またはアラインメントされた辞書ならば失敗します。
                                open_failure_flag_ = true;

                                return false;
                        }

                        size_table_[STATE_NORMAL] = information_.get(Information::ID_NORMAL_SIZE);
                        size_table_[STATE_SPECIAL] = information_.get(Information::ID_SPECIAL_SIZE);

                        buffer_table_[STATE_SPECIAL] = buffer_table_[STATE_NORMAL] + size_table_[STATE_NORMAL];

                        if (buffer_table_[STATE_NORMAL])
                        {
                                seek(SEEK_POSITION_TOP);
                        }
                }

                return true;
        }

        void getInformation(Information &information)
        {
                information = information_;
        }

        State getState()
        {
                return state_;
        }

        void setState(State state)
        {
                state_ = state;
        }

        char *getBuffer()
        {
                return buffer_table_[state_];
        }

        int getIndex()
        {
                return index_table_[state_];
        }

        void setIndex(int index)
        {
                index_table_[state_] = index;
        }

/// 指定位置にシークします。バッファ外をアクセスしようとした場合に偽を返します。
/**
 *
 * 偽を返す条件は以下の通り
 *
 * \li インデックスが -1 。つまりバッファ終端に差し掛かった。
 *
 * \li バッファ先端から(インデックスが 0 で)更に前の行へシークしようとした。
 *
 * なお、先端に差し掛かっただけではインデックスが 0 になるだけで、偽を
 * 返さないことに注意が必要です。 (インデックス 0 の状態で更にシークし
 * て初めて偽を返す)
 *
 */
        bool seek(SeekPosition position)
        {
                if (open_failure_flag_)
                {
                        return false;
                }

                bool result = true;
                switch (position)
                {
                default:
                        DEBUG_ASSERT(0);
                        break;
                case SEEK_POSITION_TOP:
                        index_table_[state_] = 0;
                        break;
                case SEEK_POSITION_BOTTOM:
                        index_table_[state_] = size_table_[state_] - 1;
                        break;
                case SEEK_POSITION_NEXT:
                        index_table_[state_] = SkkUtility::getNextLineIndex(buffer_table_[state_], index_table_[state_], size_table_[state_]);
                        break;
                case SEEK_POSITION_PREVIOUS:
                        if (index_table_[state_] == 0)
                        {
                                result = false;
                        }
                        else
                        {
                                index_table_[state_] = SkkUtility::getPreviousLineIndex(buffer_table_[state_], index_table_[state_], size_table_[state_]);
                        }
                        break;
                case SEEK_POSITION_BEGINNING_OF_LINE:
                        index_table_[state_] = SkkUtility::getBeginningOfLineIndex(buffer_table_[state_], index_table_[state_], size_table_[state_]);
                        break;
                }

                if (index_table_[state_] < 0)
                {
                        result = false;
                }

                return result;
        }

/// 指定文字を探索します。本メソッドは内部でステートを変更することに注意が必要です。「見出し」はスペースでターミネートされます。見付からなければ偽を返します。
        bool search(const char *midasi)
        {
                DEBUG_ASSERT_POINTER(midasi);
                if (open_failure_flag_)
                {
                        return false;
                }

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

// まずバイナリサーチで探索し、見付からない場合はバイナリサーチの出力を
// 利用した上で線型探索します。
                if (SkkUtility::getFixedArrayIndex(encoded_midasi) == -1)
                {
                        state_ = STATE_SPECIAL;
                        int index;
                        if (SkkUtility::searchBinary(buffer_table_[state_], size_table_[state_], encoded_midasi, index))
                        {
                                setIndex(index);
                                return true;
                        }
                        else
                        {
                                if (SkkUtility::searchLinear(buffer_table_[state_], size_table_[state_], encoded_midasi, index))
                                {
                                        setIndex(index);
                                        return true;
                                }
                        }
                }
                else
                {
                        state_ = STATE_NORMAL;
                        int index;
                        if (SkkUtility::searchBinary(buffer_table_[state_], size_table_[state_], encoded_midasi, index))
                        {
                                setIndex(index);
                                return true;
                        }
                        else
                        {
                                if (SkkUtility::searchLinear(buffer_table_[state_], size_table_[state_], encoded_midasi, index))
                                {
                                        setIndex(index);
                                        return true;
                                }
                        }
                }
                return false;
        }

        int getMidasiSize()
        {
                return SkkUtility::getMidasiSize(buffer_table_[state_], getIndex(), size_table_[state_]);
        }

        char *getMidasiPointer()
        {
                return buffer_table_[state_] + getIndex();
        }

/// index が行頭位置にあるとして、「変換文字列」のサイズを返します。サイズに改行文字は含みません。
        int getHenkanmojiretsuSize()
        {
                return SkkUtility::getHenkanmojiretsuSize(buffer_table_[state_], getIndex(), size_table_[state_]);
        }

/// index が行頭位置にあるとして、「変換文字列」のポインタを返します。
        const char *getHenkanmojiretsuPointer()
        {
                return SkkUtility::getHenkanmojiretsuPointer(buffer_table_[state_], getIndex(), size_table_[state_]);
        }

/// index が行頭位置にあるとして、 1 行のサイズを返します。サイズに改行文字は含みません。
        int getLineSize()
        {
                return SkkUtility::getLineSize(buffer_table_[state_], getIndex(), size_table_[state_]);
        }

        int getFixedArrayIndex()
        {
                return SkkUtility::getFixedArrayIndex(buffer_table_[state_] + getIndex());
        }

private:
        SkkMmap *mmap_;
        char *filename_buffer_;
        State state_;
        Information information_;

        FixedArray *fixed_array_;
        Block *block_;
        BlockShort *block_short_;
        char *string_;
        int normal_block_length_;
        int normal_string_size_;
        int special_block_length_;
        int special_string_size_;

        bool open_failure_flag_;

        char *buffer_table_[STATE_LENGTH];
        int index_table_[STATE_LENGTH];
        int size_table_[STATE_LENGTH];
};
}

#endif  // SKK_JISYO_H
