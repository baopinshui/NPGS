#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

class FBufferManagerBase
{
public:
    virtual ~FBufferManagerBase() = default;
};

class FUniformBufferManager : public FBufferManagerBase
{
public:
    template <typename DataType>
    class TUpdater
    {
    };

public:
    static FUniformBufferManager* GetInstance();

private:
    explicit FUniformBufferManager()                    = default;
    FUniformBufferManager(const FUniformBufferManager&) = delete;
    FUniformBufferManager(FUniformBufferManager&&)      = delete;
    ~FUniformBufferManager();

    FUniformBufferManager& operator=(const FUniformBufferManager&) = delete;
    FUniformBufferManager& operator=(FUniformBufferManager&&)      = delete;
};

class FStorageBufferManager : public FBufferManagerBase
{
public:
    template <typename DataType>
    class TUpdater
    {
    };

public:

    static FStorageBufferManager* GetInstance();

private:
    explicit FStorageBufferManager()                    = default;
    FStorageBufferManager(const FStorageBufferManager&) = delete;
    FStorageBufferManager(FStorageBufferManager&&)      = delete;
    ~FStorageBufferManager();

    FStorageBufferManager& operator=(const FStorageBufferManager&) = delete;
    FStorageBufferManager& operator=(FStorageBufferManager&&)      = delete;
};

_GRAPHICS_END
_RUNTIME_END
_NPGS_END

#include "ShaderDescriptorManager.inl"
