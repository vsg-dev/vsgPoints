#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <vsgPoints/BrickBuilder.h>

#include <iostream>
#include <sstream>
#include <iomanip>

#include <filesystem>
#include <string_view>

#pragma pack(1)

struct VsgIOPoint
{
    vsg::dvec3 v;
    vsg::ubvec3 c;
};

struct PackedPoint
{
    vsg::usvec3 v;
    vsg::ubvec4 c;
};

#pragma pack()


template<typename T>
std::string format_number(T v)
{
    std::vector<T> parts;
    do
    {
        parts.push_back(v % 1000);
        v /= 1000;
    } while (v > 0);


    std::stringstream s;
    auto itr = parts.rbegin();
    s << *(itr++);
    while(itr != parts.rend())
    {
        s << ',';
        s << std::setfill('0') << std::setw(3) << *(itr++);
    }

    return s.str();
}

struct Settings
{
    size_t numPointsPerBlock = 10000;
    double precision = 0.001;
    uint32_t bits = 8;
    bool write = false;
    bool plod = false;
    vsg::Path path;
    vsg::Path extension = ".vsgb";
    vsg::ref_ptr<vsg::Options> options;
    vsg::dvec3 offset;
    vsg::dbox bound;
};

using Key = vsg::ivec4;

class Brick : public vsg::Inherit<vsg::Object, Brick>
{
public:

    std::vector<PackedPoint> points;

    vsg::ref_ptr<vsg::Node> createRendering(const Settings& settings, const vsg::vec4& positionScale, const vsg::vec2& pointSize)
    {
        vsg::ref_ptr<vsg::Data> vertices;

        auto normals = vsg::vec3Value::create(vsg::vec3(0.0f, 0.0f, 1.0f));
        auto colors = vsg::ubvec4Array::create(points.size(), vsg::Data::Properties(VK_FORMAT_R8G8B8A8_UNORM));
        auto positionScaleValue = vsg::vec4Value::create(positionScale);
        auto pointSizeValue = vsg::vec2Value::create(pointSize);

        normals->properties.format = VK_FORMAT_R32G32B32_SFLOAT;
        positionScaleValue->properties.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        pointSizeValue->properties.format = VK_FORMAT_R32G32_SFLOAT;

        if (settings.bits==8)
        {
            auto vertices_8bit = vsg::ubvec3Array::create(points.size(), vsg::Data::Properties(VK_FORMAT_R8G8B8_UNORM));
            auto vertex_itr = vertices_8bit->begin();
            auto color_itr = colors->begin();
            for(auto& point : points)
            {
                (vertex_itr++)->set(static_cast<uint8_t>(point.v.x), static_cast<uint8_t>(point.v.y), static_cast<uint8_t>(point.v.z));
                *(color_itr++) = point.c;
            }

            vertices = vertices_8bit;
        }
        else if (settings.bits==10)
        {
            auto vertices_10bit = vsg::uintArray::create(points.size(), vsg::Data::Properties(VK_FORMAT_A2R10G10B10_UNORM_PACK32));
            auto vertex_itr = vertices_10bit->begin();
            auto color_itr = colors->begin();
            for(auto& point : points)
            {
                *(vertex_itr++) = 3 << 30 | (static_cast<uint32_t>(point.v.x) << 20) | (static_cast<uint32_t>(point.v.y) << 10) | (static_cast<uint32_t>(point.v.z));
                *(color_itr++) = point.c;
            }

            vertices = vertices_10bit;
        }
        else if (settings.bits==16)
        {
            auto vertices_16bit = vsg::usvec3Array::create(points.size(), vsg::Data::Properties(VK_FORMAT_R16G16B16_UNORM));
            auto vertex_itr = vertices_16bit->begin();
            auto color_itr = colors->begin();
            for(auto& point : points)
            {
                (vertex_itr++)->set(static_cast<uint16_t>(point.v.x), static_cast<uint16_t>(point.v.y), static_cast<uint16_t>(point.v.z));
                *(color_itr++) = point.c;
            }

            vertices = vertices_16bit;
        }
        else
        {
            return {};
        }


        // set up vertexDraw that will do the rendering.
        auto vertexDraw = vsg::VertexDraw::create();
        vertexDraw->assignArrays({vertices, normals, colors, positionScaleValue, pointSizeValue});
        vertexDraw->vertexCount = points.size();
        vertexDraw->instanceCount = 1;

        return vertexDraw;
    }

    vsg::ref_ptr<vsg::Node> createRendering(const Settings& settings, Key key, vsg::dbox& bound)
    {
        double brickPrecision = settings.precision * static_cast<double>(key.w);
        double brickSize = brickPrecision * pow(2.0, static_cast<double>(settings.bits));

        vsg::dvec3 position(static_cast<double>(key.x) * brickSize, static_cast<double>(key.y) * brickSize, static_cast<double>(key.z) * brickSize);
        position -= settings.offset;

        for(auto& point : points)
        {
            auto& v = point.v;
            bound.add(position.x + brickPrecision * static_cast<double>(v.x),
                      position.y + brickPrecision * static_cast<double>(v.y),
                      position.z + brickPrecision * static_cast<double>(v.z));
        }

        vsg::vec2 pointSize(brickPrecision*4.0, brickPrecision);
        vsg::vec4 positionScale(position.x, position.y, position.z, brickSize);

        return createRendering(settings, positionScale, pointSize);
    }

protected:
    virtual ~Brick() {}
};


using Bricks = std::map<Key, vsg::ref_ptr<Brick>>;
using Levels = std::list<Bricks>;

bool readBricks(const vsg::Path filename, Settings& settings, Bricks& bricks)
{
    auto& bound = settings.bound;
    bound.reset();

    std::ifstream fin(filename, std::ios::in | std::ios::binary);
    if (!fin) return false;


    fin.seekg(0, fin.end);
    size_t fileSize = fin.tellg();
    size_t numPoints = fileSize / sizeof(VsgIOPoint);
    std::cout<<"    "<<filename<<" size = "<<format_number(fileSize)<<std::endl;
    std::cout<<"    numPoints = "<<format_number(numPoints)<<std::endl;

    if (numPoints == 0) return false;

    fin.clear();
    fin.seekg(0, fin.beg);

    double multiplier = 1.0 / settings.precision;
    auto points = vsg::Array<VsgIOPoint>::create(settings.numPointsPerBlock);
    size_t numPointsRead = 0;

    decltype(PackedPoint::c)::value_type alpha = 255;

    int64_t divisor = 1 << settings.bits;
    int64_t mask = divisor - 1;

    std::cout<<"Reading data: divisor = "<<divisor<<", mask = "<<mask<<std::endl;
    while(fin)
    {
        size_t numPointsToRead = std::min(numPoints - numPointsRead, settings.numPointsPerBlock);

        if (numPointsToRead == 0) break;

        fin.read(reinterpret_cast<char*>(points->dataPointer()), numPointsToRead * sizeof(VsgIOPoint));

        numPointsRead += numPointsToRead;

        for(auto& point : *points)
        {
            if (numPointsToRead==0) break;
            --numPointsToRead;

            bound.add(point.v);

            vsg::dvec3 scaled_v = point.v * multiplier;
            vsg::dvec3 rounded_v = {std::round(scaled_v.x), std::round(scaled_v.y), std::round(scaled_v.z)};
            vsg::t_vec3<int64_t> int64_v = {static_cast<int64_t>(rounded_v.x),  static_cast<int64_t>(rounded_v.y), static_cast<int64_t>(rounded_v.z)};
            Key key = { static_cast<int32_t>(int64_v.x / divisor), static_cast<int32_t>(int64_v.y / divisor), static_cast<int32_t>(int64_v.z / divisor), 1};

            PackedPoint packedPoint;
            packedPoint.v.set(static_cast<uint16_t>(int64_v.x & mask), static_cast<uint16_t>(int64_v.y & mask), static_cast<uint16_t>(int64_v.z & mask));
            packedPoint.c.set(point.c.r, point.c.g, point.c.b, alpha) ;

            auto& brick = bricks[key];
            if (!brick)
            {
                brick = Brick::create();
            }

            brick->points.push_back(packedPoint);
        }
    }
    std::cout<<"Read bound "<<bound<<std::endl;

    return true;
}

bool generateLevel(Bricks& source, Bricks& destination, const Settings& settings)
{
    int32_t bits = settings.bits;
    for(auto& [source_key, source_brick] : source)
    {
        Key destination_key = {source_key.x / 2, source_key.y / 2, source_key.z / 2, source_key.w * 2};
        vsg::ivec3 offset = {(source_key.x & 1) << bits, (source_key.y & 1) << bits, (source_key.z & 1) << bits};

        auto& destinatio_brick = destination[destination_key];
        if (!destinatio_brick) destinatio_brick = Brick::create();

        auto& source_points = source_brick->points;
        auto& desintation_points = destinatio_brick->points;
        size_t count = source_points.size();
        for(size_t i = 0; i < count; i+= 4)
        {
            auto& p = source_points[i];

            PackedPoint new_p;
            new_p.v.x = static_cast<uint16_t>((static_cast<int32_t>(p.v.x) + offset.x)/2);
            new_p.v.y = static_cast<uint16_t>((static_cast<int32_t>(p.v.y) + offset.y)/2);
            new_p.v.z = static_cast<uint16_t>((static_cast<int32_t>(p.v.z) + offset.z)/2);
            new_p.c = p.c;

            desintation_points.push_back(new_p);
        }

    }
    return !destination.empty();
}

vsg::ref_ptr<vsg::StateGroup> createStateGroup(const Settings& settings)
{
    auto textureData = vsgPoints::createParticleImage(64);
    auto shaderSet = vsgPoints::createPointsFlatShadedShaderSet(settings.options);
    auto config = vsg::GraphicsPipelineConfig::create(shaderSet);
    bool blending = false;

    auto& defines = config->shaderHints->defines;
    defines.insert("VSG_POINT_SPRITE");

    if (settings.bits==8)
    {
        config->enableArray("vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, sizeof(vsg::ubvec3), VK_FORMAT_R8G8B8_UNORM);
    }
    else if (settings.bits==10)
    {
        config->enableArray("vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, 4, VK_FORMAT_A2R10G10B10_UNORM_PACK32);
    }
    else if (settings.bits==16)
    {
        config->enableArray("vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, sizeof(vsg::usvec3), VK_FORMAT_R16G16B16_UNORM);
    }
    else
    {
        std::cout<<"Unsupported number of bits "<<settings.bits<<std::endl;
        return {};
    }

    config->enableArray("vsg_Normal", VK_VERTEX_INPUT_RATE_INSTANCE, sizeof(vsg::vec3), VK_FORMAT_R32G32B32_SFLOAT);
    config->enableArray("vsg_Color", VK_VERTEX_INPUT_RATE_VERTEX, sizeof(vsg::ubvec4), VK_FORMAT_R8G8B8A8_UNORM);
    config->enableArray("vsg_PositionScale", VK_VERTEX_INPUT_RATE_INSTANCE, sizeof(vsg::vec4), VK_FORMAT_R32G32B32A32_SFLOAT);
    config->enableArray("vsg_PointSize", VK_VERTEX_INPUT_RATE_INSTANCE, sizeof(vsg::vec2), VK_FORMAT_R32G32_SFLOAT);

    vsg::Descriptors descriptors;
    if (textureData)
    {
        auto sampler = vsg::Sampler::create();
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        config->assignTexture(descriptors, "diffuseMap", textureData, sampler);
    }

    auto mat = vsg::PhongMaterialValue::create();
    mat->value().alphaMask = 1.0f;
    mat->value().alphaMaskCutoff = 0.0025f;
    config->assignUniform(descriptors, "material", mat);

    auto vdsl = vsg::ViewDescriptorSetLayout::create();
    config->additionalDescriptorSetLayout = vdsl;

    config->colorBlendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{
        {blending, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_SUBTRACT, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT}};

    config->inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    config->init();

    auto descriptorSet = vsg::DescriptorSet::create(config->descriptorSetLayout, descriptors);
    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, config->layout, 0, descriptorSet);

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto stateGroup = vsg::StateGroup::create();
    stateGroup->add(config->bindGraphicsPipeline);
    stateGroup->add(bindDescriptorSet);

    // assign any custom ArrayState that may be required.
    stateGroup->prototypeArrayState = shaderSet->getSuitableArrayState(config->shaderHints->defines);

    auto bindViewDescriptorSets = vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, config->layout, 1);
    stateGroup->add(bindViewDescriptorSets);

    return stateGroup;
}

vsg::ref_ptr<vsg::Node> subtile(Settings& settings, Levels::reverse_iterator level_itr, Levels::reverse_iterator end_itr, Key key, vsg::dbox& bound, bool root = false)
{
    if (level_itr == end_itr) return {};

    Bricks& bricks = *level_itr;
    auto itr = bricks.find(key);
    if (itr == bricks.end())
    {
        //std::cout<<"   "<<key<<" null "<<std::endl;
        return {};
    }

    auto& brick = (itr->second);
    auto next_itr = level_itr;
    ++next_itr;

    if (next_itr != end_itr)
    {
        std::array<vsg::ref_ptr<vsg::Node>, 8> children;
        size_t num_children = 0;

        Key subkey{key.x * 2, key.y * 2, key.z * 2, key.w / 2};

        vsg::dbox subtiles_bound;

        if (auto child = subtile(settings, next_itr, end_itr, subkey, subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey+Key(1, 0, 0, 0), subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey+Key(0, 1, 0, 0), subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey+Key(1, 1, 0, 0), subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey+Key(0, 0, 1, 0), subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey+Key(1, 0, 1, 0), subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey+Key(0, 1, 1, 0), subtiles_bound)) children[num_children++] = child;
        if (auto child = subtile(settings, next_itr, end_itr, subkey+Key(1, 1, 1, 0), subtiles_bound)) children[num_children++] = child;


        vsg::Path path = vsg::make_string(settings.path,"/",key.w,"/",key.z,"/",key.y);
        vsg::Path filename = vsg::make_string(key.x, settings.extension);
        vsg::Path full_path = path/filename;

        vsg::makeDirectory(path);


        if (num_children==1)
        {
            write(children[0], full_path);
        }
        else
        {
            auto group = vsg::Group::create();
            for(size_t i = 0; i < num_children; ++i)
            {
                group->addChild(children[i]);
            }
            write(group, full_path);
        }

        vsg::dbox local_bound;
        auto brick_node = brick->createRendering(settings, key, local_bound);

        // std::cout<<"   "<<key<<" "<<brick<<" num_children = "<<num_children<<" full_path = "<<full_path<<", bound = "<<subtiles_bound<<std::endl;

        double transition = 0.125;
        auto plod = vsg::PagedLOD::create();
        if (subtiles_bound.valid())
        {
            bound.add(subtiles_bound);
            plod->bound.center = (subtiles_bound.min + subtiles_bound.max) * 0.5;
            plod->bound.radius = vsg::length(subtiles_bound.max - subtiles_bound.min) * 0.5;

            double brickPrecision = settings.precision * static_cast<double>(key.w);
            double brickSize = brickPrecision * pow(2.0, static_cast<double>(settings.bits));

            vsg::dvec3 subtilesSize = (subtiles_bound.max - subtiles_bound.min);
            double maxSize = std::max(brickPrecision, std::max(std::max(subtilesSize.x, subtilesSize.y), subtilesSize.z));
            transition *= (maxSize/brickSize);
            // std::cout<<"maxSize = "<<maxSize<<", brickSize = "<<brickSize<<", ratio = "<<maxSize/brickSize<<std::endl;
        }
        else
        {
            std::cout<<"Warning: unable to set PagedLOD bounds"<<std::endl;
        }

        plod->children[0] = vsg::PagedLOD::Child{transition, {}}; // external child visible when it's bound occupies more than 1/4 of the height of the window
        plod->children[1] = vsg::PagedLOD::Child{0.0, brick_node}; // visible always

        if (root)
        {
            plod->filename = full_path;
        }
        else
        {
            plod->filename = vsg::Path("../../../..")/full_path;
        }


        return plod;
    }
    else
    {

        return brick->createRendering(settings, key, bound);
        std::cout<<"   "<<key<<" "<<brick<<" leaf, bound "<<bound<<std::endl;
    }

    return vsg::Node::create();
}

vsg::ref_ptr<vsg::Node> createPagedLOD(Levels& levels, Settings& settings)
{
    if (levels.empty()) return {};

    auto stateGroup = createStateGroup(settings);

    std::basic_ostringstream<vsg::Path::value_type> str;

    double brickSize = settings.precision * pow(2.0, static_cast<double>(settings.bits));
    double rootBrickSize = brickSize * std::pow(2.0, levels.size()-1);

    std::cout<<"rootBrickSize  = "<<rootBrickSize<<std::endl;

    // If only one level is present then PagedLOD not required so just add all the levels bricks to the StateGroup
    if (levels.size() == 1)
    {
        vsg::dbox bound;
        for(auto& [key, brick] : levels.back())
        {
            auto tile = brick->createRendering(settings, key, bound);
            stateGroup->addChild(tile);
        }

        return stateGroup;
    }

    // more than 1 level so create a PagedLOD hierarchy.

    auto current_itr = levels.rbegin();

    // root tile
    auto& root_level = *current_itr;
    std::cout<<"root level " << root_level.size()<<std::endl;

    for(auto& [key, brick] : root_level)
    {
        std::cout<<"root key = "<<key<<" "<<brick<<std::endl;
        vsg::dbox bound;
        if (auto child = subtile(settings, current_itr, levels.rend(), key, bound, true))
        {
            std::cout<<"root child "<<child<<std::endl;
            stateGroup->addChild(child);
        }
    }

    return stateGroup;
}

vsg::ref_ptr<vsg::Node> processRawData(const vsg::Path filename, Settings& settings)
{
    double brickSize = settings.precision * pow(2.0, static_cast<double>(settings.bits));

    std::cout<<"sizeof(VsgIOPoint) = "<<sizeof(VsgIOPoint)<<std::endl;
    std::cout<<"brickSize = "<<brickSize<<std::endl;

    std::list<Bricks> levels(1);
    auto& first_level = levels.front();
    if (!readBricks(filename, settings, first_level))
    {
        std::cout<<"Waring: unable to read file."<<std::endl;
        return {};
    }

    if (settings.bound.valid())
    {
        settings.offset = (settings.bound.max + settings.bound.min) * 0.5;
    }

    std::cout<<"After reading data "<<first_level.size()<<std::endl;

    size_t biggestBrick = 0;
    vsg::t_box<int32_t> keyBounds;
    for(auto& [key, brick] : first_level)
    {
        keyBounds.add(key.x, key.y, key.z);
        if (brick->points.size() > biggestBrick) biggestBrick = brick->points.size();
    }

    while(levels.back().size() > 1)
    {
        auto& source = levels.back();

        levels.push_back(Bricks());
        auto& destination = levels.back();

        if (!generateLevel(source, destination, settings)) break;
    }

    std::cout<<"levels = "<<levels.size()<<std::endl;

    std::cout<<"keyBounds "<<keyBounds<<std::endl;
    std::cout<<"biggest brick "<<biggestBrick<<std::endl;

    std::cout<<"PackedVertex = "<<sizeof(PackedVertex)<<std::endl;

    auto transform = vsg::MatrixTransform::create();
    transform->matrix = vsg::translate(settings.offset);

    if (auto model = createPagedLOD(levels, settings))
    {
        transform->addChild(model);
    }
    return transform;
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->sharedObjects = vsg::SharedObjects::create();

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    if (int type; arguments.read("--allocator", type)) vsg::Allocator::instance()->allocatorType = vsg::AllocatorType(type);
    if (size_t objectsBlockSize; arguments.read("--objects", objectsBlockSize)) vsg::Allocator::instance()->setBlockSize(vsg::ALLOCATOR_AFFINITY_OBJECTS, objectsBlockSize);
    if (size_t nodesBlockSize; arguments.read("--nodes", nodesBlockSize)) vsg::Allocator::instance()->setBlockSize(vsg::ALLOCATOR_AFFINITY_NODES, nodesBlockSize);
    if (size_t dataBlockSize; arguments.read("--data", dataBlockSize)) vsg::Allocator::instance()->setBlockSize(vsg::ALLOCATOR_AFFINITY_DATA, dataBlockSize);


    std::cout<<"vsg::Allocator::instance()->allocatorType = "<<int(vsg::Allocator::instance()->allocatorType)<<std::endl;
    std::cout<<"vsg::Allocator::instance()->getMemoryBlocks(vsg::ALLOCATOR_AFFINITY_OBJECTS)->blockSize = "<<
        vsg::Allocator::instance()->getMemoryBlocks(vsg::ALLOCATOR_AFFINITY_OBJECTS)->blockSize<<std::endl;

    Settings settings;
    settings.numPointsPerBlock = arguments.value<size_t>(10000, "-b");
    settings.precision = arguments.value<double>(0.001, "-p");
    settings.bits = arguments.value<uint32_t>(8, "--bits");
    settings.options = options;

    auto outputFilename = arguments.value<vsg::Path>("", "-o");

    auto group = vsg::Group::create();


    vsg::Path filename;
    while(arguments.read("-i", filename))
    {
        std::cout<<"filename = "<<filename<<std::endl;
        if (auto found_filename = vsg::findFile(filename, options))
        {
            if (outputFilename)
            {
                settings.path = vsg::filePath(outputFilename)/vsg::simpleFilename(outputFilename);
                settings.extension = vsg::fileExtension(outputFilename);
            }

            if (auto scene = processRawData(found_filename, settings))
            {
                group->addChild(scene);
            }
        }
    }

    if (outputFilename && !group->children.empty())
    {
        if (group->children.size()==1) vsg::write(group->children[0], outputFilename, options);
        else vsg::write(group, outputFilename, options);
    }

    if (arguments.read("-r") && !group->children.empty() && settings.bound.valid())
    {
        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});

        std::cout<<"windowTraits->debugLayer = "<<windowTraits->debugLayer<<std::endl;
        std::cout<<"windowTraits->apiDumpLayer = "<<windowTraits->apiDumpLayer<<std::endl;

        auto viewer = vsg::Viewer::create();
        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Could not create windows." << std::endl;
            return 1;
        }

        viewer->addWindow(window);

        auto& bound = settings.bound;
        vsg::dsphere bs((bound.max + bound.min) * 0.5, vsg::length(bound.max - bound.min)*0.5);
        double nearFarRatio = 0.001;

        auto lookAt = vsg::LookAt::create(bs.center - vsg::dvec3(0.0, -bs.radius * 3.5, 0.0), bs.center, vsg::dvec3(0.0, 0.0, 1.0));
        auto perspective  = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * bs.radius, bs.radius * 4.5);

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // add close handler to respond the close window button and pressing escape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));
        viewer->addEventHandler(vsg::Trackball::create(camera));

        auto commandGraph = vsg::createCommandGraphForView(window, camera, group);
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        viewer->compile();

        // rendering main loop
        while (viewer->advanceToNextFrame())
        {
            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

            viewer->update();

            viewer->recordAndSubmit();

            viewer->present();
        }

    }

    return 0;
}
