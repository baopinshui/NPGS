#pragma once

#include "Engine/Core/Base/Assert.h"
#include "Engine/Core/Base/Base.h"

#include "Engine/Core/Math/NumericConstants.h"

#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Core/Runtime/AssetLoaders/CommaSeparatedValues.hpp"
#include "Engine/Core/Runtime/AssetLoaders/GetAssetFullPath.h"
#include "Engine/Core/Runtime/AssetLoaders/Texture.h"

#include "Engine/Core/Runtime/Graphics/Vulkan/Buffers.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Core.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

#include "Engine/Core/Runtime/Threads/ThreadPool.h"

#include "Engine/Core/System/Generators/CivilizationGenerator.h"
#include "Engine/Core/System/Generators/OrbitalGenerator.h"
#include "Engine/Core/System/Generators/StellarGenerator.h"

#include "Engine/Core/System/Spatial/Camera.h"
#include "Engine/Core/System/Spatial/Octree.hpp"

#include "Engine/Core/Types/Entries/Astro/CelestialObject.h"
#include "Engine/Core/Types/Entries/Astro/Planet.h"
#include "Engine/Core/Types/Entries/Astro/Star.h"
#include "Engine/Core/Types/Entries/Astro/StellarSystem.h"
#include "Engine/Core/Types/Entries/NpgsObject.h"

#include "Engine/Core/Types/Properties/Intelli/Artifact.h"
#include "Engine/Core/Types/Properties/Intelli/Civilization.h"
#include "Engine/Core/Types/Properties/StellarClass.h"

#include "Engine/Utils/Logger.h"
#include "Engine/Utils/Random.hpp"
#include "Engine/Utils/Utils.h"
