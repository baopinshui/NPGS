#include "ShaderBufferManager.h"

#include <array>
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

//template <typename DataType>
//inline void FUniformBlockManager::UpdateEntrieBlock(const std::string& BlockName, const DataType& Data) const
//{
//    auto it = _SharedBlocks.find(BlockName);
//    if (it == _SharedBlocks.end())
//    {
//        return;
//    }
//
//    const FBlockInfo& Info = it->second;
//    if (sizeof(DataType) != static_cast<std::size_t>(Info.Size))
//    {
//        NpgsCoreWarn("Size mismatch for block \"{}\"", BlockName);
//        return;
//    }
//
//    glNamedBufferSubData(Info.Buffer, 0, Info.Size, &Data);
//}
//
//template <typename DataType>
//NPGS_INLINE FUniformBlockManager::TUpdater<DataType>
//FUniformBlockManager::GetBlockUpdater(const std::string& BlockName, const std::string& MemberName) const
//{
//    auto& BlockInfo = _SharedBlocks.at(BlockName);
//    return TUpdater<DataType>(BlockInfo.Buffer, BlockInfo.Offsets.at(MemberName));
//}
//
//NPGS_INLINE GLuint FUniformBlockManager::GetBlockIndex(const std::string& BlockName, GLuint Program) const
//{
//    return glGetUniformBlockIndex(Program, BlockName.c_str());
//}
//
//NPGS_INLINE GLint FUniformBlockManager::GetBlockSize(const std::string& BlockName, GLuint BlockIndex, GLuint Program) const
//{
//    GLint BlockSize = 0;
//    glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &BlockSize);
//    return BlockSize;
//}
//
//NPGS_INLINE const FUniformBlockManager::FBlockInfo* FUniformBlockManager::GetBlock(const std::string& BlockName) const
//{
//    return &_SharedBlocks.at(BlockName);
//}
//
//NPGS_INLINE const std::string FUniformBlockManager::GetBlockTypeName() const
//{
//    return std::string("Uniform");
//}
//
//template <typename DataType>
//inline void FStorageBlockManager::UpdateEntrieBlock(const std::string& BlockName, const DataType& Data) const
//{
//    auto it = _StorageBlocks.find(BlockName);
//    if (it == _StorageBlocks.end())
//    {
//        return;
//    }
//
//    const FBlockInfo& Info = it->second;
//    if (sizeof(DataType) > static_cast<std::size_t>(Info.Size)) // 注意这里用 > 而不是 !=
//    {
//        NpgsCoreWarn("Data size ({}) exceeds block size ({}) for block \"{}\"", sizeof(DataType), Info.Size, BlockName);
//        return;
//    }
//
//    glNamedBufferSubData(Info.Buffer, 0, sizeof(DataType), &Data);
//}
//
//template <typename DataType>
//inline FStorageBlockManager::TUpdater<DataType>
//FStorageBlockManager::GetBlockUpdater(const std::string& BlockName, const std::string& MemberName) const
//{
//    auto& BlockInfo = _StorageBlocks.at(BlockName);
//    return TUpdater<DataType>(BlockInfo.Buffer, BlockInfo.Offsets.at(MemberName),
//                              BlockInfo.ArrayStride, BlockInfo.bIsDynamicArray);
//}
//
//template <typename DataType>
//inline void FStorageBlockManager::ResizeArray(const std::string& BlockName, const std::string& ArrayName, std::size_t NewSize)
//{
//    auto& BlockInfo = _StorageBlocks.at(BlockName);
//    auto Updater = GetBlockUpdater<DataType>(BlockName, ArrayName);
//    Updater.Resize(NewSize);
//}
//
//template <typename DataType>
//inline void FStorageBlockManager::UpdateArray(const std::string& BlockName, const std::string& ArrayName,
//                                              const std::vector<DataType>& Data, std::size_t BeginIndex)
//{
//    auto Updater = GetBlockUpdater<DataType>(BlockName, ArrayName);
//    Updater.SubmitRange(Data.data(), BeginIndex, Data.size());
//}
//
//NPGS_INLINE GLuint FStorageBlockManager::GetBlockIndex(const std::string& BlockName, GLuint Program) const
//{
//    return glGetProgramResourceIndex(Program, GL_SHADER_STORAGE_BLOCK, BlockName.c_str());
//}
//
//NPGS_INLINE GLint FStorageBlockManager::GetBlockSize(const std::string& BlockName, GLuint BlockIndex, GLuint Program) const
//{
//    GLint BlockSize = 0;
//    static const std::array<GLenum, 1> Properties{ GL_BUFFER_DATA_SIZE };
//    glGetProgramResourceiv(Program, GL_SHADER_STORAGE_BLOCK, BlockIndex, 1, Properties.data(), 1, nullptr, &BlockSize);
//    return BlockSize;
//}
//
//NPGS_INLINE const FStorageBlockManager::FBlockInfo* FStorageBlockManager::GetBlock(const std::string& BlockName) const
//{
//    return &_StorageBlocks.at(BlockName);
//}
//
//NPGS_INLINE const std::string FStorageBlockManager::GetBlockTypeName() const
//{
//    return std::string("Storage");
//}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
