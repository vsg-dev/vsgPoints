#include <vsg/all.h>

#include <vsgPoints/Bricks.h>

class ConvertMeshToPoints : public vsg::Inherit<vsg::ConstVisitor, ConvertMeshToPoints>
{
public:

    ConvertMeshToPoints(vsg::ref_ptr<vsgPoints::Settings> settings);

    vsg::ref_ptr<vsgPoints::Bricks> bricks = vsgPoints::Bricks::create();

    using ArrayStateStack = std::vector<vsg::ref_ptr<vsg::ArrayState>>;
    ArrayStateStack arrayStateStack;
    vsg::ref_ptr<const vsg::ushortArray> ushort_indices;
    vsg::ref_ptr<const vsg::uintArray> uint_indices;

    /// get the current local to world matrix stack
    std::vector<vsg::dmat4>& localToWorldStack() { return arrayStateStack.back()->localToWorldStack; }
    vsg::dmat4 localToWorld() const { auto matrixStack = arrayStateStack.back()->localToWorldStack; return matrixStack.empty() ? vsg::dmat4{} : matrixStack.back(); }

    void applyDraw(uint32_t firstVertex, uint32_t vertexCount, uint32_t firstInstance, uint32_t instanceCount);
    void applyDrawIndexed(uint32_t firstIndex, uint32_t indexCount, uint32_t firstInstance, uint32_t instanceCount);

    void apply(const vsg::Node& node) override;
    void apply(const vsg::StateGroup& stategroup) override;
    void apply(const vsg::Transform& transform) override;
    void apply(const vsg::LOD& lod) override;
    void apply(const vsg::PagedLOD& plod) override;
    void apply(const vsg::VertexDraw& vid) override;
    void apply(const vsg::VertexIndexDraw& vid) override;
    void apply(const vsg::Geometry& geometry) override;
    void apply(const vsg::BindVertexBuffers& bvb) override;
    void apply(const vsg::BindIndexBuffer& bib) override;
    void apply(const vsg::BufferInfo& bufferInfo) override;
    void apply(const vsg::ushortArray& array) override;
    void apply(const vsg::uintArray& array) override;
    void apply(const vsg::Draw& draw) override;
    void apply(const vsg::DrawIndexed& drawIndexed) override;
};
