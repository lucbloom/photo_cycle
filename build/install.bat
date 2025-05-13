@echo off
setlocal

set SRC=PhotoCycle.scr
set DST=%SystemRoot%\System32\PhotoCycle.scr

echo Copying to System32...
copy "%SRC%" "%DST%" /Y
if errorlevel 1 (
	echo Failed to copy to System32.
)

echo Done.
endlocal
pause