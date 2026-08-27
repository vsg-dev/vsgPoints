/* <editor-fold desc="MIT License">

Copyright(c) 2023 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgPoints/BIN.h>
#include <vsgPoints/Bricks.h>

#include <vsg/io/Path.h>
#include <vsg/io/stream.h>
#include <vsg/nodes/MatrixTransform.h>

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
        std::cout << "Error: " << settings->bits << " not supported, valid values are 8, 10 and 16." << std::endl;
        return {};
    }

    auto bricks = Bricks::create(settings);

    std::ifstream fin(found_filename, std::ios::in | std::ios::binary);
    if (!fin) return {};

    auto points = vsg::Array<VsgIOPoint>::create(settings->numPointsPerBlock);
    uint8_t alpha = 255;
    vsg::vec3 normal(0.0f, 0.0f, 1.0f);

    while (fin)
    {
        size_t bytesToRead = settings->numPointsPerBlock * sizeof(VsgIOPoint);
        fin.read(reinterpret_cast<char*>(points->dataPointer()), bytesToRead);

        size_t numPointsRead = static_cast<size_t>(fin.gcount()) / sizeof(VsgIOPoint);
        if (numPointsRead == 0) break;

        for (size_t i = 0; i < numPointsRead; ++i)
        {
            auto& point = (*points)[i];
            bricks->add(point.v, vsg::ubvec4(point.c.r, point.c.g, point.c.b, alpha), normal);
        }
    }

    if (bricks->empty())
    {
        std::cout << "Waring: unable to read file." << std::endl;
        return {};
    }
    else
    {
        return bricks;
    }
}
