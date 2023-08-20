#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2023 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Inherit.h>

#include <vsgPoints/Export.h>

namespace vsgPoints
{

    enum CreateType
    {
        CREATE_FLAT,     /// generate a flat scene graph with no LOD/PagedLOD, only suitable for small datasets
        CREATE_LOD,      /// generate a hierarchical LOD scene graph, suitable for small to moderate sized datasets that can entirely fit in GPU memory
        CREATE_PAGEDLOD, /// generate a PagedLOD scene graph, suitable for large datasets that can't fit entirely in GPU memory
    };

    struct Settings : public vsg::Inherit<vsg::Object, Settings>
    {
        size_t numPointsPerBlock = 10000;
        double precision = 0.001;
        uint32_t bits = 10;
        float pointSize = 4.0f;
        float transition = 0.125f;

        CreateType createType = CREATE_LOD;

        vsg::Path path;
        vsg::Path extension = ".vsgb";
        vsg::ref_ptr<vsg::Options> options;
        vsg::dvec3 offset;
        vsg::dbox bound;
    };

} // namespace vsgPoints

EVSG_type_name(vsgPoints::Settings)
