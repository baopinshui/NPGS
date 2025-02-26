#pragma once
#include <cmath>
#include <algorithm>
#define NOMINMAX
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Engine/Core/Math/NumericConstants.h"
const double PI = 3.141592653589793238;


glm::vec4 ScaleColor(double x, const glm::vec4& color);
glm::vec4 kelvin_to_rgb(double T);
//void drawFilledCircle(GLRenderer* renderer, int width, int height, int centerX, int centerY, int radius, SDL_Color color);///////////////////////////
double variable_threshold001(double variable);
double variable_threshold00(double variable);
double variable_threshold1(double variable);
std::string random_name();///////////////////////////
bool isMouseNearLine(glm::dvec3 A, glm::dvec3 B, glm::dvec3 C, glm::dvec3 v, double theta);
std::string C8toStr(const char8_t* char8Array);
glm::dvec3 vec90rotate(double the, double theta, double phi);
glm::dvec3 vecrotate(glm::dvec3 v, double theta, double phi);
void vec1to2(double& theta, double& phi, double theta0, double phi0, double a);
std::string formatTime(double seconds);
//void drawDashedLine(GLRenderer* renderer, int width, int height, int x1, int y1, int x2, int y2, double dashLength);///////////////////////////////
//std::string PhaseToString(Npgs::AstroObject::Star::Phase phase);
std::string dToScStr(double value);
//void renderTextureWithColor(GLRenderer* renderer, Texture* texture, SDL_Color color, SDL_Rect destRect);///////////////////////////
double CalThetaFromTime(double time, double e);
double CalTimeFromTheta(double theta, double e);
glm::dvec3 CalPlanetPos(double time, double theta, double phi, double a, double e, double orbit_theta);
glm::dvec3 CalPlanetPosbyTheta(double the, double theta, double phi, double a, double e, double orbit_theta);
//StarVe findStarInSolar(const std::vector<Solar>& solarList, int targetNumber);/////////////////////
//bool starInAsolar(Solar& asolar, int number);/////////////////////////////
float GetKeplerianAngularVelocity(double Radius, double Rs);