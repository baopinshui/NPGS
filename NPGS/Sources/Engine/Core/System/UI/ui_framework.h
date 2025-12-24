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

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// --- 前置声明 ---
class UIElement;
class UIContext;
class GlobalTooltip;
class UIRoot;
// --- 1. 对齐与布局 ---

// [NEW] 仅在当前元素尺寸小于容器分配的交叉轴尺寸时生效
enum class Alignment { Start, Center, End };

enum class LengthType { Fixed, Content, Stretch };

enum class AnchorPoint
{
    None, // 默认，参与正常流式布局 (例如拉伸)
    TopLeft, TopCenter, TopRight,
    MiddleLeft, Center, MiddleRight,
    BottomLeft, BottomCenter, BottomRight
};

struct Length
{
    LengthType type = LengthType::Fixed;
    float value = 0.0f; // Fixed时为像素值，Stretch时为权重

    static Length Fixed(float px) { return { LengthType::Fixed, px }; }
    static Length Content() { return { LengthType::Content, 0.0f }; }
    static Length Stretch(float weight = 1.0f) { return { LengthType::Stretch, weight }; }

    Length(float px = 0.0f) : type(LengthType::Fixed), value(px) {}
    Length(LengthType t, float v) : type(t), value(v) {}

    bool IsFixed() const { return type == LengthType::Fixed; }
    bool IsContent() const { return type == LengthType::Content; }
    bool IsStretch() const { return type == LengthType::Stretch; }
};

// --- 2. 统一主题管理 (保持不变) ---
enum class ThemeColorID
{
    None, Text, TextHighlight, TextDisabled, Border, Accent, Custom
};

struct UITheme
{
    ImVec4 color_text = { 0.8f, 0.8f, 0.8f, 1.0f };
    ImVec4 color_text_highlight = { 1.0f, 1.0f, 1.0f, 1.0f };
    ImVec4 color_text_disabled = { 0.5f, 0.5f, 0.5f, 1.0f };
    ImVec4 color_border = { 0.5f, 0.5f, 0.5f, 1.0f };
    //NPGS-k0.73-001
    //ImVec4 color_accent = { 30.0f / 255.0f, 114.0f / 255.0f, 232.0f / 255.0f, 1.0f }; // korvo的提案    蓝
    //ImVec4 color_accent = ImVec4(0.845f, 0.845f, 0.561f, 1.0f);                       // baopinsui的提案1 米黄
    //ImVec4 color_accent = ImVec4(0.0f,1.0f,1.0f, 1.0f);                               // baopinsui的提案2 青
    //ImVec4 color_accent = ImVec4(1.0f,0.25f,0.0f, 1.0f);                              // dagger的提案1  亮红
    //ImVec4 color_accent = ImVec4(0.98f,0.14f,0.24f, 1.0f);                            //dagger的提案2   还是亮红
    //ImVec4 color_accent = ImVec4(0.0f,1.0f,0.0f, 1.0f);                               //edlsdpsy的提案  绿

    ImVec4 color_accent = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);
};

struct StyleColor
{
    ThemeColorID id = ThemeColorID::None;
    ImVec4 custom_value = { 0,0,0,0 };
    float alpha_override = -1.0f;

    StyleColor() : id(ThemeColorID::None) {}
    StyleColor(ThemeColorID _id) : id(_id) {}
    StyleColor(const ImVec4& col) : id(ThemeColorID::Custom), custom_value(col) {}

    StyleColor WithAlpha(float alpha) const
    {
        StyleColor new_color = *this;
        new_color.alpha_override = alpha;
        return new_color;
    }
    ImVec4 Resolve() const;
    bool HasValue() const { return id != ThemeColorID::None; }
};

// --- 3. UI 上下文 (保持不变) ---
class UIContext
{
public:
    static UIContext& Get() { static UIContext instance; return instance; }

    bool m_input_blocked = false;
    UIElement* m_focused_element = nullptr;
    UIElement* m_captured_element = nullptr;
    std::string m_tooltip_candidate_key;
    std::string m_tooltip_active_key;
    std::string m_tooltip_previous_candidate_key;
    float m_tooltip_timer = 0.0f;

    UITheme m_theme;
    ImFont* m_font_regular = nullptr;
    ImFont* m_font_bold = nullptr;
    ImFont* m_font_large = nullptr;
    ImFont* m_font_small = nullptr;
    ImFont* m_font_subtitle = nullptr;

    ImTextureID m_scene_blur_texture = 0;
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
};

// --- 基础工具 ---
struct Rect
{
    float x, y, w, h;
    bool Contains(const ImVec2& p) const; // 实现移至cpp
};

enum class EasingType { Linear, EaseInQuad, EaseOutQuad, EaseInOutQuad, EaseOutBack };

class AnimationUtils
{
public:
    static float Ease(float t, EasingType type);
};

// --- 核心元素基类 ---
class UIElement : public std::enable_shared_from_this<UIElement>
{
public:
    using Ptr = std::shared_ptr<UIElement>;
    using TweenCallback = std::function<void()>;

    std::function<void()> on_click;

    // 基础属性
    Rect m_rect = { 0, 0, 100, 100 };
    ImVec2 m_absolute_pos = { 0, 0 };
    bool m_visible = true;
    float m_alpha = 1.0f;
private:
    std::string m_name="";

public:
    std::string m_cached_id;
    bool m_id_dirty = true;
   

    std::string m_tooltip_key;

    // [NEW] 布局属性
    Length m_width = Length::Stretch(1.0f);  // 默认宽撑满
    Length m_height = Length::Stretch(1.0f); // 默认高撑满

    // 对齐方式 (当父容器给予的空间 > 自身大小时，决定自身在交叉轴的位置)
    Alignment m_align_h = Alignment::Start;
    Alignment m_align_v = Alignment::Start;

    AnchorPoint m_anchor = AnchorPoint::None;
    ImVec2 m_margin = { 0.0f, 0.0f }; // X: 水平边距, Y: 垂直边距

    // [NEW] 缓存的测量结果
    ImVec2 m_desired_size = { 0, 0 };

    // 交互属性
    bool m_block_input = true;
    bool m_focusable = false;
    bool m_hovered = false;
    bool m_clicked = false;

    // 资源
    ImFont* m_font = nullptr;

    UIRoot* m_root = nullptr;
    UIElement* m_parent = nullptr;
    std::vector<Ptr> m_children;

    // 动画相关
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


    virtual void UpdateSelf(float dt);

    // [MODIFIED] Update 不再负责布局计算，只负责动画和状态更新
    virtual void Update(float dt);

    // [NEW] 测量阶段：计算 DesiredSize (Bottom-Up)
    // available_size: 父容器能提供的最大空间 (如果是无限大，用 FLT_MAX)
    virtual ImVec2 Measure(ImVec2 available_size);

    // [NEW] 排列阶段：确定最终位置和大小 (Top-Down)
    // final_rect: 父容器分配给该元素的最终区域 (相对父容器坐标)
    virtual void Arrange(const Rect& final_rect);

    virtual void Draw(ImDrawList* draw_list);

    // 事件处理
    virtual void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& external_handled);
    virtual bool HandleKeyboardEvent();
    // ID 和命名管理 API
    void SetName(const std::string& name);
    const std::string& GetName() const { return m_name; }
    std::string& GetID();
    void InvalidateIDCache();
    // 层级管理
    void AddChild(Ptr child);
    void RemoveChild(Ptr child);
    virtual void ResetInteraction();

    // 功能 API
    UIElement* SetAnchor(AnchorPoint anchor, const ImVec2& margin = { 0.0f, 0.0f })
    {
        m_anchor = anchor;
        m_margin = margin;
        return this;
    }
    UIElement* SetTooltip(const std::string& key) { m_tooltip_key = key; return this; }
    void DrawGlassBackground(ImDrawList* draw_list, const ImVec2& p_min, const ImVec2& p_max, const ImVec4& BackCol = ImVec4(0.0, 0.0, 0.0, 0.6));
    void To(float* property, float target, float duration, EasingType easing = EasingType::EaseOutQuad, TweenCallback on_complete = nullptr);
    ImU32 GetColorWithAlpha(const ImVec4& col, float global_alpha) const;

    virtual ImFont* GetFont() const;
    virtual void OnClick();
    bool IsFocused() const;
    void RequestFocus();
    void CaptureMouse();
    void ReleaseMouse();
};

// --- 容器组件 ---
class Panel : public UIElement
{
public:
    StyleColor m_bg_color;
    bool m_use_glass_effect = false;

    void Draw(ImDrawList* draw_list) override;
    // 使用基类的 Layout 逻辑 (默认 Stretch 会撑满，Fixed 会固定)
};

// --- 图片控件 ---
class Image : public UIElement
{
public:
    ImTextureID m_texture_id = 0;
    ImVec2 m_uv0 = { 0, 0 };
    ImVec2 m_uv1 = { 1, 1 };
    ImVec4 m_tint_col = { 1, 1, 1, 1 };

    float m_aspect_ratio = 1.0f; // 宽度 / 高度

    Image(ImTextureID tex_id) : m_texture_id(tex_id) { m_block_input = false; }
    void SetOriginalSize(float w, float h)
    {
        if (h > 0.0f) m_aspect_ratio = w / h;
    }

    ImVec2 Measure(ImVec2 available_size) override;
    void Arrange(const Rect& final_rect) override;
    void Draw(ImDrawList* draw_list) override;
};

// --- 垂直布局容器 ---
class VBox : public UIElement
{
public:
    float m_padding = 10.0f;

    ImVec2 Measure(ImVec2 available_size) override;
    void Arrange(const Rect& final_rect) override;
};

// --- 水平布局容器 ---
class HBox : public UIElement
{
public:
    float m_padding = 10.0f;

    ImVec2 Measure(ImVec2 available_size) override;
    void Arrange(const Rect& final_rect) override;
};

// --- 滚动视图容器 ---
class ScrollView : public UIElement
{
public:
    float m_scroll_y = 0.0f;
    float m_target_scroll_y = 0.0f;
    float m_content_height = 0.0f;
    float m_scroll_speed = 20.0f;
    float m_smoothing_speed = 15.0f;
    bool m_show_scrollbar = true;

    // ScrollView 需要特殊的 Update 来处理平滑滚动
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

// --- 根元素 ---


/*
如果使用无名容器，必须保证其内部的具名子元素在最近的一个具名祖先下是唯一的


id系统查找示例：
void GameScreen::SomeLogicFunction()
{
    if (auto rkkv_button = m_ui_root->FindElementByID("rkkvButton"))
    {
        // 我们可以安全地转换类型，因为我们知道这个ID对应的是什么
        if (auto pulsar_btn = dynamic_cast<PulsarButton*>(rkkv_button))
        {
            pulsar_btn->SetStatus("i18ntext.ui.status.reloading");
        }
    }

    // 甚至可以找到复合组件内部的子部件
    if (auto rkkv_label = m_ui_root->FindElementByID("rkkvButton.label"))
    {
        if (auto tech_text = dynamic_cast<TechText*>(rkkv_label))
        {
            // 直接修改内部文本组件的颜色
            tech_text->SetColor(ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
        }
    }
}
*/
class UIRoot : public UIElement
{
public:
    UIRoot();

    UIElement* FindElementByID(const std::string& id);

    template<typename T>
    T* FindElementAs(const std::string& id)
    {
        return dynamic_cast<T*>(FindElementByID(id));
    }

    void MarkIDMapDirty();

    void Arrange(const Rect& final_rect) override;

    void Update(float dt) override; // 覆盖以实现 Measure/Arrange 调度
    void Draw(ImDrawList* draw_list) override; // 覆盖以绘制 Tooltip
    // 便捷入口
    void Draw();
    void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& external_handled) override;
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