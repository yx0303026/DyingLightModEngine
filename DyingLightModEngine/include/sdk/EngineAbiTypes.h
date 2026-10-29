#pragma once

#include <stddef.h>
#include <stdint.h>

/* 基础 ABI 概览：定义调用约定、导出标记、版本、通用结果与共享向量类型。 */
#if defined(_WIN32)
#define MODSDK_CALL __cdecl
#if defined(MODSDK_BUILD)
#define MODSDK_API __declspec(dllexport)
#else
#define MODSDK_API __declspec(dllimport)
#endif
#define MODSDK_MOD_EXPORT __declspec(dllexport)
#else
#define MODSDK_CALL
#define MODSDK_API
#define MODSDK_MOD_EXPORT
#endif

#define MODSDK_ABI_VERSION 1u

#ifdef __cplusplus
extern "C"
{
#endif

	typedef enum DlModResult
	{
		DLMOD_RESULT_SUCCESS = 0,
		DLMOD_RESULT_INVALID_ARGUMENT = 1,
		DLMOD_RESULT_UNSUPPORTED_ABI = 2,
		DLMOD_RESULT_INVALID_STATE = 3,
		DLMOD_RESULT_NOT_FOUND = 4,
		DLMOD_RESULT_IO_ERROR = 5,
		DLMOD_RESULT_PARTIAL_SUCCESS = 6,
		DLMOD_RESULT_UNSUPPORTED_FEATURE = 7
	} DlModResult;

	typedef void(MODSDK_CALL *EngineLogCallback)(void *context, uint32_t level, const char *message);
	typedef struct DlModHostApiV1 DlModHostApiV1;

	typedef struct DlModVector3V1
	{
		float x;
		float y;
		float z;
	} DlModVector3V1;

#ifdef __cplusplus
}
#endif
