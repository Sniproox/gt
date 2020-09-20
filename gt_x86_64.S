.intel_syntax noprefix

gt_thread_switch: .globl gt_thread_switch
    mov qword ptr [rdi], rsp
    mov qword ptr [rdi+8], r15
    mov qword ptr [rdi+16], r14
    mov qword ptr [rdi+24], r13
    mov qword ptr [rdi+32], r12
    mov qword ptr [rdi+40], rbx
    mov qword ptr [rdi+48], rbp

    mov rbp, qword ptr [rsi+48]
    mov rbx, qword ptr [rsi+40]
    mov r12, qword ptr [rsi+32]
    mov r13, qword ptr [rsi+24]
    mov r14, qword ptr [rsi+16]
    mov r15, qword ptr [rsi+8]
    mov rsp, qword ptr [rsi]
    ret

gt_die: .globl gt_die
    pop rsi
    pop rdi
    call gt_do_return

gt_start_thread: .globl gt_start_thread
    pop r8
    pop rdi

    mov r9, qword ptr [rdi]
    mov byte ptr [r9 + 56], 1

    mov rsi, qword ptr [rdi + 16]
    call r8
    ret
    
