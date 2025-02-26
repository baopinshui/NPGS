#include "TooolfuncForStarMap.h"
#include <cmath>
#include <algorithm>

glm::vec4 ScaleColor(double x, const glm::vec4& color) {//rgb数乘,带过曝处理
    glm::vec4 newColor;

    newColor.r = color.r * x;  // 缩放并限制在 [0, 255]
    newColor.g = color.g * x;  // 缩放并限制在 [0, 255]
    newColor.b = color.b * x;  // 缩放并限制在 [0, 255]
    newColor.a = color.a;  // 保持 alpha 不变

    return newColor;
}

glm::vec4 kelvin_to_rgb(double T) {

    double _ = (T - 6500.0) / (6500.0 * T * 2.2);
    double R = std::exp(2.05539304255011812e+04 * _);
    double G = std::exp(2.63463675111716257e+04 * _);
    double B = std::exp(3.30145738821726118e+04 * _);
    double LmulRate = 1.0 / std::max(std::max(R, G), B);
    R *= LmulRate;
    G *= LmulRate;
    B *= LmulRate;
    return glm::vec4(R, G, B, 1.0);
}

//void drawFilledCircle(GLRenderer* renderer, int width, int height, int centerX, int centerY, int radius, SDL_Color color) {
//
//    int x = radius - 1;
//    int y = 0;
//    int dx = 1;
//    int dy = 1;
//    int err = dx - (radius << 1);
//
//    while (x >= y) {
//        renderer->RendererDrawLine(width, height, { centerX + x, centerY + y }, { centerX - x, centerY + y }, { color.r / 255.0,color.g / 255.0,color.b / 255.0,color.a / 255.0 });
//        renderer->RendererDrawLine(width, height, { centerX + y, centerY + x }, { centerX - y, centerY + x }, { color.r / 255.0,color.g / 255.0,color.b / 255.0,color.a / 255.0 });
//        renderer->RendererDrawLine(width, height, { centerX - x, centerY - y }, { centerX + x, centerY - y }, { color.r / 255.0,color.g / 255.0,color.b / 255.0,color.a / 255.0 });
//        renderer->RendererDrawLine(width, height, { centerX - y, centerY - x }, { centerX + y, centerY - x }, { color.r / 255.0,color.g / 255.0,color.b / 255.0,color.a / 255.0 });
//
//        if (err <= 0) {
//            y++;
//            err += dy;
//            dy += 2;
//        }
//
//        if (err > 0) {
//            x--;
//            dx += 2;
//            err += (-radius << 1) + dx;
//        }
//    }
//}

double variable_threshold001(double variable) {
    return (variable > 0.01) ? variable : 0.01;
}

double variable_threshold00(double variable) {
    return (variable > 0.00000000001) ? variable : 0.00000000001;
}

double variable_threshold1(double variable) {
    return (variable < 1) ? variable : 1;
}



std::string random_name() {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::string tmp_s;
    tmp_s.reserve(6);

    for (int i = 0; i < 6; ++i) {
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    return tmp_s;
}


bool isMouseNearLine(glm::dvec3 A, glm::dvec3 B, glm::dvec3 C, glm::dvec3 v, double theta) {
    glm::dvec3 z = glm::normalize(v);
    glm::dvec3 AC = C - A;
    glm::dvec3 AB = B - A;
    glm::dvec3 d = glm::cross(z, AB);
    if (glm::length(d) == 0) {
        return false;
    }
    d = glm::normalize(d);
    double h = abs(glm::dot(d, AC));
    double x = -0.5 * (2 * glm::dot(AC, z) - 2 * glm::dot(AC, AB) * glm::dot(AB, z) / (glm::dot(AB, AB))) / (1 - pow((glm::dot(AB, z) / glm::length(AB)), 2));
    if (x <= 0) {
        return false;
    }
    if (h / x > theta) {
        return false;
    }

    glm::dvec3 P = C + x * z;
    glm::dvec3 AP = P - A;
    if (glm::dot(AB, AP) < 0 || glm::dot(AB, AP) > glm::dot(AB, AB)) {
        return false;
    }

    return true;
}
std::string C8toStr(const char8_t* char8Array) {
    std::u8string u8str(reinterpret_cast<const char8_t*>(char8Array));


    return std::string(reinterpret_cast<const char*>(u8str.c_str()));
}
glm::dvec3 vec90rotate(double the, double theta, double phi) {
    double CA = cos(the);
    double SA = sin(the);
    double CO = cos(phi);
    double SO = sin(phi);
    double CT = cos(theta);
    double ST = sin(theta);
    double SO2 = sin(phi / 2.0);
    double S2T = sin(2 * theta);
    glm::dvec3 result(
        CA * (CO * CT * CT + ST * ST) - SA * SO2 * SO2 * S2T,
        SA * (CT * CT + CO * ST * ST) - CA * SO2 * SO2 * S2T,
        -CA * CT * SO - SA * SO * ST
    );
    return result;

}
glm::dvec3 vecrotate(glm::dvec3 v, double theta, double phi) {

    double CO = cos(phi);
    double SO = sin(phi);
    double CT = cos(theta);
    double ST = sin(theta);
    double SO2 = sin(phi / 2.0);
    double S2T = sin(2 * theta);
    glm::dvec3 result(
        v.z * CT * SO + v.x * (CO * CT * CT + ST * ST) - v.y * SO2 * SO2 * S2T,
        v.z * SO * ST + v.y * (CT * CT + CO * ST * ST) - v.x * SO2 * SO2 * S2T,
        v.z * CO - v.x * CT * SO - v.y * SO * ST
    );
    return result;
}



void vec1to2(double& theta, double& phi, double theta0, double phi0, double a) {
    glm::dvec3 v(cos(theta) * sin(phi), sin(theta) * sin(phi), cos(phi));
    glm::dvec3 v0(cos(theta0) * sin(phi0), sin(theta0) * sin(phi0), cos(phi0));
    if (glm::length(v0 - v) > 0) {
        v = glm::normalize((v + (a * 2 * asin(glm::length((v0 - v)) / 2) * glm::normalize(glm::cross(glm::cross(v, (v0 - v)), v)))));

        phi = acos(v.z);
        theta = acos(v.x / sin(phi));
    }
    phi = std::clamp(phi, 0.0, PI);
    theta = std::clamp(theta, 0.0, PI);
    if (std::isnan(theta)) { theta = 0; }
    if (std::isnan(phi)) { phi = 0; }
}


std::string formatTime(double seconds) {
    // Define the units in seconds
    const double yearInSeconds = 31536000;  // 365 days
    const double monthInSeconds = 2628000;  // 30 days
    const double dayInSeconds = 86400;


    // Calculate years
    long long years = (seconds / yearInSeconds);
    seconds -= years * yearInSeconds;

    // Calculate months
    long long months = (seconds / monthInSeconds);
    seconds -= months * monthInSeconds;

    // Calculate days
    long long days = (seconds / dayInSeconds);
    seconds -= days * dayInSeconds;


    // Seconds left
    int remainingSeconds = static_cast<int>(seconds);

    // Prepare the formatted string
    std::string result;
    if (years > 0) {
        result += std::to_string(years) + " years ";
    }
    if (months > 0) {
        result += std::to_string(months + 1) + " months ";
    }
    if (days > 0) {
        result += std::to_string(days) + " days ";
    }
    result += std::to_string(remainingSeconds) + " s ";


    return result;
}

//void drawDashedLine(GLRenderer* renderer, int width, int height, int x1, int y1, int x2, int y2, double dashLength) {
//
//    double dx = x2 - x1;
//    double dy = y2 - y1;
//    double length = sqrt(dx * dx + dy * dy);
//
//    // Calculate how many dashes are needed
//    int numDashes = static_cast<int>(length / dashLength);
//    if (numDashes >= 1000) {
//        numDashes = 1000;
//    }
//    // Calculate dash components
//    double dash_dx = dx / numDashes;
//    double dash_dy = dy / numDashes;
//
//    // Draw dashes
//    for (int i = 0; i < numDashes; ++i) {
//
//        // Calculate start and end points of each dash
//        int dash_x1 = static_cast<int>(x1 + i * dash_dx);
//        int dash_y1 = static_cast<int>(y1 + i * dash_dy);
//        int dash_x2 = static_cast<int>(x1 + (i + 0.5) * dash_dx);
//        int dash_y2 = static_cast<int>(y1 + (i + 0.5) * dash_dy);
//
//        // Draw dash
//        renderer->RendererDrawLine(width, height, { x1,y1 }, { x2,y2 }, { 100 / 255.0, 100 / 255.0, 100 / 255.0, 255 / 255.0 });
//    }
//
//    // Make sure the last dash reaches the endpoint of the line
//   // SDL_RenderDrawLine(renderer, static_cast<int>(x1 + numDashes * dash_dx), static_cast<int>(y1 + numDashes * dash_dy), x2, y2);
//}

//std::string PhaseToString(Npgs::AstroObject::Star::Phase phase) {
//    switch (phase) {
//    case Npgs::AstroObject::Star::Phase::kPrevMainSequence: return          C8toStr(u8"前主序");
//    case Npgs::AstroObject::Star::Phase::kMainSequence: return              C8toStr(u8"氢主序");
//    case Npgs::AstroObject::Star::Phase::kRedGiant: return                  C8toStr(u8"红巨星");
//    case Npgs::AstroObject::Star::Phase::kCoreHeBurn: return                C8toStr(u8"氦主序");
//    case Npgs::AstroObject::Star::Phase::kEarlyAgb: return                  C8toStr(u8"早期渐近巨星");
//    case Npgs::AstroObject::Star::Phase::kThermalPulseAgb: return           C8toStr(u8"热脉冲渐近巨星");
//    case Npgs::AstroObject::Star::Phase::kPostAgb: return                   C8toStr(u8"后渐近巨星支");
//    case Npgs::AstroObject::Star::Phase::kWolfRayet: return                 C8toStr(u8"沃尔夫-拉叶星");
//    case Npgs::AstroObject::Star::Phase::kHeliumWhiteDwarf: return          C8toStr(u8"氦白矮星");
//    case Npgs::AstroObject::Star::Phase::kCarbonOxygenWhiteDwarf: return    C8toStr(u8"碳氧白矮星");
//    case Npgs::AstroObject::Star::Phase::kOxygenNeonMagnWhiteDwarf: return  C8toStr(u8"氧氖镁白矮星");
//    case Npgs::AstroObject::Star::Phase::kNeutronStar: return               C8toStr(u8"中子星");
//    case Npgs::AstroObject::Star::Phase::kBlackHole: return                 C8toStr(u8"恒星质量黑洞");
//    case Npgs::AstroObject::Star::Phase::kNull: return                      C8toStr(u8"unknow");
//    }
//}


std::string dToScStr(double value) {
    std::ostringstream oss;
    double absValue = std::fabs(value);

    if (absValue >= 1e-3 && absValue < 1e+6) {
        // 计算适当的位数
        double scale = std::pow(10.0, std::floor(std::log10(absValue)) - 3);
        value = std::round(value / scale) * scale;

        // 使用定点格式
        oss << std::fixed << value;
        std::string result = oss.str();
        size_t pos = result.find_last_not_of('0');
        if (pos != std::string::npos && result[pos] == '.') {
            pos--;
        }
        result.erase(pos + 1);
        return result;
    } else {
        oss << std::scientific << std::setprecision(3) << value;
        std::string result = oss.str();
        return result;
    }

}


//void renderTextureWithColor(GLRenderer* renderer, Texture* texture, SDL_Color color, SDL_Rect destRect) {
//    renderer->Draw(texture, { destRect.x,destRect.y }, { destRect.w,destRect.h }, 0, { color.r / 255.0,color.g / 255.0 ,color.b / 255.0 ,color.a / 255.0 });
//}

double CalTimeFromTheta(double theta, double e) {
    theta = fmod(theta, 2 * PI);
    if (theta < 0) {
        theta += 2 * PI;
    }
    double tt = tan(0.5 * theta);
    double x = 0;
    if (theta > PI) { x = PI; }
    if (std::isnan(tt)) { tt = 114514; }
    double result = (2 * (atan(sqrt((1 - e) / (1 + e)) * tt) + x) - (e * sqrt(1 - e * e) * sin(theta)) / (1 + e * cos(theta))) / (2 * PI);
    if (result > 1) { result -= 1; }
    return result;
}

double CalThetaFromTime(double time, double e) {
    time = fmod(time, 1);
    if (time < 0) {
        time += 1;
    }
    int step = 0;
    double y;
    double dyx;
    double theta = PI;
    y = CalTimeFromTheta(theta, e) - time;
    dyx = pow((1 - e * e), 1.5) / pow((1 + e * cos(theta)), 2) / 2 / PI;
    theta -= y / dyx;
    theta = fmod(theta, 2 * PI);
    if (theta < 0) {
        theta += 2 * PI;
    }
    while (y > 0.0000001 || y < -0.0000001) {
        step += 1;
        y = CalTimeFromTheta(theta, e) - time;
        dyx = pow((1 - e * e), 1.5) / pow((1 + e * cos(theta)), 2);
        theta -= y / dyx;
        theta = fmod(theta, 2 * PI);
        if (theta < 0) {
            theta += 2 * PI;
        }
    }
    // std::cout << step << std::endl;
    return theta;
}

glm::dvec3 CalPlanetPos(double time, double theta, double phi, double a, double e, double orbit_theta) {
    double rt = CalThetaFromTime(time, e) + orbit_theta;
    return a * (1 - e * e) / (1 + e * cos(rt - orbit_theta)) * vec90rotate(rt, theta, phi);

}
glm::dvec3 CalPlanetPosbyTheta(double the, double theta, double phi, double a, double e, double orbit_theta) {
    double rt = the + orbit_theta;
    return (a * (1 - e * e) / (1 + e * cos(rt - orbit_theta)) * vec90rotate(rt, theta, phi));
}

//StarVe findStarInSolar(const std::vector<Solar>& solarList, int targetNumber) {
//    for (int i = 0; i < solarList.size(); ++i) {
//        const Solar& solar = solarList[i];
//        for (int j = 0; j < solar.stars.size(); ++j) {
//            if (solar.stars[j].number == targetNumber) {
//                return solar.stars[j];
//            }
//        }
//    }
//}
//bool starInAsolar(Solar& asolar, int number) {
//    bool a = 0;
//    for (auto& star : asolar.stars) {
//        if (star.number == number) {
//            a = 1;
//        }
//    }
//    return a;
//}
float GetKeplerianAngularVelocity(double Radius, double Rs)
{
    return sqrt(Npgs::kSpeedOfLight / Npgs::kLightYearToMeter * Npgs::kSpeedOfLight * Rs / Npgs::kLightYearToMeter /
        ((2.0 * Radius - 3.0 * Rs) * pow(Radius, 2)));
}