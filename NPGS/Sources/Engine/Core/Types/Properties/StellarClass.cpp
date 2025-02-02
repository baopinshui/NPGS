#include "StellarClass.h"

#include <cctype>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <utility>

#include "Engine/Core/Base/Assert.h"

_NPGS_BEGIN
_ASTRO_BEGIN

// FStellarClass implementations
// -----------------------------
FStellarClass::FStellarClass()
    : _StellarType(EStellarType::kNormalStar), _SpectralType(0)
{
}

FStellarClass::FStellarClass(EStellarType StellarType, const FSpectralType& SpectralType)
    : _StellarType(StellarType), _SpectralType(0)
{
    Load(SpectralType);
}

FStellarClass::FSpectralType FStellarClass::Data() const
{
    FSpectralType SpectralType;

    SpectralType.HSpectralClass  = static_cast<ESpectralClass>(_SpectralType >> 57 & 0x1F);
    SpectralType.Subclass        = (_SpectralType >> 53 & 0xF) + (_SpectralType >> 49 & 0xF) / 10.0f;
    SpectralType.bIsAmStar       = _SpectralType >> 48 & 0x1;
    SpectralType.MSpectralClass  = static_cast<ESpectralClass>(_SpectralType >> 44 & 0xF);
    SpectralType.AmSubclass      = (_SpectralType >> 40 & 0xF) + (_SpectralType >> 36 & 0xF) / 10.0f;
    SpectralType.LuminosityClass = static_cast<ELuminosityClass>(_SpectralType >> 32 & 0xF);
    SpectralType.SpecialMark     = static_cast<FSpecialMarkDigital>(_SpectralType & 0x1FFFFFFFF);

    if (SpectralType.HSpectralClass == ESpectralClass::kSpectral_Unknown)
    {
        SpectralType =
        {
            .HSpectralClass  = ESpectralClass::kSpectral_Unknown,
            .MSpectralClass  = ESpectralClass::kSpectral_Unknown,
            .LuminosityClass = ELuminosityClass::kLuminosity_Unknown,
            .SpecialMark     = std::to_underlying(ESpecialMark::kCode_Null),
            .Subclass        = 0.0f,
            .AmSubclass      = 0.0f,
            .bIsAmStar       = false
        };
    }

    return SpectralType;
}

bool FStellarClass::Load(const FSpectralType& SpectralType)
{
    // 结构
    // --------------------------------------------------------------------------------------------------
    // std::uint64_t
    // |----|-------|------|------|---|------|------|------|------|-------------------------------------|
    // | 00 | 00000 | 0000 | 0000 | 0 | 0000 | 0000 | 0000 | 0000 | 00000000 00000000 00000000 00000000 |
    // |----|-------|------|------|---|------|------|------|------|-------------------------------------|
    // 恒星类型 光谱  亚型高位 亚型低位 Am m 光谱 m 亚型高位 m 亚型低位 光度级 特殊标识

    std::uint64_t Data         = 0;
    std::uint64_t SubclassHigh = static_cast<std::uint64_t>(SpectralType.Subclass);
    float         Intermediate = std::round((SpectralType.Subclass - SubclassHigh) * 1000.0f) / 1000.0f;
    std::uint64_t SubclassLow  = static_cast<std::uint64_t>(Intermediate * 10.0f);

    Data |= static_cast<std::uint64_t>(_StellarType)                 << 62;
    Data |= static_cast<std::uint64_t>(SpectralType.HSpectralClass)  << 57;
    Data |= SubclassHigh                                             << 53;
    Data |= SubclassLow                                              << 49;
    Data |= static_cast<std::uint64_t>(SpectralType.bIsAmStar)       << 48;
    Data |= static_cast<std::uint64_t>(SpectralType.MSpectralClass)  << 44;

    SubclassHigh = static_cast<std::uint64_t>(SpectralType.AmSubclass);
    Intermediate = std::round((SpectralType.AmSubclass - SubclassHigh) * 1000.0f) / 1000.0f;
    SubclassLow  = static_cast<std::uint64_t>(Intermediate * 10.0f);

    Data |= SubclassHigh                                             << 40;
    Data |= SubclassLow                                              << 36;
    Data |= static_cast<std::uint64_t>(SpectralType.LuminosityClass) << 32;
    Data |= static_cast<std::uint64_t>(SpectralType.SpecialMark)     << 0;

    _SpectralType = Data;
    return true;
}

std::string FStellarClass::ToString() const
{
    FSpectralType SpectralType = Data();

    if (SpectralType.HSpectralClass == ESpectralClass::kSpectral_Unknown)
    {
        return "Unknown";
    }

    std::string SpectralTypeStr = SpectralClassToString(SpectralType.HSpectralClass, SpectralType.Subclass);

    if (SpectralType.bIsAmStar)
    {
        SpectralTypeStr += "m" + SpectralClassToString(SpectralType.MSpectralClass, SpectralType.AmSubclass);
    }

    SpectralTypeStr += LuminosityClassToString(SpectralType.LuminosityClass);
    SpectralTypeStr += SpecialMarkToString(static_cast<ESpecialMark>(SpectralType.SpecialMark));

    return SpectralTypeStr;
}

FStellarClass FStellarClass::Parse(const std::string& StellarClassStr)
{
    NpgsAssert(!StellarClassStr.empty(), "StellarClassStr is empty.");

    EStellarType        StellarType     = EStellarType::kNormalStar;
    ESpectralClass      HSpectralClass  = ESpectralClass::kSpectral_Unknown;
    ESpectralClass      MSpectralClass  = ESpectralClass::kSpectral_Unknown;
    ELuminosityClass    LuminosityClass = ELuminosityClass::kLuminosity_Unknown;
    FSpecialMarkDigital SpecialMark     = std::to_underlying(ESpecialMark::kCode_Null);
    float               Subclass        = 0.0f;
    float               AmSubclass      = 0.0f;
    bool                bIsAmStar       = false;

    EParseState State   = EParseState::kBegin;
    std::size_t Index   = 0;
    bool bParsingAmStar = false;

    while (State != EParseState::kEnd)
    {
        char Char = 0;
        char NextChar = Index + 1 >= StellarClassStr.size() ? '\0' : StellarClassStr[Index + 1];
        if (Index == StellarClassStr.size())
        {
            Char = '\0';
        }
        else
        {
            Char = StellarClassStr[Index];
        }

        switch (State)
        {
        case EParseState::kBegin:
            State = ParseStellarType(Char, StellarType, HSpectralClass, Index);
            break;

        case EParseState::kSpectralClass:
            if (!bParsingAmStar)
            {
                State = ParseSpectralClass(Char, HSpectralClass, Index);
            }
            else
            {
                State = ParseSpectralClass(Char, MSpectralClass, Index);
            }

            break;

        case EParseState::kWolfRayetStar:
            State = ParseWolfRayetStar(Char, HSpectralClass, Index);
            break;

        case EParseState::kWhiteDwarf:
            State = ParseWhiteDwarf(Char, HSpectralClass, Index);
            break;

        case EParseState::kWhiteDwarfEx:
            State = ParseWhiteDwarfEx(Char, StellarClassStr[Index - 1], HSpectralClass, Index);
            break;

        case EParseState::kSubdwarfPerfix:
            switch (Char)
            {
            case 'd':
                LuminosityClass = ELuminosityClass::kLuminosity_VI;
                State = EParseState::kSpectralClass;
                ++Index;
                break;
            default:
                State = EParseState::kEnd;
                break;
            }

            break;

        case EParseState::kSubclass:
            if (std::isdigit(Char))
            {
                if (!bParsingAmStar)
                {
                    Subclass = static_cast<float>(Char - '0');
                }
                else
                {
                    AmSubclass = static_cast<float>(Char - '0');
                }

                State = EParseState::kSubclassDecimal;
                ++Index;
            }
            else
            {
                State = EParseState::kSpecialMark;
            }

            break;

        case EParseState::kSubclassDecimal:
            if (Char == '.')
            {
                State = EParseState::kSubclassDecimalFinal;
                ++Index;
            }
            else
            {
                State = EParseState::kSpecialMark;
            }

            break;

        case EParseState::kSubclassDecimalFinal:
            if (std::isdigit(Char))
            {
                if (!bParsingAmStar)
                {
                    Subclass += 0.1f * (Char - '0');
                }
                else
                {
                    AmSubclass += 0.1f * (Char - '0');
                }
            }

            State = EParseState::kSpecialMark;
            ++Index;
            break;

        case EParseState::kSpecialMark:
            if ((State = ParseSpecialMark(Char, NextChar, SpecialMark, Index)) == EParseState::kSpectralClass)
            {
                bParsingAmStar = true;
                bIsAmStar = true;
            }

            break;

        case EParseState::kLuminosityClass:
            State = ParseLuminosityClass(Char, LuminosityClass, Index);
            break;

        case EParseState::kLuminosityClassI:
            State = ParseLuminosityClassI(Char, LuminosityClass, Index);
            break;

        case EParseState::kLuminosityClassIa:
            State = ParseLuminosityClassIa(Char, LuminosityClass);
            break;

        case EParseState::kLuminosityClassII:
            State = ParseLuminosityClassII(Char, LuminosityClass);
            break;

        case EParseState::kLuminosityClassV:
            State = ParseLuminosityClassV(Char, LuminosityClass);
            break;
        }
    }

    return { StellarType, { HSpectralClass, MSpectralClass, LuminosityClass, SpecialMark, Subclass, AmSubclass, bIsAmStar } };
}

// Processor functions implementations
// -----------------------------------
FStellarClass::EParseState FStellarClass::ParseStellarType(char Char, FStellarClass::EStellarType& StellarType, FStellarClass::ESpectralClass& HSpectralClass, std::size_t& Index)
{
    switch (Char)
    {
    case 'X':
        StellarType = FStellarClass::EStellarType::kBlackHole;
        return EParseState::kEnd;
    case 'Q':
        StellarType = FStellarClass::EStellarType::kNeutronStar;
        return EParseState::kEnd;
    case 'D':
        StellarType = FStellarClass::EStellarType::kWhiteDwarf;
        HSpectralClass = FStellarClass::ESpectralClass::kSpectral_D;
        ++Index;
        return EParseState::kWhiteDwarf;
    case 's': // sd 前缀
        StellarType = FStellarClass::EStellarType::kNormalStar;
        ++Index;
        return EParseState::kSubdwarfPerfix;
    case '?':
        return EParseState::kEnd;
    default:
        StellarType = FStellarClass::EStellarType::kNormalStar;
        return EParseState::kSpectralClass;
    }
}

FStellarClass::EParseState FStellarClass::ParseSpectralClass(char Char, FStellarClass::ESpectralClass& SpectralClass, std::size_t& Index)
{
    switch (Char)
    {
    case 'W':
        ++Index;
        return EParseState::kWolfRayetStar;
    case 'O':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_O;
        ++Index;
        return EParseState::kSubclass;
    case 'B':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_B;
        ++Index;
        return EParseState::kSubclass;
    case 'A':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_A;
        ++Index;
        return EParseState::kSubclass;
    case 'F':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_F;
        ++Index;
        return EParseState::kSubclass;
    case 'G':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_G;
        ++Index;
        return EParseState::kSubclass;
    case 'K':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_K;
        ++Index;
        return EParseState::kSubclass;
    case 'M':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_M;
        ++Index;
        return EParseState::kSubclass;
    case 'R':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_R;
        ++Index;
        return EParseState::kSubclass;
    case 'N':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_N;
        ++Index;
        return EParseState::kSubclass;
    case 'C':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_C;
        ++Index;
        return EParseState::kSubclass;
    case 'S':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_S;
        ++Index;
        return EParseState::kSubclass;
    case 'L':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_L;
        ++Index;
        return EParseState::kSubclass;
    case 'T':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_T;
        ++Index;
        return EParseState::kSubclass;
    case 'Y':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_Y;
        ++Index;
        return EParseState::kSubclass;
    default:
        return EParseState::kEnd;
    }
}

FStellarClass::EParseState FStellarClass::ParseWolfRayetStar(char Char, FStellarClass::ESpectralClass& SpectralClass, std::size_t& Index)
{
    switch (Char)
    {
    case 'C':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_WC;
        ++Index;
        return EParseState::kSubclass;
    case 'N':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_WN;
        ++Index;
        return EParseState::kSubclass;
    case 'O':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_WO;
        ++Index;
        return EParseState::kSubclass;
    default:
        return EParseState::kEnd;
    }
}

FStellarClass::EParseState FStellarClass::ParseWhiteDwarf(char Char, FStellarClass::ESpectralClass& SpectralClass, std::size_t& Index)
{
    ++Index;

    switch (Char)
    {
    case 'A':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_DA;
        return EParseState::kWhiteDwarfEx;
    case 'B':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_DB;
        return EParseState::kWhiteDwarfEx;
    case 'C':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_DC;
        return EParseState::kWhiteDwarfEx;
    case 'O':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_DO;
        return EParseState::kWhiteDwarfEx;
    case 'Q':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_DQ;
        return EParseState::kWhiteDwarfEx;
    case 'X':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_DX;
        return EParseState::kWhiteDwarfEx;
    case 'Z':
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_DZ;
        return EParseState::kWhiteDwarfEx;
    default:
        SpectralClass = FStellarClass::ESpectralClass::kSpectral_D;
        return EParseState::kSubclass;
    }
}

FStellarClass::EParseState FStellarClass::ParseWhiteDwarfEx(char Char, char PrevChar, FStellarClass::ESpectralClass& SpectralClass, std::size_t& Index)
{
    if (Char == PrevChar)
    {
        NpgsAssert(false, "Invalid white dwarf extended type.");
    }

    switch (Char)
    {
    case 'A':
        ++Index;
        break;
    case 'B':
        ++Index;
        break;
    case 'C':
        ++Index;
        break;
    case 'O':
        ++Index;
        break;
    case 'Q':
        ++Index;
        break;
    case 'X':
        ++Index;
        break;
    case 'Z':
        ++Index;
        break;
    default:
        break;
    }

    return EParseState::kSubclass;
}

FStellarClass::EParseState FStellarClass::ParseLuminosityClass(char Char, FStellarClass::ELuminosityClass& LuminosityClass, std::size_t& Index)
{
    switch (Char)
    {
    case '0':
        if (LuminosityClass == FStellarClass::ELuminosityClass::kLuminosity_Unknown)
        {
            LuminosityClass  = FStellarClass::ELuminosityClass::kLuminosity_0;
            return EParseState::kSpecialMark;
        }
        else
        {
            return EParseState::kEnd;
        }
    case 'I':
        ++Index;
        return EParseState::kLuminosityClassI;
    case 'V':
        ++Index;
        return EParseState::kLuminosityClassV;
    case ' ':
        ++Index;
        return EParseState::kLuminosityClass;
    default:
        return EParseState::kSpecialMark;
    }
}

FStellarClass::EParseState FStellarClass::ParseLuminosityClassI(char Char, FStellarClass::ELuminosityClass& LuminosityClass, std::size_t& Index)
{
    switch (Char)
    {
    case 'a':
        ++Index;
        return EParseState::kLuminosityClassIa;
    case 'b':
        LuminosityClass = FStellarClass::ELuminosityClass::kLuminosity_Ib;
        ++Index;
        return EParseState::kSpecialMark;
    case 'I':
        ++Index;
        return EParseState::kLuminosityClassII;
    case 'V':
        LuminosityClass = FStellarClass::ELuminosityClass::kLuminosity_IV;
        ++Index;
        return EParseState::kSpecialMark;
    default:
        LuminosityClass = FStellarClass::ELuminosityClass::kLuminosity_I;
        return EParseState::kSpecialMark;
    }
}

FStellarClass::EParseState FStellarClass::ParseLuminosityClassIa(char Char, FStellarClass::ELuminosityClass& LuminosityClass)
{
    switch (Char)
    {
    case '+':
        LuminosityClass = FStellarClass::ELuminosityClass::kLuminosity_IaPlus;
        return EParseState::kSpecialMark;
    case 'b':
        LuminosityClass = FStellarClass::ELuminosityClass::kLuminosity_Iab;
        return EParseState::kSpecialMark;
    default:
        LuminosityClass = FStellarClass::ELuminosityClass::kLuminosity_Ia;
        return EParseState::kSpecialMark;
    }
}

FStellarClass::EParseState FStellarClass::ParseLuminosityClassII(char Char, FStellarClass::ELuminosityClass& LuminosityClass)
{
    switch (Char)
    {
    case 'I':
        LuminosityClass = FStellarClass::ELuminosityClass::kLuminosity_III;
        return EParseState::kSpecialMark;
    default:
        LuminosityClass = FStellarClass::ELuminosityClass::kLuminosity_II;
        return EParseState::kSpecialMark;
    }
}

FStellarClass::EParseState FStellarClass::ParseLuminosityClassV(char Char, FStellarClass::ELuminosityClass& LuminosityClass)
{
    switch (Char)
    {
    case 'I':
        LuminosityClass = FStellarClass::ELuminosityClass::kLuminosity_VI;
        return EParseState::kSpecialMark;
    default:
        LuminosityClass = FStellarClass::ELuminosityClass::kLuminosity_V;
        return EParseState::kSpecialMark;
    }
}

FStellarClass::EParseState FStellarClass::ParseSpecialMark(char Char, char NextChar, FStellarClass::FSpecialMarkDigital& SpecialMark, std::size_t& Index)
{
    switch (Char)
    {
    case 'm':
        SpecialMark |= static_cast<std::uint32_t>(FStellarClass::ESpecialMark::kCode_m);
        ++Index;
        return EParseState::kSpectralClass;
    case 'f':
        SpecialMark |= static_cast<std::uint32_t>(FStellarClass::ESpecialMark::kCode_f);
        ++Index;
        return (std::isalpha(NextChar) && std::islower(NextChar)) ? EParseState::kSpecialMark : EParseState::kEnd;
    case 'h':
        SpecialMark |= static_cast<std::uint32_t>(FStellarClass::ESpecialMark::kCode_h);
        ++Index;
        return (std::isalpha(NextChar) && std::islower(NextChar)) ? EParseState::kSpecialMark : EParseState::kEnd;
    case 'p':
        SpecialMark |= static_cast<std::uint32_t>(FStellarClass::ESpecialMark::kCode_h);
        ++Index;
        return (std::isalpha(NextChar) && std::islower(NextChar)) ? EParseState::kSpecialMark : EParseState::kEnd;
    case '+':
        ++Index;
        return EParseState::kSpecialMark;
    case ' ':
        ++Index;
        return EParseState::kSpecialMark;
    case '\0':
        return EParseState::kEnd;
    default:
        return EParseState::kLuminosityClass;
    }
}

std::string FStellarClass::SpectralClassToString(FStellarClass::ESpectralClass SpectralClass, float Subclass)
{
    std::ostringstream Stream;

    if (Subclass == std::floor(Subclass))
    {
        Stream << std::fixed << std::setprecision(0) << Subclass;
    }
    else
    {
        Subclass = std::round(Subclass * 10.0f) / 10.0f;
        Stream << std::fixed << std::setprecision(1) << Subclass;
    }

    switch (SpectralClass)
    {
    case FStellarClass::ESpectralClass::kSpectral_O:
        return std::string("O") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_B:
        return std::string("B") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_A:
        return std::string("A") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_F:
        return std::string("F") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_G:
        return std::string("G") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_K:
        return std::string("K") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_M:
        return std::string("M") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_R:
        return std::string("R") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_N:
        return std::string("N") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_C:
        return std::string("C") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_S:
        return std::string("S") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_WO:
        return std::string("WO") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_WN:
        return std::string("WN") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_WC:
        return std::string("WC") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_L:
        return std::string("L") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_T:
        return std::string("T") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_Y:
        return std::string("Y") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_D:
        return std::string("D") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_DA:
        return std::string("DA") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_DB:
        return std::string("DB") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_DC:
        return std::string("DC") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_DO:
        return std::string("DO") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_DQ:
        return std::string("DQ") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_DX:
        return std::string("DX") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_DZ:
        return std::string("DZ") + Stream.str();
    case FStellarClass::ESpectralClass::kSpectral_Q:
        return std::string("Q");
    case FStellarClass::ESpectralClass::kSpectral_X:
        return std::string("X");
    default:
        return std::string("Unknown");
    }
}

std::string FStellarClass::LuminosityClassToString(FStellarClass::ELuminosityClass LuminosityClass)
{
    switch (LuminosityClass)
    {
    case FStellarClass::ELuminosityClass::kLuminosity_0:
        return std::string("0");
    case FStellarClass::ELuminosityClass::kLuminosity_IaPlus:
        return std::string("Ia+");
    case FStellarClass::ELuminosityClass::kLuminosity_Ia:
        return std::string("Ia");
    case FStellarClass::ELuminosityClass::kLuminosity_Ib:
        return std::string("Ib");
    case FStellarClass::ELuminosityClass::kLuminosity_Iab:
        return std::string("Iab");
    case FStellarClass::ELuminosityClass::kLuminosity_I:
        return std::string("I");
    case FStellarClass::ELuminosityClass::kLuminosity_II:
        return std::string("II");
    case FStellarClass::ELuminosityClass::kLuminosity_III:
        return std::string("III");
    case FStellarClass::ELuminosityClass::kLuminosity_IV:
        return std::string("IV");
    case FStellarClass::ELuminosityClass::kLuminosity_V:
        return std::string("V");
    case FStellarClass::ELuminosityClass::kLuminosity_VI:
        return std::string("VI");
    default:
        return std::string("");
    }
}

std::string FStellarClass::SpecialMarkToString(FStellarClass::ESpecialMark SpecialMark)
{
    switch (SpecialMark)
    {
    case FStellarClass::ESpecialMark::kCode_Null:
        return std::string("");
    case FStellarClass::ESpecialMark::kCode_f:
        return std::string("f");
    case FStellarClass::ESpecialMark::kCode_h:
        return std::string("h");
    case FStellarClass::ESpecialMark::kCode_p:
        return std::string("p");
    default:
        return std::string("");
    }
}

_ASTRO_END
_NPGS_END
