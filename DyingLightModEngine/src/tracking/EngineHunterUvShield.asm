OPTION CASEMAP:NONE

EXTERN EngineEntity_OnHunterUvShieldTimer:PROC
EXTERN EngineEntity_OnHunterUvShieldDrain:PROC
EXTERN EngineEntity_OnHunterUvShieldClientTimer:PROC
EXTERN EngineEntity_OnHunterUvShieldDefensiveSmoke:PROC
EXTERN EngineEntity_OnHunterUvShieldGroundPound:PROC
EXTERN EngineEntity_GetHunterUvShieldHudRatio:PROC
EXTERN g_engine_entity_hunter_uv_shield_timer_trampoline:QWORD
EXTERN g_engine_entity_hunter_uv_shield_drain_trampoline:QWORD
EXTERN g_engine_entity_hunter_uv_shield_client_trampoline:QWORD
EXTERN g_engine_entity_hunter_uv_shield_smoke_trampoline:QWORD
EXTERN g_engine_entity_hunter_uv_shield_ground_pound_trampoline:QWORD
EXTERN g_engine_entity_hunter_uv_shield_hud_trampoline:QWORD

PUBLIC EngineEntityHunterUvShieldTimerStub
PUBLIC EngineEntityHunterUvShieldDrainStub
PUBLIC EngineEntityHunterUvShieldClientStub
PUBLIC EngineEntityHunterUvShieldSmokeStub
PUBLIC EngineEntityHunterUvShieldGroundPoundStub
PUBLIC EngineEntityHunterUvShieldHudStub

.code

; 四处桩替换状态机上的护盾计时写入：主更新、客户端更新、防御烟雾、捶地。
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

	mov rcx, qword ptr [rbx]
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

	mov rcx, qword ptr [rbx]
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

	mov rcx, qword ptr [rbx]
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

	mov rcx, qword ptr [rbx]
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

; HUD 比例跳板：仅替换紫外线图标的进度比例，不改写游戏 UV 能量组件。
EngineEntityHunterUvShieldHudStub PROC
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
	sub rsp, 90h
	movdqu xmmword ptr [rsp+20h], xmm0
	movdqu xmmword ptr [rsp+30h], xmm1
	movdqu xmmword ptr [rsp+40h], xmm2
	movdqu xmmword ptr [rsp+50h], xmm3
	movdqu xmmword ptr [rsp+60h], xmm4
	movdqu xmmword ptr [rsp+70h], xmm5

	call EngineEntity_GetHunterUvShieldHudRatio

	xorps xmm1, xmm1
	ucomiss xmm0, xmm1
	jp HunterUvShieldHudKeepOriginal
	jb HunterUvShieldHudKeepOriginal
	movaps xmm6, xmm0

HunterUvShieldHudKeepOriginal:
	movdqu xmm0, xmmword ptr [rsp+20h]
	movdqu xmm1, xmmword ptr [rsp+30h]
	movdqu xmm2, xmmword ptr [rsp+40h]
	movdqu xmm3, xmmword ptr [rsp+50h]
	movdqu xmm4, xmmword ptr [rsp+60h]
	movdqu xmm5, xmmword ptr [rsp+70h]
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
	jmp qword ptr [g_engine_entity_hunter_uv_shield_hud_trampoline]
EngineEntityHunterUvShieldHudStub ENDP

END
