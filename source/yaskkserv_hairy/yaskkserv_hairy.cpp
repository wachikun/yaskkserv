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

#include "skk_dictionary.hpp"
#include "skk_server.hpp"
#include "skk_utility.hpp"
#include "skk_command_line.hpp"
#include "skk_simple_string.hpp"

namespace YaSkkServ
{
namespace
{
#define SERVER_IDENTIFIER "hairy"
char version_string[] = YASKKSERV_VERSION ":yaskkserv_" SERVER_IDENTIFIER " ";

#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
class GoogleJapaneseInput
{
        GoogleJapaneseInput(GoogleJapaneseInput &source);
        GoogleJapaneseInput& operator=(GoogleJapaneseInput &source);

public:
        virtual ~GoogleJapaneseInput()
        {
                delete[] skk_output_buffer_;
                delete[] http_receive_buffer_;
                delete[] http_send_buffer_;
                delete[] work_b_buffer_;
                delete[] work_a_buffer_;
        }

        GoogleJapaneseInput() :
                skk_socket_(),
                work_a_buffer_size_(4 * 1024),
                work_b_buffer_size_(4 * 1024),
                http_send_buffer_size_(4 * 1024),
                http_receive_buffer_size_(4 * 1024),
                skk_output_buffer_size_(4 * 1024),
                work_a_buffer_(new char[work_a_buffer_size_]),
                work_b_buffer_(new char[work_b_buffer_size_]),
                http_send_buffer_(new char[http_send_buffer_size_]),
                http_receive_buffer_(new char[http_receive_buffer_size_]),
                skk_output_buffer_(new char[skk_output_buffer_size_]),
                http_send_string_(http_send_buffer_, http_send_buffer_size_)
        {
        }

        // 成功すれば、内部バッファのポインタを、失敗した場合は 0 を返します。終端コードはスペースである必要があります。
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
                if (search_word_size >= work_b_buffer_size_ / 5)
                {
                        return 0;
                }
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
                                                *(work_b_buffer_ + i - 1) = '\0';
                                                break;
                                        }
                                }
                                *(work_b_buffer_ + i) = '\0';
                                break;
                        }
                        *(work_b_buffer_ + i) = static_cast<char>(c);
                }
                if (!convertIconv(work_b_buffer_, work_a_buffer_, work_a_buffer_size_, "euc-jp", "utf-8"))
                {
                        return 0;
                }
                if (!convertUtf8ToUrl(work_a_buffer_, work_b_buffer_, work_b_buffer_size_))
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
                const int margin = 128;
                if (!confirmSkk(skk_output_buffer_, skk_output_buffer_ + skk_output_buffer_size_ - margin))
                {
                        return 0;
                }
                return skk_output_buffer_;
        }
        static int getByteSize(const char *p)
        {
                int byte_size = 0;
                while (*(p + byte_size++)) {}
                return byte_size - 1;
        }

private:

        // from_code の source を to_code に変換して destination に書き込みます。成功すれば真を返します。失敗した場合の destination の内容は不定です。
        static bool convertIconv(const char *source, char *destination, int destination_size, const char *from_code, const char *to_code)
        {
                iconv_t icd = iconv_open(to_code, from_code);
                if (icd == reinterpret_cast<iconv_t>(-1))
                {
                        return false;
                }
                else
                {
                        const int nul_size = 1;
                        int byte_size = getByteSize(source) + nul_size;
                        char *in_buffer = const_cast<char*>(source);
                        char *out_buffer = destination;
                        size_t in_size = byte_size;
                        size_t out_size = destination_size;
                        for (;;)
                        {
                                size_t result = iconv(icd, &in_buffer, &in_size, &out_buffer, &out_size);
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
        static bool convertEscape4ToUtf8(const char *escape, char *utf8)
        {
                int previous_c = 0;
                for (;;)
                {
                        int c = static_cast<int>(static_cast<unsigned char>(*escape++));
                        if (c == 0)
                        {
                                *utf8++ = static_cast<char>(c);
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
                                        if (!getEscapeBinary(static_cast<int>(static_cast<unsigned char>(*escape++)), bin_0))
                                        {
                                                return false;
                                        }
                                        if (!getEscapeBinary(static_cast<int>(static_cast<unsigned char>(*escape++)), bin_1))
                                        {
                                                return false;
                                        }
                                        if (!getEscapeBinary(static_cast<int>(static_cast<unsigned char>(*escape++)), bin_2))
                                        {
                                                return false;
                                        }
                                        if (!getEscapeBinary(static_cast<int>(static_cast<unsigned char>(*escape++)), bin_3))
                                        {
                                                return false;
                                        }
                                        int unicode = (bin_0 << 12) | (bin_1 << 8) | (bin_2 << 4) | bin_3;
                                        int utf8_data = (((unicode & 0xf000) << 4) | 0xe00000) | (((unicode & 0xfc0) << 2) | 0x8000) | ((unicode & 0x3f) | 0x80);
                                        *utf8++ = static_cast<char>((utf8_data >> 16) & 0xff);
                                        *utf8++ = static_cast<char>((utf8_data >> 8) & 0xff);
                                        *utf8++ = static_cast<char>(utf8_data & 0xff);
                                }
                                else
                                {
                                        *utf8++ = static_cast<char>(c);
                                }
                        }
                        else
                        {
                                *utf8++ = static_cast<char>(c);
                        }
                        previous_c = c;
                }
                return true;
        }

        static void encodeUrl(char *&buffer, int code)
        {
                static const char table[] = "0123456789ABCDEF";
                *buffer++ = '%';
                *buffer++ = table[(code >> 4) & 0xf];
                *buffer++ = table[code & 0xf];
        }

        static bool convertUtf8ToUrl(const char *source, char *destination, int destination_size)
        {
                int work_a_use_size = getByteSize(source);
                // URL encode が 1byte -> 3byte なので、
                // SkkSimpleString のマージンなどを多目にみて 4 倍で計
                // 算。
                if (work_a_use_size >= destination_size / 4)
                {
                        return false;
                }
                const char *utf8 = source;
                char *url = destination;
                for (;;)
                {
                        int c = static_cast<int>(static_cast<unsigned char>(*utf8++));
                        if (c == 0)
                        {
                                break;
                        }
                        encodeUrl(url, c);
                }
                *url = '\0';
                return true;
        }

        bool getHttp(const char *url_encode_word, float timeout)
        {
                http_send_string_.reset();
                http_send_string_.append("GET http://www.google.com/transliterate?langpair=ja-Hira|ja&text=");
                http_send_string_.append(url_encode_word);
                http_send_string_.append(",\x0d\x0a\x0d\x0a");

                int socket_fd = skk_socket_.prepareASyncSocket("www.google.com", "http", timeout);
                bool result = socket_fd >= 0;
                if (result)
                {
                        int send_result = SkkSocket::send(socket_fd, http_send_string_.getBuffer(), http_send_string_.getSize());
                        if (send_result == -1)
                        {
                                if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
                                {
                                        result = false;
                                }
                        }
                        if (result)
                        {
                                int receive_buffer_index = 0;
                                SkkUtility::WaitAndCheckForGoogleJapaneseInput wait_and_check_for_google_japanese_input;
                                for (;;)
                                {
                                        // select() ではなくポーリングでループ処理していることに注意が必要です。
                                        // これは receive() の時間もタイムアウトとして考慮するためです。
                                        // (まあ、実際にはあまり変わらないはずですが)
                                        if (!wait_and_check_for_google_japanese_input.waitAndCheckLoopHead(timeout))
                                        {
                                                result = false;
                                                break;
                                        }
                                        const int margin = 256;
                                        int receive_size = SkkSocket::receive(socket_fd, http_receive_buffer_ + receive_buffer_index, http_receive_buffer_size_ - receive_buffer_index - margin);
                                        if (receive_size == 0)
                                        {
                                                break;
                                        }
                                        else if (receive_size == -1)
                                        {
                                                bool break_flag = true;
                                                switch (errno)
                                                {
                                                default:
                                                        break;
                                                case EAGAIN:
                                                case EINTR:
                                                case ECONNABORTED:
                                                case ECONNREFUSED:
                                                case ECONNRESET:
                                                        break_flag = false;
                                                        break;
                                                }
                                                if (break_flag)
                                                {
                                                        result = false;
                                                        break;
                                                }
                                        }
                                        else
                                        {
                                                receive_buffer_index += receive_size;
                                        }
                                }
                                http_receive_buffer_[receive_buffer_index] = '\0';
                        }
                        close(socket_fd);
                }
                return result;
        }

        // 念の為 skk バッファを確認します。不正ならば偽を返します。 Lisp っぽいコードがある場合は(問題はないとは思いますが)念の為に偽を返します。現在は必要以上にきつく見ていることに注意が必要です。
        static bool confirmSkk(const char *skk, const char *skk_tail)
        {
                for (;;)
                {
                        int c = static_cast<int>(static_cast<unsigned char>(*skk++));
                        if (c == 0)
                        {
                                break;
                        }
                        else if (c == '(')
                        {
                                const char concat[] = "concat ";
                                const char *p = skk;
                                const int nul_size = 1;
                                for (int i = 0; i != sizeof(concat) - nul_size; ++i)
                                {
                                        if (p >= skk_tail)
                                        {
                                                return false;
                                        }
                                        int concat_c = static_cast<int>(static_cast<unsigned char>(concat[i]));
                                        int p_c = static_cast<int>(static_cast<unsigned char>(*p++));
                                        if (concat_c != p_c)
                                        {
                                                return false;
                                        }
                                }
                        }
                }
                return true;
        }

        // json を更新しながら HTTP ヘッダスキップします。不正ならば偽を返します。
        static bool convertJsonToSkkSkipHttpHeader(const char *&json)
        {
                int cr_lf_counter = 0;
                int previous_c = 0;
                for (;;)
                {
                        int c = static_cast<int>(static_cast<unsigned char>(*json++));
                        if (c == 0)
                        {
                                return false;
                        }
                        if ((c == 0x0a) && (previous_c == 0x0d))
                        {
                                ++cr_lf_counter;
                                if (cr_lf_counter == 2)
                                {
                                        break;
                                }
                        }
                        if ((c != 0x0d) && (c != 0x0a))
                        {
                                cr_lf_counter = 0;
                        }
                        previous_c = c;
                }
                return true;
        }

        // buffer に code を書き込みます。 code の範囲はチェックされず p が buffer_tail より小さければ常に書き込まれます。失敗した場合は偽を返します。
        static bool convertJsonToSkkCopyOnlySkkWrite(char *&buffer, const char *buffer_tail, char code)
        {
                if (buffer < buffer_tail)
                {
                        *buffer++ = code;
                        return true;
                }
                DEBUG_ASSERT(0);
                return false;
        }

        static bool convertJsonToSkkCopyOnlySkkWrite(char *&buffer, const char *buffer_tail, char code_0, char code_1)
        {
                if (!convertJsonToSkkCopyOnlySkkWrite(buffer, buffer_tail, code_0))
                {
                        return false;
                }
                if (!convertJsonToSkkCopyOnlySkkWrite(buffer, buffer_tail, code_1))
                {
                        return false;
                }
                return true;
        }

        // buffer に文字列 p を書き込みます。終端文字は書き込まれません。失敗した場合は偽を返します。
        static bool convertJsonToSkkCopyOnlySkkWrite(char *&buffer, const char *buffer_tail, const char *p)
        {
                for (;;)
                {
                        char c = *p++;
                        if (c == 0)
                        {
                                break;
                        }
                        if (!convertJsonToSkkCopyOnlySkkWrite(buffer, buffer_tail, c))
                        {
                                return false;
                        }
                }
                return true;
        }

        static bool convertJsonToSkkCopyOnlySkkWriteOctet(char *&buffer, const char *buffer_tail, int code)
        {
                int c_3 = code / 64;
                code -= c_3 * 64;
                int c_2 = code / 8;
                code -= c_2 * 8;
                c_3 += static_cast<int>(static_cast<unsigned char>('0'));
                c_2 += static_cast<int>(static_cast<unsigned char>('0'));
                code += static_cast<int>(static_cast<unsigned char>('0'));
                if (!convertJsonToSkkCopyOnlySkkWrite(buffer, buffer_tail, '\\', static_cast<char>(c_3)))
                {
                        return false;
                }
                if (!convertJsonToSkkCopyOnlySkkWrite(buffer, buffer_tail, static_cast<char>(c_2), static_cast<char>(code)))
                {
                        return false;
                }
                return true;
        }

        // json から skk に必要な最初の候補リストを SKK の「変換文字列」形式で書き込みます。成功すれば真を返します。失敗した場合の skk の内容は不定です。
        static bool convertJsonToSkkCopyOnlySkk(const char *json, char *skk, const char *skk_tail, char *candidate_buffer_head, const char *candidate_buffer_tail, char *iconv_buffer, int iconv_buffer_size)
        {
                if (!convertJsonToSkkCopyOnlySkkWrite(skk, skk_tail, "1/"))
                {
                        return false;
                }
                bool skk_write_flag = false;
                int kakko_counter = 0;
                bool double_quote_flag = false;
                bool concat_flag = false;
                char *candidate_buffer = candidate_buffer_head;
                int candidate_counter = 0;
                DEBUG_PRINTF("json:\"%s\"", json);
                for (;;)
                {
                        int c = static_cast<int>(static_cast<unsigned char>(*json++));
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
                                        candidate_buffer = candidate_buffer_head;
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
                                        int force = static_cast<int>(static_cast<unsigned char>(*json++));
                                        if (!convertJsonToSkkCopyOnlySkkWrite(skk, skk_tail, static_cast<char>(c), static_cast<char>(force)))
                                        {
                                                return false;
                                        }
                                }
                        }
                        else if (c == '"')
                        {
                                double_quote_flag = false;
                                if (!convertJsonToSkkCopyOnlySkkWrite(candidate_buffer, candidate_buffer_tail, '\0'))
                                {
                                        return false;
                                }
                                if (!convertIconv(candidate_buffer_head, iconv_buffer, iconv_buffer_size, "utf-8", "euc-jp"))
                                {
                                        // convert fail skip
                                }
                                else
                                {
                                        if (concat_flag)
                                        {
                                                if (!convertJsonToSkkCopyOnlySkkWrite(skk, skk_tail, "(concat \""))
                                                {
                                                        return false;
                                                }
                                        }
                                        if (!convertJsonToSkkCopyOnlySkkWrite(skk, skk_tail, iconv_buffer))
                                        {
                                                return false;
                                        }
                                        if (concat_flag)
                                        {
                                                if (!convertJsonToSkkCopyOnlySkkWrite(skk, skk_tail, "\")"))
                                                {
                                                        return false;
                                                }
                                        }
                                        if (!convertJsonToSkkCopyOnlySkkWrite(skk, skk_tail, '/'))
                                        {
                                                return false;
                                        }
                                        ++candidate_counter;
                                }
                        }
                        else
                        {
                                DEBUG_ASSERT(double_quote_flag);
                                // \057 と \073 を含む candidate concat に。
                                if ((c == '/') || (c == ';'))
                                {
                                        concat_flag = true;
                                        if (!convertJsonToSkkCopyOnlySkkWriteOctet(candidate_buffer, candidate_buffer_tail, c))
                                        {
                                                return false;
                                        }
                                }
                                else
                                {
                                        if (!convertJsonToSkkCopyOnlySkkWrite(candidate_buffer, candidate_buffer_tail, static_cast<char>(c)))
                                        {
                                                return false;
                                        }
                                }
                        }
                }
                // 文字列型の convertJsonToSkkCopyOnlySkkWrite では終端文字を書き込めないため
                if (!convertJsonToSkkCopyOnlySkkWrite(skk, skk_tail, '\n', '\0'))
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
                const char *skipped_json = http_receive_buffer_;
                if (!convertJsonToSkkSkipHttpHeader(skipped_json))
                {
                        return false;
                }
                char *utf8_json_buffer = work_a_buffer_;
                if (!convertEscape4ToUtf8(skipped_json, utf8_json_buffer))
                {
                        return false;
                }
                // この時点で http_receive_buffer_ は破壊しても問題ないので、 candidate buffer として再利用。 http_send_buffer_ も同様に iconv buffer として再利用。
                const int tail_margin = 128;
                char *skk_buffer = skk_output_buffer_;
                const char *skk_buffer_tail = skk_output_buffer_ + skk_output_buffer_size_ - tail_margin;
                char *candidate_buffer = http_receive_buffer_;
                const char *candidate_buffer_tail = http_receive_buffer_ + http_receive_buffer_size_ - tail_margin;
                char *iconv_buffer = http_send_buffer_;
                int iconv_buffer_size = http_send_buffer_size_ - tail_margin;
                if (!convertJsonToSkkCopyOnlySkk(utf8_json_buffer, skk_buffer, skk_buffer_tail, candidate_buffer, candidate_buffer_tail, iconv_buffer, iconv_buffer_size))
                {
                        return false;
                }
                return true;
        }

private:
        SkkSocket skk_socket_;
        int work_a_buffer_size_;
        int work_b_buffer_size_;
        int http_send_buffer_size_;
        int http_receive_buffer_size_;
        int skk_output_buffer_size_;
        char *work_a_buffer_;
        char *work_b_buffer_;
        char *http_send_buffer_;
        char *http_receive_buffer_;
        char *skk_output_buffer_;
        SkkSimpleString http_send_string_;
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
                google_japanese_input_flag_(false)
        {
        }

        bool isGoogleJapaneseInput() const
        {
                return google_japanese_input_flag_;
        }

        void setGoogleJapaneseInputFlag(bool flag)
        {
                google_japanese_input_flag_ = flag;
        }

private:
        bool google_japanese_input_flag_;
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
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                skk_dictionary_length_(0),
                max_connection_(0),
                listen_queue_(0),
                server_completion_midasi_length_(0),
                server_completion_midasi_string_size_(0),
                server_completion_test_(1),
                server_completion_test_protocol_('4'),

                dictionary_check_update_flag_(false)
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
                        bool dictionary_check_update_flag)
        {
                skk_dictionary_ = skk_dictionary;
                dictionary_filename_table_ = dictionary_filename_table;

                skk_dictionary_length_ = skk_dictionary_length;
                max_connection_ = max_connection;
                listen_queue_ = listen_queue;
                server_completion_midasi_length_ = server_completion_midasi_length;
                server_completion_midasi_string_size_ = server_completion_midasi_string_size;
                server_completion_test_ = server_completion_test;

                dictionary_check_update_flag_ = dictionary_check_update_flag;
        }

#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        void setGoogleJapaneseInputParameter(GoogleJapaneseInputType type, float timeout)
        {
                google_japanese_input_type_ = type;
                google_japanese_input_timeout_ = timeout;
        }
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT

        bool mainLoop()
        {
                bool result = main_loop_initialize(max_connection_, listen_queue_);
                if (result)
                {
#ifndef YASKKSERV_DEBUG
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
        bool local_main_loop();

#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        bool local_main_loop_1_google_japanese_input(int work_index);
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        bool local_main_loop_1_search_single_dictionary(int work_index);
        void main_loop_get_plural_dictionary_information(int work_index,
                                                         LocalSkkDictionary *skk_dictionary,
                                                         int skk_dictionary_length,
                                                         int &found_times,
                                                         int &candidate_length,
                                                         int &total_henkanmojiretsu_size,
                                                         const char *&google_japanese_input_candidates);
        bool local_main_loop_1_search_plural_dictionary(int work_index, int candidate_length, int total_henkanmojiretsu_size, const char *google_japanese_input_candidates);
        bool local_main_loop_1_search(int work_index);
/// バッファをリセットすべきならば真を返します。
        bool local_main_loop_1(int work_index, int recv_result);
        bool local_main_loop_4_search_core(int work_index,
                                           int recv_result,
                                           SkkUtility::Hash<SkkUtility::HASH_TYPE_CANDIDATE> *hash,
                                           SkkSimpleString &string);
        bool local_main_loop_4_search(int work_index, int recv_result);
/// バッファをリセットすべきならば真を返します。
        bool local_main_loop_4(int work_index, int recv_result);

private:
        LocalSkkDictionary *skk_dictionary_;
        const char * const *dictionary_filename_table_;

#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        GoogleJapaneseInput google_japanese_input_;
        GoogleJapaneseInputType google_japanese_input_type_;
        float google_japanese_input_timeout_;
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        int skk_dictionary_length_;
        int max_connection_;
        int listen_queue_;
        int server_completion_midasi_length_;
        int server_completion_midasi_string_size_;
        int server_completion_test_;
        char server_completion_test_protocol_;

        bool dictionary_check_update_flag_;
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
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT

bool LocalSkkServer::local_main_loop_1_search_single_dictionary(int work_index)
{
        bool result = false;
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        if (skk_dictionary_->isGoogleJapaneseInput())
        {
                const char *candidates = google_japanese_input_.getSkkCandidatesEuc((work_ + work_index)->read_buffer + 1, google_japanese_input_timeout_);
                if (candidates)
                {
                        if (!send((work_ + work_index)->file_descriptor, candidates, GoogleJapaneseInput::getByteSize(candidates)))
                        {
                                (work_ + work_index)->closeAndReset();
                        }
                        result = true;
                }
        }
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
                                                                 const char *&google_japanese_input_candidates)
{
        found_times = 0;
        candidate_length = 0;
        total_henkanmojiretsu_size = 0;
        google_japanese_input_candidates = 0;
        for (int h = 0; h != skk_dictionary_length; ++h)
        {
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                if ((skk_dictionary + h)->isGoogleJapaneseInput())
                {
                        google_japanese_input_candidates = google_japanese_input_.getSkkCandidatesEuc((work_ + work_index)->read_buffer + 1, google_japanese_input_timeout_);
                        if (google_japanese_input_candidates)
                        {
                                const int protocol_1_offset = 1;
                                DEBUG_PRINTF("henkanmojiretsu_size=%d\n", SkkSimpleString::getSize(google_japanese_input_candidates + protocol_1_offset));
                                DEBUG_PRINTF("candidate_length=%d\n", SkkUtility::getCandidateLength(google_japanese_input_candidates + protocol_1_offset));
                                total_henkanmojiretsu_size += SkkSimpleString::getSize(google_japanese_input_candidates + protocol_1_offset);
                                candidate_length += SkkUtility::getCandidateLength(google_japanese_input_candidates + protocol_1_offset);
                                ++found_times;
                        }
                }
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
bool LocalSkkServer::local_main_loop_1_search_plural_dictionary(int work_index, int candidate_length, int total_henkanmojiretsu_size, const char *google_japanese_input_candidates)
{
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
        string.append("1/");

        for (int h = 0; h != skk_dictionary_length_; ++h)
        {
                const char *p = 0;
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                if ((skk_dictionary_ + h)->isGoogleJapaneseInput() && google_japanese_input_candidates)
                {
                        const int protocol_1_offset = 1;
                        p = google_japanese_input_candidates + protocol_1_offset;
                }
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
                        int length = SkkUtility::getCandidateLength(p);
                        for (int g = 0; g != length; ++g)
                        {
                                const char *start;
                                int size;
                                if (SkkUtility::getCandidateInformation(p, g, start, size))
                                {
                                        if (!hash.contain(start, size))
                                        {
                                                hash.add(start, size);
                                                const int tail_slash_size = 1;
                                                string.append(start, size + tail_slash_size);
                                        }
                                }
                                else
                                {
                                        return false;
                                }
                        }
                }
        }

        string.append('\n');

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
        main_loop_get_plural_dictionary_information(work_index,
                                                    skk_dictionary_,
                                                    skk_dictionary_length_,
                                                    found_times,
                                                    candidate_length,
                                                    total_henkanmojiretsu_size,
                                                    google_japanese_input_candidates);
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
        return local_main_loop_1_search_plural_dictionary(work_index, candidate_length, total_henkanmojiretsu_size, google_japanese_input_candidates);
}

bool LocalSkkServer::local_main_loop_1(int work_index, int recv_result)
{
        bool illegal_protocol_flag;
        bool result = main_loop_check_buffer(work_index, recv_result, illegal_protocol_flag);
        if (result)
        {
                bool found_flag = false;
                if (!illegal_protocol_flag)
                {
                        found_flag = local_main_loop_1_search(work_index);
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                        if (!found_flag && (google_japanese_input_type_ == GOOGLE_JAPANESE_INPUT_TYPE_NOTFOUND))
                        {
                                found_flag = local_main_loop_1_google_japanese_input(work_index);
                        }
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
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
                string.append("1/");
                break;

        case 3:
                string.append("1 ");
                break;

        case 4:
                if (server_completion_test_protocol_ == '4')
                {
                        string.append("1/");
                }
                else
                {
                        string.append("1 ");
                }
                break;
        }

        int add_counter = 0;
        for (int h = 0; h != skk_dictionary_length_; ++h)
        {
                if ((skk_dictionary_ + h)->isSuccess())
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
                                                                string.append('/'); // + separator_size
                                                                break;
                                                        case 3:
                                                                string.append(' '); // + separator_size
                                                                break;
                                                        case 4:
                                                                if (server_completion_test_protocol_ == '4')
                                                                {
                                                                        string.append('/'); // + separator_size
                                                                }
                                                                else
                                                                {
                                                                        string.append(' '); // + separator_size
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

        string.append("\n");

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
                if ((skk_dictionary_ + h)->search((work_ + work_index)->read_buffer + 1) ||
                    (skk_dictionary_ + h)->searchForFirstCharacter((work_ + work_index)->read_buffer + 1))
                {
                        ++found_times;
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

        if (!send((work_ + work_index)->file_descriptor, string.getBuffer(), string.getSize(server_completion_midasi_string_size_)))
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

bool LocalSkkServer::local_main_loop()
{
        bool result = true;
        for (;;)
        {
                fd_set fd_set_read;
                int select_result = main_loop_select(fd_set_read);
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
                                                goto ERROR_BREAK;
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
        }
ERROR_BREAK:
        result = false;

//SUCCESS_BREAK:
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
                           "      --server-completion-midasi-length=LENGTH\n"
                           "                           set midasi length (range [256 - 32768]  default 2048)\n"
                           "      --server-completion-midasi-string-size=SIZE\n"
                           "                           set midasi string size (range [16384 - 1048576]  default 262144)\n"
                           "      --server-completion-test=type\n"
                           "                           1:default  2:ignore slash  3:space  4:space and protocol 'c'\n"
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                           "      --google-japanese-input\n"
                           "                           enable google japanese input\n"
                           "      --google-japanese-input-timeout=SECOND\n"
                           "                           set enable google japanese input timeout (default 2.5)\n"
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                           "  -v, --version            print version\n");
        return EXIT_FAILURE;
}

int print_version()
{
        SkkUtility::printf("yaskkserv_" SERVER_IDENTIFIER " version " YASKKSERV_VERSION "\n");
        SkkUtility::printf("Copyright (C) 2005, 2006, 2007, 2008, 2011, 2012 Tadashi Watanabe\n");
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
        OPTION_TABLE_SERVER_COMPLETION_MIDASI_LENGTH,
        OPTION_TABLE_SERVER_COMPLETION_MIDASI_STRING_SIZE,
        OPTION_TABLE_SERVER_COMPLETION_TEST,
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        OPTION_TABLE_GOOGLE_JAPANESE_INPUT,
        OPTION_TABLE_GOOGLE_JAPANESE_INPUT_TIMEOUT,
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
                0, "google-japanese-input",
                SkkCommandLine::OPTION_ARGUMENT_STRING,
        },
        {
                0, "google-japanese-input-timeout",
                SkkCommandLine::OPTION_ARGUMENT_FLOAT,
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
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        LocalSkkServer::GoogleJapaneseInputType google_japanese_input_type;
        float google_japanese_input_timeout;
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
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
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
        LocalSkkServer::GOOGLE_JAPANESE_INPUT_TYPE_DISABLE,
        2.5f,
#endif  // YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
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
                                SkkUtility::printf("Illegal log-level %d (0 - 9)\n", option.log_level);
                                result = print_usage();
                                return true;
                        }
                }
                if (command_line.isOptionDefined(OPTION_TABLE_MAX_CONNECTION))
                {
                        option.max_connection = command_line.getOptionArgumentInteger(OPTION_TABLE_MAX_CONNECTION);
                        if ((option.max_connection < 1) || (option.max_connection > 1024))
                        {
                                SkkUtility::printf("Illegal max-connection %d (1 - 1024)\n", option.max_connection);
                                result = print_usage();
                                return true;
                        }
                }
                if (command_line.isOptionDefined(OPTION_TABLE_PORT))
                {
                        option.port = command_line.getOptionArgumentInteger(OPTION_TABLE_PORT);
                        if ((option.port < 1) || (option.port > 65535))
                        {
                                SkkUtility::printf("Illegal port number %d (1 - 65535)\n", option.port);
                                result = print_usage();
                                return true;
                        }
                }
                if (command_line.isOptionDefined(OPTION_TABLE_SERVER_COMPLETION_MIDASI_LENGTH))
                {
                        option.server_completion_midasi_length = command_line.getOptionArgumentInteger(OPTION_TABLE_SERVER_COMPLETION_MIDASI_LENGTH);
                        if ((option.server_completion_midasi_length < 256) || (option.server_completion_midasi_length > 32768))
                        {
                                SkkUtility::printf("Illegal midasi length %d (256 - 32768)\n", option.server_completion_midasi_length);
                                result = print_usage();
                                return true;
                        }
                }
                if (command_line.isOptionDefined(OPTION_TABLE_SERVER_COMPLETION_MIDASI_STRING_SIZE))
                {
                        option.server_completion_midasi_string_size = command_line.getOptionArgumentInteger(OPTION_TABLE_SERVER_COMPLETION_MIDASI_STRING_SIZE);
                        if ((option.server_completion_midasi_string_size < 16 * 1024) || (option.server_completion_midasi_string_size > 1024 * 1024))
                        {
                                SkkUtility::printf("Illegal string size %d (16384 - 1048576)\n", option.server_completion_midasi_string_size);
                                result = print_usage();
                                return true;
                        }
                }
                if (command_line.isOptionDefined(OPTION_TABLE_SERVER_COMPLETION_TEST))
                {
                        option.server_completion_test = command_line.getOptionArgumentInteger(OPTION_TABLE_SERVER_COMPLETION_TEST);
                        if ((option.server_completion_test < 1) || (option.server_completion_test > 4))
                        {
                                SkkUtility::printf("Illegal argument %d (1 - 4)\n", option.server_completion_test);
                                result = print_usage();
                                return true;
                        }
                }
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
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
                        else if (SkkSimpleString::compare("dictionary", p) == 0)
                        {
                                option.google_japanese_input_type = LocalSkkServer::GOOGLE_JAPANESE_INPUT_TYPE_DICTIONARY;
                        }
                        else
                        {
                                SkkUtility::printf("Illegal argument %s (disable, notfound or dictionary)\n", p);
                                result = print_usage();
                                return true;
                        }
                }
                if (command_line.isOptionDefined(OPTION_TABLE_GOOGLE_JAPANESE_INPUT_TIMEOUT))
                {
                        option.google_japanese_input_timeout = command_line.getOptionArgumentFloat(OPTION_TABLE_GOOGLE_JAPANESE_INPUT_TIMEOUT);
                        if ((option.google_japanese_input_timeout <= 0.0f) || (option.google_japanese_input_timeout > 60.0f))
                        {
                                SkkUtility::printf("Illegal second %f (0.0 - 60.0)\n", option.google_japanese_input_timeout);
                                result = print_usage();
                                return true;
                        }
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
                                SkkUtility::printf("google dictionary must less than 2\n",
                                                   command_line.getArgumentPointer(i),
                                                   i);
                                result = EXIT_FAILURE;
                                break;
                        }
                        (skk_dictionary + i)->setGoogleJapaneseInputFlag(true);
                }
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
                                       option.check_update_flag);
#ifdef YASKKSERV_CONFIG_ENABLE_GOOGLE_JAPANESE_INPUT
                skk_server->setGoogleJapaneseInputParameter(option.google_japanese_input_type, option.google_japanese_input_timeout);
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
}

int local_main(int argc, char *argv[])
{
        return local_main_core(argc, argv);
}
}
