#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
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
        TUpdater(const FDeviceLocalBuffer& Buffer, vk::DeviceSize Offset)
            : _Buffer(Buffer), _Offset(Offset)
        {
        }

        TUpdater& operator<<(const DataType& Data)
        {
            Submit(Data);
            return *this;
        }

        void Submit(const DataType& Data) const
        {
        }

    private:
        Graphics::FDeviceLocalBuffer _Buffer;
        vk::DeviceSize               _Offset;
    };

    struct FUniformBufferCreateInfo
    {
        std::string                                         Name;
        std::vector<std::pair<std::string, vk::DeviceSize>> Fields;
        std::vector<std::string>                            ShaderAssetNames;
        std::uint32_t                                       Set{};
        std::uint32_t                                       Binding{};
        vk::DescriptorType                                  Usage{};
    };

private:
    struct FBufferFieldInfo
    {
        std::string    Name;
        vk::DeviceSize Offset{};
        vk::DeviceSize Size{};
        vk::DeviceSize Alignment{};
    };

    struct FUniformBufferInfo
    {
        std::vector<FBufferFieldInfo> Fields;
        FUniformBufferCreateInfo      CreateInfo;
    };

public:
    void CreateBuffer(const FUniformBufferCreateInfo& BufferCreateInfo);

    static FShaderBufferManager* GetInstance();

private:
    explicit FShaderBufferManager()                   = default;
    FShaderBufferManager(const FShaderBufferManager&) = delete;
    FShaderBufferManager(FShaderBufferManager&&)      = delete;
    //~FShaderBufferManager();

    FShaderBufferManager& operator=(const FShaderBufferManager&) = delete;
    FShaderBufferManager& operator=(FShaderBufferManager&&)      = delete;

private:
    std::unordered_map<std::string, FUniformBufferInfo> _UniformBuffers;
};

//class FBlockManagerBase
//{
//public:
//    enum class EBlockLayout
//    {
//        kShared,
//        kStd140,
//        kStd430,
//        kScalar
//    };
//
//public:
//    virtual ~FBlockManagerBase() = default;
//
//    void VerifyBlockLayout(GLuint Program, const std::string& BlockName) const;
//
//protected:
//    struct FBlockInfo
//    {
//        std::unordered_map<std::string, GLint> Offsets{};
//        GLuint Buffer{};
//        GLuint BindingPoint{};
//        GLint ArrayStride{};
//        GLint Size{};
//        EBlockLayout Layout{ EBlockLayout::kShared };
//        bool bIsDynamicArray{ false };
//    };
//
//    struct FMemberInfo
//    {
//        std::string Name;
//        GLenum Type{};
//        GLint Offset{};
//        GLint Size{};
//        GLint ArrayStride{};
//    };
//
//protected:
//    virtual std::vector<FMemberInfo> GetMemberInfos(GLuint Program, const std::string& BlockName) const = 0;
//    virtual const FBlockInfo* GetBlock(const std::string& BlockName) const = 0;
//    virtual const std::string GetBlockTypeName() const = 0;
//};
//
//class FUniformBlockManager : public FBlockManagerBase
//{
//public:
//    template <typename DataType>
//    class TUpdater
//    {
//    public:
//        TUpdater(GLuint Buffer, GLint Offset)
//            : _Buffer(Buffer), _Offset(Offset)
//        {
//        }
//
//        TUpdater& operator<<(const DataType& Data)
//        {
//            Submit(Data);
//            return *this;
//        }
//
//        void Submit(const DataType& Data) const
//        {
//            if constexpr (glm::type<DataType>::is_vec || glm::type<DataType>::is_mat)
//            {
//                glNamedBufferSubData(_Buffer, _Offset, sizeof(DataType), glm::value_ptr(Data));
//            }
//            else
//            {
//                glNamedBufferSubData(_Buffer, _Offset, sizeof(DataType), &Data);
//            }
//        }
//
//    private:
//        GLuint _Buffer;
//        GLint  _Offset;
//    };
//
//public:
//    FBlockInfo* CreateSharedBlock(GLuint Program, const std::string& BlockName, GLuint BindingPoint,
//                                  const std::vector<std::string>& MemberNames, EBlockLayout Layout = EBlockLayout::kStd140);
//
//    void BindShaderToBlock(GLuint Program, const std::string& BlockName) const;
//
//    template <typename DataType>
//    void UpdateEntrieBlock(const std::string& BlockName, const DataType& Data) const;
//
//    template <typename DataType>
//    TUpdater<DataType> GetBlockUpdater(const std::string& BlockName, const std::string& MemberName) const;
//
//    static FUniformBlockManager* GetInstance();
//
//private:
//    explicit FUniformBlockManager()                   = default;
//    FUniformBlockManager(const FUniformBlockManager&) = delete;
//    FUniformBlockManager(FUniformBlockManager&&)      = delete;
//    ~FUniformBlockManager();
//
//    FUniformBlockManager& operator=(const FUniformBlockManager&) = delete;
//    FUniformBlockManager& operator=(FUniformBlockManager&&)      = delete;
//
//    FBlockInfo* CreateBlock(const std::string& BlockName, GLuint BindingPoint,
//                            const std::vector<std::string>& MemberNames, EBlockLayout Layout, GLuint Program);
//
//    GLuint GetBlockIndex(const std::string& BlockName, GLuint Program) const;
//    GLint GetBlockSize(const std::string& BlockName, GLuint BlockIndex, GLuint Program) const;
//    std::vector<GLint> GetBlockOffsets(const std::string& BlockName, const std::vector<std::string>& Names, GLuint Program) const;
//
//    std::vector<FMemberInfo> GetMemberInfos(GLuint Program, const std::string& BlockName) const override;
//    const FBlockInfo* GetBlock(const std::string& BlockName) const override;
//    const std::string GetBlockTypeName() const override;
//
//private:
//    std::unordered_map<std::string, FBlockInfo> _SharedBlocks;
//};
//
//class FStorageBlockManager : public FBlockManagerBase
//{
//public:
//    template <typename DataType>
//    class TUpdater
//    {
//    public:
//        TUpdater(GLuint Buffer, GLint Offset, GLint ArrayStride, bool bIsDynamicArray)
//            : _Buffer(Buffer), _Offset(Offset), _ArrayStride(ArrayStride), _bIsDynamicArray(bIsDynamicArray)
//        {
//        }
//
//        void Resize(std::size_t NewSize)
//        {
//            if (!_bIsDynamicArray)
//            {
//                return;
//            }
//
//            GLint CurrentSize = 0;
//            glGetNamedBufferParameteri64v(_Buffer, GL_BUFFER_SIZE, &CurrentSize);
//
//            GLint NewBufferSize = _Offset + NewSize * _ArrayStride;
//            if (NewBufferSize > CurrentSize)
//            {
//                GLuint NewBuffer = 0;
//                glCreateBuffers(1, &NewBuffer);
//                glNamedBufferStorage(NewBuffer, NewBufferSize, nullptr, GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);
//                glCopyNamedBufferSubData(_Buffer, NewBuffer, 0, 0, CurrentSize);
//                glDeleteBuffers(1, &_Buffer);
//                _Buffer = NewBuffer;
//            }
//        }
//
//        void SubmitRange(const std::vector<DataType>& Data, std::size_t BeginIndex) const
//        {
//            std::size_t Count = Data.size();
//            if (!_bIsDynamicArray && BeginIndex + Count > GetSize())
//            {
//                return;
//            }
//
//            GLint ElementOffset = _Offset + BeginIndex * _ArrayStride;
//            glNamedBufferSubData(_Buffer, ElementOffset, Count * sizeof(DataType), Data.data());
//        }
//
//        std::size_t GetSize() const
//        {
//            if (!_bIsDynamicArray)
//            {
//                return 0;
//            }
//
//            GLint BufferSize = 0;
//            glGetNamedBufferParameteri64v(_Buffer, GL_BUFFER_SIZE, &BufferSize);
//            return static_cast<std::size_t>((BufferSize - _Offset) / _ArrayStride);
//        }
//
//        void SubmitAt(const DataType& Data, GLint ArrayIndex) const
//        {
//            GLint ElementOffset = _Offset;
//            if (_ArrayStride > 0)
//            {
//                ElementOffset += ArrayIndex * _ArrayStride;
//            }
//
//            if constexpr (glm::type<DataType>::is_vec || glm::type<DataType>::is_mat)
//            {
//                glNamedBufferSubData(_Buffer, _Offset, sizeof(DataType), glm::value_ptr(Data));
//            }
//            else
//            {
//                glNamedBufferSubData(_Buffer, _Offset, sizeof(DataType), &Data);
//            }
//        }
//
//        void ReadBackAt(DataType& Data, GLint ArrayIndex) const
//        {
//            GLint ElementOffset = _Offset;
//            if (_ArrayStride > 0)
//            {
//                ElementOffset += ArrayIndex * _ArrayStride;
//            }
//            glGetNamedBufferSubData(_Buffer, ElementOffset, sizeof(DataType), &Data);
//        }
//
//    protected:
//        GLuint _Buffer;
//        GLint  _Offset;
//        GLint  _ArrayStride;
//        bool   _bIsDynamicArray;
//    };
//
//public:
//    FBlockInfo* CreateSharedBlock(GLuint Program, const std::string& BlockName, GLuint BindingPoint,
//                                  const std::vector<std::string>& MemberNames, EBlockLayout Layout = EBlockLayout::kStd430);
//
//    void BindShaderToBlock(GLuint Program, const std::string& BlockName) const;
//
//    template <typename DataType>
//    void UpdateEntrieBlock(const std::string& BlockName, const DataType& Data) const;
//
//    template <typename DataType>
//    TUpdater<DataType> GetBlockUpdater(const std::string& BlockName, const std::string& MemberName) const;
//
//    template <typename DataType>
//    void ResizeArray(const std::string& BlockName, const std::string& ArrayName, std::size_t NewSize);
//
//    template <typename DataType>
//    void UpdateArray(const std::string& BlockName, const std::string& ArrayName,
//                     const std::vector<DataType>& Data, std::size_t BeginIndex = 0);
//
//    static FStorageBlockManager* GetInstance();
//
//private:
//    explicit FStorageBlockManager()                   = default;
//    FStorageBlockManager(const FStorageBlockManager&) = delete;
//    FStorageBlockManager(FStorageBlockManager&&)      = delete;
//    ~FStorageBlockManager();
//
//    FStorageBlockManager& operator=(const FStorageBlockManager&) = delete;
//    FStorageBlockManager& operator=(FStorageBlockManager&&)      = delete;
//
//    FBlockInfo* CreateBlock(const std::string& BlockName, GLuint BindingPoint,
//                            const std::vector<std::string>& MemberNames, EBlockLayout Layout, GLuint Program);
//
//    GLuint GetBlockIndex(const std::string& BlockName, GLuint Program) const;
//    GLint GetBlockSize(const std::string& BlockName, GLuint BlockIndex, GLuint Program) const;
//    std::vector<GLint> GetBlockOffsets(const std::string& BlockName, const std::vector<std::string>& Names, GLuint Program) const;
//
//    std::vector<FMemberInfo> GetMemberInfos(GLuint Program, const std::string& BlockName) const override;
//    const FBlockInfo* GetBlock(const std::string& BlockName) const override;
//    const std::string GetBlockTypeName() const override;
//
//private:
//    std::unordered_map<std::string, FBlockInfo> _StorageBlocks;
//};

_GRAPHICS_END
_RUNTIME_END
_NPGS_END

#include "ShaderBufferManager.inl"
