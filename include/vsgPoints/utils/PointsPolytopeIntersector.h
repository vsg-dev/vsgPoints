#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2025 Jamie Robertson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/utils/PolytopeIntersector.h>

#include <vsg/core/Inherit.h>
#include <vsgPoints/Export.h>

namespace vsgPoints
{   
    /// PointsPolytopeIntersector is an vsg::PolytopeIntersector subclass that provides support for vsgPoints pointclouds with packed vertex formats 
    class VSGPOINTS_DECLSPEC PointsPolytopeIntersector : public vsg::Inherit<vsg::PolytopeIntersector, PointsPolytopeIntersector>
    {
    public:
        using Inherit::Inherit;

        vsg::ref_ptr<Intersection> add(const vsg::dvec3& coord, const vsg::dmat4& in_localToWorld, const std::vector<uint32_t>& indices, uint32_t instanceIndex);

        void apply(const vsg::VertexDraw& vid) override;
    };

} // namespace vsgPoints

EVSG_type_name(vsgPoints::PointsPolytopeIntersector)