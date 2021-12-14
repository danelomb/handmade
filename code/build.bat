@echo off

mkdir ..\..\build
pushd ..\..\build
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cl -Zi c:\work\handmade\code\win32_handmade.cpp user32.lib Gdi32.lib Xinput.lib
popd

