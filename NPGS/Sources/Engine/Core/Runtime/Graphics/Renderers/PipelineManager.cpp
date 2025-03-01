#include "PipelineManager.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Core/Runtime/AssetLoaders/Shader.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

void FPipelineManager::CreatePipeline(const std::string& PipelineName, const std::string& ShaderName,
                                      FGraphicsPipelineCreateInfoPack& GraphicsPipelineCreateInfoPack)
{
    auto* VulkanContext = FVulkanContext::GetClassInstance();
    auto* AssetManager  = Asset::FAssetManager::GetInstance();
    auto* Shader        = AssetManager->GetAsset<Asset::FShader>(ShaderName);

    VulkanContext->WaitIdle();

    if (ShaderName == "")
    {
        GraphicsPipelineCreateInfoPack.Update();
        FVulkanPipelineLayout PipelineLayout(GraphicsPipelineCreateInfoPack.GraphicsPipelineCreateInfo.layout, "Pipeline layout");
        _PipelineLayouts.emplace(PipelineName, std::move(PipelineLayout));

        FVulkanPipeline Pipeline(GraphicsPipelineCreateInfoPack);
        _Pipelines.emplace(PipelineName, std::move(Pipeline));

        RegisterCallback(PipelineName, EPipelineType::kGraphics);

        return;
    }

    vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo;
    auto NativeArray = Shader->GetDescriptorSetLayouts();
    PipelineLayoutCreateInfo.setSetLayouts(NativeArray);
    auto PushConstantRanges = Shader->GetPushConstantRanges();
    PipelineLayoutCreateInfo.setPushConstantRanges(PushConstantRanges);

    FVulkanPipelineLayout PipelineLayout(PipelineLayoutCreateInfo);
    GraphicsPipelineCreateInfoPack.GraphicsPipelineCreateInfo.setLayout(*PipelineLayout);
    GraphicsPipelineCreateInfoPack.ShaderStages = Shader->CreateShaderStageCreateInfo();
    _PipelineLayouts.emplace(PipelineName, std::move(PipelineLayout));

    GraphicsPipelineCreateInfoPack.VertexInputBindings.clear();
    GraphicsPipelineCreateInfoPack.VertexInputBindings.append_range(Shader->GetVertexInputBindings());
    GraphicsPipelineCreateInfoPack.VertexInputAttributes.clear();
    GraphicsPipelineCreateInfoPack.VertexInputAttributes.append_range(Shader->GetVertexInputAttributes());
    GraphicsPipelineCreateInfoPack.Update();
    _GraphicsPipelineCreateInfoPacks.emplace(PipelineName, GraphicsPipelineCreateInfoPack);

    FVulkanPipeline Pipeline(GraphicsPipelineCreateInfoPack);
    _Pipelines.emplace(PipelineName, std::move(Pipeline));

    RegisterCallback(PipelineName, EPipelineType::kGraphics);
}

void FPipelineManager::CreatePipeline(const std::string& PipelineName, const std::string& ShaderName,
                                      vk::ComputePipelineCreateInfo* ComputePipelineCreateInfo)
{
    auto* VulkanContext = FVulkanContext::GetClassInstance();
    auto* AssetManager  = Asset::FAssetManager::GetInstance();
    auto* Shader        = AssetManager->GetAsset<Asset::FShader>(ShaderName);

    VulkanContext->WaitIdle();

    if (ShaderName == "" && ComputePipelineCreateInfo != nullptr)
    {
        FVulkanPipelineLayout PipelineLayout(ComputePipelineCreateInfo->layout, "Pipeline layout");
        _PipelineLayouts.emplace(PipelineName, std::move(PipelineLayout));

        FVulkanPipeline Pipeline(*ComputePipelineCreateInfo);
        _Pipelines.emplace(PipelineName, std::move(Pipeline));

        RegisterCallback(PipelineName, EPipelineType::kCompute);

        return;
    }

    ComputePipelineCreateInfo = ComputePipelineCreateInfo == nullptr
                              ? new vk::ComputePipelineCreateInfo() : ComputePipelineCreateInfo;

    vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo;
    auto NativeArray = Shader->GetDescriptorSetLayouts();
    PipelineLayoutCreateInfo.setSetLayouts(NativeArray);
    auto PushConstantRanges = Shader->GetPushConstantRanges();
    PipelineLayoutCreateInfo.setPushConstantRanges(PushConstantRanges);

    FVulkanPipelineLayout PipelineLayout(PipelineLayoutCreateInfo);
    ComputePipelineCreateInfo->setLayout(*PipelineLayout);
    ComputePipelineCreateInfo->setStage(Shader->CreateShaderStageCreateInfo().front());
    _PipelineLayouts.emplace(PipelineName, std::move(PipelineLayout));

    _ComputePipelineCreateInfos.emplace(PipelineName, *ComputePipelineCreateInfo);

    FVulkanPipeline Pipeline(*ComputePipelineCreateInfo);
    _Pipelines.emplace(PipelineName, std::move(Pipeline));

    RegisterCallback(PipelineName, EPipelineType::kCompute);
}

void FPipelineManager::RemovePipeline(const std::string& Name)
{
    _Pipelines.erase(Name);
}

FPipelineManager* FPipelineManager::GetInstance()
{
    static FPipelineManager kInstance;
    return &kInstance;
}

void FPipelineManager::RegisterCallback(const std::string& Name, EPipelineType Type)
{
    auto VulkanContext = FVulkanContext::GetClassInstance();
    std::function<void()> CreatePipeline;
    std::function<void()> DestroyPipeline;

    if (Type == EPipelineType::kGraphics)
    {
        CreatePipeline = [this, Name, VulkanContext]() -> void
        {
            VulkanContext->WaitIdle();

            auto& SwapchainExtent = VulkanContext->GetSwapchainCreateInfo().imageExtent;
            auto& PipelineCreateInfoPack = _GraphicsPipelineCreateInfoPacks.at(Name);

            vk::Viewport Viewport{};
            Viewport.x = 0.0f;
            Viewport.y = static_cast<float>(SwapchainExtent.height);
            Viewport.width  =  static_cast<float>(SwapchainExtent.width);
            Viewport.height = -static_cast<float>(SwapchainExtent.height);
            Viewport.minDepth = 0.0f;
            Viewport.maxDepth = 1.0f;
            PipelineCreateInfoPack.Viewports.clear();
            PipelineCreateInfoPack.Viewports.push_back(Viewport);

            vk::Rect2D Scissor{};
            Scissor.offset = vk::Offset2D();
            Scissor.extent = SwapchainExtent;
            PipelineCreateInfoPack.Scissors.clear();
            PipelineCreateInfoPack.Scissors.push_back(Scissor);

            PipelineCreateInfoPack.Update();

            auto it = _Pipelines.find(Name);
            _Pipelines.erase(it);
            _Pipelines.emplace(Name, std::move(FVulkanPipeline(PipelineCreateInfoPack)));
        };

        DestroyPipeline = [this, Name]() -> void
        {
            FVulkanPipeline Pipeline = std::move(_Pipelines.at(Name));
        };
    }
    else
    {
        CreatePipeline  = []() -> void {};
        DestroyPipeline = []() -> void {};
    }

    VulkanContext->RegisterAutoRemovedCallbacks(FVulkanContext::ECallbackType::kCreateSwapchain, Name, CreatePipeline);
    VulkanContext->RegisterAutoRemovedCallbacks(FVulkanContext::ECallbackType::kDestroySwapchain, Name, DestroyPipeline);
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
