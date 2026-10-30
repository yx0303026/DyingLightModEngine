#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

class EngineEntityBindings final
{
public:
	using VectorGetter = void* (__fastcall*)(void*, void*);
	using VectorSetter = void(__fastcall*)(void*, const void*);
	using OrientationSetter = void(__fastcall*)(void*, const void*, const void*, const void*);
	using GetLevelFunction = void* (__fastcall*)(void*);
	using GetActiveLevelFunction = void* (__fastcall*)(void*);
	using GetActiveCameraFunction = void* (__fastcall*)(void*);
	using CameraPositionFunction = void* (__fastcall*)(void*, void*);
	using GetLocalIdFunction = std::uint16_t(__fastcall*)(void*);
	using GetObjectIdFunction = void* (__fastcall*)(void*, void*);
	using FindObjectsFunction = bool(__fastcall*)(void*, void*, const void*, float, const void*, bool, int*);
	using BonePositionFunction = void* (__fastcall*)(void*, void*, std::uint8_t);
	using ResolveBoneFunction = int(__fastcall*)(void*, std::uint8_t);
	using CameraMatrixFunction = const float* (__fastcall*)(void*);
	using ElementOrBoneIdFunction = int(__fastcall*)(void*, const char*);

	static constexpr std::uintptr_t GameGlobalRva = 0x1C16348;
	static constexpr std::uintptr_t GetForwardVectorImportRva = 0x137EAC0;
	static constexpr std::uintptr_t GetPositionImportRva = 0x137EAD0;
	static constexpr std::uintptr_t GetUpVectorImportRva = 0x137BA98;
	static constexpr std::uintptr_t ControlObjectOffset = 0x18;
	static constexpr std::uintptr_t HunterModelIdOffset = 0x7D8;
	static constexpr std::uint8_t HunterModelId = 0x0D;
	static constexpr std::uintptr_t HunterPositionOffset = 0x7F0;
	static constexpr std::uintptr_t HunterMovementMarkerOffset = 0x217;
	static constexpr std::uint32_t HunterMovementMarker = 2816;
	static constexpr std::uintptr_t HunterMovementVelocityOffset = 0x198;
	static constexpr std::uintptr_t HealthBodyOffset = 0x78;
	static constexpr std::uintptr_t HealthBloodMaskOffset = 0x7C;
	static constexpr std::uintptr_t HealthLastDamagerOffset = 0xB0;
	/* CHealthModule / CAIModule 的宿主实体指针。 */
	static constexpr std::uintptr_t HealthModuleOwnerOffset = 0x40;
	/* HealthModule::AddDamageToBody 与 ArmorHealthModule/BodyPartsHealthModule 覆写入口。
	   参数：rcx = HealthModule，rdx = 伤害来源实体，xmm2 = 伤害，[rsp+28h] = 伤害类型。 */
	static constexpr std::uintptr_t HealthModuleDamageSiteRva = 0x2A2830;
	static constexpr std::uintptr_t HealthModuleArmorDamageSiteRva = 0x279AF0;
	static constexpr std::uintptr_t ModuleScanSize = 0x3000;
	static constexpr std::uintptr_t HealthModuleVtableRva = 0x1455158;
	static constexpr std::uintptr_t BodyPartsHealthModuleVtableRva = 0x1447AF0;
	static constexpr std::uintptr_t ReplicationIdOffset = 0x06;
	static constexpr std::uintptr_t ActorVisualOffset = 0x338;
	/* CZombieStateMachine：BTZ 猎手状态机的状态、光免疫计时与 HUD 格挡标记。 */
	static constexpr std::uintptr_t HunterStateMachineOffset = 0xC98;
	static constexpr std::uintptr_t ZombieStateMachineStateOffset = 0x38;
	static constexpr std::uintptr_t ZombieStateMachineShieldSecondsOffset = 0xA4;
	static constexpr std::uintptr_t ZombieStateMachineShieldFlagOffset = 0xA8;
	static constexpr std::uintptr_t ZombieStateMachineSmokeChargesOffset = 0x70;
	/* 猎手 UV 组件：+0x10 为 UV 能量（被紫外线手电消耗），挂在玩家对象 +0x1348。 */
	static constexpr std::uintptr_t HunterUvComponentOffset = 0x1348;
	/* 紫外线对 UV 能量的增减入口 sub_180CE1410。 */
	static constexpr std::uintptr_t HunterUvDrainSiteRva = 0xCE1410;
	/* 状态机写入光免疫计时的四处指令（护盾递减、客户端递减、防御烟雾、捶地）。 */
	static constexpr std::uintptr_t HunterUvShieldUpdateSiteRva = 0xDD98BD;
	static constexpr std::uintptr_t HunterUvShieldClientSiteRva = 0xDD9977;
	static constexpr std::uintptr_t HunterUvShieldSmokeSiteRva = 0xDD87A2;
	static constexpr std::uintptr_t HunterUvShieldGroundPoundSiteRva = 0xDDD5F9;
	/* HUD 能力容器更新点：每帧刷新五个猎手能力项，UVBlock 位于第 5 项。 */
	static constexpr std::uintptr_t HunterUvShieldHudContainerSiteRva = 0x5C66C0;
	/* HUD 能力项更新点：写入 T_AmmoCount 文本和 I_Fill 进度。 */
	static constexpr std::uintptr_t HunterUvShieldHudAbilitySiteRva = 0x5C6B80;
	/* IUIElement::SetVisible 的游戏导入表地址。 */
	static constexpr std::uintptr_t UiElementSetVisibleImportRva = 0x137C500;

	static HMODULE GameModule() noexcept;
	static HMODULE EngineModule() noexcept;
	static bool IsSupportedGame(HMODULE module) noexcept;
	static bool IsSupportedEngine(HMODULE module) noexcept;
	static bool IsDerivedFrom(const void* object, std::string_view className) noexcept;
	/* 读取对象动态类型的装饰名，用于弹丸视觉类别判定。 */
	static bool ReadTypeName(
		const void* object,
		char* buffer,
		std::size_t capacity) noexcept;
	static const EngineEntityBindings* Resolve() noexcept;

	VectorGetter getPosition = nullptr;
	VectorGetter getForward = nullptr;
	VectorGetter getUp = nullptr;
	VectorSetter setPosition = nullptr;
	VectorSetter setDirection = nullptr;
	OrientationSetter setOrientation = nullptr;
	GetLevelFunction getLevel = nullptr;
	GetActiveLevelFunction getActiveLevel = nullptr;
	GetActiveCameraFunction getActiveCamera = nullptr;
	CameraPositionFunction getCameraPosition = nullptr;
	GetLocalIdFunction getLocalId = nullptr;
	GetObjectIdFunction getObjectId = nullptr;
	FindObjectsFunction findObjects = nullptr;
	BonePositionFunction getBonePosition = nullptr;
	ResolveBoneFunction resolveBone = nullptr;
	CameraMatrixFunction getCameraMatrix = nullptr;
	ElementOrBoneIdFunction getElementOrBoneElementId = nullptr;
};
