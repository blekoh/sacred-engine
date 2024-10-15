#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec3 outTangent;    // Pass the tangent vector to the fragment shader
layout (location = 4) out vec3 outBitangent;  // Pass the bitangent vector to the fragment shader

struct Vertex {
    vec3 position;   // 12 bytes
    float uv_x;           // 4 bytes (aligns the next vec3 to 16 bytes)
    vec3 normal;     // 12 bytes
    float uv_y;           // 4 bytes (aligns the next vec4 to 16 bytes)
    vec4 color;      // 16 bytes
    vec3 tangent;    // 12 bytes
    float padding1;       // 4 bytes padding (to align to 16 bytes)
    vec3 bitangent;  // 12 bytes
    float padding2;       // 4 bytes padding (to align to 16 bytes)
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

//push constants block
layout(push_constant) uniform constants {
	mat4 render_matrix;
	VertexBuffer vertexBuffer;
} PushConstants;

void main() 
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	
	vec4 position = vec4(v.position, 1.0f);

	gl_Position = sceneData.viewproj * PushConstants.render_matrix * position;

	outNormal = (PushConstants.render_matrix * vec4(v.normal, 0.0f)).xyz;
	outTangent = (PushConstants.render_matrix * vec4(v.tangent, 0.0f)).xyz;
	outBitangent = (PushConstants.render_matrix * vec4(v.bitangent, 0.0f)).xyz;

	outColor = v.color.xyz * materialData.colorFactors.xyz;	
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
}
