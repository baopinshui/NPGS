#include "CelestialInfoPanel.h"
#include "../TechUtils.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// --- 内部辅助类：竖排文字按钮 ---
class VerticalTextButton : public UIElement
{
public:
    std::string text;
    std::function<void()> on_click;
    VerticalTextButton(const std::string& t) : text(t) { m_block_input = true; }

    void Draw(ImDrawList* dl) override
    {
        if (!m_visible || m_alpha < 0.01f) return;
        const auto& theme = UIContext::Get().m_theme;
        ImU32 col = GetColorWithAlpha(m_hovered ? theme.color_accent : theme.color_text_disabled, 1.0f);

        ImFont* font = UIContext::Get().m_font_small; // 使用小字体
        if (font) ImGui::PushFont(font);

        float font_size = ImGui::GetFontSize();
        // 简单计算居中
        ImVec2 text_size = ImGui::CalcTextSize("A");
        float x_center = m_absolute_pos.x + (m_rect.w - text_size.x) * 0.5f;
        float y_cursor = m_absolute_pos.y + 15.0f;

        for (char c : text)
        {
            char buf[2] = { c, 0 };
            dl->AddText(ImVec2(x_center, y_cursor), col, buf);
            y_cursor += font_size;
        }
        if (font) ImGui::PopFont();
    }

    bool HandleMouseEvent(const ImVec2& p, bool down, bool click, bool release) override
    {
        bool ret = UIElement::HandleMouseEvent(p, down, click, release);
        if (m_clicked && click && on_click) on_click();
        return ret;
    }
};

// --- CelestialInfoPanel 实现 ---

CelestialInfoPanel::CelestialInfoPanel()
{
    // 自身作为一个全屏透明容器(或者不阻挡输入的容器)，用于承载子元素
    // 这样我们可以自由控制子元素的绝对位置
    m_rect = { 0, 0, 0, 0 }; // 尺寸在 Update 中并不关键，因为我们手动控制子元素位置
    m_visible = true;
    m_block_input = false; // 允许事件穿透

    auto& theme = UIContext::Get().m_theme;

    // 1. 创建竖向折叠标签 (Expanded Info)
    m_collapsed_tab = std::make_shared<TechBorderPanel>();
    m_collapsed_tab->m_rect = { 0, 0, 30, 100 }; // 位置在 Update 中设置
    m_collapsed_tab->m_visible = false;
    m_collapsed_tab->m_alpha = 0.0f;
    m_collapsed_tab->m_thickness = 1.0f;

    auto vert_btn = std::make_shared<VerticalTextButton>("EXPAND");
    vert_btn->m_rect = { 0, 0, 30, 100 };
    vert_btn->on_click = [this]() { this->ToggleCollapse(); };
    m_collapsed_tab->AddChild(vert_btn);
    AddChild(m_collapsed_tab);

    // 2. 创建主面板
    m_main_panel = std::make_shared<TechBorderPanel>();
    m_main_panel->m_rect = { 0, 0, PANEL_WIDTH, PANEL_HEIGHT };
    m_main_panel->m_use_glass_effect = true;
    m_main_panel->m_thickness = 2.0f;
    // 不阻挡输入，让子元素处理
    AddChild(m_main_panel);

    // --- 2.1 主面板内部布局 ---
    auto main_vbox = std::make_shared<VBox>();
    main_vbox->m_rect = { 15, 15, PANEL_WIDTH - 30, PANEL_HEIGHT - 30 };
    main_vbox->m_padding = 8.0f;
    m_main_panel->AddChild(main_vbox);

    // --- A. Header ---
    auto header_box = std::make_shared<HBox>();
    header_box->m_rect.h = 24.0f;

    // 标题 (模拟点击标题也能收起)
    auto title = std::make_shared<NeuralButton>("Star-001");
    title->m_rect.w = 120.0f;
    title->on_click_callback = [this]() { this->ToggleCollapse(); };

    auto subtitle = std::make_shared<NeuralButton>("Red Main Seq");
    subtitle->m_rect.w = 100.0f;
    subtitle->m_block_input = false; // 仅展示

    auto close_btn = std::make_shared<NeuralButton>(">>");
    close_btn->m_rect.w = 30.0f;
    close_btn->on_click_callback = [this]() { this->ToggleCollapse(); };

    header_box->AddChild(title);
    header_box->AddChild(subtitle);
    header_box->AddChild(close_btn);
    main_vbox->AddChild(header_box);

    // 分割线
    auto sep = std::make_shared<Panel>();
    sep->m_bg_color = theme.color_accent;
    sep->m_rect.h = 1.0f;
    main_vbox->AddChild(sep);

    // --- B. Tabs Headers ---
    auto grid_header = std::make_shared<HBox>();
    grid_header->m_rect.h = 20.0f;
    auto mk_tab = [](const char* t)
    {
        auto b = std::make_shared<NeuralButton>(t);
        b->m_fill_h = true; return b;
    };
    grid_header->AddChild(mk_tab("PHYSICS"));
    grid_header->AddChild(mk_tab("ORBIT"));
    grid_header->AddChild(mk_tab("COMP"));
    main_vbox->AddChild(grid_header);

    // --- C. Scroll View (数据区) ---
    m_scroll_view = std::make_shared<ScrollView>();
    m_scroll_view->m_fill_v = true; // 关键：填充 VBox 剩余空间

    m_content_vbox = std::make_shared<VBox>();
    m_content_vbox->m_padding = 2.0f;
    m_scroll_view->AddChild(m_content_vbox);

    main_vbox->AddChild(m_scroll_view);

    // 填充数据
    RebuildDataList();

    // --- D. Footer (进度条) ---
    auto footer_box = std::make_shared<VBox>();
    footer_box->m_rect.h = 50.0f;
    footer_box->m_padding = 4.0f;

    auto prog_label = std::make_shared<NeuralButton>(">>> Star Lifter Progress");
    prog_label->m_rect.h = 16.0f;
    prog_label->m_align_h = Alignment::Start;
    prog_label->m_block_input = false;

    auto progress = std::make_shared<TechProgressBar>("");
    progress->m_rect.h = 6.0f;
    progress->m_progress = 0.45f;

    auto prog_info = std::make_shared<HBox>();
    prog_info->m_rect.h = 16.0f;
    auto t1 = std::make_shared<NeuralButton>("Main Coil"); t1->m_rect.w = 80.0f; t1->m_block_input = false;
    auto t2 = std::make_shared<NeuralButton>("45%"); t2->m_rect.w = 40.0f; t2->m_block_input = false;
    prog_info->AddChild(t1);
    prog_info->AddChild(t2);

    footer_box->AddChild(prog_label);
    footer_box->AddChild(progress);
    footer_box->AddChild(prog_info);
    main_vbox->AddChild(footer_box);
}

// 关键：重写 Update 来控制动画和位置
void CelestialInfoPanel::Update(float dt, const ImVec2& parent_abs_pos)
{
    if (!m_visible) return;

    // 1. 获取屏幕信息
    ImVec2 display_sz = UIContext::Get().m_display_size;

    // 2. 计算动画进度
    float speed = 5.0f * dt;
    if (m_is_collapsed)
        m_anim_progress += speed;
    else
        m_anim_progress -= speed;
    m_anim_progress = std::clamp(m_anim_progress, 0.0f, 1.0f);

    // 3. 计算面板位置
    // 展开时: x = 屏幕宽 - 面板宽 - 右边距
    // 收起时: x = 屏幕宽 (完全移出)
    float expanded_x = display_sz.x - PANEL_WIDTH - RIGHT_MARGIN;
    float collapsed_x = display_sz.x + 10.0f; // 稍微多移出一点确保不可见

    float current_x = expanded_x + (collapsed_x - expanded_x) * AnimationUtils::Ease(m_anim_progress, EasingType::EaseInOutQuad);

    // 直接修改子元素的相对位置 (相对于 CelestialInfoPanel，而 Panel 本身通常在 (0,0))
    // 注意：这里的 m_rect 是相对于父级(UIRoot)的
    m_main_panel->m_rect.x = current_x;
    m_main_panel->m_rect.y = TOP_MARGIN;

    // 4. 计算收起标签位置
    if (m_anim_progress > 0.01f)
    {
        m_collapsed_tab->m_visible = true;
        m_collapsed_tab->m_alpha = m_anim_progress; // 淡入淡出
        m_collapsed_tab->m_rect.x = display_sz.x - 30.0f; // 贴在右边
        m_collapsed_tab->m_rect.y = TOP_MARGIN + 50.0f;
    }
    else
    {
        m_collapsed_tab->m_visible = false;
    }

    // 5. 调用基类 Update，这会递归调用 m_main_panel->Update(...)
    // 此时 m_main_panel 的 m_rect 已经是最新计算过的了，所以绝对位置计算会正确
    UIElement::Update(dt, parent_abs_pos);
}

void CelestialInfoPanel::ToggleCollapse()
{
    m_is_collapsed = !m_is_collapsed;
}

void CelestialInfoPanel::RebuildDataList()
{
    struct KV { std::string k, v; };
    std::vector<std::vector<KV>> groups = {
        { {"Type", "M4.5V"}, {"[Fe/H]", "+0.12"}, {"Stage", "H-Seq"}, {"Age", "2.1E+9yr"} },
        { {"Power", "4.7E+24W"}, {"Temp", "3354K"} },
        { {"Mass", "0.32Ms"}, {"Radius", "0.33Rs"} }
    };

    auto& theme = UIContext::Get().m_theme;

    for (size_t i = 0; i < groups.size(); ++i)
    {
        if (i > 0)
        {
            auto sep = std::make_shared<Panel>();
            sep->m_bg_color = ImVec4(theme.color_border.x, theme.color_border.y, theme.color_border.z, 0.3f);
            sep->m_rect.h = 1.0f;
            m_content_vbox->AddChild(sep);
        }
        for (const auto& kv : groups[i])
        {
            auto row = std::make_shared<HBox>();
            row->m_rect.h = 16.0f;

            auto k = std::make_shared<NeuralButton>(kv.k + ":");
            k->m_align_h = Alignment::Start; k->m_rect.w = 120.0f; k->m_block_input = false;

            auto v = std::make_shared<NeuralButton>(kv.v);
            v->m_align_h = Alignment::End; v->m_fill_h = true; v->m_block_input = false;
            // 可以通过自定义 NeuralButton 颜色来高亮数值

            row->AddChild(k); row->AddChild(v);
            m_content_vbox->AddChild(row);
        }
    }
}

_UI_END
_SYSTEM_END
_NPGS_END