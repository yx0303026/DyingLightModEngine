#include "EngineCollisionRuntime.h"

#include <Windows.h>

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {

	const wchar_t* const kEngineModuleName = L"engine_x64_rwdi.dll";
	constexpr std::uint32_t kExpectedEngineImageSize = 0xDFB000;
	constexpr std::uintptr_t kControlModelOffset = 0x08;
	constexpr std::uintptr_t kModelTraceModeOffset = 0x417;
	constexpr std::uintptr_t kModelThreadDataOffset = 0x388;
	constexpr std::uintptr_t kMeshElementsOffset = 0x18;
	constexpr std::uintptr_t kMeshElementCountOffset = 0x28;
	constexpr std::uintptr_t kElementThreadDataOffset = 0x28;
	constexpr std::uintptr_t kElementTransformOffset = 0x20;
	constexpr std::uintptr_t kElementStride = 0x78;
	constexpr std::uintptr_t kElementFlagsOffset = 0x00;
	constexpr std::uintptr_t kElementMinimumOffset = 0x04;
	constexpr std::uintptr_t kElementMaximumOffset = 0x10;
	constexpr std::uintptr_t kResolveModelDataRva = 0x09A520;
	constexpr std::uintptr_t kResolveElementDataRva = 0x12FA10;
	constexpr std::uintptr_t kGetWorldMatrixRva = 0x4862F0;
	constexpr std::uint32_t kTraceBoxMask = 0x600000;
	constexpr std::uint8_t kBoxTraceMode = 1;
	constexpr std::uint8_t kMixedTraceMode = 4;
	constexpr std::uint16_t kMaximumElements = 1024;
	constexpr std::size_t kMaximumElementNameLength = 128;

	using ResolveThreadDataFunction = void* (__fastcall*)(void*);
	using GetWorldMatrixFunction = const float* (__fastcall*)(void*);
	using GetElementNameFunction = const char* (__fastcall*)(void*, int);

	bool IsSupportedEngineModule(HMODULE module) noexcept
	{
		if (module == nullptr) {
			return false;
		}
		__try {
			const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
			if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
				return false;
			}
			const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
				reinterpret_cast<const std::uint8_t*>(module) + dos->e_lfanew);
			return nt->Signature == IMAGE_NT_SIGNATURE &&
				nt->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
				nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
				nt->OptionalHeader.SizeOfImage == kExpectedEngineImageSize;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
	}

	bool IsFinite(const DlModVector3V1& value) noexcept
	{
		return std::isfinite(value.x) && std::isfinite(value.y) &&
			std::isfinite(value.z);
	}

	bool IsValidBounds(
		const DlModVector3V1& minimum,
		const DlModVector3V1& maximum) noexcept
	{
		return IsFinite(minimum) && IsFinite(maximum) &&
			minimum.x < maximum.x && minimum.y < maximum.y &&
			minimum.z < maximum.z;
	}

	template<typename Value>
	bool ReadField(const void* object, std::uintptr_t offset, Value& value) noexcept
	{
		if (object == nullptr) {
			return false;
		}
		__try {
			value = *reinterpret_cast<const Value*>(
				static_cast<const std::uint8_t*>(object) + offset);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
	}

	template<typename Function>
	Function ResolveRva(HMODULE module, std::uintptr_t rva) noexcept
	{
		return module == nullptr
			? nullptr
			: reinterpret_cast<Function>(
				reinterpret_cast<std::uintptr_t>(module) + rva);
	}

	struct CollisionBindings final {
		HMODULE module = nullptr;
		ResolveThreadDataFunction resolveModelData = nullptr;
		ResolveThreadDataFunction resolveElementData = nullptr;
		GetWorldMatrixFunction getWorldMatrix = nullptr;
		GetElementNameFunction getElementName = nullptr;
	};

	const CollisionBindings& ResolveBindings(HMODULE engine) noexcept
	{
		static CollisionBindings bindings;
		if (bindings.module == engine) {
			return bindings;
		}

		bindings = {};
		bindings.module = engine;
		bindings.resolveModelData =
			ResolveRva<ResolveThreadDataFunction>(engine, kResolveModelDataRva);
		bindings.resolveElementData =
			ResolveRva<ResolveThreadDataFunction>(engine, kResolveElementDataRva);
		bindings.getWorldMatrix =
			ResolveRva<GetWorldMatrixFunction>(engine, kGetWorldMatrixRva);
		bindings.getElementName = reinterpret_cast<GetElementNameFunction>(
			GetProcAddress(
				engine,
				"?GetElementNameCStr@IModelObject@@QEBAPEBDH@Z"));
		return bindings;
	}

	void ClearSnapshot(DlModHitboxSnapshotV1& snapshot) noexcept
	{
		snapshot.traceMode = 0;
		snapshot.unsupportedCount = 0;
		snapshot.requiredCount = 0;
		snapshot.count = 0;
	}

	bool ReadName(
		void* controlObject,
		int elementIndex,
		GetElementNameFunction getName,
		char (&output)[kMaximumElementNameLength]) noexcept
	{
		output[0] = '\0';
		if (controlObject == nullptr || getName == nullptr) {
			return false;
		}
		__try {
			const char* source = getName(controlObject, elementIndex);
			if (source == nullptr) {
				return false;
			}
			for (std::size_t index = 0; index + 1 < sizeof(output); ++index) {
				output[index] = source[index];
				if (output[index] == '\0') {
					return index != 0;
				}
			}
			output[sizeof(output) - 1] = '\0';
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			output[0] = '\0';
			return false;
		}
	}

	bool ReadMatrix(
		void* transform,
		GetWorldMatrixFunction getWorldMatrix,
		float (&output)[12]) noexcept
	{
		if (transform == nullptr || getWorldMatrix == nullptr) {
			return false;
		}
		__try {
			const float* source = getWorldMatrix(transform);
			if (source == nullptr) {
				return false;
			}
			for (std::size_t index = 0; index < 12; ++index) {
				output[index] = source[index];
				if (!std::isfinite(output[index])) {
					return false;
				}
			}
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
	}

}

const DlModCollisionApiV1* EngineCollisionRuntime::GetApi() noexcept
{
	static const DlModCollisionApiV1 api = {
		sizeof(DlModCollisionApiV1),
		MODSDK_ABI_VERSION,
		&CaptureHitboxes
	};
#if !defined(_M_X64)
	return nullptr;
#else
	return &api;
#endif
}

DlModResult MODSDK_CALL EngineCollisionRuntime::CaptureHitboxes(
	void* controlObject,
	DlModHitboxSnapshotV1* snapshot) noexcept
{
	if (snapshot == nullptr ||
		snapshot->structSize < sizeof(DlModHitboxSnapshotV1) ||
		snapshot->abiVersion != MODSDK_ABI_VERSION ||
		snapshot->elementSize != sizeof(DlModHitboxV1) ||
		(snapshot->capacity != 0 && snapshot->elements == nullptr) ||
		controlObject == nullptr) {
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}

	const std::uint32_t capacity = snapshot->capacity;
	DlModHitboxV1* elements = snapshot->elements;
	ClearSnapshot(*snapshot);

#if !defined(_M_X64)
	(void)capacity;
	(void)elements;
	return DLMOD_RESULT_UNSUPPORTED_FEATURE;
#else
	const HMODULE engine = GetModuleHandleW(kEngineModuleName);
	if (!IsSupportedEngineModule(engine)) {
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	}

	void* model = nullptr;
	if (!ReadField(controlObject, kControlModelOffset, model) || model == nullptr) {
		return DLMOD_RESULT_NOT_FOUND;
	}

	std::uint8_t traceMode = 0;
	if (!ReadField(model, kModelTraceModeOffset, traceMode)) {
		return DLMOD_RESULT_INVALID_STATE;
	}
	snapshot->traceMode = traceMode;
	if (traceMode != kBoxTraceMode && traceMode != kMixedTraceMode) {
		snapshot->unsupportedCount = 1;
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	}

	const CollisionBindings& bindings = ResolveBindings(engine);
	if (bindings.resolveModelData == nullptr ||
		bindings.resolveElementData == nullptr ||
		bindings.getWorldMatrix == nullptr) {
		return DLMOD_RESULT_INVALID_STATE;
	}
	void* modelData = nullptr;
	__try {
		modelData = bindings.resolveModelData(
			static_cast<std::uint8_t*>(model) + kModelThreadDataOffset);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		modelData = nullptr;
	}

	void* mesh = nullptr;
	std::uint16_t elementCount = 0;
	void* elementStorage = nullptr;
	if (modelData == nullptr ||
		!ReadField(modelData, 0, mesh) ||
		!ReadField(mesh, kMeshElementCountOffset, elementCount) ||
		elementCount > kMaximumElements ||
		!ReadField(mesh, kMeshElementsOffset, elementStorage) ||
		(elementCount != 0 && elementStorage == nullptr)) {
		return DLMOD_RESULT_INVALID_STATE;
	}

	for (std::uint16_t index = 0; index < elementCount; ++index) {
		const auto* element = static_cast<const std::uint8_t*>(elementStorage) +
			static_cast<std::size_t>(index) * kElementStride;
		void* elementData = nullptr;
		__try {
			elementData = bindings.resolveElementData(
				const_cast<std::uint8_t*>(element) + kElementThreadDataOffset);
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			elementData = nullptr;
		}

		std::uint32_t flags = 0;
		if (elementData == nullptr ||
			!ReadField(elementData, kElementFlagsOffset, flags)) {
			++snapshot->unsupportedCount;
			continue;
		}
		if ((flags & kTraceBoxMask) != kTraceBoxMask) {
			continue;
		}

		DlModHitboxV1 hitbox{};
		hitbox.structSize = sizeof(DlModHitboxV1);
		hitbox.elementIndex = index;
		hitbox.flags = flags;
		void* transform = nullptr;
		if (!ReadField(elementData, kElementMinimumOffset, hitbox.localMinimum) ||
			!ReadField(elementData, kElementMaximumOffset, hitbox.localMaximum) ||
			!ReadField(element, kElementTransformOffset, transform) ||
			!IsValidBounds(hitbox.localMinimum, hitbox.localMaximum) ||
			!ReadMatrix(transform, bindings.getWorldMatrix, hitbox.worldMatrix)) {
			++snapshot->unsupportedCount;
			continue;
		}
		if (bindings.getElementName != nullptr) {
			ReadName(controlObject, index, bindings.getElementName, hitbox.name);
		}

		++snapshot->requiredCount;
		if (snapshot->count < capacity) {
			elements[snapshot->count] = hitbox;
			++snapshot->count;
		}
	}

	if (snapshot->requiredCount == 0) {
		return DLMOD_RESULT_NOT_FOUND;
	}
	return snapshot->count == snapshot->requiredCount
		? DLMOD_RESULT_SUCCESS
		: DLMOD_RESULT_PARTIAL_SUCCESS;
#endif
}
