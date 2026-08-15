@echo off
REM SxS (Side-by-Side) エラー原因特定用スクリプト
REM 管理者として実行し、手順に従って nodoka.exe を実行してエラーを再現してください。
setlocal
set LOGDIR=%~dp0..\sxs_trace_log
if not exist "%LOGDIR%" mkdir "%LOGDIR%"
cd /d "%LOGDIR%"

echo.
echo === SxS トレース手順 ===
echo 1. このウィンドウで以下のコマンドを実行します（トレース開始）
echo 2. 別のウィンドウまたはエクスプローラーから D:\nodoka\Release\nodoka.exe を実行し、SxS エラーを再現してください
echo 3. エラーが出たら、このウィンドウに戻り Enter キーを押してトレースを停止します
echo 4. 変換コマンドで sxs_trace.txt が生成されます。ERROR や resolution の行を確認してください
echo.

set "KITBIN=C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x86"
if not exist "%KITBIN%\mt.exe" set "KITBIN=C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x86"
if not exist "%KITBIN%\mt.exe" set "KITBIN="

echo トレースを開始します。nodoka.exe を実行してエラーを再現したら、このウィンドウで Enter を押してください。
sxstrace trace -logfile:sxs_trace.etl
if errorlevel 1 (
  echo sxstrace が失敗しました。管理者として実行しているか確認してください。
  goto :eof
)

echo.
echo トレースをパースしてテキストに変換しています...
sxstrace parse -logfile:sxs_trace.etl -outfile:sxs_trace.txt
if errorlevel 1 (
  echo sxstrace parse が失敗しました。
  goto :eof
)

echo.
echo 完了: %LOGDIR%\sxs_trace.txt を開き、ERROR や "resolution failed" を検索して不足しているアセンブリを確認してください。
start notepad "%LOGDIR%\sxs_trace.txt"
endlocal
:eof
