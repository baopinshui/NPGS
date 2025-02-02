#pragma once

#include <random>
#include "Engine/Core/Base/Base.h"

_NPGS_BEGIN
_UTIL_BEGIN

template <typename Ty = float, typename RandomEngine = std::mt19937>
class TDistribution
{
public:
    virtual ~TDistribution()                    = default;
    virtual Ty operator()(RandomEngine& Engine) = 0;
    virtual Ty Generate(RandomEngine& Engine)   = 0;
};

template <typename Ty = int, typename RandomEngine = std::mt19937>
class TUniformIntDistribution : public TDistribution<Ty>
{
public:
    TUniformIntDistribution() = default;
    TUniformIntDistribution(Ty Min, Ty Max) : _Distribution(Min, Max) {}

    Ty operator()(RandomEngine& Engine) override
    {
        return _Distribution(Engine);
    }

    Ty Generate(RandomEngine& Engine) override
    {
        return operator()(Engine);
    }

private:
    std::uniform_int_distribution<Ty> _Distribution;
};

template <typename Ty = float, typename RandomEngine = std::mt19937>
class TUniformRealDistribution : public TDistribution<Ty, RandomEngine>
{
public:
    TUniformRealDistribution() = default;
    TUniformRealDistribution(Ty Min, Ty Max) : _Distribution(Min, Max) {}

    Ty operator()(RandomEngine& Engine) override
    {
        return _Distribution(Engine);
    }

    Ty Generate(RandomEngine& Engine) override
    {
        return operator()(Engine);
    }

private:
    std::uniform_real_distribution<Ty> _Distribution;
};

template <typename Ty = float, typename RandomEngine = std::mt19937>
class TNormalDistribution : public TDistribution<Ty, RandomEngine>
{
public:
    TNormalDistribution() = default;
    TNormalDistribution(Ty Mean, Ty Sigma) : _Distribution(Mean, Sigma) {}

    Ty operator()(RandomEngine& Engine) override
    {
        return _Distribution(Engine);
    }

    Ty Generate(RandomEngine& Engine) override
    {
        return operator()(Engine);
    }

private:
    std::normal_distribution<Ty> _Distribution;
};

template <typename Ty = float, typename RandomEngine = std::mt19937>
class TLogNormalDistribution : public TDistribution<Ty, RandomEngine>
{
public:
    TLogNormalDistribution() = default;
    TLogNormalDistribution(Ty Mean, Ty Sigma) : _Distribution(Mean, Sigma) {}

    Ty operator()(RandomEngine& Engine) override
    {
        return _Distribution(Engine);
    }

    Ty Generate(RandomEngine& Engine) override
    {
        return operator()(Engine);
    }

private:
    std::lognormal_distribution<Ty> _Distribution;
};

template <typename RandomEngine = std::mt19937>
class TBernoulliDistribution : public TDistribution<double, RandomEngine>
{
public:
    TBernoulliDistribution() = default;
    TBernoulliDistribution(double Probability) : _Distribution(Probability) {}

    double operator()(RandomEngine& Engine) override
    {
        return _Distribution(Engine);
    }

    double Generate(RandomEngine& Engine) override
    {
        return operator()(Engine);
    }

private:
    std::bernoulli_distribution _Distribution;
};

_UTIL_END
_NPGS_END
