# nexpress — IExpress 互換 SFX パッケージャ

`nexpress` は Windows 標準の `iexpress.exe` / `wextract.exe`(SFXアーカイブの作成・展開)の
代替品です。`.sed` ファイルのフォーマットや `TargetName`/`SourceFiles0` の書式は IExpress
互換のまま、以下を実現するために自作されています。

- **RPTファイル不要**: IExpress には「相対パスの `TargetName` を渡すと RPT ファイルの
  パス計算を誤る」既知のバグがあり、`C:\-name.RPT` のような変なパスにログを書こうとして
  失敗することがある。nexpress は RPT を一切書かないので、このバグ自体が起こらない。
- **CVE級の脆弱性対策込み**: 一時フォルダ/一時ファイルの扱いに CWE-377(予測可能な一時ファイル)・
  CWE-59(シンボリックリンク追従)・CWE-427/428(DLL/実行ファイル探索パスハイジャック)・
  CWE-22(Zip-Slip 相当のパストラバーサル)対策を作り込んである(詳細は後述)。
- nodoka のインストーラ `nodoka-<VERSION>_setup.exe` を作るビルドステップ
  (`distrib/distrib.mak`)から呼ばれている。

構成は2つの実行ファイルに分かれます。

| ファイル | 役割 | 動く場所 |
|---|---|---|
| `nexpress.exe` | `.sed` を読み、指定ファイル群を CAB にまとめ、`stub.exe` を先頭に貼り付けて 1本の SFX EXE を作る | ビルドマシン(nmake実行時) |
| `stub.exe` | `nexpress.exe` が生成する SFX EXE の中身そのもの。自分自身から CAB を取り出し、展開して起動する | エンドユーザー環境(setup.exe実行時) |

---

## ビルド・配置

- `nexpress.vcxproj` : `nexpress.exe` をビルド。`stub\Win32\Release\stub.exe` を
  `nexpress.rc` 経由でリソース `IDR_STUB`(`RCDATA`, `resource.h` で ID=101)として埋め込む。
  そのため **`stub.exe` を先にビルドしてから `nexpress.exe` をビルドする**必要がある。
- `stub\stub.vcxproj` : `stub.exe` をビルド。マニフェストで
  `UACExecutionLevel=requireAdministrator`(常に昇格実行)。
- 両プロジェクトとも `<AdditionalOptions>/DEPENDENTLOADFLAG:0x800</AdditionalOptions>` を
  リンカに指定(後述の CWE-427 対策)。
- `distrib/distrib.mak` からは `..\tools\nexpress\Win32\Release\nexpress.exe` を
  `IEXPRESS` マクロとして参照している(旧 `iexpress.exe` 呼び出し行はコメントアウトで残置)。

---

## 使い方

### 1. `.sed` を作る — `tools/geniexpress`

`geniexpress` は Perl スクリプトで、IExpress 互換の `.sed` テキストを標準出力に吐く。

```
perl tools/geniexpress SFX.EXE TITLE SETUP.EXE [Files...]
```

- `SFX.EXE` : 出力する SFX EXE 名(`TargetName`)。ディレクトリ区切りが無ければ
  `.\` を自動で前置(前述の RPT パスバグ回避の名残。nexpress 自体は RPT を書かないので
  実害はないが、IExpress 互換のため踏襲)。
- `TITLE` : `FriendlyName`。
- `SETUP.EXE` : SFX 実行時に起動する実行ファイル名(`AppLaunched`)。CAB に含める
  ファイルの1つである必要がある。
- `Files...` : CAB に含めるファイル。**`..\` で始まる相対パスのみ**採用される
  (`distrib.mak` の呼び出し規約に合わせたフィルタ)。空文字列や `=` 単体の引数は
  nmake の行継続の都合で紛れ込むことがあるため無視する。

distrib.mak での実際の呼び出し(`all:` ターゲット):

```
$(GENIEXPRESS) nodoka-$(VERSION)_setup.exe "Nodoka $(VERSION)" setup.exe $(DISTRIB) > __nodoka__.sed
```

### 2. SFX を作る — `nexpress.exe`

```
nexpress /N <sedfile>
```

`.sed` の `[Options]` から `TargetName`・`AppLaunched`、`[SourceFiles]`/`[SourceFiles0]` から
ベースディレクトリと収録ファイル一覧を読み、`TargetName` で指定した EXE を生成する。

処理の流れ:
1. `.sed` のパスを解決(`TargetName`・`SourceFiles0` はいずれも `.sed` の置き場所からの相対)。
2. `[SourceFiles0]` の `相対パス=` 形式のキー一覧を読み取り、実ファイルへ解決。
   **CAB内の格納名は常にソースファイルのベース名**になる(ディレクトリ構造は保持しない)。
3. ベース名が重複するファイルが2つ以上あればここで即エラー終了する(後述「既知の落とし穴」参照)。
4. FCI (`cabinet.dll` の Cabinet File Interface)で全ファイルを1本の CAB に LZX 圧縮でまとめる。
5. `stub.exe`(リソース埋め込み分)→ CAB データ → `SFXTrailer` 構造体、の順に連結して
   `TargetName` へ書き出す。

出力ファイルのバイナリレイアウト:

```
[stub.exe 本体] [CAB データ] [SFXTrailer]
```

署名(`signtool sign`)すると、Authenticode の `WIN_CERTIFICATE` がさらに末尾に付く
(`[stub.exe][CAB][SFXTrailer][WIN_CERTIFICATE]`)。

`SFXTrailer`(`main.cpp`/`stub.cpp` で完全に一致させる必要がある固定レイアウト構造体):

```c
struct SFXTrailer {
    UINT64 magic;          // 'N','E','X','P','R','S','S','1' の8バイトをリトルエンディアンで詰めたタグ
    UINT64 cabOffset;      // ファイル先頭からCABデータまでのオフセット(=stub.exeのサイズ)
    UINT64 cabSize;        // CABデータのバイト数
    char   appLaunched[256]; // 展開後に起動する実行ファイル名(ANSI, destDir相対)
};
```

### 3. SFX を実行する — `stub.exe`(=生成された `setup.exe` 等そのもの)

```
<package>              通常起動: 展開して AppLaunched を実行し、終了後に一時フォルダを消す
<package> /T:<folder>  <folder> を作業フォルダにして展開・実行。終了後は展開したファイルのみ削除
                        (フォルダ自体は残す。ユーザー指定フォルダを丸ごと消さないため)
<package> /C /T:<folder>  <folder> へ展開のみ。AppLaunched は実行しない。フォルダは残す
<package> /C           /T: 省略時はフォルダ参照ダイアログで展開先を選ばせる(IExpress互換)。
                        キャンセルなら何もせず終了(exit code 0)
```

`wextract.exe` の `/Q` と `/C:<Cmd>` は未実装(需要が出たら追加、の位置づけ)。

起動時の処理(概要):
1. 自分自身(`GetModuleFileNameW`)を開き、末尾 64KB を後方から `NEXPRESS_MAGIC` で走査して
   `SFXTrailer` を見つける(署名後は末尾に証明書が付くため、末尾ちょうどではなく走査が必要)。
2. `CreateSecureTempDir()`(→後述 `secure_temp.h`)でランダム名・所有者限定 DACL の
   作業フォルダを作り、そこに CAB を書き出す。
3. FDI (`cabinet.dll` の File Decompression Interface)で CAB を `destDir` に展開。
   展開先は `/T:` 指定があればそのフォルダ、なければ作業フォルダ配下の `out\`。
4. `/C` なら展開だけで終了。そうでなければ `AppLaunched` を起動して終了コードを待ち、
   同じ終了コードで自分も終了。
5. 後片付け: `/T:` 使用時は展開した個々のファイルだけ削除(フォルダは残す)。
   それ以外は作業フォルダごと再帰削除。

**ログ**: 自分自身(exe)と同じ場所・同じベース名で `<name>.log`(UTF-8, BOM付き)を書く。
`extractOnly`/`targetDir` の設定値から、トレーラ検出位置、展開したファイル1件ずつ、
`CreateProcess` の成否まで記録されるので、配布先での不具合調査はまずこのログを見るとよい。

---

## セキュリティ設計 — `secure_temp.h`

`nexpress.exe`(ビルドマシン上)と `stub.exe`(エンドユーザー環境、既定で管理者昇格実行)の
両方が使う共有ヘッダ。特に `stub.exe` は攻撃対象になりやすい(ダウンロードフォルダに置かれ、
昇格実行される)ため、一時フォルダまわりの脆弱性を作り込まないよう以下を保証している。

| 関数 | 役割 |
|---|---|
| `HardenDllSearchPath()` | `SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32)` + `SetDllDirectoryW("")`。以降の `LoadLibrary` をSystem32限定にし、CWDをDLL探索対象から外す |
| `RandomHexName()` | `RtlGenRandom`(advapi32 の内部エクスポート名 `SystemFunction036`)で128bit乱数を取り32桁hex文字列化 |
| `BuildOwnerOnlySD()` | 継承なし保護DACL(`D:P`)、SYSTEM+起動ユーザーのみフルコントロールのセキュリティ記述子を作る |
| `CreateSecureTempDir()` | `%TEMP%\nexpress_<32桁hex>` を上記DACL付きで新規作成。`ERROR_ALREADY_EXISTS`(=128bit衝突、実質的に攻撃)なら使い回さず最大10回リトライ |
| `SecureDeleteDirRecursive()` | 再帰削除。**リパースポイント(ジャンクション/シンボリックリンク)には絶対に入り込まず、リンクそのものだけを外す** |

対応している脅威と対策の対応表:

| CWE | 内容 | 対策 |
|---|---|---|
| CWE-377 (予測可能な一時ファイル) | 一時フォルダ名を推測して先回りできない | 128bit暗号乱数の名前 |
| CWE-59 (リンク追従) | 事前に仕込まれたジャンクション/シンボリックリンクを"乗っ取って"使ってしまう | `CreateDirectoryW`/CAB展開先とも `CREATE_NEW` で「今この瞬間に自分が作った実体」であることを保証。削除時もリンクの中身を追わない |
| — (アクセス制御) | 共有TEMP環境で他ユーザーから読み書きされる | 起動ユーザー+SYSTEM限定の保護DACL |
| CWE-22 (Zip-Slip相当のパストラバーサル) | CAB内エントリ名に `..`・パス区切り・ドライブレターを混ぜて展開先の外に書き込む | `fdintCOPY_FILE` でエントリ名を検査し、該当すれば展開を拒否して中止 |
| CWE-427 (DLL探索パスハイジャック) | `cabinet.dll` などの暗黙リンクDLLを、SFXを置いたフォルダ(Downloads等)から先に読ませて昇格実行時にコード実行 | リンカ `/DEPENDENTLOADFLAG:0x800` + `HardenDllSearchPath()` の二重対策 |
| CWE-428 (未引用実行パス) | `CreateProcess` の未引用コマンドラインに空白入りパスを渡すと意図しないexeが実行され得る | `lpApplicationName` にフルパスを明示し、`argv[0]` を引用符で囲む |

`stub.exe` は `requireAdministrator` で常に昇格実行されるため、これらの穴は
「単なるバグ」ではなく **ローカル権限昇格(EoP)に直結する**、という位置づけで作り込まれている。

---

## 既知の落とし穴

- **CAB内の格納名はベース名のみ**(ディレクトリ構造を持たない)。そのため、
  ソースファイル群に**同じベース名を持つファイルが2つ以上**あると、CAB内で名前が
  衝突する。`nexpress.exe` は追加前にこれを検出して `Die()` で明示的にエラー終了する
  (「どの2つのソースパスが衝突しているか」を表示)。
  - 昔は検出しておらず、`stub.cpp` が展開時に `CREATE_ALWAYS` で黙って上書きしていたため
    症状が隠れていたが、CWE-59対策で展開先を `CREATE_NEW` に変えた結果、2つ目の展開が
    `ERROR_FILE_EXISTS`(`err=80`)で失敗するようになり、症状が表面化した。
  - 実例: `distrib.mak` の `DISTRIB_PDB` に x86/x64 双方の `nodokad.pdb` が
    素の相対パスで列挙されており、ベース名が衝突していた。ドライバ本体(`.sys/.inf/.cat`)や
    `DriverManager.exe` は既に `nodokadx86.*` のようにリネームコピーしてから配布リストに
    載せる対応が取られていたが、`nodokad.pdb` だけこの対応が漏れていたのが原因
    (2026-07-16 修正、`distrib.mak` に `nodokadx86.pdb` へのリネームコピーを追加)。
  - **教訓**: `distrib.mak` の `DISTRIB_*` リストに新しいファイルを追加するときは、
    x86/x64間や複数出力ディレクトリ間でベース名が衝突しないか必ず確認すること。
    衝突する場合は既存の `cp -f` リネームパターンに倣う。
- `/T:` で指定した既存フォルダに同名ファイルが残っていた場合、展開直前に
  `DeleteFileW` で先に消してから `CREATE_NEW` している(ユーザー指定フォルダでは
  上書きが正当な操作のため)。フォルダを丸ごと再帰削除する経路とは扱いが違う点に注意。
- `nexpress.exe`(ビルドマシン側)は攻撃面としては優先度が低いが、同じ `secure_temp.h` を
  流用しているため一時ファイル(CAB, FCIの一時ファイル)も同じ保証を受ける。
