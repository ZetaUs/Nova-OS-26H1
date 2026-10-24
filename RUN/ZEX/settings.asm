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
    mov eax,5
    mov ebx,win_title_st
    mov ecx,480
    mov edx,340
    int 0x80

    mov eax,1
    mov ebx,msg_st
    int 0x80

    mov eax,2
    int 0x80

    mov eax,0
    int 0x80
code_end:

data_start:
win_title_st db "设置",0
msg_st db "[设置] 调整系统参数、外观、驱动",0
data_end:
