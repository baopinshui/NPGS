#pragma once

#include "xstdafx.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
// #include <boost/multiprecision/cpp_bin_float.hpp>
// #include <boost/multiprecision/cpp_int.hpp>
#include <fast-cpp-csv-parser/csv.h>

#define GLFW_INCLUDE_VULKAN
#define VK_USE_PLATFORM_WIN32_KHR
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

#include <magic_enum/magic_enum_all.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_format_traits.hpp>

#define NOMINMAX
#include <Windows.h>

#ifndef _RELEASE
#define NPGS_ENABLE_CONSOLE_LOGGER
#endif // _RELEASE
#include "Engine/Utils/Logger.h"
