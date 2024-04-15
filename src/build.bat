@echo off

rem The only reason why this exists is because
rem I want to run build.bat from vim while vim
rem is ran from the src directory

pushd ..

build.bat

popd
