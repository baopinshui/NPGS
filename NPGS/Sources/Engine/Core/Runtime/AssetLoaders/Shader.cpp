#include "Shader.h"

#include <cstddef>
#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <set>
#include <utility>

#include <spirv_cross/spirv_reflect.hpp>

#include "Engine/Core/Base/Config/EngineConfig.h"
#include "Engine/Core/Runtime/AssetLoaders/GetAssetFullPath.h"
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_ASSET_BEGIN

namespace
{
    vk::ShaderStageFlagBits GetShaderStageFromFilename(const std::string& Filename)
    {
        if (Filename.find("vert") != std::string::npos)
        {
            return vk::ShaderStageFlagBits::eVertex;
        }
        else if (Filename.find("frag") != std::string::npos)
        {
            return vk::ShaderStageFlagBits::eFragment;
        }
        else if (Filename.find("comp") != std::string::npos)
        {
            return vk::ShaderStageFlagBits::eCompute;
        }
        else if (Filename.find("geom") != std::string::npos)
        {
            return vk::ShaderStageFlagBits::eGeometry;
        }
        else if (Filename.find("tesc") != std::string::npos)
        {
            return vk::ShaderStageFlagBits::eTessellationControl;
        }
        else if (Filename.find("tese") != std::string::npos)
        {
            return vk::ShaderStageFlagBits::eTessellationEvaluation;
        }
        else
        {
            return vk::ShaderStageFlagBits::eAll;
        }
    }

    vk::Format GetVectorFormat(spirv_cross::SPIRType::BaseType BaseType, std::uint32_t Components)
    {
        switch (BaseType)
        {
        case spirv_cross::SPIRType::BaseType::Int:
            switch (Components)
            {
            case 1:
                return vk::Format::eR32Sint;
            case 2:
                return vk::Format::eR32G32Sint;
            case 3:
                return vk::Format::eR32G32B32Sint;
            case 4:
                return vk::Format::eR32G32B32A32Sint;
            default:
                return vk::Format::eUndefined;
            }
        case spirv_cross::SPIRType::BaseType::UInt:
            switch (Components)
            {
            case 1:
                return vk::Format::eR32Uint;
            case 2:
                return vk::Format::eR32G32Uint;
            case 3:
                return vk::Format::eR32G32B32Uint;
            case 4:
                return vk::Format::eR32G32B32A32Uint;
            default:
                return vk::Format::eUndefined;
            }
        case spirv_cross::SPIRType::BaseType::Float:
            switch (Components)
            {
            case 1:
                return vk::Format::eR32Sfloat;
            case 2:
                return vk::Format::eR32G32Sfloat;
            case 3:
                return vk::Format::eR32G32B32Sfloat;
            case 4:
                return vk::Format::eR32G32B32A32Sfloat;
            default:
                return vk::Format::eUndefined;
            }
        case spirv_cross::SPIRType::BaseType::Double:
            switch (Components)
            {
            case 1:
                return vk::Format::eR64Sfloat;
            case 2:
                return vk::Format::eR64G64Sfloat;
            case 3:
                return vk::Format::eR64G64B64Sfloat;
            case 4:
                return vk::Format::eR64G64B64A64Sfloat;
            default:
                return vk::Format::eUndefined;
            }
        default:
            return vk::Format::eUndefined;
        }
    }

    std::uint32_t GetTypeSize(spirv_cross::SPIRType::BaseType BaseType)
    {
        switch (BaseType)
        {
        case spirv_cross::SPIRType::BaseType::Int:
            return sizeof(int);
        case spirv_cross::SPIRType::BaseType::UInt:
            return sizeof(unsigned int);
        case spirv_cross::SPIRType::BaseType::Float:
            return sizeof(float);
        case spirv_cross::SPIRType::BaseType::Double:
            return sizeof(double);
        default:
            return 0;
        }
    }
}

FShader::FShader(const std::vector<std::string>& ShaderFiles, const FResourceInfo& ResourceInfo)
{
    InitializeShaders(ShaderFiles, ResourceInfo);
    CreateDescriptors();
}

FShader::FShader(FShader&& Other) noexcept
    :
    _ShaderModules(std::move(Other._ShaderModules)),
    _ReflectionInfo(std::exchange(Other._ReflectionInfo, {})),
    _DescriptorSetLayoutsMap(std::move(Other._DescriptorSetLayoutsMap)),
    _DescriptorSetsMap(std::move(Other._DescriptorSetsMap)),
    _DescriptorSets(std::move(Other._DescriptorSets)),
    _DescriptorPool(std::move(Other._DescriptorPool)),
    _bDescriptorSetsNeedUpdate(std::exchange(Other._bDescriptorSetsNeedUpdate, false))
{
}

FShader& FShader::operator=(FShader&& Other) noexcept
{
    if (this != &Other)
    {
        _ShaderModules           = std::move(Other._ShaderModules);
        _ReflectionInfo          = std::exchange(Other._ReflectionInfo, {});
        _DescriptorSetLayoutsMap = std::move(Other._DescriptorSetLayoutsMap);
        _DescriptorSetsMap       = std::move(Other._DescriptorSetsMap);
        _DescriptorSets          = std::move(Other._DescriptorSets);
        _DescriptorPool          = std::move(Other._DescriptorPool);
    }

    return *this;
}

std::vector<vk::PipelineShaderStageCreateInfo> FShader::GetShaderStageCreateInfo() const
{
    std::vector<vk::PipelineShaderStageCreateInfo> ShaderStageCreateInfos;
    for (const auto& [Stage, ShaderModule] : _ShaderModules)
    {
        vk::PipelineShaderStageCreateInfo ShaderStageCreateInfo({}, Stage, *ShaderModule, "main");
        ShaderStageCreateInfos.push_back(ShaderStageCreateInfo);
    }

    return ShaderStageCreateInfos;
}

std::vector<vk::DescriptorSetLayout> FShader::GetDescriptorSetLayouts() const
{
    std::vector<vk::DescriptorSetLayout> NativeTypeLayouts;
    for (const auto& [Set, Layouts] : _DescriptorSetLayoutsMap)
    {
        NativeTypeLayouts.push_back(*Layouts.front());
    }

    return NativeTypeLayouts;
}

void FShader::InitializeShaders(const std::vector<std::string>& ShaderFiles, const FResourceInfo& ResourceInfo)
{
    for (const auto& Filename : ShaderFiles)
    {
        FShaderInfo ShaderInfo = LoadShader(GetAssetFullPath(EAssetType::kShader, Filename));
        if (ShaderInfo.Code.empty())
        {
            return;
        }

        vk::ShaderModuleCreateInfo ShaderModuleCreateInfo({}, ShaderInfo.Code);
        Graphics::FVulkanShaderModule ShaderModule(ShaderModuleCreateInfo);
        _ShaderModules.emplace_back(ShaderInfo.Stage, std::move(ShaderModule));

        ReflectShader(ShaderInfo, ResourceInfo);
    }
}

FShader::FShaderInfo FShader::LoadShader(const std::string& Filename)
{
    if (!std::filesystem::exists(Filename))
    {
        NpgsCoreError("Failed to load shader: \"{}\": No such file or directory.", Filename);
        return {};
    }

    std::ifstream ShaderFile(Filename, std::ios::ate | std::ios::binary);
    if (!ShaderFile.is_open())
    {
        NpgsCoreError("Failed to open shader: \"{}\": Access denied.", Filename);
        return {};
    }

    std::size_t FileSize = static_cast<std::size_t>(ShaderFile.tellg());
    std::vector<std::uint32_t> ShaderCode(FileSize / sizeof(std::uint32_t));
    ShaderFile.seekg(0);
    ShaderFile.read(reinterpret_cast<char*>(ShaderCode.data()), static_cast<std::streamsize>(FileSize));
    ShaderFile.close();

    vk::ShaderStageFlagBits Stage = GetShaderStageFromFilename(Filename);
    return { ShaderCode, Stage };
}

void FShader::ReflectShader(const FShaderInfo& ShaderInfo, const FResourceInfo& ResourceInfo)
{
    std::unique_ptr<spirv_cross::CompilerReflection> Reflection;
    std::unique_ptr<spirv_cross::ShaderResources>    Resources;
    try
    {
        Reflection = std::make_unique<spirv_cross::CompilerReflection>(ShaderInfo.Code);
        Resources  = std::make_unique<spirv_cross::ShaderResources>(Reflection->get_shader_resources());
    }
    catch (const spirv_cross::CompilerError& e)
    {
        NpgsCoreError("SPIR-V Cross compiler error: {}", e.what());
        return;
    }
    catch (const std::exception& e)
    {
        NpgsCoreError("Shader reflection failed: {}", e.what());
        return;
    }

    for (const auto& PushConstant : Resources->push_constant_buffers)
    {
        const auto&   Type        = Reflection->get_type(PushConstant.type_id);
        std::size_t   BufferSize  = Reflection->get_declared_struct_size(Type);
        std::uint32_t TotalOffset = 0;

        if (_ReflectionInfo.PushConstants.size() > 0)
        {
            TotalOffset = _ReflectionInfo.PushConstants.back().offset + _ReflectionInfo.PushConstants.back().size;
        }

        const auto& PushConstantNames = ResourceInfo.PushConstantInfos.at(ShaderInfo.Stage);
        for (std::uint32_t i = 0; i != Type.member_types.size(); ++i)
        {
            const std::string& MemberName   = PushConstantNames[i];
            std::uint32_t      MemberOffset = Reflection->get_member_decoration(Type.self, i, spv::DecorationOffset);
            const auto&        MemberType   = Reflection->get_type(Type.member_types[i]);

            _PushConstantOffsetsMap[MemberName] = MemberOffset;
            NpgsCoreTrace("  Member \"{}\" at offset={}", MemberName, MemberOffset);
        }

        NpgsCoreTrace("Push Constant \"{}\" size={} bytes, offset={}", PushConstant.name, BufferSize - TotalOffset, TotalOffset);
        vk::PushConstantRange PushConstantRange(ShaderInfo.Stage, TotalOffset, static_cast<std::uint32_t>(BufferSize - TotalOffset));
        _ReflectionInfo.PushConstants.push_back(PushConstantRange);
    }

    std::unordered_map<std::uint64_t, bool> DynemicBufferMap;
    for (const auto& Buffer : ResourceInfo.UniformBufferInfos)
    {
        std::uint64_t Key = (static_cast<std::uint64_t>(Buffer.Set) << 32) | Buffer.Binding;
        DynemicBufferMap[Key] = Buffer.bIsDynamic;
    }

    auto CheckDynamic = [&DynemicBufferMap](std::uint32_t Set, std::uint32_t Binding) -> bool
    {
        std::uint64_t Key = (static_cast<std::uint64_t>(Set) << 32) | Binding;
        auto it = DynemicBufferMap.find(Key);
        return it != DynemicBufferMap.end() ? it->second : false;
    };

    for (const auto& UniformBuffer : Resources->uniform_buffers)
    {
        std::uint32_t Set     = Reflection->get_decoration(UniformBuffer.id, spv::DecorationDescriptorSet);
        std::uint32_t Binding = Reflection->get_decoration(UniformBuffer.id, spv::DecorationBinding);

        const auto&   Type       = Reflection->get_type(UniformBuffer.type_id);
        std::uint32_t ArraySize  = Type.array.empty() ? 1 : Type.array[0];
        bool          bIsDynamic = CheckDynamic(Set, Binding);

        NpgsCoreTrace("UBO \"{}\" at set={}, binding={} is {}, array_size={}",
                      UniformBuffer.name, Set, Binding, bIsDynamic ? "dynamic" : "static", ArraySize);

        vk::DescriptorSetLayoutBinding LayoutBinding = vk::DescriptorSetLayoutBinding()
            .setBinding(Binding)
            .setDescriptorType(bIsDynamic ? vk::DescriptorType::eUniformBufferDynamic : vk::DescriptorType::eUniformBuffer)
            .setDescriptorCount(ArraySize)
            .setStageFlags(ShaderInfo.Stage);

        _ReflectionInfo.DescriptorSetBindings[Set].push_back(LayoutBinding);

        vk::DescriptorPoolSize PoolSize(LayoutBinding.descriptorType, Config::Graphics::kMaxFrameInFlight);
    }

    for (const auto& StorageBuffer : Resources->storage_buffers)
    {
        std::uint32_t Set     = Reflection->get_decoration(StorageBuffer.id, spv::DecorationDescriptorSet);
        std::uint32_t Binding = Reflection->get_decoration(StorageBuffer.id, spv::DecorationBinding);

        const auto&   Type       = Reflection->get_type(StorageBuffer.type_id);
        std::uint32_t ArraySize  = Type.array.empty() ? 1 : Type.array[0];
        bool          bIsDynamic = CheckDynamic(Set, Binding);

        NpgsCoreTrace("SSBO \"{}\" at set={}, binding={} is {}, array_size={}",
                      StorageBuffer.name, Set, Binding, bIsDynamic ? "dynamic" : "static", ArraySize);

        vk::DescriptorSetLayoutBinding LayoutBinding = vk::DescriptorSetLayoutBinding()
            .setBinding(Binding)
            .setDescriptorType(bIsDynamic ? vk::DescriptorType::eStorageBufferDynamic : vk::DescriptorType::eStorageBuffer)
            .setDescriptorCount(ArraySize)
            .setStageFlags(ShaderInfo.Stage);
    }

    for (const auto& CombinedSampler : Resources->sampled_images)
    {
        std::uint32_t Set     = Reflection->get_decoration(CombinedSampler.id, spv::DecorationDescriptorSet);
        std::uint32_t Binding = Reflection->get_decoration(CombinedSampler.id, spv::DecorationBinding);

        const auto&   Type      = Reflection->get_type(CombinedSampler.type_id);
        std::uint32_t ArraySize = Type.array.empty() ? 1 : Type.array[0];

        NpgsCoreTrace("Combined Sampler \"{}\" at set={}, binding={}, array_size={}",
                      CombinedSampler.name, Set, Binding, ArraySize);

        vk::DescriptorSetLayoutBinding LayoutBinding = vk::DescriptorSetLayoutBinding()
            .setBinding(Binding)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setDescriptorCount(ArraySize)
            .setStageFlags(ShaderInfo.Stage);

        _ReflectionInfo.DescriptorSetBindings[Set].push_back(LayoutBinding);
    }

    for (const auto& Sampler : Resources->separate_samplers)
    {
        std::uint32_t Set     = Reflection->get_decoration(Sampler.id, spv::DecorationDescriptorSet);
        std::uint32_t Binding = Reflection->get_decoration(Sampler.id, spv::DecorationBinding);

        const auto&   Type      = Reflection->get_type(Sampler.type_id);
        std::uint32_t ArraySize = Type.array.empty() ? 1 : Type.array[0];

        NpgsCoreTrace("Separate Sampler \"{}\" at set={}, binding={}, array_size={}", Sampler.name, Set, Binding, ArraySize);

        vk::DescriptorSetLayoutBinding LayoutBinding = vk::DescriptorSetLayoutBinding()
            .setBinding(Binding)
            .setDescriptorType(vk::DescriptorType::eSampler)
            .setDescriptorCount(ArraySize)
            .setStageFlags(ShaderInfo.Stage);

        _ReflectionInfo.DescriptorSetBindings[Set].push_back(LayoutBinding);
    }

    for (const auto& Image : Resources->separate_images)
    {
        std::uint32_t Set     = Reflection->get_decoration(Image.id, spv::DecorationDescriptorSet);
        std::uint32_t Binding = Reflection->get_decoration(Image.id, spv::DecorationBinding);

        const auto&   Type      = Reflection->get_type(Image.type_id);
        std::uint32_t ArraySize = Type.array.empty() ? 1 : Type.array[0];

        NpgsCoreTrace("Separate Image \"{}\" at set={}, binding={}, array_size={}", Image.name, Set, Binding, ArraySize);

        vk::DescriptorSetLayoutBinding LayoutBinding = vk::DescriptorSetLayoutBinding()
            .setBinding(Binding)
            .setDescriptorType(vk::DescriptorType::eSampledImage)
            .setDescriptorCount(ArraySize)
            .setStageFlags(ShaderInfo.Stage);

        _ReflectionInfo.DescriptorSetBindings[Set].push_back(LayoutBinding);
    }

    if (ShaderInfo.Stage == vk::ShaderStageFlagBits::eVertex)
    {
        std::unordered_map<std::uint32_t, FVertexBufferInfo> BufferMap;
        for (const auto& Buffer : ResourceInfo.VertexBufferInfos)
        {
            BufferMap[Buffer.Binding] = Buffer;
        }

        std::unordered_map<std::uint32_t, std::pair<std::uint32_t, std::uint32_t>> LocationMap;
        for (const auto& Vertex : ResourceInfo.VertexAttributeInfos)
        {
            LocationMap[Vertex.Location] = { Vertex.Binding, Vertex.Offset };
        }

        std::uint32_t CurrentBinding = 0;
        std::uint32_t CurrentOffset  = 0;

        std::set<vk::VertexInputBindingDescription> UniqueBindings;

        for (const auto& Input : Resources->stage_inputs)
        {
            const auto&   Type     = Reflection->get_type(Input.type_id);
            std::uint32_t Location = Reflection->get_decoration(Input.id, spv::DecorationLocation);

            auto LocationIt = LocationMap.find(Location);
            std::uint32_t Binding = LocationIt != LocationMap.end() ? LocationIt->second.first  : CurrentBinding;
            std::uint32_t Offset  = LocationIt != LocationMap.end() ? LocationIt->second.second : 0;

            auto BufferIt = BufferMap.find(Binding);
            std::uint32_t Stride = BufferIt != BufferMap.end() ? BufferIt->second.Stride : GetTypeSize(Type.basetype) * Type.vecsize;
            bool bIsPerInstance  = BufferIt != BufferMap.end() ? BufferIt->second.bIsPerInstance : false;

            UniqueBindings.emplace(Binding, Stride, bIsPerInstance ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex);

            bool bIsMatrix = Type.columns > 1;
            if (bIsMatrix)
            {
                Stride = GetTypeSize(Type.basetype) * Type.columns * Type.vecsize;
                for (std::uint32_t Column = 0; Column != Type.columns; ++Column)
                {
                    _ReflectionInfo.VertexInputAttributes.emplace_back(
                        Location + Column,
                        Binding,
                        GetVectorFormat(Type.basetype, Type.vecsize),
                        Offset + GetTypeSize(Type.basetype) * Column * Type.vecsize
                    );
                }

                NpgsCoreTrace("Vertex Attribute \"{}\" at location={}, binding={}, offset={}, stride={}, rate={} (matrix)",
                              Input.name, Location, Binding, Offset, Stride, bIsPerInstance ? "per instance" : "per vertex");
            }
            else
            {
                _ReflectionInfo.VertexInputAttributes.emplace_back(
                    Location,
                    Binding,
                    GetVectorFormat(Type.basetype, Type.vecsize),
                    Offset
                );

                NpgsCoreTrace("Vertex Attribute \"{}\" at location={}, binding={}, offset={}, stride={}, rate={}",
                              Input.name, Location, Binding, Offset, Stride, bIsPerInstance ? "per instance" : "per vertex");
            }

            if (LocationIt == LocationMap.end())
            {
                ++CurrentBinding;
            }
        }

        _ReflectionInfo.VertexInputBindings =
            std::vector<vk::VertexInputBindingDescription>(UniqueBindings.begin(), UniqueBindings.end());
    }

    NpgsCoreInfo("Shader reflection completed.");
}

void FShader::CreateDescriptors()
{
    for (const auto& [Set, Bindings] : _ReflectionInfo.DescriptorSetBindings)
    {
        vk::DescriptorSetLayoutCreateInfo LayoutCreateInfo({}, Bindings);
        for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
        {
            _DescriptorSetLayoutsMap[Set].emplace_back(LayoutCreateInfo);
        }

        NpgsCoreTrace("Created descriptor set layout for set {} with {} bindings", Set, Bindings.size());
    }

    std::vector<vk::DescriptorPoolSize> PoolSizes;
    for (const auto& [Set, Bindings] : _ReflectionInfo.DescriptorSetBindings)
    {
        for (const auto& Binding : Bindings)
        {
            auto it = std::find_if(PoolSizes.begin(), PoolSizes.end(), [&Binding](const vk::DescriptorPoolSize& Size) -> bool
            {
                return Size.type == Binding.descriptorType;
            });

            if (it != PoolSizes.end())
            {
                it->descriptorCount += Config::Graphics::kMaxFrameInFlight;
            }
            else
            {
                PoolSizes.emplace_back(Binding.descriptorType, Config::Graphics::kMaxFrameInFlight);
            }
        }
    }

    std::uint32_t MaxSets =
        static_cast<std::uint32_t>(_ReflectionInfo.DescriptorSetBindings.size() * Config::Graphics::kMaxFrameInFlight);

    vk::DescriptorPoolCreateInfo PoolCreateInfo({}, MaxSets, PoolSizes);
    _DescriptorPool = std::make_unique<Graphics::FVulkanDescriptorPool>(PoolCreateInfo);

    for (const auto& [Set, Layouts] : _DescriptorSetLayoutsMap)
    {
        std::vector<Graphics::FVulkanDescriptorSet> Sets(Config::Graphics::kMaxFrameInFlight);
        _DescriptorPool->AllocateSets(Layouts, Sets);

        NpgsCoreTrace("Allocated {} descriptor sets for set {}", Sets.size(), Set);
        _DescriptorSetsMap[Set] = std::move(Sets);
    }
}

void FShader::UpdateDescriptorSets(std::uint32_t FrameIndex)
{
    if (!_bDescriptorSetsNeedUpdate)
    {
        return;
    }

    _DescriptorSets[FrameIndex].clear();

    for (const auto& [Set, DescriptorSets] : _DescriptorSetsMap)
    {
        if (FrameIndex < DescriptorSets.size())
        {
            _DescriptorSets[FrameIndex].push_back(*DescriptorSets[FrameIndex]);
        }
    }

    _bDescriptorSetsNeedUpdate = false;
}

_ASSET_END
_RUNTIME_END
_NPGS_END
