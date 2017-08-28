#include "renderer_common.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;

out vec3 v_normal;
out vec3 v_world_position;
out vec3 v_view_space_position;
out vec2 v_texcoord;
out vec3 v_tangent;
out vec3 v_bitangent;

void main()
{
    vec4 worldPosition = u_modelMatrix * vec4(inPosition, 1.0);
    gl_Position = u_viewProjMatrix * worldPosition;
    v_view_space_position = (u_modelViewMatrix * vec4(inPosition, 1.0)).xyz;
    v_normal = normalize((u_modelMatrixIT * vec4(inNormal, 1.0)).xyz);
    v_world_position = worldPosition.xyz;
    v_texcoord = inTexCoord.st;
    v_tangent = inTangent;
    v_bitangent = inBitangent;
}