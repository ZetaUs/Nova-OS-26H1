@echo off
setlocal
call "%~dp0build.bat"
if errorlevel 1 exit /b 1

"%~dp0..\Program\qemu\qemu-system-x86_64.exe" -drive file="%~dp0build\nova-os-26h2.img",format=raw -m 128M -boot c -vga std -device isa-debug-exit,iobase=0xf4,iosize=0x04
