@echo off
setlocal

set SRC=..\x64\Release\PhotoCycle.scr
set DST1=PhotoCycle.scr
set DST2=%SystemRoot%\System32\PhotoCycle.scr

echo Copying to local build directory...
copy "%SRC%" "%DST1%" /Y

echo Copying to System32...
copy "%SRC%" "%DST2%" /Y
if errorlevel 1 (
	echo Failed to copy to System32. Opening local build folder...
	REM start "" explorer.exe "%SystemRoot%\System32"
	start taskmgr
)

echo Done.
endlocal
pause