# digital sign to all bin	###############################################################

# User-mode binaries: SigRemover applied before re-signing.
# Note: cts4nodoka64.dll is not built (x64 Cirque SDK unavailable), so excluded.
TARGET_BIN	= ..\Win32\Release\nodoka.exe		\
		..\Win32\Release\nodoka_hil.exe		\
		..\Win32\Release\nodoka_limit.exe	\
		..\Win32\Release\nodoka.dll			\
		..\x64\Release\nodoka64.exe			\
		..\x64\Release\nodoka64_nua.exe		\
		..\x64\Release\nodoka64_hil.exe		\
		..\x64\Release\nodoka64_limit.exe	\
		..\x64\Release\nodoka64.dll			\
		..\Win32\Sample\nodoka.exe			\
		..\Win32\Sample\nodoka_hil.exe		\
		..\Win32\Sample\nodoka_limit.exe	\
		..\x64\Sample\nodoka64.exe			\
		..\x64\Sample\nodoka64_nua.exe		\
		..\x64\Sample\nodoka64_hil.exe		\
		..\x64\Sample\nodoka64_limit.exe	\
		..\Win32\Release\setup.exe			\
		..\x64\Release\setup64.exe			\
		..\Win32\Release\nshell.exe			\
		..\x64\Release\nshell64.exe			\
		..\Win32\Release\gamepad.dll		\
		..\x64\Release\gamepad64.dll		\
		..\Win32\Release\nodoka_helper.exe	\
		..\distrib\files\GuiEdit.exe		\
		..\distrib\files\dotnet_starter.exe	\
		..\sirius_sdk\sirius_hook_for_nodoka_x86.dll	\
		..\sirius_sdk\sirius_hook_for_nodoka_x64.dll	\
		..\Win32\ats4nodoka\ats4nodoka.dll	\
		..\x64\ats4nodoka\ats4nodoka64.dll	\
		..\Win32\cts4nodoka\cts4nodoka.dll	\
		..\Win32\sts4nodoka\sts4nodoka.dll	\
		..\x64\sts4nodoka\sts4nodoka64.dll	\
		..\Win32\Release\DriverManager.exe	\
		..\x64\Release\DriverManager.exe	\
		..\tools\nexpress\Win32\Release\nexpress.exe

# Driver files: sign directly, no SigRemover.
# (inf2cat runs in d/d.x64 WDK build; .cat already has correct hashes)
TARGET_SYS	= ..\Win32\Release\d\nodokad.sys	\
		..\Win32\Release\d\nodokad.cat		\
		..\x64\Release\d.x64\nodokad.sys	\
		..\x64\Release\d.x64\nodokad.cat



# tools		###############################################################

SIGNTOOL	= "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe"
SIGREMOVER	= "C:\Users\applet\Documents\GitHub\nodoka\nodoka\tools\SigRemover.exe"

# rules		###############################################################

# "all" must be the first target so nmake builds it by default
all: remove_sig
		@echo [d.sign] signing start
#		-$(SIGNTOOL) sign /v /a /n "Applet LLC" /tr http://timestamp.globalsign.com/tsa/r45standard /td sha256 /fd sha256 /ph $(TARGET_BIN) $(TARGET_SYS)
		-$(SIGNTOOL) sign /v /a /n "Applet LLC" /tr http://timestamp.globalsign.com/tsa/r45standard /td sha256 /fd sha256 /ph $(TARGET_BIN)
		@echo [d.sign] signing done

# Remove existing signatures from user-mode binaries.
# Three separate for-loops avoid cmd.exe misinterpreting ")" in "(NoSig)"
# as the closing paren of a do-block (a known cmd.exe parser quirk).
#   Loop 1: run SigRemover on each file  -> creates "foo (NoSig).ext"
#   Loop 2: delete original where NoSig copy exists
#   Loop 3: rename NoSig copy back to original name
remove_sig:
		@echo [d.sign] remove_sig start
		-for %%f in ($(TARGET_BIN)) do $(SIGREMOVER) -i "%%f"
		-for %%f in ($(TARGET_BIN)) do if exist "%%~dpnf (NoSig)%%~xf" del "%%f"
		-for %%f in ($(TARGET_BIN)) do if exist "%%~dpnf (NoSig)%%~xf" move "%%~dpnf (NoSig)%%~xf" "%%~dpnf%%~xf"
		@echo [d.sign] remove_sig done
