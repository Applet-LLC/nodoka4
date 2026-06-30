# distribution	###############################################################

!if ( "$(_DEBUG)" == "" )
!if ( "$(RETAIL)" == "1" )
VERSION		= 4.31
TARGET_x86	= ..\Win32\Release\nodoka.exe					\
			..\Win32\Release\nodoka_hil.exe				\
			..\Win32\Release\nodoka_limit.exe				\
			..\Win32\Release\nodoka.dll					\
			..\sirius_sdk\sirius_hook_for_nodoka_x86.dll
TARGET_x64	= ..\x64\Release\nodoka64.exe					\
			..\x64\Release\nodoka64_nua.exe				\
			..\x64\Release\nodoka64_hil.exe				\
			..\x64\Release\nodoka64_limit.exe				\
			..\x64\Release\nodoka64.dll					\
			..\sirius_sdk\sirius_hook_for_nodoka_x64.dll
!else
VERSION		= 4.31_sample
TARGET_x86	= ..\Win32\Sample\nodoka.exe					\
			..\Win32\Sample\nodoka_hil.exe				\
			..\Win32\Sample\nodoka_limit.exe				\
			..\Win32\Release\nodoka.dll					\
			..\sirius_sdk\sirius_hook_for_nodoka_x86.dll
TARGET_x64	= ..\x64\Sample\nodoka64.exe					\
			..\x64\Sample\nodoka64_nua.exe				\
			..\x64\Sample\nodoka64_hil.exe				\
			..\x64\Sample\nodoka64_limit.exe				\
			..\x64\Release\nodoka64.dll					\
			..\sirius_sdk\sirius_hook_for_nodoka_x64.dll
!endif
!else
VERSION		= 4.31_debug
TARGET_x86	= ..\Win32\Debug\nodoka.exe						\
			..\Win32\Debug\nodoka_hil.exe					\
			..\Win32\Debug\nodoka_limit.exe				\
			..\Win32\Debug\nodoka.dll						\
			..\sirius_sdk\sirius_hook_for_nodoka_x86.dll
TARGET_x64	= ..\x64\Debug\nodoka64.exe						\
			..\x64\Debug\nodoka64_nua.exe					\
			..\x64\Debug\nodoka64_hil.exe					\
			..\x64\Debug\nodoka64_limit.exe				\
			..\x64\Debug\nodoka64.dll						\
			..\sirius_sdk\sirius_hook_for_nodoka_x64.dll
!endif

DISTRIB_BIN	=												\
		..\Win32\Release\setup.exe							\
		..\x64\Release\setup64.exe							\
		..\Win32\Release\nshell.exe							\
		..\x64\Release\nshell64.exe							\
		..\Win32\Release\gamepad.dll						\
		..\x64\Release\gamepad64.dll						\
		..\Win32\Release\nodoka_helper.exe					\
		..\distrib\files\GuiEdit.exe						\
		..\distrib\files\dotnet_starter.exe					\
		..\Win32\Release\DriverManager.exe					\
		..\x64\Release\DriverManager64.exe

DISTRIB_SETTINGS =											\
		..\dot.nodoka\104.nodoka							\
		..\dot.nodoka\104on109.nodoka						\
		..\dot.nodoka\109.nodoka							\
		..\dot.nodoka\109on104.nodoka						\
		..\dot.nodoka\default.nodoka						\
		..\dot.nodoka\default2.nodoka						\
		..\dot.nodoka\emacsedit.nodoka						\
		..\dot.nodoka\doten.nodoka							\
		..\dot.nodoka\dot.nodoka							\
		..\dot.nodoka\dotjp.nodoka							\
		..\dot.nodoka\read-keyboard-define.nodoka			\
		..\dot.nodoka\Shift-F2_toggle_US-JP-Keyboard.nodoka	\
		..\dot.nodoka\add-mouse-gamepad.nodoka				\
		..\gamepad\gamepad.nodoka							\
		..\gamepad\gamepad-mouse.nodoka						\
		..\gamepad\gamepad2-mouse.nodoka

DISTRIB_MANUAL	=											\
		..\doc\banner-ja.gif								\
		..\doc\CONTENTS-ja.html								\
		..\doc\CONTENTS-en.html								\
		..\doc\CUSTOMIZE-ja.html							\
		..\doc\CUSTOMIZE-en.html							\
		..\doc\edit-setting-ja.png							\
		..\doc\investigate-ja.png							\
		..\doc\log-ja.jpg									\
		..\doc\MANUAL-ja.html								\
		..\doc\MANUAL-en.html								\
		..\doc\menu-ja.png									\
		..\doc\pause-ja.png									\
		..\doc\README-ja.html								\
		..\doc\README-en.html								\
		..\doc\README.css									\
		..\doc\setting-ja.png								\
		..\doc\syntax.txt									\
		..\doc\target.png									\
		..\doc\version.jpg									\
		..\doc\tasktray-icon.png							\
		..\doc\copy-ja.png									\
		..\doc\virtualstore-ja.png							\
		..\doc\icon0.png									\
		..\doc\regedit.png									\
		..\doc\104.gif										\
		..\doc\109.gif										\
		..\doc\version86.jpg								\
		..\doc\tasktray-icon7.png							\
		..\doc\tasktray-icon7help.png						\
		..\doc\tasktray-icon7help2.png						\
		..\doc\tasktray-icon7help3.png						\
		..\doc\GuiEdit.png									\
		..\doc\setup0.jpg									\
		..\doc\setup1.jpg									\
		..\doc\setup3.jpg									\
		..\dot.nodoka\readme.txt							\
		..\dot.nodoka\readme-en.txt							\
		..\dot.nodoka\nshell.txt							\
		..\dot.nodoka\LICENSE.txt							\
		..\dot.nodoka\LICENSE_JP.txt						\
		..\dot.nodoka\nodoka-mode.el						\
		..\doc\GUIEdit-ja.html								\
		..\doc\gui-edit-main-describe.png					\
		..\doc\gui-edit-command-main-edited.png				\
		..\doc\gui-edit-command-wizard-other3.png			\
		..\doc\gui-edit-command-wizard-other2.png			\
		..\doc\gui-edit-command-wizard-other1.png			\
		..\doc\gui-edit-command-wizard-mod3.png				\
		..\doc\gui-edit-command-wizard-mod2.png				\
		..\doc\gui-edit-command-wizard-mod1.png				\
		..\doc\gui-edit-command-wizard-include2.png			\
		..\doc\gui-edit-command-wizard-include1.png			\
		..\doc\gui-edit-command-wizard-keymap3.png			\
		..\doc\gui-edit-command-wizard-keymap2.png			\
		..\doc\gui-edit-command-wizard-keymap1.png			\
		..\doc\gui-edit-command-wizard-3.png				\
		..\doc\gui-edit-command-wizard-2.png				\
		..\doc\gui-edit-command-wizard-1.png				\
		..\doc\gui-edit-start-new.png						\
		..\doc\gui-edit-main-loaded.png						\
		..\doc\gui-edit-right-click.png						\
		..\doc\gui-edit-setting1.png						\
		..\doc\gui-edit-setting2.png						\
		..\doc\copy-contrib.png								\
		..\doc\gui-edit-dot.nodoka.png						\
		..\doc\gui-edit-sample.nodoka.png					\
		..\doc\gui-edit-cursor.nodoka.png
		
DISTRIB_CONTRIBS =								\
		..\contrib\109onAX.nodoka				\
		..\contrib\nodoka-settings.txt			\
		..\contrib\dvorak.nodoka				\
		..\contrib\dvorak109.nodoka				\
		..\contrib\keitai.nodoka				\
		..\contrib\ax.nodoka					\
		..\contrib\98x1.nodoka					\
		..\contrib\DVORAKon109.nodoka			\
		..\contrib\sample.nodoka				\
		..\contrib\other.nodoka					\
		..\contrib\ime.nodoka					\
		..\contrib\cursor.nodoka				\
		..\contrib\no_badusb.nodoka				\

DISTRIB_TS4NODOKA =								\
		..\ts4nodoka\thumbsense.nodoka			\
		..\Win32\sts4nodoka\sts4nodoka.dll		\
		..\x64\sts4nodoka\sts4nodoka64.dll		\
		..\Win32\cts4nodoka\cts4nodoka.dll		\
#		..\x64\cts4nodoka\cts4nodoka64.dll		\
		..\Win32\ats4nodoka\ats4nodoka.dll		\
		..\x64\ats4nodoka\ats4nodoka64.dll		\
		

DISTRIB_DRIVER	=								\
		..\Win32\Release\d\nodokadx86.sys		\
		..\Win32\Release\d\nodokadx86.inf		\
		..\Win32\Release\d\nodokadx86.cat		\
		..\x64\Release\d.x64\nodokad.sys		\
		..\x64\Release\d.x64\nodokad.inf		\
		..\x64\Release\d.x64\nodokad.cat

DISTRIB_PDB		=								\
		..\Win32\Release\nodoka.dll.pdb			\
		..\Win32\Release\nodoka.exe.pdb			\
		..\x64\Release\nodoka64.dll.pdb			\
		..\x64\Release\nodoka64.exe.pdb			\
		..\Win32\Release\nodoka_helper.exe.pdb	\
		..\Win32\Release\nodokad.pdb			\
		..\x64\Release\nodokad.pdb

DISTRIB		=									\
		$(TARGET_x86)							\
		$(TARGET_x64)							\
		$(DISTRIB_BIN)							\
		$(DISTRIB_SETTINGS)						\
		$(DISTRIB_MANUAL)						\
		$(DISTRIB_CONTRIBS)						\
		$(DISTRIB_DRIVER)						\
		$(DISTRIB_PDB)							\
		$(DISTRIB_TS4NODOKA)


# tools		###############################################################
PERL		= C:\cygwin64\bin\perl
#IEXPRESS	= ..\tools\iexpress.exe
IEXPRESS	= ..\tools\nexpress\Win32\Release\nexpress.exe
DOCXX		= doc++.exe
MAKEDEPEND	= $(PERL) ..\tools\makedepend -o.obj
DOS2UNIX	= $(PERL) ..\tools\dos2unix
UNIX2DOS	= $(PERL) ..\tools\unix2dos
MAKEFUNC	= $(PERL) ..\tools\makefunc
GETCVSFILES	= $(PERL) ..\tools\getcvsfiles
GENIEXPRESS	= $(PERL) ..\tools\geniexpress
SIGNTOOL	= "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe"

SIGN_VERIFY	=											\
		..\Win32\Release\nodoka.exe						\
		..\Win32\Release\nodoka_hil.exe					\
		..\Win32\Release\nodoka_limit.exe				\
		..\Win32\Release\nodoka.dll						\
		..\x64\Release\nodoka64.exe						\
		..\x64\Release\nodoka64_nua.exe					\
		..\x64\Release\nodoka64_hil.exe					\
		..\x64\Release\nodoka64_limit.exe				\
		..\x64\Release\nodoka64.dll						\
		..\Win32\Sample\nodoka.exe						\
		..\Win32\Sample\nodoka_hil.exe					\
		..\Win32\Sample\nodoka_limit.exe				\
		..\x64\Sample\nodoka64.exe						\
		..\x64\Sample\nodoka64_nua.exe					\
		..\x64\Sample\nodoka64_hil.exe					\
		..\x64\Sample\nodoka64_limit.exe				\
		..\Win32\Release\setup.exe						\
		..\x64\Release\setup64.exe						\
		..\Win32\Release\nshell.exe						\
		..\x64\Release\nshell64.exe						\
		..\Win32\Release\gamepad.dll					\
		..\x64\Release\gamepad64.dll					\
		..\Win32\Release\nodoka_helper.exe				\
		..\distrib\files\GuiEdit.exe					\
		..\distrib\files\dotnet_starter.exe				\
		..\sirius_sdk\sirius_hook_for_nodoka_x86.dll	\
		..\sirius_sdk\sirius_hook_for_nodoka_x64.dll	\
		..\Win32\ats4nodoka\ats4nodoka.dll				\
		..\x64\ats4nodoka\ats4nodoka64.dll				\
		..\Win32\cts4nodoka\cts4nodoka.dll				\
		..\Win32\sts4nodoka\sts4nodoka.dll				\
		..\x64\sts4nodoka\sts4nodoka64.dll				\
		..\Win32\Release\d\nodokad.sys					\
		..\Win32\Release\d\nodokad.cat					\
		..\x64\Release\d.x64\nodokad.sys				\
		..\x64\Release\d.x64\nodokad.cat				\
		..\Win32\Release\DriverManager.exe				\
		..\x64\Release\DriverManager.exe


# rules		###############################################################

all:
		-@echo "we need cygwin tool"
		-$(SIGNTOOL) verify /pa /v $(SIGN_VERIFY)
		-cp -f ..\Win32\Release\d\nodokad.sys ..\Win32\Release\d\nodokadx86.sys
		-cp -f ..\Win32\Release\d\nodokad.inf ..\Win32\Release\d\nodokadx86.inf
		-cp -f ..\Win32\Release\d\nodokad.cat ..\Win32\Release\d\nodokadx86.cat
		-cp -f ..\x64\Release\DriverManager.exe ..\x64\Release\DriverManager64.exe
		-rm -f nodoka-$(VERSION) 
		-ln -s . nodoka-$(VERSION)
#		-bash -c "tar cvjf nodoka-$(VERSION)-src.tar.bz2 `$(GETCVSFILES) | sed 's/^./nodoka-$(VERSION)/'`"
		-rm -f nodoka-$(VERSION) 
		-$(GENIEXPRESS) \
			nodoka-$(VERSION)_setup.exe \
			"Nodoka $(VERSION)" \
			setup.exe $(DISTRIB) > __nodoka__.sed
#		-$(UNIX2DOS) $(DISTRIB_SETTINGS) $(DISTRIB_CONTRIBS)
		-$(IEXPRESS) /N __nodoka__.sed
#		-$(DOS2UNIX) $(DISTRIB_SETTINGS) $(DISTRIB_CONTRIBS)
#		-$(RM) __nodoka__.sed
		-$(SIGNTOOL) sign /v /a /n "Applet LLC" /tr http://timestamp.globalsign.com/tsa/r45standard /td sha256 /fd sha256 /ph nodoka-$(VERSION)_setup.exe
#		-$(SIGNTOOL) sign /v /a /n "Applet LLC" /fd sha256 nodoka-$(VERSION)_setup.exe
