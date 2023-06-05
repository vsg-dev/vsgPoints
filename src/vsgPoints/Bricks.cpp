/* <editor-fold desc="MIT License">

Copyright(c) 2023 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgPoints/Bricks.h>

#include <vsg/io/Logger.h>

#include <iostream>

using namespace vsgPoints;

Bricks::Bricks(vsg::ref_ptr<Settings> in_settings) :
    settings(in_settings)
{
}

void Bricks::add(const vsg::dvec3& v, const vsg::ubvec4& c)
{
    settings->bound.add(v);

    auto divide_round = [](int64_t value, int64_t divisor) -> int64_t {
        if (value < 0)
            return -1 - (-value / divisor);
        else
            return value / divisor;
    };

    double multiplier = 1.0 / settings->precision;
    int64_t divisor = 1 << settings->bits;

    vsg::dvec3 scaled_v = v * multiplier;
    vsg::dvec3 rounded_v = {std::round(scaled_v.x), std::round(scaled_v.y), std::round(scaled_v.z)};
    vsg::t_vec3<int64_t> int64_v = {static_cast<int64_t>(rounded_v.x), static_cast<int64_t>(rounded_v.y), static_cast<int64_t>(rounded_v.z)};
    vsg::t_vec3<int64_t> int64_key = {divide_round(int64_v.x, divisor), divide_round(int64_v.y, divisor), divide_round(int64_v.z, divisor)};
    Key key = {static_cast<int32_t>(int64_key.x), static_cast<int32_t>(int64_key.y), static_cast<int32_t>(int64_key.z), 1};

    PackedPoint packedPoint;
    packedPoint.v.set(int64_v.x - key.x * divisor, int64_v.y - key.y * divisor, int64_v.z - key.z * divisor);
    packedPoint.c.set(c.r, c.g, c.b, c.a);

    auto& brick = bricks[key];
    if (!brick)
    {
        brick = Brick::create();
    }

    brick->points.push_back(packedPoint);
}

size_t Bricks::count() const
{
    size_t num = 0;
    for(auto& [key, brick] : bricks)
    {
        num += brick->points.size();
    }
    return num;
}
