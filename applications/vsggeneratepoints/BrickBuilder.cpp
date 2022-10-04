#include "BrickBuilder.h"
#include <vsg/io/Logger.h>
#include <vsg/all.h>
#include <iostream>

using namespace vsgPoints;

namespace vsgPoints
{
struct Brick : public vsg::Inherit<vsg::Object, Brick>
{
    Brick(const vsg::dvec3& in_position, double brickSize, size_t num, vsg::ref_ptr<vsg::vec3Array> shared_normals, vsg::ref_ptr<vsg::ubvec4Array> shared_colors) :
        end_index(0),
        position(in_position),
        multiplier(1.0f / brickSize),
        positionScale(vsg::vec4Value::create(in_position.x, in_position.y, in_position.z, brickSize)),
        vertices(vsg::vec3Array::create(num)),
        normals(shared_normals),
        colors(shared_colors)
    {
        vsg::info("Allocating Brick ", num, " position = ", position, ", brickSize = ", brickSize);

        if (!normals) normals = vsg::vec3Array::create(num);
        if (!colors) colors = vsg::ubvec4Array::create(num);
    }

    virtual void setVertex(size_t i, const vsg::vec3& vertex)
    {
        auto v = (vertex - position) * multiplier;
        vertices->set(i, v);
    }

    virtual void setNormal(size_t i, const vsg::vec3& norm)
    {
        normals->set(i, norm);
    }

    virtual void setColor(size_t i, const vsg::ubvec4& color)
    {
        colors->set(i, color);
    }

    size_t end_index;
    vsg::vec3 position;
    float multiplier;
    vsg::ref_ptr<vsg::vec4Value> positionScale;
    vsg::ref_ptr<vsg::vec3Array> vertices;
    vsg::ref_ptr<vsg::vec3Array> normals;
    vsg::ref_ptr<vsg::ubvec4Array> colors;
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

vsg::ref_ptr<vsg::StateGroup> vsgPoints::createStateGroup(vsg::ref_ptr<const vsg::Options> options, vsg::ref_ptr<vsg::vec4Value> viewport, vsg::ref_ptr<vsg::vec2Value> pointSize, bool lighting, VkVertexInputRate normalInputRate, VkVertexInputRate colorInputRate)
{
    bool blending = false;

    for (auto& path : options->paths)
    {
        std::cout << "path = " << path << std::endl;
    }

    // load shaders
    auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/brick.vert", options);
    //if (!vertexShader) vertexShader = assimp_vert(); // fallback to shaders/assimp_vert.cpp

    vsg::ref_ptr<vsg::ShaderStage> fragmentShader;
    if (lighting)
    {
        fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/assimp_phong.frag", options);
        //if (!fragmentShader) fragmentShader = assimp_phong_frag();
    }
    else
    {
        fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/assimp_flat_shaded.frag", options);
        //if (!fragmentShader) fragmentShader = assimp_flat_shaded_frag();
    }

    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        std::cout << "vertexShader = " << vertexShader << std::endl;
        std::cout << "fragmentShader = " << fragmentShader << std::endl;

        return {};
    }

    vsg::info("vertexShader = ", vertexShader);
    vsg::info("fragmentShader = ", fragmentShader);

    auto shaderHints = vsg::ShaderCompileSettings::create();
    auto& defines = shaderHints->defines;

    vertexShader->module->hints = shaderHints;
    vertexShader->module->code = {};

    fragmentShader->module->hints = shaderHints;
    fragmentShader->module->code = {};

    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings;

    // enable the point sprite code paths
    defines.push_back("VSG_POINT_SPRITE");
    defines.push_back("VSG_POSITION_SCALE");

    vsg::ref_ptr<vsg::Data> textureData;
#if 0
    textureData = vsg::read_cast<vsg::Data>("textures/lz.vsgb", options);
#else
    textureData = createParticleImage(64);
#endif
    if (textureData)
    {
        std::cout << "textureData = " << textureData << std::endl;

        // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
        descriptorBindings.push_back(VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
        defines.push_back("VSG_DIFFUSE_MAP");
    }

    {
        descriptorBindings.push_back(VkDescriptorSetLayoutBinding{8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}); // Viewport
        descriptorBindings.push_back(VkDescriptorSetLayoutBinding{9, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}); // PointSize
        descriptorBindings.push_back(VkDescriptorSetLayoutBinding{10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}); // Material
    }

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::DescriptorSetLayouts descriptorSetLayouts{descriptorSetLayout};

    if (lighting)
    {
        auto viewDescriptorSetLayout = vsg::ViewDescriptorSetLayout::create();
        descriptorSetLayouts.push_back(viewDescriptorSetLayout);
    }

    vsg::PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls automatically provided by the VSG's DispatchTraversal
    };

    auto pipelineLayout = vsg::PipelineLayout::create(descriptorSetLayouts, pushConstantRanges);

    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX} // vertex data
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
#if VERTEX_TYPE==4
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0} // vertex data
#else
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R8G8B8A8_UNORM, 0} // vertex data
#endif
    };

    vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{1, sizeof(vsg::vec3), normalInputRate});  // normal data
    vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}); // normal data

    vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{2, 4, colorInputRate});                 // color data
    vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R8G8B8A8_UNORM, 0}); // color data

    vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{3, sizeof(vsg::vec4), VK_VERTEX_INPUT_RATE_INSTANCE});  // position and scale data
    vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{3, 3, VK_FORMAT_R32G32B32A32_SFLOAT, 0}); // position and scale data

    auto rasterState = vsg::RasterizationState::create();

    auto colorBlendState = vsg::ColorBlendState::create();
    colorBlendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{
        {blending, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_SUBTRACT, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT}};

    auto inputAssemblyState = vsg::InputAssemblyState::create();
    inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    vsg::GraphicsPipelineStates pipelineStates{
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        inputAssemblyState,
        rasterState,
        vsg::MultisampleState::create(),
        colorBlendState,
        vsg::DepthStencilState::create()};

    auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);

    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    // create texture image and associated DescriptorSets and binding

    vsg::Descriptors descriptors;
    if (textureData)
    {
        auto sampler = vsg::Sampler::create();
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        auto texture = vsg::DescriptorImage::create(sampler, textureData, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        descriptors.push_back(texture);
    }

    descriptors.push_back( vsg::DescriptorBuffer::create(viewport, 8) );
    descriptors.push_back( vsg::DescriptorBuffer::create(pointSize, 9) );

    auto mat = vsg::PhongMaterialValue::create();
    mat->value().alphaMask = 1.0f;
    mat->value().alphaMaskCutoff = 0.0025f;

    auto material = vsg::DescriptorBuffer::create(mat, 10);
    descriptors.push_back(material);


    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, descriptors);
    auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, vsg::DescriptorSets{descriptorSet});

    auto sg = vsg::StateGroup::create();
    sg->add(bindGraphicsPipeline);
    sg->add(bindDescriptorSets);

    if (lighting)
    {
        sg->add(vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1));
    }

    return sg;
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
        auto brick_position = (vsg::dvec3(vertex) - extents.min)/ brickSize;
        auto brick_i = std::floor(brick_position.x);
        auto brick_j = std::floor(brick_position.y);
        auto brick_k = std::floor(brick_position.z);
        auto index = brick_i + brick_j * (num_y) + brick_k * (num_y * num_z);
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
        auto brick_position = (vsg::dvec3(vertex) - extents.min)/ brickSize;
        auto brick_i = std::floor(brick_position.x);
        auto brick_j = std::floor(brick_position.y);
        auto brick_k = std::floor(brick_position.z);
        auto index = brick_i + brick_j * (num_y) + brick_k * (num_y * num_z);
        auto& brick = bricks[index];
        if (!brick)
        {
            brick = Brick::create(brick_position, brickSize, pointsPerBrick[index], shared_normals, shared_colors);
            activeBricks.push_back(brick);
        }

        brick->setVertex(brick->end_index, vertex);
        if (perVertexNormals) brick->setNormal(brick->end_index, normals->at(i));
        if (perVertexColors) brick->setColor(brick->end_index, colors->at(i));
        ++(brick->end_index);
    }

    auto pointSize = vsg::vec2Value::create();
    pointSize->value().set(min_spacing*3.0f, min_spacing);

    bool lighting = perVertexNormals;
    VkVertexInputRate normalInputRate = perVertexNormals ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
    VkVertexInputRate colorInputRate = perVertexColors ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;

    auto sg = vsgPoints::createStateGroup(options, viewport, pointSize, lighting, normalInputRate, colorInputRate);

    for(auto& brick : activeBricks)
    {
        //vsg::info("brick ", brick, ", positiveScale = ", brick->positionScale->value(), " vertices ", brick->vertices, ",  ", brick->vertices->size(), ",  ", brick->normals->size(), ",  ", brick->colors->size());

        vsg::DataList arrays;
        arrays.push_back(brick->vertices);
        arrays.push_back(brick->normals);
        arrays.push_back(brick->colors);
        arrays.push_back(brick->positionScale);

        auto bindVertexBuffers = vsg::BindVertexBuffers::create();
        bindVertexBuffers->assignArrays(arrays);

        auto commands = vsg::Commands::create();
        commands->addChild(bindVertexBuffers);
        commands->addChild(vsg::Draw::create(brick->vertices->size(), 1, 0, 0));

        sg->addChild(commands);
    }

    root = sg;

    vsg::info("activeBricks = ", activeBricks.size(), ", bricks.size() = ", bricks.size());
}

vsg::ref_ptr<vsg::Node> BrickBuilder::build()
{
    return root;
}
