/* <editor-fold desc="MIT License">

Copyright(c) 2023 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgPoints/BrickShaderSet.h>
#include <vsgPoints/create.h>

#include <vsg/io/Logger.h>
#include <vsg/io/write.h>
#include <vsg/nodes/CullGroup.h>
#include <vsg/nodes/LOD.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/nodes/PagedLOD.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/VertexDraw.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/state/material.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>

#include <iostream>

using namespace vsgPoints;

vsg::ref_ptr<vsg::Node> vsgPoints::createSceneGraph(vsg::ref_ptr<vsgPoints::Bricks> bricks, vsg::ref_ptr<vsgPoints::Settings> settings)
{
    if (bricks->empty())
    {
        vsg::warn("createSceneGraph(", bricks, ", ", settings, ") bricks is empty(), cannot create scene graph.");
        return {};
    }

    if (settings->createType == vsgPoints::CREATE_FLAT)
    {

        auto cullGroup = vsg::CullGroup::create();
        cullGroup->bound.center = (settings->bound.max + settings->bound.min) * 0.5;
        cullGroup->bound.radius = vsg::length(settings->bound.max - settings->bound.min) * 0.5;

        if (settings->bound.valid())
        {
            settings->offset = (settings->bound.max + settings->bound.min) * 0.5;
        }

        auto transform = vsg::MatrixTransform::create();
        transform->matrix = vsg::translate(settings->offset);
        cullGroup->addChild(transform);

        auto group = vsgPoints::createStateGroup(*settings);
        transform->addChild(group);

        vsg::dbox bound;
        for (auto& [key, brick] : *bricks)
        {
            if (auto node = brick->createRendering(*(bricks->settings), key, bound))
            {
                group->addChild(node);
            }
        }

        return cullGroup;
    }
    else
    {
        auto transform = vsg::MatrixTransform::create();

        vsg::t_box<int32_t> keyBounds;
        for (auto& [key, brick] : *bricks)
        {
            keyBounds.add(key.x, key.y, key.z);
        }

        auto key_origin = keyBounds.min;
        auto translated_bricks = Bricks::create();
        for (auto& [key, brick] : *bricks)
        {
            (*translated_bricks)[Key{key.x - key_origin.x, key.y - key_origin.y, key.z - key_origin.z, key.w}] = brick;
        }

        bricks = translated_bricks;

        double brickPrecision = settings->precision;
        double brickSize = brickPrecision * pow(2.0, static_cast<double>(settings->bits));

        vsg::dvec3 offset(static_cast<double>(key_origin.x) * brickSize,
                          static_cast<double>(key_origin.y) * brickSize,
                          static_cast<double>(key_origin.z) * brickSize);

        settings->offset = vsg::dvec3(0.0, 0.0, 0.0);

        transform->matrix = vsg::translate(offset);

        vsgPoints::Levels levels;
        levels.push_back(bricks);

        while (levels.back()->size() > 1)
        {
            auto& source = levels.back();

            levels.push_back(vsgPoints::Bricks::create());
            auto& destination = levels.back();

            if (!vsgPoints::generateLevel(*source, *destination, *settings)) break;
        }

        vsg::debug("levels = ", levels.size());

        if (auto model = createPagedLOD(levels, *settings))
        {
            transform->addChild(model);
        }

        return transform;
    }
}

bool vsgPoints::generateLevel(vsgPoints::Bricks& source, vsgPoints::Bricks& destination, const vsgPoints::Settings& settings)
{
    int32_t bits = settings.bits;
    for (auto& [source_key, source_brick] : source)
    {
        vsgPoints::Key destination_key = {source_key.x / 2, source_key.y / 2, source_key.z / 2, source_key.w * 2};
        vsg::ivec3 offset = {(source_key.x & 1) << bits, (source_key.y & 1) << bits, (source_key.z & 1) << bits};

        auto& destination_brick = destination[destination_key];
        if (!destination_brick) destination_brick = vsgPoints::Brick::create();

        auto& source_points = source_brick->points;
        auto& destination_points = destination_brick->points;
        size_t count = source_points.size();
        for (size_t i = 0; i < count; i += 4)
        {
            auto& p = source_points[i];

            vsgPoints::PackedPoint new_p;
            new_p.v.x = static_cast<uint16_t>((static_cast<int32_t>(p.v.x) + offset.x) / 2);
            new_p.v.y = static_cast<uint16_t>((static_cast<int32_t>(p.v.y) + offset.y) / 2);
            new_p.v.z = static_cast<uint16_t>((static_cast<int32_t>(p.v.z) + offset.z) / 2);
            new_p.c = p.c;

            destination_points.push_back(new_p);
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

    if (settings.bits == 8)
    {
        config->enableArray("vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, sizeof(vsg::ubvec3), VK_FORMAT_R8G8B8_UNORM);
    }
    else if (settings.bits == 10)
    {
        config->enableArray("vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, 4, VK_FORMAT_A2R10G10B10_UNORM_PACK32);
    }
    else if (settings.bits == 16)
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

    if (textureData)
    {
        auto sampler = vsg::Sampler::create();
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        config->assignTexture("diffuseMap", textureData, sampler);
    }

    auto mat = vsg::PhongMaterialValue::create();
    mat->value().alphaMask = 1.0f;
    mat->value().alphaMaskCutoff = 0.0025f;
    config->assignUniform("material", mat);

    struct SetPipelineStates : public vsg::Visitor
    {
        bool blending = false;
        SetPipelineStates(bool in_blending) { blending = in_blending; }

        void apply(vsg::Object& object) { object.traverse(*this); }
        void apply(vsg::InputAssemblyState& ias) { ias.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; }
        void apply(vsg::ColorBlendState& cbs) { cbs.configureAttachments(blending); }
    } sps(blending);

    config->accept(sps);

    config->init();

    // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
    auto stateGroup = vsg::StateGroup::create();

    config->copyTo(stateGroup);

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
        if (auto child = subtile(settings, next_itr, end_itr, subkey + vsgPoints::Key(1, 0, 0, 0), subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey + vsgPoints::Key(0, 1, 0, 0), subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey + vsgPoints::Key(1, 1, 0, 0), subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey + vsgPoints::Key(0, 0, 1, 0), subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey + vsgPoints::Key(1, 0, 1, 0), subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey + vsgPoints::Key(0, 1, 1, 0), subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey + vsgPoints::Key(1, 1, 1, 0), subtiles_bound)) children[num_children++] = child;

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

            transition *= (maxSize / brickSize);

            // vsg::info("maxSize = ", maxSize,", brickSize = ",brickSize,", ratio = ",maxSize/brickSize);
        }
        else
        {
            vsg::warn("unable to set PagedLOD bounds, num_children = ", num_children);
            bs.center = (local_bound.min + local_bound.max) * 0.5;
            bs.radius = vsg::length(local_bound.max - local_bound.max) * 0.5;
        }

        if (settings.createType == CREATE_PAGEDLOD)
        {
            vsg::Path path = vsg::make_string(settings.path, "/", key.w, "/", key.z, "/", key.y);
            vsg::Path filename = vsg::make_string(key.x, settings.extension);
            vsg::Path full_path = path / filename;

            vsg::makeDirectory(path);

            if (num_children == 1)
            {
                vsg::write(children[0], full_path);
            }
            else
            {
                auto group = vsg::Group::create();
                for (size_t i = 0; i < num_children; ++i)
                {
                    group->addChild(children[i]);
                }
                vsg::write(group, full_path);
            }

            auto plod = vsg::PagedLOD::create();
            plod->bound = bs;
            plod->children[0] = vsg::PagedLOD::Child{transition, {}};  // external child visible when its bound occupies more than ~1/4 of the height of the window
            plod->children[1] = vsg::PagedLOD::Child{0.0, brick_node}; // visible always

            if (root)
            {
                plod->filename = full_path;
            }
            else
            {
                plod->filename = vsg::Path("../../../..") / full_path;
            }

            //vsg::info("plod ", key, " ", brick, " plod ", plod, ", brick->points.size() = ",  brick->points.size());

            return plod;
        }
        else
        {
            vsg::ref_ptr<vsg::Node> child_node;
            if (num_children == 1)
            {
                child_node = children[0];
            }
            else
            {
                auto group = vsg::Group::create();
                for (size_t i = 0; i < num_children; ++i)
                {
                    group->addChild(children[i]);
                }

                child_node = group;
            }

            auto lod = vsg::LOD::create();
            lod->bound = bs;
            lod->addChild(vsg::LOD::Child{transition, child_node}); // high res child
            lod->addChild(vsg::LOD::Child{0.0, brick_node});        // lower res child

            //vsg::info("lod ", key, " ", brick, " lod ", lod, ", brick->points.size() = ",  brick->points.size());

            return lod;
        }
    }
    else
    {
        auto leaf = brick->createRendering(settings, key, bound);
        //vsg::info("leaf key  ",key, " ", brick, " leaf ", leaf, ", bound ", bound, ", brick->points.size() = ",  brick->points.size());
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
    double rootBrickSize = brickSize * std::pow(2.0, levels.size() - 1);

    vsg::debug("rootBrickSize  = ", rootBrickSize);

    // If only one level is present then PagedLOD is not required so just add all the levels bricks to the StateGroup
    if (levels.size() == 1)
    {
        vsg::dbox bound;
        for (auto& [key, brick] : *(levels.back()))
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
    vsg::debug("root level ", root_level->size());

    for (auto& [key, brick] : *root_level)
    {
        vsg::debug("root key = ", key, " ", brick);
        vsg::dbox bound;
        if (auto child = subtile(settings, current_itr, levels.rend(), key, bound, true))
        {
            vsg::debug("root child ", child);
            stateGroup->addChild(child);
        }
    }

    return stateGroup;
}
