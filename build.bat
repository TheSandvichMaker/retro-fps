@echo off

if "%1"=="clean" rmdir build /S /Q

if not exist build mkdir build

set flags=/nologo /Z7 /WX /W4 /wd4201 /wd4115 /wd4013 /wd4116 /wd4324 /std:c11 /I..\src /I..\external\include /DUNICODE=1 /D_CRT_SECURE_NO_WARNINGS /DPLATFORM_WIN32=1
set debug_flags=/Od /MTd /DDREAM_SLOW=1
set release_flags=/O2 /MT
set linker_flags=/opt:ref /incremental:no
set libraries=user32.lib dxguid.lib d3d11.lib dxgi.lib d3dcompiler.lib gdi32.lib user32.lib ole32.lib ksuser.lib shell32.lib Synchronization.lib DbgHelp.lib d3d12.lib

rem copy garbage

robocopy external\bin run *.exe *.dll *.pdb /S > NUL

pushd build

if not exist dxc_wrapper_debug.obj (
	echo]
	echo building dxc wrapper... :(
	echo]
	cl /c ..\src\engine\rhi\d3d12\dxc.cpp /Fo:dxc_wrapper_debug.obj %flags% %debug_flags%
	cl /c ..\src\engine\rhi\d3d12\dxc.cpp /Fo:dxc_wrapper_release.obj %flags% %release_flags%
)

rem ========================================================================================================================

echo]
echo =========================
echo    RETRO - DEBUG BUILD
echo =========================
echo]

..\tools\ctime -begin win32_retro_debug.ctm

cl ..\src\engine\entry_win32.c dxc_wrapper_debug.obj /Fe:win32_retro_debug.exe %flags% %debug_flags% /link %linker_flags% %libraries%
set last_error=%ERRORLEVEL%

..\tools\ctime -end win32_retro_debug.ctm %last_error%

if %last_error% neq 0 goto bail

rem copy build artifacts
robocopy . ..\run *_debug.exe *_debug.dll *_debug.pdb /S > NUL

rem ========================================================================================================================

echo]
echo =========================
echo   RETRO - RELEASE BUILD
echo =========================
echo]

..\tools\ctime -begin win32_retro_release.ctm

cl ..\src\engine\entry_win32.c dxc_wrapper_release.obj /Fe:win32_retro_release.exe %flags% %release_flags% /link %linker_flags% %libraries%
set last_error=%ERRORLEVEL%

..\tools\ctime -end win32_retro_release.ctm %last_error%

if %last_error% neq 0 goto bail

rem copy build artifacts
robocopy . ..\run *_release.exe *_release.dll *_release.pdb /S > NUL

rem ========================================================================================================================

:bail

popd
