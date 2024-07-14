@echo off

rmdir build /S /Q
tools\luajit.exe metagen\metagen.lua clean