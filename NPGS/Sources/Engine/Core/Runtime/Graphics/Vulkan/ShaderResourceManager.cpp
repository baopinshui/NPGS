#include "ShaderResourceManager.h"

#include <algorithm>
#include <array>
#include <utility>

#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Core/Runtime/AssetLoaders/Shader.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

void FShaderResourceManager::BindShaderToBuffers(const std::string& BufferName, const std::string& ShaderName)
{
    auto it = _UniformBuffers.find(BufferName);
    if (it == _UniformBuffers.end())
    {
        NpgsCoreError("Failed to find buffer \"{}\".", BufferName);
        return;
    }

    auto& StoredBufferInfo = it->second;
    vk::DescriptorType Usage   = StoredBufferInfo.CreateInfo.Usage;
    std::uint32_t      Set     = StoredBufferInfo.CreateInfo.Set;
    std::uint32_t      Binding = StoredBufferInfo.CreateInfo.Binding;
    vk::DeviceSize     Size    = StoredBufferInfo.Size;

    auto* Shader = Asset::FAssetManager::GetInstance()->GetAsset<Asset::FShader>(ShaderName);
    if (Shader == nullptr)
    {
        NpgsCoreError("Failed to find shader asset: \"{}\".", ShaderName);
        return;
    }

    for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
    {
        vk::DescriptorBufferInfo WriteBufferInfo(*StoredBufferInfo.Buffers[i].GetBuffer(), 0, Size);
        Shader->WriteDynamicDescriptors<vk::DescriptorBufferInfo>(Set, Binding, i, Usage, { WriteBufferInfo });
    }
}

void FShaderResourceManager::BindShaderToBuffer(std::uint32_t FrameIndex, const std::string& BufferName, const std::string& ShaderName)
{
    auto it = _UniformBuffers.find(BufferName);
    if (it == _UniformBuffers.end())
    {
        NpgsCoreError("Failed to find buffer \"{}\".", BufferName);
        return;
    }

    auto& StoredBufferInfo = it->second;
    vk::DescriptorType Usage   = StoredBufferInfo.CreateInfo.Usage;
    std::uint32_t      Set     = StoredBufferInfo.CreateInfo.Set;
    std::uint32_t      Binding = StoredBufferInfo.CreateInfo.Binding;
    vk::DeviceSize     Size    = StoredBufferInfo.Size;

    auto* Shader = Asset::FAssetManager::GetInstance()->GetAsset<Asset::FShader>(ShaderName);
    if (Shader == nullptr)
    {
        NpgsCoreError("Failed to find shader asset: \"{}\".", ShaderName);
        return;
    }

    vk::DescriptorBufferInfo WriteBufferInfo(*StoredBufferInfo.Buffers[FrameIndex].GetBuffer(), 0, Size);
    Shader->WriteDynamicDescriptors<vk::DescriptorBufferInfo>(Set, Binding, FrameIndex, Usage, { WriteBufferInfo });
}

void FShaderResourceManager::BindShadersToBuffers(const std::string& BufferName, const std::vector<std::string>& ShaderNames)
{
    auto it = _UniformBuffers.find(BufferName);
    if (it == _UniformBuffers.end())
    {
        NpgsCoreError("Failed to find buffer \"{}\".", BufferName);
        return;
    }

    auto& StoredBufferInfo = it->second;
    vk::DescriptorType Usage   = StoredBufferInfo.CreateInfo.Usage;
    std::uint32_t      Set     = StoredBufferInfo.CreateInfo.Set;
    std::uint32_t      Binding = StoredBufferInfo.CreateInfo.Binding;
    vk::DeviceSize     Size    = StoredBufferInfo.Size;

    for (const auto& ShaderName : ShaderNames)
    {
        auto* Shader = Asset::FAssetManager::GetInstance()->GetAsset<Asset::FShader>(ShaderName);
        if (Shader == nullptr)
        {
            NpgsCoreError("Failed to find shader asset: \"{}\".", ShaderName);
            return;
        }

        for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
        {
            vk::DescriptorBufferInfo WriteBufferInfo(*StoredBufferInfo.Buffers[i].GetBuffer(), 0, Size);
            Shader->WriteDynamicDescriptors<vk::DescriptorBufferInfo>(Set, Binding, i, Usage, { WriteBufferInfo });
        }
    }
}

void FShaderResourceManager::BindShadersToBuffer(std::uint32_t FrameIndex, const std::string& BufferName, const std::vector<std::string>& ShaderNames)
{
    auto it = _UniformBuffers.find(BufferName);
    if (it == _UniformBuffers.end())
    {
        NpgsCoreError("Failed to find buffer \"{}\".", BufferName);
        return;
    }

    auto& StoredBufferInfo = it->second;
    vk::DescriptorType Usage   = StoredBufferInfo.CreateInfo.Usage;
    std::uint32_t      Set     = StoredBufferInfo.CreateInfo.Set;
    std::uint32_t      Binding = StoredBufferInfo.CreateInfo.Binding;
    vk::DeviceSize     Size    = StoredBufferInfo.Size;

    for (const auto& ShaderName : ShaderNames)
    {
        auto* Shader = Asset::FAssetManager::GetInstance()->GetAsset<Asset::FShader>(ShaderName);
        if (Shader == nullptr)
        {
            NpgsCoreError("Failed to find shader asset: \"{}\".", ShaderName);
            return;
        }

        vk::DescriptorBufferInfo WriteBufferInfo(*StoredBufferInfo.Buffers[FrameIndex].GetBuffer(), 0, Size);
        Shader->WriteDynamicDescriptors<vk::DescriptorBufferInfo>(Set, Binding, FrameIndex, Usage, { WriteBufferInfo });
    }
}

FShaderResourceManager* FShaderResourceManager::GetInstance()
{
    static FShaderResourceManager kInstance;
    return &kInstance;
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
