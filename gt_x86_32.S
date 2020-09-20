.intel_syntax noprefix

gt_thread_switch: .globl gt_thread_switch
    mov edx, [esp+4]
    mov eax, [esp+8]

    mov dword ptr [edx], ebx
    mov dword ptr [edx+4], esi
    mov dword ptr [edx+8], edi
    mov dword ptr [edx+12], ebp
    mov dword ptr [edx+16], esp

    mov esp, dword ptr [eax+16]
    mov ebp, dword ptr [eax+12]
    mov edi, dword ptr [eax+8]
    mov esi, dword ptr [eax+4]
    mov ebx, dword ptr [eax]

    ret


gt_die: .globl gt_die
    pop eax //other args are alerady on the stack in proper order
    call gt_do_return

