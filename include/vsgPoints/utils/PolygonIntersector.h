#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2025 Jamie Robertson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgPoints/Export.h>

#include <vsg/maths/plane.h>
#include <vsg/utils/Intersector.h>


namespace vsgPoints
{
    using Edge = std::vector<vsg::dvec3>;
    using Edges = std::vector<std::vector<vsg::dvec3>>;
    using Polytope = std::vector<vsg::dplane>;

    class VSGPOINTS_DECLSPEC PolygonIntersector : public vsg::Inherit<vsg::Intersector, PolygonIntersector>
    {
    public:
        /// create a vsgPoints intersector for a screen polygon with window space dimensions projected into world coords using the Camera's projection and view matrices.
        PolygonIntersector(const vsg::Camera& camera, std::vector<vsg::dvec2> polygon, vsg::ref_ptr<vsg::ArrayState> initialArrayData = {});

        class VSGPOINTS_DECLSPEC Intersection : public Inherit<Object, Intersection>
        {
        public:
            Intersection() {}
            Intersection(const vsg::dmat4& in_localToWorld,const NodePath& in_nodePath, const vsg::DataList& in_arrays,
                const std::vector<uint32_t>& in_indices, uint32_t in_instanceIndex);

            vsg::dmat4 localToWorld;
            NodePath nodePath;
            vsg::DataList arrays;
            std::vector<uint32_t> indices;
            uint32_t instanceIndex = 0;

            // return true if Intersection is valid
            operator bool() const { return !nodePath.empty(); }

            vsg::dvec3 worldPos(uint32_t index);
        };

        bool invert_selection;

        using Intersections = std::vector<vsg::ref_ptr<Intersection>>;
        Intersections intersections;

        uint64_t intersectionCount();

        vsg::ref_ptr<Intersection> add(const vsg::dmat4& in_localToWorld, const std::vector<uint32_t>& indices, uint32_t instanceIndex);

        void pushTransform(const vsg::Transform& transform) override;
        void popTransform() override;

        bool intersects(const vsg::dsphere& bs) override;

        bool intersectDraw(uint32_t firstVertex, uint32_t vertexCount, uint32_t firstInstance, uint32_t instanceCount) override;

        bool intersectDrawIndexed(uint32_t firstIndex, uint32_t indexCount, uint32_t firstInstance, uint32_t instanceCount) override;

        void apply(const vsg::VertexDraw& vid) override;

    protected:
        std::vector<Polytope> _polytopeStack;
        std::vector<Edges> _polygonEdgeStack;
    };

} // namespace vsgPoints

EVSG_type_name(vsgPoints::PolygonIntersector)