#pragma once

#include <memory>
#include <spdlog/spdlog.h>
#include "Engine/Core/Base/Base.h"

_NPGS_BEGIN
_UTIL_BEGIN

class FLogger
{
public:
    static void Initialize();
    static std::shared_ptr<spdlog::logger>& GetCoreLogger();
    static std::shared_ptr<spdlog::logger>& GetClientLogger();

private:
    FLogger()  = default;
    ~FLogger() = default;

    static std::shared_ptr<spdlog::logger> _kCoreLogger;
    static std::shared_ptr<spdlog::logger> _kClientLogger;
};

_UTIL_END
_NPGS_END

#include "Logger.inl"

#if defined(NPGS_ENABLE_CONSOLE_LOGGER)
// Core logger
// -----------
#define NpgsCoreCritical(...) ::Npgs::Util::FLogger::GetCoreLogger()->critical(__VA_ARGS__)
#define NpgsCoreError(...)    ::Npgs::Util::FLogger::GetCoreLogger()->error(__VA_ARGS__)
#define NpgsCoreInfo(...)     ::Npgs::Util::FLogger::GetCoreLogger()->info(__VA_ARGS__)
#define NpgsCoreTrace(...)    ::Npgs::Util::FLogger::GetCoreLogger()->trace(__VA_ARGS__)
#define NpgsCoreWarn(...)     ::Npgs::Util::FLogger::GetCoreLogger()->warn(__VA_ARGS__)

// Client logger
// -------------
#define NpgsCritical(...)     ::Npgs::Util::FLogger::GetClientLogger()->critical(__VA_ARGS__)
#define NpgsError(...)        ::Npgs::Util::FLogger::GetClientLogger()->error(__VA_ARGS__)
#define NpgsInfo(...)         ::Npgs::Util::FLogger::GetClientLogger()->info(__VA_ARGS__)
#define NpgsTrace(...)        ::Npgs::Util::FLogger::GetClientLogger()->trace(__VA_ARGS__)
#define NpgsWarn(...)         ::Npgs::Util::FLogger::GetClientLogger()->warn(__VA_ARGS__)

#elif defined(NPGS_ENABLE_FILE_LOGGER)

#define NpgsCoreCritical(...) ::Npgs::Util::FLogger::GetCoreLogger()->critical(__VA_ARGS__)
#define NpgsCoreError(...)    ::Npgs::Util::FLogger::GetCoreLogger()->error(__VA_ARGS__)
#define NpgsCoreInfo(...)     ::Npgs::Util::FLogger::GetCoreLogger()->info(__VA_ARGS__)
#define NpgsCoreTrace(...)    static_cast<void>(0)
#define NpgsCoreWarn(...)     ::Npgs::Util::FLogger::GetCoreLogger()->warn(__VA_ARGS__)

// Client logger
// -------------
#define NpgsCritical(...)     ::Npgs::Util::FLogger::GetClientLogger()->critical(__VA_ARGS__)
#define NpgsError(...)        ::Npgs::Util::FLogger::GetClientLogger()->error(__VA_ARGS__)
#define NpgsInfo(...)         ::Npgs::Util::FLogger::GetClientLogger()->info(__VA_ARGS__)
#define NpgsTrace(...)        static_cast<void>(0)
#define NpgsWarn(...)         ::Npgs::Util::FLogger::GetClientLogger()->warn(__VA_ARGS__)

#else

#define NpgsCoreCritical(...) static_cast<void>(0)
#define NpgsCoreError(...)    static_cast<void>(0)
#define NpgsCoreInfo(...)     static_cast<void>(0)
#define NpgsCoreTrace(...)    static_cast<void>(0)
#define NpgsCoreWarn(...)     static_cast<void>(0)

#define NpgsCritical(...)     static_cast<void>(0)
#define NpgsError(...)        static_cast<void>(0)
#define NpgsInfo(...)         static_cast<void>(0)
#define NpgsTrace(...)        static_cast<void>(0)
#define NpgsWarn(...)         static_cast<void>(0)

#endif // NPGS_ENABLE_CONSOLE_LOGGER || NPGS_ENABLE_FILE_LOGGER
