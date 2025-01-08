/* <editor-fold desc="MIT License">

Copyright(c) 2023 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgPoints/AsciiPoints.h>
#include <vsgPoints/Bricks.h>

#include <vsg/io/Path.h>
#include <vsg/io/read_line.h>
#include <vsg/io/stream.h>
#include <vsg/nodes/MatrixTransform.h>

#include <fstream>

#include <iostream>

using namespace vsgPoints;

AsciiPoints::AsciiPoints() :
    supportedExtensions{".3dc", ".asc"}
{
}

vsg::ref_ptr<vsg::Object> AsciiPoints::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!vsg::compatibleExtension(filename, options, supportedExtensions)) return {};

    auto filenameToUse = vsg::findFile(filename, options);
    if (!filenameToUse) return {};

    vsg::ref_ptr<vsgPoints::Settings> settings;
    if (options) settings = const_cast<vsg::Options*>(options.get())->getRefObject<vsgPoints::Settings>("settings");
    if (!settings) settings = vsgPoints::Settings::create();

    if (settings->bits != 8 && settings->bits != 10 && settings->bits != 16)
    {
        std::cout << "Error: " << settings->bits << " not supported, valid values are 8, 10 and 16." << std::endl;
        return {};
    }

    auto bricks = vsgPoints::Bricks::create(settings);

    auto values = vsg::doubleArray::create(10);
    uint8_t alpha = 255;
    vsg::vec3 normal(0.0f, 0.0f, 1.0f);

    std::ifstream fin(filenameToUse);
    while (fin)
    {
        if (auto numValuesRead = vsg::read_line(fin, values->data(), values->size()))
        {
            if (numValuesRead >= 9)
            {
                bricks->add(vsg::dvec3(values->at(0), values->at(1), values->at(2)), vsg::ubvec4(values->at(3), values->at(4), values->at(5), alpha), vsg::vec3(values->at(6), values->at(7), values->at(8)));
                settings->normals = true;
            }
            else if (numValuesRead >= 6)
            {
                bricks->add(vsg::dvec3(values->at(0), values->at(1), values->at(2)), vsg::ubvec4(values->at(3), values->at(4), values->at(5), alpha), normal);
            }
        }
    }

    if (bricks->empty()) return {};

    return bricks;
}
