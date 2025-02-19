#include "Wrappers.h"

#include <cstddef>
#include <algorithm>
#include <limits>
#include <utility>

#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/Graphics/Vulkan/Core.h"
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

namespace
{
    std::uint32_t GetMemoryTypeIndex(const vk::PhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties,
                                     const vk::MemoryRequirements& MemoryRequirements, vk::MemoryPropertyFlags MemoryPropertyFlags)
    {
        for (std::size_t i = 0; i != PhysicalDeviceMemoryProperties.memoryTypeCount; ++i)
        {
            if (MemoryRequirements.memoryTypeBits & static_cast<std::uint32_t>(Bit(i)) &&
                (PhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & MemoryPropertyFlags) == MemoryPropertyFlags)
            {
                return static_cast<std::uint32_t>(i);
            }
        }

        return std::numeric_limits<std::uint32_t>::max();
    }
}

FGraphicsPipelineCreateInfoPack::FGraphicsPipelineCreateInfoPack()
{
    LinkToGraphicsPipelineCreateInfo();
    GraphicsPipelineCreateInfo.setBasePipelineIndex(-1);
}

FGraphicsPipelineCreateInfoPack::FGraphicsPipelineCreateInfoPack(FGraphicsPipelineCreateInfoPack&& Other) noexcept
    :
    GraphicsPipelineCreateInfo(std::exchange(Other.GraphicsPipelineCreateInfo, {})),
    VertexInputStateCreateInfo(std::exchange(Other.VertexInputStateCreateInfo, {})),
    InputAssemblyStateCreateInfo(std::exchange(Other.InputAssemblyStateCreateInfo, {})),
    TessellationStateCreateInfo(std::exchange(Other.TessellationStateCreateInfo, {})),
    ViewportStateCreateInfo(std::exchange(Other.ViewportStateCreateInfo, {})),
    RasterizationStateCreateInfo(std::exchange(Other.RasterizationStateCreateInfo, {})),
    MultisampleStateCreateInfo(std::exchange(Other.MultisampleStateCreateInfo, {})),
    DepthStencilStateCreateInfo(std::exchange(Other.DepthStencilStateCreateInfo, {})),
    ColorBlendStateCreateInfo(std::exchange(Other.ColorBlendStateCreateInfo, {})),
    DynamicStateCreateInfo(std::exchange(Other.DynamicStateCreateInfo, {})),

    ShaderStages(std::move(Other.ShaderStages)),
    VertexInputBindings(std::move(Other.VertexInputBindings)),
    VertexInputAttributes(std::move(Other.VertexInputAttributes)),
    Viewports(std::move(Other.Viewports)),
    Scissors(std::move(Other.Scissors)),
    ColorBlendAttachmentStates(std::move(Other.ColorBlendAttachmentStates)),
    DynamicStates(std::move(Other.DynamicStates)),

    DynamicViewportCount(std::exchange(Other.DynamicViewportCount, 1)),
    DynamicScissorCount(std::exchange(Other.DynamicScissorCount, 1))
{
    LinkToGraphicsPipelineCreateInfo();
    UpdateAllInfoData();
}

FGraphicsPipelineCreateInfoPack& FGraphicsPipelineCreateInfoPack::operator=(FGraphicsPipelineCreateInfoPack&& Other) noexcept
{
    if (this != &Other)
    {
        GraphicsPipelineCreateInfo   = std::exchange(Other.GraphicsPipelineCreateInfo,   {});

        VertexInputStateCreateInfo   = std::exchange(Other.VertexInputStateCreateInfo,   {});
        InputAssemblyStateCreateInfo = std::exchange(Other.InputAssemblyStateCreateInfo, {});
        TessellationStateCreateInfo  = std::exchange(Other.TessellationStateCreateInfo,  {});
        ViewportStateCreateInfo      = std::exchange(Other.ViewportStateCreateInfo,      {});
        RasterizationStateCreateInfo = std::exchange(Other.RasterizationStateCreateInfo, {});
        MultisampleStateCreateInfo   = std::exchange(Other.MultisampleStateCreateInfo,   {});
        DepthStencilStateCreateInfo  = std::exchange(Other.DepthStencilStateCreateInfo,  {});
        ColorBlendStateCreateInfo    = std::exchange(Other.ColorBlendStateCreateInfo,    {});
        DynamicStateCreateInfo       = std::exchange(Other.DynamicStateCreateInfo,       {});

        ShaderStages                 = std::move(Other.ShaderStages);
        VertexInputBindings          = std::move(Other.VertexInputBindings);
        VertexInputAttributes        = std::move(Other.VertexInputAttributes);
        Viewports                    = std::move(Other.Viewports);
        Scissors                     = std::move(Other.Scissors);
        ColorBlendAttachmentStates   = std::move(Other.ColorBlendAttachmentStates);
        DynamicStates                = std::move(Other.DynamicStates);

        DynamicViewportCount         = std::exchange(Other.DynamicViewportCount, 1);
        DynamicScissorCount          = std::exchange(Other.DynamicScissorCount, 1);

        LinkToGraphicsPipelineCreateInfo();
        UpdateAllInfoData();
    }

    return *this;
}

void FGraphicsPipelineCreateInfoPack::Update()
{
    ViewportStateCreateInfo.setViewportCount(
        Viewports.size() ? static_cast<std::uint32_t>(Viewports.size()) : DynamicViewportCount);
    ViewportStateCreateInfo.setScissorCount(
        Scissors.size() ? static_cast<std::uint32_t>(Scissors.size()) : DynamicScissorCount);

    UpdateAllInfoData();
}

void FGraphicsPipelineCreateInfoPack::LinkToGraphicsPipelineCreateInfo()
{
    GraphicsPipelineCreateInfo.setPVertexInputState(&VertexInputStateCreateInfo)
                              .setPInputAssemblyState(&InputAssemblyStateCreateInfo)
                              .setPTessellationState(&TessellationStateCreateInfo)
                              .setPViewportState(&ViewportStateCreateInfo)
                              .setPRasterizationState(&RasterizationStateCreateInfo)
                              .setPMultisampleState(&MultisampleStateCreateInfo)
                              .setPDepthStencilState(&DepthStencilStateCreateInfo)
                              .setPColorBlendState(&ColorBlendStateCreateInfo)
                              .setPDynamicState(&DynamicStateCreateInfo);
}

void FGraphicsPipelineCreateInfoPack::UpdateAllInfoData()
{
    if (Viewports.empty())
    {
        ViewportStateCreateInfo.setPViewports(nullptr);
    }
    else
    {
        ViewportStateCreateInfo.setViewports(Viewports);
    }

    if (Scissors.empty())
    {
        ViewportStateCreateInfo.setPScissors(nullptr);
    }
    else
    {
        ViewportStateCreateInfo.setScissors(Scissors);
    }

    GraphicsPipelineCreateInfo.setStages(ShaderStages);
    VertexInputStateCreateInfo.setVertexBindingDescriptions(VertexInputBindings);
    VertexInputStateCreateInfo.setVertexAttributeDescriptions(VertexInputAttributes);
    ColorBlendStateCreateInfo.setAttachments(ColorBlendAttachmentStates);
    DynamicStateCreateInfo.setDynamicStates(DynamicStates);

    LinkToGraphicsPipelineCreateInfo();
}

FFormatInfo GetFormatInfo(vk::Format Format)
{
    FFormatInfo Info;

    Info.ComponentCount = vk::componentCount(Format);
    Info.ComponentSize  = vk::componentBits(Format, 0) / 8;
    Info.PixelSize      = vk::blockSize(Format);

    if (Format == vk::Format::eD16UnormS8Uint)
    {
        Info.PixelSize = 4;
    }
    else if (Format == vk::Format::eD32SfloatS8Uint)
    {
        Info.PixelSize = 8;
    }

    if (Format == vk::Format::eUndefined)
    {
        Info.RawDataType = FFormatInfo::ERawDataType::kOther;
    }
    else
    {
        const char* NumericFormat = vk::componentNumericFormat(Format, 0);
        if (Util::Equal(NumericFormat, "SINT")    || Util::Equal(NumericFormat, "UINT")  ||
            Util::Equal(NumericFormat, "SNORM")   || Util::Equal(NumericFormat, "UNORM") ||
            Util::Equal(NumericFormat, "SSCALED") || Util::Equal(NumericFormat, "USCALED"))
        {
            Info.RawDataType = FFormatInfo::ERawDataType::kInteger;
        }
        else if (Util::Equal(NumericFormat, "SFLOAT") || Util::Equal(NumericFormat, "UFLOAT"))
        {
            Info.RawDataType = FFormatInfo::ERawDataType::kFloatingPoint;
        }
        else
        {
            Info.RawDataType = FFormatInfo::ERawDataType::kInteger;
        }
    }

    return Info;
}

vk::Format ConvertToFloat16(vk::Format Float32Format)
{
    switch (Float32Format)
    {
    case vk::Format::eR32Sfloat:
        return vk::Format::eR16Sfloat;
    case vk::Format::eR32G32Sfloat:
        return vk::Format::eR16G16Sfloat;
    case vk::Format::eR32G32B32Sfloat:
        return vk::Format::eR16G16B16Sfloat;
    case vk::Format::eR32G32B32A32Sfloat:
        return vk::Format::eR16G16B16A16Sfloat;
    default:
        return vk::Format::eUndefined;
    }
}

// Wrapper for vk::CommandBuffer
// -----------------------------
vk::Result FVulkanCommandBuffer::Begin(const vk::CommandBufferInheritanceInfo& InheritanceInfo,
                                       vk::CommandBufferUsageFlags Flags) const
{
    vk::CommandBufferBeginInfo CommandBufferBeginInfo(Flags, &InheritanceInfo);
    try
    {
        _Handle.begin(CommandBufferBeginInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to begin command buffer: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    return vk::Result::eSuccess;
}

vk::Result FVulkanCommandBuffer::Begin(vk::CommandBufferUsageFlags Flags) const
{
    vk::CommandBufferBeginInfo CommandBufferBeginInfo(Flags);
    try
    {
        _Handle.begin(CommandBufferBeginInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to begin command buffer: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    return vk::Result::eSuccess;
}

vk::Result FVulkanCommandBuffer::End() const
{
    try
    {
        _Handle.end();
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to end command buffer: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    return vk::Result::eSuccess;
}

// Wrapper for vk::CommandPool
// ---------------------------
FVulkanCommandPool::FVulkanCommandPool(vk::CommandPoolCreateInfo& CreateInfo)
    : FVulkanCommandPool(FVulkanCore::GetClassInstance()->GetDevice(), CreateInfo)
{
}

FVulkanCommandPool::FVulkanCommandPool(vk::Device Device, vk::CommandPoolCreateInfo& CreateInfo)
    : Base(Device)
{
    CreateCommandPool(CreateInfo);
}

FVulkanCommandPool::FVulkanCommandPool(std::uint32_t QueueFamilyIndex, vk::CommandPoolCreateFlags Flags)
    : FVulkanCommandPool(FVulkanCore::GetClassInstance()->GetDevice(), QueueFamilyIndex, Flags)
{
}

FVulkanCommandPool::FVulkanCommandPool(vk::Device Device, std::uint32_t QueueFamilyIndex, vk::CommandPoolCreateFlags Flags)
    : Base(Device)
{
    _ReleaseInfo = "Command pool destroyed successfully.";
    _Status      = CreateCommandPool(QueueFamilyIndex, Flags);
}

vk::Result FVulkanCommandPool::AllocateBuffer(vk::CommandBufferLevel Level, vk::CommandBuffer& Buffer) const
{
    vk::CommandBufferAllocateInfo CommandBufferAllocateInfo(_Handle, Level, 1);
    try
    {
        Buffer = _Device.allocateCommandBuffers(CommandBufferAllocateInfo)[0];
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to allocate command buffer: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Command buffer allocated successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanCommandPool::AllocateBuffer(vk::CommandBufferLevel Level, FVulkanCommandBuffer& Buffer) const
{
    vk::CommandBufferAllocateInfo CommandBufferAllocateInfo(_Handle, Level, 1);
    try
    {
        *Buffer = _Device.allocateCommandBuffers(CommandBufferAllocateInfo)[0];
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to allocate command buffer: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Command buffer allocated successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanCommandPool::AllocateBuffers(vk::CommandBufferLevel Level, std::vector<vk::CommandBuffer>& Buffers) const
{
    vk::CommandBufferAllocateInfo CommandBufferAllocateInfo(_Handle, Level, static_cast<std::uint32_t>(Buffers.size()));
    try
    {
        Buffers = _Device.allocateCommandBuffers(CommandBufferAllocateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to allocate command buffers: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Command buffers allocated successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanCommandPool::AllocateBuffers(vk::CommandBufferLevel Level, std::vector<FVulkanCommandBuffer>& Buffers) const
{
    vk::CommandBufferAllocateInfo CommandBufferAllocateInfo(_Handle, Level, static_cast<std::uint32_t>(Buffers.size()));
    std::vector<vk::CommandBuffer> CommandBuffers;
    try
    {
        CommandBuffers = _Device.allocateCommandBuffers(CommandBufferAllocateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to allocate command buffers: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    Buffers.resize(CommandBuffers.size());
    for (std::size_t i = 0; i != CommandBuffers.size(); ++i)
    {
        *Buffers[i] = CommandBuffers[i];
    }

    NpgsCoreInfo("Command buffers allocated successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanCommandPool::FreeBuffer(vk::CommandBuffer& Buffer) const
{
    _Device.freeCommandBuffers(_Handle, Buffer);
    Buffer = vk::CommandBuffer();
    NpgsCoreInfo("Command buffer freed successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanCommandPool::FreeBuffer(FVulkanCommandBuffer& Buffer) const
{
    return FreeBuffer(*Buffer);
}

vk::Result FVulkanCommandPool::FreeBuffers(std::vector<vk::CommandBuffer>& Buffers) const
{
    _Device.freeCommandBuffers(_Handle, Buffers);
    Buffers.clear();
    NpgsCoreInfo("Command buffers freed successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanCommandPool::FreeBuffers(std::vector<FVulkanCommandBuffer>& Buffers) const
{
    std::vector<vk::CommandBuffer> CommandBuffers;
    CommandBuffers.reserve(Buffers.size());

    for (auto& Buffer : Buffers)
    {
        CommandBuffers.push_back(*Buffer);
    }

    return FreeBuffers(CommandBuffers);
}

vk::Result FVulkanCommandPool::CreateCommandPool(vk::CommandPoolCreateInfo& CreateInfo)
{
    try
    {
        _Handle = _Device.createCommandPool(CreateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create command pool: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Command pool created successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanCommandPool::CreateCommandPool(std::uint32_t QueueFamilyIndex, vk::CommandPoolCreateFlags Flags)
{
    vk::CommandPoolCreateInfo CreateInfo(Flags, QueueFamilyIndex);
    return CreateCommandPool(CreateInfo);
}

// Wrapper for vk::DeviceMemory
// ----------------------------
FVulkanDeviceMemory::FVulkanDeviceMemory(const vk::MemoryAllocateInfo& AllocateInfo)
    : FVulkanDeviceMemory(FVulkanCore::GetClassInstance()->GetDevice(),
                          FVulkanCore::GetClassInstance()->GetPhysicalDeviceProperties(),
                          FVulkanCore::GetClassInstance()->GetPhysicalDeviceMemoryProperties(),
                          AllocateInfo)
{
}

FVulkanDeviceMemory::FVulkanDeviceMemory(vk::Device Device, const vk::PhysicalDeviceProperties& PhysicalDeviceProperties,
                                         const vk::PhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties,
                                         const vk::MemoryAllocateInfo& AllocateInfo)
    :
    Base(Device),
    _PhysicalDeviceProperties(&PhysicalDeviceProperties),
    _PhysicalDeviceMemoryProperties(&PhysicalDeviceMemoryProperties),
    _MappedDataMemory(nullptr),
    _MappedTargetMemory(nullptr),
    _bPersistentlyMapped(false)
{
    _ReleaseInfo = "Device memory freed successfully.";
    _Status      = AllocateDeviceMemory(AllocateInfo);
}

FVulkanDeviceMemory::FVulkanDeviceMemory(FVulkanDeviceMemory&& Other) noexcept
    :
    Base(std::move(Other)),
    _PhysicalDeviceProperties(std::exchange(Other._PhysicalDeviceProperties, nullptr)),
    _PhysicalDeviceMemoryProperties(std::exchange(Other._PhysicalDeviceMemoryProperties, nullptr)),
    _AllocationSize(std::exchange(Other._AllocationSize, 0)),
    _MemoryPropertyFlags(std::exchange(Other._MemoryPropertyFlags, {})),
    _MappedDataMemory(std::exchange(Other._MappedDataMemory, nullptr)),
    _MappedTargetMemory(std::exchange(Other._MappedTargetMemory, nullptr)),
    _bPersistentlyMapped(std::exchange(Other._bPersistentlyMapped, false))
{
}

FVulkanDeviceMemory::~FVulkanDeviceMemory()
{
    if (_bPersistentlyMapped && (_MappedDataMemory != nullptr || _MappedTargetMemory != nullptr))
    {
        UnmapMemory(0, _AllocationSize);
        _MappedDataMemory    = nullptr;
        _MappedTargetMemory  = nullptr;
        _bPersistentlyMapped = false;
    }
}

FVulkanDeviceMemory& FVulkanDeviceMemory::operator=(FVulkanDeviceMemory&& Other) noexcept
{
    if (this != &Other)
    {
        Base::operator=(std::move(Other));

        _PhysicalDeviceProperties       = std::exchange(Other._PhysicalDeviceProperties, nullptr);
        _PhysicalDeviceMemoryProperties = std::exchange(Other._PhysicalDeviceMemoryProperties, nullptr);
        _AllocationSize                 = std::exchange(Other._AllocationSize, 0);
        _MemoryPropertyFlags            = std::exchange(Other._MemoryPropertyFlags, {});
        _MappedDataMemory               = std::exchange(Other._MappedDataMemory, nullptr);
        _MappedTargetMemory             = std::exchange(Other._MappedTargetMemory, nullptr);
        _bPersistentlyMapped            = std::exchange(Other._bPersistentlyMapped, false);
    }

    return *this;
}

vk::Result FVulkanDeviceMemory::MapMemoryForSubmit(vk::DeviceSize Offset, vk::DeviceSize Size, void*& Target)
{
    vk::DeviceSize AdjustedOffset = 0;
    if (!(_MemoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent))
    {
        AdjustedOffset = AlignNonCoherentMemoryRange(Offset, Size);
    }

    vk::Result Result = MapMemory(Offset, Size, Target);
    if (Result == vk::Result::eSuccess && !(_MemoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent))
    {
        Target = static_cast<std::byte*>(Target) + AdjustedOffset;
    }

    _MappedTargetMemory = Target;
    return Result;
}

vk::Result FVulkanDeviceMemory::MapMemoryForFetch(vk::DeviceSize Offset, vk::DeviceSize Size, void*& Data)
{
    vk::DeviceSize AdjustedOffset = 0;
    if (!(_MemoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent))
    {
        AdjustedOffset = AlignNonCoherentMemoryRange(Offset, Size);
    }

    vk::Result Result = MapMemory(Offset, Size, Data);
    if (Result == vk::Result::eSuccess && !(_MemoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent))
    {
        Data = static_cast<std::byte*>(Data) + AdjustedOffset;
        vk::MappedMemoryRange MappedMemoryRange(_Handle, Offset, Size);
        try
        {
            _Device.invalidateMappedMemoryRanges(MappedMemoryRange);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to invalidate mapped memory range: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }
    }

    _MappedDataMemory = Data;
    return Result;
}

vk::Result FVulkanDeviceMemory::UnmapMemory(vk::DeviceSize Offset, vk::DeviceSize Size)
{
    if (!(_MemoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent))
    {
        AlignNonCoherentMemoryRange(Offset, Size);
        vk::MappedMemoryRange MappedMemoryRange(_Handle, Offset, Size);
        try
        {
            _Device.flushMappedMemoryRanges(MappedMemoryRange);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to flush mapped memory range: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }
    }

    _Device.unmapMemory(_Handle);
    _MappedDataMemory   = nullptr;
    _MappedTargetMemory = nullptr;
    return vk::Result::eSuccess;
}

vk::Result FVulkanDeviceMemory::SubmitData(vk::DeviceSize MapOffset, vk::DeviceSize SubmitOffset, vk::DeviceSize Size, const void* Data)
{
    void* Target = nullptr;
    if (!_bPersistentlyMapped || _MappedTargetMemory == nullptr)
    {
        vk::DeviceSize MappedBase = _bPersistentlyMapped ? 0 : MapOffset;
        if (vk::Result Result = MapMemoryForSubmit(MappedBase, Size, Target); Result != vk::Result::eSuccess)
        {
            return Result;
        }
    }
    else
    {
        Target = _MappedTargetMemory;
    }

    std::copy(static_cast<const std::byte*>(Data), static_cast<const std::byte*>(Data) + Size, static_cast<std::byte*>(Target) + SubmitOffset);

    if (!_bPersistentlyMapped)
    {
        return UnmapMemory(MapOffset, Size);
    }

    return vk::Result::eSuccess;
}

vk::Result FVulkanDeviceMemory::FetchData(vk::DeviceSize MapOffset, vk::DeviceSize FetchOffset, vk::DeviceSize Size, void* Target)
{
    void* Data = nullptr;
    if (!_bPersistentlyMapped || _MappedTargetMemory == nullptr)
    {
        vk::DeviceSize MappedBase = _bPersistentlyMapped ? 0 : MapOffset;
        if (vk::Result Result = MapMemoryForFetch(MappedBase, Size, Data); Result != vk::Result::eSuccess)
        {
            return Result;
        }
    }
    else
    {
        Data = _MappedDataMemory;
    }

    std::copy(static_cast<const std::byte*>(Data) + FetchOffset, static_cast<const std::byte*>(Data) + FetchOffset + Size, static_cast<std::byte*>(Target));

    if (!_bPersistentlyMapped)
    {
        return UnmapMemory(MapOffset, Size);
    }

    return vk::Result::eSuccess;
}

vk::Result FVulkanDeviceMemory::AllocateDeviceMemory(const vk::MemoryAllocateInfo& AllocateInfo)
{
    if (AllocateInfo.memoryTypeIndex >= _PhysicalDeviceMemoryProperties->memoryTypeCount)
    {
        NpgsCoreError("Invalid memory type index: {}.", AllocateInfo.memoryTypeIndex);
        return vk::Result::eErrorMemoryMapFailed;
    }

    try
    {
        _Handle = _Device.allocateMemory(AllocateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to allocate memory: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }
    _AllocationSize      = AllocateInfo.allocationSize;
    _MemoryPropertyFlags = _PhysicalDeviceMemoryProperties->memoryTypes[AllocateInfo.memoryTypeIndex].propertyFlags;

    NpgsCoreInfo("Device memory allocated successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanDeviceMemory::MapMemory(vk::DeviceSize Offset, vk::DeviceSize Size, void*& Data) const
{
    try
    {
        Data = _Device.mapMemory(_Handle, Offset, Size, {});
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to map memory: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    // NpgsCoreInfo("Memory mapped successfully.");
    return vk::Result::eSuccess;
}

vk::DeviceSize FVulkanDeviceMemory::AlignNonCoherentMemoryRange(vk::DeviceSize& Offset, vk::DeviceSize& Size) const
{
    vk::DeviceSize NonCoherentAtomSize = _PhysicalDeviceProperties->limits.nonCoherentAtomSize;
    vk::DeviceSize OriginalOffset      = Offset;
    vk::DeviceSize RangeBegin          = Offset;
    vk::DeviceSize RangeEnd            = Offset + Size;

    RangeBegin = RangeBegin                           / NonCoherentAtomSize * NonCoherentAtomSize;
    RangeEnd   = (RangeEnd + NonCoherentAtomSize - 1) / NonCoherentAtomSize * NonCoherentAtomSize;
    RangeEnd   = std::min(RangeEnd, _AllocationSize);

    Offset = RangeBegin;
    Size   = RangeEnd - RangeBegin;

    return OriginalOffset - RangeBegin;
}

// Wrapper for vk::Buffer
// ----------------------
FVulkanBuffer::FVulkanBuffer(const vk::BufferCreateInfo& CreateInfo)
    : FVulkanBuffer(FVulkanCore::GetClassInstance()->GetDevice(),
                    FVulkanCore::GetClassInstance()->GetPhysicalDeviceMemoryProperties(),
                    CreateInfo)
{
}

FVulkanBuffer::FVulkanBuffer(vk::Device Device, const vk::PhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties,
                             const vk::BufferCreateInfo& CreateInfo)
    : Base(Device), _PhysicalDeviceMemoryProperties(&PhysicalDeviceMemoryProperties)
{
    _ReleaseInfo = "Buffer destroyed successfully.";
    _Status      = CreateBuffer(CreateInfo);
}

vk::MemoryAllocateInfo FVulkanBuffer::CreateMemoryAllocateInfo(vk::MemoryPropertyFlags Flags) const
{
    vk::MemoryRequirements MemoryRequirements = _Device.getBufferMemoryRequirements(_Handle);
    std::uint32_t MemoryTypeIndex = GetMemoryTypeIndex(*_PhysicalDeviceMemoryProperties, MemoryRequirements, Flags);
    vk::MemoryAllocateInfo MemoryAllocateInfo(MemoryRequirements.size, MemoryTypeIndex);

    return MemoryAllocateInfo;
}

vk::Result FVulkanBuffer::BindMemory(const FVulkanDeviceMemory& DeviceMemory, vk::DeviceSize Offset) const
{
    try
    {
        _Device.bindBufferMemory(_Handle, *DeviceMemory, Offset);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to bind buffer memory: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Buffer memory bound successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanBuffer::CreateBuffer(const vk::BufferCreateInfo& CreateInfo)
{
    try
    {
        _Handle = _Device.createBuffer(CreateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create buffer: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Buffer created successfully.");
    return vk::Result::eSuccess;
}

// Wrapper for vk::BufferView
// --------------------------
FVulkanBufferView::FVulkanBufferView(const vk::BufferViewCreateInfo& CreateInfo)
    : FVulkanBufferView(FVulkanCore::GetClassInstance()->GetDevice(), CreateInfo)
{
}

FVulkanBufferView::FVulkanBufferView(vk::Device Device, const vk::BufferViewCreateInfo& CreateInfo)
    : Base(Device)
{
    _ReleaseInfo = "Buffer view destroyed successfully.";
    _Status      = CreateBufferView(CreateInfo);
}

FVulkanBufferView::FVulkanBufferView(const FVulkanBuffer& Buffer, vk::Format Format, vk::DeviceSize Offset,
                                     vk::DeviceSize Range, vk::BufferViewCreateFlags Flags)
    : FVulkanBufferView(FVulkanCore::GetClassInstance()->GetDevice(), Buffer, Format, Offset, Range, Flags)
{
}

FVulkanBufferView::FVulkanBufferView(vk::Device Device, const FVulkanBuffer& Buffer, vk::Format Format,
                                     vk::DeviceSize Offset, vk::DeviceSize Range, vk::BufferViewCreateFlags Flags)
    : Base(Device)
{
    _ReleaseInfo = "Buffer view destroyed successfully.";
    _Status      = CreateBufferView(Buffer, Format, Offset, Range, Flags);
}

vk::Result FVulkanBufferView::CreateBufferView(const vk::BufferViewCreateInfo& CreateInfo)
{
    try
    {
        _Handle = _Device.createBufferView(CreateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create buffer view: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Buffer view created successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanBufferView::CreateBufferView(const FVulkanBuffer& Buffer, vk::Format Format, vk::DeviceSize Offset,
                                               vk::DeviceSize Range, vk::BufferViewCreateFlags Flags)
{
    vk::BufferViewCreateInfo CreateInfo(Flags, *Buffer, Format, Offset, Range);
    return CreateBufferView(CreateInfo);
}

// Wrapper for vk::DescriptorSetLayout
// -----------------------------------
FVulkanDescriptorSetLayout::FVulkanDescriptorSetLayout(const vk::DescriptorSetLayoutCreateInfo& CreateInfo)
    : FVulkanDescriptorSetLayout(FVulkanCore::GetClassInstance()->GetDevice(), CreateInfo)
{
}

FVulkanDescriptorSetLayout::FVulkanDescriptorSetLayout(vk::Device Device, const vk::DescriptorSetLayoutCreateInfo& CreateInfo)
    : Base(Device)
{
    _ReleaseInfo = "Descriptor set layout destroyed successfully.";
    _Status      = CreateDescriptorSetLayout(CreateInfo);
}

std::vector<vk::DescriptorSetLayout>
FVulkanDescriptorSetLayout::GetNativeTypeArray(const std::vector<FVulkanDescriptorSetLayout>& Vector)
{
    std::vector<vk::DescriptorSetLayout> NativeArray(Vector.size());
    for (std::size_t i = 0; i != Vector.size(); ++i)
    {
        NativeArray[i] = *Vector[i];
    }
    return NativeArray;
}

vk::Result FVulkanDescriptorSetLayout::CreateDescriptorSetLayout(const vk::DescriptorSetLayoutCreateInfo& CreateInfo)
{
    try
    {
        _Handle = _Device.createDescriptorSetLayout(CreateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create descriptor set layout: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Descriptor set layout created successfully.");
    return vk::Result::eSuccess;
}

// Wrapper for vk::DescriptorPool
// ------------------------------
FVulkanDescriptorPool::FVulkanDescriptorPool(const vk::DescriptorPoolCreateInfo& CreateInfo)
    : FVulkanDescriptorPool(FVulkanCore::GetClassInstance()->GetDevice(), CreateInfo)
{
}

FVulkanDescriptorPool::FVulkanDescriptorPool(vk::Device Device, const vk::DescriptorPoolCreateInfo& CreateInfo)
    : Base(Device)
{
    _ReleaseInfo = "Descriptor pool destroyed successfully.";
    _Status      = CreateDescriptorPool(CreateInfo);
}

FVulkanDescriptorPool::FVulkanDescriptorPool(std::uint32_t MaxSets, const std::vector<vk::DescriptorPoolSize>& PoolSizes,
                                             vk::DescriptorPoolCreateFlags Flags)
    : FVulkanDescriptorPool(FVulkanCore::GetClassInstance()->GetDevice(), MaxSets, PoolSizes, Flags)
{
}

FVulkanDescriptorPool::FVulkanDescriptorPool(vk::Device Device, std::uint32_t MaxSets, const std::vector<vk::DescriptorPoolSize>& PoolSizes,
                                             vk::DescriptorPoolCreateFlags Flags)
    : Base(Device)
{
    _ReleaseInfo = "Descriptor pool destroyed successfully.";
    _Status      = CreateDescriptorPool(MaxSets, PoolSizes, Flags);
}

vk::Result FVulkanDescriptorPool::AllocateSets(const std::vector<vk::DescriptorSetLayout>& Layouts,
                                               std::vector<vk::DescriptorSet>& Sets) const
{
    if (Layouts.size() > Sets.size())
    {
        NpgsCoreError("Descriptor set layout count ({}) is larger than descriptor set count ({}).", Layouts.size(), Sets.size());
        return vk::Result::eErrorInitializationFailed;
    }

    vk::DescriptorSetAllocateInfo DescriptorSetAllocateInfo(_Handle, Layouts);
    try
    {
        Sets = _Device.allocateDescriptorSets(DescriptorSetAllocateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to allocate descriptor sets: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Descriptor sets allocated successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanDescriptorPool::AllocateSets(const std::vector<vk::DescriptorSetLayout>& Layouts,
                                               std::vector<FVulkanDescriptorSet>& Sets) const
{
    std::vector<vk::DescriptorSet> DescriptorSets(Layouts.size());
    if (vk::Result Result = AllocateSets(Layouts, DescriptorSets); Result != vk::Result::eSuccess)
    {
        return Result;
    }
    Sets.resize(DescriptorSets.size());
    for (std::size_t i = 0; i != DescriptorSets.size(); ++i)
    {
        *Sets[i] = DescriptorSets[i];
    }

    return vk::Result::eSuccess;
}

vk::Result FVulkanDescriptorPool::AllocateSets(const std::vector<FVulkanDescriptorSetLayout>& Layouts,
                                               std::vector<vk::DescriptorSet>& Sets) const
{
    std::vector<vk::DescriptorSetLayout> DescriptorSetLayouts;
    DescriptorSetLayouts.reserve(Layouts.size());
    for (auto& Layout : Layouts)
    {
        DescriptorSetLayouts.push_back(*Layout);
    }

    return AllocateSets(DescriptorSetLayouts, Sets);
}

vk::Result FVulkanDescriptorPool::AllocateSets(const std::vector<FVulkanDescriptorSetLayout>& Layouts,
                                               std::vector<FVulkanDescriptorSet>& Sets) const
{
    std::vector<vk::DescriptorSetLayout> DescriptorSetLayouts;
    DescriptorSetLayouts.reserve(Layouts.size());
    for (auto& Layout : Layouts)
    {
        DescriptorSetLayouts.push_back(*Layout);
    }

    std::vector<vk::DescriptorSet> DescriptorSets(Layouts.size());
    if (vk::Result Result = AllocateSets(DescriptorSetLayouts, DescriptorSets); Result != vk::Result::eSuccess)
    {
        return Result;
    }
    Sets.resize(DescriptorSets.size());
    for (std::size_t i = 0; i != DescriptorSets.size(); ++i)
    {
        *Sets[i] = DescriptorSets[i];
    }

    return vk::Result::eSuccess;
}

vk::Result FVulkanDescriptorPool::FreeSets(std::vector<vk::DescriptorSet>& Sets) const
{
    if (!Sets.empty())
    {
        _Device.freeDescriptorSets(_Handle, Sets);
        Sets.clear();
    }

    return vk::Result::eSuccess;
}

vk::Result FVulkanDescriptorPool::FreeSets(std::vector<FVulkanDescriptorSet>& Sets) const
{
    if (!Sets.empty())
    {
        std::vector<vk::DescriptorSet> DescriptorSets;
        DescriptorSets.reserve(Sets.size());
        for (auto& Set : Sets)
        {
            DescriptorSets.push_back(*Set);
        }
        Sets.clear();
        return FreeSets(DescriptorSets);
    }

    return vk::Result::eSuccess;
}

vk::Result FVulkanDescriptorPool::CreateDescriptorPool(const vk::DescriptorPoolCreateInfo& CreateInfo)
{
    try
    {
        _Handle = _Device.createDescriptorPool(CreateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create descriptor pool: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Descriptor pool created successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanDescriptorPool::CreateDescriptorPool(std::uint32_t MaxSets, const std::vector<vk::DescriptorPoolSize>& PoolSizes,
                                                       vk::DescriptorPoolCreateFlags Flags)
{
    vk::DescriptorPoolCreateInfo CreateInfo(Flags, MaxSets, PoolSizes);
    return CreateDescriptorPool(CreateInfo);
}

// Wrapper for vk::Fence
// ---------------------
FVulkanFence::FVulkanFence(const vk::FenceCreateInfo& CreateInfo)
    : FVulkanFence(FVulkanCore::GetClassInstance()->GetDevice(), CreateInfo)
{
}

FVulkanFence::FVulkanFence(vk::Device Device, const vk::FenceCreateInfo& CreateInfo)
    : Base(Device)
{
    // _ReleaseInfo = "Fence destroyed successfully.";
    _Status      = CreateFence(CreateInfo);
}

FVulkanFence::FVulkanFence(vk::FenceCreateFlags Flags)
    : FVulkanFence(FVulkanCore::GetClassInstance()->GetDevice(), Flags)
{
}

FVulkanFence::FVulkanFence(vk::Device Device, vk::FenceCreateFlags Flags)
    : Base(Device)
{
    // _ReleaseInfo = "Fence destroyed successfully.";
    _Status      = CreateFence(Flags);
}

vk::Result FVulkanFence::Wait() const
{
    vk::Result Result;
    try
    {
        Result = _Device.waitForFences(_Handle, vk::True, std::numeric_limits<std::uint64_t>::max());
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to wait for fence: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    return Result;
}

vk::Result FVulkanFence::Reset() const
{
    try
    {
        _Device.resetFences(_Handle);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to reset fence: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    return vk::Result::eSuccess;
}

vk::Result FVulkanFence::WaitAndReset() const
{
    vk::Result WaitResult = Wait();
    if (WaitResult != vk::Result::eSuccess)
    {
        return WaitResult;
    }

    vk::Result ResetResult = Reset();
    return ResetResult;
}

vk::Result FVulkanFence::GetStatus() const
{
    vk::Result Result = _Device.getFenceStatus(_Handle);
    if (Result != vk::Result::eSuccess)
    {
        NpgsCoreError("Failed to get fence status: {}.", vk::to_string(Result));
        return Result;
    }

    return vk::Result::eSuccess;
}

vk::Result FVulkanFence::CreateFence(const vk::FenceCreateInfo& CreateInfo)
{
    try
    {
        _Handle = _Device.createFence(CreateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create fence: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    // NpgsCoreInfo("Fence created successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanFence::CreateFence(vk::FenceCreateFlags Flags)
{
    vk::FenceCreateInfo FenceCreateInfo(Flags);
    return CreateFence(FenceCreateInfo);
}

// Wrapper for vk::Framebuffer
// ---------------------------
FVulkanFramebuffer::FVulkanFramebuffer(const vk::FramebufferCreateInfo& CreateInfo)
    : FVulkanFramebuffer(FVulkanCore::GetClassInstance()->GetDevice(), CreateInfo)
{
}

FVulkanFramebuffer::FVulkanFramebuffer(vk::Device Device, const vk::FramebufferCreateInfo& CreateInfo)
    : Base(Device)
{
    _ReleaseInfo = "Framebuffer destroyed successfully.";
    _Status      = CreateFramebuffer(CreateInfo);
}

vk::Result FVulkanFramebuffer::CreateFramebuffer(const vk::FramebufferCreateInfo& CreateInfo)
{
    try
    {
        _Handle = _Device.createFramebuffer(CreateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create framebuffer: {}.", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Framebuffer created successfully.");
    return vk::Result::eSuccess;
}

// Wrapper for vk::Image
// ---------------------
FVulkanImage::FVulkanImage(vk::ImageCreateInfo& CreateInfo)
    : FVulkanImage(FVulkanCore::GetClassInstance()->GetDevice(),
                   FVulkanCore::GetClassInstance()->GetPhysicalDeviceMemoryProperties(),
                   CreateInfo)
{
}

FVulkanImage::FVulkanImage(vk::Device Device, const vk::PhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties,
                           const vk::ImageCreateInfo& CreateInfo)
    : Base(Device), _PhysicalDeviceMemoryProperties(&PhysicalDeviceMemoryProperties)
{
    _ReleaseInfo = "Image destroyed successfully.";
    _Status      = CreateImage(CreateInfo);
}

vk::MemoryAllocateInfo FVulkanImage::CreateMemoryAllocateInfo(vk::MemoryPropertyFlags Flags) const
{
    vk::MemoryRequirements MemoryRequirements = _Device.getImageMemoryRequirements(_Handle);
    std::uint32_t MemoryTypeIndex = GetMemoryTypeIndex(*_PhysicalDeviceMemoryProperties, MemoryRequirements, Flags);

    if (MemoryTypeIndex == std::numeric_limits<std::uint32_t>::max() &&
        Flags & vk::MemoryPropertyFlagBits::eLazilyAllocated)
    {
        Flags &= ~vk::MemoryPropertyFlagBits::eLazilyAllocated;
        MemoryTypeIndex = GetMemoryTypeIndex(*_PhysicalDeviceMemoryProperties, MemoryRequirements, Flags);
    }

    vk::MemoryAllocateInfo MemoryAllocateInfo(MemoryRequirements.size, MemoryTypeIndex);

    return MemoryAllocateInfo;
}

vk::Result FVulkanImage::BindMemory(const FVulkanDeviceMemory& DeviceMemory, vk::DeviceSize Offset) const
{
    try
    {
        _Device.bindImageMemory(_Handle, *DeviceMemory, Offset);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to bind image memory: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Image memory bound successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanImage::CreateImage(const vk::ImageCreateInfo& CreateInfo)
{
    try
    {
        _Handle = _Device.createImage(CreateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create image: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Image created successfully.");
    return vk::Result::eSuccess;
}

// Wrapper for vk::ImageView
// -------------------------
FVulkanImageView::FVulkanImageView(const vk::ImageViewCreateInfo& CreateInfo)
    : FVulkanImageView(FVulkanCore::GetClassInstance()->GetDevice(), CreateInfo)
{
}

FVulkanImageView::FVulkanImageView(vk::Device Device, const vk::ImageViewCreateInfo& CreateInfo)
    : Base(Device)
{
    _ReleaseInfo = "Image view destroyed successfully.";
    _Status      = CreateImageView(CreateInfo);
}

FVulkanImageView::FVulkanImageView(const FVulkanImage& Image, vk::ImageViewType ViewType, vk::Format Format,
                                   vk::ComponentMapping Components, vk::ImageSubresourceRange SubresourceRange,
                                   vk::ImageViewCreateFlags Flags)
    : FVulkanImageView(FVulkanCore::GetClassInstance()->GetDevice(), Image, ViewType, Format, Components, SubresourceRange, Flags)
{
}

FVulkanImageView::FVulkanImageView(vk::Device Device, const FVulkanImage& Image, vk::ImageViewType ViewType,
                                   vk::Format Format, vk::ComponentMapping Components,
                                   vk::ImageSubresourceRange SubresourceRange, vk::ImageViewCreateFlags Flags)
    : Base(Device)
{
    _ReleaseInfo = "Image view destroyed successfully.";
    _Status      = CreateImageView(Image, ViewType, Format, Components, SubresourceRange, Flags);
}

vk::Result FVulkanImageView::CreateImageView(const vk::ImageViewCreateInfo& CreateInfo)
{
    try
    {
        _Handle = _Device.createImageView(CreateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create image view: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Image view created successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanImageView::CreateImageView(const FVulkanImage& Image, vk::ImageViewType ViewType, vk::Format Format,
                                             vk::ComponentMapping Components, vk::ImageSubresourceRange SubresourceRange,
                                             vk::ImageViewCreateFlags Flags)
{
    vk::ImageViewCreateInfo CreateInfo(Flags, *Image, ViewType, Format, Components, SubresourceRange);
    return CreateImageView(CreateInfo);
}

// Wrapper for vk::PipelineCache
// -----------------------------
FVulkanPipelineCache::FVulkanPipelineCache(vk::PipelineCacheCreateFlags Flags)
    : FVulkanPipelineCache(FVulkanCore::GetClassInstance()->GetDevice(), Flags)
{
}

FVulkanPipelineCache::FVulkanPipelineCache(vk::Device Device, vk::PipelineCacheCreateFlags Flags)
    : Base(Device)
{
    _ReleaseInfo = "Pipeline cache destroyed successfully.";
    _Status      = CreatePipelineCache(Flags);
}

FVulkanPipelineCache::FVulkanPipelineCache(vk::PipelineCacheCreateFlags Flags, const std::vector<std::byte>& InitialData)
    : FVulkanPipelineCache(FVulkanCore::GetClassInstance()->GetDevice(), Flags, InitialData)
{
}

FVulkanPipelineCache::FVulkanPipelineCache(vk::Device Device, vk::PipelineCacheCreateFlags Flags, const std::vector<std::byte>& InitialData)
    : Base(Device)
{
    _ReleaseInfo = "Pipeline cache destroyed successfully.";
    _Status      = CreatePipelineCache(Flags, InitialData);
}

FVulkanPipelineCache::FVulkanPipelineCache(const vk::PipelineCacheCreateInfo& CreateInfo)
    : FVulkanPipelineCache(FVulkanCore::GetClassInstance()->GetDevice(), CreateInfo)
{
}

FVulkanPipelineCache::FVulkanPipelineCache(vk::Device Device, const vk::PipelineCacheCreateInfo& CreateInfo)
    : Base(Device)
{
    _ReleaseInfo = "Pipeline cache destroyed successfully.";
    _Status      = CreatePipelineCache(CreateInfo);
}

vk::Result FVulkanPipelineCache::CreatePipelineCache(vk::PipelineCacheCreateFlags Flags)
{
    vk::PipelineCacheCreateInfo CreateInfo(Flags);
    return CreatePipelineCache(CreateInfo);
}

vk::Result FVulkanPipelineCache::CreatePipelineCache(vk::PipelineCacheCreateFlags Flags, const std::vector<std::byte>& InitialData)
{
    vk::PipelineCacheCreateInfo CreateInfo = vk::PipelineCacheCreateInfo()
        .setFlags(Flags)
        .setInitialData<std::byte>(InitialData);

    return CreatePipelineCache(CreateInfo);
}

vk::Result FVulkanPipelineCache::CreatePipelineCache(const vk::PipelineCacheCreateInfo& CreateInfo)
{
    try
    {
        _Handle = _Device.createPipelineCache(CreateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create pipeline cache: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Pipeline cache created successfully.");
    return vk::Result::eSuccess;
}

// Wrapper for vk::Pipeline
// ------------------------
FVulkanPipeline::FVulkanPipeline(const vk::GraphicsPipelineCreateInfo& CreateInfo, const FVulkanPipelineCache* Cache)
    : FVulkanPipeline(FVulkanCore::GetClassInstance()->GetDevice(), CreateInfo, Cache)
{
}

FVulkanPipeline::FVulkanPipeline(vk::Device Device, const vk::GraphicsPipelineCreateInfo& CreateInfo,
                                 const FVulkanPipelineCache* Cache)
    : Base(Device)
{
    _ReleaseInfo = "Graphics pipeline destroyed successfully.";
    _Status      = CreateGraphicsPipeline(CreateInfo, Cache);
}

FVulkanPipeline::FVulkanPipeline(const vk::ComputePipelineCreateInfo& CreateInfo, const FVulkanPipelineCache* Cache)
    : FVulkanPipeline(FVulkanCore::GetClassInstance()->GetDevice(), CreateInfo, Cache)
{
}

FVulkanPipeline::FVulkanPipeline(vk::Device Device, const vk::ComputePipelineCreateInfo& CreateInfo,
                                 const FVulkanPipelineCache* Cache)
    : Base(Device)
{
    _ReleaseInfo = "Compute pipeline destroyed successfully.";
    _Status      = CreateComputePipeline(CreateInfo, Cache);
}

vk::Result FVulkanPipeline::CreateGraphicsPipeline(const vk::GraphicsPipelineCreateInfo& CreateInfo,
                                                   const FVulkanPipelineCache* Cache)
{
    try
    {
        vk::PipelineCache PipelineCache = Cache ? **Cache : vk::PipelineCache();
        _Handle = _Device.createGraphicsPipeline(PipelineCache, CreateInfo).value;
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create graphics pipeline: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Graphics pipeline created successfully");
    return vk::Result::eSuccess;
}

vk::Result FVulkanPipeline::CreateComputePipeline(const vk::ComputePipelineCreateInfo& CreateInfo,
                                                  const FVulkanPipelineCache* Cache)
{
    try
    {
        vk::PipelineCache PipelineCache = Cache ? **Cache : vk::PipelineCache();
        _Handle = _Device.createComputePipeline(PipelineCache, CreateInfo).value;
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create compute pipeline: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Compute pipeline created successfully");
    return vk::Result::eSuccess;
}

// Wrapper for vk::PipelineLayout
// ------------------------------
FVulkanPipelineLayout::FVulkanPipelineLayout(const vk::PipelineLayoutCreateInfo& CreateInfo)
    : FVulkanPipelineLayout(FVulkanCore::GetClassInstance()->GetDevice(), CreateInfo)
{
}

FVulkanPipelineLayout::FVulkanPipelineLayout(vk::Device Device, const vk::PipelineLayoutCreateInfo& CreateInfo)
    : Base(Device)
{
    _ReleaseInfo = "Pipeline layout destroyed successfully.";
    _Status      = CreatePipelineLayout(CreateInfo);
}

vk::Result FVulkanPipelineLayout::CreatePipelineLayout(const vk::PipelineLayoutCreateInfo& CreateInfo)
{
    try
    {
        _Handle = _Device.createPipelineLayout(CreateInfo);
        return vk::Result::eSuccess;
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create pipeline layout: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Pipeline layout created successfully");
    return vk::Result::eSuccess;
}

// Wrapper for vk::RenderPass
// --------------------------
FVulkanRenderPass::FVulkanRenderPass(const vk::RenderPassCreateInfo& CreateInfo)
    : FVulkanRenderPass(FVulkanCore::GetClassInstance()->GetDevice(), CreateInfo)
{
}

FVulkanRenderPass::FVulkanRenderPass(vk::Device Device, const vk::RenderPassCreateInfo& CreateInfo)
    : Base(Device)
{
    _ReleaseInfo = "Render pass destroyed successfully.";
    _Status      = CreateRenderPass(CreateInfo);
}

void FVulkanRenderPass::CommandBegin(const FVulkanCommandBuffer& CommandBuffer, const FVulkanFramebuffer& Framebuffer,
                                     const vk::Rect2D& RenderArea, const std::vector<vk::ClearValue>& ClearValues,
                                     const vk::SubpassContents& SubpassContents) const
{
    vk::RenderPassBeginInfo RenderPassBeginInfo(_Handle, *Framebuffer, RenderArea, ClearValues);
    CommandBegin(CommandBuffer, RenderPassBeginInfo, SubpassContents);
}

vk::Result FVulkanRenderPass::CreateRenderPass(const vk::RenderPassCreateInfo& CreateInfo)
{
    try
    {
        _Handle = _Device.createRenderPass(CreateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create render pass: {}.", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Render pass created successfully.");
    return vk::Result::eSuccess;
}

// Wrapper for vk::Sampler
// -----------------------
FVulkanSampler::FVulkanSampler(const vk::SamplerCreateInfo& CreateInfo)
    : FVulkanSampler(FVulkanCore::GetClassInstance()->GetDevice(), CreateInfo)
{
}

FVulkanSampler::FVulkanSampler(vk::Device Device, const vk::SamplerCreateInfo& CreateInfo)
    : Base(Device)
{
    _ReleaseInfo = "Sampler destroyed successfully.";
    _Status      = CreateSampler(CreateInfo);
}

vk::Result FVulkanSampler::CreateSampler(const vk::SamplerCreateInfo& CreateInfo)
{
    try
    {
        _Handle = _Device.createSampler(CreateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create sampler: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Sampler created successfully.");
    return vk::Result::eSuccess;
}

// Wrapper for vk::Semaphore
// -------------------------
FVulkanSemaphore::FVulkanSemaphore(const vk::SemaphoreCreateInfo& CreateInfo)
    : FVulkanSemaphore(FVulkanCore::GetClassInstance()->GetDevice(), CreateInfo)
{
}

FVulkanSemaphore::FVulkanSemaphore(vk::Device Device, const vk::SemaphoreCreateInfo& CreateInfo)
    : Base(Device)
{
    _ReleaseInfo = "Semaphore destroyed successfully.";
    _Status      = CreateSemaphore(CreateInfo);
}

FVulkanSemaphore::FVulkanSemaphore(vk::SemaphoreCreateFlags Flags)
    : FVulkanSemaphore(FVulkanCore::GetClassInstance()->GetDevice(), Flags)
{
}

FVulkanSemaphore::FVulkanSemaphore(vk::Device Device, vk::SemaphoreCreateFlags Flags)
    : Base(Device)
{
    _ReleaseInfo = "Semaphore destroyed successfully.";
    _Status      = CreateSemaphore(Flags);
}

vk::Result FVulkanSemaphore::CreateSemaphore(const vk::SemaphoreCreateInfo& CreateInfo)
{
    try
    {
        _Handle = _Device.createSemaphore(CreateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create semaphore: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Semaphore created successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanSemaphore::CreateSemaphore(vk::SemaphoreCreateFlags Flags)
{
    vk::SemaphoreCreateInfo SemaphoreCreateInfo(Flags);
    return CreateSemaphore(SemaphoreCreateInfo);
}

// Wrapper for vk::ShaderModule
// ----------------------------
FVulkanShaderModule::FVulkanShaderModule(const vk::ShaderModuleCreateInfo& CreateInfo)
    : FVulkanShaderModule(FVulkanCore::GetClassInstance()->GetDevice(), CreateInfo)
{
}

FVulkanShaderModule::FVulkanShaderModule(vk::Device Device, const vk::ShaderModuleCreateInfo& CreateInfo)
    : Base(Device)
{
    _ReleaseInfo = "Shader module destroyed successfully.";
    _Status      = CreateShaderModule(CreateInfo);
}

// FVulkanShaderModule::FVulkanShaderModule(const std::string& Filename)
//     : FVulkanShaderModule(FVulkanCore::GetClassInstance()->GetDevice(), Filename)
// {
// }

// FVulkanShaderModule::FVulkanShaderModule(vk::Device Device, const std::string& Filename)
//     : Base(Device)
// {
//     _ReleaseInfo = "Shader module destroyed successfully.";
//     _Status      = CreateShaderModule(Filename);
// }

vk::Result FVulkanShaderModule::CreateShaderModule(const vk::ShaderModuleCreateInfo& CreateInfo)
{
    try
    {
        _Handle = _Device.createShaderModule(CreateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create shader module: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Shader module created successfully.");
    return vk::Result::eSuccess;
}

// vk::Result FVulkanShaderModule::CreateShaderModule(const std::string& Filename)
// {
//     std::ifstream ShaderFile(Filename, std::ios::ate | std::ios::binary);
//     if (!ShaderFile.is_open())
//     {
//         NpgsCoreError("Failed to open shader file: {}", Filename);
//         return vk::Result::eErrorInitializationFailed;
//     }
// 
//     std::size_t FileSize = static_cast<std::size_t>(ShaderFile.tellg());
//     std::vector<std::uint32_t> ShaderCode(FileSize / sizeof(std::uint32_t));
//     ShaderFile.seekg(0);
//     ShaderFile.read(reinterpret_cast<char*>(ShaderCode.data()), FileSize);
//     ShaderFile.close();
// 
//     vk::ShaderModuleCreateInfo ShaderModuleCreateInfo({}, ShaderCode);
//     return CreateShaderModule(ShaderModuleCreateInfo);
// }
// -------------------
// Native wrappers end

FVulkanBufferMemory::FVulkanBufferMemory(const vk::BufferCreateInfo& BufferCreateInfo, vk::MemoryPropertyFlags MemoryPropertyFlags)
    : FVulkanBufferMemory(FVulkanCore::GetClassInstance()->GetDevice(),
                          FVulkanCore::GetClassInstance()->GetPhysicalDeviceProperties(),
                          FVulkanCore::GetClassInstance()->GetPhysicalDeviceMemoryProperties(),
                          BufferCreateInfo, MemoryPropertyFlags)
{
}

FVulkanBufferMemory::FVulkanBufferMemory(vk::Device Device, const vk::PhysicalDeviceProperties& PhysicalDeviceProperties,
                                         const vk::PhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties,
                                         const vk::BufferCreateInfo& BufferCreateInfo, vk::MemoryPropertyFlags MemoryPropertyFlags)
    : Base(nullptr, std::make_unique<FVulkanBuffer>(Device, PhysicalDeviceMemoryProperties, BufferCreateInfo))
{
    auto MemoryAllocateInfo = _Resource->CreateMemoryAllocateInfo(MemoryPropertyFlags);
    _Memory = std::make_unique<FVulkanDeviceMemory>(
        Device, PhysicalDeviceProperties, PhysicalDeviceMemoryProperties, MemoryAllocateInfo);

    _bMemoryBound = _Memory->IsValid() && (_Resource->BindMemory(*_Memory) == vk::Result::eSuccess);
}

FVulkanImageMemory::FVulkanImageMemory(const vk::ImageCreateInfo& ImageCreateInfo, vk::MemoryPropertyFlags MemoryPropertyFlags)
    : FVulkanImageMemory(FVulkanCore::GetClassInstance()->GetDevice(),
                         FVulkanCore::GetClassInstance()->GetPhysicalDeviceProperties(),
                         FVulkanCore::GetClassInstance()->GetPhysicalDeviceMemoryProperties(),
                         ImageCreateInfo, MemoryPropertyFlags)
{
}

FVulkanImageMemory::FVulkanImageMemory(vk::Device Device, const vk::PhysicalDeviceProperties& PhysicalDeviceProperties,
                                       const vk::PhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties,
                                       const vk::ImageCreateInfo& ImageCreateInfo, vk::MemoryPropertyFlags MemoryPropertyFlags)
    : Base(nullptr, std::make_unique<FVulkanImage>(Device, PhysicalDeviceMemoryProperties, ImageCreateInfo))
{
    auto MemoryAllocateInfo = _Resource->CreateMemoryAllocateInfo(MemoryPropertyFlags);
    _Memory = std::make_unique<FVulkanDeviceMemory>(
        Device, PhysicalDeviceProperties, PhysicalDeviceMemoryProperties, MemoryAllocateInfo);
    _bMemoryBound = _Memory->IsValid() && (_Resource->BindMemory(*_Memory) == vk::Result::eSuccess);
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
