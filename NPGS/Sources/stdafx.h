#pragma once

#include "xstdafx.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <fast-cpp-csv-parser/csv.h>

#define GLFW_INCLUDE_VULKAN
#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#include <GLFW/glfw3.h>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <spirv_cross/spirv_reflect.hpp>
#include <stb_image.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_format_traits.hpp>

#include <Windows.h>

#ifndef _RELEASE
#define NPGS_ENABLE_CONSOLE_LOGGER
#else
#define GLM_FORCE_INLINE
#define NPGS_ENABLE_FILE_LOGGER
#endif // _RELEASE
#include "Engine/Utils/Logger.h"
