OPTION CASEMAP:NONE

EXTERN EngineEntity_OnDamageEntry:PROC
EXTERN EngineEntity_OnDamageReturn:PROC
EXTERN g_engine_entity_damage_outer_trampoline:QWORD
EXTERN g_engine_entity_damage_base_trampoline:QWORD

PUBLIC EngineEntityDamageOuterStub
PUBLIC EngineEntityDamageBaseStub

.code

; 受伤入口桩。调用约定与原版一致：
;   rcx = CHealthModule，rdx = 造成伤害的实体（可为 0），xmm2 = 伤害值，
;   [rsp+28h] = 伤害类型。
; 回调返回 64 位值：低 32 位为改写后的伤害位，bit32 置位表示允许继续结算。
; 允许时以函数方式调用跳板（原版序言），返回后再触发受伤后回调；
; 阻止时直接把返回值置 0 并返回，等同于本次伤害从未发生。
EngineEntityDamageOuterStub PROC
	push rbp
	mov rbp, rsp
	sub rsp, 100h

	mov qword ptr [rsp+60h], rcx
	mov qword ptr [rsp+68h], rdx
	mov qword ptr [rsp+70h], r8
	mov qword ptr [rsp+78h], r9
	movdqu xmmword ptr [rsp+80h], xmm0
	movdqu xmmword ptr [rsp+90h], xmm1
	movdqu xmmword ptr [rsp+0A0h], xmm2
	movdqu xmmword ptr [rsp+0B0h], xmm3

	mov rcx, qword ptr [rsp+60h]
	mov rdx, qword ptr [rsp+68h]
	movd r8d, xmm2
	mov r9d, dword ptr [rbp+30h]
	mov eax, dword ptr [rbp+60h]
	mov dword ptr [rsp+20h], eax
	call EngineEntity_OnDamageEntry

	mov r10, rax
	bt r10, 32
	jnc EngineEntityDamageOuterBlocked

	mov rcx, qword ptr [rsp+60h]
	mov rdx, qword ptr [rsp+68h]
	mov r8, qword ptr [rsp+70h]
	mov r9, qword ptr [rsp+78h]
	movdqu xmm0, xmmword ptr [rsp+80h]
	movdqu xmm1, xmmword ptr [rsp+90h]
	movd xmm2, r10d
	movdqu xmm3, xmmword ptr [rsp+0B0h]

	mov rax, qword ptr [rbp+30h]
	mov qword ptr [rsp+20h], rax
	mov rax, qword ptr [rbp+38h]
	mov qword ptr [rsp+28h], rax
	mov rax, qword ptr [rbp+40h]
	mov qword ptr [rsp+30h], rax
	mov rax, qword ptr [rbp+48h]
	mov qword ptr [rsp+38h], rax
	mov rax, qword ptr [rbp+50h]
	mov qword ptr [rsp+40h], rax
	mov rax, qword ptr [rbp+58h]
	mov qword ptr [rsp+48h], rax
	mov rax, qword ptr [rbp+60h]
	mov qword ptr [rsp+50h], rax
	mov rax, qword ptr [rbp+68h]
	mov qword ptr [rsp+58h], rax

	call qword ptr [g_engine_entity_damage_outer_trampoline]

	movdqu xmmword ptr [rsp+0C0h], xmm0
	movd ecx, xmm0
	call EngineEntity_OnDamageReturn
	movdqu xmm0, xmmword ptr [rsp+0C0h]
	jmp EngineEntityDamageOuterDone

EngineEntityDamageOuterBlocked:
	xorps xmm0, xmm0

EngineEntityDamageOuterDone:
	mov rsp, rbp
	pop rbp
	ret
EngineEntityDamageOuterStub ENDP

; 基类 HealthModule::AddDamageToBody 桩：由 Armor/BodyParts 覆写内部转调，
; 引擎用线程局部的调用深度区分“外层事件”和嵌套转调，嵌套时只透传。
EngineEntityDamageBaseStub PROC
	push rbp
	mov rbp, rsp
	sub rsp, 100h

	mov qword ptr [rsp+60h], rcx
	mov qword ptr [rsp+68h], rdx
	mov qword ptr [rsp+70h], r8
	mov qword ptr [rsp+78h], r9
	movdqu xmmword ptr [rsp+80h], xmm0
	movdqu xmmword ptr [rsp+90h], xmm1
	movdqu xmmword ptr [rsp+0A0h], xmm2
	movdqu xmmword ptr [rsp+0B0h], xmm3

	mov rcx, qword ptr [rsp+60h]
	mov rdx, qword ptr [rsp+68h]
	movd r8d, xmm2
	mov r9d, dword ptr [rbp+30h]
	mov eax, dword ptr [rbp+60h]
	mov dword ptr [rsp+20h], eax
	call EngineEntity_OnDamageEntry

	mov r10, rax
	bt r10, 32
	jnc EngineEntityDamageBaseBlocked

	mov rcx, qword ptr [rsp+60h]
	mov rdx, qword ptr [rsp+68h]
	mov r8, qword ptr [rsp+70h]
	mov r9, qword ptr [rsp+78h]
	movdqu xmm0, xmmword ptr [rsp+80h]
	movdqu xmm1, xmmword ptr [rsp+90h]
	movd xmm2, r10d
	movdqu xmm3, xmmword ptr [rsp+0B0h]

	mov rax, qword ptr [rbp+30h]
	mov qword ptr [rsp+20h], rax
	mov rax, qword ptr [rbp+38h]
	mov qword ptr [rsp+28h], rax
	mov rax, qword ptr [rbp+40h]
	mov qword ptr [rsp+30h], rax
	mov rax, qword ptr [rbp+48h]
	mov qword ptr [rsp+38h], rax
	mov rax, qword ptr [rbp+50h]
	mov qword ptr [rsp+40h], rax
	mov rax, qword ptr [rbp+58h]
	mov qword ptr [rsp+48h], rax
	mov rax, qword ptr [rbp+60h]
	mov qword ptr [rsp+50h], rax
	mov rax, qword ptr [rbp+68h]
	mov qword ptr [rsp+58h], rax

	call qword ptr [g_engine_entity_damage_base_trampoline]

	movdqu xmmword ptr [rsp+0C0h], xmm0
	movd ecx, xmm0
	call EngineEntity_OnDamageReturn
	movdqu xmm0, xmmword ptr [rsp+0C0h]
	jmp EngineEntityDamageBaseDone

EngineEntityDamageBaseBlocked:
	xorps xmm0, xmm0

EngineEntityDamageBaseDone:
	mov rsp, rbp
	pop rbp
	ret
EngineEntityDamageBaseStub ENDP

END
