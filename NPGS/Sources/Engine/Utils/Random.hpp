#pragma once

#include <random>
#include <type_traits>
#include "Engine/Core/Base/Base.h"

_NPGS_BEGIN
_UTIL_BEGIN

template <typename BaseType = float, typename RandomEngine = std::mt19937>
requires std::is_class_v<RandomEngine>
class TDistribution
{
public:
    virtual ~TDistribution()                          = default;
    virtual BaseType operator()(RandomEngine& Engine) = 0;
    virtual BaseType Generate(RandomEngine& Engine)   = 0;
};

template <typename BaseType = int, typename RandomEngine = std::mt19937>
requires std::is_class_v<RandomEngine>
class TUniformIntDistribution : public TDistribution<BaseType>
{
public:
    TUniformIntDistribution() = default;
    TUniformIntDistribution(BaseType Min, BaseType Max) : _Distribution(Min, Max) {}

    BaseType operator()(RandomEngine& Engine) override
    {
        return _Distribution(Engine);
    }

    BaseType Generate(RandomEngine& Engine) override
    {
        return operator()(Engine);
    }

private:
    std::uniform_int_distribution<BaseType> _Distribution;
};

template <typename BaseType = float, typename RandomEngine = std::mt19937>
requires std::is_class_v<RandomEngine>
class TUniformRealDistribution : public TDistribution<BaseType, RandomEngine>
{
public:
    TUniformRealDistribution() = default;
    TUniformRealDistribution(BaseType Min, BaseType Max) : _Distribution(Min, Max) {}

    BaseType operator()(RandomEngine& Engine) override
    {
        return _Distribution(Engine);
    }

    BaseType Generate(RandomEngine& Engine) override
    {
        return operator()(Engine);
    }

private:
    std::uniform_real_distribution<BaseType> _Distribution;
};

template <typename BaseType = float, typename RandomEngine = std::mt19937>
requires std::is_class_v<RandomEngine>
class TNormalDistribution : public TDistribution<BaseType, RandomEngine>
{
public:
    TNormalDistribution() = default;
    TNormalDistribution(BaseType Mean, BaseType Sigma) : _Distribution(Mean, Sigma) {}

    BaseType operator()(RandomEngine& Engine) override
    {
        return _Distribution(Engine);
    }

    BaseType Generate(RandomEngine& Engine) override
    {
        return operator()(Engine);
    }

private:
    std::normal_distribution<BaseType> _Distribution;
};

template <typename BaseType = float, typename RandomEngine = std::mt19937>
requires std::is_class_v<RandomEngine>
class TLogNormalDistribution : public TDistribution<BaseType, RandomEngine>
{
public:
    TLogNormalDistribution() = default;
    TLogNormalDistribution(BaseType Mean, BaseType Sigma) : _Distribution(Mean, Sigma) {}

    BaseType operator()(RandomEngine& Engine) override
    {
        return _Distribution(Engine);
    }

    BaseType Generate(RandomEngine& Engine) override
    {
        return operator()(Engine);
    }

private:
    std::lognormal_distribution<BaseType> _Distribution;
};

template <typename RandomEngine = std::mt19937>
requires std::is_class_v<RandomEngine>
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
