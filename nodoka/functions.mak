MAKEFUNC	= perl ..\tools\makefunc

functions.h:	engine.h ..\tools\makefunc
		$(MAKEFUNC) < engine.h > functions.h
