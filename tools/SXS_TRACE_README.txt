SxS (サイドバイサイド構成) エラー原因特定手順
============================================

「このアプリケーションのサイドバイサイド構成が正しくないため、
 アプリケーションが開始できませんでした。」が出る場合の診断用です。

1. 管理者としてコマンドプロンプトを開く
   （sxstrace は管理者権限が必要です）

2. 次のバッチを実行する:
   tools\sxs_trace_sxs_error.bat

3. 表示される手順に従う:
   - トレース開始後、D:\nodoka\Release\nodoka.exe を実行してエラーを再現
   - エラーが出たら、トレース用ウィンドウで Enter を押して停止
   - 生成された sxs_trace_log\sxs_trace.txt を開く

4. sxs_trace.txt 内で次を検索:
   - "ERROR"
   - "resolution failed"
   - "dependentAssembly"
   どのアセンブリ（名前・バージョン）が解決できなかったかが分かります。

5. 判明したアセンブリに応じた対応:
   - 「複数の requestedPrivileges 要素」→ カスタムマニフェストから trustInfo を削除（本改修で実施済み）
   - Microsoft.VC*.CRT → 該当バージョンの Visual C++ 再頒布可能パッケージをインストール
   - Microsoft.Windows.Common-Controls → マニフェストの processorArchitecture を x86/amd64 の小文字に統一（本改修で実施済み）
   - その他 → イベントビューアのアプリケーションログも参照

本改修で実施した内容:
- 複数の requestedPrivileges による SxS エラー対策: カスタムマニフェストから trustInfo を削除（リンカーが追加する1つだけになる）
- nodoka.manifest 等の processorArchitecture を "X86" → "x86" に統一
- assemblyIdentity の閉じタグのインデントを修正
- 修正後は exe プロジェクトのリビルドを推奨
