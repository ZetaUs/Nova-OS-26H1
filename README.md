# Nova OS 26H2

Nova OS 26H2 is a small BIOS-bootable x86-64 OS experiment. It reuses the Nova identity and high-resolution desktop direction of the `Try 1\os` project, while replacing its 32-bit protected-mode kernel with a native 64-bit freestanding C++ kernel.

## Build

From the workspace root, run:

```powershell
.\os\build.bat
```

The build uses the Dev-C++ MinGW-w64 compiler (`-m64`), the bundled NASM assembler, and Python 3 to embed the Chinese glyphs and ASC48 English glyphs used by the desktop and menu. It produces `os\build\nova-os-26h2.img` so that an older running QEMU instance can keep using the previous disk image. If Dev-C++ is installed elsewhere, set `DEVCPP_HOME` to its installation directory before building.

The BIOS loader requests a VBE linear framebuffer, enables the QEMU/Bochs VBE 1920x1080x32 mode, builds identity-mapped 2 MiB page tables for the first 4 GiB, and enters x86-64 long mode. The full-HD mode reduces software-rendering and framebuffer-copy work. Desktop and loading frames are drawn to a back buffer before being presented, preventing partially rendered screen transitions. The build downsamples the source PNG into a 192x192 antialiased logo mask and embeds it with run-length encoding. The desktop wallpaper is generated from `image\Wallpaper\img0.png`, stored as a compact RGB565 image in the disk image, and loaded by the BIOS stage before entering long mode. Replace that PNG to change the default wallpaper; other source wallpapers can remain in the same folder. Desktop app icons follow the Explorer, Notepad, Browser, Settings, and Terminal layout from the Nova reference site. The software mouse pointer uses the antialiased image in `image\mouse.png` and retains one-pixel movement precision. ASCII text uses coverage-based antialiasing to retain detail when scaled, and desktop icon labels are centered beneath their icons. Shutdown first exits QEMU through its debug-exit device, then requests ACPI S5 on emulators without that device, avoiding a CPU halt before the exit request. Back-buffer drawing uses normal memory writes, and each row is copied to the framebuffer with `REP MOVSD` for faster full-screen presentation. The kernel is deliberately freestanding and does not depend on a hosted C++ runtime.

## Run

```powershell
.\os\run.bat
```

This builds the image and launches it in the bundled QEMU standard VGA device. A Nova loading screen appears before the 1920x1080 desktop. The desktop displays a software mouse pointer in the center of the screen. The desktop, taskbar, and start menu use Chinese labels. Press the **Windows key** or click the Nova button at the bottom left to open or close the start menu. In the open menu, use **Up/Down** to select **重启** or **关机**, then press **Enter**; these actions are also available by mouse. Press **Esc** to close an open menu. Selecting **关机** powers off the guest and exits QEMU.

## Image layout

| Disk sectors | Contents |
| --- | --- |
| 0 | BIOS boot sector |
| 1-8 | Real-mode VBE and x86-64 loader |
| 9-104 | 64-bit kernel image |
| 105-611 | 480x270 RGB565 Nova wallpaper |
