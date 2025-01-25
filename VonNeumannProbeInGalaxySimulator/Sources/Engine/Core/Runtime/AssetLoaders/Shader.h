#pragma once

#include <cstdint>
#include <memory>
#include <string>
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

    struct FResourceInfo
    {
        std::vector<FVertexBufferInfo>    VertexBufferInfos;
        std::vector<FVertexAttributeInfo> VertexAttributeInfos;
        std::vector<FUniformBufferInfo>   UniformBufferInfos;
    };

private:
    struct FShaderInfo
    {
        std::vector<std::uint32_t> Code;
        vk::ShaderStageFlagBits    Stage{};
    };

    struct FVertexInputAttributeInfo
    {
        std::string                         Name;
        vk::VertexInputAttributeDescription Attribute;
        vk::VertexInputBindingDescription   Binding;
    };

    struct FShaderReflectionInfo
    {
        std::unordered_map<std::uint32_t, std::vector<vk::DescriptorSetLayoutBinding>> DescriptorSetBindings;
        std::vector<vk::PushConstantRange>                                             PushConstants;
        std::vector<FVertexInputAttributeInfo>                                         VertexInputAttributes;
    };

public:
    FShader(const std::vector<std::string>& ShaderFiles, const FResourceInfo& ResourceInfo);
    FShader(const FShader&) = default;
    FShader(FShader&& Other) noexcept;
    ~FShader();

    FShader& operator=(const FShader&) = default;
    FShader& operator=(FShader&& Other) noexcept;

    void WriteSharedDescriptors(std::uint32_t Set, std::uint32_t Binding, vk::DescriptorType Type,
                                const std::vector<vk::DescriptorBufferInfo>& BufferInfos);

    void WriteSharedDescriptors(std::uint32_t Set, std::uint32_t Binding, vk::DescriptorType Type,
                                const std::vector<vk::DescriptorImageInfo>& ImageInfos);

    void WriteDynamicDescriptors(std::uint32_t Set, std::uint32_t Binding, std::uint32_t FrameIndex, vk::DescriptorType Type,
                                 const std::vector<vk::DescriptorBufferInfo>& BufferInfos);

    std::vector<vk::PipelineShaderStageCreateInfo> GetShaderStageCreateInfo() const;
    std::vector<vk::DescriptorSetLayout> GetDescriptorSetLayouts() const;
    std::vector<vk::PushConstantRange> GetPushConstantRanges() const;
    const std::vector<vk::DescriptorSet>& GetDescriptorSets();

private:
    void InitializeShaders(const std::vector<std::string>& ShaderFiles, const FResourceInfo& ResourceInfo);
    FShaderInfo LoadShader(const std::string& Filename);
    void ReflectShader(const FShaderInfo& ShaderInfo, const FResourceInfo& ResourceInfo);
    void CreateDescriptors();
    void UpdateDescriptorSets();

private:
    std::vector<std::pair<vk::ShaderStageFlagBits, Graphics::FVulkanShaderModule>>       _ShaderModules;
    FShaderReflectionInfo                                                                _ReflectionInfo;
    std::unordered_map<std::uint32_t, std::vector<Graphics::FVulkanDescriptorSetLayout>> _DescriptorSetLayoutsMap;
    std::unordered_map<std::uint32_t, std::vector<Graphics::FVulkanDescriptorSet>>       _DescriptorSetsMap;
    std::vector<vk::DescriptorSet>                                                       _DescriptorSets;
    std::unique_ptr<Graphics::FVulkanDescriptorPool>                                     _DescriptorPool;
    bool                                                                                 _bDescriptorSetsNeedUpdate;
};

_ASSET_END
_RUNTIME_END
_NPGS_END

#include "Shader.inl"
