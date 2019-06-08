%set TARGET_DIR=Release
%set CC=gcc -Os -DUNICODE
%set LD=gcc -s -mwindows -municode
mkdir %TARGET_DIR%
windres -i src/myhotkey.rc -o %TARGET_DIR%/res.o
%CC% -c -o %TARGET_DIR%/myhotkey.o src/myhotkey.c
%CC% -c -o %TARGET_DIR%/syserr.o src/syserr.c
%LD% -o %TARGET_DIR%/myhotkey %TARGET_DIR%/myhotkey.o %TARGET_DIR%/syserr.o %TARGET_DIR%/res.o -luserenv -lole32
