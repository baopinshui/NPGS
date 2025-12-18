#include "UILoader.h"
#include <fstream>


_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

std::shared_ptr<UIRoot> UILoader::LoadFromFile(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        NpgsCoreError("UILoader: Failed to open file '{}'", filepath);
        return nullptr;
    }
    json data = json::parse(file);

    auto root_element = ParseElement(data);
    return std::dynamic_pointer_cast<UIRoot>(root_element);
}

void UILoader::RegisterAction(const std::string& action_id, ActionCallback callback)
{
    m_actions[action_id] = callback;
}

std::shared_ptr<UIElement> UILoader::ParseElement(const json& node)
{
    if (!node.contains("type") || !node["type"].is_string()) return nullptr;

    std::string type = node["type"];

    auto element = UIFactory::Get().Create(type);

    if (!element)
    {
        NpgsCoreError("UILoader: Failed to create element of type '{}'. Is it registered or handled as a special case?", type);
        return nullptr;
    }

    if (node.contains("name") && node["name"].is_string())
    {
        element->SetName(node["name"]);
    }

    if (node.contains("properties") && node["properties"].is_object())
    {
        ApplyProperties(element.get(), node["properties"]);
    }

    if (node.contains("children") && node["children"].is_array())
    {
        for (const auto& child_node : node["children"])
        {
            auto child_element = ParseElement(child_node);
            if (child_element)
            {
                element->AddChild(child_element);
            }
        }
    }
    return element;
}

// 这是一个简化的属性解析器，实际中会更庞大
void UILoader::ApplyProperties(UIElement* element, const json& props)
{
    // --- Common Properties (for all UIElement) ---
    if (props.contains("width")) element->m_width = ParseLength(props["width"]);
    if (props.contains("height")) element->m_height = ParseLength(props["height"]);
    if (props.contains("alignH")) element->m_align_h = ParseAlignment(props["alignH"]);
    if (props.contains("alignV")) element->m_align_v = ParseAlignment(props["alignV"]);
    if (props.contains("tooltip")) element->SetTooltip(props["tooltip"]);
    if (props.contains("blockInput")) element->m_block_input = props["blockInput"];
    if (props.contains("visible")) element->m_visible = props["visible"];
    if (props.contains("alpha")) element->m_alpha = props["alpha"];
    if (props.contains("font")) element->m_font = ParseFont(props["font"]);
    if (props.contains("anchor"))
    {
        ImVec2 margin = { 0,0 };
        if (props.contains("margin") && props["margin"].is_array() && props["margin"].size() == 2)
        {
            margin = ImVec2(props["margin"][0], props["margin"][1]);
        }
        element->SetAnchor(ParseAnchorPoint(props["anchor"]), margin);
    }

    // --- Type-Specific Properties ---
    if (auto* panel = dynamic_cast<Panel*>(element))
    {
        if (props.contains("useGlassEffect")) panel->m_use_glass_effect = props["useGlassEffect"];
        if (props.contains("bgColor")) panel->m_bg_color = ParseColor(props["bgColor"]);
    }
    if (auto* vbox = dynamic_cast<VBox*>(element))
    {
        if (props.contains("padding")) vbox->m_padding = props["padding"];
    }
    if (auto* hbox = dynamic_cast<HBox*>(element))
    {
        if (props.contains("padding")) hbox->m_padding = props["padding"];
    }
    if (auto* text = dynamic_cast<TechText*>(element))
    {
        if (props.contains("sourceText")) text->SetSourceText(props["sourceText"]);
        if (props.contains("color")) text->SetColor(ParseColor(props["color"]));
        if (props.contains("useGlow")) text->m_use_glow = props["useGlow"];
        if (props.contains("glowColor")) text->m_glow_color = ParseColor(props["glowColor"]);
        if (props.contains("glowIntensity")) text->m_glow_intensity = props["glowIntensity"];
        if (props.contains("glowSpread")) text->m_glow_spread = props["glowSpread"];
        if (props.contains("animMode")) text->m_anim_mode = ParseTechTextAnimMode(props["animMode"]);
        if (props.contains("sizingMode")) text->SetSizing(ParseTechTextSizingMode(props["sizingMode"]));
        if (props.contains("scrollDuration")) text->m_scroll_duration = props["scrollDuration"];
    }
    if (auto* btn = dynamic_cast<TechButton*>(element))
    {
        if (props.contains("sourceText")) btn->SetSourceText(props["sourceText"]);
        
        if (props.contains("selected")) btn->m_selected = props["selected"];
        if (props.contains("useGlass")) btn->m_use_glass = props["useGlass"];
        if (props.contains("animSpeed")) btn->m_anim_speed = props["animSpeed"];
        if (props.contains("padding") && props["padding"].is_array() && props["padding"].size() == 2)
        {
            btn->SetPadding({ props["padding"][0], props["padding"][1] });
        }
        // Colors
        if (props.contains("colorBg")) btn->m_color_bg = ParseColor(props["colorBg"]);
        if (props.contains("colorBgHover")) btn->m_color_bg_hover = ParseColor(props["colorBgHover"]);
        if (props.contains("colorText")) btn->m_color_text = ParseColor(props["colorText"]);
        if (props.contains("colorTextHover")) btn->m_color_text_hover = ParseColor(props["colorTextHover"]);
        if (props.contains("colorBorder")) btn->m_color_border = ParseColor(props["colorBorder"]);
        if (props.contains("colorBorderHover")) btn->m_color_border_hover = ParseColor(props["colorBorderHover"]);
        if (props.contains("colorSelectedBg")) btn->m_color_selected_bg = ParseColor(props["colorSelectedBg"]);
        if (props.contains("colorSelectedText")) btn->m_color_selected_text = ParseColor(props["colorSelectedText"]);
        // Action
        if (props.contains("onClick"))
        {
            std::string action_id = props["onClick"];
            auto it = m_actions.find(action_id);
            if (it != m_actions.end())
            {
                btn->on_click = it->second;
            }
        }
    }
    if (auto* img = dynamic_cast<Image*>(element))
    {
        if (props.contains("texture")) img->m_texture_id = ParseTexture(props["texture"]);
        if (props.contains("tintColor")) img->m_tint_col = ParseColor(props["tintColor"]).Resolve();
        if (props.contains("aspectRatio")) img->m_aspect_ratio = props["aspectRatio"];
    }
    if (auto* scroll = dynamic_cast<ScrollView*>(element))
    {
        if (props.contains("scrollSpeed")) scroll->m_scroll_speed = props["scrollSpeed"];
        if (props.contains("smoothingSpeed")) scroll->m_smoothing_speed = props["smoothingSpeed"];
        if (props.contains("showScrollbar")) scroll->m_show_scrollbar = props["showScrollbar"];
    }
    if (auto* hscroll = dynamic_cast<HorizontalScrollView*>(element))
    {
        if (props.contains("scrollSpeed")) hscroll->m_scroll_speed = props["scrollSpeed"];
        if (props.contains("smoothingSpeed")) hscroll->m_smoothing_speed = props["smoothingSpeed"];
        if (props.contains("showScrollbar")) hscroll->m_show_scrollbar = props["showScrollbar"];
    }
    if (auto* vignette = dynamic_cast<ScreenVignette*>(element))
    {
        if (props.contains("color")) vignette->m_color = ParseColor(props["color"]).Resolve();
        if (props.contains("feather")) vignette->m_feather = props["feather"];
    }
    if (auto* border_panel = dynamic_cast<TechBorderPanel*>(element))
    {
        if (props.contains("glassColor")) border_panel->m_glass_col = ParseColor(props["glassColor"]);
        if (props.contains("thickness")) border_panel->m_thickness = props["thickness"];
        if (props.contains("forceAccentColor")) border_panel->m_force_accent_color = props["forceAccentColor"];
        if (props.contains("showFlowBorder")) border_panel->m_show_flow_border = props["showFlowBorder"];
        if (props.contains("flowPeriod")) border_panel->m_flow_period = props["flowPeriod"];
        if (props.contains("flowLengthRatio")) border_panel->m_flow_length_ratio = props["flowLengthRatio"];
        if (props.contains("flowSegmentCount")) border_panel->m_flow_segment_count = props["flowSegmentCount"];
        if (props.contains("flowRandomness")) border_panel->m_flow_randomness = props["flowRandomness"];
        if (props.contains("flowUseGradient")) border_panel->m_flow_use_gradient = props["flowUseGradient"];
    }
    if (auto* divider = dynamic_cast<TechDivider*>(element))
    {
        if (props.contains("color")) divider->m_color = ParseColor(props["color"]);
        if (props.contains("visualHeight")) divider->m_visual_height = props["visualHeight"];
        if (props.contains("useGradient")) divider->m_use_gradient = props["useGradient"];
    }
    if (auto* pbar = dynamic_cast<TechProgressBar*>(element))
    {
        if (props.contains("progress")) pbar->m_progress = props["progress"];
        if (props.contains("label")) pbar->m_label = props["label"];
        if (props.contains("colorBg")) pbar->m_color_bg = ParseColor(props["colorBg"]);
        if (props.contains("colorFill")) pbar->m_color_fill = ParseColor(props["colorFill"]);
        if (props.contains("colorText")) pbar->m_color_text = ParseColor(props["colorText"]);
        if (props.contains("barHeight")) pbar->m_bar_height = props["barHeight"];
        if (props.contains("labelSpacing")) pbar->m_label_spacing = props["labelSpacing"];
    }
    if (auto* pbtn = dynamic_cast<PulsarButton*>(element))
    {
        if (props.contains("canExecute")) pbtn->SetExecutable(props["canExecute"]);
    }
}
// --- Helper Implementations ---
Length UILoader::ParseLength(const json& node)
{
    if (node.is_number()) return Length::Fixed(node);
    if (node.is_string())
    {
        std::string s = node;
        if (s == "Content") return Length::Content();
        if (s == "Stretch") return Length::Stretch(1.0f);
    }
    if (node.is_object())
    {
        std::string type = node["type"];
        if (type == "Fixed") return Length::Fixed(node["value"]);
        if (type == "Content") return Length::Content();
        if (type == "Stretch")
        {
            return node.contains("value") ? Length::Stretch(node["value"]) : Length::Stretch(1.0f);
        }
    }
    return Length::Fixed(0);
}
StyleColor UILoader::ParseColor(const json& node)
{
    if (node.is_string())
    {
        std::string id_str = node;
        if (id_str == "Accent") return ThemeColorID::Accent;
        if (id_str == "Text") return ThemeColorID::Text;
        if (id_str == "TextHighlight") return ThemeColorID::TextHighlight;
        if (id_str == "TextDisabled") return ThemeColorID::TextDisabled;
        if (id_str == "Border") return ThemeColorID::Border;
    }
    if (node.is_array() && node.size() >= 3)
    {
        float a = node.size() > 3 ? (float)node[3] : 1.0f;
        return ImVec4(node[0], node[1], node[2], a);
    }
    if (node.is_object())
    {
        if (node.contains("id")) return ParseColor(node["id"]);
        if (node.contains("custom")) return ParseColor(node["custom"]);
    }
    return ThemeColorID::None;
}
Alignment UILoader::ParseAlignment(const json& node)
{
    std::string val = node;
    if (val == "Center") return Alignment::Center;
    if (val == "End") return Alignment::End;
    return Alignment::Start;
}

ImFont* UILoader::ParseFont(const std::string& font_name)
{
    auto& ctx = UIContext::Get();
    if (font_name == "large") return ctx.m_font_large;
    if (font_name == "bold") return ctx.m_font_bold;
    if (font_name == "small") return ctx.m_font_small;
    if (font_name == "subtitle") return ctx.m_font_subtitle;
    return ctx.m_font_regular;
}
ImTextureID UILoader::ParseTexture(const std::string& texture_name)
{
    // Placeholder: In a real engine, this would query an asset manager.
    // For now, we return 0, assuming textures are set from code later.
    // [修改] 对于 GameScreen，我们知道某些纹理是存在的，可以尝试从 AppContext 获取
    if (texture_name == "RKKV_ICON")
    {
        // 警告：UILoader 不应该直接访问 AppContext。这是一个临时的 hack。
        // 更好的做法是 UILoader 查询一个 AssetManager，
        // 而 AssetManager 在启动时由 Application 填充了 AppContext 中的纹理。
        // 由于我们没有 AssetManager，这里只能假设有某种全局访问方式。
        // 目前，我们还是返回0，让C++代码在加载后手动设置。
        NpgsCoreWarn("UILoader: Texture lookup for '{}' is not fully implemented. Texture will be set via code.", texture_name);
        return 0;
    }
    NpgsCoreWarn("UILoader: Texture lookup for '{}' is not implemented. Returning null.", texture_name);
    return 0;
}
AnchorPoint UILoader::ParseAnchorPoint(const json& node)
{
    std::string val = node;
    if (val == "TopLeft") return AnchorPoint::TopLeft;
    if (val == "TopCenter") return AnchorPoint::TopCenter;
    if (val == "TopRight") return AnchorPoint::TopRight;
    if (val == "MiddleLeft") return AnchorPoint::MiddleLeft;
    if (val == "Center") return AnchorPoint::Center;
    if (val == "MiddleRight") return AnchorPoint::MiddleRight;
    if (val == "BottomLeft") return AnchorPoint::BottomLeft;
    if (val == "BottomCenter") return AnchorPoint::BottomCenter;
    if (val == "BottomRight") return AnchorPoint::BottomRight;
    return AnchorPoint::None;
}

TechButton::Style UILoader::ParseTechButtonStyle(const json& node)
{
    std::string val = node;
    if (val == "Tab") return TechButton::Style::Tab;
    if (val == "Vertical") return TechButton::Style::Vertical;
    if (val == "Invisible") return TechButton::Style::Invisible;
    return TechButton::Style::Normal;
}

TechTextAnimMode UILoader::ParseTechTextAnimMode(const json& node)
{
    std::string val = node;
    if (val == "Hacker") return TechTextAnimMode::Hacker;
    if (val == "Scroll") return TechTextAnimMode::Scroll;
    return TechTextAnimMode::None;
}

TechTextSizingMode UILoader::ParseTechTextSizingMode(const json& node)
{
    std::string val = node;
    if (val == "Fixed") return TechTextSizingMode::Fixed;
    if (val == "AutoHeight") return TechTextSizingMode::AutoHeight;
    return TechTextSizingMode::AutoWidthHeight;
}
_UI_END
_SYSTEM_END
_NPGS_END