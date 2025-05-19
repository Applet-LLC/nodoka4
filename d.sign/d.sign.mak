# digital sign to all bin	###############################################################

TARGET	= ..\Release\nodoka.exe				\
		..\Release\nodoka_hil.exe			\
		..\Release\nodoka_limit.exe			\
		..\x64\Release\nodoka64.exe			\
		..\x64\Release\nodoka64_hil.exe		\
		..\x64\Release\nodoka64_limit.exe	\
		..\Sample\nodoka.exe				\
		..\Sample\nodoka_hil.exe			\
		..\Sample\nodoka_limit.exe			\
		..\x64\Sample\nodoka64.exe			\
		..\x64\Sample\nodoka64_hil.exe		\
		..\x64\Sample\nodoka64_limit.exe	\
		..\Release\setup.exe				\
		..\x64\Release\setup64.exe			\
		..\dot.nodoka\nshell.exe			\
		..\dot.nodoka\nshell64.exe			\
		..\Release\nodoka_helper.exe		\
		..\Release\GuiEdit.exe				\
		..\Release\dotnet_starter.exe		
#d:\nodoka\d.rescue\i386\nodokadrsc.sys	\
#d:\nodoka\d.rescue\amd64\nodokadx64rsc.sys	\
#d:\nodoka\d\i386\nodokad.sys		\
#d:\nodoka\d\amd64\nodokadx64.sys



# tools		###############################################################

SIGNTOOL	= "c:\Program Files (x86)\Windows Kits\10\bin\x64\signtool.exe"

# rules		###############################################################

all:
		-$(SIGNTOOL) sign /v /ac "d:\nodoka\DigiCertHighAssuranceEVRootCA.crt" /s my /n "Applet LLC" /tr http://timestamp.digicert.com /fd sha256 /td sha256 $(TARGET)
