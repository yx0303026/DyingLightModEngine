#pragma once

#include <Windows.h>

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
	static constexpr std::uintptr_t ModuleScanSize = 0x3000;
	static constexpr std::uintptr_t HealthModuleVtableRva = 0x1455158;
	static constexpr std::uintptr_t BodyPartsHealthModuleVtableRva = 0x1447AF0;
	static constexpr std::uintptr_t ReplicationIdOffset = 0x06;

	static HMODULE GameModule() noexcept;
	static HMODULE EngineModule() noexcept;
	static bool IsSupportedGame(HMODULE module) noexcept;
	static bool IsSupportedEngine(HMODULE module) noexcept;
	static bool IsDerivedFrom(const void* object, std::string_view className) noexcept;
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
};
