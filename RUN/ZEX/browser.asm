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
    mov ebx,win_title_br
    mov ecx,640
    mov edx,400
    int 0x80

    mov eax,1
    mov ebx,msg_br
    int 0x80

    mov eax,2
    int 0x80

    mov eax,0
    int 0x80
code_end:

data_start:
win_title_br db "浏览器",0
msg_br db "[浏览器] 访问网页资源",0
data_end:
