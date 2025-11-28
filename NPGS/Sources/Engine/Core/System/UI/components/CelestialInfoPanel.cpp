#include "CelestialInfoPanel.h"
#include "../TechUtils.h"
#include "TechText.h"
#include "TechDivider.h"
#include "TechButton.h"      // 引入统一的 TechButton
#include "TechProgressBar.h" // 引入进度条组件

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

CelestialInfoPanel::CelestialInfoPanel()
{
    // 自身作为容器，不阻挡输入，全透明
    m_rect = { 0, 0, 0, 0 };
    m_visible = true;
    m_block_input = false;

    auto& theme = UIContext::Get().m_theme;

    // =========================================================
    // 1. 创建侧边把手 (Collapsed Tab) - 重构为 TechButton
    // =========================================================
    // 不再需要外层 Panel 套娃，直接使用支持 Vertical 样式和 Glass 效果的按钮
    m_collapsed_btn = std::make_shared<TechButton>("INFO", TechButton::Style::Vertical);
    m_collapsed_btn->m_rect = { 0, 0, 24, 100 }; // 窄条设计
    m_collapsed_btn->SetUseGlass(true);          // 开启毛玻璃背景
    m_collapsed_btn->on_click = [this]() { this->ToggleCollapse(); };

    AddChild(m_collapsed_btn);

    // =========================================================
    // 2. 创建主面板背景
    // =========================================================
    m_main_panel = std::make_shared<TechBorderPanel>();
    m_main_panel->m_rect = { 0, 0, PANEL_WIDTH, PANEL_HEIGHT };
    m_main_panel->m_use_glass_effect = true;
    m_main_panel->m_thickness = 2.0f;
    AddChild(m_main_panel);

    // --- 主面板内部布局容器 ---
    auto main_vbox = std::make_shared<VBox>();
    main_vbox->m_rect = { 15, 15, PANEL_WIDTH - 30, PANEL_HEIGHT - 30 };
    main_vbox->m_padding = 4.0f; // 整体布局紧凑
    m_main_panel->AddChild(main_vbox);

    // =========================================================
    // A. Header 区域 (标题 + 分割线)
    // =========================================================
    {
        auto header_row = std::make_shared<HBox>();
        header_row->m_rect.h = 28.0f; // 固定高度

        // 1. 主标题 (左对齐，大号，高亮)
        auto title = std::make_shared<TechText>("Star-001");
        title->m_font = UIContext::Get().m_font_bold;
        title->SetColor(theme.color_accent);
        title->m_align_v = Alignment::End; // 底部对齐
        title->m_rect.w = 120.0f;          // 预留宽度

        // 2. 副标题 (右对齐，小号，灰色)
        auto subtitle = std::make_shared<TechText>("Red Main Seq");
        subtitle->SetColor(theme.color_text_disabled);
        subtitle->m_font = UIContext::Get().m_font_small;
        subtitle->m_align_v = Alignment::End; // 底部对齐
        subtitle->m_align_h = Alignment::End; // 靠右
        subtitle->m_fill_h = true;            // 填满剩余空间

        header_row->AddChild(title);
        header_row->AddChild(subtitle);

        // [修复] 必须添加到主布局中
        main_vbox->AddChild(header_row);

        // 3. Header 下方的分割线
        auto div = std::make_shared<TechDivider>();
        div->m_color = theme.color_accent;
        div->m_rect.h = 8.0f; // 分割线占位高度
        main_vbox->AddChild(div);
    }

    // =========================================================
    // B. Tabs 区域 (实心/透明切换风格)
    // =========================================================
    {
        auto tabs_box = std::make_shared<HBox>();
        tabs_box->m_rect.h = 20.0f;
        tabs_box->m_padding = 4.0f; // 标签之间的间距

        // 辅助 lambda：创建 Tab 按钮
        auto mk_tab = [&](const char* txt, bool selected)
        {
            auto t = std::make_shared<TechButton>(txt, TechButton::Style::Tab);
            t->SetSelected(selected);
            t->m_fill_h = true; // 均分宽度
            t->on_click = [t, this]()
            {
                // 这里只是简单的视觉演示，实际逻辑需要遍历所有 tab 重置 selected 状态
                t->SetSelected(true);
                // TODO: 可以在这里调用 RebuildDataList(...) 切换显示内容
            };
            return t;
        };

        tabs_box->AddChild(mk_tab("PHYSICS", true)); // 默认选中第一个
        tabs_box->AddChild(mk_tab("ORBIT", false));
        tabs_box->AddChild(mk_tab("COMP", false));

        main_vbox->AddChild(tabs_box);
    }

    // 增加一点垂直间距
    auto spacer = std::make_shared<UIElement>();
    spacer->m_rect.h = 4.0f;
    main_vbox->AddChild(spacer);

    // =========================================================
    // C. 数据滚动区
    // =========================================================
    m_scroll_view = std::make_shared<ScrollView>();
    m_scroll_view->m_fill_v = true; // 占据中间剩余的所有垂直空间

    m_content_vbox = std::make_shared<VBox>();
    m_content_vbox->m_padding = 8.0f; // 组与组之间的间距
    m_scroll_view->AddChild(m_content_vbox);

    main_vbox->AddChild(m_scroll_view);

    // 初始填充数据
    RebuildDataList();

    // =========================================================
    // D. Footer 区域 (进度条)
    // =========================================================
    {
        auto footer_box = std::make_shared<VBox>();
        footer_box->m_rect.h = 50.0f; // 固定 footer 高度
        footer_box->m_padding = 2.0f;

        // 1. 顶部分割线 (灰色，细一点)
        auto sep = std::make_shared<TechDivider>();
        sep->m_color = theme.color_border;
        sep->m_rect.h = 8.0f;
        footer_box->AddChild(sep);

        // 2. 标题
        auto prog_label = std::make_shared<TechText>(">>> Star Lifter Progress");
        prog_label->SetColor(theme.color_accent);
        prog_label->m_rect.h = 16.0f;
        prog_label->m_font = UIContext::Get().m_font_small;

        // 3. 进度条 (带背景槽的样式在 TechProgressBar 内部实现)
        auto progress = std::make_shared<TechProgressBar>("");
        progress->m_rect.h = 6.0f;
        progress->m_progress = 0.45f;

        // 4. 底部详情 (Main Coil ... 45%)
        auto prog_info = std::make_shared<HBox>();
        prog_info->m_rect.h = 14.0f;

        auto t1 = std::make_shared<TechText>("Main Coil");
        t1->SetColor(theme.color_text_disabled);
        t1->m_font = UIContext::Get().m_font_small;
        t1->m_rect.w = 100.0f;

        auto t2 = std::make_shared<TechText>("45%");
        t2->SetColor(theme.color_text);
        t2->m_font = UIContext::Get().m_font_small; // 建议用等宽字体
        t2->m_fill_h = true;
        t2->m_align_h = Alignment::End;

        prog_info->AddChild(t1);
        prog_info->AddChild(t2);

        footer_box->AddChild(prog_label);
        footer_box->AddChild(progress);
        footer_box->AddChild(prog_info);

        main_vbox->AddChild(footer_box);
    }
}

void CelestialInfoPanel::Update(float dt, const ImVec2& parent_abs_pos)
{
    if (!m_visible) return;

    ImVec2 display_sz = UIContext::Get().m_display_size;

    // 1. 动画状态机
    float speed = 5.0f * dt;
    if (m_is_collapsed)
        m_anim_progress += speed;
    else
        m_anim_progress -= speed;
    m_anim_progress = std::clamp(m_anim_progress, 0.0f, 1.0f);

    // 2. 计算位置 (EaseInOutQuad 插值)

    // [修改] 展开时：面板右边缘紧贴屏幕右侧 (X = 屏幕宽 - 面板宽)
    // 去掉了之前的 RIGHT_MARGIN
    float expanded_x = display_sz.x - PANEL_WIDTH;

    // [修改] 收起时：面板左边缘正好在屏幕右侧外 (X = 屏幕宽)
    float collapsed_x = display_sz.x;

    // 插值计算当前 X
    float current_x = expanded_x + (collapsed_x - expanded_x) * AnimationUtils::Ease(m_anim_progress, EasingType::EaseInOutQuad);

    // 设置主面板位置
    m_main_panel->m_rect.x = current_x;
    m_main_panel->m_rect.y = TOP_MARGIN;

    // 3. 设置把手位置 (始终吸附在面板左侧)
    m_collapsed_btn->m_visible = true;

    m_collapsed_btn->m_rect.x = current_x - m_collapsed_btn->m_rect.w - 10.0f;

    m_collapsed_btn->m_rect.y = TOP_MARGIN + 40.0f; // 把手垂直偏移

    // 4. 调用基类 Update
    UIElement::Update(dt, parent_abs_pos);
}

void CelestialInfoPanel::ToggleCollapse()
{
    m_is_collapsed = !m_is_collapsed;
    m_collapsed_btn->ResetInteraction();
}

// 数据列表构建：实现 [左侧竖线] + [右侧内容] 的分组布局
void CelestialInfoPanel::RebuildDataList()
{
    m_content_vbox->m_children.clear();

    struct KV { std::string k, v; };
    std::vector<std::vector<KV>> groups = {
        { {"Type", "M4.5V"}, {"[Fe/H]", "+0.12"}, {"Stage", "H-Seq"}, {"Progress", "0.2%"}, {"Age", "2.1E+9yr"}, {"Life", "2.2E+12yr"}, {"End", "Fade"}, {"Remnant", "He-WD"} },
        { {"Power", "4.7E+24W"}, {"Temp", "3354K"} },
        { {"Mass", "0.32Ms"}, {"Radius", "0.33Rs"} }
    };

    auto& theme = UIContext::Get().m_theme;

    // 定义颜色
    ImVec4 col_line = theme.color_accent;
    col_line.w = 0.5f; // 半透明竖线
    ImVec4 col_key = theme.color_text_disabled;
    ImVec4 col_val = theme.color_text;
    ImVec4 col_highlight = theme.color_accent;

    for (const auto& group : groups)
    {
        // 1. 创建水平容器，容纳 [竖线] 和 [数据列]
        auto group_hbox = std::make_shared<HBox>();
        group_hbox->m_padding = 8.0f; // 竖线与文字的间距

        // 2. 左侧竖线 (复用 Panel)
        auto line = std::make_shared<Panel>();
        line->m_rect.w = 2.0f;       // 线宽
        line->m_bg_color = col_line; // 颜色
        line->m_align_v = Alignment::Stretch; // 纵向撑满
        // 这里假设 Panel 默认没有边框/毛玻璃，或者可以配置关闭，仅作为色块使用
        group_hbox->AddChild(line);

        // 3. 右侧内容列 (VBox)
        auto content_col = std::make_shared<VBox>();
        content_col->m_padding = 1.0f; // 行间距极小，视觉紧凑
        content_col->m_fill_h = true;  // 填满水平空间

        for (const auto& kv : group)
        {
            auto row = std::make_shared<HBox>();
            row->m_rect.h = 14.0f; // 紧凑行高

            // Key
            auto k = std::make_shared<TechText>(kv.k + ":", col_key);
            k->m_align_h = Alignment::Start;
            k->m_font = UIContext::Get().m_font_small;
            k->m_rect.w = 90.0f; // 固定宽度对齐

            // Value
            auto v = std::make_shared<TechText>(kv.v, col_val);
            v->m_align_h = Alignment::End;
            v->m_fill_h = true;
            v->m_font = UIContext::Get().m_font_small; // 建议在 TechText 内部强制使用 Monospace 字体处理数字

            // 特定字段高亮演示
            if (kv.k == "Stage" || kv.k == "Temp")
            {
                v->SetColor(col_highlight);
            }

            row->AddChild(k);
            row->AddChild(v);
            content_col->AddChild(row);
        }

        group_hbox->AddChild(content_col);

        // 将整组添加到主内容区
        m_content_vbox->AddChild(group_hbox);

        // 组与组之间的间距 (透明 Spacer)
        auto spacer = std::make_shared<UIElement>();
        spacer->m_rect.h = 8.0f;
        m_content_vbox->AddChild(spacer);
    }
}

_UI_END
_SYSTEM_END
_NPGS_END