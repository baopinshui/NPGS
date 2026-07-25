#define GLM_FORCE_ALIGNED_GENTYPES
#include "Npgs.h"
#include "Application.h"

using namespace Npgs;
using namespace Npgs::Util;

int main()
{
    FLogger::Initialize();

    FApplication App({ 1280, 960 }, "Learn glNext FPS:", false, true);
    App.ExecuteMainRender();
    return 0;
}
