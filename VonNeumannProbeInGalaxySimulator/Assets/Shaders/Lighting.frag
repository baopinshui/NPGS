#version 460 core

layout(location = 0) out vec4 FragColor;

struct Light
{
	vec3 Position;
	vec3 AmbientColor;
	vec3 DiffuseColor;
	vec3 SpecularColor;
};

in vec3 Normal;
in vec2 TexCoord;
in vec3 FragPos;

uniform float     iShininess;
uniform vec3      iViewPos;
uniform sampler2D iDiffuseTex0;
uniform sampler2D iSpecularTex0;
uniform Light     iLight;

void main()
{
#ifdef LAMP_CUBE
	FragColor = vec4(1.0);
	return;
#endif
	vec3 AmbientColor = iLight.AmbientColor * texture(iDiffuseTex0, TexCoord).rgb;

	vec3 LightDir = normalize(iLight.Position - FragPos);
	float DiffuseFactor = max(dot(Normal, LightDir), 0.0);
	vec3 DiffuseColor = iLight.DiffuseColor * texture(iDiffuseTex0, TexCoord).rgb * DiffuseFactor;

	vec3 ViewDir = normalize(iViewPos - FragPos);
	vec3 ReflectDir = reflect(-LightDir, Normal);
	float SpecularFactor = pow(max(dot(ViewDir, ReflectDir), 0.0), iShininess);
	vec3 SpecularColor = iLight.SpecularColor * texture(iSpecularTex0, TexCoord).rgb * SpecularFactor;

	vec3 Result = AmbientColor + DiffuseColor + SpecularColor;
	FragColor = vec4(Result, 1.0);
}