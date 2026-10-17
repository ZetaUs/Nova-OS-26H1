# Nova 26H2

Nova 26H2 is a small BIOS-bootable x86-64 OS experiment. It reuses the Nova identity and high-resolution desktop direction of the `Try 1\os` project, while replacing its 32-bit protected-mode kernel with a native 64-bit freestanding C++ kernel.

## Build

From the workspace root, run:

```powershell
.\os\build.bat
```

The build uses the Dev-C++ MinGW-w64 compiler (`-m64`), the bundled NASM assembler, and Python 3 to embed the Chinese glyphs used by the desktop and menu. It produces `os\build\nova-os-26h2.img` so that an older running QEMU instance can keep using the previous disk image. If Dev-C++ is installed elsewhere, set `DEVCPP_HOME` to its installation directory before building.

The BIOS loader requests a VBE linear framebuffer, enables the QEMU/Bochs VBE 2560x1440x32 mode, builds identity-mapped 2 MiB page tables for the first 4 GiB, and enters x86-64 long mode. Desktop and loading frames are drawn to a back buffer before being presented, preventing partially rendered screen transitions. The kernel is deliberately freestanding and does not depend on a hosted C++ runtime.

## Run

```powershell
.\os\run.bat
```

This builds the image and launches it in the bundled QEMU standard VGA device. A Nova loading screen appears before the 2560x1440 desktop. The desktop displays a software mouse pointer in the center of the screen. The desktop, taskbar, and start menu use Chinese labels. Press the **Windows key** or click the Nova button at the bottom left to open or close the start menu. The menu provides **重启** and **关机** actions. Press **Esc** to close an open menu.

## Image layout

| Disk sectors | Contents |
| --- | --- |
| 0 | BIOS boot sector |
| 1-8 | Real-mode VBE and x86-64 loader |
| 9-72 | 64-bit kernel image |
