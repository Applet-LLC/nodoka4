汎用キーバインディング変更ソフト「のどか」について

https://appletllc.com/ applet@bp.iij4u.or.jp 2026-06-27


1. 概要
　「窓使いの憂鬱」というソフトの派生バージョンです。サポートや開発主体が異なりますので、ご注意ください。

　dot.nodokaという設定ファイルを用いて、ある程度自由に、キーボードの配列を変えることが可能です。

　サポートOSは、Windows 10 22H2以降です。
、デバイスドライバはx86(32bit)版はWindows 10 22H2, x64(64bit)版はWindows 11 24H2にてWHQL署名取得したものとなります。
　実行ファイル、DLLならびにデバイスドライバに署名が付いています。
　
　試用版は起動後30分で自動終了します。

　シェアウェアです。
　ソースコードは、EPL 2.0(Eclipse Public License 2.0) で https://gitlab.com/appletllc/nodoka4 にて公開しています。

　正式版(税込み$11)は、https://appletllc.com/%e3%82%bd%e3%83%95%e3%83%88%e3%82%a6%e3%82%a7%e3%82%a2/ から購入可能です。
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
　　
　　setupファイルであるnodoka-4.31_sample_setup.exe (試用版), nodoka-4.31_setup.exe (正式版)のいずれかを管理者権限で実行してください。

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

　ユーザサポート掲示板は、https://jbbs.shitaraba.net/computer/41517/ です。
　こちらで不具合報告や、調査依頼、ユーザ間の情報交換が可能です。


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

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.


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

2026-06-27 4.31

　ライセンスをCPL(Common Public License)から、EPL 2.0(Eclipse Public License 2.0)に変更しました。

(1) サポートOSの変更と次版の話
　従来 Windows 2000, XP, Vista, 7, 8, 10をサポートとしてきました。しかしながら本バージョンでは、Windows 10 22H2以降でのサポートとします。

　Windows 10 バージョン1903（May 2019 Update)でも動作可能ですが、しかし、これより古いバージョンでは、インストールするとキー入力できないなどの異常状態になります。
　Windows Serverは未サポートです。カーネルのバージョンからみて、Windows Server 2022以降であればインストールは可能です。Windows Server 2019以前だと不具合に遭遇します。

　Windows 10 32bit(x86)向けは、この4.31で最終となり、次版からWindows 11対応だけとなる予定です。
　また次版は、メージャーバージョンアップを予定しており以下の機能提供を想定しています。
　　Combo/TapDance/TapHold機能の実装, 複数キーボード対応の完全版、設定のGUI化トライアル

　予定は未定なので、実装を確約するものではありませんが、Combo/TapDance/TapHoldについては実験実装済で、本バージョンでも試せます。本Readmeの末尾をご覧ください。

(2) バグフィックスと仕様変更
1) デバイスドライバのWindows 11対応とWindows Update時のキー入力不可事象の解消
2) モディファイヤーキー押しっぱなし事象の改善策の追加
3) LL Hook実装の修正
4) 管理者権限アプリとの連動不具合修正
5) ウィンドウ移動・拡大関数での座標精度を向上
6) 設定ファイル読み込みバグ修正
7) 開発環境変更
8) リモートデスクトップ対応
9) 次版に向けての実験実装について

1) デバイスドライバのWindows 11対応とWindows Update時のキー入力不可事象の解消
　Windowsのアップデート/アップグレードの際、デバイスドライバの登録状態が正しく処理されるようWindows 10 バージョン1903以降向けのプリミティブドライバとしてコードを刷新
　バージョン番号は1.38(以前は1.33)としました。1.34, 1.35, 1.36, 1.37は欠番です。

　infファイルによるインストールから、以前のようにsetupからインストールするように変更
　x86(32bit)版はWindows 10 22H2, x64(64bit)版はWindows 11 24H2にてWHQL署名取得しています。

　プリミティブドライバとしたので、Windows Update対応できたこととなります。デバイスドライバのインストール先は、C:\Windows\System32\drivers から
　C:\Windows\System32\DriverStore\FileRepository以下の各デバイスドライバごとのフォルダに変更になっています。

　4.30(nodokadは1.31)が入った環境で、のどか4.31のインストールを行うと、古いドライバの設定は削除され、新しいドライバの設定がレジストリに記述されます。
  Windows 10 20H2から21H2、22H2へといったWindows Updateをのどかがインストールされた状態で実施して、のどか4.31ではキー入力不可になる事例には現在のところ遭遇していません。
　少なくとも可能性を下げることはできたと思います。ただ、nodokad.sysが入っていない状態にされる場合があり、その場合には、インストールのやり直しが必要となる場合があります。

　それでもWindows Updateを実施した後で、ログイン画面にてキー入力ができない状態に遭遇した場合、以下を試すことが可能です。
　ログイン画面でスクリーンキーボードが使える場合には、それを使ってください。
　スクリーンキーボードが使えない場合、ログイン画面になる直前の起動画面中に電源オフを何度か実行して、回復モードに入り
　前回正常起動時の構成で起動する方法で復旧できます。

2) モディファイヤーキー押しっぱなし事象の改善策の追加
　以下の2種を追加しました。デフォルト無効です。既存のdef option CheckModifierとも、いずれも共存します。
  ShiftキーやCtrlキーの押しっぱなしが頻発するようであれば、試せるという位置づけとなります。

　def option SyncModifierGracePeriod = <ms>   # 例: 500 （0 は機能無効の意味）

　キーリマップ実施の際、キー出力時に、システムへの反映が数10m 遅延することがあります（LL Hookへの遅延、およびデバイスドライバへの非同期WriteFile）。この遅延期間中に次のキーが押されると`GetAsyncKeyState` が0を返すため、正常な修飾キーが誤ってクリアされます。
　この誤検知を防止するために、グレースピリオドはモディファイヤーキーが「押された」と記録されてから指定したms以内のクリアを無視し
誤検知を防止します。**500 ms 程度**を推奨します。実際、これが起きていないようであれば設定不要です。
　

　def option ModifierAutoClear = <秒>          # 例: 5 （0 は機能無効の意味）

　モデファイヤーキー(Shift, Ctrl, Alt, Win)を押した後キー入力が無い時間を測り、指定した時間が過ぎると
　モディファイヤーキーのUpを送り、自動クリアします。
　CheckModiferよりも指定は簡単ですが、細かな制御はできません。

　余談ですが、Hyper-V環境では、各モディファイヤーキーのUpを定期的に送るというワークアラウンドが採用されているようです。ログウィンドウで時々観測できます。


3) LL Hook実装の修正
　ログダイアログ表示中に、フォーカスがログダイアログにある場合、ログが表示されない不具合を修正しました。
　engine.cppにおいて、LL Hookコールバックの処理をWindows Messageではなく、Eventを使うように修正(これはyamyと同じ実装です)。
　その結果ポーリングによるキー入力遅延を5msから無しとし、反応良くなりました。


4) 管理者権限アプリとの連動
　マニフェストに uiAccess=trueと記述していても、管理者権限アプリのウィンドウ情報が取れないなど、うまく動いていなかったため修正しました。
　ただしDebugViewでログ出力を見ると、Microsoft Edge WebView2 を使ったアプリでは、DLLのロードが拒否されるため、ウィンド情報が取れません。
　また、uiAccess=tureがついたnodoka64.exeなどの実行ファイルは、Program Files以下以外で動作させると、エラーダイアログを表示して、正常に動作できないため
　nodoka64_nua.exe を用意しました。そちらはuiAccess=false指定のマニフェストとしました。

　キーリマップができない、ウィンドウの情報が取れないアプリの場合、nodoka64_hil.exeをそのまま起動して、より高い権限で動作させる。
　あるいはnodoka64_nua.exeを管理者権限で動作させると対応できる可能性があります。


5) ウィンドウ移動・拡大関数での座標精度を向上
　&WindowMaximize, &WindowHMaximize, &WindowVMaximize, &WindowMoveTo, &WindowMonitorTo, &WindowResizeTo, &WindowResizeToPer, &WindowResizeMoveTo, &WindowResizeMoveToPer において
　125%以上のDPI環境でウィンドウ座標計算が物理ピクセルで行われず、移動・拡大先がずれていた問題を修正
　またWindows 10以降のDWMによる不可視ボーダー（シャドウ領域）を除いた正確なウィンドウ矩形を取得し使用するように修正


6) 設定ファイル読み込みバグ対応
　先頭の1bytesあるいは2bypesを読み飛ばす不具合を直しました。
　UTF-8 BOM付き対応も実施し、ASCII/Shift-JIS/UTF-8(BOM有り無し)/UTF-16のいずれであっても読み込めるようにしました。


7) 開発環境変更
　Visual Studio 2005とDDKの併用から、Visual Studio 2022 + EWDKの環境に移行しました。
　その際、すべてのソースコードを UTF-8に変更してます。
　同時に、64bit対応時に積み残した64bitポインタキャスト(DWORD -> DWRD_PTR, (HWND) -> (HWND)(ULONG_PTR)などを修正し、build時にワーニングを減らしました。

　すべての実行ファイル、DLLファイル、SYSファイルにEV署名を付与しています。
　nodoka64.exeから読み出すDLLにはデジタル署名が必要です。これはDLLインジェクションの脆弱性回避策のひとつです。
　なおAzure Artifact Signingは取得していません。

8) リモートデスクトップ対応
　リモートデスクトップ環境では、従来LL Hookでの動作状態となります。しかし nodoka64.exe起動時の引数に -f をつけると、デバイスドライバモードで動作します。
  Hyper-V環境では、デバイスドライバを使いたい場合、-fをつけることが必要になります。
　本機能は4.27から実装していました。これはWindows 7以降のRDP機能では、terminpt.sysで構成されるRemote Desktop Keyboard Deviceが他のキーボードと同様に
　存在し、nodokad.sysとの連動が可能になっていることによります。参考情報としては以下のサイトに記載があります。

　How to enable a third-party driver to intercept and disable the SAS keyboard sequence in Remote Desktop Protocol (RDP) sessions for Windows 7, Windows Server 2008 R2, Windows 8 and Windows Server 2012
　http://web.archive.org/web/20150111112645/http://support.microsoft.com/kb/2867446

　なお nodokad.sysでは、Windows Serverへの複数セッションには対応していません。


9) 次版に向けての実験実装について
　以下の3案の実装を進めています。このうち、本4.31では、 Combo/TapDance/TapHold機能を試すことが可能です。

・GUI設定エディタ
　　別アプリのため、本4.31では実装無し

・複数キーボード完全対応
　　kdbaddid.sys(未公開)のデバイスドライバを使った複数キーボード対応コードが実装済ですが、該当デバイスドライバが無い環境では動作しません。
　　外部キーボードをデバイスごとあるいはインスタンスごとに、K1からK7モディファイヤーに紐づける機能です。
　　既存4.20での実装とは異なり、キーダウンの最初からキーを区別できるようになります。

・Combo/TapDance/TapHold の具体化確認のための実装
　ドキュメントとしては以下の通りです。ちなみにComboの最高キー数を6個としたのは、点字の六点入力を意識しました。
　なおComboで多くのキーの同時入力を使うには、Nキーロールオーバーのキーボードを必要とする場合があるので
　設定どおりに動くとは限りません。

--- Combo 
2個から6個までのキーの同時押しによるキーリマップの実現
　combo [修飾子-] KEY1 KEY2 [KEY3 … KEY6] [window=<ms>] = ACTION

# Combo用のdef option
def option ComboWindow           = <ms>                          # デフォルト 50
　タイムアウト方式での時間幅の設定

def option ComboDetector         = timeout | immediate | rollover | strict-order | zero-latency
　同時押し検出方法の指定

def option ComboIdleThreshold    = <ms>                          # デフォルト 0（無効）
　同時押し検出猶予時間

def option ComboOverlapRatio     = <0–100>                       # デフォルト 0（無効）
　同時押し判断に、重なり割合も付加し精度を向上させます

def option ComboNestedAlwaysMatch = on | off                     # デフォルト off
　Combo第1キーが押下中にタイムアウトした場合、Combo検出モードを継続
　このオプションが効くモードは、タイマーがあるtimeout, strict-order, zero-latencyのみ

ComboDector引数の意味
　同時押し検出方法をモード名で指定します。

モード		動作
timeout		タイムアウト方式（デフォルト）。ComboWindow 内に全キーが揃えば発火
immediate	タイマーなし。全キーが揃った瞬間に即発火
rollover	タイマーなし。コンボ外キーの割り込みを許容
strict-order	定義順通りの押下順序を要求
zero-latency	key1 を即時出力し、コンボ成立時は IME バックスペース補正で取り消し
　              IME On時に機能し、IME Off時には、immediateで動く

&SetComboDetector 関数
　Combに関するパラメーターを本関数実行以後で変更します。
　&SetComboDetector(mode, window, overlap, nested, idle)

　引数の指定は、modeのみ文字列で、残りは数値のみ記述できます。

　&SetComboDetector(immediate, -1, -1, -1, -1)
　&SetComboDetector(rollover,  80, -1, -1, -1)

引数	型	意味
mode	モード名	timeout / immediate / rollover / strict-order / zero-latency
window	ms / -1	コンボウィンドウ
overlap	0–100 / -1	オーバーラップ率閾値
nested	0 or 1 / -1	ComboNestedAlwaysMatch
idle	ms / -1	ComboIdleThreshold


--- TapDance
　単位時間内での、シングル/ダブル/トリプルでのそれぞれ異なるアクションを実施します。

# グローバルオプション（キーマップ外）
def option TapDanceTimeout = <ms>    # デフォルト 300

# ルール定義（keymap 内）
tapdance [修飾子-] KEY [=]
    tap1=ACTION                      # 1回タップ（必須）
    [tap2=ACTION]                    # 2回タップ（省略可）
    [tap3=ACTION]                    # 3回タップ（省略可）
    [timeout=<ms>]                   # このルール専用のタイムアウト（省略でグローバル値）

tapdance Escape = tap1=Escape  tap2=C-OpenBracket  timeout=200


--- TapHold
キーの押下時間で「短押し（タップ）」と「長押し（ホールド）」を判定し、異なるアクションを実行

# グローバルオプション（キーマップ外）
def option TapHoldThreshold = <ms>          # デフォルト 200
　Tap/Hold判定閾値

def option TapHoldInterrupt = tap | hold    # デフォルト tap
　TapHold判定中に他のキーが押された場合、指定されたtapあるいはholdと判断します。

def option TapHoldPermissiveHold = on|off      # デフォルト off
　他キーの「DOWN→UP」が完了してから HOLD 確定

def option TapHoldOnOtherKeyPress = on|off     # デフォルト off
　他キーが DOWN した瞬間に HOLD 確定

def option TapHoldQuickTapTerm = 120           # 0=無効、デフォルト 0
　直前タップから 指定時間内に同じキーを押したら保留とせず、タップ動作とする

# ルール定義（keymap 内）
taphold [修飾子-] KEY [=]
    tap=ACTION
    hold=ACTION
    [threshold=<ms>]          # このルール専用の閾値（省略でグローバル値）
    [interrupt=tap|hold]      # このルール専用の割り込み動作（省略でグローバル値）

ルールごとに def optionの定義を上書きできます。

taphold A  tap=a     hold=Shift_L              permissive_hold=on
taphold T  tap=t     hold=&Layer(layer_edit)   hold_on_other_key=on
taphold SP tap=Space hold=&Layer(num)          quick_tap_term=120


taphold F  tap=F  hold=C-                     # F: 短押し→f / 長押し→Ctrl


以上
