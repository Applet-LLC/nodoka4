汎用キーバインディング変更ソフト「のどか」について

https://appletllc.com/ appletllc@gmail.com 2026-08-26


1. 概要
　「窓使いの憂鬱」というソフトの派生バージョンです。サポートや開発主体が異なりますので、ご注意ください。

　dot.nodokaという設定ファイルを用いて、ある程度自由に、キーボードの配列を変えることが可能です。

　サポートOSは、Windows 10 22H2以降です。
、デバイスドライバはx86(32bit)版はWindows 10 22H2, x64(64bit)版はWindows 11 24H2にてWHQL署名取得したものとなります。
　実行ファイル、DLLならびにデバイスドライバに署名が付いています。
　
　試用版は起動後30分で自動終了します。

　シェアウェアです。
　ソースコードは、EPL 2.0(Eclipse Public License 2.0) で https://github.com/Applet-LLC/nodoka4 にて公開しています。

　正式版(税込み$11(=1800円))は、https://appletllc.com/%e3%82%bd%e3%83%95%e3%83%88%e3%82%a6%e3%82%a7%e3%82%a2/ から購入可能です。
　決済にはGumroadを用いており、クレジットカードが必要となります。


2. インストール/起動/アンインストール/カスタマイズ方法

　制限事項、不具合等もあるので、詳細については下記をご覧ください。

　ご案内ページ	https://appletllc.com/web/nodoka.htm
  Q&A		https://appletllc.com/web/nodoka-QandA.htm
  ヘルプ	https://appletllc.com/web/nodoka-doc/README-ja.html

 (1)インストール方法
　　まず、動作中ののどかを終了させて、タスクマネージャーのスタートアップアプリで起動を有効にしている場合には、無効にしてください。

　　のどかのインストールを実施するには、まずVisual C++ v14 再頒布可能パッケージが必要です。
　　以下のページから、お使いのOSのアーキテクチャー64bit/32bitの別に応じて、x64, x86用のファイルをダウンロードして、実行してインストールを済ませてください。

　　サポートされている最新の Visual C++ 再頒布可能パッケージのダウンロード | Microsoft Learn
　　https://learn.microsoft.com/ja-jp/cpp/windows/latest-supported-vc-redist?view=msvc-170
　　https://aka.ms/vc14/vc_redist.x64.exe
　　https://aka.ms/vc14/vc_redist.x86.exe

　　ここで、Windowsを再起動してください。のどかが起動していない状態で、setupを実行することで、インストールが失敗することを防ぎます。
　　
　　setupファイルであるnodoka-4.33_sample_setup.exe (試用版), nodoka-4.33_setup.exe (正式版)のいずれかを管理者権限で実行してください。

　　デバイスドライバは、setup実行時にインストールされます。必要に応じてインストール確認のダイアログが表示されます。
　　またファイルのインストールが終わると、再起動を要求されるので、再起動してください。

　　再起動後、先にタスクマネージャーでのスタートアップ設定を無効にした場合には、有効にしてください。


 (2)実行方法
　スタートメニューに登録された 「のどか」をダブルクリックする。あるいはスタートアップに登録した場合には、Windowsログイン時に自動実行されます。
　なお試用版では、起動後30分で自動終了します。


 (3)アンインストール方法
　コントロールパネルのプログラムの追加と削除から「のどか」を見つけて、アンインストールしてください。
　アンインストールしても、実行中のファイルを削除できずに、c:\Program Files\nodokaフォルダにファイルが残ったり
　またショートカットが、スタートメニューやスタートアップに残ることがあります。
　その場合には、エクスプローラーから手作業で削除してください。

  デバイスドライバも同時に削除します。


 (4)使い方やカスタマイズ方法
　使い方やカスタマイズ方法については、上記ヘルプをご覧ください。

  また、英語ページでは、設定方法のサンプルがいくつかあります。
     https://www.appletllc.com/web/en/sample.htm


3. 制限事項やユーザサポートなど

　2.で記載したヘルプファイルをご覧ください。

　なお安定動作確保に努めますが、不具合が発生した場合、試用版、正式版に関わらず、その責任の所在は使用者にあります。あしからずご了承ください。

　ユーザサポート掲示板は、匿名であれば、https://jbbs.shitaraba.net/computer/41517/ です。
　こちらでもサポート可能ですが、直接、不具合報告されたい場合には、以下をご利用ください。
　リリース直後の不具合確認や報告は、こちらの方が早いです。
　https://github.com/Applet-LLC/nodoka4/issues


4. 著作権表示

繭 Version 4.00.0, のどか Version 4.01〜4.30
Copyright (C) 2008～2026 applet <applet@bp.iij4u.or.jp> All rights reserved.

ライセンスは、CPL(Common Public License)です。詳細は Common_Public_License_1_0.txt をご覧ください。Common_Public_License_1_0_JP.txt は、日本語参考訳です。
なお、これらのファイルは過去のバージョン（4.01〜4.30）に同梱していたものです。


のどか Version 4.31以降
Copyright (C) 2008～2026 applet <applet@bp.iij4u.or.jp> All rights reserved.

ライセンスは、EPL 2.0(Eclipse Public License 2.0)に変更されました。詳細は LICENSE.txt をご覧ください。LICENSE_JP.txt は、日本語参考訳です。


窓使いの憂鬱 Version 3.30以前

Copyright (C) 1999-2005, TAGA Nayuta <nayuta@users.sourceforge.net>
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. 
Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution. 
The name of the author may not be used to endorse or promote products derived from this software without specific prior written permission. 
THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


Yet Another Mado tsukai no Yuutsu(YAMY)

  Copyright (C) 2009, KOBAYASHI Yoshiaki <gimy@users.sourceforge.jp>
    All rights reserved.

  Redistribution and use in source and binary forms,
  with or without modification, are permitted provided
  that the following conditions are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above
       copyright notice, this list of conditions and the following
       disclaimer in the documentation and/or other materials provided
       with the distribution.
    3. The name of the author may not be used to endorse or promote
       products derived from this software without specific prior
       written permission. 

  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
  THE POSSIBILITY OF SUCH DAMAGE.


Boost.Regex 
Copyright (c) 1998-2007 John Maddock

Boost.Program_options
Copyright (c) 2002-2004 Vladimir Prus

Boost C++ Libraries 1.85.0

This product includes software from the Boost C++ Libraries,
distributed under the Boost Software License, Version 1.0.
https://www.boost.org/

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or
organization obtaining a copy of the software and accompanying
documentation covered by this license (the "Software") to use,
reproduce, display, distribute, execute, and transmit the Software,
and to prepare derivative works of the Software, and to permit
third-parties to whom the Software is furnished to do so, all subject
to the following:

The copyright notices in the Software and this entire statement,
including the above license grant, this restriction and the following
disclaimer, must be included in all copies of the Software, in whole
or in part, and all derivative works of the Software, unless such
copies or derivative works are solely in the form of machine-executable
object code generated by a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND
NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR ANYONE
DISTRIBUTING THE SOFTWARE BE LIABLE FOR ANY DAMAGES OR OTHER LIABILITY,
WHETHER IN CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

天狼 sirius  http://www.sirius.spline.tv/wiki/WikiStart
The MIT License
Copyright (C) 2008-2010, MATSUMOTO Reiji <matsumoto@spline.oc.to>
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.


5. 謝辞

偉大かつ、手放すことができない「窓使いの憂鬱」を作成された TAGA Nayuta氏。大変どうもありがとうございます。
また Vista向けの 3.30.1ソースを作成され、2chでのスレッドで公開してくださった無名の方、大変どうもありがとうございます。
また、YAMYを開発されたKOBAYASHI Yoshiaki氏。大変どうもありがとうございます。


6. 更新内容　（詳細や履歴についてはヘルプの改版履歴をご覧ください。）

2026-08-26 4.33

　概要
　Combo/TapDanceで、モディファイヤキー(Shift/Ctrl等)が押されたまま戻らなくなることがある不具合を修正しました。
　また、TapDanceの御認識修正と、Combo/TapDance/TapHoldまわりのドキュメント記載を整備しました。
　あわせて、.nodoka拡張子をエクスプローラーの「新規作成」メニューに追加しました。
(1) Combo/TapDanceのモディファイヤキー押しっぱなし不具合の修正
　Combo/TapDance/TapHoldは、それぞれタイマーによる確定処理(タイムアウトでの確定)を持っていますが、この確定処理が、
　メインループ末尾で行われるモディファイヤキー(Shift/Ctrl/Alt/Win)の解放処理を素通りしてしまうことがあり、Comboの発火時や
　TapDanceのタップ確定時やに、本来離されるべきモディファイヤキーが押されたままになる不具合を修正しました。

　あわせて、TapDanceのタイムアウト成立とほぼ同時に次のキー入力が発生した場合、本来独立した2回のタップとして処理されるべき
　ところが、タイミング次第で誤って1回のダブルタップとして結合されてしまう競合状態も修正しました。

　なお、Combo/TapHoldについても、判定の境界(閾値やタイムアウトとほぼ同時にキー操作が重なるケース)で、Combo成立/不成立や
　tap/holdの判定が稀に振れることがある点を確認していますが、モディファイヤキーの押しっぱなしなど実害を伴う挙動ではないため、
　今回は対応を見送っています。

(2) .nodoka拡張子をエクスプローラーの「新規作成」メニューに追加
　エクスプローラーで、フォルダ内を右クリック→新規作成メニューから、「のどか ファイル」(.nodoka)を直接作成できるようにしました。
　アンインストール時には、このメニュー項目も削除されます。

(3) Combo/TapDance/TapHoldのドキュメント記載整備
　カスタマイズ方法のヘルプ(CUSTOMIZE-ja.html)について、KEYを指定する箇所にはFUNCTIONも指定できることの明記、
　TapDanceのKEYは1つのみ指定可能である旨の明記、TapHoldのルール単位オプション(permissive_hold=、hold_on_other_key=、
　quick_tap_term=)の記述例の追加、未定義の関数を使っていたサンプルを実際に動作する記述例へ差し替えるなど、記載の整備を行いました。
　あわせて、いわゆるLayerの概念は、ロックキー(L0～LF)やmod0～9(M0～M9)を用いて実現できる旨を明記しました。

(4) 起動時引数 -t / -d の追加(自動テスト向け)
　外部からの自動テストやスクリプトでの利用を想定し、起動時引数に -t <名前> と -d <ファイルパス> を追加しました。
　・-t <名前>: タスクトレイアイコンのメニューの「設定切り替え」等で使われているのと同じ名前を指定して、該当する設定(.nodokaファイル)へ
　切り替えて再ロードします。すでに「のどか」が起動していれば切り替えて再ロード、まだ起動していなければその設定を選択したうえで起動します。
　名前が見つからない場合には何も変更されません。
　・-d <ファイルパス>: ログウィンドウの内容を、指定したファイルにも追記出力します(UTF-8、BOM無し)。指定すると詳細ログも自動的に有効になります。
　詳しい説明は、カスタマイズ方法のヘルプの「オプション　起動時引数」の項をご覧ください。

(5) その他の修正
　1) Combo/TapDance/TapHoldの状態遷移処理で、詳細ログにキー入力内容が出力されないことがあった記録漏れを修正しました。
　2) バージョン情報ダイアログの著作権表示の連絡先メールアドレスを、appletllc@gmail.com に更新しました。


以上
