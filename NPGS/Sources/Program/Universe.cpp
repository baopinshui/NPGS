#include "Universe.h"

#include <cstdlib>
#include <algorithm>
#include <array>
#include <format>
#include <iomanip>
#include <iterator>
#include <limits>
#include <print>
#include <sstream>
#include <string>
#include <utility>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Math/NumericConstants.h"
#include "Engine/Core/System/Generators/OrbitalGenerator.h"
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN

namespace SysGen = Npgs::System::Generator;

FUniverse::FUniverse(std::uint32_t Seed, std::size_t StarCount, std::size_t ExtraGiantCount, std::size_t ExtraMassiveStarCount,
                     std::size_t ExtraNeutronStarCount, std::size_t ExtraBlackHoleCount, std::size_t ExtraMergeStarCount,
                     float UniverseAge)
    :
    _RandomEngine(Seed),
    _SeedGenerator(0ull, std::numeric_limits<std::uint32_t>::max()),
    _CommonGenerator(0.0f, 1.0f),
    _ThreadPool(Runtime::Thread::FThreadPool::GetInstance()),

    _StarCount(StarCount),
    _ExtraGiantCount(ExtraGiantCount),
    _ExtraMassiveStarCount(ExtraMassiveStarCount),
    _ExtraNeutronStarCount(ExtraNeutronStarCount),
    _ExtraBlackHoleCount(ExtraBlackHoleCount),
    _ExtraMergeStarCount(ExtraMergeStarCount),
    _UniverseAge(UniverseAge)
{
    std::vector<std::uint32_t> Seeds(32);
    for (int i = 0; i != 32; ++i)
    {
        Seeds[i] = _SeedGenerator(_RandomEngine);
    }

    std::shuffle(Seeds.begin(), Seeds.end(), _RandomEngine);
    std::seed_seq SeedSequence(Seeds.begin(), Seeds.end());
    _RandomEngine.seed(SeedSequence);
}

FUniverse::~FUniverse()
{
}

void FUniverse::FillUniverse()
{
    int MaxThread = _ThreadPool->GetMaxThreadCount();

    GenerateStars(MaxThread);
    FillStellarSystem(MaxThread);

    _ThreadPool->Terminate();
}

void FUniverse::ReplaceStar(std::size_t DistanceRank, const Astro::AStar& StarData)
{
    for (auto& System : _StellarSystems)
    {
        if (DistanceRank == System.GetBaryDistanceRank())
        {
            auto& Stars = System.StarsData();
            if (Stars.size() > 1)
            {
                return; // TODO: 处理双星
            }

            Stars.clear();
            Stars.push_back(std::make_unique<Astro::AStar>(StarData));
        }
    }
}

void FUniverse::CountStars()
{
    constexpr int kTypeOIndex = 0;
    constexpr int kTypeBIndex = 1;
    constexpr int kTypeAIndex = 2;
    constexpr int kTypeFIndex = 3;
    constexpr int kTypeGIndex = 4;
    constexpr int kTypeKIndex = 5;
    constexpr int kTypeMIndex = 6;

    std::array<std::size_t, 7> MainSequence{};
    std::array<std::size_t, 7> Subgiants{};
    std::array<std::size_t, 7> Giants{};
    std::array<std::size_t, 7> BrightGiants{};
    std::array<std::size_t, 7> Supergiants{};
    std::array<std::size_t, 7> Hypergiants{};

    std::size_t WolfRayet    = 0;
    std::size_t WhiteDwarfs  = 0;
    std::size_t NeutronStars = 0;
    std::size_t BlackHoles   = 0;
    std::size_t TotalStars   = 1;
    std::size_t TotalBinarys = 0;
    std::size_t TotalSingles = 0;

    struct MostLuminous
    {
        double LuminositySol{};
        const Astro::AStar* Star = nullptr;
    };

    struct MostMassive
    {
        double MassSol{};
        const Astro::AStar* Star = nullptr;
    };

    struct Largest
    {
        float RadiusSol{};
        const Astro::AStar* Star = nullptr;
    };

    struct Hottest
    {
        float Teff{};
        const Astro::AStar* Star = nullptr;
    };

    struct Oldest
    {
        double Age{};
        const Astro::AStar* Star = nullptr;
    };

    struct MostOblateness
    {
        float Oblateness{};
        const Astro::AStar* Star = nullptr;
    };

    auto FormatTitle = []() -> std::string
    {
        return std::format("{:>6} {:>6} {:>8} {:>8} {:7} {:>5} {:>13} {:>8} {:>8} {:>11} {:>8} {:>9} {:>5} {:>15} {:>9} {:>8}",
                           "InMass", "Mass", "Radius", "Age", "Class", "FeH", "Lum", "Teff", "CoreTemp", "CoreDensity", "Mdot", "WindSpeed", "Phase", "Magnetic", "Lifetime", "Oblateness");
    };

    auto FormatInfo = [](const Astro::AStar* Star) -> std::string
    {
        if (Star == nullptr)
        {
            return "No star generated.";
        }

        return std::format("{:6.2f} {:6.2f} {:8.2f} {:8.2E} {:7} {:5.2f} {:13.4f} {:8.1f} {:8.2E} {:11.2E} {:8.2E} {:9} {:5} {:15.5f} {:9.2E} {:8.2f}",
                           Star->GetInitialMass() / kSolarMass,
                           Star->GetMass() / kSolarMass,
                           Star->GetRadius() / kSolarRadius,
                           Star->GetAge(),
                           Star->GetStellarClass().ToString(),
                           Star->GetFeH(),
                           Star->GetLuminosity() / kSolarLuminosity,
                           // kSolarAbsoluteMagnitude - 2.5 * std::log10(Star->GetLuminosity() / kSolarLuminosity),
                           Star->GetTeff(),
                           Star->GetCoreTemp(),
                           Star->GetCoreDensity(),
                           Star->GetStellarWindMassLossRate() * kYearToSecond / kSolarMass,
                           static_cast<int>(std::round(Star->GetStellarWindSpeed())),
                           static_cast<int>(Star->GetEvolutionPhase()),
                           Star->GetSurfaceZ(),
                           // Star->GetSurfaceEnergeticNuclide(),
                           // Star->GetSurfaceVolatiles(),
                           // Star->GetMagneticField(),
                           Star->GetLifetime(),
                           Star->GetOblateness());
    };

    auto CountClass = [](const Astro::FStellarClass::FSpectralType& SpectralType, std::array<std::size_t, 7>& Type)
    {
        switch (SpectralType.HSpectralClass)
        {
        case Astro::FStellarClass::ESpectralClass::kSpectral_O:
            ++Type[kTypeOIndex];
            break;
        case Astro::FStellarClass::ESpectralClass::kSpectral_B:
            ++Type[kTypeBIndex];
            break;
        case Astro::FStellarClass::ESpectralClass::kSpectral_A:
            ++Type[kTypeAIndex];
            break;
        case Astro::FStellarClass::ESpectralClass::kSpectral_F:
            ++Type[kTypeFIndex];
            break;
        case Astro::FStellarClass::ESpectralClass::kSpectral_G:
            ++Type[kTypeGIndex];
            break;
        case Astro::FStellarClass::ESpectralClass::kSpectral_K:
            ++Type[kTypeKIndex];
            break;
        case Astro::FStellarClass::ESpectralClass::kSpectral_M:
            ++Type[kTypeMIndex];
            break;
        }
    };

    auto CountMostLuminous = [](const std::unique_ptr<Astro::AStar>& Star, MostLuminous& MostLuminousStar)
    {
        double LuminositySol = 0.0;
        LuminositySol = Star->GetLuminosity() / kSolarLuminosity;
        if (MostLuminousStar.LuminositySol < LuminositySol)
        {
            MostLuminousStar.LuminositySol = LuminositySol;
            MostLuminousStar.Star = Star.get();
        }
    };

    auto CountMostMassive = [](const std::unique_ptr<Astro::AStar>& Star, MostMassive& MostMassiveStar)
    {
        double MassSol = 0.0;
        MassSol = Star->GetMass() / kSolarMass;
        if (MostMassiveStar.MassSol < MassSol)
        {
            MostMassiveStar.MassSol = MassSol;
            MostMassiveStar.Star = Star.get();
        }
    };

    auto CountLargest = [](const std::unique_ptr<Astro::AStar>& Star, Largest& LargestStar)
    {
        float RadiusSol = 0.0f;
        RadiusSol = Star->GetRadius() / kSolarRadius;
        if (LargestStar.RadiusSol < RadiusSol)
        {
            LargestStar.RadiusSol = RadiusSol;
            LargestStar.Star = Star.get();
        }
    };

    auto CountHottest = [](const std::unique_ptr<Astro::AStar>& Star, Hottest& HottestStar)
    {
        float Teff = 0.0f;
        Teff = Star->GetTeff();
        if (HottestStar.Teff < Teff)
        {
            HottestStar.Teff = Teff;
            HottestStar.Star = Star.get();
        }
    };

    auto CountOldest = [](const std::unique_ptr<Astro::AStar>& Star, Oldest& OldestStar)
    {
        double Age = 0.0;
        Age = Star->GetAge();
        if (OldestStar.Age < Age)
        {
            OldestStar.Age = Age;
            OldestStar.Star = Star.get();
        }
    };

    auto CountMostOblateness = [](const std::unique_ptr<Astro::AStar>& Star, MostOblateness& MostOblatenessStar)
    {
        float Oblateness = 0.0f;
        Oblateness = Star->GetOblateness();
        if (MostOblatenessStar.Oblateness < Oblateness)
        {
            MostOblatenessStar.Oblateness = Oblateness;
            MostOblatenessStar.Star = Star.get();
        }
    };

    MostLuminous MostLuminousMainSequence;
    MostLuminous MostLuminousSubgiant;
    MostLuminous MostLuminousGiant;
    MostLuminous MostLuminousBrightGiant;
    MostLuminous MostLuminousSupergiant;
    MostLuminous MostLuminousHypergiant;
    MostLuminous MostLuminousWolfRayet;

    MostMassive MostMassiveMainSequence;
    MostMassive MostMassiveSubgiant;
    MostMassive MostMassiveGiant;
    MostMassive MostMassiveBrightGiant;
    MostMassive MostMassiveSupergiant;
    MostMassive MostMassiveHypergiant;
    MostMassive MostMassiveWolfRayet;

    Largest LargestMainSequence;
    Largest LargestSubgiant;
    Largest LargestGiant;
    Largest LargestBrightGiant;
    Largest LargestSupergiant;
    Largest LargestHypergiant;
    Largest LargestWolfRayet;

    Hottest HottestMainSequence;
    Hottest HottestSubgiant;
    Hottest HottestGiant;
    Hottest HottestBrightGiant;
    Hottest HottestSupergiant;
    Hottest HottestHypergiant;
    Hottest HottestWolfRayet;

    Oldest OldestMainSequence;
    Oldest OldestSubgiant;
    Oldest OldestGiant;
    Oldest OldestBrightGiant;
    Oldest OldestSupergiant;
    Oldest OldestHypergiant;
    Oldest OldestWolfRayet;

    MostOblateness MostOblatenessMainSequence;
    MostOblateness MostOblatenessSubgiant;
    MostOblateness MostOblatenessGiant;
    MostOblateness MostOblatenessBrightGiant;
    MostOblateness MostOblatenessSupergiant;
    MostOblateness MostOblatenessHypergiant;
    MostOblateness MostOblatenessWolfRayet;

    std::println("Star statistics results:");
    std::println("{}", FormatTitle());
    std::println("");

    for (auto& System : _StellarSystems)
    {
        for (auto& Star : System.StarsData())
        {
            ++TotalStars;

            if (Star->GetIsSingleStar())
            {
                ++TotalSingles;
            }
            else
            {
                ++TotalBinarys;
            }

            const auto& Class = Star->GetStellarClass();
            Astro::FStellarClass::EStellarType StellarType = Class.GetStellarType();
            if (StellarType != Astro::FStellarClass::EStellarType::kNormalStar)
            {
                switch (StellarType)
                {
                case Astro::FStellarClass::EStellarType::kBlackHole:
                    ++BlackHoles;
                    break;
                case Astro::FStellarClass::EStellarType::kNeutronStar:
                    ++NeutronStars;
                    break;
                case Astro::FStellarClass::EStellarType::kWhiteDwarf:
                    ++WhiteDwarfs;
                    break;
                default:
                    break;
                }

                continue;
            }

            Astro::FStellarClass::FSpectralType SpectralType = Class.Data();

            if (SpectralType.LuminosityClass == Astro::FStellarClass::ELuminosityClass::kLuminosity_Unknown)
            {
                if (SpectralType.HSpectralClass == Astro::FStellarClass::ESpectralClass::kSpectral_WC ||
                    SpectralType.HSpectralClass == Astro::FStellarClass::ESpectralClass::kSpectral_WN ||
                    SpectralType.HSpectralClass == Astro::FStellarClass::ESpectralClass::kSpectral_WO)
                {
                    ++WolfRayet;
                    CountMostLuminous(Star, MostLuminousWolfRayet);
                    CountMostMassive(Star, MostMassiveWolfRayet);
                    CountLargest(Star, LargestWolfRayet);
                    CountHottest(Star, HottestWolfRayet);
                    CountOldest(Star, OldestWolfRayet);
                    CountMostOblateness(Star, MostOblatenessWolfRayet);
                    continue;
                }
            }

            if (SpectralType.LuminosityClass == Astro::FStellarClass::ELuminosityClass::kLuminosity_0 ||
                SpectralType.LuminosityClass == Astro::FStellarClass::ELuminosityClass::kLuminosity_IaPlus)
            {
                CountClass(SpectralType, Hypergiants);
                CountMostLuminous(Star, MostLuminousHypergiant);
                CountMostMassive(Star, MostMassiveHypergiant);
                CountLargest(Star, LargestHypergiant);
                CountHottest(Star, HottestHypergiant);
                CountOldest(Star, OldestHypergiant);
                CountMostOblateness(Star, MostOblatenessHypergiant);
                continue;
            }

            if (SpectralType.LuminosityClass == Astro::FStellarClass::ELuminosityClass::kLuminosity_Ia ||
                SpectralType.LuminosityClass == Astro::FStellarClass::ELuminosityClass::kLuminosity_Iab ||
                SpectralType.LuminosityClass == Astro::FStellarClass::ELuminosityClass::kLuminosity_Ib)
            {
                CountClass(SpectralType, Supergiants);
                CountMostLuminous(Star, MostLuminousSupergiant);
                CountMostMassive(Star, MostMassiveSupergiant);
                CountLargest(Star, LargestSupergiant);
                CountHottest(Star, HottestSupergiant);
                CountOldest(Star, OldestSupergiant);
                CountMostOblateness(Star, MostOblatenessSupergiant);
                continue;
            }

            if (SpectralType.LuminosityClass == Astro::FStellarClass::ELuminosityClass::kLuminosity_II)
            {
                CountClass(SpectralType, BrightGiants);
                CountMostLuminous(Star, MostLuminousBrightGiant);
                CountMostMassive(Star, MostMassiveBrightGiant);
                CountLargest(Star, LargestBrightGiant);
                CountHottest(Star, HottestBrightGiant);
                CountOldest(Star, OldestBrightGiant);
                CountMostOblateness(Star, MostOblatenessBrightGiant);
                continue;
            }

            if (SpectralType.LuminosityClass == Astro::FStellarClass::ELuminosityClass::kLuminosity_III)
            {
                CountClass(SpectralType, Giants);
                CountMostLuminous(Star, MostLuminousGiant);
                CountMostMassive(Star, MostMassiveGiant);
                CountLargest(Star, LargestGiant);
                CountHottest(Star, HottestGiant);
                CountOldest(Star, OldestGiant);
                CountMostOblateness(Star, MostOblatenessGiant);
                continue;
            }

            if (SpectralType.LuminosityClass == Astro::FStellarClass::ELuminosityClass::kLuminosity_IV)
            {
                CountClass(SpectralType, Subgiants);
                CountMostLuminous(Star, MostLuminousSubgiant);
                CountMostMassive(Star, MostMassiveSubgiant);
                CountLargest(Star, LargestSubgiant);
                CountHottest(Star, HottestSubgiant);
                CountOldest(Star, OldestSubgiant);
                CountMostOblateness(Star, MostOblatenessSubgiant);
                continue;
            }

            if (SpectralType.LuminosityClass == Astro::FStellarClass::ELuminosityClass::kLuminosity_V)
            {
                CountClass(SpectralType, MainSequence);
                CountMostLuminous(Star, MostLuminousMainSequence);
                CountMostMassive(Star, MostMassiveMainSequence);
                CountLargest(Star, LargestMainSequence);
                CountHottest(Star, HottestMainSequence);
                CountOldest(Star, OldestMainSequence);
                CountMostOblateness(Star, MostOblatenessMainSequence);
                continue;
            }
        }
    }

    std::println("Most luminous main sequence star: luminosity: {}", MostLuminousMainSequence.LuminositySol);
    std::println("{}", FormatInfo(MostLuminousMainSequence.Star));
    std::println("Most luminous Wolf-Rayet star: luminosity: {}", MostLuminousWolfRayet.LuminositySol);
    std::println("{}", FormatInfo(MostLuminousWolfRayet.Star));
    std::println("Most luminous subgiant star: luminosity: {}", MostLuminousSubgiant.LuminositySol);
    std::println("{}", FormatInfo(MostLuminousSubgiant.Star));
    std::println("Most luminous giant star: luminosity: {}", MostLuminousGiant.LuminositySol);
    std::println("{}", FormatInfo(MostLuminousGiant.Star));
    std::println("Most luminous bright giant star: luminosity: {}", MostLuminousBrightGiant.LuminositySol);
    std::println("{}", FormatInfo(MostLuminousBrightGiant.Star));
    std::println("Most luminous supergiant star: luminosity: {}", MostLuminousSupergiant.LuminositySol);
    std::println("{}", FormatInfo(MostLuminousSupergiant.Star));
    std::println("Most luminous hypergiant star: luminosity: {}", MostLuminousHypergiant.LuminositySol);
    std::println("{}", FormatInfo(MostLuminousHypergiant.Star));
    std::println("");
    std::println("Most massive main sequence star: mass: {}", MostMassiveMainSequence.MassSol);
    std::println("{}", FormatInfo(MostMassiveMainSequence.Star));
    std::println("Most massive Wolf-Rayet star: mass: {}", MostMassiveWolfRayet.MassSol);
    std::println("{}", FormatInfo(MostMassiveWolfRayet.Star));
    std::println("Most massive subgiant star: mass: {}", MostMassiveSubgiant.MassSol);
    std::println("{}", FormatInfo(MostMassiveSubgiant.Star));
    std::println("Most massive giant star: mass: {}", MostMassiveGiant.MassSol);
    std::println("{}", FormatInfo(MostMassiveGiant.Star));
    std::println("Most massive bright giant star: mass: {}", MostMassiveBrightGiant.MassSol);
    std::println("{}", FormatInfo(MostMassiveBrightGiant.Star));
    std::println("Most massive supergiant star: mass: {}", MostMassiveSupergiant.MassSol);
    std::println("{}", FormatInfo(MostMassiveSupergiant.Star));
    std::println("Most massive hypergiant star: mass: {}", MostMassiveHypergiant.MassSol);
    std::println("{}", FormatInfo(MostMassiveHypergiant.Star));
    std::println("");
    std::println("Largest main sequence star: radius: {}", LargestMainSequence.RadiusSol);
    std::println("{}", FormatInfo(LargestMainSequence.Star));
    std::println("Largest Wolf-Rayet star: radius: {}", LargestWolfRayet.RadiusSol);
    std::println("{}", FormatInfo(LargestWolfRayet.Star));
    std::println("Largest subgiant star: radius: {}", LargestSubgiant.RadiusSol);
    std::println("{}", FormatInfo(LargestSubgiant.Star));
    std::println("Largest giant star: radius: {}", LargestGiant.RadiusSol);
    std::println("{}", FormatInfo(LargestGiant.Star));
    std::println("Largest bright giant star: radius: {}", LargestBrightGiant.RadiusSol);
    std::println("{}", FormatInfo(LargestBrightGiant.Star));
    std::println("Largest supergiant star: radius: {}", LargestSupergiant.RadiusSol);
    std::println("{}", FormatInfo(LargestSupergiant.Star));
    std::println("Largest hypergiant star: radius: {}", LargestHypergiant.RadiusSol);
    std::println("{}", FormatInfo(LargestHypergiant.Star));
    std::println("");
    std::println("Hottest main sequence star: Teff: {}", HottestMainSequence.Teff);
    std::println("{}", FormatInfo(HottestMainSequence.Star));
    std::println("Hottest Wolf-Rayet star: Teff: {}", HottestWolfRayet.Teff);
    std::println("{}", FormatInfo(HottestWolfRayet.Star));
    std::println("Hottest subgiant star: Teff: {}", HottestSubgiant.Teff);
    std::println("{}", FormatInfo(HottestSubgiant.Star));
    std::println("Hottest giant star: Teff: {}", HottestGiant.Teff);
    std::println("{}", FormatInfo(HottestGiant.Star));
    std::println("Hottest bright giant star: Teff: {}", HottestBrightGiant.Teff);
    std::println("{}", FormatInfo(HottestBrightGiant.Star));
    std::println("Hottest supergiant star: Teff: {}", HottestSupergiant.Teff);
    std::println("{}", FormatInfo(HottestSupergiant.Star));
    std::println("Hottest hypergiant star: Teff: {}", HottestHypergiant.Teff);
    std::println("{}", FormatInfo(HottestHypergiant.Star));
    std::println("");
    std::println("Oldest main sequence star: Age: {}", OldestMainSequence.Age);
    std::println("{}", FormatInfo(OldestMainSequence.Star));
    std::println("Oldest Wolf-Rayet star: Age: {}", OldestWolfRayet.Age);
    std::println("{}", FormatInfo(OldestWolfRayet.Star));
    std::println("Oldest subgiant star: Age: {}", OldestSubgiant.Age);
    std::println("{}", FormatInfo(OldestSubgiant.Star));
    std::println("Oldest giant star: Age: {}", OldestGiant.Age);
    std::println("{}", FormatInfo(OldestGiant.Star));
    std::println("Oldest bright giant star: Age: {}", OldestBrightGiant.Age);
    std::println("{}", FormatInfo(OldestBrightGiant.Star));
    std::println("Oldest supergiant star: Age: {}", OldestSupergiant.Age);
    std::println("{}", FormatInfo(OldestSupergiant.Star));
    std::println("Oldest hypergiant star: Age: {}", OldestHypergiant.Age);
    std::println("{}", FormatInfo(OldestHypergiant.Star));
    std::println("");
    std::println("Most oblateness main sequence star: Oblateness: {}", MostOblatenessMainSequence.Oblateness);
    std::println("{}", FormatInfo(MostOblatenessMainSequence.Star));
    std::println("Most oblateness Wolf-Rayet star: Oblateness: {}", MostOblatenessWolfRayet.Oblateness);
    std::println("{}", FormatInfo(MostOblatenessWolfRayet.Star));
    std::println("Most oblateness subgiant star: Oblateness: {}", MostOblatenessSubgiant.Oblateness);
    std::println("{}", FormatInfo(MostOblatenessSubgiant.Star));
    std::println("Most oblateness giant star: Oblateness: {}", MostOblatenessGiant.Oblateness);
    std::println("{}", FormatInfo(MostOblatenessGiant.Star));
    std::println("Most oblateness bright giant star: Oblateness: {}", MostOblatenessBrightGiant.Oblateness);
    std::println("{}", FormatInfo(MostOblatenessBrightGiant.Star));
    std::println("Most oblateness supergiant star: Oblateness: {}", MostOblatenessSupergiant.Oblateness);
    std::println("{}", FormatInfo(MostOblatenessSupergiant.Star));
    std::println("Most oblateness hypergiant star: Oblateness: {}", MostOblatenessHypergiant.Oblateness);
    std::println("{}", FormatInfo(MostOblatenessHypergiant.Star));

    std::size_t TotalMainSequence = 0;
    for (std::size_t Count : MainSequence)
    {
        TotalMainSequence += Count;
    }

    std::println("");
    std::println("Total main sequence: {}", TotalMainSequence);
    std::println("Total main sequence rate: {}", TotalMainSequence / static_cast<double>(TotalStars));
    std::println("Total O type star rate: {}", static_cast<double>(MainSequence[kTypeOIndex]) / static_cast<double>(TotalMainSequence));
    std::println("Total B type star rate: {}", static_cast<double>(MainSequence[kTypeBIndex]) / static_cast<double>(TotalMainSequence));
    std::println("Total A type star rate: {}", static_cast<double>(MainSequence[kTypeAIndex]) / static_cast<double>(TotalMainSequence));
    std::println("Total F type star rate: {}", static_cast<double>(MainSequence[kTypeFIndex]) / static_cast<double>(TotalMainSequence));
    std::println("Total G type star rate: {}", static_cast<double>(MainSequence[kTypeGIndex]) / static_cast<double>(TotalMainSequence));
    std::println("Total K type star rate: {}", static_cast<double>(MainSequence[kTypeKIndex]) / static_cast<double>(TotalMainSequence));
    std::println("Total M type star rate: {}", static_cast<double>(MainSequence[kTypeMIndex]) / static_cast<double>(TotalMainSequence));
    std::println("Total Wolf-Rayet / O main star rate: {}", static_cast<double>(WolfRayet) / static_cast<double>(MainSequence[kTypeOIndex]));

    std::println("O type main sequence: {}\nB type main sequence: {}\nA type main sequence: {}\nF type main sequence: {}\nG type main sequence: {}\nK type main sequence: {}\nM type main sequence: {}",
                 MainSequence[kTypeOIndex], MainSequence[kTypeBIndex], MainSequence[kTypeAIndex], MainSequence[kTypeFIndex], MainSequence[kTypeGIndex], MainSequence[kTypeKIndex], MainSequence[kTypeMIndex]);
    std::println("O type subgiants: {}\nB type subgiants: {}\nA type subgiants: {}\nF type subgiants: {}\nG type subgiants: {}\nK type subgiants: {}\nM type subgiants: {}",
                 Subgiants[kTypeOIndex], Subgiants[kTypeBIndex], Subgiants[kTypeAIndex], Subgiants[kTypeFIndex], Subgiants[kTypeGIndex], Subgiants[kTypeKIndex], Subgiants[kTypeMIndex]);
    std::println("O type giants: {}\nB type giants: {}\nA type giants: {}\nF type giants: {}\nG type giants: {}\nK type giants: {}\nM type giants: {}",
                 Giants[kTypeOIndex], Giants[kTypeBIndex], Giants[kTypeAIndex], Giants[kTypeFIndex], Giants[kTypeGIndex], Giants[kTypeKIndex], Giants[kTypeMIndex]);
    std::println("O type bright giants: {}\nB type bright giants: {}\nA type bright giants: {}\nF type bright giants: {}\nG type bright giants: {}\nK type bright giants: {}\nM type bright giants: {}",
                 BrightGiants[kTypeOIndex], BrightGiants[kTypeBIndex], BrightGiants[kTypeAIndex], BrightGiants[kTypeAIndex], BrightGiants[kTypeFIndex], BrightGiants[kTypeGIndex], BrightGiants[kTypeMIndex]);
    std::println("O type supergiants: {}\nB type supergiants: {}\nA type supergiants: {}\nF type supergiants: {}\nG type supergiants: {}\nK type supergiants: {}\nM type supergiants: {}",
                 Supergiants[kTypeOIndex], Supergiants[kTypeBIndex], Supergiants[kTypeAIndex], Supergiants[kTypeFIndex], Supergiants[kTypeGIndex], Supergiants[kTypeKIndex], Supergiants[kTypeMIndex]);
    std::println("O type hypergiants: {}\nB type hypergiants: {}\nA type hypergiants: {}\nF type hypergiants: {}\nG type hypergiants: {}\nK type hypergiants: {}\nM type hypergiants: {}",
                 Hypergiants[kTypeOIndex], Hypergiants[kTypeBIndex], Hypergiants[kTypeAIndex], Hypergiants[kTypeFIndex], Hypergiants[kTypeGIndex], Hypergiants[kTypeKIndex], Hypergiants[kTypeMIndex]);
    std::println("Wolf-Rayet stars: {}", WolfRayet);
    std::println("White dwarfs: {}\nNeutron stars: {}\nBlack holes: {}", WhiteDwarfs, NeutronStars, BlackHoles);
    std::println("");
    std::println("Number of single stars: {}", TotalSingles);
    std::println("Number of binary stars: {}", TotalBinarys);
    std::println("");
}

template <typename AstroType, typename DataType>
void FUniverse::MakeChunks(int MaxThread, std::vector<DataType>& Data, std::vector<std::vector<DataType>>& DataLists,
                           std::vector<std::promise<std::vector<AstroType>>>& Promises,
                           std::vector<std::future<std::vector<AstroType>>>& ChunkFutures)
{
    for (std::size_t i = 0; i != Data.size(); ++i)
    {
        std::size_t ThreadId = i % MaxThread;
        DataLists[ThreadId].push_back(std::move(Data[i]));
    }

    Promises.resize(MaxThread);

    for (int i = 0; i != MaxThread; ++i)
    {
        ChunkFutures.push_back(Promises[i].get_future());
    }
}

void FUniverse::GenerateStars(int MaxThread)
{
    NpgsCoreInfo("Initializating and generating basic properties...");
    std::vector<SysGen::FStellarGenerator> Generators;
    std::vector<SysGen::FStellarGenerator::FBasicProperties> BasicProperties;

    auto CreateGenerators =
    [&, this](SysGen::FStellarGenerator::EStellarTypeGenerationOption StellarTypeOption =
              SysGen::FStellarGenerator::EStellarTypeGenerationOption::kRandom,
              float MassLowerLimit = 0.1f,  float MassUpperLimit = 300.0f,
              SysGen::FStellarGenerator::EGenerationDistribution MassDistribution =
              SysGen::FStellarGenerator::EGenerationDistribution::kFromPdf,
              float AgeLowerLimit  = 0.0f,  float AgeUpperLimit  = 1.26e10f,
              SysGen::FStellarGenerator::EGenerationDistribution AgeDistribution  = 
              SysGen::FStellarGenerator::EGenerationDistribution::kFromPdf,
              float FeHLowerLimit  = -4.0f, float FeHUpperLimit  = 0.5f,
              SysGen::FStellarGenerator::EGenerationDistribution FeHDistribution  =
              SysGen::FStellarGenerator::EGenerationDistribution::kFromPdf) -> void
    {
        for (int i = 0; i != MaxThread; ++i)
        {
            std::vector<std::uint32_t> Seeds(32);
            for (int i = 0; i != 32; ++i)
            {
                Seeds[i] = _SeedGenerator(_RandomEngine);
            }

            std::shuffle(Seeds.begin(), Seeds.end(), _RandomEngine);
            std::seed_seq SeedSequence(Seeds.begin(), Seeds.end());

            SysGen::FStellarGenerator::FStellarGenerationInfo GenerationInfo
            {
                .SeedSequence       = &SeedSequence,
                .StellarTypeOption  = StellarTypeOption,
                .MultiplicityOption = SysGen::FStellarGenerator::EMultiplicityGenerationOption::kSingleStar,
                .UniverseAge        = _UniverseAge,
                .MassLowerLimit     = MassLowerLimit,
                .MassUpperLimit     = MassUpperLimit,
                .MassDistribution   = MassDistribution,
                .AgeLowerLimit      = AgeLowerLimit,
                .AgeUpperLimit      = AgeUpperLimit,
                .AgeDistribution    = AgeDistribution,
                .FeHLowerLimit      = FeHLowerLimit,
                .FeHUpperLimit      = FeHUpperLimit,
                .FeHDistribution    = FeHDistribution
            };

            Generators.push_back(GenerationInfo);
        }
    };

    // 生成基础属性
    auto GenerateBasicProperties = [&, this](std::size_t NumStars) -> void
    {
        for (std::size_t i = 0; i != NumStars; ++i)
        {
            std::size_t ThreadId = i % Generators.size();
            BasicProperties.push_back(Generators[ThreadId].GenerateBasicProperties());
        }
    };

    // 特殊星基础参数设置
    if (_ExtraGiantCount != 0)
    {
        Generators.clear();
        CreateGenerators(SysGen::FStellarGenerator::EStellarTypeGenerationOption::kGiant, 1.0f, 35.0f);
        GenerateBasicProperties(_ExtraGiantCount);
    }

    if (_ExtraMassiveStarCount != 0)
    {
        Generators.clear();
        CreateGenerators(SysGen::FStellarGenerator::EStellarTypeGenerationOption::kRandom,
                         20.0f, 300.0f, SysGen::FStellarGenerator::EGenerationDistribution::kUniform,
                         0.0f,  3.5e6f, SysGen::FStellarGenerator::EGenerationDistribution::kUniform);
        GenerateBasicProperties(_ExtraMassiveStarCount);
    }

    if (_ExtraNeutronStarCount != 0)
    {
        Generators.clear();
        CreateGenerators(SysGen::FStellarGenerator::EStellarTypeGenerationOption::kDeathStar,
                         10.0f, 20.0f, SysGen::FStellarGenerator::EGenerationDistribution::kUniform,
                         1e7f,  1e8f,  SysGen::FStellarGenerator::EGenerationDistribution::kUniformByExponent);
        GenerateBasicProperties(_ExtraNeutronStarCount);
    }

    if (_ExtraBlackHoleCount != 0)
    {
        Generators.clear();
        CreateGenerators(SysGen::FStellarGenerator::EStellarTypeGenerationOption::kRandom,
                         35.0f,  300.0f, SysGen::FStellarGenerator::EGenerationDistribution::kUniform,
                         1e7f, 1.26e10f, SysGen::FStellarGenerator::EGenerationDistribution::kFromPdf,
                         -2.0, 0.5);
        GenerateBasicProperties(_ExtraBlackHoleCount);
    }

    if (_ExtraMergeStarCount != 0)
    {
        Generators.clear();
        CreateGenerators(SysGen::FStellarGenerator::EStellarTypeGenerationOption::kMergeStar,
                         0.0f, 0.0f, SysGen::FStellarGenerator::EGenerationDistribution::kUniform,
                         1e6f, 1e8f, SysGen::FStellarGenerator::EGenerationDistribution::kUniformByExponent);
        GenerateBasicProperties(_ExtraMergeStarCount);
    }

    std::size_t CommonStarsCount =
        _StarCount - _ExtraGiantCount - _ExtraMassiveStarCount - _ExtraNeutronStarCount - _ExtraBlackHoleCount - _ExtraMergeStarCount;

    Generators.clear();
    CreateGenerators(SysGen::FStellarGenerator::EStellarTypeGenerationOption::kRandom, 0.075f);
    GenerateBasicProperties(CommonStarsCount);

    NpgsCoreInfo("Interpolating stellar data as {} physical cores...", MaxThread);

    std::vector<Astro::AStar> Stars = InterpolateStars(MaxThread, Generators, BasicProperties);

    NpgsCoreInfo("Building stellar octree in 8 threads...");
    GenerateSlots(0.1f, _StarCount, 0.004f);

    NpgsCoreInfo("Linking positions in octree to stellar systems...");
    _StellarSystems.reserve(_StarCount);
    std::shuffle(Stars.begin(), Stars.end(), _RandomEngine);
    std::vector<glm::vec3> Slots;
    OctreeLinkToStellarSystems(Stars, Slots);

    NpgsCoreInfo("Generating binary stars...");
    GenerateBinaryStars(MaxThread);

    NpgsCoreInfo("Sorting...");
    std::sort(Slots.begin(), Slots.end(), [](const glm::vec3& Point1, const glm::vec3& Point2)
    {
        return glm::length(Point1) < glm::length(Point2);
    });

    NpgsCoreInfo("Assigning name...");
    std::string Name;
    std::ostringstream Stream;
    for (auto& System : _StellarSystems)
    {
        glm::vec3 Position = System.GetBaryPosition();
        auto it = std::lower_bound(Slots.begin(), Slots.end(), Position,
        [](const glm::vec3& Point1, const glm::vec3& Point2) -> bool
        {
            return glm::length(Point1) < glm::length(Point2);
        });
        std::ptrdiff_t Offset = it - Slots.begin();
        Stream << std::setfill('0') << std::setw(8) << std::to_string(Offset);
        Name = "SYSTEM-" + Stream.str();
        System.SetBaryName(Name).SetBaryDistanceRank(Offset);

        auto& Stars = System.StarsData();
        if (Stars.size() > 1)
        {
            std::sort(Stars.begin(), Stars.end(),
            [](const std::unique_ptr<Astro::AStar>& Star1, std::unique_ptr<Astro::AStar>& Star2) -> bool
            {
                return Star1->GetMass() > Star2->GetMass();
            });

            char Rank = 'A';
            for (auto& Star : Stars)
            {
                Star->SetName("STAR-" + Stream.str() + " " + Rank);
                ++Rank;
            }
        }
        else
        {
            Stars.front()->SetName("STAR-" + Stream.str());
        }

        Stream.str("");
        Stream.clear();
    }

    NpgsCoreInfo("Reset home stellar system...");
    FNodeType* HomeNode = _Octree->Find(glm::vec3(0.0f), [](const FNodeType& Node) -> bool
    {
        if (Node.IsLeafNode())
        {
            auto& Points = Node.GetPoints();
            return std::find(Points.begin(), Points.end(), glm::vec3(0.0f)) != Points.end();
        }
        else
        {
            return false;
        }
    });

    auto* HomeSystem = HomeNode->GetLink([](Astro::FStellarSystem* System) -> bool
    {
        return System->GetBaryPosition() == glm::vec3(0.0f);
    });
    HomeNode->RemoveStorage();
    HomeNode->AddPoint(glm::vec3(0.0f));
    HomeSystem->SetBaryNormal(glm::vec2(0.0f));

    for (auto& Star : HomeSystem->StarsData())
    {
        Star->SetNormal(glm::vec3(0.0f));
    }

    NpgsCoreInfo("Stellar generation completed.");
}

void FUniverse::FillStellarSystem(int MaxThread)
{
    NpgsCoreInfo("Generating planets...");

    std::vector<SysGen::FOrbitalGenerator> Generators;

    for (int i = 0; i != MaxThread; ++i)
    {
        std::vector<std::uint32_t> Seeds(32);
        for (int i = 0; i != 32; ++i)
        {
            Seeds[i] = _SeedGenerator(_RandomEngine);
        }

        std::shuffle(Seeds.begin(), Seeds.end(), _RandomEngine);
        std::seed_seq SeedSequence(Seeds.begin(), Seeds.end());

        Generators.push_back(SeedSequence);
    }
}

std::vector<Astro::AStar>
FUniverse::InterpolateStars(int MaxThread, std::vector<SysGen::FStellarGenerator>& Generators,
                            std::vector<SysGen::FStellarGenerator::FBasicProperties>& BasicProperties)
{
    std::vector<std::vector<SysGen::FStellarGenerator::FBasicProperties>> PropertyLists(MaxThread);
    std::vector<std::promise<std::vector<Astro::AStar>>> Promises(MaxThread);
    std::vector<std::future<std::vector<Astro::AStar>>> ChunkFutures;

    MakeChunks(MaxThread, BasicProperties, PropertyLists, Promises, ChunkFutures);

    for (int i = 0; i != MaxThread; ++i)
    {
        _ThreadPool->Submit([&, i]() -> void
        {
            std::vector<Astro::AStar> Stars;
            for (auto& Properties : PropertyLists[i])
            {
                Stars.push_back(Generators[i].GenerateStar(Properties));
            }
            Promises[i].set_value(std::move(Stars));
        });
    }

    BasicProperties.clear();

    std::vector<Astro::AStar> Stars;
    for (auto& Future : ChunkFutures)
    {
        auto Chunk = Future.get();
        Stars.insert(Stars.end(), std::make_move_iterator(Chunk.begin()), std::make_move_iterator(Chunk.end()));
    }

    return Stars;
}

void FUniverse::GenerateSlots(float MinDistance, std::size_t SampleCount, float Density)
{
    float Radius     = std::pow((3.0f * SampleCount / (4 * Math::kPi * Density)), (1.0f / 3.0f));
    float LeafSize   = std::pow((1.0f / Density), (1.0f / 3.0f));
    int   Exponent   = static_cast<int>(std::ceil(std::log2(Radius / LeafSize)));
    float LeafRadius = LeafSize * 0.5f;
    float RootRadius = LeafSize * static_cast<float>(std::pow(2, Exponent));

    _Octree = std::make_unique<System::Spatial::TOctree<Astro::FStellarSystem>>(glm::vec3(0.0), RootRadius);
    _Octree->BuildEmptyTree(LeafRadius); // 快速构建一个空树，每个叶子节点作为一个格子，用于生成恒星

    // 遍历八叉树，将距离原点大于半径的叶子节点标记为无效，保证恒星只会在范围内生成
    _Octree->Traverse([Radius](FNodeType& Node) -> void
    {
        if (Node.IsLeafNode() && glm::length(Node.GetCenter()) > Radius)
        {
            Node.SetValidation(false);
        }
    });

    std::size_t ValidLeafCount = _Octree->GetCapacity();
    std::vector<FNodeType*> LeafNodes;

    auto CollectLeafNodes = [&LeafNodes](FNodeType& Node) -> void
    {
        if (Node.IsLeafNode())
        {
            LeafNodes.push_back(&Node);
        }
    };

    // 使用栅格采样，八叉树的每个叶子节点作为一个格子，在这个格子中生成一个恒星
    while (ValidLeafCount != SampleCount)
    {
        LeafNodes.clear();
        _Octree->Traverse(CollectLeafNodes);
        std::shuffle(LeafNodes.begin(), LeafNodes.end(), _RandomEngine); // 打乱叶子节点，保证随机性

        // 删除或收回叶子节点，直到格子数量等于目标数量
        if (ValidLeafCount < SampleCount)
        {
            for (auto* Node : LeafNodes)
            {
                glm::vec3 Center = Node->GetCenter();
                float Distance = glm::length(Center);
                if (!Node->GetValidation() && Distance >= Radius && Distance <= Radius + LeafRadius)
                {
                    Node->SetValidation(true);
                    if (++ValidLeafCount == SampleCount)
                    {
                        break;
                    }
                }
            }
        }
        else
        {
            for (auto* Node : LeafNodes)
            {
                glm::vec3 Center = Node->GetCenter();
                float Distance = glm::length(Center);
                if (Node->GetValidation() && Distance >= Radius - LeafRadius && Distance <= Radius)
                {
                    Node->SetValidation(false);
                    if (--ValidLeafCount == SampleCount)
                    {
                        break;
                    }
                }
            }
        }
    }

    Util::TUniformRealDistribution Offset(-LeafRadius, LeafRadius - MinDistance); // 用于随机生成恒星位置相对于叶子节点中心点的偏移量
    // 遍历八叉树，为每个有效的叶子节点生成一个恒星
    _Octree->Traverse([&Offset, LeafRadius, MinDistance, this](FNodeType& Node) -> void
    {
        if (Node.IsLeafNode() && Node.GetValidation())
        {
            glm::vec3 Center(Node.GetCenter());
            glm::vec3 StellarSlot(Center.x + Offset(_RandomEngine),
                                  Center.y + Offset(_RandomEngine),
                                  Center.z + Offset(_RandomEngine));
            Node.AddPoint(StellarSlot);
        }
    });

    // 为了保证恒星系统的唯一性，将原点附近所在的叶子节点作为存储初始恒星系统的结点
    // 寻找包含了 (LeafRadius, LeafRadius, LeafRadius) 的叶子节点，将这个格子存储的位置修改为原点
    FNodeType* HomeNode = _Octree->Find(glm::vec3(LeafRadius), [](const FNodeType& Node) -> bool
    {
        return Node.IsLeafNode();
    });

    // 把最靠近原点的格子存储旧的位置点删除，加入初始恒星系统
    HomeNode->RemoveStorage();
    HomeNode->AddPoint(glm::vec3(0.0f));
}

void FUniverse::OctreeLinkToStellarSystems(std::vector<Astro::AStar>& Stars, std::vector<glm::vec3>& Slots)
{
    std::size_t Index = 0;

    _Octree->Traverse([&](FNodeType& Node) -> void
    {
        if (Node.IsLeafNode() && Node.GetValidation())
        {
            for (const auto& Point : Node.GetPoints())
            {
                Astro::FBaryCenter NewBary(Point, glm::vec2(0.0f), 0, "");
                Astro::FStellarSystem NewSystem(NewBary);
                NewSystem.StarsData().push_back(std::make_unique<Astro::AStar>(Stars.back()));
                NewSystem.SetBaryNormal(NewSystem.StarsData().front()->GetNormal());
                Stars.pop_back();

                _StellarSystems.push_back(std::move(NewSystem));

                Node.AddLink(&_StellarSystems[Index]);
                Slots.push_back(Point);
                ++Index;
            }
        }
    });
}

void FUniverse::GenerateBinaryStars(int MaxThread)
{
    std::vector<SysGen::FStellarGenerator> Generators;
    for (int i = 0; i != MaxThread; ++i)
    {
        std::vector<std::uint32_t> Seeds(32);
        for (int i = 0; i != 32; ++i)
        {
            Seeds[i] = _SeedGenerator(_RandomEngine);
        }

        std::shuffle(Seeds.begin(), Seeds.end(), _RandomEngine);
        std::seed_seq SeedSequence(Seeds.begin(), Seeds.end());

        SysGen::FStellarGenerator::FStellarGenerationInfo GenerationInfo
        {
            .SeedSequence       = &SeedSequence,
            .StellarTypeOption  = SysGen::FStellarGenerator::EStellarTypeGenerationOption::kRandom,
            .MultiplicityOption = SysGen::FStellarGenerator::EMultiplicityGenerationOption::kBinarySecondStar
        };

        Generators.push_back(GenerationInfo);
    }

    std::vector<Astro::FStellarSystem*> BinarySystems;
    for (auto& System : _StellarSystems)
    {
        const auto& Star = System.StarsData().front();
        if (!Star->GetIsSingleStar())
        {
            BinarySystems.push_back(&System);
        }
    }

    std::vector<SysGen::FStellarGenerator::FBasicProperties> BasicProperties;
    for (std::size_t i = 0; i != BinarySystems.size(); ++i)
    {
        std::size_t ThreadId    = i % MaxThread;
        auto& SelectedGenerator = Generators[ThreadId];

        const auto& Star              = BinarySystems[i]->StarsData().front();
        float FirstStarInitialMassSol = Star->GetInitialMass() / kSolarMass;
        float MassLowerLimit          = std::max(0.075f, 0.1f * FirstStarInitialMassSol);
        float MassUpperLimit          = std::min(10 * FirstStarInitialMassSol, 300.0f);

        SelectedGenerator.SetMassLowerLimit(MassLowerLimit);
        SelectedGenerator.SetMassUpperLimit(MassUpperLimit);
        SelectedGenerator.SetLogMassSuggestDistribution(
            std::make_unique<Util::TNormalDistribution<>>(std::log10(FirstStarInitialMassSol), 0.25f));

        double Age = Star->GetAge();
        float  FeH = Star->GetFeH();

        if (std::to_underlying(Star->GetEvolutionPhase()) > 10)
        {
            Age -= Star->GetLifetime();
        }

        BasicProperties.push_back(SelectedGenerator.GenerateBasicProperties(static_cast<float>(Age), FeH));
    }

    std::vector<Astro::AStar> Stars = InterpolateStars(MaxThread, Generators, BasicProperties);

    for (std::size_t i = 0; i != BinarySystems.size(); ++i)
    {
        BinarySystems[i]->StarsData().push_back(std::make_unique<Astro::AStar>(Stars[i]));
    }
}

_NPGS_END
