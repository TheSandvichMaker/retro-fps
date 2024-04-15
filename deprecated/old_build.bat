@echo off
if not exist build\build.exe call bootstrap.bat
build\build.exe %*
