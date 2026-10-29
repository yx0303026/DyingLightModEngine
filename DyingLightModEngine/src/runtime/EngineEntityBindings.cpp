#include "runtime/EngineEntityBindings.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>

namespace
{
	constexpr wchar_t kGameModuleName[] = L"gamedll_x64_rwdi.dll";
	constexpr wchar_t kEngineModuleName[] = L"engine_x64_rwdi.dll";
	constexpr std::uint32_t kGameImageSize = 0x1F64000;
	constexpr std::uint32_t kEngineImageSize = 0xDFB000;
	constexpr char kSetPositionName[] = "?SetWorldPosition@IControlObject@@QEAAXAEBVvec3@@@Z";
	constexpr char kSetDirectionName[] = "?SetWorldDir@IControlObject@@QEAAXAEBVvec3@@@Z";
	constexpr char kSetOrientationName[] = "?FromUpForwardPos@IControlObject@@QEAAXAEBVvec3@@00@Z";
	constexpr char kGetLevelName[] = "?GetLevel@IGSObject@@QEBAPEAVILevel@@XZ";
	constexpr char kGetActiveLevelName[] = "?GetActiveLevel@IGame@@QEAAPEAVILevel@@XZ";
	constexpr char kGetActiveCameraName[] =
		"?GetFirstActiveViewCamera@ILevel@@QEAAPEAVIBaseCamera@@XZ";
	constexpr char kCameraPositionName[] = "?GetPosition@IBaseCamera@@QEBA?BVvec3@@XZ";
	constexpr char kGetLocalIdName[] = "?ReplGetLocalId@IGame@@QEBAGXZ";
	constexpr char kGetObjectIdName[] = "?ReplGetId@IGSObject@@QEBA?AUCObjectId@Repl@Net@@XZ";
	constexpr char kFindObjectsName[] =
		"?FindObjectsInRadius@ILevel@@QEAA_NPEAV?$vector@PEAVIControlObject@@@ttl@@AEBVvec3@@MPEBVCRTTI@@_NPEAH@Z";
	constexpr char kGetBonePositionName[] = "?GetBoneJointPos@IModelObject@@QEBA?AVvec3@@E@Z";
	constexpr char kResolveBoneName[] = "?GetMeshElemFromBoneID@IModelObject@@QEBAHE@Z";

	template <typename Function>
	Function ResolveExport(HMODULE module, const char *name) noexcept
	{
		return module == nullptr
				   ? nullptr
				   : reinterpret_cast<Function>(GetProcAddress(module, name));
	}

	template <typename Function>
	Function ResolveImport(HMODULE module, std::uintptr_t rva) noexcept
	{
		if (module == nullptr)
			return nullptr;
		__try
		{
			return *reinterpret_cast<Function *>(
				reinterpret_cast<std::uintptr_t>(module) + rva);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return nullptr;
		}
	}

	bool ReadImageSize(HMODULE module, std::uint32_t &imageSize) noexcept
	{
		imageSize = 0;
		if (module == nullptr)
		{
			return false;
		}
		__try
		{
			const auto *base = reinterpret_cast<const std::uint8_t *>(module);
			const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
			if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
				dos->e_lfanew > 1 << 20)
			{
				return false;
			}
			const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(base + dos->e_lfanew);
			if (nt->Signature != IMAGE_NT_SIGNATURE ||
				nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
				nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
			{
				return false;
			}
			imageSize = nt->OptionalHeader.SizeOfImage;
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	struct RttiCompleteObjectLocator
	{
		std::uint32_t signature;
		std::uint32_t offset;
		std::uint32_t constructorDisplacement;
		std::int32_t typeDescriptorRva;
		std::int32_t classDescriptorRva;
		std::int32_t selfRva;
	};
}

HMODULE EngineEntityBindings::GameModule() noexcept
{
	return GetModuleHandleW(kGameModuleName);
}

HMODULE EngineEntityBindings::EngineModule() noexcept
{
	return GetModuleHandleW(kEngineModuleName);
}

bool EngineEntityBindings::IsSupportedGame(HMODULE module) noexcept
{
	std::uint32_t imageSize = 0;
	return ReadImageSize(module, imageSize) && imageSize == kGameImageSize;
}

bool EngineEntityBindings::IsSupportedEngine(HMODULE module) noexcept
{
	std::uint32_t imageSize = 0;
	return ReadImageSize(module, imageSize) && imageSize == kEngineImageSize;
}

bool EngineEntityBindings::IsDerivedFrom(
	const void *object,
	std::string_view className) noexcept
{
	if (object == nullptr || className.empty() || className.size() >= 256)
	{
		return false;
	}
	__try
	{
		const auto *vtable = *reinterpret_cast<void *const *const *>(object);
		if (vtable == nullptr || vtable[-1] == nullptr)
			return false;
		const auto *locator = static_cast<const RttiCompleteObjectLocator *>(vtable[-1]);
		if (locator->signature != 1 || locator->selfRva <= 0 ||
			locator->classDescriptorRva <= 0)
		{
			return false;
		}
		const auto imageBase = reinterpret_cast<std::uintptr_t>(locator) -
							   static_cast<std::uint32_t>(locator->selfRva);
		std::uint32_t imageSize = 0;
		if (!ReadImageSize(reinterpret_cast<HMODULE>(imageBase), imageSize))
			return false;
		const auto hierarchyRva = static_cast<std::uint32_t>(locator->classDescriptorRva);
		if (hierarchyRva > imageSize || imageSize - hierarchyRva < 16)
			return false;
		const auto *hierarchy = reinterpret_cast<const std::uint32_t *>(imageBase + hierarchyRva);
		const std::uint32_t count = hierarchy[2];
		const std::uint32_t arrayRva = hierarchy[3];
		if (count == 0 || count > 256 || arrayRva >= imageSize ||
			count > (imageSize - arrayRva) / sizeof(std::uint32_t))
		{
			return false;
		}
		const auto *bases = reinterpret_cast<const std::uint32_t *>(imageBase + arrayRva);
		for (std::uint32_t index = 0; index < count; ++index)
		{
			const std::uint32_t descriptorRva = bases[index];
			if (descriptorRva >= imageSize ||
				imageSize - descriptorRva < className.size() + 21)
				return false;
			const char *name = reinterpret_cast<const char *>(imageBase + descriptorRva + 16);
			if (name[0] == '.' && name[1] == '?' && name[2] == 'A' &&
				(name[3] == 'V' || name[3] == 'U') &&
				std::strncmp(name + 4, className.data(), className.size()) == 0 &&
				name[4 + className.size()] == '@')
			{
				return true;
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
	return false;
}

const EngineEntityBindings *EngineEntityBindings::Resolve() noexcept
{
	static EngineEntityBindings bindings;
	static std::atomic_bool resolved = false;
	static std::mutex resolveMutex;
	if (resolved.load(std::memory_order_acquire))
		return &bindings;
	const std::lock_guard<std::mutex> guard(resolveMutex);
	if (resolved.load(std::memory_order_relaxed))
		return &bindings;

	const HMODULE engine = EngineModule();
	const HMODULE game = GameModule();
	if (!IsSupportedEngine(engine) || !IsSupportedGame(game))
		return nullptr;

	bindings.getPosition = ResolveImport<VectorGetter>(game, GetPositionImportRva);
	bindings.getForward = ResolveImport<VectorGetter>(game, GetForwardVectorImportRva);
	bindings.getUp = ResolveImport<VectorGetter>(game, GetUpVectorImportRva);
	bindings.setPosition = ResolveExport<VectorSetter>(engine, kSetPositionName);
	bindings.setDirection = ResolveExport<VectorSetter>(engine, kSetDirectionName);
	bindings.setOrientation = ResolveExport<OrientationSetter>(engine, kSetOrientationName);
	bindings.getLevel = ResolveExport<GetLevelFunction>(engine, kGetLevelName);
	bindings.getActiveLevel = ResolveExport<GetActiveLevelFunction>(engine, kGetActiveLevelName);
	bindings.getActiveCamera = ResolveExport<GetActiveCameraFunction>(
		engine, kGetActiveCameraName);
	bindings.getCameraPosition = ResolveExport<CameraPositionFunction>(
		engine, kCameraPositionName);
	bindings.getLocalId = ResolveExport<GetLocalIdFunction>(engine, kGetLocalIdName);
	bindings.getObjectId = ResolveExport<GetObjectIdFunction>(engine, kGetObjectIdName);
	bindings.findObjects = ResolveExport<FindObjectsFunction>(engine, kFindObjectsName);
	bindings.getBonePosition = ResolveExport<BonePositionFunction>(engine, kGetBonePositionName);
	bindings.resolveBone = ResolveExport<ResolveBoneFunction>(engine, kResolveBoneName);

	const bool coreResolved = bindings.getPosition != nullptr &&
							  bindings.getForward != nullptr && bindings.getUp != nullptr &&
							  bindings.setPosition != nullptr && bindings.setOrientation != nullptr &&
							  bindings.getLevel != nullptr && bindings.getActiveLevel != nullptr &&
							  bindings.getActiveCamera != nullptr &&
							  bindings.getCameraPosition != nullptr &&
							  bindings.getLocalId != nullptr && bindings.getObjectId != nullptr &&
							  bindings.findObjects != nullptr;
	if (!coreResolved)
		return nullptr;
	resolved.store(true, std::memory_order_release);
	return &bindings;
}
