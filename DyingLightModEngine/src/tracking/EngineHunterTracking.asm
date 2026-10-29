OPTION CASEMAP:NONE

EXTERN EngineEntity_OnHunterPosition:PROC
EXTERN EngineEntity_OnHunterMovement:PROC
EXTERN g_engine_entity_hunter_position_trampoline:QWORD
EXTERN g_engine_entity_hunter_movement_trampoline:QWORD

PUBLIC EngineEntityHunterPositionStub
PUBLIC EngineEntityHunterMovementStub

.code

EngineEntityHunterPositionStub PROC
	movss dword ptr [rdi+7F4h], xmm1
	pushfq
	push rax
	push rcx
	push rdx
	push r8
	push r9
	push r10
	push r11
	push rdi
	push rbx

	mov rbx, rsp
	and rsp, -16
	sub rsp, 80h
	movdqu xmmword ptr [rsp+20h], xmm0
	movdqu xmmword ptr [rsp+30h], xmm1
	movdqu xmmword ptr [rsp+40h], xmm2
	movdqu xmmword ptr [rsp+50h], xmm3
	movdqu xmmword ptr [rsp+60h], xmm4
	movdqu xmmword ptr [rsp+70h], xmm5
	mov rcx, qword ptr [rbx+8]
	call EngineEntity_OnHunterPosition
	movdqu xmm0, xmmword ptr [rsp+20h]
	movdqu xmm1, xmmword ptr [rsp+30h]
	movdqu xmm2, xmmword ptr [rsp+40h]
	movdqu xmm3, xmmword ptr [rsp+50h]
	movdqu xmm4, xmmword ptr [rsp+60h]
	movdqu xmm5, xmmword ptr [rsp+70h]
	mov rsp, rbx

	pop rbx
	pop rdi
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdx
	pop rcx
	pop rax
	popfq
	jmp qword ptr [g_engine_entity_hunter_position_trampoline]
EngineEntityHunterPositionStub ENDP

EngineEntityHunterMovementStub PROC
	movss dword ptr [rbx+1A0h], xmm2
	pushfq
	push rax
	push rcx
	push rdx
	push r8
	push r9
	push r10
	push r11
	push rdi
	push rbx

	mov rbx, rsp
	and rsp, -16
	sub rsp, 80h
	movdqu xmmword ptr [rsp+20h], xmm0
	movdqu xmmword ptr [rsp+30h], xmm1
	movdqu xmmword ptr [rsp+40h], xmm2
	movdqu xmmword ptr [rsp+50h], xmm3
	movdqu xmmword ptr [rsp+60h], xmm4
	movdqu xmmword ptr [rsp+70h], xmm5
	mov rcx, qword ptr [rbx]
	call EngineEntity_OnHunterMovement
	movdqu xmm0, xmmword ptr [rsp+20h]
	movdqu xmm1, xmmword ptr [rsp+30h]
	movdqu xmm2, xmmword ptr [rsp+40h]
	movdqu xmm3, xmmword ptr [rsp+50h]
	movdqu xmm4, xmmword ptr [rsp+60h]
	movdqu xmm5, xmmword ptr [rsp+70h]
	mov rsp, rbx

	pop rbx
	pop rdi
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdx
	pop rcx
	pop rax
	popfq
	jmp qword ptr [g_engine_entity_hunter_movement_trampoline]
EngineEntityHunterMovementStub ENDP

END
