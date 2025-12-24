#pragma once
#include "Engine/Core/Base/Base.h"
#include <Engine/Core/Math/NumericConstants.h>
#include <imgui.h>
#include "I18n/I18nManager.h"
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>
#include <string>
#include <cmath>
#include <variant>

/**
 * @file ui_framework.h
 * @brief NPGS Engine - 自定义保留模式UI框架核心
 *
 * 该框架在 ImGui 的基础上实现了一套完整的保留模式 (Retained Mode) UI 系统。
 * 核心设计思想是两阶段布局 (Two-Pass Layout):
 * 1. 测量 (Measure Pass): 从子到父 (Bottom-Up)，计算每个元素期望的尺寸 (Desired Size)。
 * 2. 排列 (Arrange Pass): 从父到子 (Top-Down)，为每个元素分配最终的实际位置和大小。
 *
 * 这种设计提供了强大的布局能力，支持固定尺寸、内容自适应和弹性拉伸等多种模式，
 * 类似于 WPF/XAML, Jetpack Compose, 或 Web 的 Flexbox 布局。
 */

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// --- 前置声明 ---
class UIElement;
class UIContext;
class GlobalTooltip;
class UIRoot;

// =================================================================================
// 1. 对齐与布局 (Alignment & Layout)
// =================================================================================

/**
 * @enum Alignment
 * @brief 交叉轴对齐方式。
 * 仅在当前元素尺寸小于容器在交叉轴上分配给它的空间时生效。
 * 例如，在一个VBox（主轴是垂直）中，它决定了元素的水平对齐。
 */
enum class Alignment { Start, Center, End };

/**
 * @enum LengthType
 * @brief 定义元素在某个轴上的尺寸计算方式。
 */
enum class LengthType
{
    Fixed,    // 固定像素值
    Content,  // 尺寸由内容（例如子元素）决定
    Stretch   // 按权重拉伸以填充父容器的剩余空间
};

/**
 * @enum AnchorPoint
 * @brief 锚点，用于将元素钉在父容器（通常是UIRoot）的特定位置。
 */
enum class AnchorPoint
{
    None,         // 默认值，参与正常的流式布局（例如在VBox/HBox中排列，或被拉伸）
    TopLeft, TopCenter, TopRight,
    MiddleLeft, Center, MiddleRight,
    BottomLeft, BottomCenter, BottomRight
};

/**
 * @struct Length
 * @brief 描述一个长度值，可以是固定的像素，也可以是弹性的权重。
 */
struct Length
{
    LengthType type = LengthType::Fixed;
    float value = 0.0f; // type=Fixed时为像素值；type=Stretch时为权重。

    static Length Fixed(float px) { return { LengthType::Fixed, px }; }
    static Length Content() { return { LengthType::Content, 0.0f }; }
    static Length Stretch(float weight = 1.0f) { return { LengthType::Stretch, weight }; }

    // 构造函数
    Length(float px = 0.0f) : type(LengthType::Fixed), value(px) {}
    Length(LengthType t, float v) : type(t), value(v) {}

    // 便捷检查函数
    bool IsFixed() const { return type == LengthType::Fixed; }
    bool IsContent() const { return type == LengthType::Content; }
    bool IsStretch() const { return type == LengthType::Stretch; }
};


// =================================================================================
// 2. 统一主题管理 (Theming)
// =================================================================================

enum class ThemeColorID
{
    None, Text, TextHighlight, TextDisabled, Border, Accent, Custom
};

struct UITheme
{
    // 基础颜色定义
    ImVec4 color_text = { 0.8f, 0.8f, 0.8f, 1.0f };
    ImVec4 color_text_highlight = { 1.0f, 1.0f, 1.0f, 1.0f };
    ImVec4 color_text_disabled = { 0.5f, 0.5f, 0.5f, 1.0f };
    ImVec4 color_border = { 0.5f, 0.5f, 0.5f, 1.0f };
    ImVec4 color_accent = ImVec4(0.0f, 1.0f, 1.0f, 1.0f); // 当前选用的主题色：青色
};

/**
 * @struct StyleColor
 * @brief 描述一个样式颜色，可以是主题预设，也可以是自定义颜色。
 */
struct StyleColor
{
    ThemeColorID id = ThemeColorID::None;
    ImVec4 custom_value = { 0,0,0,0 };
    float alpha_override = -1.0f; // -1 表示不覆盖 alpha

    StyleColor() : id(ThemeColorID::None) {}
    StyleColor(ThemeColorID _id) : id(_id) {}
    StyleColor(const ImVec4& col) : id(ThemeColorID::Custom), custom_value(col) {}

    // 返回一个修改了Alpha通道的新StyleColor实例
    StyleColor WithAlpha(float alpha) const
    {
        StyleColor new_color = *this;
        new_color.alpha_override = alpha;
        return new_color;
    }

    // 从UIContext解析出最终的ImVec4颜色值
    ImVec4 Resolve() const;
    bool HasValue() const { return id != ThemeColorID::None; }
};


// =================================================================================
// 3. UI 上下文 (UI Context)
// =================================================================================

/**
 * @class UIContext
 * @brief 全局UI上下文，管理主题、字体、输入状态和全局资源。
 */
class UIContext
{
public:
    static UIContext& Get() { static UIContext instance; return instance; }

    // 全局状态
    bool m_input_blocked = false;        // 外部逻辑（如相机旋转）是否阻塞UI输入
    UIElement* m_focused_element = nullptr;  // 当前拥有键盘焦点的元素
    UIElement* m_captured_element = nullptr; // 当前捕获鼠标输入的元素（例如，按钮被按下时）

    // Tooltip 相关
    std::string m_tooltip_candidate_key;      // 当前鼠标悬停元素的tooltip key
    std::string m_tooltip_active_key;         // 经延迟后确认要显示的tooltip key
    std::string m_tooltip_previous_candidate_key;
    float m_tooltip_timer = 0.0f;

    // 主题与字体
    UITheme m_theme;
    ImFont* m_font_regular = nullptr;
    ImFont* m_font_bold = nullptr;
    ImFont* m_font_large = nullptr;
    ImFont* m_font_small = nullptr;
    ImFont* m_font_subtitle = nullptr;

    // 全局资源
    ImTextureID m_scene_blur_texture = 0; // 用于毛玻璃效果的背景模糊贴图
    ImVec2 m_display_size = { 1920, 1080 };

    void NewFrame();

    void SetInputBlocked(bool blocked) { m_input_blocked = blocked; }
    void RequestTooltip(const std::string& key);
    void UpdateTooltipLogic(float dt);
    ImVec4 GetThemeColor(ThemeColorID id) const;
    void SetFocus(UIElement* element);
    void ClearFocus();
    void SetCapture(UIElement* element);
    void ReleaseCapture();
    bool IsCapturing() const { return m_captured_element != nullptr; }

private:
    UIContext() = default; // 单例模式
    UIContext(const UIContext&) = delete;
    UIContext& operator=(const UIContext&) = delete;
};


// =================================================================================
// 4. 基础工具 (Utilities)
// =================================================================================

struct Rect
{
    float x, y, w, h;
    bool Contains(const ImVec2& p) const;
};

enum class EasingType { Linear, EaseInQuad, EaseOutQuad, EaseInOutQuad, EaseOutBack };

class AnimationUtils
{
public:
    static float Ease(float t, EasingType type);
};


// =================================================================================
// 5. 核心元素基类 (Core Base Class)
// =================================================================================

class UIElement : public std::enable_shared_from_this<UIElement>
{
public:
    using Ptr = std::shared_ptr<UIElement>;
    using TweenCallback = std::function<void()>;

    // --- 事件回调 ---
    std::function<void()> on_click;

    // --- 基础属性 ---
    Rect m_rect = { 0, 0, 100, 100 };   // 最终由Arrange阶段计算出的、相对于父元素的矩形区域
    ImVec2 m_absolute_pos = { 0, 0 }; // 最终的屏幕绝对坐标
    bool m_visible = true;
    float m_alpha = 1.0f;

    // --- ID 与命名 ---
private:
    std::string m_name = "";          // 元素的具名标识，用于构成ID
public:
    std::string m_cached_id;          // 缓存的完整ID路径（如 "Menu.Buttons.Confirm"）
    bool m_id_dirty = true;           // ID缓存是否需要重建
    std::string m_tooltip_key;

    // --- 布局属性 ---
    Length m_width = Length::Stretch(1.0f);
    Length m_height = Length::Stretch(1.0f);
    Alignment m_align_h = Alignment::Start;
    Alignment m_align_v = Alignment::Start;
    AnchorPoint m_anchor = AnchorPoint::None;
    ImVec2 m_margin = { 0.0f, 0.0f }; // 配合Anchor使用，提供边缘的偏移

    // --- 缓存的测量结果 ---
    ImVec2 m_desired_size = { 0, 0 }; // 在Measure阶段计算出的期望尺寸

    // --- 交互属性 ---
    bool m_block_input = true;      // 是否阻挡鼠标事件传递给下层元素
    bool m_focusable = false;       // 是否可接收键盘焦点
    bool m_hovered = false;         // 鼠标是否悬停
    bool m_clicked = false;         // 鼠标是否按下（在释放前都为true）

    // --- 资源与层级 ---
    ImFont* m_font = nullptr;
    UIRoot* m_root = nullptr;
    UIElement* m_parent = nullptr;
    std::vector<Ptr> m_children;

    // --- 动画系统 ---
    struct Tween
    {
        float* target;
        float start, end, current_time, duration;
        EasingType easing;
        bool active;
        TweenCallback on_complete;
    };
    std::vector<Tween> m_tweens;

    virtual ~UIElement() = default;

    // --- 核心流程虚函数 ---

    // [MODIFIED] 仅更新动画和自身逻辑状态，不涉及布局
    virtual void Update(float dt);

    /**
     * @brief [布局流程-阶段1] 测量 (Measure)
     * @param available_size 父容器提供的可用空间。子元素应根据此空间计算自己的期望尺寸。
     * @return ImVec2 自身计算出的期望尺寸 (Desired Size)。
     * 这是一个自下而上 (Bottom-Up) 的过程。
     */
    virtual ImVec2 Measure(ImVec2 available_size);

    /**
     * @brief [布局流程-阶段2] 排列 (Arrange)
     * @param final_rect 父容器分配给该元素的、相对于父容器左上角的最终矩形区域。
     * 元素需在此区域内确定自己的位置和大小，并继续为自己的子元素调用Arrange。
     * 这是一个自上而下 (Top-Down) 的过程。
     */
    virtual void Arrange(const Rect& final_rect);

    /**
     * @brief [绘制流程] 使用ImGui的DrawList进行绘制。
     * @param draw_list ImGui的绘制列表。
     */
    virtual void Draw(ImDrawList* draw_list);

    // --- 事件处理虚函数 ---
    virtual void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& external_handled);
    virtual bool HandleKeyboardEvent();

    // --- API ---

    // ID 和命名
    void SetName(const std::string& name);
    const std::string& GetName() const { return m_name; }
    std::string& GetID();
    void InvalidateIDCache();

    // 层级管理
    void AddChild(Ptr child);
    void RemoveChild(Ptr child);
    virtual void ResetInteraction();

    // 功能性链式调用API
    UIElement* SetAnchor(AnchorPoint anchor, const ImVec2& margin = { 0.0f, 0.0f })
    {
        m_anchor = anchor;
        m_margin = margin;
        return this;
    }
    UIElement* SetTooltip(const std::string& key) { m_tooltip_key = key; return this; }

    // 动画
    void To(float* property, float target, float duration, EasingType easing = EasingType::EaseOutQuad, TweenCallback on_complete = nullptr);

    // 绘制工具
    void DrawGlassBackground(ImDrawList* draw_list, const ImVec2& p_min, const ImVec2& p_max, const ImVec4& BackCol = ImVec4(0.0, 0.0, 0.0, 0.6));
    ImU32 GetColorWithAlpha(const ImVec4& col, float global_alpha) const;

    // Getters & Setters
    virtual ImFont* GetFont() const;
    virtual void OnClick();
    bool IsFocused() const;
    void RequestFocus();
    void CaptureMouse();
    void ReleaseMouse();

protected:
    // 仅更新自身状态，不递归子节点
    virtual void UpdateSelf(float dt);
};


// =================================================================================
// 6. 容器组件 (Container Components)
// =================================================================================

class Panel : public UIElement
{
public:
    StyleColor m_bg_color;
    bool m_use_glass_effect = false;

    void Draw(ImDrawList* draw_list) override;
};

class VBox : public UIElement
{
public:
    float m_padding = 10.0f; // 子元素之间的垂直间距

    ImVec2 Measure(ImVec2 available_size) override;
    void Arrange(const Rect& final_rect) override;
};

class HBox : public UIElement
{
public:
    float m_padding = 10.0f; // 子元素之间的水平间距

    ImVec2 Measure(ImVec2 available_size) override;
    void Arrange(const Rect& final_rect) override;
};

class ScrollView : public UIElement
{
public:
    float m_scroll_y = 0.0f;
    float m_target_scroll_y = 0.0f;
    float m_content_height = 0.0f;
    float m_scroll_speed = 20.0f;
    float m_smoothing_speed = 15.0f;
    bool m_show_scrollbar = true;

    void Update(float dt) override;
    ImVec2 Measure(ImVec2 available_size) override;
    void Arrange(const Rect& final_rect) override;
    void Draw(ImDrawList* draw_list) override;
    void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& external_handled) override;
};

class HorizontalScrollView : public UIElement
{
public:
    float m_scroll_x = 0.0f;
    float m_target_scroll_x = 0.0f;
    float m_content_width = 0.0f;
    float m_scroll_speed = 20.0f;
    float m_smoothing_speed = 15.0f;
    bool m_show_scrollbar = false;

    void Update(float dt) override;
    ImVec2 Measure(ImVec2 available_size) override;
    void Arrange(const Rect& final_rect) override;
    void Draw(ImDrawList* draw_list) override;
    void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& external_handled) override;
};


// =================================================================================
// 7. 基础控件 (Basic Controls)
// =================================================================================

class Image : public UIElement
{
public:
    ImTextureID m_texture_id = 0;
    ImVec2 m_uv0 = { 0, 0 };
    ImVec2 m_uv1 = { 1, 1 };
    ImVec4 m_tint_col = { 1, 1, 1, 1 };

    float m_aspect_ratio = 1.0f; // 宽高比 (宽度 / 高度)

    Image(ImTextureID tex_id) : m_texture_id(tex_id) { m_block_input = false; }
    void SetOriginalSize(float w, float h)
    {
        if (h > 0.0f) m_aspect_ratio = w / h;
    }

    // Image需要重写布局方法以保持宽高比
    ImVec2 Measure(ImVec2 available_size) override;
    void Arrange(const Rect& final_rect) override;
    void Draw(ImDrawList* draw_list) override;
};


// =================================================================================
// 8. 根元素 (Root Element)
// =================================================================================

/**
 * @class UIRoot
 * @brief UI元素树的根节点，驱动整个UI的更新、布局和绘制流程。
 * 同时管理ID到元素的快速查找映射表。
 *
 * ID系统使用示例:
 * // 在某个UI逻辑函数中
 * void GameScreen::SomeLogicFunction()
 * {
 *     // 通过ID查找一个按钮
 *     if (auto* rkkv_button = m_ui_root->FindElementAs<PulsarButton>("rkkvButton"))
 *     {
 *         pulsar_btn->SetStatus("i18ntext.ui.status.reloading");
 *     }
 *
 *     // 甚至可以找到复合组件内部的子部件
 *     if (auto* rkkv_label = m_ui_root->FindElementAs<TechText>("rkkvButton.label"))
 *     {
 *         tech_text->SetColor(ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
 *     }
 * }
 */
class UIRoot : public UIElement
{
public:
    UIRoot();

    // --- ID系统 API ---
    UIElement* FindElementByID(const std::string& id);
    template<typename T>
    T* FindElementAs(const std::string& id)
    {
        return dynamic_cast<T*>(FindElementByID(id));
    }
    void MarkIDMapDirty();

    // --- 核心流程覆盖 ---
    void Arrange(const Rect& final_rect) override;
    void Update(float dt) override;
    void Draw(ImDrawList* draw_list) override;
    void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& external_handled) override;

    // --- 便捷入口 ---
    void Draw();

private:
    std::shared_ptr<GlobalTooltip> m_tooltip;
    std::unordered_map<std::string, UIElement*> m_id_map;
    bool m_id_map_dirty = true;

    void RebuildIDMap();
    void BuildIDMapRecursive(UIElement* element);
};

_UI_END
_SYSTEM_END
_NPGS_END