#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelView;
} pc;


layout(location = 0) in vec3 vsg_Vertex;
layout(location = 1) in vec3 vsg_Normal;
layout(location = 2) in vec4 vsg_Color;


layout(location = 0) out vec3 eyePos;
layout(location = 1) out vec3 normalDir;
layout(location = 2) out vec4 vertexColor;

layout(location = 5) out vec3 viewDir;

layout(binding = 8) uniform Viewport
{
    float x;
    float y;
    float width;
    float height;
} viewport;

layout(binding = 9) uniform PointSize
{
    float size;
    float minDistance;
} pointSize;

out gl_PerVertex{
    vec4 gl_Position;
    float gl_PointSize;
};

void main()
{
    vec4 vertex = vec4(vsg_Vertex, 1.0) /** vec4(256, 256, 256, 1)*/;
    vec4 normal = vec4(vsg_Normal, 0.0);

    gl_Position = (pc.projection * pc.modelView) * vertex;

    eyePos = vec4(pc.modelView * vertex).xyz;
    viewDir = -eyePos;//normalize(-eyePos);
    normalDir = (pc.modelView * normal).xyz;
    vertexColor = vsg_Color;

    float dist = max(pointSize.minDistance, abs(eyePos.z));
    gl_PointSize = viewport.height * (pointSize.size / dist);
}

