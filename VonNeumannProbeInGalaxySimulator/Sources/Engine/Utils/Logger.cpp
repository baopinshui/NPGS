#include "Logger.h"

#include <spdlog/sinks/stdout_color_sinks.h>

_NPGS_BEGIN
_UTIL_BEGIN

void FLogger::Initialize()
{
    // 修改日志格式模式
    // %^ %$ 是颜色开始和结束的标记
    // %l 是日志级别 (TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL)
    // %T 是时间
    // %n 是日志器名称
    // %v 是实际的日志消息
    spdlog::set_pattern("[%T][%^%l%$] %n: %v");

    _kCoreLogger   = spdlog::stdout_color_mt("NPGS");
    _kClientLogger = spdlog::stdout_color_mt("App");

    auto ConsoleSink = dynamic_cast<spdlog::sinks::stdout_color_sink_mt*>(_kCoreLogger->sinks()[0].get());
    if (ConsoleSink)
    {
        ConsoleSink->set_color(spdlog::level::trace, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    }

    ConsoleSink = dynamic_cast<spdlog::sinks::stdout_color_sink_mt*>(_kClientLogger->sinks()[0].get());
    if (ConsoleSink)
    {
        ConsoleSink->set_color(spdlog::level::trace, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    }

    _kCoreLogger->set_level(spdlog::level::trace);
    _kClientLogger->set_level(spdlog::level::trace);
}


std::shared_ptr<spdlog::logger> FLogger::_kCoreLogger   = nullptr;
std::shared_ptr<spdlog::logger> FLogger::_kClientLogger = nullptr;

_UTIL_END
_NPGS_END
