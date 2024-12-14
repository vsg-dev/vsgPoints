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
#include <vsg/nodes/LOD.h>
#include <vsg/nodes/PagedLOD.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/VertexDraw.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/state/material.h>
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

    if (settings.bits == 8)
    {
        auto vertices_8bit = vsg::ubvec3Array::create(points.size(), vsg::Data::Properties(VK_FORMAT_R8G8B8_UNORM));
        auto vertex_itr = vertices_8bit->begin();
        auto color_itr = colors->begin();
        for (auto& point : points)
        {
            (vertex_itr++)->set(static_cast<uint8_t>(point.v.x), static_cast<uint8_t>(point.v.y), static_cast<uint8_t>(point.v.z));
            *(color_itr++) = point.c;
        }

        vertices = vertices_8bit;
    }
    else if (settings.bits == 10)
    {
        auto vertices_10bit = vsg::uintArray::create(points.size(), vsg::Data::Properties(VK_FORMAT_A2R10G10B10_UNORM_PACK32));
        auto vertex_itr = vertices_10bit->begin();
        auto color_itr = colors->begin();
        for (auto& point : points)
        {
            *(vertex_itr++) = 3 << 30 | (static_cast<uint32_t>(point.v.x) << 20) | (static_cast<uint32_t>(point.v.y) << 10) | (static_cast<uint32_t>(point.v.z));
            *(color_itr++) = point.c;
        }

        vertices = vertices_10bit;
    }
    else if (settings.bits == 16)
    {
        auto vertices_16bit = vsg::usvec3Array::create(points.size(), vsg::Data::Properties(VK_FORMAT_R16G16B16_UNORM));
        auto vertex_itr = vertices_16bit->begin();
        auto color_itr = colors->begin();
        for (auto& point : points)
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

    for (auto& point : points)
    {
        auto& v = point.v;
        bound.add(position.x + brickPrecision * static_cast<double>(v.x),
                  position.y + brickPrecision * static_cast<double>(v.y),
                  position.z + brickPrecision * static_cast<double>(v.z));
    }

    vsg::vec2 pointSize(brickPrecision * settings.pointSize, brickPrecision);
    vsg::vec4 positionScale(position.x, position.y, position.z, brickSize - brickPrecision);

    return createRendering(settings, positionScale, pointSize);
}
