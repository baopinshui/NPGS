#include "ShaderResourceManager.h"

#include <utility>

#include "Engine/Core/Base/Config/EngineConfig.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Utils/FieldReflection.hpp"
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

template <typename StructType>
requires std::is_class_v<StructType>
inline void FShaderResourceManager::CreateBuffers(const FUniformBufferCreateInfo& BufferCreateInfo,
                                                  const VmaAllocationCreateInfo* AllocationCreateInfo,
                                                  std::uint32_t BufferCount)
{
    const auto* VulkanContext = FVulkanContext::GetClassInstance();
    FUniformBufferInfo BufferInfo;
    BufferInfo.CreateInfo = BufferCreateInfo;

    vk::DeviceSize MinUniformAlignment = VulkanContext->GetPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment;
    StructType Buffer{};
    Util::ForEachField(Buffer, [&](const auto& Field, std::size_t Index)
    {
        FBufferFieldInfo FieldInfo;
        FieldInfo.Size      = sizeof(decltype(Field));
        FieldInfo.Alignment = BufferCreateInfo.Usage == vk::DescriptorType::eUniformBufferDynamic
                            ? (FieldInfo.Size + MinUniformAlignment - 1) & ~(MinUniformAlignment - 1)
                            : FieldInfo.Size;
        FieldInfo.Offset    = BufferCreateInfo.Usage == vk::DescriptorType::eUniformBufferDynamic
                            ? BufferInfo.Size
                            : reinterpret_cast<const std::byte*>(&Field) - reinterpret_cast<const std::byte*>(&Buffer);

        BufferInfo.Size += FieldInfo.Alignment;
        BufferInfo.Fields.emplace(BufferCreateInfo.Fields[Index], std::move(FieldInfo));
    });

    StructType EmptyData{};

    BufferCount = BufferCount ? BufferCount : Config::Graphics::kMaxFrameInFlight;

    BufferInfo.Buffers.reserve(BufferCount);
    for (std::uint32_t i = 0; i != BufferCount; ++i)
    {
        if (AllocationCreateInfo != nullptr)
        {
            vk::BufferCreateInfo BufferCreateInfo({}, BufferInfo.Size,
                vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst);
            BufferInfo.Buffers.emplace_back(*AllocationCreateInfo, BufferCreateInfo);
        }
        else
        {
            BufferInfo.Buffers.emplace_back(BufferInfo.Size, vk::BufferUsageFlagBits::eUniformBuffer);
        }

        BufferInfo.Buffers[i].EnablePersistentMapping();
        BufferInfo.Buffers[i].CopyData(0, 0, BufferInfo.Size, &EmptyData);
    }

    _UniformBuffers.emplace(BufferCreateInfo.Name, std::move(BufferInfo));
}

NPGS_INLINE void FShaderResourceManager::RemoveBuffer(const std::string& Name)
{
    auto it = _UniformBuffers.find(Name);
    if (it == _UniformBuffers.end())
    {
        NpgsCoreError("Failed to find buffer \"{}\".", Name);
        return;
    }

    _UniformBuffers.erase(it);
}

template <typename StructType>
requires std::is_class_v<StructType>
inline void FShaderResourceManager::UpdateEntrieBuffers(const std::string& Name, const StructType& Data)
{
    // auto it = _UniformBuffers.find(Name);
    // if (it == _UniformBuffers.end())
    // {
    //     NpgsCoreError("Failed to find buffer \"{}\".", Name);
    //     return;
    // }

    auto& UniformBuffers = _UniformBuffers.at(Name);
    for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
    {
        UniformBuffers.Buffers[i].CopyData(0, 0, UniformBuffers.Size, &Data);
    }
}

template <typename StructType>
requires std::is_class_v<StructType>
inline void FShaderResourceManager::UpdateEntrieBuffer(std::uint32_t FrameIndex, const std::string& Name, const StructType& Data)
{
    // auto it = _UniformBuffers.find(Name);
    // if (it == _UniformBuffers.end())
    // {
    //     NpgsCoreError("Failed to find buffer \"{}\".", Name);
    //     return;
    // }

    // it->second.Buffers[FrameIndex].CopyData(0, 0, it->second.Size, &Data);

    auto& UniformBuffers = _UniformBuffers.at(Name);
    UniformBuffers.Buffers[FrameIndex].CopyData(0, 0, UniformBuffers.Size, &Data);
}

template <typename FieldType>
NPGS_INLINE std::vector<FShaderResourceManager::TUpdater<FieldType>>
FShaderResourceManager::GetFieldUpdaters(const std::string& BufferName, const std::string& FieldName) const
{
    auto& BufferInfo = _UniformBuffers.at(BufferName);
    std::vector<TUpdater<FieldType>> Updaters;
    for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
    {
        Updaters.emplace_back(BufferInfo.Buffers[i], BufferInfo.Fields.at(FieldName).Offset, BufferInfo.Fields.at(FieldName).Size);
    }

    return Updaters;
}

template <typename FieldType>
NPGS_INLINE FShaderResourceManager::TUpdater<FieldType>
FShaderResourceManager::GetFieldUpdater(std::uint32_t FrameIndex, const std::string& BufferName, const std::string& FieldName) const
{
    auto& BufferInfo = _UniformBuffers.at(BufferName);
    return TUpdater<FieldType>(BufferInfo.Buffers[FrameIndex], BufferInfo.Fields.at(FieldName).Offset, BufferInfo.Fields.at(FieldName).Size);
}

template <typename... Args>
NPGS_INLINE void FShaderResourceManager::BindShadersToBuffers(const std::string& BufferName, Args&&... ShaderNames)
{
    std::vector<std::string> ShaderNameList{ std::forward<Args>(ShaderNames)... };
    BindShaderListToBuffers(BufferName, ShaderNameList);
}

template <typename... Args>
NPGS_INLINE void FShaderResourceManager::BindShadersToBuffer(std::uint32_t FrameIndex, const std::string& BufferName, Args&&... ShaderNames)
{
    std::vector<std::string> ShaderNameList{ std::forward<Args>(ShaderNames)... };
    BindShaderListToBuffer(FrameIndex, BufferName, ShaderNameList);
}

NPGS_INLINE const Graphics::FDeviceLocalBuffer& FShaderResourceManager::GetBuffer(std::uint32_t FrameIndex, const std::string& BufferName)
{
    auto& BufferInfo = _UniformBuffers.at(BufferName);
    return BufferInfo.Buffers[FrameIndex];
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
