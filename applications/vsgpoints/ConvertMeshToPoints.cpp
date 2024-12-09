#include <vsg/all.h>

#include "ConvertMeshToPoints.h"

#include <iostream>

ConvertMeshToPoints::ConvertMeshToPoints(vsg::ref_ptr<vsgPoints::Settings> settings) :
    bricks(vsgPoints::Bricks::create(settings))
{
    arrayStateStack.reserve(4);
    arrayStateStack.emplace_back(vsg::ArrayState::create());
}


void ConvertMeshToPoints::applyDraw(uint32_t firstVertex, uint32_t vertexCount, uint32_t firstInstance, uint32_t instanceCount)
{
    auto& arrayState = *arrayStateStack.back();
    uint32_t lastIndex = instanceCount > 1 ? (firstInstance + instanceCount) : firstInstance + 1;
    uint32_t endVertex = firstVertex + vertexCount;
    auto matrix = localToWorld();

    vsg::ubvec4 color(255,255,255,255);

    for (uint32_t instanceIndex = firstInstance; instanceIndex < lastIndex; ++instanceIndex)
    {
        if (auto vertices = arrayState.vertexArray(instanceIndex))
        {
            for (uint32_t i = firstVertex; i < endVertex; ++i)
            {
                bricks->add(matrix * vsg::dvec3(vertices->at(i)), color);
            }
        }
    }
}

void ConvertMeshToPoints::applyDrawIndexed(uint32_t firstIndex, uint32_t indexCount, uint32_t firstInstance, uint32_t instanceCount)
{
    auto& arrayState = *arrayStateStack.back();
    uint32_t lastIndex = instanceCount > 1 ? (firstInstance + instanceCount) : firstInstance + 1;
    uint32_t endIndex = firstIndex + indexCount;
    auto matrix = localToWorld();

    vsg::ubvec4 color(255,255,255,255);

    if (ushort_indices)
    {
        for (uint32_t instanceIndex = firstInstance; instanceIndex < lastIndex; ++instanceIndex)
        {
            if (auto vertices = arrayState.vertexArray(instanceIndex))
            {
                for (uint32_t i = firstIndex; i < endIndex; ++i)
                {
                    bricks->add(matrix * vsg::dvec3(vertices->at(ushort_indices->at(i))), color);
                }
            }
        }
    }
    else if (uint_indices)
    {
        for (uint32_t instanceIndex = firstInstance; instanceIndex < lastIndex; ++instanceIndex)
        {
            if (auto vertices = arrayState.vertexArray(instanceIndex))
            {
                for (uint32_t i = firstIndex; i < endIndex; ++i)
                {
                    bricks->add(matrix * vsg::dvec3(vertices->at(uint_indices->at(i))), color);
                }
            }
        }
    }
}

void ConvertMeshToPoints::apply(const vsg::Node& node)
{
    node.traverse(*this);
}

void ConvertMeshToPoints::apply(const vsg::StateGroup& stategroup)
{
    auto arrayState = stategroup.prototypeArrayState ? stategroup.prototypeArrayState->cloneArrayState(arrayStateStack.back()) : arrayStateStack.back()->cloneArrayState();

    for (auto& statecommand : stategroup.stateCommands)
    {
        statecommand->accept(*arrayState);
    }

    arrayStateStack.emplace_back(arrayState);

    stategroup.traverse(*this);

    arrayStateStack.pop_back();
}

void ConvertMeshToPoints::apply(const vsg::Transform& transform)
{
    auto& l2wStack = localToWorldStack();

    vsg::dmat4 localToWorld = l2wStack.empty() ? transform.transform({}) : transform.transform(l2wStack.back());
    l2wStack.push_back(localToWorld);

    transform.traverse(*this);

    l2wStack.pop_back();
}

void ConvertMeshToPoints::apply(const vsg::LOD& lod)
{
    if (!lod.children.empty() && lod.children[0].node) lod.children[0].node->accept(*this);
}

void ConvertMeshToPoints::apply(const vsg::PagedLOD& plod)
{
    if (plod.children[0].node) plod.children[0].node->accept(*this);
    else if (plod.children[1].node) plod.children[1].node->accept(*this);
}

void ConvertMeshToPoints::apply(const vsg::VertexDraw& vid)
{
    auto& arrayState = *arrayStateStack.back();
    arrayState.apply(vid);
    if (!arrayState.vertices) return;

    applyDraw(vid.firstVertex, vid.vertexCount, vid.firstInstance, vid.instanceCount);
}

void ConvertMeshToPoints::apply(const vsg::VertexIndexDraw& vid)
{
    auto& arrayState = *arrayStateStack.back();
    arrayState.apply(vid);
    if (!arrayState.vertices) return;

    if (vid.indices) vid.indices->accept(*this);

    applyDrawIndexed(vid.firstIndex, vid.indexCount, vid.firstInstance, vid.instanceCount);
}

void ConvertMeshToPoints::apply(const vsg::Geometry& geometry)
{
    auto& arrayState = *arrayStateStack.back();
    arrayState.apply(geometry);
    if (!arrayState.vertices) return;

    if (geometry.indices) geometry.indices->accept(*this);

    for (auto& command : geometry.commands)
    {
        command->accept(*this);
    }
}

void ConvertMeshToPoints::apply(const vsg::BindVertexBuffers& bvb)
{
    arrayStateStack.back()->apply(bvb);
}

void ConvertMeshToPoints::apply(const vsg::BindIndexBuffer& bib)
{
    bib.indices->accept(*this);
}

void ConvertMeshToPoints::apply(const vsg::BufferInfo& bufferInfo)
{
    if (bufferInfo.data) bufferInfo.data->accept(*this);
}

void ConvertMeshToPoints::apply(const vsg::ushortArray& array)
{
    ushort_indices = &array;
    uint_indices = nullptr;
}

void ConvertMeshToPoints::apply(const vsg::uintArray& array)
{
    ushort_indices = nullptr;
    uint_indices = &array;
}

void ConvertMeshToPoints::apply(const vsg::Draw& draw)
{
    applyDraw(draw.firstVertex, draw.vertexCount, draw.firstInstance, draw.instanceCount);
}

void ConvertMeshToPoints::apply(const vsg::DrawIndexed& drawIndexed)
{
    applyDrawIndexed(drawIndexed.firstIndex, drawIndexed.indexCount, drawIndexed.firstInstance, drawIndexed.instanceCount);
}
