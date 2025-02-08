#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan_handles.hpp>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Buffers.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

class FShaderBufferManager
{
public:
    template <typename DataType>
    class TUpdater
    {
    public:
        TUpdater(const FDeviceLocalBuffer& Buffer, vk::DeviceSize Offset, vk::DeviceSize Size)
            : _Buffer(&Buffer), _Offset(Offset), _Size(Size)
        {
        }

        const TUpdater& operator<<(const DataType& Data) const
        {
            Submit(Data);
            return *this;
        }

        void Submit(const DataType& Data) const
        {
            _Buffer->CopyData(0, _Offset, _Size, &Data);
        }

    private:
        const Graphics::FDeviceLocalBuffer* _Buffer;
        vk::DeviceSize                      _Offset;
        vk::DeviceSize                      _Size;
    };

    struct FUniformBufferCreateInfo
    {
        std::string              Name;
        std::vector<std::string> Fields;
        std::uint32_t            Set{};
        std::uint32_t            Binding{};
        vk::DescriptorType       Usage{};
    };

private:
    struct FBufferFieldInfo
    {
        vk::DeviceSize Offset{};
        vk::DeviceSize Size{};
        vk::DeviceSize Alignment{};
    };

    struct FUniformBufferInfo
    {
        std::unordered_map<std::string, FBufferFieldInfo> Fields;
        std::vector<FDeviceLocalBuffer>                   Buffers;
        FUniformBufferCreateInfo                          CreateInfo;
        vk::DeviceSize                                    Size{};
    };

public:
    template <typename StructType>
    requires std::is_class_v<StructType>
    void CreateBuffer(const FUniformBufferCreateInfo& BufferCreateInfo);

    template <typename StructType>
    requires std::is_class_v<StructType>
    void UpdateEntrieBuffers(const std::string& Name, const StructType& Data);

    template <typename StructType>
    requires std::is_class_v<StructType>
    void UpdateEntrieBuffer(std::uint32_t FrameIndex, const std::string& Name, const StructType& Data);

    template <typename FieldType>
    std::vector<TUpdater<FieldType>> GetFieldUpdaters(const std::string& BufferName, const std::string& FieldName) const;

    template <typename FieldType>
    TUpdater<FieldType> GetFieldUpdater(std::uint32_t FrameIndex, const std::string& BufferName, const std::string& FieldName) const;

    void BindShaderToBuffers(const std::string& BufferName, const std::string& ShaderName);
    void BindShaderToBuffer(std::uint32_t FrameIndex, const std::string& BufferName, const std::string& ShaderName);
    void BindShadersToBuffers(const std::string& BufferName, const std::vector<std::string>& ShaderNames);
    void BindShadersToBuffer(std::uint32_t FrameIndex, const std::string& BufferName, const std::vector<std::string>& ShaderNames);

    const Graphics::FDeviceLocalBuffer& GetBuffer(std::uint32_t FrameIndex, const std::string& BufferName);

    static FShaderBufferManager* GetInstance();

private:
    explicit FShaderBufferManager()                   = default;
    FShaderBufferManager(const FShaderBufferManager&) = delete;
    FShaderBufferManager(FShaderBufferManager&&)      = delete;
    ~FShaderBufferManager()                           = default;

    FShaderBufferManager& operator=(const FShaderBufferManager&) = delete;
    FShaderBufferManager& operator=(FShaderBufferManager&&)      = delete;

private:
    std::unordered_map<std::string, FUniformBufferInfo> _UniformBuffers;
};

_GRAPHICS_END
_RUNTIME_END
_NPGS_END

#include "ShaderBufferManager.inl"
