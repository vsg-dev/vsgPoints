#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <vsgPoints/BIN.h>
#include <vsgPoints/AsciiPoints.h>
#include <vsgPoints/Brick.h>

#include "ConvertMeshToPoints.h"

#include <iostream>

vsg::ref_ptr<vsg::Node> createPagedLODSceneGraph(vsg::ref_ptr<vsgPoints::Bricks> bricks, vsg::ref_ptr<vsgPoints::Settings> settings)
{
    if (bricks->empty())
    {
        std::cout<<"createPagedLODSceneGraph("<<bricks<<", "<<settings<<") bricks is empty(), cannot create scene graph."<<std::endl;
        return {};
    }

    vsgPoints::Levels levels;
    levels.push_back(bricks);

    if (settings->bound.valid())
    {
        settings->offset = (settings->bound.max + settings->bound.min) * 0.5;
    }

    while(levels.back()->size() > 1)
    {
        auto& source = levels.back();

        levels.push_back(vsgPoints::Bricks::create());
        auto& destination = levels.back();

        if (!vsgPoints::generateLevel(*source, *destination, *settings)) break;
    }

    std::cout<<"levels = "<<levels.size()<<std::endl;

    size_t biggestBrick = 0;
    vsg::t_box<int32_t> keyBounds;
    for(auto& [key, brick] : *bricks)
    {
        keyBounds.add(key.x, key.y, key.z);
        if (brick->points.size() > biggestBrick) biggestBrick = brick->points.size();
    }
    std::cout<<"keyBounds "<<keyBounds<<std::endl;
    std::cout<<"biggest brick "<<biggestBrick<<std::endl;

    auto transform = vsg::MatrixTransform::create();
    transform->matrix = vsg::translate(settings->offset);

    if (auto model = createPagedLOD(levels, *settings))
    {
        transform->addChild(model);
    }

    return transform;
}

vsg::ref_ptr<vsg::Node> createFlatSceneGraph(vsg::ref_ptr<vsgPoints::Bricks> bricks, vsg::ref_ptr<vsgPoints::Settings> settings)
{
    if (bricks->empty())
    {
        std::cout<<"createPagedLODSceneGraph("<<bricks<<", "<<settings<<") bricks is empty(), cannot create scene graph."<<std::endl;
        return {};
    }

    if (settings->bound.valid())
    {
        settings->offset = (settings->bound.max + settings->bound.min) * 0.5;
    }

    auto cullGroup = vsg::CullGroup::create();
    cullGroup->bound.center = (settings->bound.max + settings->bound.min) * 0.5;
    cullGroup->bound.radius = vsg::length(settings->bound.max - settings->bound.min) * 0.5;

    auto transform = vsg::MatrixTransform::create();
    transform->matrix = vsg::translate(settings->offset);
    cullGroup->addChild(transform);

    auto group = vsgPoints::createStateGroup(*settings);
    transform->addChild(group);

    vsg::dbox bound;
    for(auto& [key, brick] : *bricks)
    {
        if (auto node = brick->createRendering(*(bricks->settings), key, bound))
        {
            group->addChild(node);
        }
    }

    return cullGroup;
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->sharedObjects = vsg::SharedObjects::create();

    options->add(vsgPoints::BIN::create());
    options->add(vsgPoints::AsciiPoints::create());

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    arguments.read(options);

    if (int type; arguments.read("--allocator", type)) vsg::Allocator::instance()->allocatorType = vsg::AllocatorType(type);
    if (size_t objectsBlockSize; arguments.read("--objects", objectsBlockSize)) vsg::Allocator::instance()->setBlockSize(vsg::ALLOCATOR_AFFINITY_OBJECTS, objectsBlockSize);
    if (size_t nodesBlockSize; arguments.read("--nodes", nodesBlockSize)) vsg::Allocator::instance()->setBlockSize(vsg::ALLOCATOR_AFFINITY_NODES, nodesBlockSize);
    if (size_t dataBlockSize; arguments.read("--data", dataBlockSize)) vsg::Allocator::instance()->setBlockSize(vsg::ALLOCATOR_AFFINITY_DATA, dataBlockSize);

    // set up the Settings used to guide how point clouds are setup in the scene graph
    auto settings = vsgPoints::Settings::create();
    options->setObject("settings", settings);
    settings->options = vsg::Options::create(*options);

    arguments.read("-b", settings->numPointsPerBlock);
    arguments.read("-p", settings->precision);
    arguments.read("-t", settings->transition);
    arguments.read("--ps", settings->pointSize);
    arguments.read("--bits", settings->bits);
    bool flat = arguments.read("--flat");
    bool convert_mesh = arguments.read("--mesh");
    bool add_model = !arguments.read("--no-model");

    bool writeOnly = false;
    auto outputFilename = arguments.value<vsg::Path>("", "-o");
    if (outputFilename)
    {
        settings->path =  vsg::filePath(outputFilename)/vsg::simpleFilename(outputFilename);
        settings->extension = vsg::fileExtension(outputFilename);
        writeOnly = !arguments.read({"-v", "--viewer"});
    }

    auto group = vsg::Group::create();

    vsg::Path filename;
    while(arguments.read("-m", filename))
    {
    }

    while(arguments.read("-i", filename))
    {
        auto object = vsg::read(filename, options);
        if (auto node = object.cast<vsg::Node>())
        {
            if (convert_mesh)
            {
                ConvertMeshToPoints convert(settings);
                node->accept(convert);
                auto bricks = convert.bricks;

                auto scene = flat ?
                    createFlatSceneGraph(bricks, settings) :
                    createPagedLODSceneGraph(bricks, settings);

                if (scene) group->addChild(scene);
            }

            if (add_model)
            {
                group->addChild(node);
            }
        }
        else if (auto bricks = object.cast<vsgPoints::Bricks>())
        {
            auto scene = flat ?
                createFlatSceneGraph(bricks, settings) :
                createPagedLODSceneGraph(bricks, settings);

            if (scene) group->addChild(scene);
        }
    }

    if (group->children.empty())
    {
        std::cout<<"Error: no data loaded."<<std::endl;
        return 1;
    }

    vsg::ref_ptr<vsg::Node> vsg_scene;
    if (group->children.size()==1) vsg_scene = group->children[0];
    else vsg_scene = group;

    if (outputFilename)
    {
        vsg::write(vsg_scene, outputFilename, options);

        std::cout<<"Written scene graph to "<<outputFilename<<std::endl;

        if (writeOnly) return 0;
    }

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});

    auto viewer = vsg::Viewer::create();
    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create windows." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // compute the bounds of the scene graph to help position camera
    vsg::dbox bounds = vsg::visit<vsg::ComputeBounds>(vsg_scene).bounds;

    vsg::dvec3 center = (bounds.min + bounds.max) * 0.5;
    double radius = vsg::length(bounds.max - bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    auto lookAt = vsg::LookAt::create(center + vsg::dvec3(0.0, -radius * 3.5, 0.0), center, vsg::dvec3(0.0, 0.0, 1.0));
    auto perspective  = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));
    viewer->addEventHandler(vsg::Trackball::create(camera));

    auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
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

    return 0;
}
