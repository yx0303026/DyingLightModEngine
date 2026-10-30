#include "runtime/EngineCameraRuntime.h"

#include "runtime/EngineEntityBindings.h"

#include <Windows.h>

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace
{
	bool IsFinite(const DlModVector3V1 &value) noexcept
	{
		return std::isfinite(value.x) && std::isfinite(value.y) &&
			   std::isfinite(value.z);
	}

	bool ReadCamera(const EngineEntityBindings &bindings, void *&camera) noexcept
	{
		camera = nullptr;
		const HMODULE game = EngineEntityBindings::GameModule();
		if (game == nullptr || bindings.getActiveLevel == nullptr ||
			bindings.getActiveCamera == nullptr)
		{
			return false;
		}
		std::uintptr_t gameInstance = 0;
		__try
		{
			gameInstance = *reinterpret_cast<const std::uintptr_t *>(
				reinterpret_cast<std::uintptr_t>(game) +
				EngineEntityBindings::GameGlobalRva);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
		if (gameInstance == 0)
			return false;
		__try
		{
			void *level = bindings.getActiveLevel(
				reinterpret_cast<void *>(gameInstance));
			if (level == nullptr)
				return false;
			camera = bindings.getActiveCamera(level);
			return camera != nullptr;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			camera = nullptr;
			return false;
		}
	}

	bool ReadCameraMatrix(const void *camera, float (&matrix)[16]) noexcept
	{
		const EngineEntityBindings *bindings = EngineEntityBindings::Resolve();
		if (bindings == nullptr || bindings->getCameraMatrix == nullptr ||
			camera == nullptr)
		{
			return false;
		}
		__try
		{
			const float *source = bindings->getCameraMatrix(
				const_cast<void *>(camera));
			if (source == nullptr)
				return false;
			for (std::size_t index = 0; index < 16; ++index)
			{
				if (!std::isfinite(source[index]))
					return false;
				matrix[index] = source[index];
			}
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}
}

const DlModCameraApiV1 *EngineCameraRuntime::GetApi() noexcept
{
	static const DlModCameraApiV1 api = {
		sizeof(DlModCameraApiV1),
		MODSDK_ABI_VERSION,
		&ReadPosition,
		&ReadMatrix,
		&Project};
#if defined(_M_X64)
	return &api;
#else
	return nullptr;
#endif
}

DlModResult MODSDK_CALL EngineCameraRuntime::ReadPosition(
	DlModVector3V1 *position) noexcept
{
	if (position == nullptr)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	*position = {};
	const EngineEntityBindings *bindings = EngineEntityBindings::Resolve();
	void *camera = nullptr;
	if (bindings == nullptr || bindings->getCameraPosition == nullptr ||
		!ReadCamera(*bindings, camera))
	{
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	}
	DlModVector3V1 value{};
	__try
	{
		if (bindings->getCameraPosition(camera, &value) != &value ||
			!IsFinite(value))
		{
			return DLMOD_RESULT_NOT_FOUND;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return DLMOD_RESULT_INVALID_STATE;
	}
	*position = value;
	return DLMOD_RESULT_SUCCESS;
}

DlModResult MODSDK_CALL EngineCameraRuntime::ReadMatrix(
	float *matrix,
	uint32_t count) noexcept
{
	if (matrix == nullptr || count < DLMOD_CAMERA_MATRIX_COUNT)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	const EngineEntityBindings *bindings = EngineEntityBindings::Resolve();
	void *camera = nullptr;
	if (bindings == nullptr || bindings->getCameraMatrix == nullptr ||
		!ReadCamera(*bindings, camera))
	{
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	}
	float values[16]{};
	if (!ReadCameraMatrix(camera, values))
		return DLMOD_RESULT_NOT_FOUND;
	for (std::size_t index = 0; index < 16; ++index)
		matrix[index] = values[index];
	return DLMOD_RESULT_SUCCESS;
}

DlModResult MODSDK_CALL EngineCameraRuntime::Project(
	const DlModVector3V1 *world,
	float viewportWidth,
	float viewportHeight,
	float *screenX,
	float *screenY) noexcept
{
	if (world == nullptr || screenX == nullptr || screenY == nullptr ||
		!IsFinite(*world) || !std::isfinite(viewportWidth) ||
		!std::isfinite(viewportHeight) || viewportWidth <= 0.0F ||
		viewportHeight <= 0.0F)
	{
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	*screenX = 0.0F;
	*screenY = 0.0F;

	const EngineEntityBindings *bindings = EngineEntityBindings::Resolve();
	void *camera = nullptr;
	if (bindings == nullptr || bindings->getCameraMatrix == nullptr ||
		!ReadCamera(*bindings, camera))
	{
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	}
	float matrix[16]{};
	if (!ReadCameraMatrix(camera, matrix))
		return DLMOD_RESULT_NOT_FOUND;

	const float clipX = world->x * matrix[0] + world->y * matrix[1] +
						world->z * matrix[2] + matrix[3];
	const float clipY = world->x * matrix[4] + world->y * matrix[5] +
						world->z * matrix[6] + matrix[7];
	const float clipW = world->x * matrix[12] + world->y * matrix[13] +
						world->z * matrix[14] + matrix[15];
	if (!std::isfinite(clipX) || !std::isfinite(clipY) ||
		!std::isfinite(clipW) || clipW < 0.001F)
	{
		return DLMOD_RESULT_NOT_FOUND;
	}
	const float x = (clipX / clipW + 1.0F) * viewportWidth * 0.5F;
	const float y = (1.0F - clipY / clipW) * viewportHeight * 0.5F;
	if (!std::isfinite(x) || !std::isfinite(y))
		return DLMOD_RESULT_INVALID_STATE;
	*screenX = x;
	*screenY = y;
	return DLMOD_RESULT_SUCCESS;
}
