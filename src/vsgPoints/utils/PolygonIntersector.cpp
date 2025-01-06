/* <editor-fold desc="MIT License">

Copyright(c) 2025 Jamie Robertson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgPoints/utils/PolygonIntersector.h>

#include <vsg/app/Camera.h>
#include <vsg/nodes/Transform.h>
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

struct RayTriangleIntersector
{
    vsg::dvec3 d, v0, v1, v2, e1, e2, h;
    double a;

    RayTriangleIntersector():a(0.0){}

    RayTriangleIntersector(const vsg::dvec3& in_ray_dir, const vsg::dvec3& in_v0, const vsg::dvec3& in_v1, const vsg::dvec3& in_v2) : 
        d(in_ray_dir), v0(in_v0), v1(in_v1), v2(in_v2)
    {
        e1 = v1 - v0;
        e2 = v2 - v0;
        h = vsg::cross(in_ray_dir, e2);
        a = vsg::dot(e1, h);
    }

    bool intersectsTriangle(const vsg::dvec3& pt)
    {
        const double epsilon = 1e-10;
        if (a > -epsilon && a < epsilon)
            return (false);

        double f = 1.0 / a;
        vsg::dvec3 s = pt - v0;
        double u = f * (vsg::dot(s, h));
            
        if (u < 0.0 || u > 1.0)
            return (false);
            
        vsg::dvec3 q = vsg::cross(s, e1);
        double v = f * vsg::dot(d, q);
            
        if (v < 0.0 || u + v > 1.0)
            return (false);

        double t = f * dot(e2, q);
            
        return (t > epsilon);
    }
};

struct RayEdgeIntersector
{
    RayTriangleIntersector t1;
    RayTriangleIntersector t2;

    vsg::dvec3 plane_normal;
    double dot_normal_ray;


    RayEdgeIntersector(const vsg::dvec3& in_ray_dir, const vsg::dvec3& in_v0, const vsg::dvec3& in_v1, const vsg::dvec3& in_v2, const vsg::dvec3& in_v3)
        : t1(RayTriangleIntersector(in_ray_dir, in_v0, in_v2, in_v3)), t2(RayTriangleIntersector(in_ray_dir, in_v0, in_v1, in_v2))
    {
        plane_normal = vsg::cross(t1.e1, t1.e2);
        dot_normal_ray = vsg::dot(plane_normal, in_ray_dir);
    }

    bool intersects(const vsg::dvec3& pt)
    {
        double dot_normal_point = vsg::dot(plane_normal, pt - t1.v0);
        if (dot_normal_ray * dot_normal_point > 0)
            return false;

        return t1.intersectsTriangle(pt) || t2.intersectsTriangle(pt);
    }
};


PolygonIntersector::PolygonIntersector(const vsg::Camera& camera, std::vector<vsg::dvec2> polygon, vsg::ref_ptr<vsg::ArrayState> initialArrayData) :
    Inherit(initialArrayData)
{
    invert_selection = false;

    auto viewport = camera.getViewport();

    std::vector<vsg::dvec2> ndc_polygon;
    vsg::dbox ndc_bounds;
    for (auto& pt : polygon)
    {
        double ndc_x = (viewport.width > 0) ? (2.0 * (pt.x - static_cast<double>(viewport.x)) / static_cast<double>(viewport.width) - 1.0) : pt.x;
        double ndc_y = (viewport.height > 0) ? (2.0 * (pt.y - static_cast<double>(viewport.y)) / static_cast<double>(viewport.height) - 1.0) : pt.y;
        vsg::dvec2 ndc(ndc_x, ndc_y);
        if (ndc_polygon.size() > 0 && ndc == ndc_polygon.back())
            continue;
        ndc_polygon.push_back(ndc);
        ndc_bounds.add(ndc.x, ndc.y, 0.0);
    }

    // Ensure closed
    if (!ndc_polygon.empty() && ndc_polygon.front() != ndc_polygon.back())
        ndc_polygon.push_back(ndc_polygon.front());

    if (ndc_polygon.size() < 3)
        ndc_polygon.clear();

    auto projectionMatrix = camera.projectionMatrix->transform();
    auto viewMatrix = camera.viewMatrix->transform();
    bool reverse_depth = (projectionMatrix(2, 2) > 0.0);

    double ndc_near = reverse_depth ? viewport.maxDepth : viewport.minDepth;
    double ndc_far = reverse_depth ? viewport.minDepth : viewport.maxDepth;

    Polytope clipspace;
    clipspace.push_back(vsg::dplane(1.0, 0.0, 0.0, -ndc_bounds.min.x)); // left
    clipspace.push_back(vsg::dplane(-1.0, 0.0, 0.0, ndc_bounds.max.x)); // right
    clipspace.push_back(vsg::dplane(0.0, 1.0, 0.0, -ndc_bounds.min.y)); // bottom
    clipspace.push_back(vsg::dplane(0.0, -1.0, 0.0, ndc_bounds.max.y)); // top
    clipspace.push_back(vsg::dplane(0.0, 0.0, -1.0, ndc_near)); // near
    clipspace.push_back(vsg::dplane(0.0, 0.0, 1.0, ndc_far));   // far

    Polytope eyespace;
    for (auto& pl : clipspace)
    {
        eyespace.push_back(pl * projectionMatrix);
    }

    _polytopeStack.push_back(eyespace);

    Polytope worldspace;
    for (auto& pl : eyespace)
    {
        worldspace.push_back(pl * viewMatrix);
    }

    _polytopeStack.push_back(worldspace);

    vsg::dmat4 eyeToWorld = inverse(viewMatrix);
    localToWorldStack().push_back(viewMatrix);
    worldToLocalStack().push_back(eyeToWorld);

    // edges
    Edges clipEdges;

    //   v1__v2
    //   /    \
    //  /______\
    // v0      v3
    for (int i = 1; i < ndc_polygon.size(); i++)
    {
        auto v0 = vsg::dvec3(ndc_polygon[i - 1].x, ndc_polygon[i - 1].y, ndc_near);
        auto v1 = vsg::dvec3(ndc_polygon[i - 1].x, ndc_polygon[i - 1].y, ndc_far);
        auto v2 = vsg::dvec3(ndc_polygon[i].x, ndc_polygon[i].y, ndc_far);
        auto v3 = vsg::dvec3(ndc_polygon[i].x, ndc_polygon[i].y, ndc_near);
        Edge edge = {v0, v1, v2, v3};
        clipEdges.push_back(edge);
    }

    auto inv_projectionMatrix = vsg::inverse(projectionMatrix);
    Edges eyeEdges;
    for (auto& edge : clipEdges)
    {
        eyeEdges.push_back(Edge{inv_projectionMatrix * edge[0], inv_projectionMatrix * edge[1],
                                inv_projectionMatrix * edge[2], inv_projectionMatrix * edge[3]});
    }

    _polygonEdgeStack.push_back(eyeEdges);

    Edges worldEdges;
    for (auto& edge : eyeEdges)
    {
        worldEdges.push_back(Edge{eyeToWorld * edge[0], eyeToWorld * edge[1],
                                    eyeToWorld * edge[2], eyeToWorld * edge[3]});
    }

    _polygonEdgeStack.push_back(worldEdges);
}

PolygonIntersector::Intersection::Intersection(const vsg::dmat4& in_localToWorld, const NodePath& in_nodePath, const vsg::DataList& in_arrays, const std::vector<uint32_t>& in_indices, uint32_t in_instanceIndex) :
    localToWorld(in_localToWorld),
    nodePath(in_nodePath),
    arrays(in_arrays),
    indices(in_indices),
    instanceIndex(in_instanceIndex)
{
}

vsg::dvec3 PolygonIntersector::Intersection::worldPos(uint32_t index)
{
    switch (arrays[0]->properties.format)
    {
        case VK_FORMAT_R8G8B8_UNORM:
            return localToWorld * vsg::dvec3(arrays[0].cast<vsg::ubvec3Array>()->at(index));
            break;
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
            {
                uint32_t v = arrays[0].cast<vsg::uintArray>()->at(index);
                return localToWorld * vsg::dvec3(static_cast<double>((v >> 20) & 0x3FF), static_cast<double>((v >> 10) & 0x3FF), static_cast<double>(v & 0x3FF));
            }
            break;
        case VK_FORMAT_R16G16B16_UNORM:
                return localToWorld * vsg::dvec3(arrays[0].cast<vsg::usvec3Array>()->at(index));
            break;
    }
    return vsg::dvec3(0.0, 0.0, 0.0);
}


vsg::ref_ptr<PolygonIntersector::Intersection> PolygonIntersector::add(const vsg::dmat4& localToWorld, const std::vector<uint32_t>& indices, uint32_t instanceIndex)
{
    vsg::ref_ptr<Intersection> intersection;

    intersection = Intersection::create(localToWorld, _nodePath, arrayStateStack.back()->arrays, indices, instanceIndex);
    intersections.emplace_back(intersection);

    return intersection;
}

uint64_t PolygonIntersector::intersectionCount()
{
    uint64_t count = 0;
    for (auto& intersection : intersections)
        count += intersection->indices.size();
    return count;
}

void PolygonIntersector::pushTransform(const vsg::Transform& transform)
{
    auto& l2wStack = localToWorldStack();
    auto& w2lStack = worldToLocalStack();

    vsg::dmat4 localToWorld = l2wStack.empty() ? transform.transform(vsg::dmat4{}) : transform.transform(l2wStack.back());
    vsg::dmat4 worldToLocal = inverse(localToWorld);

    l2wStack.push_back(localToWorld);
    w2lStack.push_back(worldToLocal);

    // bounding polytope
    const auto& worldspace_polytope = _polytopeStack.front();

    Polytope localspace_polytope;
    for (auto& pl : worldspace_polytope)
    {
        localspace_polytope.push_back(pl * localToWorld);
    }

    _polytopeStack.push_back(localspace_polytope);

    // edges
    const auto& worldspace_edges = _polygonEdgeStack.front();

    Edges localspace_edges;
    for (auto& edge : worldspace_edges)
    {
        localspace_edges.emplace_back(Edge{worldToLocal * edge[0], worldToLocal * edge[1], worldToLocal * edge[2], worldToLocal * edge[3]});
    }
    _polygonEdgeStack.push_back(localspace_edges);
}

void PolygonIntersector::popTransform()
{
    _polytopeStack.pop_back();
    _polygonEdgeStack.pop_back();
    localToWorldStack().pop_back();
    worldToLocalStack().pop_back();
}

bool PolygonIntersector::intersects(const vsg::dsphere& bs)
{
    if (!bs.valid()) return false;

    const auto& polytope = _polytopeStack.back();

    return vsg::intersect(polytope, bs) || invert_selection;
}

bool PolygonIntersector::intersectDraw(uint32_t firstVertex, uint32_t vertexCount, uint32_t firstInstance, uint32_t instanceCount)
{
    return false;
}

bool PolygonIntersector::intersectDrawIndexed(uint32_t firstIndex, uint32_t indexCount, uint32_t firstInstance, uint32_t instanceCount)
{
    return false;
}


void PolygonIntersector::apply(const vsg::VertexDraw& vid)
{
    if (_polygonEdgeStack.back().empty())
        return;

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
                vsg::dmat4 worldToLocal = vsg::inverse(localToWorld);

                auto polytope = _polytopeStack.back();
                for (auto& pl : polytope)
                {
                    pl = pl * localToWorld;
                }

                // transform edges to packed vertex coordinates
                auto edges = _polygonEdgeStack.back();
                for (auto& edge : edges)
                {
                    for (auto& v : edge)
                    {
                        v = worldToLocal * v;
                        int a = 0;
                    }
                }

                localToWorld = computeTransform(_nodePath) * localToWorld;

                // test ray direction across near / far of first edge
                //   v1__v2
                //   /    \
                //  /______\
                // v0       v3
                vsg::dvec3 ray_dir = vsg::normalize(vsg::cross(edges.front()[1] - edges.front()[0], edges.front()[3] - edges.front()[0]));
                std::vector<RayEdgeIntersector> edgeIntersectors;

                for (auto &edge : edges)
                {
                    edgeIntersectors.emplace_back(RayEdgeIntersector(ray_dir, edge[0], edge[1], edge[2], edge[3]));
                }

                // Test whether entire brick intersected
                std::vector<vsg::dvec3> brickCorners ={
                        vsg::dvec3(0.0, 0.0, 0.0),
                        vsg::dvec3(0.0, 0.0, divisor),
                        vsg::dvec3(divisor, 0.0, 0.0),
                        vsg::dvec3(divisor, 0.0, divisor),
                        vsg::dvec3(0.0, divisor, 0.0),
                        vsg::dvec3(0.0, divisor, divisor),
                        vsg::dvec3(divisor, divisor, 0.0),
                        vsg::dvec3(divisor, divisor, divisor)};

                bool all_intersected = true;
                for (auto& v : brickCorners)
                {
                    int n_intersections = 0;
                    for (auto& edgeIntersector : edgeIntersectors)
                    {
                        if (edgeIntersector.intersects(v))
                            n_intersections++;
                    }
                    if (invert_selection && n_intersections % 2 == 1)
                    {
                        all_intersected = false;
                        break;
                    }
                    else if (!invert_selection && n_intersections % 2 == 0)
                    {
                        all_intersected = false;
                        break;
                    }
                }
                if (all_intersected)
                {
                    uint32_t lastIndex = vid.instanceCount > 1 ? (vid.firstInstance + vid.instanceCount) : vid.firstInstance + 1;
                    for (uint32_t inst = vid.firstInstance; inst < lastIndex; ++inst)
                    {
                        uint32_t endVertex = vid.firstVertex + vid.vertexCount;
                        std::vector<uint32_t> intersectedIndices;
                        for (uint32_t i = vid.firstVertex; i < endVertex; ++i)
                            intersectedIndices.push_back(i);

                        if (!intersectedIndices.empty())
                            add(localToWorld, intersectedIndices, inst);
                    }
                    return;
                }

                auto processVertices = [&](auto src_vertices, auto getVertex) {
                    uint32_t lastIndex = vid.instanceCount > 1 ? (vid.firstInstance + vid.instanceCount) : vid.firstInstance + 1;
                    for (uint32_t inst = vid.firstInstance; inst < lastIndex; ++inst)
                    {
                        uint32_t endVertex = vid.firstVertex + vid.vertexCount;
                        std::vector<uint32_t> intersectedIndices;
                        for (uint32_t i = vid.firstVertex; i < endVertex; ++i)
                        {
                            auto vertex = getVertex(src_vertices->at(i));
                            if (invert_selection || edgeIntersectors.size() < 9 || vsg::inside(polytope, vertex))
                            {
                                int n_intersections = 0;
                                for (auto& edgeIntersector : edgeIntersectors)
                                {
                                    if (edgeIntersector.intersects(vertex))
                                        n_intersections++;
                                }
                                if (n_intersections % 2 == (invert_selection ? 0 : 1))
                                    intersectedIndices.push_back(i);
                            }
                        }

                        if (!intersectedIndices.empty())
                            add(localToWorld, intersectedIndices, inst);
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
}
