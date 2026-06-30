MAKEFUNC	= C:\cygwin64\bin\perl ..\tools\makefunc

functions.h:	..\nodoka\engine.h ..\tools\makefunc
		$(MAKEFUNC) < ..\nodoka\engine.h > functions.h

clean:
	if exist functions.h del functions.h
