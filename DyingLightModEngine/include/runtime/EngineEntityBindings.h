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

	/* gamedll 中保存 IGame 单例指针的全局槽位；解引用后作为 GetActiveLevel /
	   ReplGetLocalId 的 this，是定位活动关卡与本地玩家的唯一起点。 */
	static constexpr std::uintptr_t GameGlobalRva = 0x1C16348;
	/* gamedll 导入表（IAT）槽位，存放引擎 IControlObject 前向向量读取函数地址；
	   解引用得到的指针即 VectorGetter，不是函数本身。 */
	static constexpr std::uintptr_t GetForwardVectorImportRva = 0x137EAC0;
	/* gamedll IAT 槽位，存放 IControlObject 世界坐标读取函数地址。 */
	static constexpr std::uintptr_t GetPositionImportRva = 0x137EAD0;
	/* gamedll IAT 槽位，存放 IControlObject 上向量读取函数地址。 */
	static constexpr std::uintptr_t GetUpVectorImportRva = 0x137BA98;
	/* 实体基址到 IControlObject 子对象的偏移：entity + 0x18 即控制对象接口。
	   位置/朝向等接口函数必须以该地址为 this 调用，反向减回去可还原实体基址。 */
	static constexpr std::uintptr_t ControlObjectOffset = 0x18;
	/* IControlObject + 0x7D8：uint8 模型 ID，用于区分猎手与普通人类玩家。 */
	static constexpr std::uintptr_t HunterModelIdOffset = 0x7D8;
	/* 猎手（BTZ Night Hunter）的模型 ID 取值。 */
	static constexpr std::uint8_t HunterModelId = 0x0D;
	/* IControlObject + 0x7F0：vec3 世界坐标；猎手位置回调中直接读取该字段，
	   不再走 getPosition 接口，可避免在回调里重入虚函数。 */
	static constexpr std::uintptr_t HunterPositionOffset = 0x7F0;
	/* 猎手移动组件 + 0x217：uint32 组件类型标记，用于确认收到的对象是猎手移动组件。 */
	static constexpr std::uintptr_t HunterMovementMarkerOffset = 0x217;
	/* 猎手移动组件的标记值（0xB00），与 HunterMovementMarkerOffset 处的值比对。 */
	static constexpr std::uint32_t HunterMovementMarker = 2816;
	/* 猎手移动组件 + 0x198：vec3 速度，供运动跟踪器计算速度/加速度（ObserveVelocity）。 */
	static constexpr std::uintptr_t HunterMovementVelocityOffset = 0x198;
	/* CHealthModule + 0x78：float 主生命值（游戏按 max(old - damage, 0) 回写）。 */
	static constexpr std::uintptr_t HealthBodyOffset = 0x78;
	/* CHealthModule + 0x7C：float 第二层血量/护甲，承受 25% 上限的溢出伤害。 */
	static constexpr std::uintptr_t HealthBloodMaskOffset = 0x7C;
	/* CHealthModule + 0xB0：uint64 最后伤害者标识。 */
	static constexpr std::uintptr_t HealthLastDamagerOffset = 0xB0;
	/* CHealthModule 由 AIModule 派生，+0x40 保存宿主实体指针（与 IActor 指针一致）。 */
	static constexpr std::uintptr_t HealthModuleOwnerOffset = 0x40;
	/* CHealthModule::AddDamageToBody 函数入口（非虚表槽位）。玩家、AI、载具等所有挂了
	   HealthModule 的实体共用此入口；HealthModule 族三个主虚表 slot 263（+0x838）都指向
	   这里或下方的 Armor/BodyParts 覆写。
	   签名：float __fastcall(this, source, xmm2 = damage, r9 = arg4,
	   [rsp+28h] = damageType, …, [rsp+58h] = skipHealth)，返回 xmm0 = 结算后最终伤害。 */
	static constexpr std::uintptr_t HealthModuleDamageSiteRva = 0x2A2830;
	/* ArmorHealthModule / BodyPartsHealthModule 的 AddDamageToBody 覆写入口（同样是函数体，
	   不是虚表地址）。参数寄存器布局与基类一致，内部会先以 skipHealth = 1 转调基类算最终伤害，
	   再以调用方传入的 skipHealth 扣主生命，因此一次受伤可能产生 1~2 次调用，需要按深度去重。 */
	static constexpr std::uintptr_t HealthModuleArmorDamageSiteRva = 0x279AF0;
	/* 在实体基址起的这段范围内按指针步长扫描，用虚表比对定位 HealthModule 实例。 */
	static constexpr std::uintptr_t ModuleScanSize = 0x3000;
	/* CHealthModule 主虚表（??_7...@@6B@），用于 FindHealthModule 的扫描比对。 */
	static constexpr std::uintptr_t HealthModuleVtableRva = 0x1455158;
	/* BodyPartsHealthModule 主虚表，同样只用于扫描比对。 */
	static constexpr std::uintptr_t BodyPartsHealthModuleVtableRva = 0x1447AF0;
	/* IGSObject::ReplGetId 返回的 CObjectId + 0x06：uint16 网络复制 ID，
	   与 IGame::ReplGetLocalId 的本地 ID 比对即可定位本地玩家实体。 */
	static constexpr std::uintptr_t ReplicationIdOffset = 0x06;
	/* ActorDI + 0x338：视觉对象（HumanAIVis / ZombieAIVis）指针；ActorDI 自身没有
	   可见的分类特征时，靠它继续判定实体种类。 */
	static constexpr std::uintptr_t ActorVisualOffset = 0x338;
	/* 猎手对象 + 0xC98：CZombieStateMachine*（BTZ 猎手专用状态机）。
	   下列四个字段都相对该状态机指针计算。 */
	static constexpr std::uintptr_t HunterStateMachineOffset = 0xC98;
	/* CZombieStateMachine + 0x38：int32 EZombieState（0 Idle … 6 GroundPound、
	   7 DefensiveSmoke、9~11 UvHeal 系列、12/13 BuggyCharge）。 */
	static constexpr std::uintptr_t ZombieStateMachineStateOffset = 0x38;
	/* CZombieStateMachine + 0xA4：float 光免疫剩余秒数，即 HUD 上的“紫外线护盾”。 */
	static constexpr std::uintptr_t ZombieStateMachineShieldSecondsOffset = 0xA4;
	/* CZombieStateMachine + 0xA8：uint8 UV 格挡标记，仅决定 HUD 是否显示 UVBlock 图标，
	   不参与计时；PIN 模式下由引擎主动置 1 以保持界面一致。 */
	static constexpr std::uintptr_t ZombieStateMachineShieldFlagOffset = 0xA8;
	/* CZombieStateMachine + 0x70：int32 防御烟雾剩余充能，-1 表示无限。 */
	static constexpr std::uintptr_t ZombieStateMachineSmokeChargesOffset = 0x70;
	/* 玩家对象 + 0x1348：UV 组件指针；组件 + 0x10 为 float UV 能量（被紫外线手电消耗），
	   + 0x20 为满值标记。 */
	static constexpr std::uintptr_t HunterUvComponentOffset = 0x1348;
	/* sub_180CE1410：UV 能量的增减入口，猎手 state case 3/6/7 通过它改动组件能量。 */
	static constexpr std::uintptr_t HunterUvDrainSiteRva = 0xCE1410;
	/* sub_180DD8620 的 UvHeal_Anticipation 状态写入点。 */
	static constexpr std::uintptr_t HunterUvRecoveryStartSiteRva = 0xDD8F27;
	/* sub_180DDDC10 的 UvHeal_Recovery 状态写入点。 */
	static constexpr std::uintptr_t HunterUvRecoveryCompleteSiteRva = 0xDDDDDD;
	/* 写入 + 0xA4 光免疫计时的四处指令（均为 movss [rbx+0A4h]），钩子按点安装：
	   主更新 sub_180DD9120：timer = max(timer - dt, 0)，每帧递减。 */
	static constexpr std::uintptr_t HunterUvShieldUpdateSiteRva = 0xDD98BD;
	/* 客户端更新 sub_180DD9920：同样的递减，仅在本地控制条件成立时执行。 */
	static constexpr std::uintptr_t HunterUvShieldClientSiteRva = 0xDD9977;
	/* 防御烟雾 sub_180DD8620：timer = params[734]，并把 +0xA8 置 1。 */
	static constexpr std::uintptr_t HunterUvShieldSmokeSiteRva = 0xDD87A2;
	/* 烟雾开始提交被拒绝时，跳过计时器写入和 HUD 标记后的继续地址。 */
	static constexpr std::uintptr_t HunterUvShieldSmokeAfterFlagRva = 0xDD8E48;
	/* 捶地 sub_180DDD4C0：timer = max(timer, params[735])，并把 +0xA8 置 0。 */
	static constexpr std::uintptr_t HunterUvShieldGroundPoundSiteRva = 0xDDD5F9;
	/* 捶地开始提交被拒绝时，跳过计时器写入和 HUD 标记后的继续地址。 */
	static constexpr std::uintptr_t HunterUvShieldGroundPoundAfterFlagRva = 0xDDD7BD;
	/* HudZombieAbilities 容器更新入口：容器 + 0x68 为能力项数量，+ 0x60 + 4 * sizeof(void*)
	   处缓存 UVBlock（索引 4）能力项指针。游戏每帧最多刷新 5 个猎手能力项。 */
	static constexpr std::uintptr_t HunterUvShieldHudContainerSiteRva = 0x5C66C0;
	/* 单个能力项更新点：+0x28 为 I_Fill 进度条、+0x30 为 T_AmmoCount 文本，
	   引擎通过 IUIElement::SetVisible 隐藏这两项以去除原版护盾读数。 */
	static constexpr std::uintptr_t HunterUvShieldHudAbilitySiteRva = 0x5C6B80;
	/* gamedll IAT 槽位，存放 IUIElement::SetVisible(this, bool) 的解析地址。 */
	static constexpr std::uintptr_t UiElementSetVisibleImportRva = 0x137C500;

	/* 游戏主模块 gamedll_x64_rwdi.dll；未加载或被其它进程占用时返回 nullptr。 */
	static HMODULE GameModule() noexcept;
	/* 引擎模块 engine_x64_rwdi.dll；setPosition 等写接口通过其导出表解析。 */
	static HMODULE EngineModule() noexcept;
	/* 校验模块映像大小与预期游戏版本一致，避免在其它版本上套用错误 RVA。 */
	static bool IsSupportedGame(HMODULE module) noexcept;
	/* 校验引擎模块映像大小与预期版本一致。 */
	static bool IsSupportedEngine(HMODULE module) noexcept;
	/* 沿 RTTI 基类表判断对象是否由 className 派生；失败（含非法指针）返回 false。 */
	static bool IsDerivedFrom(const void* object, std::string_view className) noexcept;
	/* 读取对象动态类型的装饰名，用于弹丸视觉类别判定。 */
	static bool ReadTypeName(
		const void* object,
		char* buffer,
		std::size_t capacity) noexcept;
	/* 返回进程内唯一的绑定实例；模块或版本不匹配时返回 nullptr，结果首次解析后缓存。 */
	static const EngineEntityBindings* Resolve() noexcept;

	/* 以下接口在 Resolve() 中一次性解析，任一为空表示该功能不可用。 */
	/* IControlObject 世界坐标读取（实体需先按 ControlObjectOffset 取接口）。 */
	VectorGetter getPosition = nullptr;
	/* IControlObject 前向向量。 */
	VectorGetter getForward = nullptr;
	/* IControlObject 上向量。 */
	VectorGetter getUp = nullptr;
	/* IControlObject::SetWorldPosition。 */
	VectorSetter setPosition = nullptr;
	/* IControlObject::SetWorldDir。 */
	VectorSetter setDirection = nullptr;
	/* IControlObject::FromUpForwardPos，一次写入朝向三轴。 */
	OrientationSetter setOrientation = nullptr;
	/* IGSObject::GetLevel：任意游戏对象的所属关卡。 */
	GetLevelFunction getLevel = nullptr;
	/* IGame::GetActiveLevel：当前活动关卡。 */
	GetActiveLevelFunction getActiveLevel = nullptr;
	/* ILevel::GetFirstActiveViewCamera：用于取视图矩阵的相机。 */
	GetActiveCameraFunction getActiveCamera = nullptr;
	/* IBaseCamera::GetPosition。 */
	CameraPositionFunction getCameraPosition = nullptr;
	/* IGame::ReplGetLocalId：本地玩家复制 ID（uint16）。 */
	GetLocalIdFunction getLocalId = nullptr;
	/* IGSObject::ReplGetId：返回 CObjectId（返回值缓冲区由调用方提供）。 */
	GetObjectIdFunction getObjectId = nullptr;
	/* ILevel::FindObjectsInRadius：批量收集半径内的 IControlObject 指针。 */
	FindObjectsFunction findObjects = nullptr;
	/* IModelObject::GetBoneJointPos：按骨骼 ID 取关节位置（this 必须是实体基址）。 */
	BonePositionFunction getBonePosition = nullptr;
	/* IModelObject::GetMeshElemFromBoneID：把骨骼 ID 解析为网格元素，返回负值表示无效。 */
	ResolveBoneFunction resolveBone = nullptr;
	/* IBaseCamera::GetCombinedMatrix：4x4 视图投影矩阵，按行主序存放（m[0..3] 为第一行，
	   世界坐标以 p·M 方式投影）。 */
	CameraMatrixFunction getCameraMatrix = nullptr;
	/* IModelObject::GetElementOrBoneElementID：按骨骼名取 ID，返回负值表示未找到。 */
	ElementOrBoneIdFunction getElementOrBoneElementId = nullptr;
};
