@echo off


mkdir ..\..\build
pushd ..\..\build
cl -Zi w:\code\win32_handmade.cpp user32.lib Gdi32.lib Xinput.lib
popd

