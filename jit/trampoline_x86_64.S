/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 kivitas                                             */
/* Licensed under the GNU GPL v3 or later; see the LICENSE file.           */
/*                                                                        */
/* kiv_jit_call(const void *entry) -> int64_t                             */
/*                                                                        */
/* Transfers control from the C++ VM into JIT-generated code and back.    */
/* It saves the callee-saved registers the System V ABI requires, aligns  */
/* the stack to 16 bytes at the call, and restores everything on return.  */
/* Generated code can therefore use any scratch register without          */
/* corrupting the interpreter's state. The result is returned in rax.     */
/*                                                                        */
/* Comments use C syntax on purpose: this file goes through the C         */
/* preprocessor, where a leading '#' would be read as a directive.        */

        .intel_syntax noprefix
        .text
        .globl  kiv_jit_call
        .type   kiv_jit_call, @function

kiv_jit_call:
        push    rbp
        push    rbx
        push    r12
        push    r13
        push    r14
        push    r15
        sub     rsp, 8              /* entry rsp is 8 mod 16; 6 pushes + 8 realign to 0 mod 16 */
        call    rdi                 /* rdi = generated code entry point */
        add     rsp, 8
        pop     r15
        pop     r14
        pop     r13
        pop     r12
        pop     rbx
        pop     rbp
        ret
        .size   kiv_jit_call, .-kiv_jit_call

        /* Mark the stack non-executable. */
        .section .note.GNU-stack,"",@progbits
