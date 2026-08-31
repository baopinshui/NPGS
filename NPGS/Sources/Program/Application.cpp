//baopinsui的话：这个部分充满不规范的一次性代码，（因为这不是我的强项)，目前我最得意的作品请见blackhole_common.glsl
#include "Application.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_aligned.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Engine/Core/Base/Config/EngineConfig.h"
#include "Engine/Core/Math/NumericConstants.h"
#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Core/Runtime/AssetLoaders/Shader.h"
#include "Engine/Core/Runtime/AssetLoaders/Texture.h"
#include "Engine/Core/Runtime/Graphics/Renderers/PipelineManager.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/ShaderResourceManager.h"
#include "Engine/Utils/Logger.h"
#include "Engine/Utils/TooolfuncForStarMap.h"
#include "DataStructures.h"

#include "Engine/Core/System/UI/AppContext.h"
#include "Engine/Core/System/UI/Screens/MainMenuScreen.h"
#include "Engine/Core/System/UI/Screens/GameScreen.h"

#include <chrono>
#include <fstream>
#include <sstream>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp> 
// 在 Application.cpp 的 #include 区域添加
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <iomanip>
#include <filesystem>
#include <iostream> // for std::cerr
#include <algorithm> // for std::max
#include <cmath> // for std::sqrt

#include <glm/gtc/packing.hpp>
// 添加一个全局的截图请求标志位
static bool g_bRequestScreenshot = false;
static bool g_GeodesicMode = false; // 新增：测地线模式开关
static bool g_bHideUIAndMouse = false;
static std::vector<std::string> g_DiskTextures;
static int g_CurrentDiskState = -1; // -1 表示使用默认体积盘，>=0 表示当前显示的图片索引
static float g_OriginalThinRs = 0.0f;
static float g_OriginalHopper = 0.0f;
static bool g_DiskStateChanged = false;
static Npgs::Runtime::Graphics::FVulkanSampler* g_GlobalSampler = nullptr;
FGameArgs GameArgs{};
FBlackHoleArgs BlackHoleArgs{};




static double g_GeoState[20];
static bool g_isOutgoing = false;
static double g_UniverseSign = 1.0;

double g_Q = 0.0; // 对应黑洞的电荷 (Electric Charge)
double g_P = 0.0; // 对应黑洞的磁荷 (Magnetic Monopole)
namespace GeodesicIntegrator
{

extern double g_ProperAcceleration[3];
double g_ProperAcceleration[3] = { 0.0, 0.0, 0.0 };
extern double g_CameraCharge;
double g_CameraCharge = 0.0;
extern double g_ProperTime;
double g_ProperTime = 0.0;
double GetIntermediateSign(const double StartX[4], const double CurrentX[4], double CurrentSign, double a)
{
    if (StartX[1] * CurrentX[1] < 0.0)
    {
        double t = StartX[1] / (StartX[1] - CurrentX[1]);
        double mix_x = StartX[0] + t * (CurrentX[0] - StartX[0]);
        double mix_z = StartX[2] + t * (CurrentX[2] - StartX[2]);
        double rho_cross = std::sqrt(mix_x * mix_x + mix_z * mix_z);
        if (rho_cross < std::abs(a))
        {
            return -CurrentSign;
        }
    }
    return CurrentSign;
}
const double EPS = 1e-16;
// Kerr-Schild 度规计算
void ComputeMetric(const double X[4], double a, double Q, double fade, double signR, bool isOut, double g_down[4][4], double g_up[4][4], double& r_out)
{
    double x = X[0], y = X[1], z = X[2];
    double a2 = a * a;
    double R2 = x * x + y * y + z * z;
    double u_val = R2 - a2;
    double v = 4.0 * a2 * y * y;

    double r2 = 0.0;
    if (u_val >= 0.0) r2 = 0.5 * (u_val + std::sqrt(u_val * u_val + v));
    else r2 = (2.0 * a2 * y * y) / std::fmax(1e-20, std::sqrt(u_val * u_val + v) - u_val);
    double r = signR * std::sqrt(std::fmax(r2, 0.0));
    r_out = r;
    double f = 0.0;
    if (std::abs(r) > 1e-6)
    {
        double num = 2.0 * 0.5 * r * r * r - Q * Q * r * r; // M=0.5
        double den = r * r * r * r + a2 * y * y;
        f = (num / std::fmax(1e-20, den)) * fade;
    }
    double dir = isOut ? -1.0 : 1.0;
    double inv = 1.0 / std::fmax(1e-20, r2 + a2);
    double lx = (dir * r * x - a * z) * inv;
    double ly = (dir * y) / r;
    double lz = (dir * r * z + a * x) * inv;
    double l[4] = { lx, ly, lz, -1.0 };
    double l_down_vec[4] = { lx, ly, lz, 1.0 };
    double eta_down[4] = { 1.0, 1.0, 1.0, -1.0 };
    double eta_up[4] = { 1.0, 1.0, 1.0, -1.0 };
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            g_down[i][j] = (i == j ? eta_down[i] : 0.0) + f * l_down_vec[i] * l_down_vec[j];
            g_up[i][j] = (i == j ? eta_up[i] : 0.0) - f * l[i] * l[j];
        }
    }
}
// 数值差分计算 Christoffel 符号
// 解析计算 Kerr-Schild 度规的克氏符 (无数值差分，极其稳定且高效)
void ComputeChristoffel(const double X[4], double a, double Q, double fade, double signR, bool isOut, double Gamma[4][4][4], double (*F_down)[4] = nullptr, double (*g_up_out)[4] = nullptr)
{
    double x = X[0], y = X[1], z = X[2];
    double a2 = a * a;
    double Q2 = Q * Q;
    double R2 = x * x + y * y + z * z;
    double u_val = R2 - a2;
    double v = 4.0 * a2 * y * y;

    // S 代表 sqrt(u^2 + v)，它是导数分母中的核心项。限制最小值防止环奇点除零。
    double S = std::fmax(1e-20, std::sqrt(u_val * u_val + v));

    double r2 = 0.0;
    if (u_val >= 0.0) r2 = 0.5 * (u_val + S);
    else r2 = (2.0 * a2 * y * y) / std::fmax(1e-20, S - u_val);

    double r = signR * std::sqrt(std::fmax(r2, 0.0));

    // 关键优化：引入 Y = y / r，利用代数恒等式严格避免 r->0 时的 0/0 奇点
    double Y = 0.0;
    if (std::abs(r) > 1e-10)
    {
        Y = y / r;
    }
    else
    {
        if (std::abs(a) > 1e-20)
        {
            double signY = (y >= 0.0) ? 1.0 : -1.0;
            Y = signY * signR * std::sqrt(std::fmax(0.0, r2 - u_val)) / std::abs(a);
        }
        else
        {
            Y = 0.0;
        }
    }

    // 1. 计算 r 的空间偏导数 \partial_k r (时间偏导为 0)
    double dr[4] = { 0.0 };
    dr[0] = (r * x) / S;
    dr[1] = (Y * (r2 + a2)) / S; // 原本是 y*(r^2+a^2)/(r*S)，用 Y 替换消去 0/0
    dr[2] = (r * z) / S;
    dr[3] = 0.0;

    // 2. 计算纯量 f 及其空间偏导数 \partial_k f
    double N = r * r * r - Q2 * r2; // 根据原代码 2*0.5*r^3，M默认=0.5，所以 2M=1.0
    double D = r2 * r2 + a2 * y * y;
    double D_inv = 1.0 / std::fmax(1e-20, D);
    double f = N * D_inv * fade;

    double df[4] = { 0.0 };
    for (int k = 0; k < 3; ++k)
    {
        double dN_k = (3.0 * r2 - 2.0 * Q2 * r) * dr[k];
        double dD_k = 4.0 * r * r2 * dr[k];
        if (k == 1) dD_k += 2.0 * a2 * y; // y 方向独有的偏导项
        df[k] = (dN_k * D - N * dD_k) * D_inv * D_inv * fade; // 假定 fade 在局部空间常数
    }

    // 3. 计算类光矢量 l_down 及其空间偏导数 \partial_k l_\mu
    double dir = isOut ? -1.0 : 1.0;
    double inv_r2a2 = 1.0 / std::fmax(1e-20, r2 + a2);

    double l_down[4];
    l_down[0] = (dir * r * x - a * z) * inv_r2a2;
    l_down[1] = dir * Y;
    l_down[2] = (dir * r * z + a * x) * inv_r2a2;
    l_down[3] = 1.0;

    double dl_down[3][4] = { 0.0 }; // dl_down[k][mu] 表示 \partial_k l_\mu
    for (int k = 0; k < 3; ++k)
    {
        double dinv = -inv_r2a2 * inv_r2a2 * 2.0 * r * dr[k];

        // \partial_k l_0
        double term0 = dir * x * dr[k];
        if (k == 0) term0 += dir * r;
        if (k == 2) term0 -= a;
        dl_down[k][0] = term0 * inv_r2a2 + (dir * r * x - a * z) * dinv;

        // \partial_k l_1 (高度化简：利用 Kerr-Schild 恒等式极大消去 0/0 奇点)
        if (k == 0) dl_down[k][1] = -dir * Y * x / S;
        else if (k == 1) dl_down[k][1] = dir * r * (1.0 - Y * Y) / S;
        else if (k == 2) dl_down[k][1] = -dir * Y * z / S;

        // \partial_k l_2
        double term2 = dir * z * dr[k];
        if (k == 2) term2 += dir * r;
        if (k == 0) term2 += a;
        dl_down[k][2] = term2 * inv_r2a2 + (dir * r * z + a * x) * dinv;

        // \partial_k l_3 (l_3 恒为 1.0)
        dl_down[k][3] = 0.0;
    }

    // 4. 重建逆度规 g_up (g^{\mu\nu})
    double eta_up[4] = { 1.0, 1.0, 1.0, -1.0 };
    double l_up[4] = { l_down[0], l_down[1], l_down[2], -1.0 };
    double g_up[4][4];
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            g_up[i][j] = (i == j ? eta_up[i] : 0.0) - f * l_up[i] * l_up[j];
            if (g_up_out) g_up_out[i][j] = g_up[i][j]; // [新增]：允许导出逆度规
        }
    }

    // 5. 计算度规偏导数 \partial_k g_{\mu\nu}
    // 注意：因时空平稳，\partial_t g_{\mu\nu} = dg_down[3][mu][nu] = 0.0
    double dg_down[4][4][4] = { 0.0 };
    for (int k = 0; k < 3; ++k)
    {
        for (int mu = 0; mu < 4; ++mu)
        {
            for (int nu = 0; nu < 4; ++nu)
            {
                // g_down = \eta + f * l_mu * l_nu
                dg_down[k][mu][nu] = df[k] * l_down[mu] * l_down[nu] +
                    f * dl_down[k][mu] * l_down[nu] +
                    f * l_down[mu] * dl_down[k][nu];
            }
        }
    }

    // 6. 组装 Christoffel 符号
    for (int lambda = 0; lambda < 4; ++lambda)
    {
        for (int mu = 0; mu < 4; ++mu)
        {
            for (int nu = 0; nu < 4; ++nu)
            {
                double sum = 0.0;
                for (int rho = 0; rho < 4; ++rho)
                {
                    sum += 0.5 * g_up[lambda][rho] * (dg_down[mu][rho][nu] + dg_down[nu][rho][mu] - dg_down[rho][mu][nu]);
                }
                Gamma[lambda][mu][nu] = sum;
            }
        }
    }
    // === [新增] 7. 顺风车计算 Kerr-Newman 电磁张量 (Faraday Tensor) ===
    // === [修改] 7. 顺风车计算 Kerr-Newman 电磁张量 (包含电荷与磁荷的线性叠加) ===
    if (F_down)
    {
        double F_unit_down[4][4];

        // 1. 先计算出 "单位电荷 (Q=1)" 对应的矢量势 P_unit
        double P_unit = -1.0 * r * r * r * D_inv * fade;
        double dP_unit[4] = { 0.0 };

        for (int k = 0; k < 3; ++k)
        {
            // \partial_k P_unit
            double dN_P_k = -1.0 * 3.0 * r * r * dr[k];
            double dD_k = 4.0 * r * r2 * dr[k];
            if (k == 1) dD_k += 2.0 * a2 * y;
            dP_unit[k] = (dN_P_k * D - (-1.0 * r * r * r) * dD_k) * D_inv * D_inv * fade;
        }

        for (int mu = 0; mu < 4; ++mu)
        {
            for (int nu = 0; nu < 4; ++nu)
            {
                // \partial_\mu A_\nu
                double d_mu_A_nu = 0.0;
                if (mu < 3) d_mu_A_nu = dP_unit[mu] * l_down[nu] + P_unit * dl_down[mu][nu];

                // \partial_\nu A_\mu
                double d_nu_A_mu = 0.0;
                if (nu < 3) d_nu_A_mu = dP_unit[nu] * l_down[mu] + P_unit * dl_down[nu][mu];

                // 单位电场张量: F^{(unit)}_{\mu\nu}
                F_unit_down[mu][nu] = d_mu_A_nu - d_nu_A_mu;
            }
        }

        // 2. 将单位电场张量升指标得到 F_unit^{\mu\nu} (为求 Hodge 对偶做准备)
        double F_unit_up[4][4] = { 0.0 };
        for (int mu = 0; mu < 4; ++mu)
        {
            for (int nu = 0; nu < 4; ++nu)
            {
                double sum = 0.0;
                for (int alpha = 0; alpha < 4; ++alpha)
                {
                    for (int beta = 0; beta < 4; ++beta)
                    {
                        sum += g_up[mu][alpha] * g_up[nu][beta] * F_unit_down[alpha][beta];
                    }
                }
                F_unit_up[mu][nu] = sum;
            }
        }

        // 3. 计算 Hodge 对偶，得到 "单位磁单极子场" star_F_unit
        // \star F_{\mu\nu} = 0.5 * \epsilon_{\mu\nu\rho\sigma} F_unit^{\rho\sigma}
        double star_F_unit_down[4][4];
        star_F_unit_down[0][1] = F_unit_up[2][3];
        star_F_unit_down[0][2] = -F_unit_up[1][3];
        star_F_unit_down[0][3] = F_unit_up[1][2];
        star_F_unit_down[1][2] = F_unit_up[0][3];
        star_F_unit_down[1][3] = -F_unit_up[0][2];
        star_F_unit_down[2][3] = F_unit_up[0][1];

        // 补全反对称矩阵的下三角与对角线
        star_F_unit_down[1][0] = -star_F_unit_down[0][1];
        star_F_unit_down[2][0] = -star_F_unit_down[0][2];
        star_F_unit_down[3][0] = -star_F_unit_down[0][3];
        star_F_unit_down[2][1] = -star_F_unit_down[1][2];
        star_F_unit_down[3][1] = -star_F_unit_down[1][3];
        star_F_unit_down[3][2] = -star_F_unit_down[2][3];

        for (int i = 0; i < 4; ++i) star_F_unit_down[i][i] = 0.0;

        // 4. 根据全局 Q 和 P 完美线性叠加，得到真正的电磁张量
        for (int mu = 0; mu < 4; ++mu)
        {
            for (int nu = 0; nu < 4; ++nu)
            {
                F_down[mu][nu] = g_Q * F_unit_down[mu][nu] + g_P * star_F_unit_down[mu][nu];
            }
        }
    }
}
void EvaluateDerivatives(const double Y[20], double a, double Q, double fade, double signR, bool isOut, double dY[20])
{
    double Gamma[4][4][4];
    double F_down[4][4]; // [新增]
    double g_up[4][4];   // [新增]
    // 传入指针以索取附加几何量
    ComputeChristoffel(Y, a, Q, fade, signR, isOut, Gamma, F_down, g_up);

    for (int i = 0; i < 4; ++i) dY[i] = Y[4 + i]; // dx/dtau = u

    // === [新增] 计算洛伦兹加速度 a_{Lorentz}^\mu = (q/m) F^\mu_{~~\nu} U^\nu ===
    double a_Lorentz_up[4] = { 0.0 };
    double a_Lorentz_down[4] = { 0.0 };
    if (g_CameraCharge != 0.0)
    {
        for (int mu = 0; mu < 4; ++mu)
            for (int nu = 0; nu < 4; ++nu)
                a_Lorentz_down[mu] += g_CameraCharge * F_down[mu][nu] * Y[4 + nu];

        for (int mu = 0; mu < 4; ++mu)
            for (int nu = 0; nu < 4; ++nu)
                a_Lorentz_up[mu] += g_up[mu][nu] * a_Lorentz_down[nu];
    }

    for (int i = 0; i < 4; ++i)
    {
        double sum = 0;
        for (int mu = 0; mu < 4; ++mu)
            for (int nu = 0; nu < 4; ++nu)
                sum -= Gamma[i][mu][nu] * Y[4 + mu] * Y[4 + nu];

        // === [修改]：同时应用引擎飞船推力(火箭) 和 洛伦兹力 ===
        sum += g_ProperAcceleration[0] * Y[8 + i] +
            g_ProperAcceleration[1] * Y[12 + i] +
            g_ProperAcceleration[2] * Y[16 + i] +
            a_Lorentz_up[i];
        dY[4 + i] = sum;
    }

    for (int a_idx = 0; a_idx < 3; ++a_idx)
    { // 平移输运方程
        int offset = 8 + 4 * a_idx;

        // === [新增]：广义 Fermi-Walker Transport 约束量 a \cdot E_{(a_idx)} ===
        double a_dot_E = g_ProperAcceleration[a_idx]; // 推力在局部标架的天然投影
        if (g_CameraCharge != 0.0)
        {
            // 加上洛伦兹力的标架投影部分
            for (int mu = 0; mu < 4; ++mu)
                a_dot_E += a_Lorentz_down[mu] * Y[offset + mu];
        }

        for (int i = 0; i < 4; ++i)
        {
            double sum = 0;
            for (int mu = 0; mu < 4; ++mu)
                for (int nu = 0; nu < 4; ++nu)
                    sum -= Gamma[i][mu][nu] * Y[4 + mu] * Y[offset + nu];

            // === [修改]：Fermi-Walker Transport 确保相机在任何加速度下绝对刚性非旋转正交 ===
            sum += Y[4 + i] * a_dot_E;
            dY[offset + i] = sum;
        }
    }
}
void GramSchmidt(double Y[20], double a, double Q, double fade, double signR, bool isOut)
{
    double g_down[4][4], g_up[4][4], dummy_r;
    ComputeMetric(Y, a, Q, fade, signR, isOut, g_down, g_up, dummy_r);
    auto dotP = [&](const double* v1, const double* v2)
    {
        double sum = 0;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) sum += g_down[i][j] * v1[i] * v2[j];
        return sum;
    };

    // 约束2：严格归一化四维速度 U (必须是类时矢量，U.U 应该为 -1)
    double normU2 = dotP(Y + 4, Y + 4);
    double factorU = 1.0 / std::sqrt(std::fmax(1e-12, std::abs(normU2)));
    for (int i = 0; i < 4; ++i) Y[4 + i] *= factorU;

    // 再次计算归一化后的 U.U (避免浮点误差累积，获取精确负值用于投影除法)
    double exact_normU2 = dotP(Y + 4, Y + 4);
    for (int a_idx = 0; a_idx < 3; ++a_idx)
    {
        double* E = Y + 8 + 4 * a_idx;

        // 约束1：空间基底必须严格与 U 正交。数学公式: E' = E - (E.U / U.U) * U
        double dotEU = dotP(E, Y + 4);
        double projU = dotEU / std::min(-1e-12, exact_normU2); // exact_normU2 是负数
        for (int i = 0; i < 4; ++i) E[i] -= projU * Y[4 + i];

        // 约束3：3个空间基底之间必须严格两两正交。
        for (int b_idx = 0; b_idx < a_idx; ++b_idx)
        {
            double* Eb = Y + 8 + 4 * b_idx;
            double dotEEb = dotP(E, Eb);
            double normEb2 = dotP(Eb, Eb); // Eb 是类空矢量，理论上是正数
            // E' = E - (E.Eb / Eb.Eb) * Eb
            double projEb = dotEEb / std::fmax(1e-12, normEb2);
            for (int i = 0; i < 4; ++i) E[i] -= projEb * Eb[i];
        }

        // 约束2（续）：严格归一化空间基矢 (必须是类空矢量，E.E 应该为 +1)
        double normE2 = dotP(E, E);
        double factorE = 1.0 / std::sqrt(std::fmax(1e-12, std::abs(normE2)));
        for (int i = 0; i < 4; ++i) E[i] *= factorE;
    }
}
// 与 GLSL 严格一致的换系函数
void TransformKS(double X[4], double P[4], double signR, double M, double a, double Q, bool out_to_in)
{
    double x = X[0], y = X[1], z = X[2], t = X[3];
    double px = P[0], py = P[1], pz = P[2], pt = P[3];
    double a2 = a * a, M2 = M * M, Q2 = Q * Q;
    double R2 = x * x + y * y + z * z;
    double u = R2 - a2;
    double v = 4.0 * a2 * y * y;
    double r2 = (u >= 0.0) ? 0.5 * (u + std::sqrt(u * u + v)) : 0.5 * v / std::fmax(1e-20, std::sqrt(u * u + v) - u);
    double r = signR * std::sqrt(std::fmax(r2, 0.0));
    double Delta = r * r - 2.0 * M * r + a2 + Q2;
    double safe_Delta = (Delta >= 0 ? 1 : -1) * std::fmax(std::abs(Delta), EPS);
    double D = r * r * r * r + a2 * y * y;
    double safe_D = std::fmax(D, 1e-12);
    double grad_r[3] = { (r * r * r * x) / safe_D, (r * (r * r + a2) * y) / safe_D, (r * r * r * z) / safe_D };
    double delta_disc = M2 - a2 - Q2;
    double F_r = 0.0, g_r = 0.0;
    double abs_Delta_safe = std::fmax(std::abs(Delta), EPS);
    if (delta_disc > EPS)
    {
        double K = std::sqrt(delta_disc);
        double frac = std::abs(r - (M + K)) / std::fmax(std::abs(r - (M - K)), EPS);
        F_r = 2.0 * M * std::log(abs_Delta_safe) + ((2.0 * M2 - Q2) / K) * std::log(std::fmax(frac, EPS));
        g_r = (a / K) * std::log(std::fmax(frac, EPS));
    }
    else if (delta_disc < -EPS)
    {
        double K = std::sqrt(-delta_disc);
        double atan_arg = std::atan((r - M) / K);
        F_r = 2.0 * M * std::log(abs_Delta_safe) + (2.0 * (2.0 * M2 - Q2) / K) * atan_arg;
        g_r = (2.0 * a / K) * atan_arg;
    }
    else
    {
        double rM = r - M;
        double safe_rM = (rM >= 0 ? 1 : -1) * std::fmax(std::abs(rM), EPS);
        F_r = 4.0 * M * std::log(std::fmax(std::abs(rM), EPS)) - 2.0 * (2.0 * M2 - Q2) / safe_rM;
        g_r = -2.0 * a / safe_rM;
    }
    g_r += 2.0 * std::atan2(a, r);
    double F_prime = 2.0 * (2.0 * M * r - Q2) / safe_Delta;
    double g_prime = 2.0 * a / safe_Delta - 2.0 * a / (r * r + a * a);
    double Ly = z * px - x * pz;
    double K_p = F_prime * pt + g_prime * Ly;
    double dir = out_to_in ? -1.0 : 1.0;

    double angle = -dir * g_r;
    double time_shift = -dir * F_r;
    double P_tilde[3] = { px + dir * grad_r[0] * K_p, py + dir * grad_r[1] * K_p, pz + dir * grad_r[2] * K_p };
    double cos_a = std::cos(angle), sin_a = std::sin(angle);
    X[0] = x * cos_a + z * sin_a;
    X[1] = y;
    X[2] = z * cos_a - x * sin_a;
    X[3] = t + time_shift;
    P[0] = P_tilde[2] * sin_a + P_tilde[0] * cos_a;
    P[1] = P_tilde[1];
    P[2] = -P_tilde[0] * sin_a + P_tilde[2] * cos_a;
    P[3] = pt;
}
// 向量指标升降
void ChangeIndex(double V[4], const double g[4][4])
{
    double out[4] = { 0 };
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) out[i] += g[i][j] * V[j];
    for (int i = 0; i < 4; ++i) V[i] = out[i];
}
void CheckAndSwitchCoords(double Y[20], double a, double Q, double fade, double& signR, bool& isOut)
{
    double g_down[4][4], g_up[4][4], dummy_r;
    ComputeMetric(Y, a, Q, fade, signR, isOut, g_down, g_up, dummy_r);

    // 计算当前系下的速度分量之和，用于发散判定
    double current_Sum = 0;
    for (int i = 0; i < 4; ++i) current_Sum += std::abs(Y[4 + i]);

    double Y_test[20];
    for (int i = 0; i < 20; ++i) Y_test[i] = Y[i];

    // 1. 将所有向量降指标 (使用原坐标系的 g_down)
    ChangeIndex(Y_test + 4, g_down);
    for (int k = 0; k < 3; ++k)
    {
        ChangeIndex(Y_test + 8 + 4 * k, g_down);
    }

    // 2. 提前保存原坐标的完美副本
    double X_orig[4] = { Y_test[0], Y_test[1], Y_test[2], Y_test[3] };

    // 3. 变换时空坐标 X 本身和四维速度 U 
    // 这一步执行后，Y_test[0..3] 会被正确更新为目标系的新坐标
    TransformKS(Y_test, Y_test + 4, signR, 0.5, a, Q, isOut);

    // 4. 变换空间标架 E1, E2, E3
    // 必须每次喂入原坐标副本 dummyX，确保 Jacobian 矩阵的计算依赖的是原系坐标
    for (int k = 0; k < 3; ++k)
    {
        double dummyX[4] = { X_orig[0], X_orig[1], X_orig[2], X_orig[3] };
        TransformKS(dummyX, Y_test + 8 + 4 * k, signR, 0.5, a, Q, isOut);
    }

    // 5. 计算目标系度规并升指标
    double test_g_down[4][4], test_g_up[4][4];
    ComputeMetric(Y_test, a, Q, fade, signR, !isOut, test_g_down, test_g_up, dummy_r);

    ChangeIndex(Y_test + 4, test_g_up);
    for (int k = 0; k < 3; ++k)
    {
        ChangeIndex(Y_test + 8 + 4 * k, test_g_up);
    }

    // 6. 检验目标系是否更加平滑 (发散抑制)
    double test_Sum = 0;
    for (int i = 0; i < 4; ++i) test_Sum += std::abs(Y_test[4 + i]);

    if (current_Sum > 2.0 * test_Sum)
    {
        // 确实更加平滑，确认换系，将 test 状态覆盖回真实状态 Y
        for (int i = 0; i < 20; ++i) Y[i] = Y_test[i];
        isOut = !isOut;
    }
}
void InitializeGeodesicState(glm::vec3 pos, glm::vec3 vel, double a, double Q)
{
    g_GeoState[0] = pos.x; g_GeoState[1] = pos.y; g_GeoState[2] = pos.z; g_GeoState[3] = 0.0;

    double g_down[4][4] = { 0.0 }, g_up[4][4] = {0.0}, r_out=0.0;
    // 使用当前坐标求出度规张量
    ComputeMetric(g_GeoState, a, Q, 1.0, g_UniverseSign, g_isOutgoing, g_down, g_up, r_out);

    // ========================================================
    // 将传入的三维坐标速度(v/c)转换为满足归一化条件的四维速度 U^mu
    // ========================================================
    double v_coord[4] = { vel.x, vel.y, vel.z, 1.0 };
    double V_sq = 0.0;

    // 计算模长平方： V^2 = g_ij * v^i * v^j + 2 * g_it * v^i + g_tt
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            V_sq += g_down[i][j] * v_coord[i] * v_coord[j];
        }
    }

    if (V_sq < -1e-6)
    {
        // 合法的类时速度 (未超光速)
        double Ut = 1.0 / std::sqrt(-V_sq);
        g_GeoState[4] = vel.x * Ut;
        g_GeoState[5] = vel.y * Ut;
        g_GeoState[6] = vel.z * Ut;
        g_GeoState[7] = Ut;
    }
    else
    {
        // 如果相机移动超光速，或处于能层内无法保持静止，尝试回退
        if (g_down[3][3] < -1e-6)
        {
            // 回退到静态观者
            g_GeoState[4] = 0; g_GeoState[5] = 0; g_GeoState[6] = 0;
            g_GeoState[7] = 1.0 / std::sqrt(-g_down[3][3]);
        }
        else
        {
            // 如果连静态观者都做不到（例如在能层内），强制给一个向内的下落速度以防崩溃
            double inward_v[4] = { -pos.x * 0.1, -pos.y * 0.1, -pos.z * 0.1, 1.0 };
            double dV_sq = 0;
            for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) dV_sq += g_down[i][j] * inward_v[i] * inward_v[j];

            if (dV_sq < 0.0)
            {
                double Ut = 1.0 / std::sqrt(-dV_sq);
                g_GeoState[4] = inward_v[0] * Ut; g_GeoState[5] = inward_v[1] * Ut;
                g_GeoState[6] = inward_v[2] * Ut; g_GeoState[7] = Ut;
            }
            else
            {
                g_GeoState[4] = 0; g_GeoState[5] = 0; g_GeoState[6] = 0; g_GeoState[7] = 1.0;
            }
        }
    }

    // 计算四维速度的协变形式 (用于投影计算)
    double U_down[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            U_down[i] += g_down[i][j] * g_GeoState[4 + j];

    // ========================================================
    // 构建与 GLSL 完全一致的空间标架 e1, e2, e3
    // 使用 GLSL 中相同的球面映射切线向量进行正交化
    // ========================================================
    glm::vec3 m_r = -glm::normalize(pos);
    glm::vec3 WorldUp = glm::vec3(0.0, 1.0, 0.0);
    if (std::abs(glm::dot(m_r, WorldUp)) > 0.999f)
    {
        WorldUp = glm::vec3(1.0, 0.0, 0.0);
    }
    glm::vec3 m_phi = glm::normalize(glm::cross(WorldUp, m_r));
    glm::vec3 m_theta = glm::cross(m_phi, m_r);

    // 共用的空间正交化闭包
    auto projectAndNormalize = [&](double* e, double* e_d)
    {
        // e = e + dot(e, U_down) * U_up
        double dot_e_U = 0.0;
        for (int i = 0; i < 4; ++i) dot_e_U += e[i] * U_down[i];
        for (int i = 0; i < 4; ++i) e[i] += dot_e_U * g_GeoState[4 + i];

        // 降指标
        for (int i = 0; i < 4; ++i)
        {
            e_d[i] = 0.0;
            for (int j = 0; j < 4; ++j) e_d[i] += g_down[i][j] * e[j];
        }

        // n = sqrt(max(1e-9, dot(e, e_d)))
        double norm = 0.0;
        for (int i = 0; i < 4; ++i) norm += e[i] * e_d[i];
        norm = std::sqrt(std::fmax(1e-9, norm));
        for (int i = 0; i < 4; ++i)
        {
            e[i] /= norm;
            e_d[i] /= norm;
        }
    };

    double e1[4] = { m_r.x, m_r.y, m_r.z, 0.0 };
    double e1_d[4];
    projectAndNormalize(e1, e1_d);

    double e2[4] = { m_theta.x, m_theta.y, m_theta.z, 0.0 };
    double dot_e2_U = 0.0;
    for (int i = 0; i < 4; ++i) dot_e2_U += e2[i] * U_down[i];
    for (int i = 0; i < 4; ++i) e2[i] += dot_e2_U * g_GeoState[4 + i];

    double dot_e2_e1 = 0.0;
    for (int i = 0; i < 4; ++i) dot_e2_e1 += e2[i] * e1_d[i];
    for (int i = 0; i < 4; ++i) e2[i] -= dot_e2_e1 * e1[i];

    double e2_d[4];
    projectAndNormalize(e2, e2_d);

    double e3[4] = { m_phi.x, m_phi.y, m_phi.z, 0.0 };
    double dot_e3_U = 0.0;
    for (int i = 0; i < 4; ++i) dot_e3_U += e3[i] * U_down[i];
    for (int i = 0; i < 4; ++i) e3[i] += dot_e3_U * g_GeoState[4 + i];

    double dot_e3_e1 = 0.0;
    for (int i = 0; i < 4; ++i) dot_e3_e1 += e3[i] * e1_d[i];
    for (int i = 0; i < 4; ++i) e3[i] -= dot_e3_e1 * e1[i];

    double dot_e3_e2 = 0.0;
    for (int i = 0; i < 4; ++i) dot_e3_e2 += e3[i] * e2_d[i];
    for (int i = 0; i < 4; ++i) e3[i] -= dot_e3_e2 * e2[i];

    double e3_d[4];
    projectAndNormalize(e3, e3_d);

    // ========================================================
    // 将 m_r, m_theta, m_phi 的物理标架逆投影回 X, Y, Z 三维笛卡尔基底
    // 在主循环中计算头偏时，C++使用了 -= 减去这组基底。
    // 这将完美等价于 GLSL 中：P_up = U_up - (k_r * e1 + k_theta * e2 + k_phi * e3)
    // 从而消灭模式切换产生的光行差与洛伦兹偏转跳变。
    // ========================================================
    for (int i = 0; i < 4; ++i)
    {
        g_GeoState[8 + i] = m_r.x * e1[i] + m_theta.x * e2[i] + m_phi.x * e3[i]; // E_x
        g_GeoState[12 + i] = m_r.y * e1[i] + m_theta.y * e2[i] + m_phi.y * e3[i]; // E_y
        g_GeoState[16 + i] = m_r.z * e1[i] + m_theta.z * e2[i] + m_phi.z * e3[i]; // E_z
    }
    GramSchmidt(g_GeoState, a, Q, 1.0, g_UniverseSign, g_isOutgoing);
}
void StepRK4(double dtau, double a, double Q)
{
    double fade = 1.0;
    CheckAndSwitchCoords(g_GeoState, a, Q, fade, g_UniverseSign, g_isOutgoing);
    double k1[20], k2[20], k3[20], k4[20], Y_temp[20];

    EvaluateDerivatives(g_GeoState, a, Q, fade, g_UniverseSign, g_isOutgoing, k1);

    for (int i = 0; i < 20; ++i) Y_temp[i] = g_GeoState[i] + 0.5 * dtau * k1[i];
    double sign2 = GetIntermediateSign(g_GeoState, Y_temp, g_UniverseSign, a);
    EvaluateDerivatives(Y_temp, a, Q, fade, sign2, g_isOutgoing, k2);

    for (int i = 0; i < 20; ++i) Y_temp[i] = g_GeoState[i] + 0.5 * dtau * k2[i];
    double sign3 = GetIntermediateSign(g_GeoState, Y_temp, g_UniverseSign, a);
    EvaluateDerivatives(Y_temp, a, Q, fade, sign3, g_isOutgoing, k3);

    for (int i = 0; i < 20; ++i) Y_temp[i] = g_GeoState[i] + dtau * k3[i];
    double sign4 = GetIntermediateSign(g_GeoState, Y_temp, g_UniverseSign, a);
    EvaluateDerivatives(Y_temp, a, Q, fade, sign4, g_isOutgoing, k4);

    double oldX[4] = { g_GeoState[0], g_GeoState[1], g_GeoState[2], g_GeoState[3] };
    for (int i = 0; i < 20; ++i) g_GeoState[i] += (dtau / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);

    // 更新总宇宙标记
    g_UniverseSign = GetIntermediateSign(oldX, g_GeoState, g_UniverseSign, a);

    GramSchmidt(g_GeoState, a, Q, fade, g_UniverseSign, g_isOutgoing);
}
}










FMatrices Matrices;
FLightMaterial LightMaterial;
float cfov = 80.0f;
float camsmth = 1.0f;
float qfm = 0.0f;
_NPGS_BEGIN

namespace Art = Runtime::Asset;
namespace Grt = Runtime::Graphics;
namespace SysSpa = System::Spatial;
namespace UI = Npgs::System::UI;

double get_orbit_energy(double x, double a, double q2)
{
    if (x < q2) return 1e100; // 低于理论极限，返回极大罚值防止越界

    // sqrt(x - q^2) 是广义相对论测地线方程中提取出的公共项
    double sq = std::sqrt(std::fmax(0.0, x - q2));

    // 分母内部的判别式，其最大根对应光子球 (Photon Sphere)
    double F = x * x - 3.0 * x + 2.0 * q2 + 2.0 * a * sq;

    // 如果处在光子球内部或边界，轨道不稳定或不存在，赋予极大的能量值
    if (F <= 1e-15) return 1e100;

    // 分子部分
    double num = x * x - 2.0 * x + q2 + a * sq;

    // 完整的圆轨道能量公式
    return num / (x * std::sqrt(F));
}

/**
 * @brief 核心函数：计算 Kerr-Newman 黑洞的最内稳定圆轨道 (ISCO) 半径
 *
 * @param M 黑洞质量
 * @param a_star 无量纲自旋参数 a/M (绝对值 <= 1, 带符号表示顺/逆行)
 * @param Q_star 无量纲电荷参数 Q/M
 * @return double ISCO 半径
 */
double calculate_KN_ISCO(double M, double a_star, double Q_star)
{
    double a = a_star;
    double q2 = Q_star * Q_star;

    // 1. 物理有效性检查：必须满足 a^2 + Q^2 <= M^2
    if (a * a + q2 > 1.0 + 1e-9)
    {
        std::cerr << "错误: 参数不构成黑洞 (a^2 + Q^2 > M^2)，为裸奇点。" << std::endl;
        return -1.0;
    }

    // 2. 特殊情况拦截：极端 Kerr 顺行轨道
    // 在极端Kerr下，ISCO 刚好位于视界 (x=1)，此时 E(x) 出现 0/0 奇点。
    // 在此直接返回已知的解析极限结果。
    if (abs(a - 1.0) < 1e-7 && q2 < 1e-7)
    {
        return M * 1.0;
    }

    // 3. 黄金分割搜索求极小值点 (ISCO 对应 E(x) 曲线的最低点)
    double left = std::fmax(1.0, q2) + 1e-5;
    double right = 15.0; // ISCO 理论最大值为 9M (逆行极端Kerr)，设 15 为安全上限

    const double invphi = (std::sqrt(5.0) - 1.0) / 2.0;
    const double invphi2 = (3.0 - std::sqrt(5.0)) / 2.0;

    double c = left + invphi2 * (right - left);
    double d = left + invphi * (right - left);

    double fc = get_orbit_energy(c, a, q2);
    double fd = get_orbit_energy(d, a, q2);

    double tol = 1e-11; // 极高精度容差

    while ((right - left) > tol)
    {
        if (fc < fd)
        {
            right = d;
            d = c;
            fd = fc;
            c = left + invphi2 * (right - left);
            fc = get_orbit_energy(c, a, q2);
        }
        else
        {
            left = c;
            c = d;
            fc = fd;
            d = left + invphi * (right - left);
            fd = get_orbit_energy(d, a, q2);
        }
    }

    // 返回最低点对应的半径
    double x_isco = 0.5 * (left + right);
    return x_isco * M;
}

FApplication::FApplication(const vk::Extent2D& WindowSize, const std::string& WindowTitle,
    bool bEnableVSync, bool bEnableFullscreen)
    :
    _VulkanContext(Grt::FVulkanContext::GetClassInstance()),
    _WindowTitle(WindowTitle),
    _WindowSize(WindowSize),
    _bEnableVSync(bEnableVSync),
    _bEnableFullscreen(bEnableFullscreen),
    m_beam_energy("1.919 E+30"),
    m_rkkv_mass("5.14 E+13"),
    m_is_beam_button_active(false),
    m_is_rkkv_button_active(false)
{
    if (!InitializeWindow())
    {
        NpgsCoreError("Failed to create application.");
    }
}

FApplication::~FApplication()
{
    System::I18nManager::Get().UnregisterCallback(this);
}
void FApplication::Quit()
{
    if (_Window)
    {
        glfwSetWindowShouldClose(_Window, GLFW_TRUE);
    }
}
void FApplication::Command(const std::string& command)
{
    if (command.find("/tp ") == 0)
    {
        std::istringstream iss(command.substr(4));
        double x, y, z, t, vx, vy, vz;
        // 解析形如 "/tp x y z t vx vy vz" 的字符串
        if (iss >> x >> y >> z >> t >> vx >> vy >> vz)
        {
            if (g_GeodesicMode)
            {
                // 1. 设置坐标位置与时间 (X^\mu)
                g_GeoState[0] = x;
                g_GeoState[1] = y;
                g_GeoState[2] = z;
                g_GeoState[3] = t;
                // 2. 设置空间速度分量 (U^x, U^y, U^z)
                g_GeoState[4] = vx;
                g_GeoState[5] = vy;
                g_GeoState[6] = vz;
                double a = BlackHoleArgs.Spin * 0.5; // M = 0.5
                double Q = sqrt(pow(g_Q, 2.0) + pow(g_P, 2.0)) * 0.5;

                // 强制要求解释为 ingoing kerrschild 系
                g_isOutgoing = false;
                // 3. 计算度规张量以求出 U^t
                double g_down[4][4], g_up[4][4], dummy_r;
                GeodesicIntegrator::ComputeMetric(g_GeoState, a, Q, 1.0, g_UniverseSign, g_isOutgoing, g_down, g_up, dummy_r);
                // 解二次方程 g_{\mu\nu} U^\mu U^\nu = -1 求解时间分量 Ut (即 g_GeoState[7])
                // A * Ut^2 + B * Ut + C = 0
                double A = g_down[3][3];
                double B = 2.0 * (g_down[3][0] * vx + g_down[3][1] * vy + g_down[3][2] * vz);
                double C = 1.0;
                for (int i = 0; i < 3; ++i)
                {
                    for (int j = 0; j < 3; ++j)
                    {
                        double ui = (i == 0) ? vx : (i == 1) ? vy : vz;
                        double uj = (j == 0) ? vx : (j == 1) ? vy : vz;
                        C += g_down[i][j] * ui * uj;
                    }
                }
                double discriminant = B * B - 4.0 * A * C;
                double Ut = 1.0;
                if (discriminant >= 0.0)
                {
                    double root1 = (-B - std::sqrt(discriminant)) / (2.0 * A);
                    double root2 = (-B + std::sqrt(discriminant)) / (2.0 * A);
                    // 判断哪个解对应的是指向未来的类时矢量 (要求协变能量分量 E = -U_t > 0 => U_t < 0)
                    auto checkEnergy = [&](double ut_val)
                    {
                        return g_down[3][0] * vx + g_down[3][1] * vy + g_down[3][2] * vz + g_down[3][3] * ut_val;
                    };
                    double E1 = checkEnergy(root1);
                    double E2 = checkEnergy(root2);
                    if (E1 < 0.0 && E2 >= 0.0) Ut = root1;
                    else if (E2 < 0.0 && E1 >= 0.0) Ut = root2;
                    else Ut = std::fmax(root1, root2); // Fallback
                }
                else
                {
                    // 若给定速度非类时 (如超光速)，退化取极值
                    if (std::abs(A) > 1e-10) Ut = -B / (2.0 * A);
                    else Ut = 1.0;
                }
                g_GeoState[7] = Ut;
                // 4. 重建与之完全正交的相机空间标架 e1, e2, e3
                glm::vec3 pos(x, y, z);
                if (glm::length(pos) < 1e-6) pos = glm::vec3(1.0, 0.0, 0.0);

                glm::vec3 m_r = -glm::normalize(pos);
                glm::vec3 WorldUp = glm::vec3(0.0, 1.0, 0.0);
                if (std::abs(glm::dot(m_r, WorldUp)) > 0.999f)
                {
                    WorldUp = glm::vec3(1.0, 0.0, 0.0);
                }
                glm::vec3 m_phi = glm::normalize(glm::cross(WorldUp, m_r));
                glm::vec3 m_theta = glm::cross(m_phi, m_r);
                double U_down[4] = { 0, 0, 0, 0 };
                for (int i = 0; i < 4; ++i)
                    for (int j = 0; j < 4; ++j)
                        U_down[i] += g_down[i][j] * g_GeoState[4 + j];
                auto projectAndNormalize = [&](double* e, double* e_d)
                {
                    double dot_e_U = 0.0;
                    for (int i = 0; i < 4; ++i) dot_e_U += e[i] * U_down[i];
                    for (int i = 0; i < 4; ++i) e[i] += dot_e_U * g_GeoState[4 + i];
                    for (int i = 0; i < 4; ++i)
                    {
                        e_d[i] = 0.0;
                        for (int j = 0; j < 4; ++j) e_d[i] += g_down[i][j] * e[j];
                    }
                    double norm = 0.0;
                    for (int i = 0; i < 4; ++i) norm += e[i] * e_d[i];
                    norm = std::sqrt(std::fmax(1e-9, norm));
                    for (int i = 0; i < 4; ++i)
                    {
                        e[i] /= norm;
                        e_d[i] /= norm;
                    }
                };
                double e1[4] = { m_r.x, m_r.y, m_r.z, 0.0 };
                double e1_d[4];
                projectAndNormalize(e1, e1_d);
                double e2[4] = { m_theta.x, m_theta.y, m_theta.z, 0.0 };
                double dot_e2_U = 0.0;
                for (int i = 0; i < 4; ++i) dot_e2_U += e2[i] * U_down[i];
                for (int i = 0; i < 4; ++i) e2[i] += dot_e2_U * g_GeoState[4 + i];
                double dot_e2_e1 = 0.0;
                for (int i = 0; i < 4; ++i) dot_e2_e1 += e2[i] * e1_d[i];
                for (int i = 0; i < 4; ++i) e2[i] -= dot_e2_e1 * e1[i];
                double e2_d[4];
                projectAndNormalize(e2, e2_d);
                double e3[4] = { m_phi.x, m_phi.y, m_phi.z, 0.0 };
                double dot_e3_U = 0.0;
                for (int i = 0; i < 4; ++i) dot_e3_U += e3[i] * U_down[i];
                for (int i = 0; i < 4; ++i) e3[i] += dot_e3_U * g_GeoState[4 + i];
                double dot_e3_e1 = 0.0;
                for (int i = 0; i < 4; ++i) dot_e3_e1 += e3[i] * e1_d[i];
                for (int i = 0; i < 4; ++i) e3[i] -= dot_e3_e1 * e1[i];
                double dot_e3_e2 = 0.0;
                for (int i = 0; i < 4; ++i) dot_e3_e2 += e3[i] * e2_d[i];
                for (int i = 0; i < 4; ++i) e3[i] -= dot_e3_e2 * e2[i];
                double e3_d[4];
                projectAndNormalize(e3, e3_d);
                for (int i = 0; i < 4; ++i)
                {
                    g_GeoState[8 + i] = m_r.x * e1[i] + m_theta.x * e2[i] + m_phi.x * e3[i];
                    g_GeoState[12 + i] = m_r.y * e1[i] + m_theta.y * e2[i] + m_phi.y * e3[i];
                    g_GeoState[16 + i] = m_r.z * e1[i] + m_theta.z * e2[i] + m_phi.z * e3[i];
                }

                // 5. 使用 Gram-Schmidt 过程进行最后的精细化数值稳定修正
                GeodesicIntegrator::GramSchmidt(g_GeoState, a, Q, 1.0, g_UniverseSign, g_isOutgoing);
            }
        }
    }
}
void FApplication::ExecuteMainRender()
{    // 初始化 UI 渲染器
    _uiRenderer = std::make_unique<Grt::FVulkanUIRenderer>();
    // =========================================================================
    // [修改] UI 初始化逻辑
    // =========================================================================

    if (!_uiRenderer->Initialize(_Window))
    {
        NpgsCoreError("Failed to initialize UI renderer");
        return;
    }
    // using namespace Npgs::System::UI::;



     // =========================================================================
    std::unique_ptr<Grt::FColorAttachment> HistoryAttachment;
    // [新增] 预计算阶段的附件
    std::unique_ptr<Grt::FColorAttachment> DistortionAttachment; // 对应 OutDistortionFlag (Set 1, Binding 3)
    std::unique_ptr<Grt::FColorAttachment> VolumetricAttachment; // 对应 OutVolumetric (Set 1, Binding 4)
    // [修改] BlackHoleAttachment 现在作为合成后的输出
    std::unique_ptr<Grt::FColorAttachment> BlackHoleAttachment;
    std::unique_ptr<Grt::FColorAttachment> PreBloomAttachment;
    std::unique_ptr<Grt::FColorAttachment> GaussBlurAttachment;
    // [新增] 用于存储当前帧无UI的纯净画面 (Full Size)
    std::unique_ptr<Grt::FColorAttachment> SceneColorAttachment;
    // [修改] 我们需要两个大小相同的中间纹理来进行 Ping-Pong
// 它们的大小应该是我们模糊链中最大的尺寸，即 1/2 分辨率
    std::unique_ptr<Grt::FColorAttachment> UIBlurPingAttachment;
    std::unique_ptr<Grt::FColorAttachment> UIBlurPongAttachment;
    // [新增] 用于 UI 背景模糊的最终图 (Half Size)
    std::unique_ptr<Grt::FColorAttachment> UIBlurAttachment;


    vk::RenderingAttachmentInfo DistortionAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f})));

    // Prepass 输出 1
    vk::RenderingAttachmentInfo VolumetricAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f})));

    // Composite 输出
    vk::RenderingAttachmentInfo BlackHoleAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingAttachmentInfo HistoryAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingAttachmentInfo PreBloomAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingAttachmentInfo GaussBlurAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);
    vk::RenderingAttachmentInfo SceneColorAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::Extent2D HalfWindowSize = { _WindowSize.width / 2, _WindowSize.height / 2 };
    if (HalfWindowSize.width == 0) HalfWindowSize.width = 1;
    if (HalfWindowSize.height == 0) HalfWindowSize.height = 1;

    auto CreateFramebuffers = [&]() -> void
    {
        _VulkanContext->WaitIdle();

        vk::Extent2D HalfWindowSize = { _WindowSize.width / 2, _WindowSize.height / 2 };
        if (HalfWindowSize.width == 0) HalfWindowSize.width = 1;
        if (HalfWindowSize.height == 0) HalfWindowSize.height = 1;

        // 1. History (保持全分辨率)
        HistoryAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR16G16B16A16Sfloat, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);

        // 2. Prepass Attachments (修改为 HalfWindowSize)
        // Distortion: 即使是半分辨率，RG32F 也能保证向量精度
        DistortionAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR32G32B32A32Sfloat, HalfWindowSize, 1, vk::SampleCountFlagBits::e1, // <--- 使用 HalfWindowSize
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);

        // Volumetric: 半分辨率
        VolumetricAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR16G16B16A16Sfloat, HalfWindowSize, 1, vk::SampleCountFlagBits::e1, // <--- 使用 HalfWindowSize
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);

        // 3. Composite Output (保持全分辨率)
        BlackHoleAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR16G16B16A16Sfloat, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment);

        PreBloomAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR16G16B16A16Sfloat, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);

        GaussBlurAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR16G16B16A16Sfloat, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);


        SceneColorAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR8G8B8A8Unorm, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst
        );

        // [修改] 创建用于 Ping-Pong 的纹理 (Half Size)
        vk::Extent2D halfSize = { _WindowSize.width / 2, _WindowSize.height / 2 };
        if (halfSize.width == 0) halfSize.width = 1;
        if (halfSize.height == 0) halfSize.height = 1;

        vk::ImageUsageFlags blurUsageFlags = vk::ImageUsageFlagBits::eSampled |
            vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eTransferDst;

        UIBlurPingAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR8G8B8A8Unorm, halfSize, 1, vk::SampleCountFlagBits::e1, blurUsageFlags
        );
        UIBlurPongAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR8G8B8A8Unorm, halfSize, 1, vk::SampleCountFlagBits::e1, blurUsageFlags
        );
        UIBlurAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR8G8B8A8Unorm, halfSize, 1, vk::SampleCountFlagBits::e1, blurUsageFlags
        );

        HistoryAttachmentInfo.setImageView(*HistoryAttachment->GetImageView());
        DistortionAttachmentInfo.setImageView(*DistortionAttachment->GetImageView()); // New
        VolumetricAttachmentInfo.setImageView(*VolumetricAttachment->GetImageView()); // New
        BlackHoleAttachmentInfo.setImageView(*BlackHoleAttachment->GetImageView());
        PreBloomAttachmentInfo.setImageView(*PreBloomAttachment->GetImageView());
        GaussBlurAttachmentInfo.setImageView(*GaussBlurAttachment->GetImageView());
        SceneColorAttachmentInfo.setImageView(*SceneColorAttachment->GetImageView());
    };

    auto DestroyFramebuffers = [&]() -> void
    {
        _VulkanContext->WaitIdle();
    };

    CreateFramebuffers();

    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "CreateFramebuffers", CreateFramebuffers);
    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kDestroySwapchain, "DestroyFramebuffers", DestroyFramebuffers);

    // Create pipeline layout
    // ----------------------
    auto* AssetManager = Art::FAssetManager::GetInstance();

    Art::FShader::FResourceInfo QuadResourceInfo
    {
        { { 0, sizeof(FQuadOnlyVertex), false } },
        { { 0, 0, offsetof(FQuadOnlyVertex, Position) } },
        {
            { 0, 0, false },
            { 0, 1, false }
        }
    };

    Art::FShader::FResourceInfo ComputeResourceInfo
    {
        {}, {},
        { { 0, 0, false } },
        { { vk::ShaderStageFlagBits::eCompute, { "ibHorizontal" } } }
    };

    Art::FShader::FResourceInfo BlendResourceInfo
    {
        { { 0, sizeof(FQuadOnlyVertex), false } },
        { { 0, 0, offsetof(FQuadOnlyVertex, Position) } },
        { { 0, 0, false } }
    };
    std::vector<std::string> PrepassShaderFiles({ "ScreenQuad.vert.spv", "BlackHole_prepass.frag.spv" });
    std::vector<std::string> CompositeShaderFiles({ "ScreenQuad.vert.spv", "BlackHole_composite.frag.spv" });
    std::vector<std::string> PreBloomShaderFiles({ "PreBloom.comp.spv" });
    std::vector<std::string> GaussBlurShaderFiles({ "GaussBlur.comp.spv" });
    std::vector<std::string> BlendShaderFiles({ "ScreenQuad.vert.spv", "ColorBlend.frag.spv" });

    VmaAllocationCreateInfo TextureAllocationCreateInfo
    {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };
    AssetManager->AddAsset<Art::FShader>("BlackHolePrepass", PrepassShaderFiles, QuadResourceInfo);
    AssetManager->AddAsset<Art::FShader>("BlackHoleComposite", CompositeShaderFiles, QuadResourceInfo);

    AssetManager->AddAsset<Art::FShader>("PreBloom", PreBloomShaderFiles, ComputeResourceInfo);
    AssetManager->AddAsset<Art::FShader>("GaussBlur", GaussBlurShaderFiles, ComputeResourceInfo);
    AssetManager->AddAsset<Art::FShader>("Blend", BlendShaderFiles, BlendResourceInfo);
    AssetManager->AddAsset<Art::FTextureCube>(
        "Background0", TextureAllocationCreateInfo, "Universe0Skybox", vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm,
        vk::ImageCreateFlagBits::eMutableFormat, true, false);
    AssetManager->AddAsset<Art::FTextureCube>(
        "Antiground0", TextureAllocationCreateInfo, "Antiverse0Skybox", vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm,
        vk::ImageCreateFlagBits::eMutableFormat, true, false);
    AssetManager->AddAsset<Art::FTextureCube>(
        "Background1", TextureAllocationCreateInfo, "Universe1Skybox", vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm,
        vk::ImageCreateFlagBits::eMutableFormat, true, false);
    AssetManager->AddAsset<Art::FTextureCube>(
        "Antiground1", TextureAllocationCreateInfo, "Antiverse1Skybox", vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm,
        vk::ImageCreateFlagBits::eMutableFormat, true, false);
    AssetManager->AddAsset<Art::FTextureCube>(
        "Background2", TextureAllocationCreateInfo, "Universe2Skybox", vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm,
        vk::ImageCreateFlagBits::eMutableFormat, true, false);
    AssetManager->AddAsset<Art::FTextureCube>(
        "Antiground2", TextureAllocationCreateInfo, "Antiverse2Skybox", vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm,
        vk::ImageCreateFlagBits::eMutableFormat, true, false);
    AssetManager->AddAsset<Art::FTexture2D>(
        "RKKV", TextureAllocationCreateInfo, "ButtonMap/rkkv0.png", vk::Format::eR8G8B8A8Unorm,
        vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlags(), false, false);
    AssetManager->AddAsset<Art::FTexture2D>(
        "stage0", TextureAllocationCreateInfo, "stage0.png", vk::Format::eR8G8B8A8Unorm,
        vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlags(), false, false);
    AssetManager->AddAsset<Art::FTexture2D>(
        "stage1", TextureAllocationCreateInfo, "stage1.png", vk::Format::eR8G8B8A8Unorm,
        vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlags(), false, false);
    AssetManager->AddAsset<Art::FTexture2D>(
        "stage2", TextureAllocationCreateInfo, "stage2.png", vk::Format::eR8G8B8A8Unorm,
        vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlags(), false, false);
    AssetManager->AddAsset<Art::FTexture2D>(
        "stage3", TextureAllocationCreateInfo, "stage3.png", vk::Format::eR8G8B8A8Unorm,
        vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlags(), false, false);
    AssetManager->AddAsset<Art::FTexture2D>(
        "stage4", TextureAllocationCreateInfo, "stage4.png", vk::Format::eR8G8B8A8Unorm,
        vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlags(), false, false);
    // ================= [新增] 动态加载 Disk 文件夹下的所有图片 =================
    std::string diskPath = "Assets/Textures/Disk";
    if (std::filesystem::exists(diskPath))
    {
        for (const auto& entry : std::filesystem::directory_iterator(diskPath))
        {
            if (entry.path().extension() == ".png" || entry.path().extension() == ".jpg")
            {
                std::string filename = entry.path().filename().string();
                std::string texName = "DiskTex_" + filename;
                AssetManager->AddAsset<Art::FTexture2D>(
                    texName, TextureAllocationCreateInfo, "Disk/" + filename, vk::Format::eR8G8B8A8Unorm,
                    vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlags(), false, false);
                g_DiskTextures.push_back(texName);
            }
        }
    }
    // =========================================================================
    AssetManager->AddAsset<Art::FTexture2D>(
        "NPGSTexture", TextureAllocationCreateInfo, "nw.png", vk::Format::eR8G8B8A8Unorm,
        vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlags(), false, false);


    auto* PrepassShader = AssetManager->GetAsset<Art::FShader>("BlackHolePrepass");
    auto* CompositeShader = AssetManager->GetAsset<Art::FShader>("BlackHoleComposite");
    auto* PreBloomShader = AssetManager->GetAsset<Art::FShader>("PreBloom");
    auto* GaussBlurShader = AssetManager->GetAsset<Art::FShader>("GaussBlur");
    auto* BlendShader = AssetManager->GetAsset<Art::FShader>("Blend");
    auto* Background0 = AssetManager->GetAsset<Art::FTextureCube>("Background0");
    auto* Antiground0 = AssetManager->GetAsset<Art::FTextureCube>("Antiground0");
    auto* Background1 = AssetManager->GetAsset<Art::FTextureCube>("Background1");
    auto* Antiground1 = AssetManager->GetAsset<Art::FTextureCube>("Antiground1");
    auto* Background2 = AssetManager->GetAsset<Art::FTextureCube>("Background2");
    auto* Antiground2 = AssetManager->GetAsset<Art::FTextureCube>("Antiground2");
    auto* RKKV = AssetManager->GetAsset<Art::FTexture2D>("RKKV");
    auto* stage0 = AssetManager->GetAsset<Art::FTexture2D>("stage0");
    auto* stage1 = AssetManager->GetAsset<Art::FTexture2D>("stage1");
    auto* stage2 = AssetManager->GetAsset<Art::FTexture2D>("stage2");
    auto* stage3 = AssetManager->GetAsset<Art::FTexture2D>("stage3");
    auto* stage4 = AssetManager->GetAsset<Art::FTexture2D>("stage4");
    auto* NPGSTexture = AssetManager->GetAsset<Art::FTexture2D>("NPGSTexture");
    Grt::FShaderResourceManager::FUniformBufferCreateInfo GameArgsCreateInfo
    {
        .Name = "GameArgs",
        .Fields = { "Resolution", "FovRadians", "Time","GameTime", "TimeDelta", "TimeRate" },
        .Set = 0,
        .Binding = 0,
        .Usage = vk::DescriptorType::eUniformBuffer
    };
    Grt::FShaderResourceManager::FUniformBufferCreateInfo PrepassGameArgsCreateInfo = GameArgsCreateInfo;
    PrepassGameArgsCreateInfo.Name = "GameArgsPrepass";

    Grt::FShaderResourceManager::FUniformBufferCreateInfo BlackHoleArgsCreateInfo
    {
        .Name = "BlackHoleArgs",
        .Fields = { "InverseCamRot;", "BlackHoleRelativePosRs", "BlackHoleRelativeDiskNormal","BlackHoleRelativeDiskTangen","CameraVelocity","ie1_up","ie1_up","ie2_up","ie3_up","iU_up",
                    "iCamDataCoordisOutgoing","DEBUG","Prepass","Whitehole","InWhichUniverse","Grid","EnableHeatHaze","EnableShadowCulling", "ObserverMode","Polarization","iUseImageDisk",
                    "Quality","UniverseSign",
                     "BlackHoleTime","BlackHoleMassSol", "Spin","Q2", "Mu", "AccretionRate","BackShiftMax","DensestarsurfaceR","DensestarBlackbodyIntensityExponent","DensestarRedShiftColorExponent","DensestarRedShiftIntensityExponent","DensestarBrightmut","InterRadiusRs", "OuterRadiusRs","ThinRs","Hopper", "Brightmut","Darkmut","Reddening","Saturation"
                     , "BlackbodyIntensityExponent","RedShiftColorExponent","RedShiftIntensityExponent","ImageRotationSpeed","PolarizationAngle","HeatHaze","BackgroundBrightmut","PhotonRingBoost","PhotonRingColorTempBoost","BoostRot","JetRedShiftIntensityExponent","JetBrightmut","JetSaturation","JetShiftMax","BlendWeight"},
        .Set = 0,
        .Binding = 1,
        .Usage = vk::DescriptorType::eUniformBuffer
    };

    auto ShaderResourceManager = Grt::FShaderResourceManager::GetInstance();
    ShaderResourceManager->CreateBuffers<FGameArgs>(GameArgsCreateInfo);
    ShaderResourceManager->CreateBuffers<FGameArgs>(PrepassGameArgsCreateInfo);
    ShaderResourceManager->CreateBuffers<FBlackHoleArgs>(BlackHoleArgsCreateInfo);

    // Create graphics pipeline
    // ------------------------
    vk::SamplerCreateInfo SamplerCreateInfo = Art::FTextureBase::CreateDefaultSamplerCreateInfo();
    std::vector<vk::DescriptorImageInfo> ImageInfos;
    Grt::FVulkanSampler Sampler(SamplerCreateInfo);
    g_GlobalSampler = &Sampler;

    SamplerCreateInfo
        .setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setMipmapMode(vk::SamplerMipmapMode::eNearest);

    Grt::FVulkanSampler FramebufferSampler(SamplerCreateInfo);
    SamplerCreateInfo.setMagFilter(vk::Filter::eNearest).setMinFilter(vk::Filter::eNearest);
    Grt::FVulkanSampler PointSampler(SamplerCreateInfo);

    auto CreatePostDescriptors = [&]() -> void
    {
        ImageInfos.clear();
        ImageInfos.push_back(Background0->CreateDescriptorImageInfo(Sampler));
        PrepassShader->WriteSharedDescriptors(1, 1, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(Antiground0->CreateDescriptorImageInfo(Sampler));
        PrepassShader->WriteSharedDescriptors(1, 2, vk::DescriptorType::eCombinedImageSampler, ImageInfos);


        ImageInfos.clear();
        ImageInfos.push_back(Background1->CreateDescriptorImageInfo(Sampler));
        PrepassShader->WriteSharedDescriptors(1, 3, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(Antiground1->CreateDescriptorImageInfo(Sampler));
        PrepassShader->WriteSharedDescriptors(1, 4, vk::DescriptorType::eCombinedImageSampler, ImageInfos);


        ImageInfos.clear();
        ImageInfos.push_back(Background2->CreateDescriptorImageInfo(Sampler));
        PrepassShader->WriteSharedDescriptors(1, 5, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(Antiground2->CreateDescriptorImageInfo(Sampler));
        PrepassShader->WriteSharedDescriptors(1, 6, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(NPGSTexture->CreateDescriptorImageInfo(Sampler));
        PrepassShader->WriteSharedDescriptors(1, 9, vk::DescriptorType::eCombinedImageSampler, ImageInfos);
        // 2. Composite Descriptors (Set 1)
        // ----------------------------------------------------
        // Binding 0: History Texture
        ImageInfos.clear();
        vk::DescriptorImageInfo HistoryFrameImageInfo(nullptr, *HistoryAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        ImageInfos.push_back(HistoryFrameImageInfo);
        CompositeShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eSampledImage, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(Background0->CreateDescriptorImageInfo(Sampler));
        CompositeShader->WriteSharedDescriptors(1, 1, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(Antiground0->CreateDescriptorImageInfo(Sampler));
        CompositeShader->WriteSharedDescriptors(1, 2, vk::DescriptorType::eCombinedImageSampler, ImageInfos);


        ImageInfos.clear();
        ImageInfos.push_back(Background1->CreateDescriptorImageInfo(Sampler));
        CompositeShader->WriteSharedDescriptors(1, 3, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(Antiground1->CreateDescriptorImageInfo(Sampler));
        CompositeShader->WriteSharedDescriptors(1, 4, vk::DescriptorType::eCombinedImageSampler, ImageInfos);


        ImageInfos.clear();
        ImageInfos.push_back(Background2->CreateDescriptorImageInfo(Sampler));
        CompositeShader->WriteSharedDescriptors(1, 5, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(Antiground2->CreateDescriptorImageInfo(Sampler));
        CompositeShader->WriteSharedDescriptors(1, 6, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(NPGSTexture->CreateDescriptorImageInfo(Sampler));
        CompositeShader->WriteSharedDescriptors(1, 9, vk::DescriptorType::eCombinedImageSampler, ImageInfos);


        ImageInfos.clear();
        vk::DescriptorImageInfo DistortionImageInfo(*PointSampler, *DistortionAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        ImageInfos.push_back(DistortionImageInfo);
        CompositeShader->WriteSharedDescriptors(1, 7, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        vk::DescriptorImageInfo VolumetricImageInfo(*FramebufferSampler, *VolumetricAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        ImageInfos.push_back(VolumetricImageInfo);
        CompositeShader->WriteSharedDescriptors(1, 8, vk::DescriptorType::eCombinedImageSampler, ImageInfos);


        vk::DescriptorImageInfo BlackHoleImageInfo(
            *FramebufferSampler, *BlackHoleAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::DescriptorImageInfo PreBloomImageInfoForSample(
            *FramebufferSampler, *PreBloomAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::DescriptorImageInfo PreBloomImageInfoForStore(
            *FramebufferSampler, *PreBloomAttachment->GetImageView(), vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo GaussBlurImageInfoForSample(
            *FramebufferSampler, *GaussBlurAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::DescriptorImageInfo GaussBlurImageInfoForStore(
            *FramebufferSampler, *GaussBlurAttachment->GetImageView(), vk::ImageLayout::eGeneral);

        ImageInfos.clear();
        ImageInfos.push_back(BlackHoleImageInfo);
        PreBloomShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eCombinedImageSampler, ImageInfos);
        ImageInfos.clear();
        ImageInfos.push_back(PreBloomImageInfoForStore);
        PreBloomShader->WriteSharedDescriptors(1, 1, vk::DescriptorType::eStorageImage, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(PreBloomImageInfoForSample);
        GaussBlurShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eCombinedImageSampler, ImageInfos);
        ImageInfos.clear();
        ImageInfos.push_back(GaussBlurImageInfoForStore);
        GaussBlurShader->WriteSharedDescriptors(1, 1, vk::DescriptorType::eStorageImage, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(BlackHoleImageInfo);
        ImageInfos.push_back(GaussBlurImageInfoForSample);
        BlendShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eCombinedImageSampler, ImageInfos);



        // [新增] 将 UI 模糊纹理注册给 ImGui
        if (_uiRenderer && UIBlurAttachment)
        {
            // 1. 注册纹理
            ImTextureID blurTexID = _uiRenderer->AddTexture(
                *FramebufferSampler, // 复用现有的线性采样器
                *UIBlurAttachment->GetImageView(),
                vk::ImageLayout::eShaderReadOnlyOptimal
            );

            // 2. 更新 UIContext
            auto& ctx = Npgs::System::UI::UIContext::Get();
            ctx.m_scene_blur_texture = blurTexID;
            ctx.m_display_size = ImVec2((float)_WindowSize.width, (float)_WindowSize.height);
        }

    };

    CreatePostDescriptors();
    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "CreatePostDescriptor", CreatePostDescriptors);

    // 绑定 UBO
    std::vector<std::string> BindShaders{ "BlackHoleComposite", "PreBloom", "GaussBlur", "Blend" };
    ShaderResourceManager->BindShadersToBuffers("GameArgs", BindShaders);
    ShaderResourceManager->BindShaderToBuffers("GameArgsPrepass", "BlackHolePrepass");
    // 注意：只给 Prepass 绑定 BlackHoleArgs 也可以，但 Composite 可能需要一些参数（如 BlendWeight），所以都绑定
    ShaderResourceManager->BindShaderToBuffers("BlackHoleArgs", "BlackHolePrepass");
    ShaderResourceManager->BindShaderToBuffers("BlackHoleArgs", "BlackHoleComposite");





    ImTextureID RKKVID = _uiRenderer->AddTexture(
        *FramebufferSampler,
        *RKKV->GetImageView(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );
    stage0ID = _uiRenderer->AddTexture(
        *FramebufferSampler,
        *stage0->GetImageView(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );
    stage1ID = _uiRenderer->AddTexture(
        *FramebufferSampler,
        *stage1->GetImageView(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );
    stage2ID = _uiRenderer->AddTexture(
        *FramebufferSampler,
        *stage2->GetImageView(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );
    stage3ID = _uiRenderer->AddTexture(
        *FramebufferSampler,
        *stage3->GetImageView(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );
    stage4ID = _uiRenderer->AddTexture(
        *FramebufferSampler,
        *stage4->GetImageView(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );

#include "Vertices.inc"
    Grt::FDeviceLocalBuffer QuadOnlyVertexBuffer(QuadOnlyVertices.size() * sizeof(FQuadOnlyVertex), vk::BufferUsageFlagBits::eVertexBuffer);
    QuadOnlyVertexBuffer.CopyData(QuadOnlyVertices);

    // =========================================================================
    // [创建] Pipelines
    // =========================================================================
    std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

    auto* PipelineManager = Grt::FPipelineManager::GetInstance();
    vk::PipelineColorBlendAttachmentState ColorBlendAttachmentState = vk::PipelineColorBlendAttachmentState()
        .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

    // 1. Prepass Pipeline (2 Outputs)
    std::array<vk::Format, 2> PrepassFormats{
        vk::Format::eR32G32B32A32Sfloat, // Distortion
        vk::Format::eR16G16B16A16Sfloat  // Volumetric
    };
    vk::PipelineRenderingCreateInfo PrepassRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
        .setColorAttachmentCount(2)
        .setColorAttachmentFormats(PrepassFormats);

    Grt::FGraphicsPipelineCreateInfoPack PrepassCreateInfoPack;
    PrepassCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&PrepassRenderingCreateInfo);
    PrepassCreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
    PrepassCreateInfoPack.ColorBlendAttachmentStates = { ColorBlendAttachmentState, ColorBlendAttachmentState }; // 两个附件都需要 Blend State (虽然通常 Disable Blend)
    PrepassCreateInfoPack.Viewports.emplace_back(
        0.0f, static_cast<float>(HalfWindowSize.height), static_cast<float>(HalfWindowSize.width),
        -static_cast<float>(HalfWindowSize.height), 0.0f, 1.0f
    );
    PrepassCreateInfoPack.Scissors.emplace_back(vk::Offset2D(), HalfWindowSize);
    PrepassCreateInfoPack.DynamicStates = dynamicStates;
    // [可选] 建议将原本的 Viewports 和 Scissors 清空，因为它们会被动态设置覆盖
    PrepassCreateInfoPack.Viewports.clear();
    PrepassCreateInfoPack.Scissors.clear();
    PipelineManager->CreateGraphicsPipeline("BlackHolePrepassPipeline", "BlackHolePrepass", PrepassCreateInfoPack);

    // 2. Composite Pipeline (1 Output)
    std::array<vk::Format, 1> CompositeFormat{ vk::Format::eR16G16B16A16Sfloat };
    vk::PipelineRenderingCreateInfo CompositeRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
        .setColorAttachmentCount(1)
        .setColorAttachmentFormats(CompositeFormat);

    Grt::FGraphicsPipelineCreateInfoPack CompositeCreateInfoPack;
    CompositeCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&CompositeRenderingCreateInfo);
    CompositeCreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
    CompositeCreateInfoPack.ColorBlendAttachmentStates.emplace_back(ColorBlendAttachmentState);
    CompositeCreateInfoPack.Viewports.emplace_back(0.0f, static_cast<float>(_WindowSize.height), static_cast<float>(_WindowSize.width), -static_cast<float>(_WindowSize.height), 0.0f, 1.0f);
    CompositeCreateInfoPack.Scissors.emplace_back(vk::Offset2D(), _WindowSize);
    CompositeCreateInfoPack.DynamicStates = dynamicStates;
    CompositeCreateInfoPack.Viewports.clear();
    CompositeCreateInfoPack.Scissors.clear();
    PipelineManager->CreateGraphicsPipeline("BlackHoleCompositePipeline", "BlackHoleComposite", CompositeCreateInfoPack);

    // 3. Other Pipelines
    std::array<vk::Format, 1> SceneColorFormat{ vk::Format::eR8G8B8A8Unorm };
    vk::PipelineRenderingCreateInfo BlendRenderingCreateInfo = vk::PipelineRenderingCreateInfo().setColorAttachmentCount(1).setColorAttachmentFormats(SceneColorFormat);
    Grt::FGraphicsPipelineCreateInfoPack BlendCreateInfoPack = CompositeCreateInfoPack; // 复用配置
    // 3. 修改 BlendPipeline 配置
    BlendCreateInfoPack.DynamicStates = dynamicStates;
    BlendCreateInfoPack.Viewports.clear();
    BlendCreateInfoPack.Scissors.clear();
    BlendCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&BlendRenderingCreateInfo);

    PipelineManager->CreateGraphicsPipeline("BlendPipeline", "Blend", BlendCreateInfoPack);
    PipelineManager->CreateComputePipeline("PreBloomPipeline", "PreBloom");
    PipelineManager->CreateComputePipeline("GaussBlurPipeline", "GaussBlur");

    vk::Pipeline PrepassPipeline;
    vk::Pipeline CompositePipeline;
    vk::Pipeline PreBloomPipeline;
    vk::Pipeline GaussBlurPipeline;
    vk::Pipeline BlendPipeline;

    auto GetPipelines = [&]() -> void
    {
        PrepassPipeline = PipelineManager->GetPipeline("BlackHolePrepassPipeline");
        CompositePipeline = PipelineManager->GetPipeline("BlackHoleCompositePipeline");
        PreBloomPipeline = PipelineManager->GetPipeline("PreBloomPipeline");
        GaussBlurPipeline = PipelineManager->GetPipeline("GaussBlurPipeline");
        BlendPipeline = PipelineManager->GetPipeline("BlendPipeline");
    };

    GetPipelines();
    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "GetPipelines", GetPipelines);

    auto PrepassPipelineLayout = PipelineManager->GetPipelineLayout("BlackHolePrepassPipeline");
    auto CompositePipelineLayout = PipelineManager->GetPipelineLayout("BlackHoleCompositePipeline");
    auto PreBloomPipelineLayout = PipelineManager->GetPipelineLayout("PreBloomPipeline");
    auto GaussBlurPipelineLayout = PipelineManager->GetPipelineLayout("GaussBlurPipeline");
    auto BlendPipelineLayout = PipelineManager->GetPipelineLayout("BlendPipeline");

    // Fences & Command Buffers
    std::vector<Grt::FVulkanFence> InFlightFences;
    std::vector<Grt::FVulkanSemaphore> Semaphores_ImageAvailable;
    std::vector<Grt::FVulkanSemaphore> Semaphores_RenderFinished;
    for (std::size_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
    {
        InFlightFences.emplace_back(vk::FenceCreateFlagBits::eSignaled);
        Semaphores_ImageAvailable.emplace_back(vk::SemaphoreCreateFlags());
        Semaphores_RenderFinished.emplace_back(vk::SemaphoreCreateFlags());
    }

    std::vector<Grt::FVulkanCommandBuffer> GraphicsCommandBuffers(Config::Graphics::kMaxFrameInFlight);
    _VulkanContext->GetGraphicsCommandPool().AllocateBuffers(vk::CommandBufferLevel::ePrimary, GraphicsCommandBuffers);

    vk::DeviceSize Offset = 0;
    std::uint32_t  CurrentFrame = 0;
    vk::ImageSubresourceRange SubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

    // Init History Frame (保持不变)
    auto InitHistoryFrame = [&]() -> void
    {
        vk::ImageMemoryBarrier2 InitHistoryBarrier(vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *HistoryAttachment->GetImage(), SubresourceRange);
        vk::DependencyInfo InitialDependencyInfo = vk::DependencyInfo().setDependencyFlags(vk::DependencyFlagBits::eByRegion).setImageMemoryBarriers(InitHistoryBarrier);
        auto& CommandBuffer = _VulkanContext->GetTransferCommandBuffer();
        CommandBuffer.Begin();
        CommandBuffer->pipelineBarrier2(InitialDependencyInfo);
        CommandBuffer.End();
        _VulkanContext->ExecuteGraphicsCommands(CommandBuffer);
    };
    InitHistoryFrame();
    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "InitHistoryFrame", InitHistoryFrame);

    // UI Init (保持不变)
    System::UI::AppContext appContext{ .Application = this, .UIRenderer = _uiRenderer.get(), .GameArgs = &GameArgs, .BlackHoleArgs = &BlackHoleArgs, .GameTime = &GameTime, .TimeRate = &TimeRate, .RealityTime = &RealityTime, .RKKVID = RKKVID, .stage0ID = stage0ID, .stage1ID = stage1ID, .stage2ID = stage2ID, .stage3ID = stage3ID, .stage4ID = stage4ID };
    m_screen_manager = std::make_unique<System::UI::ScreenManager>();
    m_screen_manager->RegisterScreen("Game", std::make_shared<System::UI::GameScreen>(appContext));
    m_screen_manager->RegisterScreen("MainMenu", std::make_shared<System::UI::MainMenuScreen>(appContext));
    m_screen_manager->RequestPushScreen("MainMenu");
    m_screen_manager->ApplyPendingChanges();

    glm::vec4 LastBlackHoleRelativePos(0.0f, 0.0f, 0.0f, 1.0f);
    glm::mat4x4 lastdir(0.0f);
    //LastFrameTime = glfwGetTime();
    while (!glfwWindowShouldClose(_Window))
    {
        while (glfwGetWindowAttrib(_Window, GLFW_ICONIFIED))
        {
            glfwWaitEvents();
        }

        InFlightFences[CurrentFrame].WaitAndReset();

        glfwPollEvents();
        // 开始 UI 帧


// ==== 将这段代码直接放在 glfwPollEvents(); 的下一行 ====
        if (g_bRequestScreenshot)
        {
            g_bRequestScreenshot = false;
            std::cout << "[Screenshot] Starting Automated 4K High-Quality Batch Capture..." << std::endl;

            _VulkanContext->WaitIdle();

            // 1. 备份当前帧状态
            vk::Extent2D OldWindowSize = _WindowSize;
            FGameArgs OldGameArgs = GameArgs;
            FBlackHoleArgs OldBlackHoleArgs = BlackHoleArgs;
            float Old_cfov = cfov; // 备份相机FOV

            // 2. 覆盖全局渲染设定为 4K (3840x2160) 和高精度
            _WindowSize = vk::Extent2D{ 3840, 2160 };
            GameArgs.Resolution = glm::vec2(3840.0f, 2160.0f);
            BlackHoleArgs.Prepass = 0;       // 强制禁用 Prepass，全分辨率采样
            BlackHoleArgs.Quality = 10.0f;   // 高精度步长

            // 重新创建 4K 的 Framebuffers 和 Descriptors
            CreateFramebuffers();
            CreatePostDescriptors();

            // 提前为主机拷贝准备 Staging Buffer (只需分配一次)
            vk::Device device = _VulkanContext->GetDevice();
            vk::DeviceSize bufferSize = 3840 * 2160 * 8; // R16G16B16A16
            vk::BufferCreateInfo bufInfo({}, bufferSize, vk::BufferUsageFlagBits::eTransferDst);
            vk::Buffer stagingBuffer = device.createBuffer(bufInfo);

            vk::MemoryRequirements memReqs = device.getBufferMemoryRequirements(stagingBuffer);
            vk::PhysicalDeviceMemoryProperties memProperties = _VulkanContext->GetPhysicalDevice().getMemoryProperties();
            uint32_t memTypeIndex = 0;
            for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
            {
                if ((memReqs.memoryTypeBits & (1 << i)) &&
                    (memProperties.memoryTypes[i].propertyFlags & (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)) ==
                    (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent))
                {
                    memTypeIndex = i;
                    break;
                }
            }
            vk::MemoryAllocateInfo allocInfo(memReqs.size, memTypeIndex);
            vk::DeviceMemory stagingMemory = device.allocateMemory(allocInfo);
            device.bindBufferMemory(stagingBuffer, stagingMemory, 0);

            // ================= 批量截图参数与任务列表 =================
            struct ScreenshotTask
            {
                int MaxExtension;       // 是否开启最大延拓 (0 / 1)
                int Universe;           // 相机位于哪个宇宙
                float a;                // 自旋
                float Q;                // 电荷
                float ThinRs;           // 盘的厚度
                float Hopper;           // 盘的斜率
                float Phi;              // 相机倾角
                float UniverseSign;     // 相机在 r>0侧(1.0f) 还是 r<0侧(-1.0f)
                int Polarization;       // 是否绘制偏振 (0 / 1)
                float FOV;              // fov
                std::string NameSuffix; // 任务描述后缀
            };

            std::vector<ScreenshotTask> captureTasks = {
                // MaxExt, Univ, a,    Q,    ThinRs, Hop,  Phi,   Sign,  Pol, FOV,   NameSuffix
                //{ 0,       0,    0.2f, 0.0f, 0.0f,   0.5f, 80.0f, 1.0f,  0,   15.0f, "fig5" },
                //{ 1,       1,    0.2f, 0.0f, 0.0f,   0.5f, 80.0f, 1.0f,  0,   15.0f, "fig5" },
                //{ 1,       1,    0.2f, 0.0f, 0.0f,   0.5f, 80.0f, -1.0f,  0,   0.2f, "fig5" },
                //
                //{ 0,       0,    0.6f, 0.0f, 0.0f,   0.5f, 80.0f, 1.0f,  0,   15.0f, "fig5" },
                //{ 1,       1,    0.6f, 0.0f, 0.0f,   0.5f, 80.0f, 1.0f,  0,   15.0f, "fig5" },
                //{ 1,       1,    0.6f, 0.0f, 0.0f,   0.5f, 80.0f, -1.0f,  0,   0.9f, "fig5" },
                //
                //{ 0,       0,    1.0f, 0.0f, 0.0f,   0.5f, 80.0f, 1.0f,  0,   15.0f, "fig5" },
                //{ 1,       1,    1.0f, 0.0f, 0.0f,   0.5f, 80.0f, 1.0f,  0,   15.0f, "fig5" },
                //{ 1,       1,    1.0f, 0.0f, 0.0f,   0.5f, 80.0f, -1.0f,  0,   1.5f, "fig5" },
                                                                                         
                { 1,       1,    0.8f, 0.0f, 2.0f,   0.0f, 17.0f, 1.0f,  1,   9.0f, "fig11" },
                // 可在此处继续追加你的不同参数配置列表...
            };

            // 获取时间前缀并创建目录
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            char timeStr[64];
            std::strftime(timeStr, sizeof(timeStr), "%Y%m%d_%H%M%S", std::localtime(&time));

            std::string folderName = "Screenshot_Batch_" + std::string(timeStr);
            std::filesystem::create_directories(folderName); // 创建文件夹

            const int AccumulationFrames = 8; // 渲染多帧进行等权重混合，消除噪点

            // ================= 开始单层遍历任务 =================
            for (const auto& task : captureTasks)
            {
                // 1. 设置当前任务的所有物理与外观参数
                BlackHoleArgs.Whitehole = task.MaxExtension;
                BlackHoleArgs.InWhichUniverse = task.Universe;
                BlackHoleArgs.Spin = task.a;
                BlackHoleArgs.Q2 = pow(task.Q,2.0);
                BlackHoleArgs.ThinRs = task.ThinRs;
                BlackHoleArgs.Hopper = task.Hopper;
                BlackHoleArgs.UniverseSign = task.UniverseSign;
                BlackHoleArgs.Polarization = task.Polarization;
                BlackHoleArgs.Grid = 0; // 默认关闭网格

                // ================= 动态计算最内稳定轨道 (ISCO) 和视界 =================
                {
                    double a_star = task.a;
                    double q_star = task.Q;
                    double q2_star = q_star * q_star;

                    // 计算 ISCO 半径 (单位: Rs)
                    double isco_Rs = calculate_KN_ISCO(0.5, a_star, q_star);

                    // 计算外视界半径 (单位: Rs)
                    double horizon_discriminant = 1.0 - a_star * a_star - q2_star;
                    double outer_horizon_Rs = 0.0;
                    if (horizon_discriminant >= 0.0)
                    {
                        outer_horizon_Rs = 0.5 * (1.0 + sqrt(horizon_discriminant));
                    }

                    // 设置吸积盘内边界为 ISCO 和 (视界+0.1) 中的较大值
                    BlackHoleArgs.InterRadiusRs = std::fmax((float)isco_Rs, (float)outer_horizon_Rs + 0.1f);

                    std::cout << "  [Param Update] a*=" << a_star << ", Q*=" << q_star
                        << " -> ISCO=" << isco_Rs << " Rs, Horizon+ =" << outer_horizon_Rs << " Rs"
                        << " -> InterRadiusRs set to " << BlackHoleArgs.InterRadiusRs << std::endl;
                }
                // =================================================================================

                std::cout << "[Screenshot] Capturing Task: " << task.NameSuffix << " ..." << std::endl;

                // 2. 设置相机角度与 FOV
                _FreeCamera->TeleportOrbit(0.0f, task.Phi);
                cfov = task.FOV;
                _FreeCamera->SetFov(cfov);
                GameArgs.FovRadians = glm::radians(cfov);

                // 计算画面横向跨度 (tan(0.5 * fov) * 距离)
                                // 计算画面横向跨度 (tan(0.5 * fov) * 距离) 仅用于记录
                                // 3. 提前计算 Rs (用于将世界距离转换为以 Rs 为单位)
                float Rs = 2.0 * abs(BlackHoleArgs.BlackHoleMassSol) * kGravityConstant / pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter;

                // 计算画面横向跨度 (tan(0.5 * fov) * 距离)，并除以 Rs 转换为 Rs 单位
                glm::vec3 camWorldPos = _FreeCamera->GetCameraVector(System::Spatial::FCamera::EVectorType::kPosition);
                float distance = glm::length(camWorldPos);
                float span = (std::tan(glm::radians(cfov * 0.5f)) * distance) / Rs;

                // 更新相机相关的 Shader 矩阵参数 (必须使用原版的 ViewMatrix 乘法)
                BlackHoleArgs.InverseCamRot = glm::mat4_cast(glm::conjugate(_FreeCamera->GetOrientation()));
                BlackHoleArgs.BlackHoleRelativePosRs = glm::vec4(glm::vec3(_FreeCamera->GetViewMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) / Rs, 1.0f);
                BlackHoleArgs.BlackHoleRelativeDiskNormal = (glm::mat4_cast(_FreeCamera->GetOrientation()) * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
                BlackHoleArgs.BlackHoleRelativeDiskTangen = (glm::mat4_cast(_FreeCamera->GetOrientation()) * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
                // ★★★ 必须清空历史帧缓冲！否则切换视角后前几帧会有严重残影 ★★★
                InitHistoryFrame();

                // 4. 多帧累积循环
                for (int i = 1; i <= AccumulationFrames; i++)
                {
                    GameArgs.Time += 0.033f;
                    BlackHoleArgs.BlendWeight = 1.0f / static_cast<float>(i);

                    // 更新 GPU UBO
                    ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "GameArgs", GameArgs);
                    FGameArgs PrepassArgs = GameArgs;
                    PrepassArgs.Resolution = GameArgs.Resolution * 0.5f;
                    ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "GameArgsPrepass", PrepassArgs);
                    ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "BlackHoleArgs", BlackHoleArgs);

                    // 录制独立的截图 CommandBuffer
                    auto& Cmd = _VulkanContext->GetTransferCommandBuffer();
                    Cmd.Begin();

                    vk::Extent2D Half4K = { 1920, 1080 };

                    // === 阶段 A: Prepass ===
                    vk::ImageMemoryBarrier2 b1(vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *DistortionAttachment->GetImage(), SubresourceRange);
                    vk::ImageMemoryBarrier2 b2(vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *VolumetricAttachment->GetImage(), SubresourceRange);
                    std::array preBarriers = { b1, b2 };
                    Cmd->pipelineBarrier2(vk::DependencyInfo().setDependencyFlags(vk::DependencyFlagBits::eByRegion).setImageMemoryBarriers(preBarriers));

                    std::array<vk::RenderingAttachmentInfo, 2> PrepassAttachments = { DistortionAttachmentInfo, VolumetricAttachmentInfo };
                    vk::RenderingInfo PrepassRenderingInfo = vk::RenderingInfo().setRenderArea(vk::Rect2D({ 0, 0 }, Half4K)).setLayerCount(1).setColorAttachments(PrepassAttachments);

                    Cmd->beginRendering(PrepassRenderingInfo);
                    Cmd->bindVertexBuffers(0, *QuadOnlyVertexBuffer.GetBuffer(), Offset);
                    Cmd->bindPipeline(vk::PipelineBindPoint::eGraphics, PrepassPipeline);
                    Cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, PrepassPipelineLayout, 0, PrepassShader->GetDescriptorSets(CurrentFrame), {});

                    vk::Viewport vpHalf(0.0f, 1080.0f, 1920.0f, -1080.0f, 0.0f, 1.0f);
                    vk::Rect2D scHalf({ 0, 0 }, { 1920, 1080 });
                    Cmd->setViewport(0, 1, &vpHalf);
                    Cmd->setScissor(0, 1, &scHalf);
                    Cmd->draw(6, 1, 0, 0);
                    Cmd->endRendering();

                    // === 阶段 B: Composite ===
                    vk::ImageMemoryBarrier2 cb1(vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *DistortionAttachment->GetImage(), SubresourceRange);
                    vk::ImageMemoryBarrier2 cb2(vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *VolumetricAttachment->GetImage(), SubresourceRange);
                    vk::ImageMemoryBarrier2 cb3(vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *BlackHoleAttachment->GetImage(), SubresourceRange);
                    std::array compBarriers = { cb1, cb2, cb3 };
                    Cmd->pipelineBarrier2(vk::DependencyInfo().setDependencyFlags(vk::DependencyFlagBits::eByRegion).setImageMemoryBarriers(compBarriers));

                    vk::RenderingInfo CompositeRenderingInfo = vk::RenderingInfo().setRenderArea(vk::Rect2D({ 0, 0 }, _WindowSize)).setLayerCount(1).setColorAttachments(BlackHoleAttachmentInfo);

                    Cmd->beginRendering(CompositeRenderingInfo);
                    Cmd->bindVertexBuffers(0, *QuadOnlyVertexBuffer.GetBuffer(), Offset);
                    Cmd->bindPipeline(vk::PipelineBindPoint::eGraphics, CompositePipeline);
                    Cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, CompositePipelineLayout, 0, CompositeShader->GetDescriptorSets(CurrentFrame), {});

                    vk::Viewport vpFull(0.0f, 2160.0f, 3840.0f, -2160.0f, 0.0f, 1.0f);
                    vk::Rect2D scFull({ 0, 0 }, { 3840, 2160 });
                    Cmd->setViewport(0, 1, &vpFull);
                    Cmd->setScissor(0, 1, &scFull);
                    Cmd->draw(6, 1, 0, 0);
                    Cmd->endRendering();

                    // === 更新历史帧缓冲 (时序累积) ===
                    vk::ImageMemoryBarrier2 PreCopySrcBarrier(
                        vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead,
                        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal,
                        vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *BlackHoleAttachment->GetImage(), SubresourceRange);
                    vk::ImageMemoryBarrier2 PreCopyDstBarrier(
                        vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead,
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferDstOptimal,
                        vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *HistoryAttachment->GetImage(), SubresourceRange);
                    std::array preCopyBarriers{ PreCopySrcBarrier, PreCopyDstBarrier };
                    Cmd->pipelineBarrier2(vk::DependencyInfo().setDependencyFlags(vk::DependencyFlagBits::eByRegion).setImageMemoryBarriers(preCopyBarriers));

                    vk::ImageCopy HistoryCopyRegion = vk::ImageCopy()
                        .setSrcSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
                        .setDstSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
                        .setExtent(vk::Extent3D(3840, 2160, 1));

                    Cmd->copyImage(*BlackHoleAttachment->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                        *HistoryAttachment->GetImage(), vk::ImageLayout::eTransferDstOptimal, HistoryCopyRegion);

                    vk::ImageMemoryBarrier2 PostCopySrcBarrier(
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead,
                        vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead,
                        vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                        vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *BlackHoleAttachment->GetImage(), SubresourceRange);
                    vk::ImageMemoryBarrier2 PostCopyDstBarrier(
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead,
                        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                        vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *HistoryAttachment->GetImage(), SubresourceRange);

                    // === 阶段 C: PreBloom ===
                    vk::ImageMemoryBarrier2 InitPreBloomBarrier(
                        vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone,
                        vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite,
                        vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
                        vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *PreBloomAttachment->GetImage(), SubresourceRange);

                    std::array bloomInitBarriers = { PostCopySrcBarrier, PostCopyDstBarrier, InitPreBloomBarrier };
                    Cmd->pipelineBarrier2(vk::DependencyInfo().setDependencyFlags(vk::DependencyFlagBits::eByRegion).setImageMemoryBarriers(bloomInitBarriers));
                    std::uint32_t ssWorkX = (3840 + 9) / 10;
                    std::uint32_t ssWorkY = (2160 + 9) / 10;

                    Cmd->bindPipeline(vk::PipelineBindPoint::eCompute, PreBloomPipeline);
                    Cmd->bindDescriptorSets(vk::PipelineBindPoint::eCompute, PreBloomPipelineLayout, 0, PreBloomShader->GetDescriptorSets(CurrentFrame), {});
                    Cmd->dispatch(ssWorkX, ssWorkY, 1);

                    // === 阶段 D: GaussBlur ===
                    vk::ImageMemoryBarrier2 FirstBlurBarrier(vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *PreBloomAttachment->GetImage(), SubresourceRange);
                    vk::ImageMemoryBarrier2 InitGaussBlurBarrier(vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *GaussBlurAttachment->GetImage(), SubresourceRange);
                    std::array blurInitBarriers = { FirstBlurBarrier, InitGaussBlurBarrier };
                    Cmd->pipelineBarrier2(vk::DependencyInfo().setDependencyFlags(vk::DependencyFlagBits::eByRegion).setImageMemoryBarriers(blurInitBarriers));

                    vk::Bool32 bHorizontal = vk::True;
                    Cmd->bindPipeline(vk::PipelineBindPoint::eCompute, GaussBlurPipeline);
                    Cmd->pushConstants(GaussBlurPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(vk::Bool32), &bHorizontal);
                    Cmd->bindDescriptorSets(vk::PipelineBindPoint::eCompute, GaussBlurPipelineLayout, 0, GaussBlurShader->GetDescriptorSets(CurrentFrame), {});
                    Cmd->dispatch(ssWorkX, ssWorkY, 1);

                    vk::ImageMemoryBarrier2 CopybackSrcBarrier(vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *GaussBlurAttachment->GetImage(), SubresourceRange);
                    vk::ImageMemoryBarrier2 CopybackDstBarrier(vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferDstOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *PreBloomAttachment->GetImage(), SubresourceRange);
                    std::array CopybackBarriers{ CopybackSrcBarrier, CopybackDstBarrier };
                    Cmd->pipelineBarrier2(vk::DependencyInfo().setDependencyFlags(vk::DependencyFlagBits::eByRegion).setImageMemoryBarriers(CopybackBarriers));

                    vk::ImageCopy CopybackRegion = vk::ImageCopy().setSrcSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1)).setDstSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1)).setExtent(vk::Extent3D(3840, 2160, 1));
                    Cmd->copyImage(*GaussBlurAttachment->GetImage(), vk::ImageLayout::eTransferSrcOptimal, *PreBloomAttachment->GetImage(), vk::ImageLayout::eTransferDstOptimal, CopybackRegion);

                    vk::ImageMemoryBarrier2 ResampleBarrier(vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *PreBloomAttachment->GetImage(), SubresourceRange);
                    vk::ImageMemoryBarrier2 RewriteBarrier(vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *GaussBlurAttachment->GetImage(), SubresourceRange);
                    std::array RestoreBarriers{ ResampleBarrier, RewriteBarrier };
                    Cmd->pipelineBarrier2(vk::DependencyInfo().setDependencyFlags(vk::DependencyFlagBits::eByRegion).setImageMemoryBarriers(RestoreBarriers));

                    bHorizontal = vk::False;
                    Cmd->pushConstants(GaussBlurPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(vk::Bool32), &bHorizontal);
                    Cmd->bindDescriptorSets(vk::PipelineBindPoint::eCompute, GaussBlurPipelineLayout, 0, GaussBlurShader->GetDescriptorSets(CurrentFrame), {});
                    Cmd->dispatch(ssWorkX, ssWorkY, 1);

                    vk::ImageMemoryBarrier2 BlendSampleBarrier(vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *GaussBlurAttachment->GetImage(), SubresourceRange);
                    Cmd->pipelineBarrier2(vk::DependencyInfo().setDependencyFlags(vk::DependencyFlagBits::eByRegion).setImageMemoryBarriers(BlendSampleBarrier));

                    // === 阶段 E: Blend ===
                    vk::ImageMemoryBarrier2 sceneColorInitBarrier(vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *SceneColorAttachment->GetImage(), SubresourceRange);
                    Cmd->pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(sceneColorInitBarrier));

                    vk::RenderingInfo sceneRenderingInfo = vk::RenderingInfo().setRenderArea(vk::Rect2D({ 0, 0 }, _WindowSize)).setLayerCount(1).setColorAttachments(SceneColorAttachmentInfo);

                    Cmd->beginRendering(sceneRenderingInfo);
                    Cmd->bindVertexBuffers(0, *QuadOnlyVertexBuffer.GetBuffer(), Offset);
                    Cmd->bindPipeline(vk::PipelineBindPoint::eGraphics, BlendPipeline);
                    Cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, BlendPipelineLayout, 0, BlendShader->GetDescriptorSets(CurrentFrame), {});

                    Cmd->setViewport(0, 1, &vpFull);
                    Cmd->setScissor(0, 1, &scFull);
                    Cmd->draw(6, 1, 0, 0);
                    Cmd->endRendering();

                    // 如果是最后一帧，将结果拷贝到主机的 Staging Buffer
                    if (i == AccumulationFrames)
                    {
                        // 此时 HistoryAttachment 已经包含了最纯净的累积 HDR 物理数值，绕过后续的 Blend 和 Bloom
                        vk::ImageMemoryBarrier2 copySrcBarrier(
                            vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead,
                            vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead,
                            vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferSrcOptimal,
                            vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *HistoryAttachment->GetImage(), SubresourceRange);
                        Cmd->pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(copySrcBarrier));

                        // 执行拷贝
                        vk::BufferImageCopy copyRegion(0, 0, 0, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), vk::Offset3D(0, 0, 0), vk::Extent3D(3840, 2160, 1));
                        Cmd->copyImageToBuffer(*HistoryAttachment->GetImage(), vk::ImageLayout::eTransferSrcOptimal, stagingBuffer, 1, &copyRegion);

                        // 将 History 贴图状态还原，并设置主机内存屏障
                        vk::ImageMemoryBarrier2 restoreBarrier(
                            vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead,
                            vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead,
                            vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                            vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *HistoryAttachment->GetImage(), SubresourceRange);

                        vk::MemoryBarrier2 hostBarrier(
                            vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                            vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostRead);

                        Cmd->pipelineBarrier2(vk::DependencyInfo().setMemoryBarriers(hostBarrier).setImageMemoryBarriers(restoreBarrier));
                    }

                    Cmd.End();

                    // 提交并阻塞等待单次渲染结束
                    _VulkanContext->ExecuteGraphicsCommands(Cmd);
                    _VulkanContext->WaitIdle();
                } // 结束多帧混合循环

                // 5. 保存当前任务截图 (无损保存为 .hdr 浮点文件)
                void* data = device.mapMemory(stagingMemory, 0, bufferSize, vk::MemoryMapFlags());

                // 将显存中读取的 FP16 数据转换为 FP32 (Standard float)
                uint16_t* fp16_pixels = static_cast<uint16_t*>(data);
                float* fp32_pixels = new float[3840 * 2160 * 3]; // HDR 文件通常只存 RGB 三通道

                for (size_t p = 0; p < 3840 * 2160; ++p)
                {
                    fp32_pixels[p * 3 + 0] = glm::unpackHalf1x16(fp16_pixels[p * 4 + 0]);
                    fp32_pixels[p * 3 + 1] = glm::unpackHalf1x16(fp16_pixels[p * 4 + 1]);
                    fp32_pixels[p * 3 + 2] = glm::unpackHalf1x16(fp16_pixels[p * 4 + 2]);
                }

                // 文件名拼接（将除FOV以外的所有参量展开，并包含算出的横向跨度 Span）
                std::string filename = folderName + "/Data_4K_" + std::string(timeStr) + "_" + task.NameSuffix
                    + "_Ext" + std::to_string(task.MaxExtension)
                    + "_Univ" + std::to_string(task.Universe)
                    + "_a" + std::to_string(task.a)
                    + "_Q" + std::to_string(task.Q)
                    + "_Thin" + std::to_string(task.ThinRs)
                    + "_Hop" + std::to_string(task.Hopper)
                    + "_Phi" + std::to_string(task.Phi)
                    + "_Sign" + std::to_string(task.UniverseSign)
                    + "_Pol" + std::to_string(task.Polarization)
                    + "_Span" + std::to_string(span) + ".hdr";

                // 使用 stbi_write_hdr 保存真正的浮点数矩阵
                stbi_write_hdr(filename.c_str(), 3840, 2160, 3, fp32_pixels);
                std::cout << "  -> Saved RAW HDR: " << filename << std::endl;

                delete[] fp32_pixels;
                device.unmapMemory(stagingMemory);

            } // 结束任务列表的单层遍历

            std::cout << "[Screenshot] Batch Capture Completed!" << std::endl;

            // 6. 清理内存并还原初始状态
            device.destroyBuffer(stagingBuffer);
            device.freeMemory(stagingMemory);

            _WindowSize = OldWindowSize;
            GameArgs = OldGameArgs;
            BlackHoleArgs = OldBlackHoleArgs;
            cfov = Old_cfov;
            _FreeCamera->SetFov(cfov);

            ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "GameArgs", GameArgs);
            FGameArgs PrepassArgsRestored = GameArgs;
            PrepassArgsRestored.Resolution = GameArgs.Resolution * 0.5f;
            ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "GameArgsPrepass", PrepassArgsRestored);
            ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "BlackHoleArgs", BlackHoleArgs);

            // 回复正常尺寸的 Attachments 和 Descriptors
            CreateFramebuffers();
            CreatePostDescriptors();
            InitHistoryFrame();
            CurrentTime = glfwGetTime();
            LastFrameTime = CurrentTime;
        }
        // ==== 截图代码植入结束 ====






        _uiRenderer->BeginFrame();

        auto& ui_ctx = Npgs::System::UI::UIContext::Get();
        ui_ctx.m_display_size = ImVec2((float)_WindowSize.width, (float)_WindowSize.height);

        // =========================================================================
        // [修改] 游戏主循环中的 UI 处理
        // =========================================================================
        ui_ctx.SetInputBlocked(_bIsDraggingInWorld || g_bHideUIAndMouse);
        m_screen_manager->Update(_DeltaTime);
        m_screen_manager->ApplyPendingChanges();

        // 只有在未隐藏 UI 的情况下才执行绘制
        if (!g_bHideUIAndMouse)
        {
            m_screen_manager->Draw();
            // Render other standard ImGui windows
            RenderDebugUI();
        }
        // =========================================================================

        _uiRenderer->EndFrame();
        // Uniform update
        // --------------
        _FreeCamera->SetFov(cfov);


        {
            float Rs = 2.0 * abs(BlackHoleArgs.BlackHoleMassSol) * kGravityConstant / pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter;
            if (FrameCount <= 10)
            {
                GameArgs.Resolution = glm::vec2(_WindowSize.width, _WindowSize.height);
                GameArgs.FovRadians = glm::radians(_FreeCamera->GetCameraZoom());
                GameArgs.Time = RealityTime;
                GameArgs.GameTime = GameTime;
                GameArgs.TimeDelta = static_cast<float>(_DeltaTime);
                GameArgs.TimeRate = TimeRate;
                LastBlackHoleRelativePos = BlackHoleArgs.BlackHoleRelativePosRs;
                lastdir = BlackHoleArgs.InverseCamRot;
                ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "GameArgs", GameArgs);
                FGameArgs PrepassArgs = GameArgs;
                PrepassArgs.Resolution = GameArgs.Resolution * 0.5f; // <--- 关键：分辨率减半
                ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "GameArgsPrepass", PrepassArgs);
                BlackHoleArgs.InverseCamRot = glm::mat4_cast(glm::conjugate(_FreeCamera->GetOrientation()));
                BlackHoleArgs.BlackHoleRelativePosRs = glm::vec4(glm::vec3(_FreeCamera->GetViewMatrix() * glm::vec4(0.0 * BlackHoleArgs.BlackHoleMassSol * kGravityConstant / pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter, 0.0f, -0.000f, 1.0f)) / Rs, 1.0);
                BlackHoleArgs.BlackHoleRelativeDiskNormal = (glm::mat4_cast(_FreeCamera->GetOrientation()) * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
                BlackHoleArgs.BlackHoleRelativeDiskTangen = (glm::mat4_cast(_FreeCamera->GetOrientation()) * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
                BlackHoleArgs.CameraVelocity = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

                BlackHoleArgs.DEBUG = 0;
                //BlackHoleArgs.Prepass = 1;
                BlackHoleArgs.Prepass = 1;
                BlackHoleArgs.Whitehole = 0;
                //BlackHoleArgs.Whitehole = 1;
                BlackHoleArgs.InWhichUniverse = 0;
                BlackHoleArgs.Grid = 0;
                BlackHoleArgs.EnableHeatHaze = 0;
                BlackHoleArgs.EnableShadowCulling = 0;
                BlackHoleArgs.ObserverMode = 0;
                BlackHoleArgs.Polarization = 0;
                BlackHoleArgs.UseImageDisk = 0;

                BlackHoleArgs.Quality = 1.0;
                BlackHoleArgs.UniverseSign = 1.0;
                //BlackHoleArgs.UniverseSign = -1.0;
                BlackHoleArgs.BlackHoleTime = GameTime * kSpeedOfLight / Rs / kLightYearToMeter;
                BlackHoleArgs.BlackHoleMassSol = 1.49e7f;
                BlackHoleArgs.Spin = 0.998f;
                BlackHoleArgs.Q2 =(pow(g_Q,2.0)+ pow(g_P, 2.0));
                BlackHoleArgs.Mu = 1.0f;
                BlackHoleArgs.AccretionRate = (1e-12);
                BlackHoleArgs.BackShiftMax = 1.5f;

                BlackHoleArgs.DensestarsurfaceR = 0.0;
                BlackHoleArgs.DensestarBlackbodyIntensityExponent = 4.0;
                BlackHoleArgs.DensestarRedShiftColorExponent = 1.0;
                BlackHoleArgs.DensestarRedShiftIntensityExponent = 4.0;
                BlackHoleArgs.DensestarBrightmut = 1.0;


                BlackHoleArgs.InterRadiusRs = 200.0;
                BlackHoleArgs.OuterRadiusRs = 25.0;
                BlackHoleArgs.ThinRs = 0.75;
                BlackHoleArgs.Hopper = 0.4;
                BlackHoleArgs.Brightmut = 1.0;
                BlackHoleArgs.Darkmut = 0.5;
                BlackHoleArgs.Reddening = 0.3;
                BlackHoleArgs.Saturation = 0.5;
                //BlackHoleArgs.Darkmut = 0.0;
                //BlackHoleArgs.Reddening = 0.0;
                //BlackHoleArgs.Saturation = 0.0;
                BlackHoleArgs.BlackbodyIntensityExponent = 1.0;
                //BlackHoleArgs.BlackbodyIntensityExponent = 4.0;
                BlackHoleArgs.RedShiftColorExponent = 1.0;
                BlackHoleArgs.RedShiftIntensityExponent = 4.0;
                BlackHoleArgs.ImageRotationSpeed = 0.00765619656 * (3.06 / 3.0);
                BlackHoleArgs.PolarizationAngle = 0.0;
                BlackHoleArgs.HeatHaze = 0.0;
                BlackHoleArgs.BackgroundBrightmut = 2.0;
                //BlackHoleArgs.BackgroundBrightmut = 0.0;
                BlackHoleArgs.PhotonRingBoost = 0.0;
                BlackHoleArgs.PhotonRingColorTempBoost = 0.0;
                BlackHoleArgs.BoostRot = 0.0;
                BlackHoleArgs.JetRedShiftIntensityExponent = 4.0;
                BlackHoleArgs.JetBrightmut = 1.0;
                BlackHoleArgs.JetSaturation = 0.0;
                BlackHoleArgs.JetShiftMax = 3.0;



                //BlackHoleArgs.DEBUG = 0;
                //BlackHoleArgs.Prepass = 1;
                //BlackHoleArgs.Whitehole = 1;
                //BlackHoleArgs.InWhichUniverse = 0;
                //BlackHoleArgs.Grid = 0;
                //BlackHoleArgs.EnableHeatHaze = 0;
                //BlackHoleArgs.EnableShadowCulling = 0;
                //BlackHoleArgs.ObserverMode = 0;
                //BlackHoleArgs.Polarization = 0.0;
                //BlackHoleArgs.Quality = 1.0;
                //BlackHoleArgs.UniverseSign = 1.0;
                //BlackHoleArgs.BlackHoleTime = GameTime * kSpeedOfLight / Rs / kLightYearToMeter;
                //BlackHoleArgs.BlackHoleMassSol = 1.49e7f;
                //BlackHoleArgs.Spin = 0.8f;
                //BlackHoleArgs.Q = 0.0f;
                //BlackHoleArgs.Mu = 1.0f;
                //BlackHoleArgs.AccretionRate = (1e-8);
                //BlackHoleArgs.BackShiftMax = 1.02f;
                //BlackHoleArgs.InterRadiusRs = 1.0;
                //BlackHoleArgs.OuterRadiusRs = 25;
                //BlackHoleArgs.ThinRs = 0.75;
                //BlackHoleArgs.Hopper = 0.24;
                //BlackHoleArgs.Brightmut = 1.0;
                //BlackHoleArgs.Darkmut = 0.0;
                //BlackHoleArgs.Reddening = 0.0;
                //BlackHoleArgs.Saturation = 0.0;
                //BlackHoleArgs.BlackbodyIntensityExponent = 4.0;
                //BlackHoleArgs.RedShiftColorExponent = 1.0;
                //BlackHoleArgs.RedShiftIntensityExponent = 4.0;
                //BlackHoleArgs.PolarizationAngle = 0.0;
                //BlackHoleArgs.HeatHaze = 0.0;
                //BlackHoleArgs.BackgroundBrightmut = 0.0;
                //BlackHoleArgs.PhotonRingBoost = 0.0;
                //BlackHoleArgs.PhotonRingColorTempBoost = 0.0;
                //BlackHoleArgs.BoostRot = 0.0;
                //BlackHoleArgs.JetRedShiftIntensityExponent = 4.0;
                //BlackHoleArgs.JetBrightmut = 1.0;
                //BlackHoleArgs.JetSaturation = 0.0;
                //BlackHoleArgs.JetShiftMax = 3.0;

            }
            else
            {
                GameArgs.Resolution = glm::vec2(_WindowSize.width, _WindowSize.height);
                GameArgs.FovRadians = glm::radians(_FreeCamera->GetCameraZoom());
                GameArgs.Time = RealityTime;
                GameArgs.GameTime = GameTime;
                GameArgs.TimeDelta = static_cast<float>(_DeltaTime);
                GameArgs.TimeRate = TimeRate;
                LastBlackHoleRelativePos = BlackHoleArgs.BlackHoleRelativePosRs;
                lastdir = BlackHoleArgs.InverseCamRot;
                ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "GameArgs", GameArgs);
                FGameArgs PrepassArgs = GameArgs;
                PrepassArgs.Resolution = GameArgs.Resolution * 0.5f; // <--- 关键：分辨率减半
                ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "GameArgsPrepass", PrepassArgs);
                BlackHoleArgs.BlackHoleTime = GameTime * kSpeedOfLight / Rs / kLightYearToMeter;
                BlackHoleArgs.InverseCamRot = glm::mat4_cast(glm::conjugate(_FreeCamera->GetOrientation()));
                BlackHoleArgs.BlackHoleRelativePosRs = glm::vec4(glm::vec3(_FreeCamera->GetViewMatrix() * glm::vec4(0.0f, 0.0f, -0.000f, 1.0f)) / Rs, 1.0);
                BlackHoleArgs.BlackHoleRelativeDiskNormal = (glm::mat4_cast(_FreeCamera->GetOrientation()) * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
                BlackHoleArgs.BlackHoleRelativeDiskTangen = (glm::mat4_cast(_FreeCamera->GetOrientation()) * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
                BlackHoleArgs.Q2 = (pow(g_Q, 2.0) + pow(g_P, 2.0));



                // 修正后的代码
                glm::vec3 posDiff = _FreeCamera->GetCameraVector(System::Spatial::FCamera::EVectorType::kPosition) - LastCameraWorldPos;

                // 计算真实的无量纲物理速度 (v / c)
                // posDiff 的单位是光年，转成米后除以 (时间 * 光速)
                glm::vec3 currentDimVelocity = posDiff * (kLightYearToMeter / float(_LastDeltaTime * TimeRate * kSpeedOfLight));

                // 平滑过渡
                BlackHoleArgs.CameraVelocity += float(1.0 - exp(-_DeltaTime / 0.1)) * (glm::vec4(currentDimVelocity, 0.0f) - BlackHoleArgs.CameraVelocity);
                if (glm::any(glm::isnan(BlackHoleArgs.CameraVelocity)) || glm::any(glm::isinf(BlackHoleArgs.CameraVelocity)))
                {
                    BlackHoleArgs.CameraVelocity = glm::vec4(0.0f);
                }
            }

            Rs = 2.0 * abs(BlackHoleArgs.BlackHoleMassSol) * kGravityConstant / pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter;
            BlackHoleArgs.BlendWeight = (1.0 - pow(0.5, (_DeltaTime) / std::fmax(std::min((0.131 * 36.0 / (GameArgs.TimeRate) * (Rs / 0.00000465)), 0.5), 0.06)));
            if (!(abs(glm::quat((lastdir - BlackHoleArgs.InverseCamRot)).w - 0.5) < 0.001 * _DeltaTime || abs(glm::quat((lastdir - BlackHoleArgs.InverseCamRot)).w - 0.0) < 0.001 * _DeltaTime) ||
                glm::length(glm::vec3(LastBlackHoleRelativePos - BlackHoleArgs.BlackHoleRelativePosRs)) > (glm::length(glm::vec3(LastBlackHoleRelativePos)) - 1.0) * 0.006 * _DeltaTime || glm::length(BlackHoleArgs.CameraVelocity) > 0.0001)
            {
                BlackHoleArgs.BlendWeight = 1.0f;
            }
            if (int(glfwGetTime()) < 1)
            {
                _FreeCamera->SetTargetOrbitAxis(glm::vec3(0., 1., 0.)); _FreeCamera->SetTargetOrbitCenter(glm::vec3(0, 0, 0));
            }//else{ _FreeCamera->SetTargetOrbitAxis(glm::vec3(0., -1., -0.)); _FreeCamera->SetTargetOrbitCenter(glm::vec3(0.,0.0*5.586e-5f, 0));
           // }
           // _FreeCamera->ProcessMouseMovement(10, 0);
                        // ==================== [修改] 先执行测地线积分以获得当帧最新的准确位置 ====================
            if (g_GeodesicMode)
            {
                float Rs = 2.0 * abs(BlackHoleArgs.BlackHoleMassSol) * kGravityConstant / pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter;
                double total_dtau = _DeltaTime * TimeRate * kSpeedOfLight / Rs / kLightYearToMeter;

                double dtau_remaining = std::abs(total_dtau);
                double dtau_sign = (total_dtau >= 0.0) ? 1.0 : -1.0;

                // 限制单帧最大子步数，防止极端 TimeRate 导致主线程卡死
                const int MAX_SUBSTEPS_PER_FRAME = 1500;
                int step_count = 0;

                while (dtau_remaining > 1e-9 && step_count < MAX_SUBSTEPS_PER_FRAME)
                {
                    double abs_a = std::abs(BlackHoleArgs.Spin * 0.5); // M = 0.5，物理自旋 a = Spin * M
                    double rho = std::sqrt(g_GeoState[0] * g_GeoState[0] + g_GeoState[2] * g_GeoState[2]);
                    double r_sq = (rho - abs_a) * (rho - abs_a) + g_GeoState[1] * g_GeoState[1];
                    double R = std::sqrt(std::fmax(1e-12, r_sq));

                    double scaled_R = std::fmax(R / 2.0, 1.0);
                    double max_dtau_gravity = 0.005 * scaled_R * std::sqrt(scaled_R);

                    double U_spatial_mag = std::sqrt(g_GeoState[4] * g_GeoState[4] + g_GeoState[5] * g_GeoState[5] + g_GeoState[6] * g_GeoState[6]);
                    double max_dtau_kinematic = (0.05 * R) / std::fmax(1e-6, U_spatial_mag);

                    double safe_dtau = std::min(max_dtau_gravity, max_dtau_kinematic);
                    safe_dtau = std::clamp(safe_dtau, 0.0005, 5.0);

                    double current_step = std::min(dtau_remaining, safe_dtau);

                    // 执行单步积分
                    GeodesicIntegrator::StepRK4(current_step * dtau_sign, BlackHoleArgs.Spin * 0.5, sqrt(pow(g_Q, 2.0) + pow(g_P, 2.0)) * 0.5);
                    GeodesicIntegrator::g_ProperTime += (current_step * dtau_sign);
                    dtau_remaining -= current_step;
                    step_count++;
                }

                // 覆盖坐标与时间
                BlackHoleArgs.BlackHoleRelativePosRs = glm::vec4(g_GeoState[0], g_GeoState[1], g_GeoState[2], g_GeoState[3]);
                BlackHoleArgs.BlackHoleTime = g_GeoState[3];
                // 【修复点】：把积分器内部精确处理好的 g_UniverseSign 赋给 BlackHoleArgs，而不是反过来
                BlackHoleArgs.UniverseSign = g_UniverseSign;

                // 设置 Shader 的读取标志
                BlackHoleArgs.ObserverMode = -1;
                BlackHoleArgs.iCamDataCoordisOutgoing = g_isOutgoing ? 1 : 0;
                BlackHoleArgs.CameraVelocity = glm::vec4(0.0f); // 速度已在四维向量中，置零

                // 提取相机的鼠标旋转，作用于空间基底
                glm::mat4 headRot = glm::mat4_cast(glm::conjugate(_FreeCamera->GetOrientation()));

                glm::vec4 e1(0), e2(0), e3(0);
                for (int j = 0; j < 3; ++j)
                {
                    glm::vec4 base_j(g_GeoState[8 + 4 * j], g_GeoState[8 + 4 * j + 1], g_GeoState[8 + 4 * j + 2], g_GeoState[8 + 4 * j + 3]);
                    e1 -= headRot[0][j] * base_j;
                    e2 -= headRot[1][j] * base_j;
                    e3 -= headRot[2][j] * base_j;
                }

                BlackHoleArgs.ie1_up = e1;
                BlackHoleArgs.ie2_up = e2;
                BlackHoleArgs.ie3_up = e3;
                BlackHoleArgs.iU_up = glm::vec4(g_GeoState[4], g_GeoState[5], g_GeoState[6], g_GeoState[7]);

                // 保持 InverseCamRot 依然生效以兼容光线计算
                BlackHoleArgs.InverseCamRot = headRot;
            }
            else
            {
                if (BlackHoleArgs.ObserverMode == -1) BlackHoleArgs.ObserverMode = 0;
            }


            // ==================== [修改] 获取最新计算出的坐标后再进行穿越与宇宙判断 ====================
            glm::vec3 pos;
            Rs = 2.0 * abs(BlackHoleArgs.BlackHoleMassSol) * kGravityConstant / pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter;

            if (g_GeodesicMode)
            {
                // 测地线状态是 Rs 单位，需转回世界单位
                pos = glm::vec3(g_GeoState[0], g_GeoState[1], g_GeoState[2]) * Rs;
            }
            else
            {
                pos = _FreeCamera->GetCameraVector(System::Spatial::FCamera::EVectorType::kPosition);
            }

            // 2. 物理参数准备
            float M = 0.5f * Rs;
            float a = BlackHoleArgs.Spin * M;      // 物理自旋 a
            float Q_phys = sqrt(pow(g_Q, 2.0) + pow(g_P, 2.0)) * M;    // 物理电荷 Q (注意量纲跟随M)
            float a2 = a * a;
            float Q2 = Q_phys * Q_phys;

            // 获取当前帧相机世界坐标及 UniverseSign 更新逻辑
            glm::vec3 currentPos = pos;
            if (LastCameraWorldPos.y * currentPos.y <= 0.0f && FrameCount > 1)
            {
                float denom = LastCameraWorldPos.y - currentPos.y;
                if (std::abs(denom) > 0)
                {
                    float t = LastCameraWorldPos.y / denom;
                    float intersectX = LastCameraWorldPos.x + t * (currentPos.x - LastCameraWorldPos.x);
                    float intersectZ = LastCameraWorldPos.z + t * (currentPos.z - LastCameraWorldPos.z);
                    float rho2 = intersectX * intersectX + intersectZ * intersectZ;
                    if (rho2 < a2)
                    {
                        // 【修复点】：仅当漫游模式时才在这里粗略判断穿环，测地模式已经由积分器自行完成了精准翻转
                        if (!g_GeodesicMode)
                        {
                            BlackHoleArgs.UniverseSign *= -1.0f;
                        }
                    }
                }
            }
            LastCameraWorldPos = currentPos;

            // 3. 从 KS 坐标求解 BL 半径 r
            float x2 = pos.x * pos.x;
            float y2 = pos.y * pos.y;
            float z2 = pos.z * pos.z;
            float R2 = x2 + y2 + z2;

            float b = R2 - a2;
            float c = a2 * y2;
            float r2 = 0.5f * (b + std::sqrt(b * b + 4.0f * c));
            float r = std::sqrt(r2) * BlackHoleArgs.UniverseSign;

            // 4. 计算 Kerr-Newman 度规函数 f
            float sigma_times_r2 = r2 * r2 + a2 * y2;
            float f = ((2.0f * M * r - Q2) * r2) / sigma_times_r2;

            // 5. 计算视界半径
            float delta_discriminant = M * M - a2 - Q2;
            float horizon_outer = 0.0f;
            float horizon_inner = 0.0f;
            bool isNakedSingularity = delta_discriminant < 0.0f;

            if (!isNakedSingularity)
            {
                float sqrt_delta = std::sqrt(delta_discriminant);
                horizon_outer = M + sqrt_delta;
                horizon_inner = M - sqrt_delta;
            }
            static float s_last_r = r; // 记录上一帧的 BL 半径 r
            // 仅在内视界存在且观测模式为 2 时执行
            if (!isNakedSingularity && BlackHoleArgs.Whitehole == 1)
            {
                // 若上一帧还在内视界之外，当前帧进入了内视界（向内穿过）
                if (s_last_r > horizon_inner && r <= horizon_inner && BlackHoleArgs.UniverseSign == 1.0f)
                {
                    // 在 0 和 1 之间翻转 InWhichUniverse
                    BlackHoleArgs.InWhichUniverse = (BlackHoleArgs.InWhichUniverse + 1) % 3;
                }
            }

            s_last_r = r; // 逻辑判断完后更新 s_last_r 供下一帧使用

            // 紧接着就是：
            // ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "BlackHoleArgs", BlackHoleArgs);
            // =====================================================================
            // =====================================================================

            ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "BlackHoleArgs", BlackHoleArgs);

            _VulkanContext->SwapImage(*Semaphores_ImageAvailable[CurrentFrame]);
            std::uint32_t ImageIndex = _VulkanContext->GetCurrentImageIndex();

            // BlendAttachmentInfo.setImageView(_VulkanContext->GetSwapchainImageView(ImageIndex));

            std::uint32_t WorkgroundX = (_WindowSize.width + 9) / 10;
            std::uint32_t WorkgroundY = (_WindowSize.height + 9) / 10;

            // Record BlackHole rendering commands
            // -----------------------------------
            auto& CurrentBuffer = GraphicsCommandBuffers[CurrentFrame];
            CurrentBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);


            vk::Extent2D CurrentHalfSize = { _WindowSize.width / 2, _WindowSize.height / 2 };
            if (CurrentHalfSize.width == 0) CurrentHalfSize.width = 1;
            if (CurrentHalfSize.height == 0) CurrentHalfSize.height = 1;

            // 定义视口和裁剪
            vk::Viewport PrepassViewport(
                0.0f, static_cast<float>(CurrentHalfSize.height),
                static_cast<float>(CurrentHalfSize.width), -static_cast<float>(CurrentHalfSize.height),
                0.0f, 1.0f
            );
            vk::Rect2D PrepassScissor({ 0, 0 }, CurrentHalfSize);



            {
                vk::ImageMemoryBarrier2 b1(vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *DistortionAttachment->GetImage(), SubresourceRange);
                vk::ImageMemoryBarrier2 b2(vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *VolumetricAttachment->GetImage(), SubresourceRange);
                std::array barriers = { b1, b2 };
                CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setDependencyFlags(vk::DependencyFlagBits::eByRegion).setImageMemoryBarriers(barriers));
            }

            std::array<vk::RenderingAttachmentInfo, 2> PrepassAttachments = {
                            DistortionAttachmentInfo,
                            VolumetricAttachmentInfo
            };

            vk::RenderingInfo PrepassRenderingInfo = vk::RenderingInfo()
                .setRenderArea(vk::Rect2D({ 0, 0 }, CurrentHalfSize))
                .setLayerCount(1)
                .setColorAttachments(PrepassAttachments); // 或者使用 

            CurrentBuffer->beginRendering(PrepassRenderingInfo);
            CurrentBuffer->bindVertexBuffers(0, *QuadOnlyVertexBuffer.GetBuffer(), Offset);
            CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, PrepassPipeline);
            CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, PrepassPipelineLayout, 0, PrepassShader->GetDescriptorSets(CurrentFrame), {});
            CurrentBuffer->draw(6, 1, 0, 0);
            CurrentBuffer->endRendering();

            // =====================================================================
            // PASS 2: Composite (Render to BlackHoleAttachment, Reading Prepass)
            // =====================================================================

            // 1. Barrier: Transition Prepass Attachments to ShaderReadOnly
            //    Barrier: Transition Composite Output (BlackHole) to ColorAttachmentOptimal
            {
                // Distortion & Volumetric: Write -> Read
                vk::ImageMemoryBarrier2 b1(vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *DistortionAttachment->GetImage(), SubresourceRange);
                vk::ImageMemoryBarrier2 b2(vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *VolumetricAttachment->GetImage(), SubresourceRange);
                // BlackHole: Undef -> Write
                vk::ImageMemoryBarrier2 b3(vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *BlackHoleAttachment->GetImage(), SubresourceRange);

                std::array barriers = { b1, b2, b3 };
                CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setDependencyFlags(vk::DependencyFlagBits::eByRegion).setImageMemoryBarriers(barriers));
            }

            vk::RenderingInfo CompositeRenderingInfo = vk::RenderingInfo()
                .setRenderArea(vk::Rect2D({ 0, 0 }, _WindowSize))
                .setLayerCount(1)
                .setColorAttachments(BlackHoleAttachmentInfo);

            CurrentBuffer->beginRendering(CompositeRenderingInfo);
            CurrentBuffer->bindVertexBuffers(0, *QuadOnlyVertexBuffer.GetBuffer(), Offset);
            CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, CompositePipeline);
            // 注意：Composite Shader 的 Descriptor Set 包含了 Prepass 的结果
            CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, CompositePipelineLayout, 0, CompositeShader->GetDescriptorSets(CurrentFrame), {});
            CurrentBuffer->draw(6, 1, 0, 0);
            CurrentBuffer->endRendering();

            vk::ImageMemoryBarrier2 PreCopySrcBarrier(
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *BlackHoleAttachment->GetImage(),
                SubresourceRange);

            vk::ImageMemoryBarrier2 PreCopyDstBarrier(
                vk::PipelineStageFlagBits2::eFragmentShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::ImageLayout::eTransferDstOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *HistoryAttachment->GetImage(),
                SubresourceRange);

            std::array PreCopyBarriers{ PreCopySrcBarrier, PreCopyDstBarrier };
            vk::DependencyInfo PreCopyDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(PreCopyBarriers);

            vk::ImageCopy HistoryCopyRegion = vk::ImageCopy()
                .setSrcSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
                .setDstSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
                .setExtent(vk::Extent3D(_WindowSize.width, _WindowSize.height, 1));

            CurrentBuffer->pipelineBarrier2(PreCopyDependencyInfo);
            CurrentBuffer->copyImage(*BlackHoleAttachment->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                *HistoryAttachment->GetImage(), vk::ImageLayout::eTransferDstOptimal, HistoryCopyRegion);

            vk::ImageMemoryBarrier2 PostCopySrcBarrier(
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *BlackHoleAttachment->GetImage(),
                SubresourceRange);

            vk::ImageMemoryBarrier2 PostCopyDstBarrier(
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eFragmentShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *HistoryAttachment->GetImage(),
                SubresourceRange);

            std::array PostCopyBarriers{ PostCopySrcBarrier, PostCopyDstBarrier };
            vk::DependencyInfo PostCopyDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(PostCopyBarriers);

            CurrentBuffer->pipelineBarrier2(PostCopyDependencyInfo);
            //CurrentBuffer.End();

            //_VulkanContext->ExecuteGraphicsCommands(CurrentBuffer);

            //// Record PreBloom rendering commands
            //// ----------------------------------
            //CurrentBuffer = PreBloomCommandBuffers[CurrentFrame];
            //CurrentBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

            vk::ImageMemoryBarrier2 InitPreBloomBarrier(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eGeneral,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *PreBloomAttachment->GetImage(),
                SubresourceRange);

            vk::DependencyInfo PreBloomInitialDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(InitPreBloomBarrier);

            CurrentBuffer->pipelineBarrier2(PreBloomInitialDependencyInfo);

            CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eCompute, PreBloomPipeline);
            CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eCompute, PreBloomPipelineLayout, 0,
                PreBloomShader->GetDescriptorSets(CurrentFrame), {});

            CurrentBuffer->dispatch(WorkgroundX, WorkgroundY, 1);

            vk::ImageMemoryBarrier2 FirstBlurBarrier(
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderWrite,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eGeneral,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *PreBloomAttachment->GetImage(),
                SubresourceRange);

            vk::DependencyInfo FirstBlurDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(FirstBlurBarrier);

            CurrentBuffer->pipelineBarrier2(FirstBlurDependencyInfo);
            //CurrentBuffer.End();
            //_VulkanContext->ExecuteComputeCommands(CurrentBuffer);

            //// Record GaussBlur rendering commands
            //// -----------------------------------
            //CurrentBuffer = GaussBlurCommandBuffers[CurrentFrame];
            //CurrentBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

            vk::ImageMemoryBarrier2 InitGaussBlurBarrier(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eGeneral,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *GaussBlurAttachment->GetImage(),
                SubresourceRange);

            vk::DependencyInfo GaussBlurInitialDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(InitGaussBlurBarrier);

            CurrentBuffer->pipelineBarrier2(GaussBlurInitialDependencyInfo);

            vk::Bool32 bHorizontal = vk::True;

            CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eCompute, GaussBlurPipeline);
            CurrentBuffer->pushConstants(GaussBlurPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0,
                sizeof(vk::Bool32), &bHorizontal);

            CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eCompute, GaussBlurPipelineLayout, 0,
                GaussBlurShader->GetDescriptorSets(CurrentFrame), {});

            CurrentBuffer->dispatch(WorkgroundX, WorkgroundY, 1);

            vk::ImageMemoryBarrier2 CopybackSrcBarrier(
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderWrite,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead,
                vk::ImageLayout::eGeneral,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *GaussBlurAttachment->GetImage(),
                SubresourceRange);

            vk::ImageMemoryBarrier2 CopybackDstBarrier(
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::ImageLayout::eTransferDstOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *PreBloomAttachment->GetImage(),
                SubresourceRange);

            std::array CopybackBarriers{ CopybackSrcBarrier, CopybackDstBarrier };
            vk::DependencyInfo CopybackDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(CopybackBarriers);

            CurrentBuffer->pipelineBarrier2(CopybackDependencyInfo);

            vk::ImageCopy CopybackRegion = vk::ImageCopy()
                .setSrcSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
                .setDstSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
                .setExtent(vk::Extent3D(_WindowSize.width, _WindowSize.height, 1));

            CurrentBuffer->copyImage(*GaussBlurAttachment->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                *PreBloomAttachment->GetImage(), vk::ImageLayout::eTransferDstOptimal, CopybackRegion);

            vk::ImageMemoryBarrier2 ResampleBarrier(
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *PreBloomAttachment->GetImage(),
                SubresourceRange);

            vk::ImageMemoryBarrier2 RewriteBarrier(
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderWrite,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::ImageLayout::eGeneral,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *GaussBlurAttachment->GetImage(),
                SubresourceRange);

            std::array RestoreBarriers{ ResampleBarrier, RewriteBarrier };
            vk::DependencyInfo RerenderDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(RestoreBarriers);

            CurrentBuffer->pipelineBarrier2(RerenderDependencyInfo);

            bHorizontal = vk::False;

            CurrentBuffer->pushConstants(GaussBlurPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0,
                sizeof(vk::Bool32), &bHorizontal);

            CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eCompute, GaussBlurPipelineLayout, 0,
                GaussBlurShader->GetDescriptorSets(CurrentFrame), {});

            CurrentBuffer->dispatch(WorkgroundX, WorkgroundY, 1);

            vk::ImageMemoryBarrier2 BlendSampleBarrier(
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderWrite,
                vk::PipelineStageFlagBits2::eFragmentShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eGeneral,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *GaussBlurAttachment->GetImage(),
                SubresourceRange);

            vk::DependencyInfo BlendSampleDepencencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(BlendSampleBarrier);

            CurrentBuffer->pipelineBarrier2(BlendSampleDepencencyInfo);
            // 7. Blend Pass: 渲染到 SceneColorAttachment
            {
                vk::ImageMemoryBarrier2 sceneColorInitBarrier(
                    vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone,
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
                    vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                    *SceneColorAttachment->GetImage(), SubresourceRange
                );
                CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(sceneColorInitBarrier));

                vk::RenderingInfo sceneRenderingInfo = vk::RenderingInfo()
                    .setRenderArea(vk::Rect2D({ 0, 0 }, _WindowSize))
                    .setLayerCount(1)
                    .setColorAttachments(SceneColorAttachmentInfo);

                CurrentBuffer->beginRendering(sceneRenderingInfo);
                CurrentBuffer->bindVertexBuffers(0, *QuadOnlyVertexBuffer.GetBuffer(), Offset);
                CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, BlendPipeline);
                CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, BlendPipelineLayout, 0, BlendShader->GetDescriptorSets(CurrentFrame), {});
                CurrentBuffer->draw(6, 1, 0, 0);
                CurrentBuffer->endRendering();
            }

            // 8. UI Background Iterative Blur Pass
            // =========================================================================
            {
                // --- 定义模糊迭代次数 ---
                // 3 次迭代会降到 1/8 分辨率再升回来，模糊效果会非常明显
                const int blurPasses = 3;

                // --- 准备 Ping-Pong 指针 ---
                Grt::FColorAttachment* ping = UIBlurPingAttachment.get();
                Grt::FColorAttachment* pong = UIBlurPongAttachment.get();

                // --- 初始屏障 ---
                // SceneColor: ColorAttach -> TransferSrc
                // Ping: Undefined -> TransferDst
                vk::ImageMemoryBarrier2 sceneToSrcBarrier(
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
                    vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead,
                    vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal,
                    vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                    *SceneColorAttachment->GetImage(), SubresourceRange
                );
                vk::ImageMemoryBarrier2 pingToDstBarrier(
                    vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone,
                    vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                    vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                    *ping->GetImage(), SubresourceRange
                );
                std::array initialBarriers = { sceneToSrcBarrier, pingToDstBarrier };
                CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(initialBarriers));

                // --- 第一次降采样 (Full -> Half) ---
                vk::ImageBlit blit = {};
                blit.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
                blit.srcOffsets[1] = vk::Offset3D{ (int32_t)_WindowSize.width, (int32_t)_WindowSize.height, 1 };
                blit.dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
                blit.dstOffsets[1] = vk::Offset3D{ (int32_t)(_WindowSize.width / 2), (int32_t)(_WindowSize.height / 2), 1 };
                CurrentBuffer->blitImage(
                    *SceneColorAttachment->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                    *ping->GetImage(), vk::ImageLayout::eTransferDstOptimal,
                    blit, vk::Filter::eLinear
                );

                // --- 循环降采样 (Ping-Pong) ---
                for (int i = 1; i < blurPasses; ++i)
                {
                    // 屏障: ping(Dst->Src), pong(Undefined/PrevState->Dst)
                    vk::ImageMemoryBarrier2 pingToSrcBarrier(
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead,
                        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
                        vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                        *ping->GetImage(), SubresourceRange
                    );
                    vk::ImageMemoryBarrier2 pongToDstBarrier(
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead, // Pong could have been a src before
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eTransferDstOptimal,
                        vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                        *pong->GetImage(), SubresourceRange
                    );
                    std::array loopBarriers = { pingToSrcBarrier, pongToDstBarrier };
                    CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(loopBarriers));

                    // Blit from ping to pong
                    blit.srcOffsets[1] = vk::Offset3D{ (int32_t)(_WindowSize.width >> i), (int32_t)(_WindowSize.height >> i), 1 };
                    blit.dstOffsets[1] = vk::Offset3D{ (int32_t)(_WindowSize.width >> (i + 1)), (int32_t)(_WindowSize.height >> (i + 1)), 1 };
                    CurrentBuffer->blitImage(
                        *ping->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                        *pong->GetImage(), vk::ImageLayout::eTransferDstOptimal,
                        blit, vk::Filter::eLinear
                    );

                    // Swap for next iteration
                    std::swap(ping, pong);
                }

                // --- 循环升采样 (Ping-Pong back to Half-res) ---
                for (int i = blurPasses - 1; i > 0; --i)
                {
                    // 屏障: ping(Dst->Src), pong(PrevState->Dst)
                    vk::ImageMemoryBarrier2 pingToSrcBarrier(
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead,
                        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
                        vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                        *ping->GetImage(), SubresourceRange
                    );
                    vk::ImageMemoryBarrier2 pongToDstBarrier(
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead,
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eTransferDstOptimal,
                        vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                        *pong->GetImage(), SubresourceRange
                    );
                    std::array loopBarriers = { pingToSrcBarrier, pongToDstBarrier };
                    CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(loopBarriers));

                    // Blit from ping to pong
                    blit.srcOffsets[1] = vk::Offset3D{ (int32_t)(_WindowSize.width >> (i + 1)), (int32_t)(_WindowSize.height >> (i + 1)), 1 };
                    blit.dstOffsets[1] = vk::Offset3D{ (int32_t)(_WindowSize.width >> i), (int32_t)(_WindowSize.height >> i), 1 };
                    CurrentBuffer->blitImage(
                        *ping->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                        *pong->GetImage(), vk::ImageLayout::eTransferDstOptimal,
                        blit, vk::Filter::eLinear
                    );

                    // Swap for next iteration
                    std::swap(ping, pong);
                }

                // --- 最后一次 Blit 到最终的 UIBlurAttachment ---
                vk::ImageMemoryBarrier2 finalSrcBarrier(
                    vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                    vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead,
                    vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
                    vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                    *ping->GetImage(), SubresourceRange
                );
                vk::ImageMemoryBarrier2 finalDstBarrier(
                    vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone,
                    vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                    vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                    *UIBlurAttachment->GetImage(), SubresourceRange
                );
                std::array finalBlitBarriers = { finalSrcBarrier, finalDstBarrier };
                CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(finalBlitBarriers));

                blit.srcOffsets[1] = vk::Offset3D{ (int32_t)(_WindowSize.width / 2), (int32_t)(_WindowSize.height / 2), 1 };
                blit.dstOffsets[1] = vk::Offset3D{ (int32_t)(_WindowSize.width / 2), (int32_t)(_WindowSize.height / 2), 1 };
                CurrentBuffer->blitImage(
                    *ping->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                    *UIBlurAttachment->GetImage(), vk::ImageLayout::eTransferDstOptimal,
                    blit, vk::Filter::eLinear
                );

                // --- Final Barrier for UI Sampling ---
                vk::ImageMemoryBarrier2 uiBlurReadyBarrier(
                    vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                    vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead,
                    vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                    vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                    *UIBlurAttachment->GetImage(), SubresourceRange
                );
                CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(uiBlurReadyBarrier));
            }

            // 9. Copy Scene to Swapchain
            {
                vk::ImageMemoryBarrier2 swapchainCopyDstBarrier(
                    vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone,
                    vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                    vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                    _VulkanContext->GetSwapchainImage(ImageIndex), SubresourceRange
                );
                CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(swapchainCopyDstBarrier));

                vk::ImageCopy copyRegion = {};
                copyRegion.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
                copyRegion.dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
                copyRegion.extent = vk::Extent3D{ _WindowSize.width, _WindowSize.height, 1 };

                CurrentBuffer->copyImage(
                    *SceneColorAttachment->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                    _VulkanContext->GetSwapchainImage(ImageIndex), vk::ImageLayout::eTransferDstOptimal,
                    copyRegion
                );
            }

            // 10. UI Render Pass (on top of Swapchain)
            {
                vk::ImageMemoryBarrier2 swapchainUIBarrier(
                    vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
                    vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal,
                    vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                    _VulkanContext->GetSwapchainImage(ImageIndex), SubresourceRange
                );
                CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(swapchainUIBarrier));

                vk::RenderingAttachmentInfo uiAttachmentInfo = vk::RenderingAttachmentInfo()
                    .setImageView(_VulkanContext->GetSwapchainImageView(ImageIndex))
                    .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                    .setLoadOp(vk::AttachmentLoadOp::eLoad)
                    .setStoreOp(vk::AttachmentStoreOp::eStore);

                vk::RenderingInfo uiRenderingInfo = vk::RenderingInfo()
                    .setRenderArea(vk::Rect2D({ 0, 0 }, _WindowSize))
                    .setLayerCount(1)
                    .setColorAttachments(uiAttachmentInfo);

                CurrentBuffer->beginRendering(uiRenderingInfo);
                _uiRenderer->Render(*CurrentBuffer);
                CurrentBuffer->endRendering();
            }

            // 11. Final Present Barrier
            vk::ImageMemoryBarrier2 PresentBarrier(
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::PipelineStageFlagBits2::eBottomOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ImageLayout::ePresentSrcKHR,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                _VulkanContext->GetSwapchainImage(ImageIndex),
                SubresourceRange);

            vk::DependencyInfo PresentDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(PresentBarrier);

            CurrentBuffer->pipelineBarrier2(PresentDependencyInfo);

            // =========================================================================
            // [新代码块结束]
            // =========================================================================

            CurrentBuffer.End();

            _VulkanContext->SubmitCommandBufferToGraphics(
                *CurrentBuffer, *Semaphores_ImageAvailable[CurrentFrame],
                *Semaphores_RenderFinished[CurrentFrame], *InFlightFences[CurrentFrame]);
            _VulkanContext->PresentImage(*Semaphores_RenderFinished[CurrentFrame]);
        }
        CurrentFrame = (CurrentFrame + 1) % Config::Graphics::kMaxFrameInFlight;

        ProcessInput();
        update();
        // ================= [新增] 处理图片吸积盘的描述符更新 =================
        if (g_DiskStateChanged && g_GlobalSampler)
        {
            g_DiskStateChanged = false;
            auto* AssetManager = Art::FAssetManager::GetInstance();
            auto* PrepassShader = AssetManager->GetAsset<Art::FShader>("BlackHolePrepass");
            auto* CompositeShader = AssetManager->GetAsset<Art::FShader>("BlackHoleComposite");

            Art::FTexture2D* Tex = nullptr;
            if (g_CurrentDiskState == -1)
            {
                Tex = AssetManager->GetAsset<Art::FTexture2D>("NPGSTexture"); // 默认的替位图
            }
            else
            {
                Tex = AssetManager->GetAsset<Art::FTexture2D>(g_DiskTextures[g_CurrentDiskState]);
            }

            if (Tex && PrepassShader && CompositeShader)
            {
                std::vector<vk::DescriptorImageInfo> UpdateImageInfos;
                UpdateImageInfos.push_back(Tex->CreateDescriptorImageInfo(*g_GlobalSampler));

                // 将最新选择的图片更新到 Binding 9 中
                PrepassShader->WriteSharedDescriptors(1, 9, vk::DescriptorType::eCombinedImageSampler, UpdateImageInfos);
                CompositeShader->WriteSharedDescriptors(1, 9, vk::DescriptorType::eCombinedImageSampler, UpdateImageInfos);
            }
        }
        // =====================================================================
    }

    _VulkanContext->WaitIdle();
    Terminate();
}

void FApplication::Terminate()
{
    if (_uiRenderer)
    {
        _uiRenderer->Shutdown();
        _uiRenderer.reset();
    }
    _VulkanContext->WaitIdle();
    glfwDestroyWindow(_Window);
    glfwTerminate();
}


bool FApplication::InitializeWindow()
{
    if (glfwInit() == GLFW_FALSE)
    {
        NpgsCoreError("Failed to initialize GLFW.");
        return false;
    };
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // 注意：全屏模式下透明缓冲通常无效或会导致兼容性问题，根据需求保留
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, true);
    GLFWmonitor* PrimaryMonitor = nullptr;

    if (_bEnableFullscreen)
    {
        // 获取主显示器
        PrimaryMonitor = glfwGetPrimaryMonitor();


        const GLFWvidmode* mode = glfwGetVideoMode(PrimaryMonitor);
        _WindowSize.width = mode->width;
        _WindowSize.height = mode->height;

    }
    // 将 PrimaryMonitor 传给第四个参数
    _Window = glfwCreateWindow(_WindowSize.width, _WindowSize.height, _WindowTitle.c_str(), PrimaryMonitor, nullptr);

    if (_Window == nullptr)
    {
        NpgsCoreError("Failed to create GLFW window.");
        glfwTerminate();
        return false;
    }

    InitializeInputCallbacks();

    std::uint32_t ExtensionCount = 0;
    const char** Extensions = glfwGetRequiredInstanceExtensions(&ExtensionCount);
    if (Extensions == nullptr)
    {
        NpgsCoreError("Failed to get required instance extensions.");
        glfwDestroyWindow(_Window);
        glfwTerminate();
        return false;
    }

    for (std::uint32_t i = 0; i != ExtensionCount; ++i)
    {
        _VulkanContext->AddInstanceExtension(Extensions[i]);
    }

    _VulkanContext->AddDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    vk::Result Result;
    if ((Result = _VulkanContext->CreateInstance()) != vk::Result::eSuccess)
    {
        glfwDestroyWindow(_Window);
        glfwTerminate();
        return false;
    }

    vk::SurfaceKHR Surface;
    if (glfwCreateWindowSurface(_VulkanContext->GetInstance(), _Window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&Surface)) != VK_SUCCESS)
    {
        NpgsCoreError("Failed to create window surface.");
        glfwDestroyWindow(_Window);
        glfwTerminate();
        return false;
    }
    _VulkanContext->SetSurface(Surface);

    if (_VulkanContext->CreateDevice(0) != vk::Result::eSuccess ||
        _VulkanContext->CreateSwapchain(_WindowSize, _bEnableVSync) != vk::Result::eSuccess)
    {
        return false;
    }

    _FreeCamera = std::make_unique<SysSpa::FCamera>(glm::vec3(0.0f, 0.0f, 0.0f), 0.2, 2.5, cfov);
    _FreeCamera->SetCameraMode(true);
    return true;
}

void FApplication::InitializeInputCallbacks()
{
    glfwSetWindowUserPointer(_Window, this);
    glfwSetFramebufferSizeCallback(_Window, &FApplication::FramebufferSizeCallback);

    glfwSetScrollCallback(_Window, &FApplication::ScrollCallback);
    glfwSetMouseButtonCallback(_Window, &FApplication::MouseButtonCallback);
    glfwSetCursorPosCallback(_Window, &FApplication::CursorPosCallback);
    glfwSetKeyCallback(_Window, &FApplication::KeyCallback);
    glfwSetCharCallback(_Window, &FApplication::CharCallback);
}


void FApplication::update()
{
    _FreeCamera->SetRotationSmoothCoefficient(camsmth);
    _FreeCamera->ProcessTimeEvolution(_DeltaTime);
    GeodesicIntegrator::g_CameraCharge = qfm;
    CurrentTime = glfwGetTime();
    _LastDeltaTime = _DeltaTime;
    _DeltaTime = CurrentTime - LastFrameTime;
    RealityTime += _DeltaTime;;
    GameTime += TimeRate * _DeltaTime;
    LastFrameTime = CurrentTime;
    ++FramePerSec;
    FrameCount++;
    if (CurrentTime - PreviousTime >= 1.0)
    {
        glfwSetWindowTitle(_Window, (std::string(_WindowTitle) + " " + std::to_string(FramePerSec)).c_str());
        FramePerSec = 0;
        PreviousTime = CurrentTime;
    }
}
void FApplication::DumpArgsToJson(const std::string& filepath)
{

    std::ofstream out(filepath);
    if (!out.is_open())
    {
        std::cout << "Failed to open file for dumping args!" << std::endl;
        return;
    }

    out << "{\n";

    // ==========================================
    // 导出 FGameArgs
    // ==========================================
    out << "  \"FGameArgs\": {\n";

    // 宏定义：方便打印标量和 glm 变量（用完即抛）
#define DUMP_G(var) out << "    \"" #var "\": " << GameArgs.var << ",\n"
#define DUMP_G_GLM(var) out << "    \"" #var "\": \"" << glm::to_string(glm::vec2(GameArgs.var)) << "\",\n"

    DUMP_G_GLM(Resolution);
    DUMP_G(FovRadians);
    DUMP_G(Time);
    DUMP_G(GameTime);
    DUMP_G(TimeDelta);
    DUMP_G(TimeRate);

#undef DUMP_G
#undef DUMP_G_GLM
    out << "    \"__end__\": 0\n"; // 防止最后一个逗号导致 JSON 解析报错
    out << "  },\n";

    // ==========================================
    // 导出 FBlackHoleArgs
    // ==========================================
    out << "  \"FBlackHoleArgs\": {\n";

#define DUMP_B(var) out << "    \"" #var "\": " << BlackHoleArgs.var << ",\n"
#define DUMP_B_GLM(var) out << "    \"" #var "\": \"" << glm::to_string(BlackHoleArgs.var) << "\",\n"

    // 向量与矩阵
    DUMP_B_GLM(InverseCamRot);
    DUMP_B_GLM(BlackHoleRelativePosRs);
    DUMP_B_GLM(BlackHoleRelativeDiskNormal);
    DUMP_B_GLM(BlackHoleRelativeDiskTangen);
    DUMP_B_GLM(CameraVelocity);

    // 标量
    DUMP_B(DEBUG);
    DUMP_B(Whitehole);
    DUMP_B(InWhichUniverse);
    DUMP_B(Grid);
    DUMP_B(EnableHeatHaze);
    DUMP_B(ObserverMode);
    DUMP_B(UniverseSign);
    DUMP_B(BlackHoleTime);
    DUMP_B(BlackHoleMassSol);
    DUMP_B(Spin);
    DUMP_B(Q2);
    DUMP_B(Mu);
    DUMP_B(AccretionRate);
    DUMP_B(InterRadiusRs);
    DUMP_B(OuterRadiusRs);
    DUMP_B(ThinRs);
    DUMP_B(Hopper);
    DUMP_B(Brightmut);
    DUMP_B(Darkmut);
    DUMP_B(Reddening);
    DUMP_B(Saturation);
    DUMP_B(BlackbodyIntensityExponent);
    DUMP_B(RedShiftColorExponent);
    DUMP_B(RedShiftIntensityExponent);
    DUMP_B(HeatHaze);
    DUMP_B(BackgroundBrightmut);
    DUMP_B(PhotonRingBoost);
    DUMP_B(PhotonRingColorTempBoost);
    DUMP_B(BoostRot);
    DUMP_B(JetRedShiftIntensityExponent);
    DUMP_B(JetBrightmut);
    DUMP_B(JetSaturation);
    DUMP_B(JetShiftMax);
    DUMP_B(BlendWeight);

#undef DUMP_B
#undef DUMP_B_GLM
    out << "    \"__end__\": 0\n";
    out << "  }\n";

    out << "}\n";
    out.close();

    std::cout << "Successfully dumped shader parameters to: " << filepath << std::endl;
}
void FApplication::ProcessInput()
{
    // -----------------------------------------------------------
    // 1. 获取状态 & UI 阻挡判断
    // -----------------------------------------------------------
    ImGuiIO& io = ImGui::GetIO();
    auto& ui_ctx = Npgs::System::UI::UIContext::Get();

    bool bMouseBlocked = io.WantCaptureMouse;
    bool bKeyboardBlocked = io.WantCaptureKeyboard || (ui_ctx.m_focused_element != nullptr);

    // -----------------------------------------------------------
    // 2. 获取当前鼠标位置
    // -----------------------------------------------------------
    double currX, currY;
    glfwGetCursorPos(_Window, &currX, &currY);

    // -----------------------------------------------------------
    // 3. 处理中键：平滑归零摆头 (新增)
    // -----------------------------------------------------------
    bool isMiddleDown = glfwGetMouseButton(_Window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    if (isMiddleDown && !bMouseBlocked)
    {
        _FreeCamera->ResetSway();
    }

    // -----------------------------------------------------------
    // 4. 处理左键：绕轨道旋转中心 (原有逻辑)
    // -----------------------------------------------------------
    bool isLeftDown = glfwGetMouseButton(_Window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    static bool wasLeftDown = false; // 上一帧状态

    static int s_LeftFrameCooldown = 0;
    if (s_LeftFrameCooldown > 0)
    {
        s_LeftFrameCooldown--;
    }

    // [Case A]: 刚按下左键 (Rising Edge)
    if (isLeftDown && !wasLeftDown)
    {
        if (!bMouseBlocked && s_LeftFrameCooldown == 0)
        {
            _bLeftMousePressedInWorld = true;
            _DragStartX = currX;
            _DragStartY = currY;

            _bIsDraggingInWorld = true;
            glfwSetInputMode(_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            _bFirstMouse = true;
        }
    }
    // [Case B]: 刚松开左键 (Falling Edge)
    else if (!isLeftDown && wasLeftDown)
    {
        if (_bLeftMousePressedInWorld)
        {
            _bLeftMousePressedInWorld = false;
            _bIsDraggingInWorld = false;

            // 仅当右键没有在拖动时才恢复鼠标显示，防止左右键冲突
            if (!_bIsDraggingRightInWorld && !g_bHideUIAndMouse)
            {
                glfwSetInputMode(_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }

            glfwSetCursorPos(_Window, _DragStartX, _DragStartY);
            io.MousePos = ImVec2((float)_DragStartX, (float)_DragStartY);

            s_LeftFrameCooldown = 3;
        }
    }
    // [Case C]: 持续按住并拖动
    else if (_bLeftMousePressedInWorld && _bIsDraggingInWorld && s_LeftFrameCooldown == 0)
    {
        if (_bFirstMouse)
        {
            _LastX = currX;
            _LastY = currY;
            _bFirstMouse = false;
        }
        else
        {
            double deltaX = currX - _LastX;
            double deltaY = currY - _LastY;

            _LastX = currX;
            _LastY = currY;

            if (deltaX != 0.0 || deltaY != 0.0)
            {
                _FreeCamera->ProcessMouseMovement(deltaX, deltaY);
            }
        }
    }

    // -----------------------------------------------------------
    // 5. 处理右键：摄像机摆头 (新增逻辑，与左键高度对称)
    // -----------------------------------------------------------
    bool isRightDown = glfwGetMouseButton(_Window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    static bool wasRightDown = false; // 上一帧状态

    static int s_RightFrameCooldown = 0;
    if (s_RightFrameCooldown > 0)
    {
        s_RightFrameCooldown--;
    }

    // [Case A]: 刚按下右键 (Rising Edge)
    if (isRightDown && !wasRightDown)
    {
        if (!bMouseBlocked && s_RightFrameCooldown == 0)
        {
            _bRightMousePressedInWorld = true;
            _DragRightStartX = currX;
            _DragRightStartY = currY;

            _bIsDraggingRightInWorld = true;
            glfwSetInputMode(_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            _bFirstMouseRight = true;
        }
    }
    // [Case B]: 刚松开右键 (Falling Edge)
    else if (!isRightDown && wasRightDown)
    {
        if (_bRightMousePressedInWorld)
        {
            _bRightMousePressedInWorld = false;
            _bIsDraggingRightInWorld = false;

            // 仅当左键没有在拖动时才恢复鼠标显示
            if (!_bIsDraggingInWorld && !g_bHideUIAndMouse)
            {
                glfwSetInputMode(_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }

            glfwSetCursorPos(_Window, _DragRightStartX, _DragRightStartY);
            io.MousePos = ImVec2((float)_DragRightStartX, (float)_DragRightStartY);

            s_RightFrameCooldown = 3;
        }
    }
    // [Case C]: 右键持续按住并拖动
    else if (_bRightMousePressedInWorld && _bIsDraggingRightInWorld && s_RightFrameCooldown == 0)
    {
        if (_bFirstMouseRight)
        {
            _LastRightX = currX;
            _LastRightY = currY;
            _bFirstMouseRight = false;
        }
        else
        {
            double deltaX = currX - _LastRightX;
            double deltaY = currY - _LastRightY;

            _LastRightX = currX;
            _LastRightY = currY;

            if (deltaX != 0.0 || deltaY != 0.0)
            {
                // 调用新增的摆头处理函数
                _FreeCamera->ProcessSwayMovement(deltaX, deltaY);
            }
        }
    }

    // 更新鼠标按键历史状态
    wasLeftDown = isLeftDown;
    wasRightDown = isRightDown;

    // -----------------------------------------------------------
    // 6. 处理滚轮缩放 (消费 Buffer)
    // -----------------------------------------------------------
    // -----------------------------------------------------------
    // 6. 处理滚轮缩放 / 推力控制 (消费 Buffer)
    // -----------------------------------------------------------
    // [新增] 用于持久化存储测地模式下的推力大小，初始设为 1.5


    if (_buffered_scroll_y != 0.0f)
    {
        if (!bMouseBlocked)
        {
            if (g_GeodesicMode)
            {
                // 测地模式：滚轮控制推力（指数增减，每次滚动改变 20%），体验更平滑
                s_GeodesicThrust *= std::pow(1.2f, _buffered_scroll_y);

                std::cout << "[Geodesic Mode] Thrust power adjusted to: " << s_GeodesicThrust << std::endl;
            }
            else
            {
                // 普通模式：滚轮控制相机缩放 (FOV)
                _FreeCamera->ProcessMouseScroll(_buffered_scroll_y);
            }
        }
        _buffered_scroll_y = 0.0f;
    }

    // -----------------------------------------------------------
    // 7. 处理键盘移动
    // -----------------------------------------------------------
    if (!bKeyboardBlocked)
    {
        // 模式切换
        static bool wasTDown = false;
        bool isTDown = glfwGetKey(_Window, GLFW_KEY_T) == GLFW_PRESS;
        if (isTDown && !wasTDown && !g_GeodesicMode)
        {
            _FreeCamera->ProcessModeChange();
        }
        wasTDown = isTDown;

        // -----------------------------------------------------------------
        // 模式切换
        // -----------------------------------------------------------------
        static bool wasGDown = false;
        bool isGDown = glfwGetKey(_Window, GLFW_KEY_G) == GLFW_PRESS;
        if (isGDown && !wasGDown)
        {
            g_GeodesicMode = !g_GeodesicMode;
            if (g_GeodesicMode)
            {
                // === [新增]：进入测地模式时，自动把相机切换到自由模式 (Free Camera) ===
                _FreeCamera->SetCameraMode(false);

                float Rs = 2.0 * abs(BlackHoleArgs.BlackHoleMassSol) * kGravityConstant / pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter;
                glm::vec3 pos = _FreeCamera->GetCameraVector(SysSpa::FCamera::EVectorType::kPosition) / Rs;

                // 获取相机此刻的物理三维坐标速度 (v/c)
                glm::vec3 velo = glm::vec3(BlackHoleArgs.CameraVelocity);

                g_isOutgoing = false; // 初始为 Ingoing
                g_UniverseSign = BlackHoleArgs.UniverseSign;
                GeodesicIntegrator::g_ProperTime = 0.0;
                // 将位置和速度一起传入测地线积分器
                GeodesicIntegrator::InitializeGeodesicState(pos, velo, BlackHoleArgs.Spin * 0.5, sqrt(pow(g_Q, 2.0) + pow(g_P, 2.0)) * 0.5);
            }
        }
        wasGDown = isGDown;
        // ================= [新增] 图片切换循环逻辑 (按下 'o' 键) =================
        static bool wasODown = false;
        bool isODown = glfwGetKey(_Window, GLFW_KEY_O) == GLFW_PRESS;
        if (isODown && !wasODown)
        {
            if (!g_DiskTextures.empty())
            {
                // 第一次按下时记录体积盘原状态
                if (g_CurrentDiskState == -1)
                {
                    g_OriginalThinRs = BlackHoleArgs.ThinRs;
                    g_OriginalHopper = BlackHoleArgs.Hopper;
                }
                g_CurrentDiskState++;

                // 循环到底后恢复体积盘状态
                if (g_CurrentDiskState >= g_DiskTextures.size())
                {
                    g_CurrentDiskState = -1;
                    BlackHoleArgs.ThinRs = g_OriginalThinRs;
                    BlackHoleArgs.Hopper = g_OriginalHopper;
                    BlackHoleArgs.UseImageDisk = 0;
                    BlackHoleArgs.Prepass = 1;
                    g_DiskStateChanged = true;
                }
                else
                {
                    // 隐藏体积盘开启图片盘
                    BlackHoleArgs.ThinRs = 0.0f;
                    BlackHoleArgs.Hopper = 0.0f;
                    BlackHoleArgs.UseImageDisk = 1;
                    BlackHoleArgs.Prepass = 0;
                    g_DiskStateChanged = true;
                }
            }
            else
            {
                std::cout << "[Warning] Assets/Textures/Disk 文件夹下没有找到图片!" << std::endl;
            }
        }
        wasODown = isODown;
        // =========================================================================

        static bool wasPDown = false;
        bool isPDown = glfwGetKey(_Window, GLFW_KEY_P) == GLFW_PRESS;
        if (isPDown && !wasPDown)
        {
            std::string filename = "C:/Users/bcy00/Desktop/python乱七八糟/ShaderArgs_" + std::to_string(FrameCount) + ".json";
            DumpArgsToJson(filename);
        }
        wasPDown = isPDown;
        static bool wasHDown = false;
        bool isHDown = glfwGetKey(_Window, GLFW_KEY_H) == GLFW_PRESS;
        if (isHDown && !wasHDown)
        {
            g_bHideUIAndMouse = !g_bHideUIAndMouse;
            if (g_bHideUIAndMouse)
            {
                glfwSetInputMode(_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            else if (!_bIsDraggingInWorld && !_bIsDraggingRightInWorld)
            {
                glfwSetInputMode(_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
        wasHDown = isHDown;

        static bool wasCtrlAltSDown = false;
        bool isCtrlDown = glfwGetKey(_Window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(_Window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
        bool isAltDown = glfwGetKey(_Window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(_Window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
        bool isSDownLocal = glfwGetKey(_Window, GLFW_KEY_S) == GLFW_PRESS;

        if (isCtrlDown && isAltDown && isSDownLocal && !wasCtrlAltSDown)
        {
            g_bRequestScreenshot = true;
        }
        wasCtrlAltSDown = isCtrlDown && isAltDown && isSDownLocal;

        if (glfwGetKey(_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(_Window, GLFW_TRUE);
        }

        if (!isCtrlDown && !isAltDown)
        {
            if (g_GeodesicMode)
            {
                // === [新增]：测地线模式下的 WASD 推力 (产生四维固有加速度) ===
                glm::vec3 accel_cam(0.0f);
                if (glfwGetKey(_Window, GLFW_KEY_W) == GLFW_PRESS) accel_cam.z -= 1.0f; // Forward
                if (glfwGetKey(_Window, GLFW_KEY_S) == GLFW_PRESS) accel_cam.z += 1.0f; // Back
                if (glfwGetKey(_Window, GLFW_KEY_A) == GLFW_PRESS) accel_cam.x -= 1.0f; // Left
                if (glfwGetKey(_Window, GLFW_KEY_D) == GLFW_PRESS) accel_cam.x += 1.0f; // Right
                if (glfwGetKey(_Window, GLFW_KEY_R) == GLFW_PRESS) accel_cam.y += 1.0f; // Up
                if (glfwGetKey(_Window, GLFW_KEY_F) == GLFW_PRESS) accel_cam.y -= 1.0f; // Down

                if (glm::length(accel_cam) > 0.1f)
                {
                    accel_cam = glm::normalize(accel_cam);

                    // 将相机的局部推力方向请求通过相机的取向转换到标架(世界)空间
                    glm::vec3 accel_ship = glm::conjugate(_FreeCamera->GetOrientation()) * accel_cam;
                    // [修改] 直接使用滚轮调节好的推力大小
                    float thrust = s_GeodesicThrust;
                    GeodesicIntegrator::g_ProperAcceleration[0] = accel_ship.x * thrust;
                    GeodesicIntegrator::g_ProperAcceleration[1] = accel_ship.y * thrust;
                    GeodesicIntegrator::g_ProperAcceleration[2] = accel_ship.z * thrust;
                }
                else
                {
                    GeodesicIntegrator::g_ProperAcceleration[0] = 0.0;
                    GeodesicIntegrator::g_ProperAcceleration[1] = 0.0;
                    GeodesicIntegrator::g_ProperAcceleration[2] = 0.0;
                }
            }
            else
            {
                // === 原有的非物理自由漫游模式(魔法移动) ===
                if (glfwGetKey(_Window, GLFW_KEY_W) == GLFW_PRESS)
                    _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kForward);
                if (glfwGetKey(_Window, GLFW_KEY_S) == GLFW_PRESS)
                    _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kBack);
                if (glfwGetKey(_Window, GLFW_KEY_A) == GLFW_PRESS)
                    _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kLeft);
                if (glfwGetKey(_Window, GLFW_KEY_D) == GLFW_PRESS)
                    _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kRight);
                if (glfwGetKey(_Window, GLFW_KEY_R) == GLFW_PRESS)
                    _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kUp);
                if (glfwGetKey(_Window, GLFW_KEY_F) == GLFW_PRESS)
                    _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kDown);
            }

            // Roll 旋转（修改飞船的横滚姿态，无论哪种模式都允许生效）
            if (glfwGetKey(_Window, GLFW_KEY_Q) == GLFW_PRESS)
                _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kRollLeft);
            if (glfwGetKey(_Window, GLFW_KEY_E) == GLFW_PRESS)
                _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kRollRight);
        }
    }
}

void FApplication::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    auto* App = static_cast<FApplication*>(glfwGetWindowUserPointer(window));
    if (App)
    {
        App->HandleMouseButton(button, action, mods);
    }
}

void FApplication::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto* App = static_cast<FApplication*>(glfwGetWindowUserPointer(window));
    if (App)
    {
        App->HandleKey(key, scancode, action, mods);
    }
}

void FApplication::CharCallback(GLFWwindow* window, unsigned int codepoint)
{
    auto* App = static_cast<FApplication*>(glfwGetWindowUserPointer(window));
    if (App)
    {
        App->HandleChar(codepoint);
    }
}

void FApplication::FramebufferSizeCallback(GLFWwindow* Window, int Width, int Height)
{
    auto* App = reinterpret_cast<FApplication*>(glfwGetWindowUserPointer(Window));
    if (App) App->HandleFramebufferSize(Width, Height);
}

void FApplication::CursorPosCallback(GLFWwindow* window, double posX, double posY)
{
    auto* App = static_cast<FApplication*>(glfwGetWindowUserPointer(window));
    if (App)
    {
        App->HandleCursorPos(posX, posY);
    }
}

void FApplication::ScrollCallback(GLFWwindow* Window, double OffsetX, double OffsetY)
{
    auto* App = reinterpret_cast<FApplication*>(glfwGetWindowUserPointer(Window));
    if (App) App->HandleScroll(OffsetX, OffsetY);
}


// Application.cpp

void FApplication::HandleMouseButton(int button, int action, int mods)
{

}

void FApplication::HandleKey(int key, int scancode, int action, int mods)
{

}

void FApplication::HandleChar(unsigned int codepoint)
{

}

void FApplication::HandleFramebufferSize(int Width, int Height)
{
    if (Width == 0 || Height == 0) return;

    _WindowSize.width = Width;
    _WindowSize.height = Height;
    _VulkanContext->WaitIdle();
    _VulkanContext->RecreateSwapchain();

    // 可以在这里处理其他需要感知窗口大小变化的逻辑（如 UI 布局更新）
}
// Application.cpp

void FApplication::HandleCursorPos(double posX, double posY)
{

}
void FApplication::HandleScroll(double OffsetX, double OffsetY)
{
    _buffered_scroll_y += (float)OffsetY;
}
// 添加调试 UI 渲染函数
#include <vector>
#include <cmath>
// 确保引入了 ImGui
// #include "imgui.h"
#include "imgui.h"
#include <cmath>
#include <vector>
// =========================================================================
// [新增] 轨迹记录数据结构 (基于长度保留)
// =========================================================================
// =========================================================================
// [修改] 轨迹记录数据结构 (增加时间与坐标系状态)
// =========================================================================
// =========================================================================
// [修改] 轨迹记录数据结构 (增加宇宙索引)
// =========================================================================
struct FTrajectoryPoint
{
    glm::vec3 Pos;
    float UniverseSign;
    float Odometer;
    double t_ks;
    bool isOut;
    int UnivIndex;  // [新增] 记录录入时的宇宙层级索引
};
static std::deque<FTrajectoryPoint> g_TrajectoryHistory;
static float g_TotalOdometer = 0.0f;

void FApplication::RenderDebugUI()
{
    double M_PI = 3.14159265358979323846;
    // ==========================================
// [新增] Kerr-Schild 到 Penrose 图的严格映射工具
// ==========================================
   // 如果你已经有全局的 ImGui::Begin()，请确保这里的名字不冲突
    ImGui::Begin("Black Hole Topology Map", nullptr, ImGuiWindowFlags_NoScrollbar);

    // 1. 获取基础物理参数与坐标 (按 Rs 归一化)
    float Rs = 2.0f * std::abs(BlackHoleArgs.BlackHoleMassSol) * kGravityConstant / std::pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter;
    if (Rs < 1e-6f) Rs = 1.0f; // 防御性除零

    float M = 0.5f; // 以 Rs 为单位，M 恒为 0.5
    float a = BlackHoleArgs.Spin * M;
    float Q = sqrt(pow(g_Q, 2.0) + pow(g_P, 2.0)) * M;
    float a2 = a * a;
    float Q2 = Q * Q;

    // 获取相机位置、朝向和速度矢量，并转化到 Rs 单位空间
    // 获取相机位置、朝向和速度矢量，并转化到 Rs 单位空间
        // 获取相机位置、朝向和速度矢量，并转化到 Rs 单位空间
    // 获取相机位置、朝向和速度矢量，并转化到 Rs 单位空间
    glm::vec3 camPos;
    glm::vec3 camDir;
    glm::vec3 camVel;
    float physical_speed = 0.0f; // [新增] 用于保存真实且平滑的物理速度

    if (g_GeodesicMode)
    {
        // 测地线数组已经是基于 Rs 的无量纲单位
        camPos = glm::vec3(g_GeoState[0], g_GeoState[1], g_GeoState[2]);
        // 提取平行输运并被鼠标旋转后的前向四维向量的空间分量
        camDir = glm::vec3(BlackHoleArgs.ie3_up.x, BlackHoleArgs.ie3_up.y, BlackHoleArgs.ie3_up.z);
        if (glm::length(camDir) > 1e-6f) camDir = glm::normalize(camDir);

        // 提取 4-速度的空间部分 (仅保留用于小地图上绘制那条绿色的方向线)
        camVel = glm::vec3(g_GeoState[4], g_GeoState[5], g_GeoState[6]);

        // ========================================================
        // [完美修正] 计算相对于全局静态观者 (Static Observer) 的物理速度
        // ========================================================
        double g_down[4][4] = { 0.0 }, g_up[4][4] = {0.0}, dummy_r;
        GeodesicIntegrator::ComputeMetric(g_GeoState, a, Q, 1.0, BlackHoleArgs.UniverseSign, g_isOutgoing, g_down, g_up, dummy_r);

        // 计算协变时间分量 U_t (物理意义为静态观者测量的能量 E = -U_t)。
        // 核心亮点：由于 t 方向是全局 Killing 向量，U_t 在 Ingoing/Outgoing 
        // 坐标切换时是天然的几何不变量！这保证了速度读数的绝对平滑。
        double U_t = g_down[3][0] * g_GeoState[4] +
            g_down[3][1] * g_GeoState[5] +
            g_down[3][2] * g_GeoState[6] +
            g_down[3][3] * g_GeoState[7];

        // KS 度规下 g_tt = -1 + f
        double g_tt = g_down[3][3];

        // 防止除零 (当粒子能量趋于 0 时)
        double U_t_sq = std::fmax(1e-12, U_t * U_t);

        // 相对静态观者的速度公式推导：
        // 洛伦兹因子 gamma = -U_t / sqrt(|g_tt|) 
        // 代入 v = sqrt(1 - 1/gamma^2) 化简得: v^2 = 1 + g_tt / U_t^2
        //
        // 物理表现：
        // 1. 能层外 (g_tt < 0)：正常区域，公式保证 v < 1
        // 2. 静界表面 (g_tt = 0)：静态观者自身以光速逃逸，测得一切速度为 v = 1
        // 3. 能层内 / 视界内 (g_tt > 0)：静态观者变为类空(快子)，测得 v > 1 (超光速)
        double v_sq = 1.0 + (g_tt / U_t_sq);

        // max(0, v_sq) 防止在正常空间因极端浮点误差产生微小的负值
        physical_speed = static_cast<float>(std::sqrt(std::fmax(0.0, v_sq)));
    }
    else
    {
        camPos = _FreeCamera->GetCameraVector(System::Spatial::FCamera::EVectorType::kPosition) / Rs;
        camDir = _FreeCamera->GetCameraVector(System::Spatial::FCamera::EVectorType::kFront);

        // 自由非物理模式下，速度就是用操作强行设定的空间位移速度
        camVel = glm::vec3(BlackHoleArgs.CameraVelocity);
        physical_speed = glm::length(camVel);
    }
    // ==========================================
    // [新增] UI 数据面板 (数值坐标与加速度面板)
    // ==========================================
    // 计算 BL 半径 r (用于显示)
    float r_ui_b = (camPos.x * camPos.x + camPos.y * camPos.y + camPos.z * camPos.z) - a2;
    float r_ui_c = a2 * camPos.y * camPos.y;
    float r_ui2 = 0.5f * (r_ui_b + std::sqrt(r_ui_b * r_ui_b + 4.0f * r_ui_c));
    float r_ui = std::sqrt(std::fmax(0.0f, r_ui2)) * BlackHoleArgs.UniverseSign;

    // ==========================================
    // [新增] 计算真正的 Boyer-Lindquist 无穷远时钟时 t_BL
    // ==========================================
    double t_BL = BlackHoleArgs.BlackHoleTime; // 默认漫游模式直接使用
    if (g_GeodesicMode)
    {
        double Delta = r_ui * r_ui - 2.0 * M * r_ui + a2 + Q2;
        double abs_Delta_safe = std::fmax(std::abs(Delta), 1e-16);
        double delta_disc = M * M - a2 - Q2;

        double F_r = 0.0; // F_r = 2 * \int (2Mr - Q^2)/Delta dr
        if (delta_disc > 1e-16)
        {
            double K = std::sqrt(delta_disc);
            double frac = std::abs(r_ui - (M + K)) / std::fmax(std::abs(r_ui - (M - K)), 1e-16);
            F_r = 2.0 * M * std::log(abs_Delta_safe) + ((2.0 * M * M - Q2) / K) * std::log(std::fmax(frac, 1e-16));
        }
        else if (delta_disc < -1e-16)
        {
            double K = std::sqrt(-delta_disc);
            double atan_arg = std::atan((r_ui - M) / K);
            F_r = 2.0 * M * std::log(abs_Delta_safe) + (2.0 * (2.0 * M * M - Q2) / K) * atan_arg;
        }
        else // 极端黑洞
        {
            double rM = r_ui - M;
            double safe_rM = (rM >= 0 ? 1.0 : -1.0) * std::fmax(std::abs(rM), 1e-16);
            F_r = 4.0 * M * std::log(std::fmax(std::abs(rM), 1e-16)) - 2.0 * (2.0 * M * M - Q2) / safe_rM;
        }

        // KS 时 \tilde{t} 与 BL 时 t 的关系：
        // Ingoing KS:  t = \tilde{t} - 0.5 * F_r
        // Outgoing KS: t = \tilde{t} + 0.5 * F_r
        double dir = g_isOutgoing ? 1.0 : -1.0;
        t_BL = g_GeoState[3] + dir * 0.5 * F_r;
    }

    // ==========================================
// [新增] 计算真正的 Boyer-Lindquist 无穷远时钟时 t_BL
// ==========================================
// ... (你原来的 t_BL 计算代码保持不变) ...

    ImGui::Text("--- Coordinates & Physics ---");
    // ========================================================
    // [新增] 4-向量与坐标系状态显示 (仅在物理测地线模式下有意义)
    // ========================================================
    if (g_GeodesicMode)
    {
        // 1. 判断并高亮显示当前处于哪个坐标系 (Chart)
        const char* chartName = g_isOutgoing ? "Outgoing Kerr-Schild (White Hole/Escape)"
            : "Ingoing Kerr-Schild (Black Hole/Fall)";
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[Chart]: %s", chartName);
        // 2. 显示 4-坐标 X^\mu = (t, x, y, z)
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.8f, 1.0f), "4-Position X^mu (t, x, y, z):");
        ImGui::Text("  %.5f,  %.4f, %.4f, %.4f",
            g_GeoState[3], g_GeoState[0], g_GeoState[1], g_GeoState[2]);
        // 3. 显示 4-速度 U^\mu = (U^t, U^x, U^y, U^z)
        ImGui::TextColored(ImVec4(0.8f, 0.5f, 1.0f, 1.0f), "4-Velocity U^mu (U^t, U^x, U^y, U^z):");
        ImGui::Text("  %.5f,  %.5f, %.5f, %.5f",
            g_GeoState[7], g_GeoState[4], g_GeoState[5], g_GeoState[6]);

        ImGui::Separator();
    }
    ImGui::Text("t (BL): %.5f Rs/c", t_BL);

    // ========================================================
    // [新增] 坐标时与固有时的差值 (t - tau) 滚动数据曲线
    // ========================================================
    if (g_GeodesicMode)
    {
        // 【注意】请将下面的 GameArgs.ProperTime 替换为你引擎中实际记录“固有时间”的变量
        // 通常在测地线积分器中会有一个累加的 tau (或 affine parameter)
        double tau = GeodesicIntegrator::g_ProperTime; // <--- 修改为你的实际变量名
        float time_dilation_diff = static_cast<float>(t_BL - tau);

        // 静态变量用于维护图表的环形缓冲区 (Task Manager style)
        const int GRAPH_SAMPLES = 200;  // 采样点数量
        static float s_TimeDiffHistory[GRAPH_SAMPLES] = { 0 };
        static int s_HistoryOffset = 0;
        static double s_LastGraphUpdateTime = 0;
        static bool s_IsFirstFrame = true;

        // 初始化：防止第一帧时因为默认0导致图表Y轴缩放极其夸张
        if (s_IsFirstFrame)
        {
            for (int i = 0; i < GRAPH_SAMPLES; ++i)
                s_TimeDiffHistory[i] = time_dilation_diff;
            s_IsFirstFrame = false;
        }

        // 控制采样刷新率 (例如每秒 60 次采样，避免图表滚动过快)
        double currentTime = ImGui::GetTime();
        if (currentTime - s_LastGraphUpdateTime > (1.0 / 60.0))
        {
            s_TimeDiffHistory[s_HistoryOffset] = time_dilation_diff;
            s_HistoryOffset = (s_HistoryOffset + 1) % GRAPH_SAMPLES;
            s_LastGraphUpdateTime = currentTime;
        }

        // 动态计算 Y 轴的 Min 和 Max 以实现任务管理器的自适应缩放效果
        float min_val = FLT_MAX;
        float max_val = -FLT_MAX;
        for (int i = 0; i < GRAPH_SAMPLES; ++i)
        {
            if (s_TimeDiffHistory[i] < min_val) min_val = s_TimeDiffHistory[i];
            if (s_TimeDiffHistory[i] > max_val) max_val = s_TimeDiffHistory[i];
        }
        // 给图表上下增加一点点 padding 留白，防止直线贴在边缘
        float padding = (max_val - min_val) * 0.1f;
        if (padding < 1e-4f) padding = 0.1f; // 防除零

        // 绘制图表
        char overlay_text[64];
        snprintf(overlay_text, sizeof(overlay_text), "t - tau: %.4f", time_dilation_diff);

        // Push 颜色让图表更有科幻感（可选）
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.15f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.0f, 0.8f, 1.0f, 1.0f));

        ImGui::PlotLines("##TimeDilationGraph",
            s_TimeDiffHistory,
            GRAPH_SAMPLES,
            s_HistoryOffset,
            overlay_text,
            min_val - padding,
            max_val + padding,
            ImVec2(ImGui::GetContentRegionAvail().x, 60.0f)); // 高度为60

        ImGui::PopStyleColor(2);

        ImGui::Text("Proper Time (tau): %.5f", tau);
    }
    // ========================================================

    ImGui::Text("r (BL): %.5f Rs", r_ui);
    ImGui::Text("x: %.4f | y: %.4f | z: %.4f (Rs)", camPos.x, camPos.y, camPos.z);
    float zenithAngleDeg = (glm::length(camPos) > 0.0f) ? glm::degrees(acosf(camPos.y / glm::length(camPos))) : 0.0f;
    ImGui::Text("Zenith Angle: %.4f", zenithAngleDeg);
    ImGui::Text("Spin (a*): %.4f | Charge (Q*): %.4f", BlackHoleArgs.Spin, sqrt(pow(g_Q, 2.0) + pow(g_P, 2.0)));
    ImGui::Text("Velocity: %.6f c", physical_speed);

    if (g_GeodesicMode)
    {
        ImGui::Separator();
        ImGui::Text("--- Propulsion ---");

        // 提取固有加速度
        double ax = GeodesicIntegrator::g_ProperAcceleration[0];
        double ay = GeodesicIntegrator::g_ProperAcceleration[1];
        double az = GeodesicIntegrator::g_ProperAcceleration[2];
        double A_code = s_GeodesicThrust;

        ImGui::Text("Proper Accel: %.4f", A_code);

        if (A_code > 1e-6)
        {
            // 在狭义相对论中，恒定固有加速度下达到速度 v 需要的固有时间为: tau = (c/A) * artanh(v/c)
            // artanh(0.1) ≈ 0.100335
            double dtau_code = 0.100335 / A_code;

            // 将代码单位时间转换回秒 (物理公式: 1单位时间 = Rs_meters / c )
            double Rs_meters = Rs * kLightYearToMeter;
            double tau_phys_sec = dtau_code * (Rs_meters / kSpeedOfLight);

            // 计算玩家在当前倍率(TimeRate)下流逝的现实时间
            double timeTo01c_sec = tau_phys_sec / std::fmax(1e-9f, GameArgs.TimeRate);

            if (timeTo01c_sec < 60.0) ImGui::Text("Real time to 0.1c: %.2f sec", timeTo01c_sec);
            else if (timeTo01c_sec < 3600.0) ImGui::Text("Real time to 0.1c: %.2f min", timeTo01c_sec / 60.0);
            else if (timeTo01c_sec < 86400.0) ImGui::Text("Real time to 0.1c: %.2f hours", timeTo01c_sec / 3600.0);
            else if (timeTo01c_sec < 31536000.0) ImGui::Text("Real time to 0.1c: %.2f days", timeTo01c_sec / 86400.0);
            else ImGui::Text("Real time to 0.1c: %.2f years", timeTo01c_sec / 31536000.0);
        }
        else
        {
            ImGui::Text("Real time to 0.1c: N/A (No thrust)");
        }
        // =========================================================================
// [新增] 瞬间变速功能：应用给定快度(Rapidity)的洛伦兹推演(Lorentz Boost)
// =========================================================================
        ImGui::Separator();
        ImGui::Text("--- Instantaneous Boost (Rapidity) ---");
        static float s_TargetRapidity = 0.0f;

        ImGui::InputFloat("Add Rapidity (dy)", &s_TargetRapidity, 0.1f, 1.0f, "%.4f");

        // 我们提供两种加速方式：沿视线、沿轨道切向
        bool bBoostLook = ImGui::Button("Boost along Look Dir (Forward)");
        ImGui::SameLine();
        bool bBoostTangent = ImGui::Button("Boost along Tangent (Velocity)");

        if (bBoostLook || bBoostTangent)
        {
            double y = s_TargetRapidity;
            double cosh_y = std::cosh(y);
            double sinh_y = std::sinh(y);

            // 获取当前度规，用于计算内积
            double g_down[4][4], g_up[4][4], dummy_r;
            double a = BlackHoleArgs.Spin * 0.5;
            double Q = sqrt(pow(g_Q, 2.0) + pow(g_P, 2.0)) * 0.5;
            GeodesicIntegrator::ComputeMetric(g_GeoState, a, Q, 1.0, g_UniverseSign, g_isOutgoing, g_down, g_up, dummy_r);

            auto dot = [&](const double* A, const double* B)
            {
                double sum = 0.0;
                for (int i = 0; i < 4; ++i)
                    for (int j = 0; j < 4; ++j)
                        sum += g_down[i][j] * A[i] * B[j];
                return sum;
            };

            double old_U[4] = { g_GeoState[4], g_GeoState[5], g_GeoState[6], g_GeoState[7] };
            double E_boost[4] = { 0.0, 0.0, 0.0, 0.0 };

            if (bBoostLook)
            {
                // 【方式一：沿视线前方】提取相机局部 -Z 轴
                glm::mat4 headRot = glm::mat4_cast(glm::conjugate(_FreeCamera->GetOrientation()));
                for (int j = 0; j < 3; ++j)
                {
                    double c_j = -headRot[2][j];
                    for (int i = 0; i < 4; ++i) E_boost[i] += c_j * g_GeoState[8 + 4 * j + i];
                }
            }
            else if (bBoostTangent)
            {
                // 【方式二：沿轨道切向】将纯空间速度投影到正交平面上
                double D[4] = { g_GeoState[4], g_GeoState[5], g_GeoState[6], 0.0 };
                double dot_DU = dot(D, old_U);

                // D_perp = D + (D·U)U (由于 U·U = -1)
                for (int i = 0; i < 4; ++i) E_boost[i] = D[i] + dot_DU * old_U[i];

                double norm2 = dot(E_boost, E_boost);
                if (norm2 > 1e-12)
                {
                    double inv_norm = 1.0 / std::sqrt(norm2);
                    for (int i = 0; i < 4; ++i) E_boost[i] *= inv_norm;
                }
                else
                {
                    // 若完全静止(如下落初始状态)，切向退化为视线向
                    glm::mat4 headRot = glm::mat4_cast(glm::conjugate(_FreeCamera->GetOrientation()));
                    for (int j = 0; j < 3; ++j)
                    {
                        double c_j = -headRot[2][j];
                        for (int i = 0; i < 4; ++i) E_boost[i] += c_j * g_GeoState[8 + 4 * j + i];
                    }
                }
            }

            // 1. 主动洛伦兹变换：更新 4-速度 U' = cosh(y) U + sinh(y) E_boost
            for (int i = 0; i < 4; ++i)
            {
                g_GeoState[4 + i] = cosh_y * old_U[i] + sinh_y * E_boost[i];
            }

            // 2. 主动洛伦兹变换：更新3个空间标架 E_j'，保持与新 U' 严格正交
            // 任意正交基矢的解析变换为：E_j' = E_j + (cosh(y) - 1)(E_j·E_boost)E_boost + sinh(y)(E_j·E_boost)U
            for (int j = 0; j < 3; ++j)
            {
                double Ej[4] = { g_GeoState[8 + 4 * j], g_GeoState[8 + 4 * j + 1], g_GeoState[8 + 4 * j + 2], g_GeoState[8 + 4 * j + 3] };
                double dot_Ej_Eboost = dot(Ej, E_boost);

                for (int i = 0; i < 4; ++i)
                {
                    g_GeoState[8 + 4 * j + i] += (cosh_y - 1.0) * dot_Ej_Eboost * E_boost[i] + sinh_y * dot_Ej_Eboost * old_U[i];
                }
            }

            // 3. 重新应用 Gram-Schmidt 过程稳定正交性
            GeodesicIntegrator::GramSchmidt(g_GeoState, a, Q, 1.0, g_UniverseSign, g_isOutgoing);
        }
        // =========================================================================
    }
    ImGui::Separator();
    // ==========================================
    // ==========================================
    // 2. 记录轨迹 (基于距离采样与总里程计算)
    // ==========================================
    const float MAX_TRAIL_LENGTH = 600.0f; // 轨迹最大保留长度 (Rs)
    bool shouldRecord = false;
    float stepDist = 0.0f;

    if (g_TrajectoryHistory.empty())
    {
        shouldRecord = true;
    }
    else
    {
        const auto& lastPt = g_TrajectoryHistory.back();
        stepDist = glm::distance(camPos, lastPt.Pos);

        // A. 如果发生宇宙跨越，立刻记录以切断连线，且这段突变不计入物理里程
        if (BlackHoleArgs.UniverseSign != lastPt.UniverseSign)
        {
            shouldRecord = true;
            stepDist = 0.0f;
        }
        // B. 如果移动超过了 0.5 Rs
        else if (stepDist > 0.5f)
        {
            shouldRecord = true;
        }
        // C. 或者距上次记录已经有了微小位移(防止超低速时太久不更新)
        else
        {
            static float s_LastRecordTime = 0.0f;
            float currentTime = glfwGetTime();
            if (currentTime - s_LastRecordTime > 0.1f && stepDist > 0.01f)
            {
                shouldRecord = true;
                s_LastRecordTime = currentTime;
            }
        }
    }

    if (shouldRecord)
    {
        g_TotalOdometer += stepDist;
        double current_t_ks = g_GeodesicMode ? g_GeoState[3] : BlackHoleArgs.BlackHoleTime;
        bool current_isOut = g_GeodesicMode ? g_isOutgoing : false;

        // [修改] 将当前的 InWhichUniverse 传入
        g_TrajectoryHistory.push_back({ camPos, BlackHoleArgs.UniverseSign, g_TotalOdometer, current_t_ks, current_isOut, BlackHoleArgs.InWhichUniverse });
    }
    // 清理队列：剔除超过最大长度阈值的尾部旧数据
    while (!g_TrajectoryHistory.empty() &&
        (g_TotalOdometer - g_TrajectoryHistory.front().Odometer) > MAX_TRAIL_LENGTH)
    {
        g_TrajectoryHistory.pop_front();
    }
    // ==========================================

    // 判断所在宇宙状态与裸奇点
    bool isAntiverse = (BlackHoleArgs.UniverseSign < 0.0f);
    float delta_discriminant = M * M - a2 - Q2;
    bool isNakedSingularity = (delta_discriminant < 0.0f);
    float r_outer = isNakedSingularity ? 0.0f : M + std::sqrt(delta_discriminant);
    float r_inner = isNakedSingularity ? 0.0f : M - std::sqrt(delta_discriminant);

    // 获取画布信息
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
    if (canvas_sz.x < 100.0f) canvas_sz.x = 100.0f;
    if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;

    // 绘制背景颜色
    ImU32 bgColor = isAntiverse ? IM_COL32(40, 10, 15, 255) : IM_COL32(15, 20, 30, 255);
    draw_list->AddRectFilled(canvas_p0, ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y), bgColor);

    // 划分为左右两个视图
    // 划分为左、中、右三个视图 [修改部分]
    float thirdWidth = canvas_sz.x / 3.0f;
    float leftPadding = 30.0f;
    ImVec2 centerSide = ImVec2(canvas_p0.x + leftPadding, canvas_p0.y + canvas_sz.y * 0.5f);
    ImVec2 centerTop = ImVec2(canvas_p0.x + thirdWidth * 1.5f, canvas_p0.y + canvas_sz.y * 0.5f);

    // Penrose 图由于是上下延伸的，我们将它的视觉中心 (tau = PI) 放在偏下方一点
    ImVec2 centerPen = ImVec2(canvas_p0.x + thirdWidth * 2.5f, canvas_p0.y + canvas_sz.y * 0.6f);
    // 动态缩放逻辑
    float camDist = glm::length(camPos);
    float displayRadius = std::fmax(1.5f, camDist * 1.2f);
    float scale = std::min(thirdWidth, canvas_sz.y) / (2.0f * displayRadius);

    // Penrose图缩放因子：保证总高度 3*PI 能装进面板
    float scalePen = std::min(thirdWidth * 0.45f / M_PI, canvas_sz.y * 0.45f / (1.5f * M_PI));
    // 坐标转换 Lambda 工具
    auto ToSideScreen = [&](float x, float y) -> ImVec2 { return ImVec2(centerSide.x + x * scale, centerSide.y - y * scale); };
    auto ToTopScreen = [&](float x, float z) -> ImVec2 { return ImVec2(centerTop.x + x * scale, centerTop.y - z * scale); };
    // Penrose 坐标转换：以 tau = PI 作为中心基准点高度
    // 绘制辅助网格和标题
    ImU32 axisColor = IM_COL32(100, 100, 100, 150);
    draw_list->AddLine(ImVec2(centerSide.x, centerSide.y), ImVec2(canvas_p0.x + thirdWidth, centerSide.y), axisColor, 1.0f);
    draw_list->AddLine(ImVec2(centerSide.x, canvas_p0.y), ImVec2(centerSide.x, canvas_p0.y + canvas_sz.y), axisColor, 1.0f);
    draw_list->AddLine(ImVec2(canvas_p0.x + thirdWidth, centerTop.y), ImVec2(canvas_p0.x + thirdWidth * 2.0f, centerTop.y), axisColor, 1.0f);
    draw_list->AddLine(ImVec2(centerTop.x, canvas_p0.y), ImVec2(centerTop.x, canvas_p0.y + canvas_sz.y), axisColor, 1.0f);

    // 面板分隔线
    draw_list->AddLine(ImVec2(canvas_p0.x + thirdWidth, canvas_p0.y), ImVec2(canvas_p0.x + thirdWidth, canvas_p0.y + canvas_sz.y), IM_COL32(200, 200, 200, 255), 2.0f);
    draw_list->AddLine(ImVec2(canvas_p0.x + thirdWidth * 2.0f, canvas_p0.y), ImVec2(canvas_p0.x + thirdWidth * 2.0f, canvas_p0.y + canvas_sz.y), IM_COL32(200, 200, 200, 255), 2.0f);
    draw_list->AddText(ImVec2(centerSide.x + 10, canvas_p0.y + 10), IM_COL32(255, 255, 255, 255), "Meridian (Y-X) Plane");
    draw_list->AddText(ImVec2(canvas_p0.x + thirdWidth + 10, canvas_p0.y + 10), IM_COL32(255, 255, 255, 255), "Top-Down (X-(-Z)) Plane");
    draw_list->AddText(ImVec2(canvas_p0.x + thirdWidth * 2.0f + 10, canvas_p0.y + 10), IM_COL32(255, 255, 255, 255), "Carter-Penrose Diagram");
    std::string univText = !isAntiverse ? "Status: r > 0 (Universe)" : "Status: r < 0 (Antiverse)";
    draw_list->AddText(ImVec2(centerSide.x + 10, canvas_p0.y + 30), !isAntiverse ? IM_COL32(200, 255, 200, 255) : IM_COL32(255, 150, 150, 255), univText.c_str());
    // 图例说明
    draw_list->AddText(ImVec2(centerSide.x + 10, canvas_p0.y + 50), IM_COL32(255, 255, 0, 255), "Yellow: Look Dir");
    draw_list->AddText(ImVec2(centerSide.x + 10, canvas_p0.y + 65), IM_COL32(0, 255, 0, 255), "Green: Velocity");
    draw_list->AddText(ImVec2(centerSide.x + 10, canvas_p0.y + 80), IM_COL32(0, 200, 255, 255), "Cyan: Trajectory (600 Rs)");

    ImU32 alphaStandard = isAntiverse ? 25 : 255;
    ImU32 alphaErgo = isAntiverse ? 15 : 200;
    ImU32 alphaCTC = isAntiverse ? 255 : 25;

    ImU32 colOuterHorizon = IM_COL32(255, 100, 100, alphaStandard);
    ImU32 colInnerHorizon = IM_COL32(100, 150, 255, alphaStandard);
    ImU32 colErgosphere = IM_COL32(150, 255, 150, alphaErgo);
    ImU32 colInnerErgo = IM_COL32(255, 200, 50, alphaErgo);
    ImU32 colCTC = IM_COL32(200, 100, 255, alphaCTC);
    ImU32 colSingularity = IM_COL32(255, 0, 255, 255);

    auto GetCTCRoots = [&](float cosT, float sinT, float& r_out, float& r_in) -> bool
    {
        auto G = [&](float r)
        {
            float r2 = r * r;
            return (r2 + a2) * (r2 + a2 * cosT * cosT) + a2 * sinT * sinT * (2.0f * M * r - Q2);
        };
        float r_start = 0.0f, r_end = -10.0f;
        int steps = 200;
        float dr = (r_end - r_start) / steps;
        std::vector<float> roots;
        float prev_G = G(r_start);

        if (std::abs(prev_G) < 1e-5f)
        {
            roots.push_back(0.0f);
            prev_G = G(r_start + dr * 0.1f);
        }
        for (int k = 1; k <= steps; ++k)
        {
            float r = r_start + k * dr;
            float curr_G = G(r);
            if (curr_G * prev_G < 0.0f)
            {
                roots.push_back(r - dr * curr_G / (curr_G - prev_G));
            }
            prev_G = curr_G;
        }
        if (roots.size() >= 2)
        {
            r_out = roots[0]; r_in = roots.back();
            return true;
        }
        else if (roots.size() == 1)
        {
            r_out = 0.0f; r_in = roots[0];
            return true;
        }
        return false;
    };

    // 4. 计算和绘制几何边界
    const int segments = 511;
    std::vector<ImVec2> sideOutPts, sideInPts;
    ImVec2 prev_ergo_out, prev_ergo_in, prev_ctc_out, prev_ctc_in;
    bool prev_ergo_valid = false, prev_ctc_valid = false;

    for (int i = 0; i <= segments; ++i)
    {
        // 限制 theta 范围只乘 PI (即 0 到 180 度)，只生成右半面数据
        float theta = (static_cast<float>(i) / segments) * 3.14159265f;
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);

        // --- 绘制正常宇宙下的静界 (Ergosphere) ---
        float ergo_discriminant = M * M - a2 * cosTheta * cosTheta - Q2;
        bool ergo_valid = (ergo_discriminant >= 0.0f);
        if (ergo_valid)
        {
            float sqrt_disc = std::sqrt(ergo_discriminant);
            float r_ergo_out = M + sqrt_disc, r_ergo_in = M - sqrt_disc;
            ImVec2 pt_ergo_out = ToSideScreen(std::sqrt(r_ergo_out * r_ergo_out + a2) * sinTheta, r_ergo_out * cosTheta);
            ImVec2 pt_ergo_in = ToSideScreen(std::sqrt(r_ergo_in * r_ergo_in + a2) * sinTheta, r_ergo_in * cosTheta);

            if (prev_ergo_valid)
            {
                draw_list->AddLine(prev_ergo_out, pt_ergo_out, colErgosphere, 1.5f);
                draw_list->AddLine(prev_ergo_in, pt_ergo_in, colInnerErgo, 1.5f);
            }
            else if (i > 0)
            {
                draw_list->AddLine(pt_ergo_out, pt_ergo_in, colErgosphere, 1.5f);
            }
            prev_ergo_out = pt_ergo_out; prev_ergo_in = pt_ergo_in;
        }
        else if (prev_ergo_valid) draw_list->AddLine(prev_ergo_out, prev_ergo_in, colErgosphere, 1.5f);
        prev_ergo_valid = ergo_valid;

        // --- 绘制反宇宙中的 CTC 边界 ---
        float r_ctc_out, r_ctc_in;
        bool ctc_valid = GetCTCRoots(cosTheta, sinTheta, r_ctc_out, r_ctc_in);
        if (ctc_valid)
        {
            ImVec2 pt_ctc_out = ToSideScreen(std::sqrt(r_ctc_out * r_ctc_out + a2) * sinTheta, r_ctc_out * cosTheta);
            ImVec2 pt_ctc_in = ToSideScreen(std::sqrt(r_ctc_in * r_ctc_in + a2) * sinTheta, r_ctc_in * cosTheta);

            if (prev_ctc_valid)
            {
                draw_list->AddLine(prev_ctc_out, pt_ctc_out, colCTC, 1.5f);
                draw_list->AddLine(prev_ctc_in, pt_ctc_in, colCTC, 1.5f);
            }
            else if (i > 0)
            {
                draw_list->AddLine(pt_ctc_out, pt_ctc_in, colCTC, 1.5f);
            }
            prev_ctc_out = pt_ctc_out; prev_ctc_in = pt_ctc_in;
        }
        else if (prev_ctc_valid) draw_list->AddLine(prev_ctc_out, prev_ctc_in, colCTC, 1.5f);
        prev_ctc_valid = ctc_valid;

        // --- 俯视图绘制 (赤道面) ---
        if (i == 0)
        {
            float eq_disc = M * M - Q2;
            if (eq_disc >= 0.0f)
            {
                draw_list->AddCircle(centerTop, std::sqrt(std::pow(M + std::sqrt(eq_disc), 2) + a2) * scale, colErgosphere, 64, 1.5f);
                draw_list->AddCircle(centerTop, std::sqrt(std::pow(M - std::sqrt(eq_disc), 2) + a2) * scale, colInnerErgo, 64, 1.5f);
            }
            if (ctc_valid)
            {
                draw_list->AddCircle(centerTop, std::sqrt(r_ctc_out * r_ctc_out + a2) * scale, colCTC, 64, 1.5f);
                draw_list->AddCircle(centerTop, std::sqrt(r_ctc_in * r_ctc_in + a2) * scale, colCTC, 64, 1.5f);
            }
        }

        // --- 绘制正常宇宙视界 ---
        if (!isNakedSingularity)
        {
            sideOutPts.push_back(ToSideScreen(std::sqrt(r_outer * r_outer + a2) * sinTheta, r_outer * cosTheta));
            sideInPts.push_back(ToSideScreen(std::sqrt(r_inner * r_inner + a2) * sinTheta, r_inner * cosTheta));
            if (i == 0)
            {
                draw_list->AddCircle(centerTop, std::sqrt(r_outer * r_outer + a2) * scale, colOuterHorizon, 64, 2.0f);
                draw_list->AddCircle(centerTop, std::sqrt(r_inner * r_inner + a2) * scale, colInnerHorizon, 64, 2.0f);
            }
        }
    }

    if (!isNakedSingularity)
    {
        draw_list->AddPolyline(sideOutPts.data(), sideOutPts.size(), colOuterHorizon, ImDrawFlags_None, 2.0f);
        draw_list->AddPolyline(sideInPts.data(), sideInPts.size(), colInnerHorizon, ImDrawFlags_None, 2.0f);
    }

    // 5. 绘制奇环 (a)
    draw_list->AddCircleFilled(ToSideScreen(std::abs(a), 0.0f), 4.0f, colSingularity);
    draw_list->AddCircle(centerTop, std::abs(a) * scale, colSingularity, 64, 2.0f);

    auto IsInCanvas = [&](ImVec2 p)
    {
        return p.x >= canvas_p0.x && p.x <= canvas_p0.x + canvas_sz.x &&
            p.y >= canvas_p0.y && p.y <= canvas_p0.y + canvas_sz.y;
    };

    // ==========================================
    // 6. 绘制轨迹历史 (基于长度渐隐)
    // ==========================================
    for (size_t i = 1; i < g_TrajectoryHistory.size(); ++i)
    {
        const auto& p1 = g_TrajectoryHistory[i - 1];
        const auto& p2 = g_TrajectoryHistory[i];

        // 避免跨越奇环/宇宙跳变时出现的长虚假连线
        if (p1.UniverseSign != p2.UniverseSign) continue;

        // 计算该线段距离最新相机的“累计长度”，从而得到 alpha
        float trailDepth = g_TotalOdometer - p2.Odometer;
        float alpha = 1.0f - (trailDepth / MAX_TRAIL_LENGTH);
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        // 颜色：当前宇宙用青色，不同宇宙用暗红色
        bool isCurrentUniv = (p1.UniverseSign == BlackHoleArgs.UniverseSign);
        ImU32 col = isCurrentUniv ? IM_COL32(0, 200, 255, (int)(alpha * 255))
            : IM_COL32(255, 50, 50, (int)(alpha * 150));

        float rho1 = std::sqrt(p1.Pos.x * p1.Pos.x + p1.Pos.z * p1.Pos.z);
        float rho2 = std::sqrt(p2.Pos.x * p2.Pos.x + p2.Pos.z * p2.Pos.z);

        ImVec2 s1 = ToSideScreen(rho1, p1.Pos.y);
        ImVec2 s2 = ToSideScreen(rho2, p2.Pos.y);

        ImVec2 t1 = ToTopScreen(p1.Pos.x, -p1.Pos.z);
        ImVec2 t2 = ToTopScreen(p2.Pos.x, -p2.Pos.z);

        if (IsInCanvas(s1) || IsInCanvas(s2)) draw_list->AddLine(s1, s2, col, 1.5f);
        if (IsInCanvas(t1) || IsInCanvas(t2)) draw_list->AddLine(t1, t2, col, 1.5f);
    }

    // ==========================================
    // 7. 绘制相机位置、朝向矢量以及速度矢量
    // ==========================================
    ImU32 camColor = IM_COL32(255, 255, 0, 255); // 黄色：朝向
    ImU32 velColor = IM_COL32(0, 255, 0, 255);   // 绿色：速度

    float rho_cam = std::sqrt(camPos.x * camPos.x + camPos.z * camPos.z);
    ImVec2 camSidePos = ToSideScreen(rho_cam, camPos.y);
    ImVec2 camTopPos = ToTopScreen(camPos.x, -camPos.z);

    // 计算朝向矢量屏幕坐标
    float drho = (rho_cam > 1e-6f) ? ((camPos.x * camDir.x + camPos.z * camDir.z) / rho_cam) : std::sqrt(camDir.x * camDir.x + camDir.z * camDir.z);
    float camLineLen = std::fmax(1.5f, camDist * 0.15f);
    ImVec2 camSideDir = ToSideScreen(rho_cam + drho * camLineLen, camPos.y + camDir.y * camLineLen);
    ImVec2 camTopDir = ToTopScreen(camPos.x + camDir.x * camLineLen, -camPos.z - camDir.z * camLineLen);

    // 计算速度矢量屏幕坐标
    ImVec2 velSideDir, velTopDir;
    bool hasVelocity = false;
    float vLen = glm::length(camVel);
    if (vLen > 1e-5f)
    {
        hasVelocity = true;
        glm::vec3 vDir = camVel / vLen;
        // 动态缩放速度线长度 (根据速率伸缩，最大限制3倍朝向基准线，以防飞出屏幕)
        float drawLen = camLineLen * std::clamp(vLen * 1.5f, 0.5f, 3.0f);

        float vDrho = (rho_cam > 1e-6f) ? ((camPos.x * vDir.x + camPos.z * vDir.z) / rho_cam) : std::sqrt(vDir.x * vDir.x + vDir.z * vDir.z);
        velSideDir = ToSideScreen(rho_cam + vDrho * drawLen, camPos.y + vDir.y * drawLen);
        velTopDir = ToTopScreen(camPos.x + vDir.x * drawLen, -camPos.z - vDir.z * drawLen);
    }

    if (IsInCanvas(camSidePos))
    {
        if (hasVelocity) draw_list->AddLine(camSidePos, velSideDir, velColor, 2.5f);
        draw_list->AddLine(camSidePos, camSideDir, camColor, 2.0f);
        draw_list->AddCircleFilled(camSidePos, 5.0f, camColor);
    }
    if (IsInCanvas(camTopPos))
    {
        if (hasVelocity) draw_list->AddLine(camTopPos, velTopDir, velColor, 2.5f);
        draw_list->AddLine(camTopPos, camTopDir, camColor, 2.0f);
        draw_list->AddCircleFilled(camTopPos, 5.0f, camColor);
    }
    // ==========================================
// [新增] 8. 绘制 Penrose 图背景网格与轨迹
// ==========================================
    // ==========================================
    // [修改] Penrose 坐标转换与视野跟随
    // ==========================================
    // 根据当前宇宙索引动态偏移视口，使当前宇宙始终居中
    float view_tau_offset = BlackHoleArgs.InWhichUniverse * (2.0f * M_PI);
    auto ToPenScreen = [&](float chi, float tau) -> ImVec2
    {
        return ImVec2(centerPen.x + chi * scalePen, centerPen.y - (tau - view_tau_offset - M_PI) * scalePen);
    };

    auto KS_to_Penrose = [&](double t, double r, double r_plus, double r_minus, double kp, double km, bool isOut, int univIndex) -> ImVec2
    {
        double U = 0, V = 0;
        // [新增] 每个宇宙层级跨越 2*PI 的高度
        double shift_tau = univIndex * (2.0 * M_PI);
        double shift_chi = 0.0;

        if (!isOut) // Ingoing (落入黑洞)
        {
            if (r >= r_minus)
            {
                // I区 和 II区
                U = -(r - r_plus) / r_plus * std::pow(std::abs((r - r_minus) / r_minus), kp / km) * std::exp(-kp * (t - r));
                V = std::exp(kp * (t + r));
            }
            else
            {
                // III'区 (内部奇点区)
                // [修复] 将 (r_minus - r) 因子置于 U 上，确保 entry point 位于菱形底部，时间流向正上方
                U = (r_minus - r) / r_minus * std::pow(std::abs((r - r_plus) / r_plus), km / kp) * std::exp(-km * (t - r));
                V = -std::exp(km * (t + r));
                shift_tau += M_PI;
            }
        }
        else // Outgoing (逃逸白洞)
        {
            if (r >= r_minus)
            {
                // 上方新 I区 和 IV区(白洞内部)
                U = -std::exp(-kp * (t - r));
                V = (r - r_plus) / r_plus * std::pow(std::abs((r - r_minus) / r_minus), kp / km) * std::exp(kp * (t + r));
                shift_tau += 2.0 * M_PI;
            }
            else
            {
                // III'区 向外反弹逃逸 
                // [修复] 与 Ingoing 共享完全相同的坐标映射，完美承接反弹轨迹
                U = (r_minus - r) / r_minus * std::pow(std::abs((r - r_plus) / r_plus), km / kp) * std::exp(-km * (t - r));
                V = -std::exp(km * (t + r));
                shift_tau += M_PI;
            }
        }

        double tU = std::atan(U);
        double tV = std::atan(V);
        double tau = tV + tU + shift_tau;
        double chi = tV - tU + shift_chi;
        return ImVec2((float)chi, (float)tau);
    }; // ==========================================
    // [新增] 9. 绘制 Penrose 图无限塔背景网格
    // ==========================================
    // ==========================================
    // [修改] 9. 绘制 Penrose 图无限塔背景网格
    // ==========================================
    if (!isNakedSingularity)
    {
        double r_plus = M + std::sqrt(delta_discriminant);
        double r_minus = M - std::sqrt(delta_discriminant);
        double kappa_plus = (r_plus - r_minus) / (2.0 * (r_plus * r_plus + a2));
        double kappa_minus = (r_minus - r_plus) / (2.0 * (r_minus * r_minus + a2));
        auto DrawPLine = [&](float c1, float t1, float c2, float t2, ImU32 col, float thick = 1.5f)
        {
            draw_list->AddLine(ToPenScreen(c1, t1), ToPenScreen(c2, t2), col, thick);
        };
        auto DrawZigZag = [&](float c1, float t1, float c2, float t2, ImU32 col)
        {
            int steps = 15;
            float dc = (c2 - c1) / steps;
            float dt = (t2 - t1) / steps;
            ImVec2 p_prev = ToPenScreen(c1, t1);
            for (int i = 1; i <= steps; ++i)
            {
                float offset = (i % 2 == 0) ? 0.08f : -0.08f;
                if (i == steps) offset = 0.0f;
                float len = std::sqrt(dc * dc + dt * dt);
                float nx = -dt / len * offset;
                float ny = dc / len * offset;
                ImVec2 p_next = ToPenScreen(c1 + i * dc + nx, t1 + i * dt + ny);
                draw_list->AddLine(p_prev, p_next, col, 2.0f);
                p_prev = p_next;
            }
        };

        ImU32 colPInf = IM_COL32(100, 255, 100, 150);
        ImU32 colPHorOut = IM_COL32(255, 100, 100, 200);
        ImU32 colPHorIn = IM_COL32(100, 150, 255, 200);
        ImU32 colPSing = IM_COL32(255, 50, 255, 200);
        bool showAllPenrose = (BlackHoleArgs.Whitehole != 0);

        // [新增] 动态绘制相邻的 3 个宇宙，形成无限延伸视觉
        int current_univ = BlackHoleArgs.InWhichUniverse;
        for (int u = current_univ - 1; u <= current_univ + 1; ++u)
        {
            float b_tau = u * (2.0f * M_PI); // 当前宇宙的基础高度基准

            // 底部 I 区与 I' 区
            DrawPLine(M_PI / 2, b_tau - M_PI / 2, M_PI, b_tau + 0, colPInf);
            DrawPLine(M_PI, b_tau + 0, M_PI / 2, b_tau + M_PI / 2, colPInf);
            DrawPLine(-M_PI / 2, b_tau - M_PI / 2, -M_PI, b_tau + 0, colPInf);
            DrawPLine(-M_PI, b_tau + 0, -M_PI / 2, b_tau + M_PI / 2, colPInf);
            DrawPLine(0, b_tau + 0, M_PI / 2, b_tau - M_PI / 2, colPHorIn);
            DrawPLine(0, b_tau + 0, -M_PI / 2, b_tau - M_PI / 2, colPHorIn);
            DrawPLine(0, b_tau + 0, M_PI / 2, b_tau + M_PI / 2, colPHorOut);
            DrawPLine(0, b_tau + 0, -M_PI / 2, b_tau + M_PI / 2, colPHorOut);

            // II 区 (黑洞内部)
            DrawPLine(M_PI / 2, b_tau + M_PI / 2, 0, b_tau + M_PI, colPHorIn);
            DrawPLine(-M_PI / 2, b_tau + M_PI / 2, 0, b_tau + M_PI, colPHorIn);

            // III 区与 III' 区
            DrawPLine(0, b_tau + M_PI, M_PI / 2, b_tau + 3 * M_PI / 2, colPHorOut);
            DrawPLine(0, b_tau + M_PI, -M_PI / 2, b_tau + 3 * M_PI / 2, colPHorOut);
            DrawZigZag(M_PI, b_tau + M_PI / 2, M_PI, b_tau + 3 * M_PI / 2, colPSing);
            DrawZigZag(-M_PI, b_tau + M_PI / 2, -M_PI, b_tau + 3 * M_PI / 2, colPSing);
            DrawPLine(M_PI / 2, b_tau + M_PI / 2, M_PI, b_tau + M_PI, IM_COL32(100, 100, 100, 100));
            DrawPLine(M_PI / 2, b_tau + 3 * M_PI / 2, M_PI, b_tau + M_PI, IM_COL32(100, 100, 100, 100));
            DrawPLine(-M_PI / 2, b_tau + M_PI / 2, -M_PI, b_tau + M_PI, IM_COL32(100, 100, 100, 100));
            DrawPLine(-M_PI / 2, b_tau + 3 * M_PI / 2, -M_PI, b_tau + M_PI, IM_COL32(100, 100, 100, 100));

            if (showAllPenrose)
            {
                // IV 区 (白洞内部) 及 顶端新 I 区
                DrawPLine(M_PI / 2, b_tau + 3 * M_PI / 2, 0, b_tau + 2 * M_PI, colPHorIn);
                DrawPLine(-M_PI / 2, b_tau + 3 * M_PI / 2, 0, b_tau + 2 * M_PI, colPHorIn);
                DrawPLine(M_PI / 2, b_tau + 3 * M_PI / 2, M_PI, b_tau + 2 * M_PI, colPInf);
                DrawPLine(M_PI, b_tau + 2 * M_PI, M_PI / 2, b_tau + 5 * M_PI / 2, colPInf);
                DrawPLine(-M_PI / 2, b_tau + 3 * M_PI / 2, -M_PI, b_tau + 2 * M_PI, colPInf);
                DrawPLine(-M_PI, b_tau + 2 * M_PI, -M_PI / 2, b_tau + 5 * M_PI / 2, colPInf);
                DrawPLine(0, b_tau + 2 * M_PI, M_PI / 2, b_tau + 5 * M_PI / 2, colPHorOut);
                DrawPLine(0, b_tau + 2 * M_PI, -M_PI / 2, b_tau + 5 * M_PI / 2, colPHorOut);
            }
            else
            {
                // 若关闭白洞，只框选当前宇宙的底部
                ImVec2 boxMin = ToPenScreen(-M_PI / 2 - 0.2f, b_tau + 3 * M_PI / 2 + 0.2f);
                ImVec2 boxMax = ToPenScreen(M_PI + 0.2f, b_tau - M_PI / 2 - 0.2f);
                if (u == current_univ) draw_list->AddRect(boxMin, boxMax, IM_COL32(0, 255, 0, 255), 0.0f, 0, 3.0f);
            }
        }
        // --- 10. 绘制轨迹映射到 Penrose 图上的坐标 ---
        ImVec2 prev_pen;
        bool has_prev_pen = false;

        for (size_t i = 0; i < g_TrajectoryHistory.size(); ++i)
        {
            const auto& pt = g_TrajectoryHistory[i];

            // 恢复该点的 BL 半径
            float r_ui_b = (pt.Pos.x * pt.Pos.x + pt.Pos.y * pt.Pos.y + pt.Pos.z * pt.Pos.z) - a2;
            float r_ui_c = a2 * pt.Pos.y * pt.Pos.y;
            float r_pt_bl = std::sqrt(std::fmax(0.0f, 0.5f * (r_ui_b + std::sqrt(r_ui_b * r_ui_b + 4.0f * r_ui_c))));

            // [修改] 传入 pt.UnivIndex
            ImVec2 pen_pos = KS_to_Penrose(pt.t_ks, r_pt_bl, r_plus, r_minus, kappa_plus, kappa_minus, pt.isOut, pt.UnivIndex);
            ImVec2 screen_pen = ToPenScreen(pen_pos.x, pen_pos.y);
            // ... (其余不变)

            float trailDepth = g_TotalOdometer - pt.Odometer;
            float alpha = std::clamp(1.0f - (trailDepth / 600.0f), 0.0f, 1.0f);
            ImU32 col = IM_COL32(0, 200, 255, (int)(alpha * 255));

            // 避免因跨越宇宙翻转而在图表上拉出水平横跨连线
            if (has_prev_pen && i > 0 &&
                g_TrajectoryHistory[i - 1].UniverseSign == pt.UniverseSign &&
                g_TrajectoryHistory[i - 1].isOut == pt.isOut)
            {
                draw_list->AddLine(prev_pen, screen_pen, col, 1.5f);
            }
            prev_pen = screen_pen;
            has_prev_pen = true;
        }
        // 画出高亮的玩家当前坐标点
        // 画出高亮的玩家当前坐标点
        double current_t_ks = g_GeodesicMode ? g_GeoState[3] : BlackHoleArgs.BlackHoleTime;
        bool current_isOut = g_GeodesicMode ? g_isOutgoing : false;
        // [修改] 传入 BlackHoleArgs.InWhichUniverse
        ImVec2 curr_pen_pos = KS_to_Penrose(current_t_ks, std::abs(r_ui), r_plus, r_minus, kappa_plus, kappa_minus, current_isOut, BlackHoleArgs.InWhichUniverse);
        draw_list->AddCircleFilled(ToPenScreen(curr_pen_pos.x, curr_pen_pos.y), 4.0f, IM_COL32(255, 255, 0, 255)); }
    ImGui::End();
}
_NPGS_END