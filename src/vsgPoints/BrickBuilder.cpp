/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgPoints/BrickBuilder.h>

#include <vsg/io/Logger.h>
#include <vsg/all.h>
#include <iostream>

using namespace vsgPoints;

namespace vsgPoints
{

struct SetArray : public vsg::Object
{
    virtual vsg::ref_ptr<vsg::Data> data() = 0;
    virtual void setVertex(size_t i, const vsg::vec3& v) = 0;
    virtual void setNormal(size_t i, const vsg::vec3& n) = 0;
    virtual void setColor(size_t i, const vsg::ubvec4& c) = 0;
};

struct SetArray_vec3Array : public vsg::Inherit<SetArray, SetArray_vec3Array>
{
    SetArray_vec3Array(vsg::ref_ptr<vsg::vec3Array> in_array, VkFormat format = VK_FORMAT_R32G32B32_SFLOAT) : array(in_array) { array->getLayout().format = format; }

    vsg::ref_ptr<vsg::vec3Array> array;

    vsg::vec3 convert(const vsg::ubvec4& v)
    {
        const float multiplier = 1.0f/255.0f;
        return vsg::vec3(static_cast<float>(v.x)*multiplier, static_cast<float>(v.y)*multiplier, static_cast<float>(v.z)*multiplier);
    }

    vsg::ref_ptr<vsg::Data> data() override { return array; }
    void setVertex(size_t i, const vsg::vec3& v) override { array->set(i, v); }
    void setNormal(size_t i, const vsg::vec3& n) override { array->set(i, n); }
    void setColor(size_t i, const vsg::ubvec4& c) override { array->set(i, convert(c)); }
};

struct SetArray_ubvec4Array : public vsg::Inherit<SetArray, SetArray_ubvec4Array>
{
    SetArray_ubvec4Array(vsg::ref_ptr<vsg::ubvec4Array> in_array, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM) : array(in_array) { array->getLayout().format = format; }

    vsg::ref_ptr<vsg::ubvec4Array> array;

    uint8_t convert(float v) const
    {
        const float multiplier = 255.0f;
        v *= multiplier;
        if (v <= 0.0f) return static_cast<uint8_t>(0);
        if (v >= multiplier) return static_cast<uint8_t>(255);
        return static_cast<uint8_t>(v);
    }

    vsg::ubvec4 convert(const vsg::vec3& v) const
    {
        return vsg::ubvec4(convert(v.x), convert(v.y), convert(v.z), 0);
    }

    vsg::ref_ptr<vsg::Data> data() override { return array; }
    void setVertex(size_t i, const vsg::vec3& v) override { array->set(i, convert(v)); }
    void setNormal(size_t i, const vsg::vec3& n) override { array->set(i, convert(n)); }
    void setColor(size_t i, const vsg::ubvec4& c) override { array->set(i, c); }
};

struct Brick : public vsg::Inherit<vsg::Object, Brick>
{
    Brick(const vsg::dvec3& in_position, double brickSize, size_t num, vsg::ref_ptr<vsg::vec3Array> shared_normals, vsg::ref_ptr<vsg::ubvec4Array> shared_colors) :
        end_index(0),
        position(in_position),
        multiplier(1.0f / brickSize),
        positionScale(vsg::vec4Value::create(in_position.x, in_position.y, in_position.z, brickSize))
    {
        //vsg::info("Allocating Brick ", num, " position = ", position, ", brickSize = ", brickSize, ", multiplier = ", multiplier);

        //set_vertices = SetArray_vec3Array::create(vsg::vec3Array::create(num));
        set_vertices = SetArray_ubvec4Array::create(vsg::ubvec4Array::create(num));

        if (shared_normals) set_normals = SetArray_vec3Array::create(shared_normals);
        else set_normals = SetArray_vec3Array::create(vsg::vec3Array::create(num));

        if (shared_colors) set_colors = SetArray_ubvec4Array::create(shared_colors);
        else set_colors = SetArray_ubvec4Array::create(vsg::ubvec4Array::create(num));
    }

    vsg::ref_ptr<vsg::Data> vertices() { return set_vertices->data(); }
    vsg::ref_ptr<vsg::Data> normals() { return set_normals->data(); }
    vsg::ref_ptr<vsg::Data> colors() { return set_colors->data(); }

    virtual void setVertex(size_t i, const vsg::vec3& vertex)
    {
        auto v = (vertex - position) * multiplier;

#if 0
        if (v.x < 0.0f || v.x > 1.0f ||
            v.y < 0.0f || v.y > 1.0f ||
            v.z < 0.0f || v.z > 1.0f)
        {
            vsg::info("fail setVertex(", i, ", vertex = ", vertex, ", v = ", v);
        }
#endif
        set_vertices->setVertex(i, v);
    }

    virtual void setNormal(size_t i, const vsg::vec3& norm)
    {
        set_normals->setNormal(i, norm);
    }

    virtual void setColor(size_t i, const vsg::ubvec4& color)
    {
        set_colors->setColor(i, color);
    }

    size_t end_index;
    vsg::vec3 position;
    float multiplier;
    vsg::ref_ptr<vsg::vec4Value> positionScale;

    vsg::ref_ptr<SetArray> set_vertices;
    vsg::ref_ptr<SetArray> set_normals;
    vsg::ref_ptr<SetArray> set_colors;
};
}

EVSG_type_name(vsgPoints::Brick);

vsg::ref_ptr<vsg::Data> vsgPoints::createParticleImage(uint32_t dim)
{
    auto data = vsg::ubvec4Array2D::create(dim, dim);
    data->getLayout().format = VK_FORMAT_R8G8B8A8_UNORM;
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
    //  TODO : if (!vertexShader) vertexShader = assimp_vert(); // fallback to shaders/assimp_vert.cpp

    auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/assimp_flat_shaded.frag", options);
    //  TODO : if (!fragmentShader) fragmentShader = assimp_flat_shaded_frag();

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});

    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_Normal", "", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_Color", "", 2, VK_FORMAT_R8G8B8A8_UNORM, vsg::ubvec4Array::create(1));
    shaderSet->addAttributeBinding("vsg_PositionScale", "VSG_POSITION_SCALE", 3, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Value::create(vsg::vec4(0.0f, 0.0f, 0.0, 1.0f)));

    shaderSet->addUniformBinding("displacementMap", "VSG_DISPLACEMENT_MAP", 0, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::vec4Array2D::create(1, 1));
    shaderSet->addUniformBinding("diffuseMap", "VSG_DIFFUSE_MAP", 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1));
    shaderSet->addUniformBinding("material", "", 0, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::PhongMaterialValue::create());
    shaderSet->addUniformBinding("viewport", "", 0, 8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::vec4Value::create());
    shaderSet->addUniformBinding("pointSize", "", 0, 9, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::vec2Value::create());

    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

    shaderSet->optionalDefines = {"VSG_POINT_SPRITE", "VSG_GREYSACLE_DIFFUSE_MAP"};

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
    //  TODO : if (!vertexShader) vertexShader = assimp_vert(); // fallback to shaders/assimp_vert.cpp
    auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/assimp_phong.frag", options);
    //  TODO : if (!fragmentShader) fragmentShader = assimp_phong_frag();

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});

    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_Normal", "", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_Color", "", 2, VK_FORMAT_R8G8B8A8_UNORM, vsg::ubvec4Array::create(1));
    shaderSet->addAttributeBinding("vsg_PositionScale", "VSG_POSITION_SCALE", 3, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Value::create(vsg::vec4(0.0f, 0.0f, 0.0, 1.0f)));

    shaderSet->addUniformBinding("diffuseMap", "VSG_DIFFUSE_MAP", 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1));
    shaderSet->addUniformBinding("normalMap", "VSG_NORMAL_MAP", 0, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec3Array2D::create(1, 1));
    shaderSet->addUniformBinding("aoMap", "VSG_LIGHTMAP_MAP", 0, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1));
    shaderSet->addUniformBinding("emissiveMap", "VSG_EMISSIVE_MAP", 0, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1));
    shaderSet->addUniformBinding("material", "", 0, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::PhongMaterialValue::create());
    shaderSet->addUniformBinding("lightData", "", 1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));
    shaderSet->addUniformBinding("viewport", "", 0, 8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::vec4Value::create());
    shaderSet->addUniformBinding("pointSize", "", 0, 9, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::vec2Value::create());

    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

    shaderSet->optionalDefines = {"VSG_GREYSACLE_DIFFUSE_MAP", "VSG_TWO_SIDED_LIGHTING", "VSG_POINT_SPRITE"};

    return shaderSet;
}

void BrickBuilder::add(vsg::ref_ptr<vsg::vec3Array> vertices, vsg::ref_ptr<vsg::vec3Array> normals, vsg::ref_ptr<vsg::ubvec4Array> colors)
{
    vsg::info("BrickBuilder::add(", vertices, ", ", normals, ", ", colors);

    bool perVertexNormals = normals ? (normals->size() == vertices->size()) : false;
    bool perVertexColors = colors ? (colors->size() == vertices->size()) : false;

    vsg::ref_ptr<vsg::vec3Array> shared_normals;
    vsg::ref_ptr<vsg::ubvec4Array> shared_colors;
    if (!perVertexNormals) shared_normals = normals;
    if (!perVertexColors) shared_colors = colors;

    double min_spacing = 1000.0;
    for(size_t i = 1; i<vertices->size(); ++i)
    {
        double spacing = vsg::length(vertices->at(i) - vertices->at(i-1));
        if (spacing < min_spacing) min_spacing = spacing;
    }

    vsg::info("min_spacing = ", min_spacing);

    for(auto& vertex : *vertices)
    {
        extents.add(vertex);
    }

    vsg::info("extents ", extents);

    double precision = std::min(0.01, min_spacing*0.5);
    double type_limit = 256;

    vsg::info("precision ", precision);

    double brickSize = precision * type_limit;
    num_x = std::ceil((extents.max.x - extents.min.x) / brickSize);
    num_y = std::ceil((extents.max.y - extents.min.y) / brickSize);
    num_z = std::ceil((extents.max.z - extents.min.z) / brickSize);

    vsg::info("brickSize ", brickSize);
    vsg::info("num_x ", num_x);
    vsg::info("num_y ", num_y);
    vsg::info("num_z ", num_z);

    std::vector<size_t> pointsPerBrick(num_x * num_y * num_z);
    size_t numBricks = 0;
    for(auto& vertex : *vertices)
    {
        auto brick_position = (vsg::dvec3(vertex) - extents.min) / brickSize;
        auto brick_i = std::floor(brick_position.x);
        auto brick_j = std::floor(brick_position.y);
        auto brick_k = std::floor(brick_position.z);
        auto index = brick_i + brick_j * (num_x) + brick_k * (num_x * num_y);
        if (pointsPerBrick[index]==0) ++numBricks; // first point in brick, so requires a Brick
        ++pointsPerBrick[index];
    }

#if 0
    for(auto num : pointsPerBrick)
    {
        vsg::info("num ", num);
    }
#endif

    std::vector<vsg::ref_ptr<Brick>> activeBricks;
    activeBricks.reserve(numBricks);

    std::vector<vsg::ref_ptr<Brick>> bricks(num_x * num_y * num_z);
    for(size_t i = 0; i < vertices->size(); ++i)
    {
        auto& vertex = vertices->at(i);
        auto brick_position = (vsg::dvec3(vertex) - extents.min) / brickSize;
        auto brick_i = std::floor(brick_position.x);
        auto brick_j = std::floor(brick_position.y);
        auto brick_k = std::floor(brick_position.z);
        auto index = brick_i + brick_j * (num_x) + brick_k * (num_x * num_y);
        auto& brick = bricks[index];
        if (!brick)
        {
            auto brick_origin = extents.min + vsg::dvec3(brick_i * brickSize, brick_j * brickSize, brick_k * brickSize);
            brick = Brick::create(brick_origin, brickSize, pointsPerBrick[index], shared_normals, shared_colors);
            activeBricks.push_back(brick);
        }

        brick->setVertex(brick->end_index, vertex);
        if (perVertexNormals) brick->setNormal(brick->end_index, normals->at(i));
        if (perVertexColors) brick->setColor(brick->end_index, colors->at(i));
        ++(brick->end_index);
    }

    if (activeBricks.empty()) return;


    auto pointSize = vsg::vec2Value::create();
    pointSize->value().set(min_spacing*3.0f, min_spacing);

    auto textureData = createParticleImage(64);
    auto shaderSet = perVertexNormals ? vsgPoints::createPointsPhongShaderSet(options) : vsgPoints::createPointsFlatShadedShaderSet(options);
    auto config = vsg::GraphicsPipelineConfig::create(shaderSet);
    bool blending = false;

    auto& defines = config->shaderHints->defines;
    defines.push_back("VSG_POINT_SPRITE");

    auto first_brick = activeBricks.front();
    auto first_vertices = first_brick->vertices();
    config->enableArray("vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, first_vertices->getLayout().stride, first_vertices->getLayout().format);
    //config->enableArray("vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, sizeof(vsg::vec3), VK_FORMAT_R32G32B32_SFLOAT);
    //config->enableArray("vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, sizeof(vsg::ubvec4), VK_FORMAT_R8G8B8A8_UNORM);

    config->enableArray("vsg_Normal", perVertexNormals ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE, sizeof(vsg::vec3));
    config->enableArray("vsg_Color", perVertexColors ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE, sizeof(vsg::ubvec4));
    config->enableArray("vsg_PositionScale", VK_VERTEX_INPUT_RATE_INSTANCE, sizeof(vsg::vec4));

    vsg::Descriptors descriptors;
    if (textureData)
    {
        auto sampler = vsg::Sampler::create();
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        config->assignTexture(descriptors, "diffuseMap", textureData, sampler);
    }

    config->assignUniform(descriptors, "viewport", viewport);
    config->assignUniform(descriptors, "pointSize", pointSize);

    auto mat = vsg::PhongMaterialValue::create();
    mat->value().alphaMask = 1.0f;
    mat->value().alphaMaskCutoff = 0.0025f;
    config->assignUniform(descriptors, "material", mat);

    auto vdsl = vsg::ViewDescriptorSetLayout::create();
    config->additionalDescrptorSetLayout = vdsl;

    config->colorBlendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{
        {blending, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_SUBTRACT, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT}};

    config->inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    config->init();

    auto descriptorSet = vsg::DescriptorSet::create(config->descriptorSetLayout, descriptors);
    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, config->layout, 0, descriptorSet);

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto stateGroup = vsg::StateGroup::create();
    stateGroup->add(config->bindGraphicsPipeline);
    stateGroup->add(bindDescriptorSet);

    // assign any custom ArrayState that may be required.
    stateGroup->prototypeArrayState = shaderSet->getSuitableArrayState(config->shaderHints->defines);

    auto bindViewDescriptorSets = vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, config->layout, 1);
    stateGroup->add(bindViewDescriptorSets);

    for(auto& brick : activeBricks)
    {
        //vsg::info("brick ", brick, ", positiveScale = ", brick->positionScale->value(), " vertices ", brick->vertices, ",  ", brick->vertices->size(), ",  ", brick->normals->size(), ",  ", brick->colors->size());

        auto brick_vertices = brick->vertices();

        vsg::DataList arrays;
        arrays.push_back(brick_vertices);
        arrays.push_back(brick->normals());
        arrays.push_back(brick->colors());
        arrays.push_back(brick->positionScale);

        auto bindVertexBuffers = vsg::BindVertexBuffers::create();
        bindVertexBuffers->assignArrays(arrays);

        auto commands = vsg::Commands::create();
        commands->addChild(bindVertexBuffers);
        commands->addChild(vsg::Draw::create(brick_vertices->valueCount(), 1, 0, 0));

        stateGroup->addChild(commands);
    }

    root = stateGroup;

    vsg::info("activeBricks = ", activeBricks.size(), ", bricks.size() = ", bricks.size());
}

vsg::ref_ptr<vsg::Node> BrickBuilder::build()
{
    return root;
}
