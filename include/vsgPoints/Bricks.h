#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2023 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/nodes/Node.h>

#include <vsgPoints/Brick.h>

#include <list>

namespace vsgPoints
{
    class VSGPOINTS_DECLSPEC Bricks : public vsg::Inherit<vsg::Object, Bricks>
    {
    public:
        Bricks(vsg::ref_ptr<Settings> in_settings = {});

        using BrickMap = std::map<Key, vsg::ref_ptr<Brick>>;
        using key_type = BrickMap::key_type;
        using mapped_type = BrickMap::mapped_type;
        using value_type = BrickMap::value_type;
        using iterator = BrickMap::iterator;
        using const_iterator = BrickMap::const_iterator;

        vsg::ref_ptr<Settings> settings;
        BrickMap bricks;

        void add(const vsg::dvec3& v, const vsg::ubvec4& c, const vsg::vec3& n);

        iterator find(Key key) { return bricks.find(key); }
        const_iterator find(Key key) const { return bricks.find(key); }

        mapped_type& operator[](Key key) { return bricks[key]; }

        iterator begin() { return bricks.begin(); }
        iterator end() { return bricks.end(); }

        const_iterator begin() const { return bricks.begin(); }
        const_iterator end() const { return bricks.end(); }

        bool empty() const { return bricks.empty(); }

        size_t size() const { return bricks.size(); }

        // number of points
        size_t count() const;
    };

    using Levels = std::list<vsg::ref_ptr<Bricks>>;

} // namespace vsgPoints

EVSG_type_name(vsgPoints::Bricks)
