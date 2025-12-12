#pragma once
#include "Engine/Core/Base/Base.h" 
#include <Engine/Core/Math/NumericConstants.h>
#include <imgui.h>
#include "Utils/I18nManager.h"
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
// --- 1. 对齐与布局 ---
// --- [NEW] 1. 统一的尺寸策略 ---
enum class LengthType
{
    Fixed,      // 固定像素值
    Fill,       // 填充剩余空间 (value 代表权重)
    Content     // 自适应内容
};

struct Length
{
    LengthType type = LengthType::Fixed;
    float value = 0.0f;

    Length(LengthType t = LengthType::Fixed, float v = 0.0f) : type(t), value(v) {}

    // 辅助构造函数
    static Length Fix(float px) { return { LengthType::Fixed, px }; }
    static Length Fill(float weight = 1.0f) { return { LengthType::Fill, weight }; }
    static Length Content() { return { LengthType::Content, 0.0f }; }
};


// --- [MODIFIED] 2. 对齐与布局 ---
// Alignment 现在只负责位置，不再负责尺寸 (移除了 Stretch)
enum class Alignment { Start, Center, End };

// --- 3. 统一主题管理 (不变) ---

enum class ThemeColorID
{
    None,           // 无色/透明/不绘制
    Text,           // color_text
    TextHighlight,  // color_text_highlight
    TextDisabled,   // color_text_disabled
    Border,         // color_border
    Accent,         // color_accent (高亮色)
    Custom          // 代表使用的是具体的 ImVec4 值
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

    ImVec4 color_accent = ImVec4(0.0f, 1.0f,1.0f, 1.0f);
};
struct StyleColor
{
    ThemeColorID id = ThemeColorID::None;
    ImVec4 custom_value = { 0,0,0,0 };

    // [NEW] Modifier: -1.0f means no override. 0.0f to 1.0f overrides the alpha.
    float alpha_override = -1.0f;

    // --- Constructors ---
    StyleColor() : id(ThemeColorID::None) {}
    StyleColor(ThemeColorID _id) : id(_id) {}
    StyleColor(const ImVec4& col) : id(ThemeColorID::Custom), custom_value(col) {}

    // [NEW] Fluent interface for setting modifiers (Chainable methods)
    StyleColor WithAlpha(float alpha) const
    {
        StyleColor new_color = *this;
        new_color.alpha_override = alpha;
        return new_color;
    }

    // [MODIFIED] The core Resolve method now applies modifiers
    ImVec4 Resolve() const;

    bool HasValue() const { return id != ThemeColorID::None; }
};
// --- 3. UI 上下文 (单例/全局管理) ---
// 用于管理焦点、鼠标捕获、主题等全局状态
class UIContext
{
public:
    static UIContext& Get() { static UIContext instance; return instance; }

    // 状态管理
    UIElement* m_focused_element = nullptr;  // 当前拥有键盘焦点的元素
    UIElement* m_captured_element = nullptr; // 当前捕获鼠标的元素 (拖拽用)
    // 悬停提示
    std::string m_tooltip_candidate_key;          // [不变] 当前帧鼠标悬停的组件的 Key
    std::string m_tooltip_active_key;             // [不变] 经过计时确认后，正在显示的 Key
    std::string m_tooltip_previous_candidate_key;
    float m_tooltip_timer = 0.0f;                 // [不变] 计时器
    //主题
    UITheme m_theme;
    //字体
    ImFont* m_font_regular = nullptr;
    ImFont* m_font_bold = nullptr;
    ImFont* m_font_large = nullptr;
    ImFont* m_font_small = nullptr;
    ImFont* m_font_subtitle = nullptr;

    //毛玻璃texture
    ImTextureID m_scene_blur_texture = 0;
    ImVec2 m_display_size = { 1920, 1080 };



    // API
    void NewFrame();
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

    // [修改] Contains 函数：不仅检测，还顺便画出来
    bool Contains(const ImVec2& p) const
    {
        bool inside = p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;

        // --- DEBUG START: 极简调试代码 ---
        // static bool enable_debug = true; // 如果你想随时开关，可以用这个静态变量
        if (true) // 强制开启调试
        {
            // 1. 获取最上层画笔（画在所有窗口之上）
            ImDrawList* fg_draw = ImGui::GetForegroundDrawList();

            // 2. 决定颜色：鼠标悬停变绿，否则显红，带一点透明度
            ImU32 col = inside ? IM_COL32(0, 255, 0, 200) : IM_COL32(255, 0, 0, 100);

            // 3. 绘制边框
            fg_draw->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), col);

            // 4. (可选) 绘制一个小叉，标示中心，方便对齐
             if (inside) fg_draw->AddLine(ImVec2(x, y), ImVec2(x+w, y+h), col);
        }
        // --- DEBUG END ---

        return inside;
    }
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


    // 基础属性
    Rect m_rect = { 0, 0, 0, 0 };
    ImVec2 m_absolute_pos = { 0, 0 };
    bool m_visible = true;
    float m_alpha = 1.0f;
    std::string m_id;
    std::string m_tooltip_key;

    Length m_width_policy = Length::Content();  // 默认自适应内容宽度
    Length m_height_policy = Length::Content(); // 默认自适应内容高度
    Alignment m_align_h = Alignment::Start;     // 水平对齐 (在父容器中)（调整x方向位置）
    Alignment m_align_v = Alignment::Start;     // 垂直对齐 (在父容器中)（调整y方向位置）


    // 交互属性
    bool m_block_input = true;   // 是否阻挡鼠标
    bool m_focusable = false;    // [新增] 是否可获得焦点
    bool m_hovered = false;
    bool m_clicked = false;      // 鼠标按下状态

    // 资源 [新增]
    ImFont* m_font = nullptr;    // 自定义字体，nullptr 使用默认

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

    // 核心生命周期
public:
    virtual void UpdateSelf(float dt, const ImVec2& parent_abs_pos);
    virtual ImVec2 Measure(const ImVec2& available_size);
public:
    virtual void Update(float dt, const ImVec2& parent_abs_pos);
    virtual void Draw(ImDrawList* draw_list);
    // 事件处理
    // 返回 true 表示事件被消费
    virtual void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& external_handled);
    virtual bool HandleKeyboardEvent(); // [新增] 键盘/字符输入接口

    // 层级管理
    void AddChild(Ptr child);
    void RemoveChild(Ptr child);
    virtual void ResetInteraction();

    // [NEW] 功能 API (链式调用)  提示文本与排版
    UIElement* SetTooltip(const std::string& key) { m_tooltip_key = key; return this; }
    UIElement* SetWidth(const Length& width) { m_width_policy = width; return this; }
    UIElement* SetHeight(const Length& height) { m_height_policy = height; return this; }
    UIElement* SetFixedSize(float w, float h) { m_width_policy = Length::Fix(w); m_height_policy = Length::Fix(h); return this; }
    UIElement* SetAlignment(Alignment h, Alignment v) { m_align_h = h; m_align_v = v; return this; }
    // 新增：绘制毛玻璃背景的辅助函数
    void DrawGlassBackground(ImDrawList* draw_list, const ImVec2& p_min, const ImVec2& p_max, const ImVec4& BackCol= ImVec4(0.0,0.0,0.0,0.6));
    void To(float* property, float target, float duration, EasingType easing = EasingType::EaseOutQuad, TweenCallback on_complete = nullptr);
    ImU32 GetColorWithAlpha(const ImVec4& col, float global_alpha) const;

    virtual ImFont* GetFont() const;

    // 焦点与捕获 API 
    bool IsFocused() const;
    void RequestFocus();
    void CaptureMouse();
    void ReleaseMouse();
};

// --- 容器组件 ---
class Panel : public UIElement
{
public:
    // 支持直接覆盖颜色，也支持使用 Theme 默认颜色
    StyleColor m_bg_color;

    bool m_use_glass_effect = false;
    Panel()
    {
        // Panel 默认应该撑满父容器，因为它通常是背景
        m_width_policy = Length::Fill();
        m_height_policy = Length::Fill();
    }
    ImVec2 Measure(const ImVec2& available_size) override;
    void Draw(ImDrawList* draw_list) override;
};

// --- [新增] 图片控件 ---
class Image : public UIElement
{
public:
    ImTextureID m_texture_id = 0;
    ImVec2 m_uv0 = { 0, 0 };
    ImVec2 m_uv1 = { 1, 1 };
    ImVec4 m_tint_col = { 1, 1, 1, 1 };

    float m_aspect_ratio = 1.0f; // 宽度 / 高度

    Image(ImTextureID tex_id);
    void SetOriginalSize(float w, float h)
    {
        if (h > 0.0f) m_aspect_ratio = w / h;
    }

    ImVec2 Measure(const ImVec2& available_size) override;
    void Draw(ImDrawList* draw_list) override;
};

// --- 垂直布局容器 ---
class VBox : public UIElement
{
public:
    float m_padding = 10.0f;
    VBox()
    {
        // VBox 默认宽度填满，高度自适应内容
        m_width_policy = Length::Fill();
        m_height_policy = Length::Content();
    }
    void Update(float dt, const ImVec2& parent_abs_pos) override;
    ImVec2 Measure(const ImVec2& available_size) override;
};

// --- [新增] 水平布局容器 ---
class HBox : public UIElement
{
public:
    float m_padding = 10.0f;
    HBox()
    {
        // HBox 默认高度填满，宽度自适应内容
        m_width_policy = Length::Content();
        m_height_policy = Length::Fill();
    }
    void Update(float dt, const ImVec2& parent_abs_pos) override;
    ImVec2 Measure(const ImVec2& available_size) override;
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

    ScrollView()
    {
        // ScrollView 默认应该撑满父容器
        m_width_policy = Length::Fill();
        m_height_policy = Length::Fill();
    }

    void Update(float dt, const ImVec2& parent_abs_pos) override;
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

    HorizontalScrollView()
    {
        // ScrollView 默认应该撑满父容器
        m_width_policy = Length::Fill();
        m_height_policy = Length::Fill();
    }

    void Update(float dt, const ImVec2& parent_abs_pos) override;
    void Draw(ImDrawList* draw_list) override;
    void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& external_handled) override;
};
// --- 根元素 (新增) ---
// 负责管理整个UI场景的顶层元素
class UIRoot : public UIElement
{
public:
    UIRoot();

    // UI系统的主要入口函数
    void Update(float dt) ;
    void Draw();
    // 重写事件处理，实现事件穿透
    void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& external_handled) override;
private: 
    std::shared_ptr<GlobalTooltip> m_tooltip;
};

_UI_END
_SYSTEM_END
_NPGS_END