#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inTangent;    // Tangent vector from vertex shader
layout (location = 4) in vec3 inBitangent;  // Bitangent vector from vertex shader

layout (location = 0) out vec4 outFragColor;

void main() 
{
    // Sample the normal map
    vec3 normalMap = texture(normalTex, inUV).rgb;
    normalMap = normalize(normalMap * 2.0 - 1.0);  // Convert from [0, 1] to [-1, 1] range

    // Amplify the normal map effect (increase this value for stronger effect)
    float normalStrength = 3.0;  // Adjust this value to make the normal map effect stronger
    normalMap = normalize(mix(vec3(0.0, 0.0, 1.0), normalMap, normalStrength));

    // Build the TBN matrix
    vec3 T = normalize(inTangent);
    vec3 B = normalize(inBitangent);
    vec3 N = normalize(inNormal);
    mat3 TBN = mat3(T, B, N);

    // Transform the normal from tangent space to world space
    vec3 worldNormal = normalize(TBN * normalMap);

    // Lighting calculations
    float lightValue = max(dot(worldNormal, sceneData.sunlightDirection.xyz), 0.1f);

    vec3 color = inColor * texture(colorTex, inUV).xyz;
    vec3 ambient = color * sceneData.ambientColor.xyz;

    outFragColor = vec4(color * lightValue * sceneData.sunlightColor.w + ambient, 1.0f);
}
