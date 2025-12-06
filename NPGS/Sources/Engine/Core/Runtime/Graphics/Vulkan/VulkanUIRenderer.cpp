// VulkanUIRenderer.cpp - 修正字体上传部分
#include "VulkanUIRenderer.h"
#include "Engine/Core/System/UI/ui_framework.h"
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

FVulkanUIRenderer::FVulkanUIRenderer()
    : _context(FVulkanContext::GetClassInstance())
{}

FVulkanUIRenderer::~FVulkanUIRenderer()
{
    Shutdown();
}

bool FVulkanUIRenderer::Initialize(GLFWwindow* window)
{
    if (_initialized) return true;

    using namespace Npgs::System::UI; // 确保能访问到 UIContext

    // 1. 创建描述符池
    CreateDescriptorPool();

    // 2. 设置 ImGui 上下文
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // 设置 ImGui 样式
    ImGui::StyleColorsDark();

    // ================= [字体加载核心修改 START] =================
     // --- A. 定义字体文件路径 ---
    // 请确保这些文件真实存在于你的 Assets/Fonts/ 目录下
    const char* enFontPath = "Assets/Fonts/Anonymous-Pro-B-1.ttf"; // 英文 (基础)
    const char* cnFontPath = "Assets/Fonts/msyhbd.ttc";              // 中文 (微软雅黑)
    const char* symFontPath = "Assets/Fonts/seguisym.ttf";          // 符号 (Segoe UI Symbol)

    // --- B. 定义特殊符号的字形范围 ---
    // 覆盖: 标点, 箭头, 数学符号, 技术符号, 圈号字符, 制表符, 方块元素, 几何图形, 杂项符号
    static const ImWchar symbolRanges[] = {
        0x2000, 0x206F, // General Punctuation (常用标点)
        0x2190, 0x21FF, // Arrows (箭头) -> ← ↑ ↓
        0x2200, 0x22FF, // Mathematical Operators (数学运算) ∀ ∂ ∈
        0x2300, 0x23FF, // Misc Technical (技术符号) ⌘ ⌚
        0x2460, 0x24FF, // Enclosed Alphanumerics (带圈字符) ① ⓐ
        0x2500, 0x257F, // Box Drawing (制表符) ┌ ┐ └ ┘

        0x2580, 0x259F, // Block Elements (方块) █ ▓ ▒ (常用于进度条)
        0x25A0, 0x25FF, // Geometric Shapes (几何图形) ◈ ◆ ● ■ (你的痛点在这里)
        0x2600, 0x26FF, // Misc Symbols (杂项) ☀ ☁ ☢ ☠ ☭ ☮ (科幻图标)
        0               // [重要] 数组必须以 0 结尾
    };

    // --- C. 定义加载辅助 Lambda ---
    auto LoadFontWithChinese = [&](float size, const char* fontName) -> ImFont*
    {
        ImFontConfig config;
        config.PixelSnapH = true; // 像素对齐，防止字体模糊
        // 1. 加载英文字体 (Base)
        ImFont* font = io.Fonts->AddFontFromFileTTF(
            enFontPath,
            size,
            nullptr,
            io.Fonts->GetGlyphRangesDefault()
        );

        if (font == nullptr)
        {
            NpgsCoreError("Failed to load base font: {} (Size: {}). Check Path!", fontName, size);
            return io.Fonts->AddFontDefault(); // 失败回退
        }

        // 2. 配置中文字体合并参数
        config.MergeMode = true;      // 关键：开启合并模式
        config.GlyphOffset.y = -0.7f; // 微调垂直偏移，中英文垂直居中(根据字体实际情况调整)

        // 3. 加载中文字体 (Merge)
        ImFont* chineseFont = io.Fonts->AddFontFromFileTTF(
            cnFontPath,
            size+1.0f, // 保持和英文一样的大小
            &config,
            io.Fonts->GetGlyphRangesChineseFull() // 常用汉字范围
        );
        // 3. 合并特殊符号字体
// 符号字体通常偏小，这里特意乘以 1.1 倍，让图标在 UI 中更饱满
// 也可以保持 size 不变，看个人喜好
        config.GlyphOffset.y = 0.0f; // 符号通常不需要额外偏移

        ImFont* symFont = io.Fonts->AddFontFromFileTTF(
            symFontPath,
            size * 1.0f,
            &config,
            symbolRanges // 使用我们在 B 步骤定义的宽范围
        );
        if (chineseFont == nullptr)
        {
            NpgsCoreError("Failed to merge Chinese font for: {}", fontName);
        }

        return font;
    };

    // 获取 UI 上下文引用
    UIContext& ctx = UIContext::Get();

    // --- 加载5种层级的字体 ---

    // 1. Regular (正文/标准): 16px
    ctx.m_font_regular = LoadFontWithChinese(16.0f, "Regular");

    // 2. Bold (标题/强调): 18px 
    // 注意：如果没有专门的 Bold 字体文件，可以用同一个文件，后续如果有 bold.ttf 可以替换路径
    ctx.m_font_bold = LoadFontWithChinese(18.0f, "Bold");

    // 3. Large (数值/大标题): 24px
    ctx.m_font_large = LoadFontWithChinese(24.0f, "Large");

    // 4. Small (单位/辅助信息): 12px
    ctx.m_font_small = LoadFontWithChinese(12.0f, "Small");


    ctx.m_font_subtitle = LoadFontWithChinese(22.0f, "Subtitle");
    // 设置默认字体为 Regular
    io.FontDefault = ctx.m_font_regular;

    // ================= [字体加载核心修改 END] =================


    // 3. 手动构建图集
    io.Fonts->Build();

    // 初始化 ImGui GLFW 后端
    if (!ImGui_ImplGlfw_InitForVulkan(window, true))
    {
        NpgsCoreError("Failed to initialize ImGui GLFW backend");
        return false;
    }

    // 初始化 ImGui Vulkan 后端
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = _context->GetInstance();
    init_info.PhysicalDevice = _context->GetPhysicalDevice();
    init_info.Device = _context->GetDevice();
    init_info.QueueFamily = _context->GetGraphicsQueueFamilyIndex();
    init_info.Queue = _context->GetGraphicsQueue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = _descriptorPool;
    init_info.Subpass = 0;
    init_info.MinImageCount = _context->GetSwapchainImageCount();
    init_info.ImageCount = _context->GetSwapchainImageCount();
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = nullptr;

    init_info.CheckVkResultFn = [](VkResult err)
    {
        if (err != VK_SUCCESS)
        {
           // NpgsCoreError(std::format("ImGui Vulkan Error: {}", static_cast<vk::Result>(err)));
        }
    };

    if (!ImGui_ImplVulkan_Init(&init_info))
    {
        NpgsCoreError("Failed to initialize ImGui Vulkan backend");
        return false;
    }

    // 上传字体纹理
    auto commandBuffer = _context->GetTransferCommandBuffer();
    commandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    ImGui_ImplVulkan_CreateFontsTexture();

    commandBuffer.End();
    _context->ExecuteGraphicsCommands(commandBuffer);

    // 清理字体上传产生的临时数据 (ImGui_ImplVulkan_DestroyFontUploadObjects 在较新版本中可能已废弃或自动处理，视版本而定)
    // ImGui_ImplVulkan_DestroyFontUploadObjects(); 

    _initialized = true;
    NpgsCoreInfo("Vulkan UI Renderer initialized successfully");
    return true;
}
void FVulkanUIRenderer::Shutdown()
{
    if (!_initialized) return;

    _context->WaitIdle();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    CleanupDescriptorPool();

    _initialized = false;
    NpgsCoreInfo("Vulkan UI Renderer shutdown");
}

void FVulkanUIRenderer::BeginFrame()
{
    if (!_initialized) return;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void FVulkanUIRenderer::EndFrame()
{
    if (!_initialized) return;

    ImGui::Render();
}

void FVulkanUIRenderer::Render(vk::CommandBuffer commandBuffer)
{
    if (!_initialized) return;

    // 渲染 ImGui
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void FVulkanUIRenderer::Resize(uint32_t width, uint32_t height)
{
    if (!_initialized) return;

    // ImGui 会自动处理窗口大小变化
}

void FVulkanUIRenderer::CreateDescriptorPool()
{
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        { vk::DescriptorType::eSampler, 1000 },
        { vk::DescriptorType::eCombinedImageSampler, 1000 },
        { vk::DescriptorType::eSampledImage, 1000 },
        { vk::DescriptorType::eStorageImage, 1000 },
        { vk::DescriptorType::eUniformTexelBuffer, 1000 },
        { vk::DescriptorType::eStorageTexelBuffer, 1000 },
        { vk::DescriptorType::eUniformBuffer, 1000 },
        { vk::DescriptorType::eStorageBuffer, 1000 },
        { vk::DescriptorType::eUniformBufferDynamic, 1000 },
        { vk::DescriptorType::eStorageBufferDynamic, 1000 },
        { vk::DescriptorType::eInputAttachment, 1000 }
    };

    vk::DescriptorPoolCreateInfo poolInfo(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        1000 * static_cast<uint32_t>(poolSizes.size()),
        poolSizes
    );

    _descriptorPool = _context->GetDevice().createDescriptorPool(poolInfo);
}

void FVulkanUIRenderer::CleanupDescriptorPool()
{
    if (_descriptorPool)
    {
        _context->GetDevice().destroyDescriptorPool(_descriptorPool);
        _descriptorPool = vk::DescriptorPool();
    }
}
ImTextureID FVulkanUIRenderer::AddTexture(vk::Sampler sampler, vk::ImageView imageView, vk::ImageLayout layout)
{
    // ImGui_ImplVulkan_AddTexture 需要 VkSampler, VkImageView, VkImageLayout (C类型)
    // vk::Sampler 等 C++ 包装器可以通过 static_cast 转换为对应的 C 类型句柄
    return (ImTextureID)ImGui_ImplVulkan_AddTexture(
        static_cast<VkSampler>(sampler),
        static_cast<VkImageView>(imageView),
        static_cast<VkImageLayout>(layout)
    );
}
_GRAPHICS_END
_RUNTIME_END
_NPGS_END