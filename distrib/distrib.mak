# distribution	###############################################################

!if ( "$(_DEBUG)" == "" )
!if ( "$(RETAIL)" == "1" )
VERSION		= 4.30
TARGET_x86	= ..\Release\nodoka.exe				\
			= ..\Release\nodoka_hil.exe		\
			= ..\Release\nodoka_limit.exe		\
			= ..\Release\nodoka.dll				\
			= ..\sirius_sdk\sirius_hook_for_nodoka_x86.dll
TARGET_x64	= ..\x64\Release\nodoka64.exe		\
			= ..\x64\Release\nodoka64_hil.exe	\
			= ..\x64\Release\nodoka64_limit.exe	\
			= ..\x64\Release\nodoka64.dll		\
			= ..\sirius_sdk\sirius_hook_for_nodoka_x64.dll
!else
VERSION		= 4.30_sample
TARGET_x86	= ..\Sample\nodoka.exe				\
			= ..\Sample\nodoka_hil.exe		\
			= ..\Sample\nodoka_limit.exe		\
			= ..\Release\nodoka.dll				\
			= ..\sirius_sdk\sirius_hook_for_nodoka_x86.dll
TARGET_x64	= ..\x64\Sample\nodoka64.exe		\
			= ..\x64\Sample\nodoka64_hil.exe	\
			= ..\x64\Sample\nodoka64_limit.exe	\
			= ..\x64\Release\nodoka64.dll		\
			= ..\sirius_sdk\sirius_hook_for_nodoka_x64.dll
!endif
!else
VERSION		= 4.30_debug
TARGET_x86	= ..\Debug\nodoka.exe				\
			= ..\Debug\nodoka_hil.exe			\
			= ..\Debug\nodoka_limit.exe			\
			= ..\Debug\nodoka.dll				\
			= ..\sirius_sdk\sirius_hook_for_nodoka_x86.dll
TARGET_x64	= ..\x64\Debug\nodoka64.exe			\
			= ..\x64\Debug\nodoka64_hil.exe		\
			= ..\x64\Debug\nodoka64_limit.exe	\
			= ..\x64\Debug\nodoka64.dll			\
			= ..\sirius_sdk\sirius_hook_for_nodoka_x64.dll
!endif

DISTRIB_BIN	=								\
		..\Release\setup.exe				\
		..\x64\Release\setup64.exe			\
		..\dot.nodoka\nshell.exe			\
		..\dot.nodoka\nshell64.exe			\
		..\gamepad\gamepad.dll				\
		..\gamepad\gamepad64.dll			\
		..\Release\nodoka_helper.exe		\
		..\Release\GuiEdit.exe				\
		..\Release\dotnet_starter.exe		\

DISTRIB_SETTINGS =							\
		..\dot.nodoka\104.nodoka			\
		..\dot.nodoka\104on109.nodoka		\
		..\dot.nodoka\109.nodoka			\
		..\dot.nodoka\109on104.nodoka		\
		..\dot.nodoka\default.nodoka		\
		..\dot.nodoka\default2.nodoka		\
		..\dot.nodoka\emacsedit.nodoka		\
		..\dot.nodoka\doten.nodoka			\
		..\dot.nodoka\dot.nodoka			\
		..\dot.nodoka\dotjp.nodoka			\
		..\dot.nodoka\read-keyboard-define.nodoka			\
		..\dot.nodoka\Shift-F2_toggle_US-JP-Keyboard.nodoka			\
		..\dot.nodoka\add-mouse-gamepad.nodoka	\
		..\gamepad\gamepad.nodoka			\
		..\gamepad\gamepad-mouse.nodoka		\
		..\gamepad\gamepad2-mouse.nodoka	\

DISTRIB_MANUAL	=							\
		..\doc\banner-ja.gif				\
		..\doc\CONTENTS-ja.html				\
		..\doc\CONTENTS-en.html				\
		..\doc\CUSTOMIZE-ja.html			\
		..\doc\CUSTOMIZE-en.html			\
		..\doc\edit-setting-ja.png			\
		..\doc\investigate-ja.png			\
		..\doc\log-ja.jpg					\
		..\doc\MANUAL-ja.html				\
		..\doc\MANUAL-en.html				\
		..\doc\menu-ja.png					\
		..\doc\pause-ja.png					\
		..\doc\README-ja.html				\
		..\doc\README-en.html				\
		..\doc\README.css					\
		..\doc\setting-ja.png				\
		..\doc\syntax.txt					\
		..\doc\target.png					\
		..\doc\version.jpg					\
		..\doc\tasktray-icon.png			\
		..\doc\copy-ja.png					\
		..\doc\virtualstore-ja.png			\
		..\doc\icon0.png					\
		..\doc\regedit.png					\
		..\doc\104.gif						\
		..\doc\109.gif						\
		..\doc\version86.jpg				\
		..\doc\tasktray-icon7.png			\
		..\doc\tasktray-icon7help.png		\
		..\doc\tasktray-icon7help2.png		\
		..\doc\tasktray-icon7help3.png		\
		..\doc\GuiEdit.png					\
		..\doc\setup0.jpg					\
		..\doc\setup1.jpg					\
		..\doc\setup3.jpg					\
		..\dot.nodoka\readme.txt			\
		..\dot.nodoka\readme-en.txt			\
		..\dot.nodoka\nshell.txt			\
		..\dot.nodoka\Common_Public_License_1_0.txt	\
		..\dot.nodoka\Common_Public_License_1_0_JP.txt	\
		..\dot.nodoka\nodoka-mode.el		\
		..\doc\GUIEdit-ja.html				\
		..\doc\gui-edit-main-describe.png	\
		..\doc\gui-edit-command-main-edited.png	\
		..\doc\gui-edit-command-wizard-other3.png	\
		..\doc\gui-edit-command-wizard-other2.png	\
		..\doc\gui-edit-command-wizard-other1.png	\
		..\doc\gui-edit-command-wizard-mod3.png	\
		..\doc\gui-edit-command-wizard-mod2.png	\
		..\doc\gui-edit-command-wizard-mod1.png	\
		..\doc\gui-edit-command-wizard-include2.png	\
		..\doc\gui-edit-command-wizard-include1.png	\
		..\doc\gui-edit-command-wizard-keymap3.png	\
		..\doc\gui-edit-command-wizard-keymap2.png	\
		..\doc\gui-edit-command-wizard-keymap1.png	\
		..\doc\gui-edit-command-wizard-3.png	\
		..\doc\gui-edit-command-wizard-2.png	\
		..\doc\gui-edit-command-wizard-1.png	\
		..\doc\gui-edit-start-new.png	\
		..\doc\gui-edit-main-loaded.png	\
		..\doc\gui-edit-right-click.png	\
		..\doc\gui-edit-setting1.png	\
		..\doc\gui-edit-setting2.png	\
		..\doc\copy-contrib.png			\
		..\doc\gui-edit-dot.nodoka.png	\
		..\doc\gui-edit-sample.nodoka.png	\
		..\doc\gui-edit-cursor.nodoka.png	\
		
DISTRIB_CONTRIBS =							\
		..\contrib\109onAX.nodoka			\
		..\contrib\nodoka-settings.txt		\
		..\contrib\dvorak.nodoka			\
		..\contrib\dvorak109.nodoka			\
		..\contrib\keitai.nodoka			\
		..\contrib\ax.nodoka				\
		..\contrib\98x1.nodoka				\
		..\contrib\DVORAKon109.nodoka		\
		..\contrib\sample.nodoka			\
		..\contrib\other.nodoka				\
		..\contrib\ime.nodoka				\
		..\contrib\cursor.nodoka			\
		..\contrib\no_badusb.nodoka			\

DISTRIB_TS4NODOKA =							\
		..\ts4nodoka\thumbsense.nodoka		\
		..\ts4nodoka\sts4nodoka.dll			\
		..\ts4nodoka\cts4nodoka.dll			\
		..\ts4nodoka\ats4nodoka.dll			\
		..\ts4nodoka\ats4nodoka64.dll		\
		..\ts4nodoka\sts4nodoka64.dll		\
		

DISTRIB_DRIVER	=							\
		..\d\i386\nodokad.sys				\
		..\d\amd64\nodokadx64.sys			\
		..\d.rescue\i386\nodokadrsc.sys		\
		..\d.rescue\amd64\nodokadx64rsc.sys	\

DISTRIB		=								\
		$(TARGET_x86)						\
		$(TARGET_x64)						\
		$(DISTRIB_BIN)						\
		$(DISTRIB_SETTINGS)					\
		$(DISTRIB_MANUAL)					\
		$(DISTRIB_CONTRIBS)					\
#		$(DISTRIB_DRIVER)					\
		$(DISTRIB_TS4NODOKA)				\


# tools		###############################################################

IEXPRESS	= ..\tools\iexpress.exe
DOCXX		= doc++.exe
MAKEDEPEND	= perl ..\tools\makedepend -o.obj
DOS2UNIX	= perl ..\tools\dos2unix
UNIX2DOS	= perl ..\tools\unix2dos
MAKEFUNC	= perl ..\tools\makefunc
GETCVSFILES	= perl ..\tools\getcvsfiles
GENIEXPRESS	= perl ..\tools\geniexpress
SIGNTOOL	= "c:\Program Files (x86)\Windows Kits\8.1\bin\x64\signtool.exe"


# rules		###############################################################

all:
		-@echo "we need cygwin tool"
		-rm -f nodoka-$(VERSION) 
		-ln -s . nodoka-$(VERSION)
#		-bash -c "tar cvjf nodoka-$(VERSION)-src.tar.bz2 `$(GETCVSFILES) | sed 's/^./nodoka-$(VERSION)/'`"
		-rm -f nodoka-$(VERSION) 
		-$(GENIEXPRESS) \
			nodoka-$(VERSION)$(SAMPLE_NAME)_setup.exe \
			"Nodoka $(VERSION)" \
			setup.exe $(DISTRIB) > __nodoka__.sed
#		-$(UNIX2DOS) $(DISTRIB_SETTINGS) $(DISTRIB_CONTRIBS)
		-$(IEXPRESS) /N __nodoka__.sed
#		-$(DOS2UNIX) $(DISTRIB_SETTINGS) $(DISTRIB_CONTRIBS)
#		-$(RM) __nodoka__.sed
#		-$(SIGNTOOL) sign /v /ac "d:\nodoka\GlobalSign Root CA.crt" /s my /n "G.K.Applet"  nodoka-$(VERSION)$(SAMPLE_NAME)_setup.exe
		-$(SIGNTOOL) sign /v /ac "d:\nodoka\DigiCertHighAssuranceEVRootCA.crt" /s my /n "Applet LLC" /tr http://timestamp.digicert.com /fd sha256 /td sha256 nodoka-$(VERSION)$(SAMPLE_NAME)_setup.exe
