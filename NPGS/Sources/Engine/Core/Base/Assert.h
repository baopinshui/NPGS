#pragma once

// Assert
// ------
#ifdef _DEBUG
#define NPGS_ENABLE_ASSERT
#endif // _DEBUG

#ifdef NPGS_ENABLE_ASSERT
#include <iostream>
#include <Windows.h>

#define NpgsAssert(Expr, ...)                                                                                 \
if (!(Expr))                                                                                                  \
{                                                                                                             \
    std::cerr << "Assertion failed: " << #Expr << " in " << __FILE__ << " at line " << __LINE__ << std::endl; \
    std::cerr << "Message: " << __VA_ARGS__ << std::endl;                                                     \
    DebugBreak();                                                                                             \
}

#define NpgsStaticAssert(Expr, ...) static_assert(Expr, __VA_ARGS__)

#else

#define NpgsAssert(Expr, ...)       static_cast<void>(0)
#define NpgsStaticAssert(Expr, ...) static_cast<void>(0)

#endif // NPGS_ENABLE_ASSERT
