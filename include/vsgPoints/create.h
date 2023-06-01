#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2023 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/nodes/Node.h>

#include <vsgPoints/Bricks.h>

namespace vsgPoints
{

    /// create a scene graph from Bricks using the Setttings as a guide to the type of scene graph to create.
    extern VSGPOINTS_DECLSPEC vsg::ref_ptr<vsg::Node> createSceneGraph(vsg::ref_ptr<vsgPoints::Bricks> bricks, vsg::ref_ptr<vsgPoints::Settings> settings);

    extern VSGPOINTS_DECLSPEC bool generateLevel(vsgPoints::Bricks& source, vsgPoints::Bricks& destination, const vsgPoints::Settings& settings);
    extern VSGPOINTS_DECLSPEC vsg::ref_ptr<vsg::StateGroup> createStateGroup(const vsgPoints::Settings& settings);
    extern VSGPOINTS_DECLSPEC vsg::ref_ptr<vsg::Node> subtile(vsgPoints::Settings& settings, vsgPoints::Levels::reverse_iterator level_itr, vsgPoints::Levels::reverse_iterator end_itr, vsgPoints::Key key, vsg::dbox& bound, bool root = false);
    extern VSGPOINTS_DECLSPEC vsg::ref_ptr<vsg::Node> createPagedLOD(vsgPoints::Levels& levels, vsgPoints::Settings& settings);

} // namespace vsgPoints
