#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <vsgPoints/BrickBuilder.h>

#include <iostream>
#include <sstream>
#include <iomanip>

#pragma pack(1)

struct VsgIOPoint
{
    vsg::dvec3 v;
    vsg::ubvec3 c;
};

struct PackedPoint
{
    vsg::ubvec3 v;
    vsg::ubvec3 c;
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

class Brick : public vsg::Inherit<vsg::Object, Brick>
{
public:

    std::vector<PackedPoint> points;

    vsg::ref_ptr<vsg::Node> createRendering(const vsg::vec4& positionScale, const vsg::vec2& pointSize)
    {
        auto vertices = vsg::ubvec3Array::create(points.size(), vsg::Data::Properties(VK_FORMAT_R8G8B8_UNORM));
        auto normals = vsg::vec3Value::create(vsg::vec3(0.0f, 0.0f, 1.0f));
        auto colors = vsg::ubvec3Array::create(points.size(), vsg::Data::Properties(VK_FORMAT_R8G8B8_UNORM));
        auto positionScaleValue = vsg::vec4Value::create(positionScale);
        auto pointSizeValue = vsg::vec2Value::create(pointSize);

        normals->properties.format = VK_FORMAT_R32G32B32_SFLOAT;
        positionScaleValue->properties.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        pointSizeValue->properties.format = VK_FORMAT_R32G32_SFLOAT;

        auto vertex_itr = vertices->begin();
        auto color_itr = colors->begin();
        for(auto& point : points)
        {
            *(vertex_itr++) = point.v;
            *(color_itr++) = point.c;
        }

        // set up vertexDraw that will do the rendering.
        auto vertexDraw = vsg::VertexDraw::create();
        vertexDraw->assignArrays({vertices, normals, colors, positionScaleValue, pointSizeValue});
        vertexDraw->vertexCount = points.size();
        vertexDraw->instanceCount = 1;

        return vertexDraw;
    }
protected:
    virtual ~Brick() {}
};


struct Settings
{
    size_t numPointsPerBlock = 10000;
    double precision = 0.001;
    double bits = 8.0;
    bool write = false;
    bool plod = false;
    vsg::Path extension = ".vsgb";
    vsg::ref_ptr<vsg::Options> options;
    vsg::dbox bound;
};

using Key = vsg::ivec4;
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

    std::cout<<"Reading data."<<std::endl;
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
            Key key = { static_cast<int32_t>(int64_v.x / 256), static_cast<int32_t>(int64_v.y / 256), static_cast<int32_t>(int64_v.z / 256), 1};

            PackedPoint packedPoint;
            packedPoint.v = { static_cast<uint8_t>(int64_v.x & 0xff), static_cast<uint8_t>(int64_v.y & 0xff), static_cast<uint8_t>(int64_v.z & 0xff)};
            packedPoint.c = point.c;

            auto& brick = bricks[key];
            if (!brick)
            {
                brick = Brick::create();
            }

            brick->points.push_back(packedPoint);
        }

    }
    return true;
}

bool generateLevel(Bricks& source, Bricks& destination)
{
    for(auto& [source_key, source_brick] : source)
    {
        Key destination_key = {source_key.x / 2, source_key.y / 2, source_key.z / 2, source_key.w * 2};
        vsg::ivec3 offset = {(source_key.x & 1) << 8, (source_key.y & 1) << 8, (source_key.z & 1) << 8};

        auto& destinatio_brick = destination[destination_key];
        if (!destinatio_brick) destinatio_brick = Brick::create();

        auto& source_points = source_brick->points;
        auto& desintation_points = destinatio_brick->points;
        size_t count = source_points.size();
        for(size_t i = 0; i < count; i+= 4)
        {
            auto& p = source_points[i];

            PackedPoint new_p;
            new_p.v.x = static_cast<int8_t>((static_cast<int32_t>(p.v.x) + offset.x)/2);
            new_p.v.y = static_cast<int8_t>((static_cast<int32_t>(p.v.y) + offset.y)/2);
            new_p.v.z = static_cast<int8_t>((static_cast<int32_t>(p.v.z) + offset.z)/2);
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

    config->enableArray("vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, sizeof(vsg::ubvec3), VK_FORMAT_R8G8B8_UNORM);
    config->enableArray("vsg_Normal", VK_VERTEX_INPUT_RATE_INSTANCE, sizeof(vsg::vec3), VK_FORMAT_R32G32B32_SFLOAT);
    config->enableArray("vsg_Color", VK_VERTEX_INPUT_RATE_VERTEX, sizeof(vsg::ubvec3), VK_FORMAT_R8G8B8_UNORM);
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

vsg::ref_ptr<vsg::Node> writeBricks(Levels& levels, const vsg::Path filename, Settings& settings)
{
    auto deliminator = vsg::Path::preferred_separator;
    vsg::Path path = vsg::filePath(filename);
    vsg::Path name = vsg::simpleFilename(filename);
    vsg::Path ext = settings.extension;

    std::basic_ostringstream<vsg::Path::value_type> str;

    double brickSize = settings.precision * pow(2.0, static_cast<double>(settings.bits));
    double levelBrickSize = brickSize;

    vsg::ref_ptr<vsg::Node> last;

    for(auto& bricks : levels)
    {
        for(auto& [key, brick] : bricks)
        {
            str.clear();
            str.str("");
            if (path) str << path << deliminator;
            str << name << deliminator << key.w << deliminator << key.z << deliminator << key.y;
            vsg::Path brick_path(str.str());

            str.clear();
            str.str("");
            str << key.x << ext;
            vsg::Path brick_filename(str.str());

            vsg::makeDirectory(brick_path);

            vsg::Path full_path = brick_path/brick_filename;

            float brickPrecision = static_cast<float>(levelBrickSize / 256.0);
            vsg::vec2 pointSize(brickPrecision, brickPrecision);

            vsg::vec4 originScale(static_cast<double>(key.x) * levelBrickSize, static_cast<double>(key.y) * levelBrickSize, static_cast<double>(key.z) * levelBrickSize, levelBrickSize);
            auto tile = brick->createRendering(originScale, pointSize);
            vsg::write(tile, full_path);

            last = tile;

            // std::cout<<"path = "<<brick_path<<"\tfilename = "<<brick_filename<<std::endl;
        }
        levelBrickSize *= 2.0;
    }

    return last;
}

vsg::ref_ptr<vsg::Node> subtile(Levels::reverse_iterator level_itr, Levels::reverse_iterator end_itr, Key key)
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

        if (auto child = subtile(next_itr, end_itr, subkey)) children[num_children++] = child;
        if (auto child = subtile(next_itr, end_itr, subkey+Key(1, 0, 0, 0))) children[num_children++] = child;
        if (auto child = subtile(next_itr, end_itr, subkey+Key(0, 1, 0, 0))) children[num_children++] = child;
        if (auto child = subtile(next_itr, end_itr, subkey+Key(1, 1, 0, 0))) children[num_children++] = child;
        if (auto child = subtile(next_itr, end_itr, subkey+Key(0, 0, 1, 0))) children[num_children++] = child;
        if (auto child = subtile(next_itr, end_itr, subkey+Key(1, 0, 1, 0))) children[num_children++] = child;
        if (auto child = subtile(next_itr, end_itr, subkey+Key(0, 1, 1, 0))) children[num_children++] = child;
        if (auto child = subtile(next_itr, end_itr, subkey+Key(1, 1, 1, 0))) children[num_children++] = child;

        std::cout<<"   "<<key<<" "<<brick<<" num_children = "<<num_children<<std::endl;

        vsg::Path path = vsg::make_string("test/",key.w,"/",key.z,"/",key.y);
        vsg::Path filename = vsg::make_string(key.x, ".vsgt");
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

        auto brick_node = vsg::VertexDraw::create();
        vsg::dsphere bound;

        auto plod = vsg::PagedLOD::create();
        plod->bound = bound;
        plod->children[0] = vsg::PagedLOD::Child{0.25, {}}; // external child visible when it's bound occupies more than 1/4 of the height of the window
        plod->children[1] = vsg::PagedLOD::Child{0.0, brick_node}; // visible always
        plod->filename = full_path;

        return plod;
    }
    else
    {
        std::cout<<"   "<<key<<" "<<brick<<" leaf"<<std::endl;

        auto leaf = vsg::VertexDraw::create();
        return leaf;
    }

    return vsg::Node::create();
}

vsg::ref_ptr<vsg::Node> createPagedLOD(Levels& levels, const vsg::Path filename, Settings& settings)
{
    if (levels.empty()) return {};

    auto stateGroup = createStateGroup(settings);

    auto deliminator = vsg::Path::preferred_separator;
    vsg::Path path = vsg::filePath(filename);
    vsg::Path name = vsg::simpleFilename(filename);
    vsg::Path ext = settings.extension;

    std::basic_ostringstream<vsg::Path::value_type> str;

    double brickSize = settings.precision * pow(2.0, static_cast<double>(settings.bits));
    double rootBrickSize = brickSize * std::pow(2.0, levels.size()-1);

    std::cout<<"rootBrickSize  = "<<rootBrickSize<<std::endl;

    // If only one level is present then PagedLOD not required so just add all the levels bricks to the StateGroup
    if (levels.size() == 1)
    {
        double levelBrickSize = rootBrickSize;

        float brickPrecision = static_cast<float>(levelBrickSize / 256.0);
        vsg::vec2 pointSize(brickPrecision, brickPrecision);

        for(auto& [key, brick] : levels.back())
        {
             vsg::vec4 originScale(static_cast<double>(key.x) * levelBrickSize, static_cast<double>(key.y) * levelBrickSize, static_cast<double>(key.z) * levelBrickSize, levelBrickSize);

            auto tile = brick->createRendering(originScale, pointSize);
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
        if (auto child = subtile(current_itr, levels.rend(), key))
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

        if (!generateLevel(source, destination)) break;
    }

    std::cout<<"levels = "<<levels.size()<<std::endl;

    std::cout<<"keyBounds "<<keyBounds<<std::endl;
    std::cout<<"biggest brick "<<biggestBrick<<std::endl;


    if (settings.plod)
    {
        return createPagedLOD(levels, filename, settings);
    }
    else if (settings.write)
    {
        vsg::ref_ptr<vsg::Node> last = writeBricks(levels, filename, settings);
        auto stateGroup = createStateGroup(settings);
        stateGroup->addChild(last);
        return stateGroup;
    }


    return {};
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
    settings.bits = arguments.value<double>(8, "--bits");
    settings.write = arguments.read("-w");
    settings.plod = arguments.read("--plod");
    settings.options = options;

    auto outputFilename = arguments.value<vsg::Path>("", "-o");

    auto group = vsg::Group::create();


    vsg::Path filename;
    while(arguments.read("-i", filename))
    {
        std::cout<<"filename = "<<filename<<std::endl;
        if (auto found_filename = vsg::findFile(filename, options))
        {
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
