@echo off


mkdir ..\..\build
pushd ..\..\build
cl -Zi w:\code\win32_handmade.cpp
popd
