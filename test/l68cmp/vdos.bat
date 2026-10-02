@echo off
rem Build prog.c with Microware C 3.2 under vDos, for run.sh (MWBUILT=
rem this directory).  Needs C:\CDI\OS9C (BIN, DEFS, LIB) as set up in the
rem vDos autoexec (CDEF, CLIB).
set PATH=%PATH%;C:\CDI\OS9C\BIN
if exist prog.r del prog.r
if exist prog del prog
if exist prog.stb del prog.stb
if exist stb\prog.stb del stb\prog.stb
xcc -r -s prog.c
l68 -g -m -n=prog -o=prog %CLIB%\cstart.r prog.r -l=%CLIB%\clib.l -l=%CLIB%\sys.l >prog.map
if exist stb\prog.stb copy stb\prog.stb prog.stb >nul
rem The round trip (run.sh copies these ROFs here): link them too.
if exist prog2 del prog2
if exist clib2.l l68 -n=prog -o=prog2 cstart2.r prog2.r -l=clib2.l -l=%CLIB%\sys.l
dir prog*
