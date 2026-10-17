@echo off
setlocal
call "%~dp0build.bat"
if errorlevel 1 exit /b 1

"%~dp0..\Program\qemu\qemu-system-x86_64.exe" -drive file="%~dp0build\nova-os.img",format=raw -m 128M -boot c -vga std -no-reboot -no-shutdown
