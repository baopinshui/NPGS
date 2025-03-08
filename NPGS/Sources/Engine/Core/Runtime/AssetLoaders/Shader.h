#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan_handles.hpp>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_ASSET_BEGIN

class FShader
{
public:
    struct FVertexBufferInfo
    {
        std::uint32_t Binding{};
        std::uint32_t Stride{};
        bool          bIsPerInstance{ false };
    };

    struct FVertexAttributeInfo
    {
        std::uint32_t Binding{};
        std::uint32_t Location{};
        std::uint32_t Offset{};
    };

    struct FUniformBufferInfo
    {
        std::uint32_t Set{};
        std::uint32_t Binding{};
        bool          bIsDynamic{ false };
    };

    struct FDynamicResourceInfo
    {
        std::uint32_t Set{};
        std::uint32_t Binding{};
    };

    struct FResourceInfo
    {
        std::vector<FVertexBufferInfo>                                        VertexBufferInfos;
        std::vector<FVertexAttributeInfo>                                     VertexAttributeInfos;
        std::vector<FUniformBufferInfo>                                       UniformBufferInfos;
        std::unordered_map<vk::ShaderStageFlagBits, std::vector<std::string>> PushConstantInfos;
    };

private:
    struct FShaderInfo
    {
        std::vector<std::uint32_t> Code;
        vk::ShaderStageFlagBits    Stage{};
    };

    struct FShaderReflectionInfo
    {
        std::unordered_map<std::uint32_t, std::vector<vk::DescriptorSetLayoutBinding>> DescriptorSetBindings;
        std::vector<vk::VertexInputBindingDescription>                                 VertexInputBindings;
        std::vector<vk::VertexInputAttributeDescription>                               VertexInputAttributes;
        std::vector<vk::PushConstantRange>                                             PushConstants;
    };

public:
    FShader(const std::vector<std::string>& ShaderFiles, const FResourceInfo& ResourceInfo);
    FShader(const FShader&) = delete;
    FShader(FShader&& Other) noexcept;
    ~FShader() = default;

    FShader& operator=(const FShader&) = delete;
    FShader& operator=(FShader&& Other) noexcept;

    template <typename DescriptorInfoType>
    requires std::is_class_v<DescriptorInfoType>
    void WriteSharedDescriptors(std::uint32_t Set, std::uint32_t Binding, vk::DescriptorType Type,
                                const std::vector<DescriptorInfoType>& DescriptorInfos);

    template <typename DescriptorInfoType>
    requires std::is_class_v<DescriptorInfoType>
    void WriteDynamicDescriptors(std::uint32_t Set, std::uint32_t Binding, std::uint32_t FrameIndex,
                                 vk::DescriptorType Type, const std::vector<DescriptorInfoType>& DescriptorInfos);

    std::vector<vk::PipelineShaderStageCreateInfo> CreateShaderStageCreateInfo() const;
    std::vector<vk::DescriptorSetLayout> GetDescriptorSetLayouts() const;
    std::vector<vk::PushConstantRange> GetPushConstantRanges() const;
    std::uint32_t GetPushConstantOffset(const std::string& Name) const;

    const std::vector<vk::VertexInputBindingDescription>& GetVertexInputBindings() const;
    const std::vector<vk::VertexInputAttributeDescription>& GetVertexInputAttributes() const;
    const std::vector<vk::DescriptorSet>& GetDescriptorSets(std::uint32_t FrameIndex);

private:
    void InitializeShaders(const std::vector<std::string>& ShaderFiles, const FResourceInfo& ResourceInfo);
    FShaderInfo LoadShader(const std::string& Filename);
    void ReflectShader(const FShaderInfo& ShaderInfo, const FResourceInfo& ResourceInfo);
    void CreateDescriptors();
    void UpdateDescriptorSets(std::uint32_t FrameIndex);
    void MarkAllFramesForUpdate();

private:
    std::vector<std::pair<vk::ShaderStageFlagBits, Graphics::FVulkanShaderModule>> _ShaderModules;
    FShaderReflectionInfo                                                          _ReflectionInfo;
    std::unordered_map<std::string, std::uint32_t>                                 _PushConstantOffsetsMap;
    std::unordered_map<std::uint32_t, Graphics::FVulkanDescriptorSetLayout>        _DescriptorSetLayoutsMap;
    std::unordered_map<std::uint32_t, std::vector<Graphics::FVulkanDescriptorSet>> _DescriptorSetsMap;
    std::unordered_map<std::uint32_t, std::vector<vk::DescriptorSet>>              _DescriptorSets;
    std::unique_ptr<Graphics::FVulkanDescriptorPool>                               _DescriptorPool;
    std::uint32_t                                                                  _DescriptorSetsUpdateMask;
};

_ASSET_END
_RUNTIME_END
_NPGS_END

#include "Shader.inl"
