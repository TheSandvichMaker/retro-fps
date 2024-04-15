@echo off

if not exist build mkdir build

echo]

set flags=/nologo /Zi /WX /W4 /wd4201 /std:c11 /I..\src /I..\external\include

pushd build
cl /Fe:build.exe %flags% ..\src\buildtool\main.c
popd
