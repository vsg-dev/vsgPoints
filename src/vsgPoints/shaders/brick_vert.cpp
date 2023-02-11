#include <vsg/io/VSG.h>
#include <vsg/io/mem_stream.h>
static auto brick_vert = []() {
static const char str[] = 
R"(#vsga 1.0.3
Root id=1 vsg::ShaderStage
{
  userObjects 0
  stage 1
  entryPointName "main"
  module id=2 vsg::ShaderModule
  {
    userObjects 0
    hints id=0
    source "#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma import_defines (VSG_POSITION_SCALE)

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

layout(location = 0) out vec3 eyePos;
layout(location = 1) out vec3 normalDir;
layout(location = 2) out vec4 vertexColor;

layout(location = 5) out vec3 viewDir;

layout(set = 1, binding = 1) uniform ViewportData
{
    vec4 values[1];
} viewportData;

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

    float dist = max(pointSize.minDistance, abs(eyePos.z));
    vec4 viewport = viewportData.values[0];
    gl_PointSize = viewport[2] * (pointSize.size / dist);
}

"
    code 0
    
  }
  NumSpecializationConstants 0
}
)";
vsg::VSG io;
return io.read_cast<vsg::ShaderStage>(reinterpret_cast<const uint8_t*>(str), sizeof(str));
};
