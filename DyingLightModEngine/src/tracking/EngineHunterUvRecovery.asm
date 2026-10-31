OPTION CASEMAP:NONE

EXTERN EngineEntityHunterUvRecoveryStateCallback:PROC
EXTERN g_engine_entity_hunter_uv_recovery_start_trampoline:QWORD
EXTERN g_engine_entity_hunter_uv_recovery_complete_trampoline:QWORD
EXTERN g_engine_entity_hunter_uv_recovery_start_blocked_continuation:QWORD

PUBLIC EngineEntityHunterUvRecoveryStartStub
PUBLIC EngineEntityHunterUvRecoveryCompleteStub

.code

; 进入状态 9 时调用模组回调；返回 0 时跳过原始七字节状态写入。
EngineEntityHunterUvRecoveryStartStub PROC
	pushfq
	push rax
	push rcx
	push rdx
	push r8
	push r9
	push r10
	push r11
	push rbx
	push rdi
	mov r11, rsp
	and rsp, -16
	sub rsp, 90h
	mov qword ptr [rsp+80h], r11
	movdqu xmmword ptr [rsp+20h], xmm0
	movdqu xmmword ptr [rsp+30h], xmm1
	movdqu xmmword ptr [rsp+40h], xmm2
	movdqu xmmword ptr [rsp+50h], xmm3
	movdqu xmmword ptr [rsp+60h], xmm4
	movdqu xmmword ptr [rsp+70h], xmm5
	mov rcx, qword ptr [r11+8]
	mov edx, 9
	call EngineEntityHunterUvRecoveryStateCallback
	movdqu xmm0, xmmword ptr [rsp+20h]
	movdqu xmm1, xmmword ptr [rsp+30h]
	movdqu xmm2, xmmword ptr [rsp+40h]
	movdqu xmm3, xmmword ptr [rsp+50h]
	movdqu xmm4, xmmword ptr [rsp+60h]
	movdqu xmm5, xmmword ptr [rsp+70h]
	mov r11, qword ptr [rsp+80h]
	mov rsp, r11
	test al, al
	jz EngineEntityHunterUvRecoveryStartBlocked
	pop rdi
	pop rbx
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdx
	pop rcx
	pop rax
	popfq
	jmp qword ptr [g_engine_entity_hunter_uv_recovery_start_trampoline]
EngineEntityHunterUvRecoveryStartBlocked:
	pop rdi
	pop rbx
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdx
	pop rcx
	pop rax
	popfq
	jmp qword ptr [g_engine_entity_hunter_uv_recovery_start_blocked_continuation]
EngineEntityHunterUvRecoveryStartStub ENDP

; 状态 11 的完成点只通知模组并继续执行原始状态写入。
EngineEntityHunterUvRecoveryCompleteStub PROC
	pushfq
	push rax
	push rcx
	push rdx
	push r8
	push r9
	push r10
	push r11
	push rbx
	push rdi
	mov r11, rsp
	and rsp, -16
	sub rsp, 90h
	mov qword ptr [rsp+80h], r11
	movdqu xmmword ptr [rsp+20h], xmm0
	movdqu xmmword ptr [rsp+30h], xmm1
	movdqu xmmword ptr [rsp+40h], xmm2
	movdqu xmmword ptr [rsp+50h], xmm3
	movdqu xmmword ptr [rsp+60h], xmm4
	movdqu xmmword ptr [rsp+70h], xmm5
	mov rcx, qword ptr [r11]
	mov edx, 11
	call EngineEntityHunterUvRecoveryStateCallback
	movdqu xmm0, xmmword ptr [rsp+20h]
	movdqu xmm1, xmmword ptr [rsp+30h]
	movdqu xmm2, xmmword ptr [rsp+40h]
	movdqu xmm3, xmmword ptr [rsp+50h]
	movdqu xmm4, xmmword ptr [rsp+60h]
	movdqu xmm5, xmmword ptr [rsp+70h]
	mov r11, qword ptr [rsp+80h]
	mov rsp, r11
	pop rdi
	pop rbx
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdx
	pop rcx
	pop rax
	popfq
	jmp qword ptr [g_engine_entity_hunter_uv_recovery_complete_trampoline]
EngineEntityHunterUvRecoveryCompleteStub ENDP

END
