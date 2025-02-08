#include "ShaderBufferManager.h"

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
inline void FShaderBufferManager::CreateBuffer(const FUniformBufferCreateInfo& BufferCreateInfo)
{
    const auto* VulkanContext = FVulkanContext::GetClassInstance();
    FUniformBufferInfo BufferInfo;
    BufferInfo.CreateInfo = BufferCreateInfo;

    vk::DeviceSize MinUniformAlignment = VulkanContext->GetPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment;
    StructType Buffer{};
    Util::ForEachField(Buffer, [&](const auto& Field, std::size_t Index)
    {
        FBufferFieldInfo FieldInfo;
        FieldInfo.Offset    = reinterpret_cast<const std::byte*>(&Field) - reinterpret_cast<const std::byte*>(&Buffer);
        FieldInfo.Size      = sizeof(decltype(Field));
        FieldInfo.Alignment = BufferCreateInfo.Usage == vk::DescriptorType::eUniformBufferDynamic
                            ? (FieldInfo.Size + MinUniformAlignment - 1) & ~(MinUniformAlignment - 1)
                            : FieldInfo.Size;

        BufferInfo.Size += FieldInfo.Alignment;
        BufferInfo.Fields.emplace(BufferCreateInfo.Fields[Index], std::move(FieldInfo));
    });

    StructType EmptyData{};

    BufferInfo.Buffers.reserve(Config::Graphics::kMaxFrameInFlight);
    for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
    {
        BufferInfo.Buffers.emplace_back(BufferInfo.Size, vk::BufferUsageFlagBits::eUniformBuffer);
        BufferInfo.Buffers[i].EnablePersistentMapping();
        BufferInfo.Buffers[i].CopyData(0, 0, BufferInfo.Size, &EmptyData);
    }

    _UniformBuffers.emplace(BufferCreateInfo.Name, std::move(BufferInfo));
}

template <typename StructType>
requires std::is_class_v<StructType>
inline void FShaderBufferManager::UpdateEntrieBuffers(const std::string& Name, const StructType& Data)
{
    auto it = _UniformBuffers.find(Name);
    if (it == _UniformBuffers.end())
    {
        NpgsCoreError("Failed to find buffer \"{}\".", Name);
        return;
    }

    for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
    {
        it->second.Buffers[i].CopyData(0, 0, it->second.Size, &Data);
    }
}

template <typename StructType>
requires std::is_class_v<StructType>
inline void FShaderBufferManager::UpdateEntrieBuffer(std::uint32_t FrameIndex, const std::string& Name, const StructType& Data)
{
    auto it = _UniformBuffers.find(Name);
    if (it == _UniformBuffers.end())
    {
        NpgsCoreError("Failed to find buffer \"{}\".", Name);
        return;
    }

    it->second.Buffers[FrameIndex].CopyData(0, 0, it->second.Size, &Data);
}

template<typename FieldType>
NPGS_INLINE std::vector<FShaderBufferManager::TUpdater<FieldType>>
FShaderBufferManager::GetFieldUpdaters(const std::string& BufferName, const std::string& FieldName) const
{
    auto& BufferInfo = _UniformBuffers.at(BufferName);
    std::vector<TUpdater<FieldType>> Updaters;
    for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
    {
        Updaters.emplace_back(BufferInfo.Buffers[i], BufferInfo.Fields.at(FieldName).Offset, BufferInfo.Fields.at(FieldName).Size);
    }

    return Updaters;
}

template<typename FieldType>
NPGS_INLINE FShaderBufferManager::TUpdater<FieldType>
FShaderBufferManager::GetFieldUpdater(std::uint32_t FrameIndex, const std::string& BufferName, const std::string& FieldName) const
{
    auto& BufferInfo = _UniformBuffers.at(BufferName);
    return TUpdater<FieldType>(BufferInfo.Buffers[FrameIndex], BufferInfo.Fields.at(FieldName).Offset, BufferInfo.Fields.at(FieldName).Size);
}

NPGS_INLINE const Graphics::FDeviceLocalBuffer& FShaderBufferManager::GetBuffer(std::uint32_t FrameIndex, const std::string& BufferName)
{
    auto& BufferInfo = _UniformBuffers.at(BufferName);
    return BufferInfo.Buffers[FrameIndex];
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
