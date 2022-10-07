#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/nodes/Node.h>

#include <vsgPoints/BrickShaderSet.h>

namespace vsgPoints
{
    class VSGPOINTS_DECLSPEC BrickBuilder : public vsg::Inherit<vsg::Object, BrickBuilder>
    {
    public:
        vsg::dbox extents;
        size_t num_x = 0;
        size_t num_y = 0;
        size_t num_z = 0;

        vsg::ref_ptr<const vsg::Options> options;
        vsg::ref_ptr<vsg::vec4Value> viewport;
        vsg::ref_ptr<vsg::Node> root;

        void add(vsg::ref_ptr<vsg::vec3Array> vertices, vsg::ref_ptr<vsg::vec3Array> normals, vsg::ref_ptr<vsg::ubvec4Array> colors);

        vsg::ref_ptr<vsg::Node> build();
    };
} // namespace vsgPoints
