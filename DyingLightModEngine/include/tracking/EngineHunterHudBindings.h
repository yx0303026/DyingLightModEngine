#pragma once

#include <Windows.h>

#include <cstdint>

/* 猎手 HUD 原生绑定概览：封装 HudHunterInfo 更新点与 IUIElement 导入接口。 */
class EngineHunterHudBindings final
{
public:
	struct Vector3
	{
		float x = 0.0F;
		float y = 0.0F;
		float z = 0.0F;
	};

	static constexpr std::uintptr_t HudInfoUpdateSiteRva = 0x566EC0;
	static constexpr std::uintptr_t HudInfoEnergyBarOffset = 0x78;

	static const EngineHunterHudBindings* Resolve() noexcept;
	void* HudInfoUpdateSite() const noexcept;
	bool HasExpectedHudInfoUpdateSignature() const noexcept;
	void* CloneElement(void* source, void* parent) const noexcept;
	void* GetParent(void* element) const noexcept;
	bool GetPosition(void* element, Vector3& position) const noexcept;
	bool GetUnscaledSize(void* element, Vector3& size) const noexcept;
	void SetPosition(void* element, const Vector3& position) const noexcept;
	void SetScale(void* element, const Vector3& scale) const noexcept;
	void SetColor(void* element, const Vector3& color) const noexcept;
	void SetVisible(void* element, bool visible) const noexcept;
	void DestroyRecursive(void* element) const noexcept;

private:
	using CloneElementFunction = void*(__fastcall*)(void*, void*);
	using GetParentFunction = void*(__fastcall*)(void*);
	using GetVectorFunction = void*(__fastcall*)(void*, Vector3*);
	using SetVectorFunction = void(__fastcall*)(void*, const Vector3*);
	using SetVisibleFunction = void(__fastcall*)(void*, bool);
	using DestroyRecursiveFunction = void(__fastcall*)(void*);

	HMODULE gameModule_ = nullptr;
	CloneElementFunction cloneElement_ = nullptr;
	GetParentFunction getParent_ = nullptr;
	GetVectorFunction getPosition_ = nullptr;
	GetVectorFunction getUnscaledSize_ = nullptr;
	SetVectorFunction setPosition_ = nullptr;
	SetVectorFunction setScale_ = nullptr;
	SetVectorFunction setColor_ = nullptr;
	SetVisibleFunction setVisible_ = nullptr;
	DestroyRecursiveFunction destroyRecursive_ = nullptr;
};
