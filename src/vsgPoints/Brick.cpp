/* <editor-fold desc="MIT License">

Copyright(c) 2023 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgPoints/Brick.h>
#include <vsgPoints/BrickShaderSet.h>

#include <vsg/io/Logger.h>
#include <vsg/io/write.h>
#include <vsg/state/material.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/nodes/VertexDraw.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/LOD.h>
#include <vsg/nodes/PagedLOD.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>

#include <iostream>

using namespace vsgPoints;

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Brick
//
Brick::Brick()
{
}

Brick::~Brick()
{
}

vsg::ref_ptr<vsg::Node> Brick::createRendering(const Settings& settings, const vsg::vec4& positionScale, const vsg::vec2& pointSize)
{
    vsg::ref_ptr<vsg::Data> vertices;

    auto normals = vsg::vec3Value::create(vsg::vec3(0.0f, 0.0f, 1.0f));
    auto colors = vsg::ubvec4Array::create(points.size(), vsg::Data::Properties(VK_FORMAT_R8G8B8A8_UNORM));
    auto positionScaleValue = vsg::vec4Value::create(positionScale);
    auto pointSizeValue = vsg::vec2Value::create(pointSize);

    normals->properties.format = VK_FORMAT_R32G32B32_SFLOAT;
    positionScaleValue->properties.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    pointSizeValue->properties.format = VK_FORMAT_R32G32_SFLOAT;

    if (settings.bits==8)
    {
        auto vertices_8bit = vsg::ubvec3Array::create(points.size(), vsg::Data::Properties(VK_FORMAT_R8G8B8_UNORM));
        auto vertex_itr = vertices_8bit->begin();
        auto color_itr = colors->begin();
        for(auto& point : points)
        {
            (vertex_itr++)->set(static_cast<uint8_t>(point.v.x), static_cast<uint8_t>(point.v.y), static_cast<uint8_t>(point.v.z));
            *(color_itr++) = point.c;
        }

        vertices = vertices_8bit;
    }
    else if (settings.bits==10)
    {
        auto vertices_10bit = vsg::uintArray::create(points.size(), vsg::Data::Properties(VK_FORMAT_A2R10G10B10_UNORM_PACK32));
        auto vertex_itr = vertices_10bit->begin();
        auto color_itr = colors->begin();
        for(auto& point : points)
        {
            *(vertex_itr++) = 3 << 30 | (static_cast<uint32_t>(point.v.x) << 20) | (static_cast<uint32_t>(point.v.y) << 10) | (static_cast<uint32_t>(point.v.z));
            *(color_itr++) = point.c;
        }

        vertices = vertices_10bit;
    }
    else if (settings.bits==16)
    {
        auto vertices_16bit = vsg::usvec3Array::create(points.size(), vsg::Data::Properties(VK_FORMAT_R16G16B16_UNORM));
        auto vertex_itr = vertices_16bit->begin();
        auto color_itr = colors->begin();
        for(auto& point : points)
        {
            (vertex_itr++)->set(static_cast<uint16_t>(point.v.x), static_cast<uint16_t>(point.v.y), static_cast<uint16_t>(point.v.z));
            *(color_itr++) = point.c;
        }

        vertices = vertices_16bit;
    }
    else
    {
        return {};
    }


    // set up vertexDraw that will do the rendering.
    auto vertexDraw = vsg::VertexDraw::create();
    vertexDraw->assignArrays({vertices, normals, colors, positionScaleValue, pointSizeValue});
    vertexDraw->vertexCount = points.size();
    vertexDraw->instanceCount = 1;

    return vertexDraw;
}

vsg::ref_ptr<vsg::Node> Brick::createRendering(const Settings& settings, Key key, vsg::dbox& bound)
{
    double brickPrecision = settings.precision * static_cast<double>(key.w);
    double brickSize = brickPrecision * pow(2.0, static_cast<double>(settings.bits));

    vsg::dvec3 position(static_cast<double>(key.x) * brickSize, static_cast<double>(key.y) * brickSize, static_cast<double>(key.z) * brickSize);
    position -= settings.offset;

    for(auto& point : points)
    {
        auto& v = point.v;
        bound.add(position.x + brickPrecision * static_cast<double>(v.x),
                  position.y + brickPrecision * static_cast<double>(v.y),
                  position.z + brickPrecision * static_cast<double>(v.z));
    }

    vsg::vec2 pointSize(brickPrecision*settings.pointSize, brickPrecision);
    vsg::vec4 positionScale(position.x, position.y, position.z, brickSize);

    return createRendering(settings, positionScale, pointSize);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Bricks
//
Bricks::Bricks(vsg::ref_ptr<Settings> in_settings) :
    settings(in_settings)
{
}

void Bricks::add(const vsg::dvec3& v, const vsg::ubvec4& c)
{
    settings->bound.add(v);

    double multiplier = 1.0 / settings->precision;
    int64_t divisor = 1 << settings->bits;
    int64_t mask = divisor - 1;

    vsg::dvec3 scaled_v = v * multiplier;
    vsg::dvec3 rounded_v = {std::round(scaled_v.x), std::round(scaled_v.y), std::round(scaled_v.z)};
    vsg::t_vec3<int64_t> int64_v = {static_cast<int64_t>(rounded_v.x),  static_cast<int64_t>(rounded_v.y), static_cast<int64_t>(rounded_v.z)};
    Key key = { static_cast<int32_t>(int64_v.x / divisor), static_cast<int32_t>(int64_v.y / divisor), static_cast<int32_t>(int64_v.z / divisor), 1};

    PackedPoint packedPoint;
    packedPoint.v.set(static_cast<uint16_t>(int64_v.x & mask), static_cast<uint16_t>(int64_v.y & mask), static_cast<uint16_t>(int64_v.z & mask));
    packedPoint.c.set(c.r, c.g, c.b, c.a) ;

    // std::cout<<"Bricks::add("<<v<<") key = "<<key<<", scaled_v = "<<scaled_v<<", rounded_v = "<<rounded_v<<", packedPoint.v = "<<packedPoint.v<<std::endl;


    auto& brick = bricks[key];
    if (!brick)
    {
        brick = Brick::create();
    }

    brick->points.push_back(packedPoint);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Help functions
//
bool vsgPoints::generateLevel(vsgPoints::Bricks& source, vsgPoints::Bricks& destination, const vsgPoints::Settings& settings)
{
    int32_t bits = settings.bits;
    for(auto& [source_key, source_brick] : source)
    {
        vsgPoints::Key destination_key = {source_key.x / 2, source_key.y / 2, source_key.z / 2, source_key.w * 2};
        vsg::ivec3 offset = {(source_key.x & 1) << bits, (source_key.y & 1) << bits, (source_key.z & 1) << bits};

        auto& destinatio_brick = destination[destination_key];
        if (!destinatio_brick) destinatio_brick = vsgPoints::Brick::create();

        auto& source_points = source_brick->points;
        auto& desintation_points = destinatio_brick->points;
        size_t count = source_points.size();
        for(size_t i = 0; i < count; i+= 4)
        {
            auto& p = source_points[i];

            vsgPoints::PackedPoint new_p;
            new_p.v.x = static_cast<uint16_t>((static_cast<int32_t>(p.v.x) + offset.x)/2);
            new_p.v.y = static_cast<uint16_t>((static_cast<int32_t>(p.v.y) + offset.y)/2);
            new_p.v.z = static_cast<uint16_t>((static_cast<int32_t>(p.v.z) + offset.z)/2);
            new_p.c = p.c;

            desintation_points.push_back(new_p);
        }

    }
    return !destination.empty();
}

vsg::ref_ptr<vsg::StateGroup> vsgPoints::createStateGroup(const vsgPoints::Settings& settings)
{
    auto textureData = vsgPoints::createParticleImage(64);
    auto shaderSet = vsgPoints::createPointsFlatShadedShaderSet(settings.options);
    auto config = vsg::GraphicsPipelineConfig::create(shaderSet);
    bool blending = false;

    auto& defines = config->shaderHints->defines;
    defines.insert("VSG_POINT_SPRITE");

    if (settings.bits==8)
    {
        config->enableArray("vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, sizeof(vsg::ubvec3), VK_FORMAT_R8G8B8_UNORM);
    }
    else if (settings.bits==10)
    {
        config->enableArray("vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, 4, VK_FORMAT_A2R10G10B10_UNORM_PACK32);
    }
    else if (settings.bits==16)
    {
        config->enableArray("vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, sizeof(vsg::usvec3), VK_FORMAT_R16G16B16_UNORM);
    }
    else
    {
        vsg::info("Unsupported number of bits ", settings.bits);
        return {};
    }

    config->enableArray("vsg_Normal", VK_VERTEX_INPUT_RATE_INSTANCE, sizeof(vsg::vec3), VK_FORMAT_R32G32B32_SFLOAT);
    config->enableArray("vsg_Color", VK_VERTEX_INPUT_RATE_VERTEX, sizeof(vsg::ubvec4), VK_FORMAT_R8G8B8A8_UNORM);
    config->enableArray("vsg_PositionScale", VK_VERTEX_INPUT_RATE_INSTANCE, sizeof(vsg::vec4), VK_FORMAT_R32G32B32A32_SFLOAT);
    config->enableArray("vsg_PointSize", VK_VERTEX_INPUT_RATE_INSTANCE, sizeof(vsg::vec2), VK_FORMAT_R32G32_SFLOAT);

    vsg::Descriptors descriptors;
    if (textureData)
    {
        auto sampler = vsg::Sampler::create();
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        config->assignTexture(descriptors, "diffuseMap", textureData, sampler);
    }

    auto mat = vsg::PhongMaterialValue::create();
    mat->value().alphaMask = 1.0f;
    mat->value().alphaMaskCutoff = 0.0025f;
    config->assignUniform(descriptors, "material", mat);

    auto vdsl = vsg::ViewDescriptorSetLayout::create();
    config->additionalDescriptorSetLayout = vdsl;

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

    return stateGroup;
}

vsg::ref_ptr<vsg::Node> vsgPoints::subtile(vsgPoints::Settings& settings, vsgPoints::Levels::reverse_iterator level_itr, vsgPoints::Levels::reverse_iterator end_itr, vsgPoints::Key key, vsg::dbox& bound, bool root)
{
    if (level_itr == end_itr) return {};

    auto& bricks = *level_itr;
    auto itr = bricks->find(key);
    if (itr == bricks->end())
    {
        //std::cout<<"   "<<key<<" null "<<std::endl;
        return {};
    }

    auto& brick = (itr->second);
    auto next_itr = level_itr;
    ++next_itr;

    if (next_itr != end_itr)
    {
        std::array<vsg::ref_ptr<vsg::Node>, 8> children;
        size_t num_children = 0;

        vsgPoints::Key subkey{key.x * 2, key.y * 2, key.z * 2, key.w / 2};

        vsg::dbox subtiles_bound;

        if (auto child = subtile(settings, next_itr, end_itr, subkey, subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey+vsgPoints::Key(1, 0, 0, 0), subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey+vsgPoints::Key(0, 1, 0, 0), subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey+vsgPoints::Key(1, 1, 0, 0), subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey+vsgPoints::Key(0, 0, 1, 0), subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey+vsgPoints::Key(1, 0, 1, 0), subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey+vsgPoints::Key(0, 1, 1, 0), subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey+vsgPoints::Key(1, 1, 1, 0), subtiles_bound)) children[num_children++] = child;

        vsg::dbox local_bound;
        auto brick_node = brick->createRendering(settings, key, local_bound);

        if (num_children == 0)
        {
            //vsg::info("Warning: unable to set PagedLOD bounds, key = ",key,", num_children = ", num_children, ", brick_node = ", brick_node, ", brick->points.size() = ",  brick->points.size());
            return brick_node;
        }

        double transition = settings.transition;
        vsg::dsphere bs;

        if (subtiles_bound.valid())
        {
            bound.add(subtiles_bound);
            bs.center = (subtiles_bound.min + subtiles_bound.max) * 0.5;
            bs.radius = vsg::length(subtiles_bound.max - subtiles_bound.min) * 0.5;

            double brickPrecision = settings.precision * static_cast<double>(key.w);
            double brickSize = brickPrecision * pow(2.0, static_cast<double>(settings.bits));

            vsg::dvec3 subtilesSize = (subtiles_bound.max - subtiles_bound.min);
            double maxSize = std::max(brickPrecision, std::max(std::max(subtilesSize.x, subtilesSize.y), subtilesSize.z));

            transition *= (maxSize/brickSize);

            // vsg::info("maxSize = ", maxSize,", brickSize = ",brickSize,", ratio = ",maxSize/brickSize);
        }
        else
        {
            vsg::info("Warning: unable to set PagedLOD bounds, num_children = ", num_children );
            bs.center = (local_bound.min + local_bound.max) * 0.5;
            bs.radius = vsg::length(local_bound.max - local_bound.max) * 0.5;
        }


        if (settings.plod)
        {
            vsg::Path path = vsg::make_string(settings.path,"/",key.w,"/",key.z,"/",key.y);
            vsg::Path filename = vsg::make_string(key.x, settings.extension);
            vsg::Path full_path = path/filename;

            vsg::makeDirectory(path);

            if (num_children==1)
            {
                vsg::write(children[0], full_path);
            }
            else
            {
                auto group = vsg::Group::create();
                for(size_t i = 0; i < num_children; ++i)
                {
                    group->addChild(children[i]);
                }
                vsg::write(group, full_path);
            }

            auto plod = vsg::PagedLOD::create();
            plod->bound = bs;
            plod->children[0] = vsg::PagedLOD::Child{transition, {}}; // external child visible when it's bound occupies more than ~1/4 of the height of the window
            plod->children[1] = vsg::PagedLOD::Child{0.0, brick_node}; // visible always

            if (root)
            {
                plod->filename = full_path;
            }
            else
            {
                plod->filename = vsg::Path("../../../..")/full_path;
            }

            //vsg::info("plod ", key, " ", brick, " plod ", plod, ", brick->points.size() = ",  brick->points.size());

            return plod;
        }
        else
        {
            vsg::ref_ptr<vsg::Node> child_node;
            if (num_children==1)
            {
                child_node = children[0];
            }
            else
            {
                auto group = vsg::Group::create();
                for(size_t i = 0; i < num_children; ++i)
                {
                    group->addChild(children[i]);
                }

                child_node = group;
            }

            auto lod = vsg::LOD::create();
            lod->bound = bs;
            lod->addChild(vsg::LOD::Child{transition, child_node}); // high res child
            lod->addChild(vsg::LOD::Child{0.0, brick_node}); // lower res child

            //vsg::info("lod ", key, " ", brick, " lod ", lod, ", brick->points.size() = ",  brick->points.size());

            return lod;
        }
    }
    else
    {
        auto leaf = brick->createRendering(settings, key, bound);
        //vsg::info("leaf  ",key, " ", brick, " leaf ", leaf, ", bound ", bound, ", brick->points.size() = ",  brick->points.size());
        return leaf;
    }

    return vsg::Node::create();
}

vsg::ref_ptr<vsg::Node> vsgPoints::createPagedLOD(vsgPoints::Levels& levels, vsgPoints::Settings& settings)
{
    if (levels.empty()) return {};

    auto stateGroup = createStateGroup(settings);

    std::basic_ostringstream<vsg::Path::value_type> str;

    double brickSize = settings.precision * pow(2.0, static_cast<double>(settings.bits));
    double rootBrickSize = brickSize * std::pow(2.0, levels.size()-1);

    vsg::info("rootBrickSize  = ", rootBrickSize);

    // If only one level is present then PagedLOD not required so just add all the levels bricks to the StateGroup
    if (levels.size() == 1)
    {
        vsg::dbox bound;
        for(auto& [key, brick] : *(levels.back()))
        {
            auto tile = brick->createRendering(settings, key, bound);
            stateGroup->addChild(tile);
        }

        return stateGroup;
    }

    // more than 1 level so create a PagedLOD hierarchy.

    auto current_itr = levels.rbegin();

    // root tile
    auto& root_level = *current_itr;
    vsg::info("root level ", root_level->size());

    for(auto& [key, brick] : *root_level)
    {
        vsg::info("root key = ", key, " ",brick);
        vsg::dbox bound;
        if (auto child = subtile(settings, current_itr, levels.rend(), key, bound, true))
        {
            vsg::info("root child ", child);
            stateGroup->addChild(child);
        }
    }

    return stateGroup;
}
