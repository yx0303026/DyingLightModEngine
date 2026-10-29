#pragma once

#include "sdk/EngineUiInputAbi.h"

#include <imgui.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace dl::imgui {

	enum class Visibility {
		Visible,
		Hidden,
		Collapsed
	};

	enum class Orientation {
		Vertical,
		Horizontal
	};

	struct Thickness {
		float left = 0.0F;
		float top = 0.0F;
		float right = 0.0F;
		float bottom = 0.0F;
	};

	struct LayoutOptions {
		float width = -1.0F;
		float height = -1.0F;
		Thickness margin{};
	};

	struct Style {
		bool hasTextColor = false;
		ImVec4 textColor{};
		bool hasWindowBackground = false;
		ImVec4 windowBackground{};
		bool hasFrameBackground = false;
		ImVec4 frameBackground{};
		bool hasButtonColor = false;
		ImVec4 buttonColor{};
		bool hasButtonHoveredColor = false;
		ImVec4 buttonHoveredColor{};
		bool hasButtonActiveColor = false;
		ImVec4 buttonActiveColor{};
		float frameRounding = -1.0F;
		float itemSpacing = -1.0F;
	};

	float GetScaleFactor() noexcept;
	float ScaleSize(float value) noexcept;
	ImVec2 ScaleSize(const ImVec2& value) noexcept;

	class UIElement {
	public:
		virtual ~UIElement();

		void Render();

		Visibility GetVisibility() const noexcept;
		void SetVisibility(Visibility visibility) noexcept;
		const std::string& GetId() const noexcept;
		void SetId(std::string id);
		const LayoutOptions& GetLayout() const noexcept;
		void SetLayout(const LayoutOptions& layout);
		const Style& GetStyle() const noexcept;
		void SetStyle(const Style& style);

	protected:
		UIElement() = default;
		UIElement(const UIElement&) = delete;
		UIElement& operator=(const UIElement&) = delete;

		virtual void OnRender() = 0;
		void RenderChild(UIElement& child);
		ImVec2 ResolveSize(const ImVec2& fallback) const noexcept;

	private:
		Visibility visibility_ = Visibility::Visible;
		std::string id_;
		LayoutOptions layout_{};
		Style style_{};
	};

	class Component : public UIElement {
	public:
		UIElement& AddChild(std::unique_ptr<UIElement> child);

		template <typename T, typename... Args>
		T& AddChild(Args&&... args)
		{
			auto child = std::make_unique<T>(std::forward<Args>(args)...);
			T& reference = *child;
			AddChild(std::move(child));
			return reference;
		}

		void RemoveAllChildren();
		const std::vector<std::unique_ptr<UIElement>>& GetChildren() const noexcept;

	protected:
		void OnRender() override;
		std::vector<std::unique_ptr<UIElement>>& Children() noexcept;

	private:
		std::vector<std::unique_ptr<UIElement>> children_;
	};

	class Panel : public Component {
	protected:
		void OnRender() override;
	};

	class UiContext {
	public:
		UiContext() = default;
		~UiContext();
		UiContext(const UiContext&) = delete;
		UiContext& operator=(const UiContext&) = delete;

		bool Create();
		void Destroy() noexcept;
		bool IsCreated() const noexcept;
		ImGuiContext* GetNativeContext() const noexcept;
		float GetScale() const noexcept;
		void SetScale(float scale) noexcept;
		void SetMouseStateProvider(ModEngineGetUiMouseStateFn provider) noexcept;
		bool IsMouseProviderActive() const noexcept;
		void BeginFrame();
		void Render(UIElement& root);
		void EndFrame();

	private:
		void ApplyMouseState() noexcept;
		void ApplyScale() noexcept;

		ImGuiContext* context_ = nullptr;
		ModEngineGetUiMouseStateFn mouseStateProvider_ = nullptr;
		bool mouseProviderActive_ = false;
		float scale_ = 1.0F;
		float appliedScale_ = 1.0F;
		ImGuiStyle baseStyle_{};
	};

} // namespace dl::imgui
