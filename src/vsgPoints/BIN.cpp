/* <editor-fold desc="MIT License">

Copyright(c) 2023 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgPoints/BIN.h>

#include <vsg/nodes/MatrixTransform.h>
#include <vsg/io/Path.h>
#include <vsg/io/stream.h>
#include <vsg/io/VSG.h>

#include <fstream>

#include <iostream>

using namespace vsgPoints;

#pragma pack(1)

struct VsgIOPoint
{
    vsg::dvec3 v;
    vsg::ubvec3 c;
};

#pragma pack()

BIN::BIN() :
   supportedExtensions{".bin"}
{
}

bool BIN::readBricks(const vsg::Path& filename, vsgPoints::Settings& settings, vsgPoints::Bricks& bricks) const
{
    auto& bound = settings.bound;
    bound.reset();

    std::ifstream fin(filename, std::ios::in | std::ios::binary);
    if (!fin) return false;

    double multiplier = 1.0 / settings.precision;
    auto points = vsg::Array<VsgIOPoint>::create(settings.numPointsPerBlock);

    decltype(vsgPoints::PackedPoint::c)::value_type alpha = 255;

    int64_t divisor = 1 << settings.bits;
    int64_t mask = divisor - 1;

    while(fin)
    {
        size_t bytesToRead = settings.numPointsPerBlock * sizeof(VsgIOPoint);
        fin.read(reinterpret_cast<char*>(points->dataPointer()), bytesToRead);

        size_t numPointsRead = static_cast<size_t>(fin.gcount()) / sizeof(VsgIOPoint);
        if (numPointsRead == 0) break;

        for(size_t i =0; i<numPointsRead; ++i)
        {
            auto& point = (*points)[i];

            bound.add(point.v);

            vsg::dvec3 scaled_v = point.v * multiplier;
            vsg::dvec3 rounded_v = {std::round(scaled_v.x), std::round(scaled_v.y), std::round(scaled_v.z)};
            vsg::t_vec3<int64_t> int64_v = {static_cast<int64_t>(rounded_v.x),  static_cast<int64_t>(rounded_v.y), static_cast<int64_t>(rounded_v.z)};
            vsgPoints::Key key = { static_cast<int32_t>(int64_v.x / divisor), static_cast<int32_t>(int64_v.y / divisor), static_cast<int32_t>(int64_v.z / divisor), 1};

            vsgPoints::PackedPoint packedPoint;
            packedPoint.v.set(static_cast<uint16_t>(int64_v.x & mask), static_cast<uint16_t>(int64_v.y & mask), static_cast<uint16_t>(int64_v.z & mask));
            packedPoint.c.set(point.c.r, point.c.g, point.c.b, alpha) ;

            auto& brick = bricks[key];
            if (!brick)
            {
                brick = vsgPoints::Brick::create();
            }

            brick->points.push_back(packedPoint);
        }
    }
    std::cout<<"Read bound "<<bound<<std::endl;

    return true;
}

vsg::ref_ptr<vsg::Node> BIN::processRawData(const vsg::Path& filename, vsgPoints::Settings& settings) const
{
    vsgPoints::Levels levels;
    levels.push_back(vsgPoints::Bricks::create());
    auto& first_level = levels.front();
    if (!readBricks(filename, settings, *first_level))
    {
        std::cout<<"Waring: unable to read file."<<std::endl;
        return {};
    }

    if (settings.bound.valid())
    {
        settings.offset = (settings.bound.max + settings.bound.min) * 0.5;
    }

    std::cout<<"After reading data "<<first_level->size()<<std::endl;

    size_t biggestBrick = 0;
    vsg::t_box<int32_t> keyBounds;
    for(auto& [key, brick] : *first_level)
    {
        keyBounds.add(key.x, key.y, key.z);
        if (brick->points.size() > biggestBrick) biggestBrick = brick->points.size();
    }

    while(levels.back()->size() > 1)
    {
        auto& source = levels.back();

        levels.push_back(vsgPoints::Bricks::create());
        auto& destination = levels.back();

        if (!generateLevel(*source, *destination, settings)) break;
    }

    std::cout<<"levels = "<<levels.size()<<std::endl;

    std::cout<<"keyBounds "<<keyBounds<<std::endl;
    std::cout<<"biggest brick "<<biggestBrick<<std::endl;

    auto transform = vsg::MatrixTransform::create();
    transform->matrix = vsg::translate(settings.offset);

    if (auto model = createPagedLOD(levels, settings))
    {
        transform->addChild(model);
    }
    return transform;
}

vsg::ref_ptr<vsg::Object> BIN::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!vsg::compatibleExtension(filename, options, supportedExtensions)) return {};

    auto found_filename = vsg::findFile(filename, options);
    if (!found_filename) return {};

    vsg::ref_ptr<vsgPoints::Settings> settings;
    if (options) settings = const_cast<vsg::Options*>(options.get())->getRefObject<vsgPoints::Settings>("settings");
    if (!settings) settings = vsgPoints::Settings::create();

    if (settings->bits != 8 && settings->bits != 10 && settings->bits != 16)
    {
        std::cout<<"Error: "<<settings->bits<<" not supported, valid values are 8, 10 and 16."<<std::endl;
        return {};
    }

    return processRawData(found_filename, *settings);
}
