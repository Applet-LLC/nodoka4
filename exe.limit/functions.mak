MAKEFUNC	= perl ..\tools\makefunc

functions.h:	..\nodoka\engine.h ..\tools\makefunc
		$(MAKEFUNC) < ..\nodoka\engine.h > ..\nodoka\functions.h
