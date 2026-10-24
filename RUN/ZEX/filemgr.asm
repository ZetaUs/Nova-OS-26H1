[bits 64]
[org 0x400000]
BSS_SIZE  equ 1024

zex_header:
    dw 0x575A
    dw 0x0002
    dq entry
    dq code_end - code_start
    dq data_end - data_start
    dq BSS_SIZE

code_start:
entry:
    ; rax=5 创建GUI窗口
    mov rax, 5
    mov rdi, win_title_fm
    mov rsi, 520
    mov rdx, 360
    syscall

    ; rax=1 输出文字
    mov rax, 1
    mov rdi, msg_fm
    syscall

    ; rax=2 等待按键
    mov rax, 2
    syscall

    ; rax=0 退出
    mov rax, 0
    mov rdi, 0
    syscall
code_end:

data_start:
win_title_fm db "文件管理器",0
msg_fm       db "[文件管理器] 浏览、复制、删除文件",0
data_end:
