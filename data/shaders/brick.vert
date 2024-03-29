#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma import_defines (VSG_POSITION_SCALE)

#define VIEW_DESCRIPTOR_SET 0
#define MATERIAL_DESCRIPTOR_SET 1

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelView;
} pc;


layout(location = 0) in vec3 vsg_Vertex;
layout(location = 1) in vec3 vsg_Normal;
layout(location = 2) in vec4 vsg_Color;

#ifdef VSG_POSITION_SCALE
layout(location = 3) in vec4 vsg_PositionScale;
#endif

layout(location = 4) in vec2 vsg_PointSize;

layout(location = 0) out vec3 eyePos;
layout(location = 1) out vec3 normalDir;
layout(location = 2) out vec4 vertexColor;

layout(location = 5) out vec3 viewDir;

layout(set = VIEW_DESCRIPTOR_SET, binding = 1) buffer ViewportData
{
    vec4 values[];
} viewportData;

out gl_PerVertex{
    vec4 gl_Position;
    float gl_PointSize;
};

void main()
{

    #ifdef VSG_POSITION_SCALE
    vec4 vertex = vec4(vsg_PositionScale.xyz + vsg_Vertex * vsg_PositionScale.w, 1.0);
    #else
    vec4 vertex = vec4(vsg_Vertex, 1.0);
    #endif

    vec4 normal = vec4(vsg_Normal, 0.0);

    gl_Position = (pc.projection * pc.modelView) * vertex;

    eyePos = vec4(pc.modelView * vertex).xyz;
    viewDir = -eyePos;//normalize(-eyePos);
    normalDir = (pc.modelView * normal).xyz;
    vertexColor = vsg_Color;

    float dist = max(vsg_PointSize[1], abs(eyePos.z));
    vec4 viewport = viewportData.values[0];
    gl_PointSize = viewport[3] * (vsg_PointSize[0] / dist);
}

