#include "EngineCollisionRuntime.h"

#include <Windows.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

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
	constexpr std::uint32_t kMaximumWireframeLines = 8192;

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

	void ClearExtendedSnapshot(DlModHitboxSnapshotV1& snapshot) noexcept
	{
		snapshot.usesBoxQueries = 0;
		snapshot.wireframeRequiredCount = 0;
		snapshot.wireframeCount = 0;
		snapshot.shapeCount = 0;
		snapshot.wireframeTruncated = 0;
	}

	DlModVector3V1 TransformPoint(
		const float (&matrix)[12],
		const DlModVector3V1& point) noexcept
	{
		return {
			matrix[0] * point.x + matrix[1] * point.y +
				matrix[2] * point.z + matrix[3],
			matrix[4] * point.x + matrix[5] * point.y +
				matrix[6] * point.z + matrix[7],
			matrix[8] * point.x + matrix[9] * point.y +
				matrix[10] * point.z + matrix[11]
		};
	}

	bool AppendWireframe(
		const DlModHitboxV1& hitbox,
		DlModHitboxSnapshotV1& snapshot,
		bool wireframeOutput) noexcept
	{
		DlModVector3V1 corners[8]{};
		for (unsigned int index = 0; index < 8; ++index) {
			corners[index] = TransformPoint(
				hitbox.worldMatrix,
				{
					(index & 1U) != 0U
						? hitbox.localMaximum.x : hitbox.localMinimum.x,
					(index & 2U) != 0U
						? hitbox.localMaximum.y : hitbox.localMinimum.y,
					(index & 4U) != 0U
						? hitbox.localMaximum.z : hitbox.localMinimum.z
				});
			if (!IsFinite(corners[index])) {
				return false;
			}
		}

		if (wireframeOutput) {
			snapshot.wireframeRequiredCount += 12;
			++snapshot.shapeCount;
			for (unsigned int index = 0; index < 8; ++index) {
				for (unsigned int bit = 1; bit <= 4; bit <<= 1) {
					if ((index & bit) == 0U &&
						snapshot.wireframeCount < kMaximumWireframeLines &&
						snapshot.wireframeCount < snapshot.wireframeCapacity &&
						snapshot.wireframeLines != nullptr) {
						snapshot.wireframeLines[snapshot.wireframeCount] = {
							corners[index],
							corners[index | bit]
						};
						++snapshot.wireframeCount;
					}
				}
			}
			if (snapshot.wireframeCount < snapshot.wireframeRequiredCount) {
				snapshot.wireframeTruncated = 1;
			}
		}
		return true;
	}

	void StoreHitbox(
		std::uint8_t* destination,
		std::uint32_t elementSize,
		const DlModHitboxV1& hitbox) noexcept
	{
		const std::size_t copySize =
			elementSize < sizeof(hitbox) ? elementSize : sizeof(hitbox);
		std::memcpy(destination, &hitbox, copySize);
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

	bool CopyRequestedName(
		const char* source,
		char (&destination)[kMaximumElementNameLength]) noexcept
	{
		if (source == nullptr) {
			return false;
		}
		__try {
			for (std::size_t index = 0; index < sizeof(destination); ++index) {
				destination[index] = source[index];
				if (destination[index] == '\0') {
					return index != 0;
				}
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			destination[0] = '\0';
			return false;
		}
		destination[0] = '\0';
		return false;
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
		&CaptureHitboxes,
		&GetHitboxByName
	};
#if !defined(_M_X64)
	return nullptr;
#else
	return &api;
#endif
}

static DlModResult MODSDK_CALL CaptureHitboxesImpl(
	void* controlObject,
	DlModHitboxSnapshotV1* snapshot,
	const char* requestedName) noexcept
{
	if (snapshot == nullptr ||
		snapshot->structSize < DLMOD_HITBOX_SNAPSHOT_V1_BASE_SIZE ||
		snapshot->abiVersion != MODSDK_ABI_VERSION ||
		snapshot->elementSize < DLMOD_HITBOX_V1_BASE_SIZE ||
		(snapshot->capacity != 0 && snapshot->elements == nullptr) ||
		controlObject == nullptr) {
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}

	const std::uint32_t capacity = snapshot->capacity;
	const std::uint32_t elementSize = snapshot->elementSize;
	DlModHitboxV1* elements = snapshot->elements;
	const bool extendedOutput =
		snapshot->structSize >= sizeof(DlModHitboxSnapshotV1);
	const std::uint32_t wireframeCapacity =
		extendedOutput ? snapshot->wireframeCapacity : 0;
	DlModCollisionLineV1* wireframeLines =
		extendedOutput ? snapshot->wireframeLines : nullptr;
	const bool wireframeOutput =
		extendedOutput &&
		(wireframeLines != nullptr || wireframeCapacity != 0);
	if (extendedOutput && wireframeCapacity != 0 && wireframeLines == nullptr) {
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	ClearSnapshot(*snapshot);
	if (extendedOutput) {
		ClearExtendedSnapshot(*snapshot);
	}
	if (extendedOutput) {
		snapshot->wireframeLines = wireframeLines;
	}

#if !defined(_M_X64)
	(void)capacity;
	(void)elementSize;
	(void)elements;
	(void)wireframeCapacity;
	(void)wireframeLines;
	(void)wireframeOutput;
	(void)requestedName;
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
	if (extendedOutput) {
		snapshot->usesBoxQueries = 1;
	}

	const CollisionBindings& bindings = ResolveBindings(engine);
	if (bindings.resolveModelData == nullptr ||
		bindings.resolveElementData == nullptr ||
		bindings.getWorldMatrix == nullptr ||
		(requestedName != nullptr && bindings.getElementName == nullptr)) {
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
		const DlModVector3V1 localCenter = {
			hitbox.localMinimum.x * 0.5f + hitbox.localMaximum.x * 0.5f,
			hitbox.localMinimum.y * 0.5f + hitbox.localMaximum.y * 0.5f,
			hitbox.localMinimum.z * 0.5f + hitbox.localMaximum.z * 0.5f
		};
		hitbox.worldCenter = TransformPoint(hitbox.worldMatrix, localCenter);
		if (!IsFinite(hitbox.worldCenter)) {
			++snapshot->unsupportedCount;
			continue;
		}
		if (bindings.getElementName != nullptr) {
			ReadName(controlObject, index, bindings.getElementName, hitbox.name);
		}
		if (requestedName != nullptr &&
			std::strcmp(hitbox.name, requestedName) != 0) {
			continue;
		}
		if (!AppendWireframe(hitbox, *snapshot, wireframeOutput)) {
			++snapshot->unsupportedCount;
			continue;
		}

		++snapshot->requiredCount;
		if (snapshot->count < capacity) {
			StoreHitbox(
				reinterpret_cast<std::uint8_t*>(elements) +
					static_cast<std::size_t>(snapshot->count) * elementSize,
				elementSize,
				hitbox);
			++snapshot->count;
		}
		if (requestedName != nullptr) {
			return DLMOD_RESULT_SUCCESS;
		}
	}

	if (snapshot->requiredCount == 0) {
		return DLMOD_RESULT_NOT_FOUND;
	}
	return snapshot->count == snapshot->requiredCount &&
		(!wireframeOutput ||
			snapshot->wireframeCount == snapshot->wireframeRequiredCount)
		? DLMOD_RESULT_SUCCESS
		: DLMOD_RESULT_PARTIAL_SUCCESS;
#endif
}

DlModResult MODSDK_CALL EngineCollisionRuntime::CaptureHitboxes(
	void* controlObject,
	DlModHitboxSnapshotV1* snapshot) noexcept
{
	return CaptureHitboxesImpl(controlObject, snapshot, nullptr);
}

DlModResult MODSDK_CALL EngineCollisionRuntime::GetHitboxByName(
	void* controlObject,
	const char* name,
	DlModHitboxV1* hitbox,
	uint32_t hitboxSize) noexcept
{
	char requestedName[kMaximumElementNameLength]{};
	if (hitbox == nullptr ||
		hitboxSize < DLMOD_HITBOX_V1_BASE_SIZE ||
		!CopyRequestedName(name, requestedName)) {
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}

	DlModHitboxSnapshotV1 snapshot{};
	snapshot.structSize = sizeof(snapshot);
	snapshot.abiVersion = MODSDK_ABI_VERSION;
	snapshot.capacity = 1;
	snapshot.elementSize = hitboxSize;
	snapshot.elements = hitbox;
	return CaptureHitboxesImpl(
		controlObject,
		&snapshot,
		requestedName);
}
