/* <editor-fold desc="MIT License">

Copyright(c) 2025 Jamie Robertson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgPoints/utils/PointsPolytopeIntersector.h>

#include <vsg/nodes/vertexdraw.h>

#include <iostream>

using namespace vsgPoints;

struct PushPopNode
{
    vsg::Intersector::NodePath& nodePath;

    PushPopNode(vsg::Intersector::NodePath& np, const vsg::Node* node) :
        nodePath(np) { nodePath.push_back(node); }
    ~PushPopNode() { nodePath.pop_back(); }
};

vsg::ref_ptr<vsg::PolytopeIntersector::Intersection> PointsPolytopeIntersector::add(const vsg::dvec3& coord, const vsg::dmat4& localToWorld, 
    const std::vector<uint32_t>& indices, uint32_t instanceIndex)
{
    vsg::ref_ptr<Intersection> intersection;
    intersection = Intersection::create(coord, localToWorld * coord, localToWorld, _nodePath, arrayStateStack.back()->arrays, indices, instanceIndex);
    intersections.emplace_back(intersection);

    return intersection;
}

void PointsPolytopeIntersector::apply(const vsg::VertexDraw& vid)
{
    auto& arrayState = *arrayStateStack.back();
    arrayState.apply(vid);
    if (!arrayState.vertices)
    {
        if (arrayState.topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST && arrayState.arrays.size() > 4)
        {
            auto ps = arrayState.arrays[arrayState.arrays.size() - 2];
            if (!ps->is_compatible(typeid(vsg::vec4Value)))
                return;

            vsg::vec4 posScale = ps->cast<vsg::vec4Value>()->value();

            if (arrayState.vertexAttribute.format == VK_FORMAT_R8G8B8_UNORM ||
                arrayState.vertexAttribute.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32 ||
                arrayState.vertexAttribute.format == VK_FORMAT_R16G16B16_UNORM)
            {
                PushPopNode ppn(_nodePath, &vid);

                double divisor = 1.0;
                switch (arrayState.vertexAttribute.format)
                {
                case VK_FORMAT_R8G8B8_UNORM:
                    divisor = pow(2.f, 8.f) - 1;
                    break;
                case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
                    divisor = pow(2.f, 10.f) - 1;
                    break;
                case VK_FORMAT_R16G16B16_UNORM:
                    divisor = pow(2.f, 16.f) - 1;
                    break;
                }

                // Matrix for unpacking packed vertices
                vsg::dmat4 localToWorld = vsg::translate(vsg::dvec3(posScale.x, posScale.y, posScale.z)) * vsg::scale(posScale.w / divisor);

                auto polytope = _polytopeStack.back();
                for (auto& pl : polytope)
                {
                    pl = pl * localToWorld;
                }

                localToWorld = computeTransform(_nodePath) * localToWorld;

                auto processVertices = [&](auto src_vertices, auto getVertex) {
                    uint32_t lastIndex = vid.instanceCount > 1 ? (vid.firstInstance + vid.instanceCount) : vid.firstInstance + 1;
                    for (uint32_t inst = vid.firstInstance; inst < lastIndex; ++inst)
                    {
                        uint32_t endVertex = vid.firstVertex + vid.vertexCount;
                        for (uint32_t i = vid.firstVertex; i < endVertex; ++i)
                        {
                            auto vertex = getVertex(src_vertices->at(i));
                            if (vsg::inside(polytope, vertex))
                            {
                                add(vsg::dvec3(vertex), localToWorld, {i}, inst);
                            }
                        }
                    }
                };

                switch (arrayState.vertexAttribute.format)
                {
                case VK_FORMAT_R8G8B8_UNORM: {
                    auto src_vertices = arrayState.arrays[arrayState.vertexAttribute.binding].cast<vsg::ubvec3Array>();
                    processVertices(src_vertices, [](const vsg::ubvec3& sv) {
                        return vsg::dvec3(static_cast<double>(sv.x), static_cast<double>(sv.y), static_cast<double>(sv.z));
                    });
                }
                break;
                case VK_FORMAT_A2R10G10B10_UNORM_PACK32: {
                    auto src_vertices = arrayState.arrays[arrayState.vertexAttribute.binding].cast<vsg::uintArray>();
                    processVertices(src_vertices, [](const uint32_t& sv) {
                        return vsg::dvec3(static_cast<double>((sv >> 20) & 0x3FF), static_cast<double>((sv >> 10) & 0x3FF), static_cast<double>(sv & 0x3FF));
                    });
                }
                break;
                case VK_FORMAT_R16G16B16_UNORM: {
                    auto src_vertices = arrayState.arrays[arrayState.vertexAttribute.binding].cast<vsg::usvec3Array>();
                    processVertices(src_vertices, [](const vsg::usvec3& sv) {
                        return vsg::dvec3(static_cast<double>(sv.x), static_cast<double>(sv.y), static_cast<double>(sv.z));
                    });
                }
                break;
                }
                return;
            }
            else
                return;
        }
    }
    PushPopNode ppn(_nodePath, &vid);

    intersectDraw(vid.firstVertex, vid.vertexCount, vid.firstInstance, vid.instanceCount);
}