#include "dl_imgui/ui.h"
#include "dl_imgui/style.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dl::imgui {
	namespace {

		class StyleScope {
		public:
			explicit StyleScope(const Style& style)
			{
				if (style.hasTextColor) {
					ImGui::PushStyleColor(ImGuiCol_Text, style.textColor);
					++colorCount_;
				}
				if (style.hasWindowBackground) {
					ImGui::PushStyleColor(ImGuiCol_WindowBg, style.windowBackground);
					++colorCount_;
				}
				if (style.hasFrameBackground) {
					ImGui::PushStyleColor(ImGuiCol_FrameBg, style.frameBackground);
					++colorCount_;
				}
				if (style.hasButtonColor) {
					ImGui::PushStyleColor(ImGuiCol_Button, style.buttonColor);
					++colorCount_;
				}
				if (style.hasButtonHoveredColor) {
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.buttonHoveredColor);
					++colorCount_;
				}
				if (style.hasButtonActiveColor) {
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, style.buttonActiveColor);
					++colorCount_;
				}
				if (style.frameRounding >= 0.0F) {
					ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ScaleSize(style.frameRounding));
					++styleCount_;
				}
				if (style.itemSpacing >= 0.0F) {
					const float spacing = ScaleSize(style.itemSpacing);
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));
					++styleCount_;
				}
			}

			~StyleScope()
			{
				ImGui::PopStyleVar(styleCount_);
				ImGui::PopStyleColor(colorCount_);
			}

		private:
			int colorCount_ = 0;
			int styleCount_ = 0;
		};

		void RenderMargin(const Thickness& margin)
		{
			if (margin.left > 0.0F || margin.top > 0.0F) {
				ImGui::Dummy(ImVec2(margin.left, margin.top));
			}
		}

		void RenderTrailingMargin(const Thickness& margin)
		{
			if (margin.right > 0.0F || margin.bottom > 0.0F) {
				ImGui::Dummy(ImVec2(margin.right, margin.bottom));
			}
		}

	} // namespace

	float GetScaleFactor() noexcept
	{
		return ImGui::GetCurrentContext() == nullptr ? 1.0F : ImGui::GetIO().FontGlobalScale;
	}

	float ScaleSize(float value) noexcept
	{
		return value < 0.0F ? value : value * GetScaleFactor();
	}

	ImVec2 ScaleSize(const ImVec2& value) noexcept
	{
		return ImVec2(ScaleSize(value.x), ScaleSize(value.y));
	}

	UIElement::~UIElement() = default;

	void UIElement::Render()
	{
		if (visibility_ == Visibility::Collapsed) {
			return;
		}

		ImGui::PushID(id_.empty() ? static_cast<const void*>(this) : id_.c_str());
		StyleScope styleScope(style_);
		const float scale = GetScaleFactor();
		const Thickness scaledMargin{
			layout_.margin.left * scale,
			layout_.margin.top * scale,
			layout_.margin.right * scale,
			layout_.margin.bottom * scale
		};
		RenderMargin(scaledMargin);
		if (visibility_ == Visibility::Visible) {
			OnRender();
		} else {
			ImGui::Dummy(ResolveSize(ImVec2(0.0F, 0.0F)));
		}
		RenderTrailingMargin(scaledMargin);
		ImGui::PopID();
	}

	Visibility UIElement::GetVisibility() const noexcept
	{
		return visibility_;
	}

	void UIElement::SetVisibility(Visibility visibility) noexcept
	{
		visibility_ = visibility;
	}

	const std::string& UIElement::GetId() const noexcept
	{
		return id_;
	}

	void UIElement::SetId(std::string id)
	{
		id_ = std::move(id);
	}

	const LayoutOptions& UIElement::GetLayout() const noexcept
	{
		return layout_;
	}

	void UIElement::SetLayout(const LayoutOptions& layout)
	{
		layout_ = layout;
	}

	const Style& UIElement::GetStyle() const noexcept
	{
		return style_;
	}

	void UIElement::SetStyle(const Style& style)
	{
		style_ = style;
	}

	void UIElement::RenderChild(UIElement& child)
	{
		child.Render();
	}

	ImVec2 UIElement::ResolveSize(const ImVec2& fallback) const noexcept
	{
		return ImVec2(
			ScaleSize(layout_.width >= 0.0F ? layout_.width : fallback.x),
			ScaleSize(layout_.height >= 0.0F ? layout_.height : fallback.y));
	}

	UIElement& Component::AddChild(std::unique_ptr<UIElement> child)
	{
		if (child == nullptr) {
			throw std::invalid_argument("Component child cannot be null");
		}
		UIElement& reference = *child;
		children_.push_back(std::move(child));
		return reference;
	}

	void Component::RemoveAllChildren()
	{
		children_.clear();
	}

	const std::vector<std::unique_ptr<UIElement>>& Component::GetChildren() const noexcept
	{
		return children_;
	}

	void Component::OnRender()
	{
		for (const auto& child : children_) {
			RenderChild(*child);
		}
	}

	std::vector<std::unique_ptr<UIElement>>& Component::Children() noexcept
	{
		return children_;
	}

	void Panel::OnRender()
	{
		Component::OnRender();
	}

	UiContext::~UiContext()
	{
		Destroy();
	}

	bool UiContext::Create()
	{
		if (context_ != nullptr) {
			return false;
		}
		context_ = ImGui::CreateContext();
		if (context_ != nullptr) {
			ImGui::SetCurrentContext(context_);
			ApplyDefaultStyle();
		}
		return context_ != nullptr;
	}

	void UiContext::Destroy() noexcept
	{
		if (context_ != nullptr) {
			ImGui::DestroyContext(context_);
			context_ = nullptr;
		}
		mouseProviderActive_ = false;
		appliedScale_ = 1.0F;
	}

	bool UiContext::IsCreated() const noexcept
	{
		return context_ != nullptr;
	}

	ImGuiContext* UiContext::GetNativeContext() const noexcept
	{
		return context_;
	}

	float UiContext::GetScale() const noexcept
	{
		return scale_;
	}

	void UiContext::SetScale(float scale) noexcept
	{
		if (!std::isfinite(scale)) {
			scale_ = 1.0F;
			return;
		}
		scale_ = std::clamp(scale, 0.1F, 8.0F);
	}

	void UiContext::SetMouseStateProvider(ModEngineGetUiMouseStateFn provider) noexcept
	{
		mouseStateProvider_ = provider;
		mouseProviderActive_ = false;
	}

	bool UiContext::IsMouseProviderActive() const noexcept
	{
		return mouseProviderActive_;
	}

	void UiContext::ApplyMouseState() noexcept
	{
		mouseProviderActive_ = false;
		if (mouseStateProvider_ == nullptr) {
			return;
		}

		DlModUiMouseStateV1 state{};
		state.structSize = sizeof(state);
		state.abiVersion = MODSDK_ABI_VERSION;
		if (mouseStateProvider_(&state) == 0U) {
			ImGui::GetIO().MouseDrawCursor = false;
			return;
		}

		ImGuiIO& io = ImGui::GetIO();
		io.MouseDrawCursor = true;
		if ((state.flags & DLMOD_UI_MOUSE_POSITION_VALID) != 0U) {
			io.AddMousePosEvent(
				static_cast<float>(state.positionX),
				static_cast<float>(state.positionY));
		}
		for (int button = 0; button < 5; ++button) {
			io.AddMouseButtonEvent(button, (state.buttons & (1U << button)) != 0U);
		}
		if (state.wheelDelta != 0.0F) {
			io.AddMouseWheelEvent(0.0F, state.wheelDelta);
		}
		mouseProviderActive_ = true;
	}

	void UiContext::BeginFrame()
	{
		if (context_ != nullptr) {
			ImGui::SetCurrentContext(context_);
			if (scale_ != appliedScale_) {
				ImGui::GetStyle().ScaleAllSizes(scale_ / appliedScale_);
				ImGui::GetIO().FontGlobalScale = scale_;
				appliedScale_ = scale_;
			}
			ApplyMouseState();
			ImGui::NewFrame();
		}
	}

	void UiContext::Render(UIElement& root)
	{
		if (context_ != nullptr) {
			ImGui::SetCurrentContext(context_);
			root.Render();
		}
	}

	void UiContext::EndFrame()
	{
		if (context_ != nullptr) {
			ImGui::SetCurrentContext(context_);
			ImGui::Render();
		}
	}

} // namespace dl::imgui
