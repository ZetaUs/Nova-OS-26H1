[bits 32]
[org 0x400000]
BSS_SIZE equ 512

zex_header:
    dw 0x575A
    dw 0x0001
    dd entry
    dd code_end - code_start
    dd data_end - data_start
    dd BSS_SIZE
    dd 0

code_start:
entry:
    ; 打开GUI窗口
    mov eax,5
    mov ebx,win_title_fm
    mov ecx,520
    mov edx,360
    int 0x80

    ; 提示文字
    mov eax,1
    mov ebx,msg_fm
    int 0x80

    mov eax,2
    int 0x80

    mov eax,0
    int 0x80
code_end:

data_start:
win_title_fm db "文件管理器",0
msg_fm db "[文件管理器] 浏览、复制、删除文件",0
data_end:
