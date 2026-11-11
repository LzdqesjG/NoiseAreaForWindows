@echo off
setlocal
set VS=D:\Program Files\Microsoft Visual Studio\2022\Community
if not exist "%VS%\VC\Auxiliary\Build\vcvars64.bat" set VS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist build mkdir build
cl /nologo /std:c++17 /O2 /EHsc /W3 /MD /utf-8 /DNOMINMAX /DUNICODE /D_UNICODE ^
   /Fo:build\ /Fe:build\NoiseArea.exe ^
   src\main.cpp src\control.cpp src\noise.cpp src\overlay.cpp src\app.cpp src\lang.cpp ^
   /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comdlg32.lib comctl32.lib shell32.lib
if errorlevel 1 exit /b 1
echo BUILD_OK
