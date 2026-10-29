#pragma once

#include "runtime/ui/NativeUiBackend.h"
#include "runtime/ui/PakUiBackend.h"

namespace dl::engineui {

/* UI 运行时概览：统一选择无 PAK 原生模式或带 PAK 自定义模式。 */
class EngineUiRuntime final {
public:
	EngineUiRuntime() = default;
	~EngineUiRuntime();
	EngineUiRuntime(const EngineUiRuntime&) = delete;
	EngineUiRuntime& operator=(const EngineUiRuntime&) = delete;

	bool Initialize(UiMode mode, const NativeUiResolver& nativeResolver,
		const PakUiResolver& pakResolver) noexcept;
	void Shutdown() noexcept;
	bool IsInitialized() const noexcept;
	UiMode GetMode() const noexcept;
	NativeUiBackend& Native() noexcept;
	PakUiBackend& Pak() noexcept;

private:
	UiMode mode_ = UiMode::Native;
	NativeUiBackend native_;
	PakUiBackend pak_;
	bool initialized_ = false;
};

} // namespace dl::engineui
