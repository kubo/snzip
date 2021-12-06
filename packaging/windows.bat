@echo off
setlocal enableextensions 
for /F "tokens=3" %%F in ('findstr /c:"project(snzip" CMakeLists.txt') do (set VER=%%F)

call :mkpkg 32 Win32
call :mkpkg 64 x64
exit /b

:mkpkg
cmake -G "Visual Studio 16 2019" -A %2 -S . -B build%1
cmake --build build%1 --config Release

mkdir snzip-%VER%-win%1
copy build%1\Release\snzip.exe snzip-%VER%-win%1\snzip.exe
copy build%1\Release\snzip.exe snzip-%VER%-win%1\snunzip.exe
zip -r snzip-%VER%-win%1.zip snzip-%VER%-win%1
