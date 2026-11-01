@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul

rem The owner uses this PC while we work: builds use one compiler at a time and run at below normal priority,
rem so that all helpers together stay near 30 percent of the machine and the owner's programs come first.
set CMAKE_BUILD_PARALLEL_LEVEL=1
start "" /b /wait /belownormal cmd /c %*
