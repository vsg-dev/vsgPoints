/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgPoints/BrickShaderSet.h>

#include "shaders/brick_vert.cpp"
#include "shaders/brick_flat_shaded_frag.cpp"
#include "shaders/brick_phong_frag.cpp"

#include <vsg/all.h>
#include <vsg/io/Logger.h>

using namespace vsgPoints;

#define VIEW_DESCRIPTOR_SET 0
#define MATERIAL_DESCRIPTOR_SET 1

vsg::ref_ptr<vsg::Data> vsgPoints::createParticleImage(uint32_t dim)
{
    auto data = vsg::ubvec4Array2D::create(dim, dim);
    data->properties.format = VK_FORMAT_R8G8B8A8_UNORM;
    float div = 2.0f / static_cast<float>(dim - 1);
    float distance_at_one = 0.5f;
    float distance_at_zero = 1.0f;

    vsg::vec2 v;
    for (uint32_t r = 0; r < dim; ++r)
    {
        v.y = static_cast<float>(r) * div - 1.0f;
        for (uint32_t c = 0; c < dim; ++c)
        {
            v.x = static_cast<float>(c) * div - 1.0f;
            float distance_from_center = vsg::length(v);
            float intensity = 1.0f - (distance_from_center - distance_at_one) / (distance_at_zero - distance_at_one);
            if (intensity > 1.0f) intensity = 1.0f;
            if (intensity < 0.0f) intensity = 0.0f;
            uint8_t alpha = static_cast<uint8_t>(intensity * 255);
            data->set(c, r, vsg::ubvec4(255, 255, 255, alpha));
        }
    }
    return data;
}

vsg::ref_ptr<vsg::ShaderSet> vsgPoints::createPointsFlatShadedShaderSet(vsg::ref_ptr<const vsg::Options> options)
{
    if (options)
    {
        // check if a ShaderSet has already been assigned to the options object, if so return it
        if (auto itr = options->shaderSets.find("points_flat"); itr != options->shaderSets.end()) return itr->second;
    }

    auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/brick.vert", options);
    if (!vertexShader) vertexShader = brick_vert(); // fallback to shaders/brick_vert.cpp

    auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/brick_flat_shaded.frag", options);
    if (!fragmentShader) fragmentShader = brick_flat_shaded_frag();

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});

    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_Normal", "", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_Color", "", 2, VK_FORMAT_R8G8B8A8_UNORM, vsg::ubvec4Array::create(1));
    shaderSet->addAttributeBinding("vsg_PositionScale", "VSG_POSITION_SCALE", 3, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Value::create(0.0f, 0.0f, 0.0, 1.0f));
    shaderSet->addAttributeBinding("vsg_PointSize", "", 4, VK_FORMAT_R32G32_SFLOAT, vsg::vec2Value::create(0.0035f, 0.001f));

    shaderSet->addDescriptorBinding("diffuseMap", "VSG_DIFFUSE_MAP", MATERIAL_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    shaderSet->addDescriptorBinding("material", "", MATERIAL_DESCRIPTOR_SET, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::PhongMaterialValue::create());

    shaderSet->addDescriptorBinding("lightData", "", VIEW_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));
    shaderSet->addDescriptorBinding("viewportData", "", VIEW_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Value::create(0,0, 1280, 1024));
    shaderSet->addDescriptorBinding("shadowMaps", "", VIEW_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));

    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

    shaderSet->optionalDefines = {"VSG_POINT_SPRITE", "VSG_GREYSCALE_DIFFUSE_MAP"};

    shaderSet->customDescriptorSetBindings.push_back(vsg::ViewDependentStateBinding::create(VIEW_DESCRIPTOR_SET));

    return shaderSet;
}

vsg::ref_ptr<vsg::ShaderSet> vsgPoints::createPointsPhongShaderSet(vsg::ref_ptr<const vsg::Options> options)
{
    if (options)
    {
        // check if a ShaderSet has already been assigned to the options object, if so return it
        if (auto itr = options->shaderSets.find("points_phong"); itr != options->shaderSets.end()) return itr->second;
    }

    auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/brick.vert", options);
    if (!vertexShader) vertexShader = brick_vert(); // fallback to shaders/brick_vert.cpp

    auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/brick_phong.frag", options);
    if (!fragmentShader) fragmentShader = brick_phong_frag();

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});

    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_Normal", "", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_Color", "", 2, VK_FORMAT_R8G8B8A8_UNORM, vsg::ubvec4Array::create(1));
    shaderSet->addAttributeBinding("vsg_PositionScale", "VSG_POSITION_SCALE", 3, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Value::create(0.0f, 0.0f, 0.0, 1.0f));
    shaderSet->addAttributeBinding("vsg_PointSize", "", 4, VK_FORMAT_R32G32_SFLOAT, vsg::vec2Value::create(0.0035f, 0.001f));

    shaderSet->addDescriptorBinding("diffuseMap", "VSG_DIFFUSE_MAP", MATERIAL_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    shaderSet->addDescriptorBinding("material", "", MATERIAL_DESCRIPTOR_SET, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::PhongMaterialValue::create());

    shaderSet->addDescriptorBinding("lightData", "", VIEW_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));
    shaderSet->addDescriptorBinding("viewportData", "", VIEW_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Value::create(0,0, 1280, 1024));
    shaderSet->addDescriptorBinding("shadowMaps", "", VIEW_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));

    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

    shaderSet->optionalDefines = {"VSG_GREYSCALE_DIFFUSE_MAP", "VSG_TWO_SIDED_LIGHTING", "VSG_POINT_SPRITE"};

    shaderSet->customDescriptorSetBindings.push_back(vsg::ViewDependentStateBinding::create(VIEW_DESCRIPTOR_SET));

    return shaderSet;
}
