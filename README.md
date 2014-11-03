# install

yaskkserv をビルドすると、以下のプログラム群が作成されます。

<dl>
  <dt>yaskkserv_simple
  <dd>もっともシンプルなサーバ
  <dt>yaskkserv_normal
  <dd>一般的なサーバ
  <dt>yaskkserv_hairy
  <dd>なんでもありサーバになる予定
  <dt>yaskkserv_make_dictionary
  <dd>yaskkserv 用辞書変換ユーティリティ
</dl>




## configure

大抵の環境で、以下のように configure 一発でビルドできます。

```sh
$ ./configure
$ make
```

configure は Perl スクリプトです。 Perl がない場合は後述の Makefile.noperl を使用します。

インストールすると PREFIX/bin にツールが、 PREFIX/sbin にサーバがインストールされます。

インストールするバイナリごとに仮想ターゲットを用意しています。


### yaskkserv_normal を yaskkserv という名前でインストール

```sh
# make install
```

or

```sh
# make install_normal
```


### yaskkserv_simple を yaskkserv という名前でインストール

```sh
# make install_simple
```


### yaskkserv_hairy を yaskkserv という名前でインストール

```sh
# make install_hairy
```


### yaskkserv_simple, yaskkserv_normal と yaskkserv_hairy をそのままの名前でインストール

```sh
# make install_all
```

### Perl が無い場合

Makefile.noperl を手で編集して、以下のようなコマンドでビルドできます。

```sh
$ make -f Makefile.noperl
```

インストールは Perl がある場合と同様、以下のように実行します。

```
# make -f Makefile.noperl install_all
```




## configure オプション

<dl>
  <dt>--gplusplus=G++
  <dd>g++ の実行ファイルを指定します。特定バージョンの g++ を使う場合に便利です。
  <dt>--help
  <dd>使用法を表示します。
  <dt>--enable-google-japanese-input
  <dd>yaskkserv_hairy の google japanese input を有効にします。(デフォルト)
  <dt>--disable-google-japanese-input
  <dd>yaskkserv_hairy の google japanese input を無効にします。
  <dt>--enable-google-suggest
  <dd>yaskkserv_hairy の google suggest を有効にします。
  <dt>--disable-google-suggest
  <dd>yaskkserv_hairy の google suggest を無効にします。(デフォルト)
  <dt>--enable-syslog
  <dd>syslog 出力を有効にします。 (デフォルト)
  <dt>--disable-error-message
  <dd>syslog 出力を無効にします。
  <dt>--enable-error-message
  <dd>エラーメッセージ出力を有効にします。 (デフォルト)
  <dt>--disable-error-message
  <dd>エラーメッセージ出力を無効にします。ヘルプメッセージ等も表示されなくなるので注意が必要です。
  <dt>--precompile
  <dd>プリコンパイルヘッダを使用します。
  <dt>--prefix=PREFIX
  <dd>インストールディレクトリを指定します。
</dl>




# つかいかた

まず yaskkserv_make_dictionary で専用の辞書を作成する必要があります。

```sh
$ yaskkserv_make_dictionary SKK-JISYO.L SKK-JISYO.L.yaskkserv
```


以上の操作で SKK-JISYO.L から SKK-JISYO.L.yaskkserv が作られます。

作成した辞書を指定してサーバを起動します。

```sh
$ yaskkserv SKK-JISYO.L.yaskkserv
```




## google japanese input を辞書として使う

google japanese input を有効にして --google-japanese-input=dictionary オプションを指定した場合、 yaskkserv_hairy では、辞書に「https://www.google.com」を指定できます。 https ではなく「http://www.google.com」を指定した場合は http でアクセスします。

```sh
$ yaskkserv --google-japanese-input=dictionary https://www.google.com
```

同様に google suggest を有効にした場合、辞書に「https://suggest.google.com」を指定できます。

```sh
$ yaskkserv --google-japanese-input=dictionary https://suggest.google.com
```

辞書は複数指定できます。

```sh
$ yaskkserv --google-japanese-input=dictionary LOCALDIC https://suggest.google.com https://www.google.com
```

google への問い合わせは --google-cache オプションでキャッシュすることも可能ですが、応答時間の違いから過去に変換した文字列を推測される恐れがあります。




## ローカル辞書に見付からなかったときのみ google にアクセスする

--google-japanese-input=notfound オプションを指定すると、辞書に候補が見付からなかったときにだけ google japanese input を検索します。以下のような組み合わせが可能です。

### 辞書に候補が見付からなければ google japanese input に https でアクセス

```sh
$ yaskkserv --google-japanese-input=notfound LOCALDIC
```

### 辞書に候補が見付からなければ google suggest に https でアクセス

```sh
$ yaskkserv --google-japanese-input=notfound --google-suggest LOCALDIC
```

### 辞書に候補が見付からなければ google suggest の次に google japanese input に https でアクセス

```sh
$ yaskkserv --google-japanese-input=notfound-suggest-input --google-suggest LOCALDIC
```

### 辞書に候補が見付からなければ google japanese input の次に google suggest に https でアクセス

```sh
$ yaskkserv --google-japanese-input=notfound-input-suggest --google-suggest LOCALDIC
```

--use-http オプションを付けることで https ではなく http でアクセスできます。
