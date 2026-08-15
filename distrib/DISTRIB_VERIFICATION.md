# distrib.mak 要求パスとプロジェクト出力先の対応表

distrib プロジェクト（distrib.mak）が要求するパスと、各プロジェクトの Release / Debug / Sample ビルドでの出力先が一致しているかをまとめたドキュメントです。

**Win32 と x64 の出力構成**: Win32 ビルドは `Win32\Release` / `Win32\Debug` / `Win32\Sample`（および各構成名フォルダ）へ、x64 ビルドは `x64\Release` / `x64\Debug` / `x64\Sample` へ出力するように統一しています。

## 1. TARGET_x86 / TARGET_x64（メイン実行体・DLL）

| distrib.mak が要求するパス | プロジェクト | 出力構成 | プロジェクトの OutDir | 一致 |
|---------------------------|-------------|----------|----------------------|------|
| `..\Win32\Release\nodoka.exe` 等 | exe (nodoka) | Release\|Win32 | `$(SolutionDir)Win32\Release\` | ✓ |
| `..\Win32\Sample\nodoka.exe` 等 | exe (nodoka) | Sample\|Win32 | `$(SolutionDir)Win32\Sample\` | ✓ |
| `..\Win32\Debug\nodoka.exe` 等 | exe (nodoka) | Debug\|Win32 | `$(SolutionDir)Win32\Debug\` | ✓ |
| `..\x64\Release\nodoka64.exe` 等 | exe.x64 等 | Release\|x64 | `$(SolutionDir)x64\Release\` | ✓ |
| `..\x64\Sample\nodoka64.exe` 等 | exe.x64 等 | Sample\|x64 | `$(SolutionDir)x64\Sample\` | ✓ |
| `..\x64\Debug\nodoka64.exe` 等 | exe.x64 等 | Debug\|x64 | `$(SolutionDir)x64\Debug\` | ✓ |

## 2. DISTRIB_BIN（共通バイナリ）

| distrib.mak が要求するパス | プロジェクト | 出力構成 | プロジェクトの OutDir | 一致 |
|---------------------------|-------------|----------|----------------------|------|
| `..\Win32\Release\setup.exe` | setup (s) | Release\|Win32 | `$(SolutionDir)Win32\Release\` | ✓ |
| `..\x64\Release\setup64.exe` | s.x64 等 | Release\|x64 | `$(SolutionDir)x64\Release\` | ✓ |
| `..\Win32\Release\gamepad.dll` | gamepad | Release\|Win32 | `$(SolutionDir)Win32\Release\` | ✓ |
| `..\x64\Release\gamepad64.dll` | gamepad.x64 | Release\|x64 | `$(SolutionDir)x64\Release\` | ✓ |
| `..\Win32\Release\nodoka_helper.exe` | nodoka_helper | Release\|Win32 | `$(SolutionDir)Win32\Release\` | ✓ |
| `..\Win32\Release\GuiEdit.exe` | GuiEdit | Release\|Win32 | `$(SolutionDir)Win32\Release\` | ✓ |
| `..\Win32\Release\dotnet_starter.exe` | dotnet_starter | Release\|Win32 | `$(SolutionDir)Win32\Release\` | ✓ |

## 3. DISTRIB_TS4NODOKA（ts4nodoka 派生プロジェクト）

| distrib.mak が要求するパス | プロジェクト | 出力構成 | プロジェクトの OutDir / 出力 | 一致 |
|---------------------------|-------------|----------|------------------------------|------|
| `..\ts4nodoka\thumbsense.nodoka` | （ソース） | - | ts4nodoka フォルダのソース | ✓ |
| `..\Win32\sts4nodoka\sts4nodoka.dll` | sts4nodoka | sts4nodoka\|Win32 | `$(SolutionDir)Win32\sts4nodoka\` | ✓ |
| `..\x64\sts4nodoka\sts4nodoka64.dll` | sts4nodoka.x64 | sts4nodoka\|x64 | `$(SolutionDir)x64\sts4nodoka\` (TargetName: sts4nodoka64) | ✓ |
| `..\Win32\cts4nodoka\cts4nodoka.dll` | cts4nodoka | cts4nodoka\|Win32 | `$(SolutionDir)Win32\cts4nodoka\` | ✓ |
| `..\x64\cts4nodoka\cts4nodoka64.dll` | cts4nodoka.x64 | cts4nodoka\|x64 | `$(SolutionDir)x64\cts4nodoka\` (TargetName: cts4nodoka64) | ✓ |
| `..\Win32\ats4nodoka\ats4nodoka.dll` | ats4nodoka.x64 | ats4nodoka\|Win32 | `$(SolutionDir)Win32\ats4nodoka\` | ✓ |
| `..\x64\ats4nodoka\ats4nodoka64.dll` | ats4nodoka.x64 | ats4nodoka\|x64 | `$(SolutionDir)x64\ats4nodoka\` (TargetName: ats4nodoka64) | ✓ |

## 4. 実施した修正の要約（Win32 フォルダ統一）

- **全プロジェクト**: Win32 構成の `OutDir` を `$(SolutionDir)$(Configuration)\` から `$(SolutionDir)Win32\$(Configuration)\` に変更し、x64 と同様にプラットフォーム別フォルダ（`Win32\Release` / `Win32\Debug` / `Win32\Sample` 等）へ出力するように統一。
- **gamepad**: Release\|Win32 を `Win32\Release\`、Release\|x64 を `x64\Release\` に変更。
- **dll**: Release\|x64 の `OutDir` を `$(SolutionDir)x64\$(Configuration)\` に変更。nodoka.lib 参照を `..\nodoka\Win32\Release\` / `..\nodoka\Win32\Debug\` に更新。
- **exe.hil / exe.limit / nodoka_helper**: nodoka.lib 参照を `..\nodoka\Win32\Release\` / `..\nodoka\Win32\Debug\` に更新。
- **nodoka (exe)**: PostBuild の signtool パスおよび AdditionalLibraryDirectories を `Win32\Release` / `Win32\Debug` に更新。
- **distrib.mak**: TARGET_x86 を `..\Win32\Release\` / `..\Win32\Sample\` / `..\Win32\Debug\` に、DISTRIB_BIN を `..\Win32\Release\` に、DISTRIB_TS4NODOKA の Win32 DLL を `..\Win32\sts4nodoka\` 等に更新。

## 5. 注意事項

- **DISTRIB_BIN** の `..\dot.nodoka\nshell.exe` / `nshell64.exe` は dot.nodoka 側のビルド出力に依存します。必要に応じて各プロジェクトの OutDir を確認してください。
- **DISTRIB_SETTINGS** / **DISTRIB_MANUAL** / **DISTRIB_CONTRIBS** はソースまたは固定パスのため、ビルド出力の一致確認対象外です。
- インストーラー作成前に、対象構成（Release / Sample / Debug）でソリューションをビルドし、上記パスにファイルが存在することを確認することを推奨します。
