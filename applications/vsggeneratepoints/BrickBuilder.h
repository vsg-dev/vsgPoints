#pragma once

#include <vsg/core/Inherit.h>
#include <vsg/nodes/Node.h>
#include <vsg/utils/ShaderSet.h>

namespace vsgPoints
{
    #define VERTEX_TYPE 4

    extern vsg::ref_ptr<vsg::Data> createParticleImage(uint32_t dim);

    extern vsg::ref_ptr<vsg::ShaderSet> createPointsFlatShadedShaderSet(vsg::ref_ptr<const vsg::Options> options);
    extern vsg::ref_ptr<vsg::ShaderSet> createPointsPhongShaderSet(vsg::ref_ptr<const vsg::Options> options);

    class BrickBuilder : public vsg::Inherit<vsg::Object, BrickBuilder>
    {
    public:
        vsg::dbox extents;
        size_t num_x = 0;
        size_t num_y = 0;
        size_t num_z = 0;

        vsg::ref_ptr<const vsg::Options> options;
        vsg::ref_ptr<vsg::vec4Value> viewport;
        vsg::ref_ptr<vsg::Node> root;

        void add(vsg::ref_ptr< vsg::vec3Array> vertices, vsg::ref_ptr<vsg::vec3Array> normals, vsg::ref_ptr<vsg::ubvec4Array> colors);

        vsg::ref_ptr<vsg::Node> build();
    };
}
