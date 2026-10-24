[bits 32]
[org 0x400000]
BSS_SIZE equ 1024

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
    mov eax,5
    mov ebx,win_title_np
    mov ecx,600
    mov edx,420
    int 0x80

    mov eax,1
    mov ebx,msg_np
    int 0x80

    mov eax,2
    int 0x80

    mov eax,0
    int 0x80
code_end:

data_start:
win_title_np db "记事本",0
msg_np db "[记事本] 编辑文本，保存为 .ztxt / .asm / .cfg",0
data_end:
