strres.h:	setup.rc
	grep IDS setup.rc | \
	sed "s/\(IDS[a-zA-Z0-9_]*\)[^""]*\("".*\)$$/\1, _T(\2),/" | \
	sed "s/""""/"") _T(""/g" > strres.h
