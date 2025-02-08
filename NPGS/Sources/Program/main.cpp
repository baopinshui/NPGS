#include "Npgs.h"
#include "Application.h"

using namespace Npgs;
using namespace Npgs::Util;

int main()
{
    FLogger::Initialize();

    FApplication App({ 1280, 960 }, "Learn glNext FPS:", true, false);
    App.ExecuteMainRender();
    return 0;
}

//#include "Engine/Utils/FieldReflection.hpp"
//
//template <typename T>
//void TestForEach()
//{
//    T s{};
//    Npgs::Util::ForEachField(s, [&s](const auto& member, size_t index) {
//        const auto offset = reinterpret_cast<const char*>(&member) -
//            reinterpret_cast<const char*>(&s);
//        std::cout << "Member " << index
//            << ", offset=" << offset
//            << ", size=" << sizeof(member)
//            << std::endl;
//    });
//}
//
//struct Test
//{
//    int a;
//    float b;
//    double c;
//    std::string d;
//    std::vector<int> e;
//    std::tuple<int, int> f;
//};
//
//int main()
//{
//    TestForEach<Test>();
//}
