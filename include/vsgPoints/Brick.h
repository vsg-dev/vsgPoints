#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2023 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/nodes/Node.h>

#include <vsgPoints/Export.h>

namespace vsgPoints
{
    #pragma pack(1)

    struct PackedPoint
    {
        vsg::usvec3 v;
        vsg::ubvec4 c;
    };

    #pragma pack()

    struct Settings
    {
        size_t numPointsPerBlock = 10000;
        double precision = 0.001;
        uint32_t bits = 8;
        float pointSize = 4.0f;
        float transition = 0.125f;
        vsg::Path path;
        vsg::Path extension = ".vsgb";
        vsg::ref_ptr<vsg::Options> options;
        vsg::dvec3 offset;
        vsg::dbox bound;
    };

    using Key = vsg::ivec4;

    class VSGPOINTS_DECLSPEC Brick : public vsg::Inherit<vsg::Object, Brick>
    {
    public:

        Brick();

        std::vector<PackedPoint> points;

        vsg::ref_ptr<vsg::Node> createRendering(const Settings& settings, const vsg::vec4& positionScale, const vsg::vec2& pointSize);
        vsg::ref_ptr<vsg::Node> createRendering(const Settings& settings, Key key, vsg::dbox& bound);

    protected:
        virtual ~Brick();
    };

    using Bricks = std::map<Key, vsg::ref_ptr<Brick>>;
    using Levels = std::list<Bricks>;

} // namespace vsgPoints
