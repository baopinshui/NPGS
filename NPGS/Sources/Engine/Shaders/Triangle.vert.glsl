#version 450
#pragma shader_stage(vertex)

layout(location = 0) in  vec3 Position;
layout(location = 1) in  vec2 TexCoord;
layout(location = 0) out vec2 TexCoordFromVert;

layout(push_constant) uniform PushConstants
{
	mat4x4 iModel;
};

layout(set = 0, binding = 0) uniform UniformBuffer
{
	// mat4x4 iModel;
	mat4x4 iView;
	mat4x4 iProjection;
};

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
//    // 打印 Model 矩阵（行主序）
//    debugPrintfEXT("\nModel Matrix (row-major):\n");
//    debugPrintfEXT("%.3f %.3f %.3f %.3f\n", iModel[0][0], iModel[1][0], iModel[2][0], iModel[3][0]);
//    debugPrintfEXT("%.3f %.3f %.3f %.3f\n", iModel[0][1], iModel[1][1], iModel[2][1], iModel[3][1]);
//    debugPrintfEXT("%.3f %.3f %.3f %.3f\n", iModel[0][2], iModel[1][2], iModel[2][2], iModel[3][2]);
//    debugPrintfEXT("%.3f %.3f %.3f %.3f\n", iModel[0][3], iModel[1][3], iModel[2][3], iModel[3][3]);
//
//    // 打印 View 矩阵（行主序）
//    debugPrintfEXT("\nView Matrix (row-major):\n");
//    debugPrintfEXT("%.3f %.3f %.3f %.3f\n", iView[0][0], iView[1][0], iView[2][0], iView[3][0]);
//    debugPrintfEXT("%.3f %.3f %.3f %.3f\n", iView[0][1], iView[1][1], iView[2][1], iView[3][1]);
//    debugPrintfEXT("%.3f %.3f %.3f %.3f\n", iView[0][2], iView[1][2], iView[2][2], iView[3][2]);
//    debugPrintfEXT("%.3f %.3f %.3f %.3f\n", iView[0][3], iView[1][3], iView[2][3], iView[3][3]);
//
//    // 打印 Projection 矩阵（行主序）
//    debugPrintfEXT("\nProjection Matrix (row-major):\n");
//    debugPrintfEXT("%.3f %.3f %.3f %.3f\n", iProjection[0][0], iProjection[1][0], iProjection[2][0], iProjection[3][0]);
//    debugPrintfEXT("%.3f %.3f %.3f %.3f\n", iProjection[0][1], iProjection[1][1], iProjection[2][1], iProjection[3][1]);
//    debugPrintfEXT("%.3f %.3f %.3f %.3f\n", iProjection[0][2], iProjection[1][2], iProjection[2][2], iProjection[3][2]);
//    debugPrintfEXT("%.3f %.3f %.3f %.3f\n", iProjection[0][3], iProjection[1][3], iProjection[2][3], iProjection[3][3]);

	TexCoordFromVert = TexCoord;
	gl_Position = iProjection * iView * iModel * vec4(Position, 1.0);
}
