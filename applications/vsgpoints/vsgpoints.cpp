#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <vsgPoints/BIN.h>
#include <vsgPoints/AsciiPoints.h>
#include <vsgPoints/create.h>

#include "ConvertMeshToPoints.h"

#include <iostream>


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
    bool convert_mesh = arguments.read("--mesh");
    bool add_model = !arguments.read("--no-model");

    if (arguments.read("--plod")) settings->createType = vsgPoints::CREATE_PAGEDLOD;
    else if (arguments.read("--lod")) settings->createType = vsgPoints::CREATE_LOD;
    else if (arguments.read("--flat")) settings->createType = vsgPoints::CREATE_FLAT;

    bool writeOnly = false;
    auto outputFilename = arguments.value<vsg::Path>("", "-o");
    if (outputFilename)
    {
        settings->path =  vsg::filePath(outputFilename)/vsg::simpleFilename(outputFilename);
        settings->extension = vsg::fileExtension(outputFilename);
        writeOnly = !arguments.read({"-v", "--viewer"});
    }
    else
    {
        if (settings->createType == vsgPoints::CREATE_PAGEDLOD)
        {
            std::cout<<"PagedLOD generation not possible without output filename. Please specify ouput filename using: -o filename.vsgb"<<std::endl;
            return 1;
        }
    }

    auto group = vsg::Group::create();

    for (int i = 1; i < argc; ++i)
    {
        vsg::Path filename = arguments[i];

        auto object = vsg::read(filename, options);
        if (auto node = object.cast<vsg::Node>())
        {
            if (convert_mesh)
            {
                ConvertMeshToPoints convert(settings);
                node->accept(convert);
                auto bricks = convert.bricks;

                if (auto scene = vsgPoints::createSceneGraph(bricks, settings))
                {
                    group->addChild(scene);
                }
            }

            if (add_model)
            {
                group->addChild(node);
            }
        }
        else if (auto bricks = object.cast<vsgPoints::Bricks>())
        {
            if (auto scene = vsgPoints::createSceneGraph(bricks, settings))
            {
                group->addChild(scene);
            }
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
