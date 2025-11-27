#pragma once
#include "Engine/Core/Base/Base.h" // 保持你原有的头文件引用
#include <Engine/Core/Math/NumericConstants.h>
#include <imgui.h>
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

// --- 1. 对齐与布局 ---
enum class Alignment { Start, Center, End, Stretch };

// --- 2. 统一主题管理 ---
struct UITheme
{
    ImVec4 color_text_highlight = { 1.0f, 1.0f, 1.0f, 1.0f };
    ImVec4 color_text = { 0.8f, 0.8f, 0.8f, 1.0f };
    ImVec4 color_text_disabled = { 0.5f, 0.5f, 0.5f, 1.0f };
    ImVec4 color_panel_bg = { 0.1f, 0.1f, 0.1f, 0.8f };
    ImVec4 color_button_bg = { 0.2f, 0.2f, 0.2f, 1.0f };
    ImVec4 color_button_hover = { 0.3f, 0.3f, 0.3f, 1.0f };
    ImVec4 color_button_active = { 0.4f, 0.4f, 0.4f, 1.0f };
    ImVec4 color_border = { 0.5f, 0.5f, 0.5f, 1.0f };
    ImVec4 color_accent = { 30.0f / 255.0f, 114.0f / 255.0f, 232.0f / 255.0f, 1.0f }; // korvo的主题色
   // ImVec4 color_accent = ImVec4(0.745f, 0.745f, 0.561f, 1.0f); // #米黄色
    //ImVec4 color_accent = ImVec4(0.0f,1.0f,0.0f, 1.0f); 
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
    UITheme m_theme;

    ImFont* m_font_regular = nullptr;
    ImFont* m_font_bold = nullptr;
    ImFont* m_font_large = nullptr;
    ImFont* m_font_small = nullptr;



    ImTextureID m_scene_blur_texture = 0;

    // [新增] 屏幕尺寸，用于计算 UV
    ImVec2 m_display_size = { 1920, 1080 };



    // API
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
        if (!true) // 强制开启调试
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
    Rect m_rect = { 0, 0, 100, 100 };
    ImVec2 m_absolute_pos = { 0, 0 };
    bool m_visible = true;
    float m_alpha = 1.0f;



    // 布局属性 [新增]
    Alignment m_align_h = Alignment::Stretch; // 水平对齐
    Alignment m_align_v = Alignment::Stretch; // 垂直对齐 (部分容器支持)

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
    virtual void Update(float dt, const ImVec2& parent_abs_pos);
    virtual void Draw(ImDrawList* draw_list);

    // 事件处理
    // 返回 true 表示事件被消费
    virtual bool HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released);
    virtual bool HandleKeyboardEvent(); // [新增] 键盘/字符输入接口

    // 层级管理
    void AddChild(Ptr child);
    void RemoveChild(Ptr child);
    virtual void ResetInteraction();

    // 功能 API
    // 新增：绘制毛玻璃背景的辅助函数
    void DrawGlassBackground(ImDrawList* draw_list, const ImVec2& p_min, const ImVec2& p_max);
    void To(float* property, float target, float duration, EasingType easing = EasingType::EaseOutQuad, TweenCallback on_complete = nullptr);
    ImU32 GetColorWithAlpha(const ImVec4& col, float global_alpha) const;

    virtual ImFont* GetFont() const;

    // 焦点与捕获 API [新增]
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
    std::optional<ImVec4> m_bg_color;

    bool m_use_glass_effect = false;

    //ImVec4 m_glass_tint = { 0.6f, 0.6f, 0.6f, 1.0f };
    void Draw(ImDrawList* draw_list) override;
};

// --- [新增] 图片控件 ---
//class Image : public UIElement
//{
//public:
//    ImTextureID m_texture_id = nullptr;
//    ImVec2 m_uv0 = { 0, 0 };
//    ImVec2 m_uv1 = { 1, 1 };
//    ImVec4 m_tint_col = { 1, 1, 1, 1 };
//
//    Image(ImTextureID tex_id) : m_texture_id(tex_id) { m_block_input = false; }
//    void Draw(ImDrawList* draw_list) override;
//};

// --- 垂直布局容器 ---
class VBox : public UIElement
{
public:
    float m_padding = 10.0f;
    void Update(float dt, const ImVec2& parent_abs_pos) override;
};

// --- [新增] 水平布局容器 ---
class HBox : public UIElement
{
public:
    float m_padding = 10.0f;
    void Update(float dt, const ImVec2& parent_abs_pos) override;
};

// --- 滚动视图容器 ---
class ScrollView : public UIElement
{
public:
    float m_scroll_y = 0.0f;
    float m_content_height = 0.0f;
    float m_scroll_speed = 20.0f;
    bool m_show_scrollbar = true;

    void Update(float dt, const ImVec2& parent_abs_pos) override;
    void Draw(ImDrawList* draw_list) override;
    bool HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released) override;
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
    bool HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released) override;
};

_UI_END
_SYSTEM_END
_NPGS_END