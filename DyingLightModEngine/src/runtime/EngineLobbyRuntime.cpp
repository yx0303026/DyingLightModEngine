#include "runtime/EngineLobbyRuntime.h"

#include "runtime/EngineLobbyCollector.h"
#include "runtime/EngineLobbyData.h"

namespace
{
	void *GetControlObject() noexcept
	{
		return static_cast<void *>(&EngineLobbyCollector::Instance());
	}
}

bool EngineLobbyRuntime::Initialize() noexcept
{
	return EngineLobbyCollector::Instance().Initialize();
}

void EngineLobbyRuntime::Shutdown() noexcept
{
	EngineLobbyCollector::Instance().Shutdown();
}

const DlModLobbyApiV1 *EngineLobbyRuntime::GetApi() noexcept
{
	static const DlModLobbyApiV1 api = {
		sizeof(DlModLobbyApiV1),
		MODSDK_ABI_VERSION,
		GetControlObject(),
		&Capture,
		&ClearHistory};
	return &api;
}

void *EngineLobbyRuntime::GetControlObject() noexcept
{
	return ::GetControlObject();
}

DlModResult MODSDK_CALL EngineLobbyRuntime::Capture(
	void *controlObject,
	DlModLobbySnapshotV1 *snapshot) noexcept
{
	if (controlObject != GetControlObject())
		return DLMOD_RESULT_INVALID_ARGUMENT;
	return EngineLobbyData::Instance().Capture(snapshot);
}

DlModResult MODSDK_CALL EngineLobbyRuntime::ClearHistory(void *controlObject) noexcept
{
	if (controlObject != GetControlObject())
		return DLMOD_RESULT_INVALID_ARGUMENT;
	EngineLobbyData::Instance().ClearHistory();
	return DLMOD_RESULT_SUCCESS;
}
