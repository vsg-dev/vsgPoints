#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <vsgPoints/Brick.h>

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

bool readBricks(const vsg::Path filename, vsgPoints::Settings& settings, vsgPoints::Bricks& bricks)
{
    auto& bound = settings.bound;
    bound.reset();

    std::ifstream fin(filename, std::ios::in | std::ios::binary);
    if (!fin) return false;

    double multiplier = 1.0 / settings.precision;
    auto points = vsg::Array<VsgIOPoint>::create(settings.numPointsPerBlock);

    decltype(vsgPoints::PackedPoint::c)::value_type alpha = 255;

    int64_t divisor = 1 << settings.bits;
    int64_t mask = divisor - 1;

    while(fin)
    {
        size_t bytesToRead = settings.numPointsPerBlock * sizeof(VsgIOPoint);
        fin.read(reinterpret_cast<char*>(points->dataPointer()), bytesToRead);

        size_t numPointsRead = static_cast<size_t>(fin.gcount()) / sizeof(VsgIOPoint);
        if (numPointsRead == 0) break;

        for(size_t i =0; i<numPointsRead; ++i)
        {
            auto& point = (*points)[i];

            bound.add(point.v);

            vsg::dvec3 scaled_v = point.v * multiplier;
            vsg::dvec3 rounded_v = {std::round(scaled_v.x), std::round(scaled_v.y), std::round(scaled_v.z)};
            vsg::t_vec3<int64_t> int64_v = {static_cast<int64_t>(rounded_v.x),  static_cast<int64_t>(rounded_v.y), static_cast<int64_t>(rounded_v.z)};
            vsgPoints::Key key = { static_cast<int32_t>(int64_v.x / divisor), static_cast<int32_t>(int64_v.y / divisor), static_cast<int32_t>(int64_v.z / divisor), 1};

            vsgPoints::PackedPoint packedPoint;
            packedPoint.v.set(static_cast<uint16_t>(int64_v.x & mask), static_cast<uint16_t>(int64_v.y & mask), static_cast<uint16_t>(int64_v.z & mask));
            packedPoint.c.set(point.c.r, point.c.g, point.c.b, alpha) ;

            auto& brick = bricks[key];
            if (!brick)
            {
                brick = vsgPoints::Brick::create();
            }

            brick->points.push_back(packedPoint);
        }
    }
    std::cout<<"Read bound "<<bound<<std::endl;

    return true;
}

vsg::ref_ptr<vsg::Node> processRawData(const vsg::Path filename, vsgPoints::Settings& settings)
{
    vsgPoints::Levels levels;
    levels.push_back(vsgPoints::Bricks::create());
    auto& first_level = levels.front();
    if (!readBricks(filename, settings, *first_level))
    {
        std::cout<<"Waring: unable to read file."<<std::endl;
        return {};
    }

    if (settings.bound.valid())
    {
        settings.offset = (settings.bound.max + settings.bound.min) * 0.5;
    }

    std::cout<<"After reading data "<<first_level->size()<<std::endl;

    size_t biggestBrick = 0;
    vsg::t_box<int32_t> keyBounds;
    for(auto& [key, brick] : *first_level)
    {
        keyBounds.add(key.x, key.y, key.z);
        if (brick->points.size() > biggestBrick) biggestBrick = brick->points.size();
    }

    while(levels.back()->size() > 1)
    {
        auto& source = levels.back();

        levels.push_back(vsgPoints::Bricks::create());
        auto& destination = levels.back();

        if (!generateLevel(*source, *destination, settings)) break;
    }

    std::cout<<"levels = "<<levels.size()<<std::endl;

    std::cout<<"keyBounds "<<keyBounds<<std::endl;
    std::cout<<"biggest brick "<<biggestBrick<<std::endl;

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

    vsgPoints::Settings settings;
    settings.numPointsPerBlock = arguments.value<size_t>(10000, "-b");
    settings.precision = arguments.value<double>(0.001, "-p");
    settings.transition = arguments.value<float>(0.125f, "-t");
    settings.pointSize = arguments.value<float>(4.0f, "--ps");
    settings.options = options;

    auto outputFilename = arguments.value<vsg::Path>("", "-o");

    settings.bits = arguments.value<uint32_t>(10, "--bits");
    if (settings.bits != 8 && settings.bits != 10 && settings.bits != 16)
    {
        std::cout<<"Error: "<<settings.bits<<" not supported, valid values are 8, 10 and 16."<<std::endl;
        return 1;
    }

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
