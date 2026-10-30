OPTION CASEMAP:NONE

EXTERN EngineEntity_OnHunterUvShieldTimer:PROC
EXTERN EngineEntity_OnHunterUvShieldDrain:PROC
EXTERN EngineEntity_OnHunterUvShieldClientTimer:PROC
EXTERN EngineEntity_OnHunterUvShieldDefensiveSmoke:PROC
EXTERN EngineEntity_OnHunterUvShieldGroundPound:PROC
EXTERN g_engine_entity_hunter_uv_shield_timer_trampoline:QWORD
EXTERN g_engine_entity_hunter_uv_shield_drain_trampoline:QWORD
EXTERN g_engine_entity_hunter_uv_shield_client_trampoline:QWORD
EXTERN g_engine_entity_hunter_uv_shield_smoke_trampoline:QWORD
EXTERN g_engine_entity_hunter_uv_shield_ground_pound_trampoline:QWORD

PUBLIC EngineEntityHunterUvShieldTimerStub
PUBLIC EngineEntityHunterUvShieldDrainStub
PUBLIC EngineEntityHunterUvShieldClientStub
PUBLIC EngineEntityHunterUvShieldSmokeStub
PUBLIC EngineEntityHunterUvShieldGroundPoundStub

.code

; 四处桩替换状态机上的护盾计时写入：主更新、客户端更新、防御烟雾、捶地。
; 保存顺序概览：栈顶依次为 Rbx、R11、R10、R9、R8、Rdx、Rcx、Rax、标志寄存器；原始 RCX 位于 [rbx+30h]。
EngineEntityHunterUvShieldTimerStub PROC
	pushfq
	push rax
	push rcx
	push rdx
	push r8
	push r9
	push r10
	push r11
	push rbx

	mov rbx, rsp
	and rsp, -16
	sub rsp, 80h
	movdqu xmmword ptr [rsp+20h], xmm1
	movdqu xmmword ptr [rsp+30h], xmm2
	movdqu xmmword ptr [rsp+40h], xmm3
	movdqu xmmword ptr [rsp+50h], xmm4
	movdqu xmmword ptr [rsp+60h], xmm5

	mov rcx, qword ptr [rbx+30h]
	movd edx, xmm0
	call EngineEntity_OnHunterUvShieldTimer

	movdqu xmm1, xmmword ptr [rsp+20h]
	movdqu xmm2, xmmword ptr [rsp+30h]
	movdqu xmm3, xmmword ptr [rsp+40h]
	movdqu xmm4, xmmword ptr [rsp+50h]
	movdqu xmm5, xmmword ptr [rsp+60h]
	mov rsp, rbx

	pop rbx
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdx
	pop rcx
	pop rax
	popfq
	jmp qword ptr [g_engine_entity_hunter_uv_shield_timer_trampoline]
EngineEntityHunterUvShieldTimerStub ENDP

; 客户端更新路径用 xmm1 写入，回调返回值需改投到 xmm1 再交给跳板。
EngineEntityHunterUvShieldClientStub PROC
	pushfq
	push rax
	push rcx
	push rdx
	push r8
	push r9
	push r10
	push r11
	push rbx

	mov rbx, rsp
	and rsp, -16
	sub rsp, 70h
	movdqu xmmword ptr [rsp+20h], xmm2
	movdqu xmmword ptr [rsp+30h], xmm3
	movdqu xmmword ptr [rsp+40h], xmm4
	movdqu xmmword ptr [rsp+50h], xmm5

	mov rcx, qword ptr [rbx+30h]
	movd edx, xmm1
	call EngineEntity_OnHunterUvShieldClientTimer

	movaps xmm1, xmm0
	movdqu xmm2, xmmword ptr [rsp+20h]
	movdqu xmm3, xmmword ptr [rsp+30h]
	movdqu xmm4, xmmword ptr [rsp+40h]
	movdqu xmm5, xmmword ptr [rsp+50h]
	mov rsp, rbx

	pop rbx
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdx
	pop rcx
	pop rax
	popfq
	jmp qword ptr [g_engine_entity_hunter_uv_shield_client_trampoline]
EngineEntityHunterUvShieldClientStub ENDP

EngineEntityHunterUvShieldSmokeStub PROC
	pushfq
	push rax
	push rcx
	push rdx
	push r8
	push r9
	push r10
	push r11
	push rbx

	mov rbx, rsp
	and rsp, -16
	sub rsp, 80h
	movdqu xmmword ptr [rsp+20h], xmm1
	movdqu xmmword ptr [rsp+30h], xmm2
	movdqu xmmword ptr [rsp+40h], xmm3
	movdqu xmmword ptr [rsp+50h], xmm4
	movdqu xmmword ptr [rsp+60h], xmm5

	mov rcx, qword ptr [rbx+30h]
	movd edx, xmm0
	call EngineEntity_OnHunterUvShieldDefensiveSmoke

	movdqu xmm1, xmmword ptr [rsp+20h]
	movdqu xmm2, xmmword ptr [rsp+30h]
	movdqu xmm3, xmmword ptr [rsp+40h]
	movdqu xmm4, xmmword ptr [rsp+50h]
	movdqu xmm5, xmmword ptr [rsp+60h]
	mov rsp, rbx

	pop rbx
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdx
	pop rcx
	pop rax
	popfq
	jmp qword ptr [g_engine_entity_hunter_uv_shield_smoke_trampoline]
EngineEntityHunterUvShieldSmokeStub ENDP

EngineEntityHunterUvShieldGroundPoundStub PROC
	pushfq
	push rax
	push rcx
	push rdx
	push r8
	push r9
	push r10
	push r11
	push rbx

	mov rbx, rsp
	and rsp, -16
	sub rsp, 80h
	movdqu xmmword ptr [rsp+20h], xmm1
	movdqu xmmword ptr [rsp+30h], xmm2
	movdqu xmmword ptr [rsp+40h], xmm3
	movdqu xmmword ptr [rsp+50h], xmm4
	movdqu xmmword ptr [rsp+60h], xmm5

	mov rcx, qword ptr [rbx+30h]
	movd edx, xmm0
	call EngineEntity_OnHunterUvShieldGroundPound

	movdqu xmm1, xmmword ptr [rsp+20h]
	movdqu xmm2, xmmword ptr [rsp+30h]
	movdqu xmm3, xmmword ptr [rsp+40h]
	movdqu xmm4, xmmword ptr [rsp+50h]
	movdqu xmm5, xmmword ptr [rsp+60h]
	mov rsp, rbx

	pop rbx
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdx
	pop rcx
	pop rax
	popfq
	jmp qword ptr [g_engine_entity_hunter_uv_shield_ground_pound_trampoline]
EngineEntityHunterUvShieldGroundPoundStub ENDP

; 函数入口桩：rcx = UV 组件，xmm1 = 增减量；回调返回的新增量改投 xmm1 后交给跳板重放序言。
EngineEntityHunterUvShieldDrainStub PROC
	pushfq
	push rax
	push rcx
	push rdx
	push r8
	push r9
	push r10
	push r11
	push rbx

	mov rbx, rsp
	and rsp, -16
	sub rsp, 70h
	movdqu xmmword ptr [rsp+20h], xmm2
	movdqu xmmword ptr [rsp+30h], xmm3
	movdqu xmmword ptr [rsp+40h], xmm4
	movdqu xmmword ptr [rsp+50h], xmm5

	mov rcx, qword ptr [rbx+30h]
	movd edx, xmm1
	call EngineEntity_OnHunterUvShieldDrain

	movaps xmm1, xmm0
	movdqu xmm2, xmmword ptr [rsp+20h]
	movdqu xmm3, xmmword ptr [rsp+30h]
	movdqu xmm4, xmmword ptr [rsp+40h]
	movdqu xmm5, xmmword ptr [rsp+50h]
	mov rsp, rbx

	pop rbx
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdx
	pop rcx
	pop rax
	popfq
	jmp qword ptr [g_engine_entity_hunter_uv_shield_drain_trampoline]
EngineEntityHunterUvShieldDrainStub ENDP

END
