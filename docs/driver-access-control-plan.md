# デバイスドライバ アクセス制御プラン（自前アプリ以外からのアクセス禁止）

作成日: 2026-07-17 / 更新: 2026-07-17（L0が特権前提のため署名確認=L2を本命へ） /
再更新: 2026-07-19（実機検証でL2単独不採用、L3必須+L2ベストエフォート併用へ方針転換）

## 0. 2026-07-19 実機検証の結論（手順1着手時に発見）

**`SeGetCachedSigningLevel` 単独では L2 は成立しない。** 実機（WDAC/Smart App Control 無効の
通常の Windows 11、開発機）で以下を確認した:

- `nodoka\common\sigcheck.c/h`（診断専用トレース版）を作成し、`d2\control.c` の
  `Nodoka2EvtFileCreate` 冒頭に仕込んでビルド・実機ロードして検証。
- 署名済み `nodoka\x64\Release\nodoka64.exe`（2026-07-16 ビルド、CN=Applet LLC、
  EV証明書サムプリント `0AF36F94140C861D17147C17334866BD3B581A87`）を**通常起動**したところ、
  `SeGetCachedSigningLevel` は `STATUS_NOT_FOUND (0xC0000225)` で失敗
  （`PsReferenceProcessFilePointer` 自体は成功）。
- 原因: `SeGetCachedSigningLevel` は「CI (Code Integrity) が過去に実際に検証しキャッシュ済み」
  の場合のみ結果を返す参照専用 API。WDAC/Device Guard/Smart App Control/S モード/PPL 等の
  強制ポリシーが無い通常の Windows は、ユーザーモード EXE の起動時に CI が署名検証を
  そもそも実行しない（コスト回避のためスキップ）。ドライバ側から「今すぐ検証してキャッシュ
  を作れ」と要求する軽量な公式 API は存在しない。
- 検討した代替（自前 Authenticode 検証: `PsSetLoadImageNotifyRoutineEx` + PE 内 PKCS#7 を
  自前パース）は不採用と判断。理由: カーネルモードには ASN.1/PKCS#7/X.509 デコーダが標準で
  存在せず（`crypt32.dll` はユーザーモード専用）、PE Authenticode ハッシュ計算・DER 再エンコード
  ・署名検証まで含めて完全新規のバイナリパーサをカーネルに持ち込むことになる。本プロジェクトの
  BSOD 履歴（`detourCreate` 周りだけで Fix 1〜K）を踏まえるとバグ密度・デバッグコストが見合わない
  （同種の実装で実際に CVE を出した例: CVE-2020-1464）。

**方針転換: L3（challenge-response）を必須のゲートとし、L2（`SeGetCachedSigningLevel`）は
"取れれば使う" ベストエフォートの早期パスとして両方実装する。** WDAC 等が有効な環境では
L2 だけで即座に弾ける（ホットパス的に軽い）が、L2 が失敗した場合（大多数の通常環境）は
必ず L3 のチャレンジレスポンスへフォールバックし、そこで最終判定する。L2 単独条件での
`ACCESS_DENIED` は出さない（誤検知で正規アプリを弾くリスクがあるため）。

### L3 実装時の注意: 既存 `token.c` は流用できるのは BCrypt API パターンのみ

`nodoka_subscribe\addid\kbdaddid\token.c`（sibling リポジトリ）を確認した。これは
**ライセンストークン検証**（サーバー発行の静的な署名済み JSON blob をレジストリから読み、
ECDSA 署名・有効期限・MachineGuid 一致を確認するだけ）であり、**チャレンジレスポンスではない**。
呼び出し元プロセスがその場で秘密鍵を所持していることを証明する仕組みは無く、blob を
コピーされれば別プロセスからでも通ってしまう（ライセンス用途としてはそれで正しいが、
呼び出し元認証には使えない）。

L3 で実際に再利用できるのは `BCryptOpenAlgorithmProvider` / `BCryptImportKeyPair`
（`BCRYPT_ECCPUBLIC_BLOB`） / `BCryptVerifySignature` / `BCryptGenRandom` という
**BCrypt CNG API の呼び出しパターンとリンク設定（`ksecdd.lib` 依存）** であって、
`ValidateLicenseToken` 関数そのものではない。`IOCTL_AUTH_BEGIN`（`BCryptGenRandom` で
nonce 発行）→ クライアントがアプリ内秘密鍵で nonce に署名 → `IOCTL_AUTH_RESPONSE` で
提出 → `BCryptVerifySignature` で埋め込み公開鍵と照合、という新規プロトコルを設計・実装する
必要がある（§5 の手順4に相当）。

## 1. 目的と脅威モデル

対象ドライバがユーザーモードに公開する「キー入力の取り出し（sniff）」「キー入力の注入（inject）」
機能を、**自前アプリ（信頼できるベンダー製バイナリ）以外から利用させない**ことが目的。

想定攻撃者:
- (T1) 一般権限の悪意アプリ … デバイスを open してキーロガー化 / キー注入する
- (T2) 管理者権限を得た悪意アプリ
- (T3) 自前プロセスへスレッド注入 / ハンドル奪取して正規経路を乗っ取る（同一ユーザー権限で可能）
- (T4) 正規アプリのリバースエンジニアリングで鍵・秘密を抽出する

**制約**: クライアントアプリに管理者特権を要求できない（→ L0/L1 の DACL 厳格化は単独では不採用）。

**現実的なゴール**: T1 を完全遮断。T2/T3/T4 を「管理者権限 + リバースエンジニアリング」なしには
不可能な水準へ。非特権のまま同一ユーザーの悪意アプリまで完全に耐えるのは PPL 無しでは困難。

## 2. 現状のコード確認結果（脆弱面の所在）

| ドライバ | 種別 | ユーザーから open 可能な名前 | 呼び出し元認証 | リスク |
|---|---|---|---|---|
| `nodoka\d` (nodokad) | WDM detour | `\\.\NodokaWalk1` | **なし** | **高**: read=sniff / write=inject |
| `nodoka\d2` (nodokad2) | KMDF control | `\\.\Nodoka2Ctl` | **なし**（SDDL が Everyone/AuthUsers 許可）| **高**: GET_EVENTS/INJECT |
| `nagi\...\kbdaddid` | filter | なし（無名デバイス）| N/A | 低: ExtraInfo タグ付けのみ |
| `nagi\...\mouaddid` | filter | なし | N/A | 低 |
| `nodoka_subscribe\...\kbdaddid` | filter + token | なし | N/A | 低 |
| `nodoka_subscribe\...\mouaddid` | filter | なし | N/A | 低 |

- `nodoka\d\nodokad.c:493` は SDDL なし・`FILE_DEVICE_SECURE_OPEN` なしで `IoCreateDevice`。
  `detourCreate`(1201) は相手を問わず成功。`detourRead`(1290)=sniff / `detourWrite`(1342)=inject。
- `nodoka\d2\control.c:31` の SDDL `D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;WD)(A;;GA;;;AU)` は
  Everyone/AuthUsers 全許可。`Nodoka2EvtFileCreate`(133) 無条件受理。
- addid 系は無名デバイスで名前 open 不可＝ユーザー指摘どおり低リスク。

## 3. 「ライセンス」と「アクセス制御」は別問題

`token.c`（ECDSA P-256）やレジストリ `License` は「機能有効化」を決めるのみ。有効化後は任意アプリが
open できる。よって別レイヤーの**呼び出し元認証**が必要。

## 4. 対策（特権不要を最優先）

### レイヤー2: CREATE 時の呼び出し元コード同一性検証（署名バイナリ確認）★本命

**デバイス DACL を緩めたまま＝クライアント非特権のまま**運用できるのが最大の利点。
デバイスは一般ユーザーが open 可能なままにし、CREATE 時に**開いてきたプロセスの実行イメージが
自製署名バイナリか**を検証、不合格なら `STATUS_ACCESS_DENIED`。SYSTEM 権限の他プロセスすら弾ける。
対象 OS Windows 10/11（NTDDI 0x0A00000C）で下記 API 利用可。

#### 実装レシピ（CREATE は PASSIVE_LEVEL・呼び出し元コンテキスト）

1. 呼び出し元プロセス取得: `PsGetCurrentProcess()` または `IoGetRequestorProcessId(Irp)`。
2. メイン実行イメージの FILE_OBJECT 取得: `PsReferenceProcessFilePointer(proc, &fileObj)`（Win8+）。
3. **`SeGetCachedSigningLevel(fileObj, &flags, &signingLevel, thumbprint, &thumbSize, &thumbAlgo)`**。
   CI が確立済みのキャッシュ署名レベルに加え**リーフ証明書サムプリント**を返す。
   これを**ドライバに埋め込んだベンダー証明書サムプリント**と `RtlCompareMemory` 照合し、
   `signingLevel` も要求水準以上か確認。
4. 不一致なら `STATUS_ACCESS_DENIED` で complete。`ObDereferenceObject(fileObj)`。

- d2 は `Nodoka2EvtFileCreate` 冒頭に挿入。nodoka\d は `detourCreate` の**最初期**
  （refcount/session/queue 操作の前）に置き即 complete。CREATE 早期失敗は Fix 1-7 の
  open 途中キャンセルより安全で BSOD リスクを増やさない。認証は open 時 1 回・ホットパス無改変。
- `SeGetCachedSigningLevel` は公開 WDK ヘッダに宣言が無い版があり自前 extern 宣言する
  （多製品採用の準ドキュメント API）。CI キャッシュ未設定時の経路（`SeSetCachedSigningLevel` 相当）に留意。

#### 課題: EV 証明書の年次更新でリーフサムプリント照合が壊れる

EV 証明書は 1 年ごとに更新され、更新のたびに**リーフ（エンドエンティティ）証明書が別物**になる
（シリアル・サムプリント・通常は鍵ペアも変わる）。`SeGetCachedSigningLevel` が返すのはこの
**リーフのサムプリント**なので、ドライバに焼き込んだ比較用サムプリント定数は更新後の値と一致しなくなる。
特にドライバが枯れて再ビルドされず古い定数のまま、アプリだけ新しい EV 署名になった場合、
**新しいアプリが古いドライバに `STATUS_ACCESS_DENIED` で拒否される**。

補足: ここで照合するのは「ドライバに焼き込んだ比較用サムプリント定数」であって、ドライバ自身の
Authenticode 署名ではない。ドライバ自身の署名が古いこと自体は（RFC3161 タイムスタンプがあれば
証明書期限切れ後もロード可能なので）ロードには支障しない。壊れるのは焼き込み定数の照合だけ。

**対処: アプリとドライバの署名（＝焼き込みサムプリント）を同じ世代で揃えておく。**
EV 証明書を更新したら、アプリの再署名と同時にドライバの比較用サムプリント定数も新証明書のものへ
更新し、ドライバを再ビルド・再署名して両者を一致させる。運用ルールとして
「EV 更新 ⇒ アプリとドライバをセットで新証明書に切り替える」を徹底すれば、リーフサムプリント照合の
ままで問題ない。ドライバを毎年触りたくない／確実にローテーション耐性を持たせたい場合は、
L3 の自前長寿命鍵（`token.c` 流用の埋め込み公開鍵）を身元の起点にする方式（EV 更新の影響を受けない）を
検討する。

#### 課題: ドライバの EV + WHQL 二重署名との関係

ドライバは EV 署名に加えて WHQL（アテステーション）署名も重畳される。これが L2 に影響するかは
**検証の向き**で決まる。

- **今の L2（ドライバがアプリを検証する向き）＝影響なし。**
  L2 が呼ぶのは `SeGetCachedSigningLevel(appFileObj, ...)` で、返るのは**アプリ側のリーフ
  サムプリント**。ドライバ自身の署名構成（EV／WHQL／二重）は照合に一切入らない。照合用の焼き込み
  定数は「アプリの EV サムプリント」なので独立。アテステーション署名は .sys に Microsoft 署名を
  **追記**するだけで、埋め込んだ比較用定数（ファイル内データ）は書き換わらず照合は無傷。
  なおアプリ側は EV のみで WHQL は付かない（WHQL はドライバ専用制度）。

- **逆向き（アプリがドライバを検証する＝将来の相互認証）＝重要な落とし穴あり。**
  アテステーション/WHQL を通すと、OS から見たドライバの署名者は Microsoft の
  **"Windows Hardware Compatibility Publisher"** になり、**ベンダー EV リーフではなくなる**
  （EV はポータル提出の認証に使われるだけで、出来上がる .sys は Microsoft 署名）。
  よってアプリ側でドライバを**ベンダー EV サムプリントで照合すると失敗する**。相互認証を入れる場合は、
  ドライバの身元を EV サムプリントではなく **L3 の自前長寿命鍵**（`token.c` 流用の埋め込み公開鍵）を
  起点にするか、「その特殊なデバイス名でロードできているのは Microsoft 署名を通った正規ドライバだけ」
  という事実に依拠する設計にする。

- **WHQL 提出時の静的解析への配慮.** `SeGetCachedSigningLevel` / `PsReferenceProcessFilePointer` は
  準ドキュメント API。アテステーション署名は機能テストを走らせないため実行時利用は問題ないが、
  本リポジトリは `*-mustfix.sarif`（DVL/CodeQL 相当）を回しているため、未宣言 extern が警告を出さない
  よう適切なプロトタイプを用意しておく。引っかかる場合は代替実装（下記）へ寄せる。

#### 代替実装（署名者を確実に取りたい場合）

`PsSetLoadImageNotifyRoutineEx` でメイン EXE ロードを捕捉し、PE 埋め込み PKCS#7（WIN_CERTIFICATE）を
**既存 token.c の BCrypt 流用で Authenticode 自前検証**。自製管理下に置けるが実装量大。
まず `SeGetCachedSigningLevel` 版を推奨。

#### 署名確認だけでは防げないもの（要 L3 併用 / 根本は PPL）

- **同一ユーザーのハンドル奪取**: 攻撃者が同ユーザーなら正規プロセスへ
  `OpenProcess(PROCESS_DUP_HANDLE)` してデバイスハンドルを複製できる。開いたのは正規アプリなので
  署名検証は通過。
- **スレッド/DLL 注入・プロセスハロウイング**: 正規署名 EXE を backing にメモリは攻撃者コード。
  `SeGetCachedSigningLevel` はファイル署名を見るためメモリ改竄は非検知。

対策: (a) L3 の暗号ハンドシェイク併用 + 起動時プロセス緩和ポリシー（非 MS バイナリブロック・動的コード禁止）で
注入耐性を上げる、(b) 根本は L4=PPL。

#### nodoka\d（detourCreate）へ L2 を入れる場合の要点

`d` にも L2 は実装可能で、以下の理由でむしろ軽く・安全にできる。

- **配置**: `detourCreate`(nodokad.c:1201) の**関数冒頭、`InterlockedIncrement(&isOpen)` の前**に
  署名チェックを置く。IRP_MJ_CREATE は PASSIVE_LEVEL・呼び出し元スレッド文脈なので
  `PsGetCurrentProcess()` が opener を返し、`PsReferenceProcessFilePointer` /
  `SeGetCachedSigningLevel` を安全に呼べる（まだ `KeAcquireSpinLock` で DISPATCH に上げていない）。
- **拒否パス**: 不合格なら `irp->IoStatus.Status = STATUS_ACCESS_DENIED; irp->IoStatus.Information = 0;`
  → `IoCompleteRequest(irp, IO_NO_INCREMENT)` → return。既存の refcount / session / queue /
  IRP キャンセル（Fix 1-7）に**一切触れない**ため、BSOD 履歴のある経路を増悪させない。
- **BCrypt 不要**: `SeGetCachedSigningLevel` は OS(CI) が済ませた検証結果（署名レベル＋リーフ証明書
  サムプリント）を返すだけなので、`d` に ECDSA/BCrypt を持ち込む必要がない。`RtlCompareMemory` で
  埋め込みサムプリントと照合するだけ＝依存が増えず、レガシードライバへの改変が最小。
- **リンク**: `SeGetCachedSigningLevel` / `PsReferenceProcessFilePointer` は ntoskrnl エクスポート。
  公開ヘッダに無い版は自前 extern 宣言（ntoskrnl.lib は kernel driver なので既にリンク済み）。
- **d2 との差（L3 併用時の注意）**: `d` は WDM で KMDF のファイルコンテキストが無い。open ごとの
  認証状態（L3）を持つには `IoGetCurrentIrpStackLocation(irp)->FileObject->FsContext` に
  自前フラグを格納し、detourRead/detourWrite で参照する形になる（FUS で複数 open があるため
  グローバル extension ではなく FILE_OBJECT 単位が必須）。L2 単独なら create 拒否だけで済み最も簡潔。

結論: **L2 を d に入れるのは可能かつ推奨形**（SeGetCachedSigningLevel + サムプリント照合のみ）。
d2 側の L2/L3 と設計を共有できる。

### レイヤー3: 暗号ハンドシェイク（チャレンジ・レスポンス／署名確認と併用）

open できても正規アプリだけが持つ秘密を証明するまで実データを渡さない。既存 `token.c` の
BCrypt + ECDSA P-256 を流用。open 直後 UNAUTHENTICATED、`IOCTL_AUTH_BEGIN` で `BCryptGenRandom`
チャレンジ発行 → クライアントがアプリ内秘密鍵で ECDSA 署名 → 埋め込み公開鍵で
`BCryptVerifySignature` → 成功まで read/inject/GET_EVENTS/INJECT は `ACCESS_DENIED`（単純フラグ判定・
open/close 無改変）。ハンドル奪取や鍵非同梱アプリを弾く。弱点は秘密鍵がアプリ内に存在すること（T4）。

### レイヤー4（任意・最強）: クライアントの PPL 化

Anti-Malware/LSA 証明書で署名した PPL とし、ドライバは呼び出し元が当該保護プロセスであることを要求。
管理者/同一ユーザー攻撃者にも実質耐える唯一の道。MS 発行証明書 / ELAM 登録が必要で重く、将来の選択肢。

### （参考）レイヤー0/1: DACL 厳格化・SYSTEM サービス化 → 今回は特権前提のため不採用

d2 の SDDL を SYSTEM+Admins へ絞る / nodokad を `IoCreateDeviceSecure` 化 / クライアントを
SYSTEM サービス化する案。T1〜T2 を安価に遮断できるが**クライアントに特権が必要**なため、
本件の制約により単独では不採用。ただし `FILE_DEVICE_SECURE_OPEN` の付与自体（名前空間 open の
DACL 適用）は署名確認と両立し害が無いので、DACL は一般ユーザー許可のまま付けておく価値はある。

## 5. 推奨実施順序

前提: L0/L1（DACL 厳格化・SYSTEM サービス化）はクライアント特権を要するため不採用。L2（署名バイナリ
確認）を本命に据え、`d`・`d2` 双方に同一設計で入れる。判定ロジック（`SeGetCachedSigningLevel` +
埋め込みベンダー証明書サムプリント照合）は共通ヘルパー化して両ドライバで共有する。

1. **L2 判定ヘルパーを共通化して先に用意**
   … `PsReferenceProcessFilePointer` + `SeGetCachedSigningLevel` + `RtlCompareMemory` で
   「呼び出し元プロセスの実行イメージ = 自製署名バイナリか」を返す関数。BCrypt 不要・依存増なし。
2. **`d`（nodokad）へ L2 を実装** … `detourCreate` 冒頭（`InterlockedIncrement(&isOpen)` の前）で
   判定し、不合格は即 `STATUS_ACCESS_DENIED`。既存 refcount/session/queue（Fix 1-7）に触れないため
   BSOD 経路を悪化させず、レガシードライバへの改変も最小。非特権のまま T1 と未署名アプリの直接 open を遮断。
3. **`d2`（nodokad2）へ同じ L2 を実装** … `Nodoka2EvtFileCreate` 冒頭。共通ヘルパー流用。
   併せて `FILE_DEVICE_SECURE_OPEN` は DACL 一般ユーザー許可のまま付与（害なし・名前空間 open を堅くする）。
4. **L3（暗号ハンドシェイク）を併設** … token.c(BCrypt+ECDSA) 流用。
   d2 は KMDF ファイルコンテキストに `isAuthenticated` を持たせ容易。`d` は `FILE_OBJECT->FsContext` に
   認証フラグを格納し detourRead/detourWrite で参照（FUS 対応で FILE_OBJECT 単位必須）。
   ハンドル奪取・鍵非同梱アプリを弾く。まず d2、必要なら d に展開。
5. **L4（PPL）は長期の選択肢** … 同一ユーザー悪意アプリ（T3=ハンドル奪取/スレッド注入/
   プロセスハロウイング）まで完全に耐える必要が出た時に検討。非特権運用とのトレードオフを最終判断。

補足: 将来的には detour 廃止＝`d2` への一本化が保守負担・BSOD リスクの根本的な低減策。
それまでは `d`・`d2` に L2 を両掛けして守る。

## 6. 変更対象ファイル一覧（各項目に [L2]／[L3] を明示）

**[L2] だけを実施する場合はドライバ側コードのみ**（下記 [L2] 行）。アプリはコード変更ゼロ。
ただしアプリが「ドライバに焼き込んだサムプリントと同じベンダー EV 証明書で署名済み」であることが
動作条件（コードではなくビルド/署名の要件。本番アプリは既に EV 署名済みなら追加作業なし。開発時のみ
テスト署名 or デバッグ限定バイパスが必要）。token.c とクライアント行は [L3] 専用なので L2 では触らない。

- **[L2]** `nodoka\d\nodokad.c` … `detourCreate` 最初期に署名検証、`FILE_DEVICE_SECURE_OPEN` 付与。
- **[L2]** `nodoka\d2\control.c` … `Nodoka2EvtFileCreate` 冒頭に署名検証。
- **[L2]** `nodoka\d2\nodokad2.h` … `SeGetCachedSigningLevel` / `PsReferenceProcessFilePointer` 等の
  extern 宣言。
- **[L2]** （任意・新規）共通ヘルパー `nodoka\common\sigcheck.c/h` 相当 …
  `PsReferenceProcessFilePointer` + `SeGetCachedSigningLevel` + `RtlCompareMemory` の判定関数を
  `d`・`d2` で共有。埋め込みベンダー証明書サムプリント定数もここ（またはヘッダ）に置く。
- **[L3]** `nodoka\d2\control.c`（追加分）… `IOCTL_AUTH_BEGIN/RESPONSE`、ファイルコンテキストの
  `isAuthenticated`/`challenge`、GET_EVENTS/INJECT の認証ガード。
- **[L3]** `nodoka\d2\public2.h` … 新 IOCTL 定義。
- **[L3]** `nodoka\d2\nodokad2.h`（追加分）… ファイルコンテキスト構造体拡張。
- **[L3]** `nodoka\d2\token.c`（新規・subscribe から流用）… `BCryptGenRandom` + `BCryptVerifySignature`。
- **[L3]** `nodoka\d\nodokad.c`（追加分）… `FILE_OBJECT->FsContext` に認証フラグ、
  detourRead/detourWrite で参照（FUS 対応で FILE_OBJECT 単位）。
- **[L3]** クライアント（nodoka-gui / サービス）… ハンドシェイク実装、秘密鍵同梱、
  起動時プロセス緩和ポリシー。※[L2] のみなら不要。

## 7. 限界の明示

L2（署名確認）は未署名アプリの直接 open を完全遮断するが、同一ユーザー権限のハンドル奪取・
スレッド注入・プロセスハロウイングは L2 単独では防げない。L3 併用で敷居を上げられるが、秘密鍵が
アプリ内に存在する以上 RE には最終的に弱い。同一ユーザー/管理者攻撃者にも耐える保証が必要なら
PPL（L4）が唯一の道。非特権運用と「同一ユーザー悪意アプリの完全遮断」は本質的にトレードオフ。

## 8. 2026-07-19 実装状況 (d2 側は実装・ビルド確認済み、実機検証待ち)

§0 の方針転換 (L3 必須 + L2 ベストエフォート) を受け、同日中に d2 側の実装を完了した。

- 新規 ECDSA P-256 鍵ペアを生成 (ライセンストークン用の鍵とは別物)。公開鍵は
  `nodoka\common\authchallenge.c`、秘密鍵は `nodoka\nodoka\authkey.cpp` に埋め込み。
- **[L3]** `nodoka\common\authchallenge.h/.c`（新規・当初案の d2\token.c から変更）…
  `NodokaAuthGenerateNonce`（BCryptGenRandom）、`NodokaAuthVerifyResponse`
  （SHA-256(ドメインタグ||nonce) を ECDSA 検証）。d 側でも共有できるよう
  `nodoka\common\` に置いた（§6 の想定パスから変更）。
- **[L3]** `public2.h` … `IOCTL_NODOKA2_AUTH_BEGIN`（nonce 発行）/
  `IOCTL_NODOKA2_AUTH_RESPONSE`（署名提出、1 回消費・5 秒有効期限）を追加。
- **[L2]** `nodoka\common\sigcheck.c/h` … トレース専用から `NodokaSigCheckFastPath()`
  （埋め込みベンダーサムプリント一致時のみ TRUE、それ以外は L3 へフォールバック）へ格上げ。
- **[L3]** `nodoka\d2\control.c` … ファイルコンテキストではなくグローバル変数
  （排他 open なので可、§6 の想定から簡略化）で認証状態を保持。
  `Nodoka2EvtFileCreate` で L2 早期パスを試行、`GET_EVENTS`/`INJECT`/`SET_MODE`
  はすべて未認証時 `STATUS_ACCESS_DENIED`。
- `d2.x64.vcxproj` に `ksecdd.lib` 追加。Debug|x64 ビルド・テスト署名まで警告 0 件で成功確認済み。
- **[L3]** クライアント側 `nodoka\nodoka\authkey.h/.cpp`（新規）+ `engine.cpp` の
  `Engine::open()` … CreateFile 成功後、SET_MODE の前に `NodokaAuthenticateV2Device()`
  を実行、失敗なら open 全体を失敗させる。`nodoka.vcxproj` に `bcrypt.lib` 追加。
  Release|x64 ビルド 0 エラーで成功確認済み。

### 秘密鍵の非公開化 (2026-07-19)

`nodoka\nodoka\authkey.cpp` に直書きしていた ECDSA 秘密鍵を `authkey_secret.h`
(新規・**git 管理対象外**) に分離した。`authkey.cpp` は
`__has_include("authkey_secret.h")` で存在確認し、無ければダミー鍵の
`authkey_secret.example.h`(git 管理対象・全ゼロ値) を使う。ダミーのままでも
コンパイルは通るが実行時の L3 認証 (`IOCTL_NODOKA2_AUTH_RESPONSE`) は必ず失敗する。
`nodoka413\.gitignore` に `nodoka/authkey_secret.h` を追加済み。
本物の鍵は本リポジトリ外 (パスワードマネージャ等) で管理し、開発機ごとに
個別配布すること。`nodoka413` へ同期する際は `authkey_secret.h` を
**絶対にコピー/コミットしない**(`authkey_secret.example.h` のみ追跡対象)。

### 実機検証結果 (2026-07-19、d2側)

ユーザーが実機で検証し、設計通りに動作することを確認した。
- 署名ありアプリ(`nodoka64.exe`): `SeGetCachedSigningLevel`早期パスは想定通り
  `STATUS_NOT_FOUND`で失敗 → L3(`AUTH_BEGIN`→`AUTH_RESPONSE: OK`)へフォールバック
  → 認証成功 → `SET_MODE`/`INJECT`とも正常動作。
- 署名なしアプリ: 署名ありアプリを停止済み(排他ロックは空き)の状態で試行し、
  デバイスドライバを実質的に使用できないことを確認(T1遮断に成功)。
  ※コントロールデバイスの`CreateFile`自体は常に成功する設計(未認証でも open は許可)で、
  `SET_MODE`/`GET_EVENTS`/`INJECT`が`STATUS_ACCESS_DENIED`になる形で拒否される。

### `d`（nodokad）側の展開は保留 (2026-07-19、ユーザー判断)

`d` は detourCreate 周りだけで Fix 1〜K まで積み上げた複雑な状態で BSOD 発生の問題が
残っており ([[project_rim_bsod_0x18]] 等)、L2/L3 認証ロジックを追加で入れることによる
新規不具合リスクの方が、現状の呼び出し元未認証という脆弱性そのものより大きいと判断し、
**`d` への L2/L3 展開は保留**とする。万一この脆弱性を指摘された場合は
「`nodokad2.sys`（L2/L3 実装済み）を使うよう案内する」ことで対応する方針。
`d` は将来的に detour 廃止・`d2` への一本化が進めば自然に解消する（§5 補足参照）。

L4（PPL）は長期の選択肢のまま。
