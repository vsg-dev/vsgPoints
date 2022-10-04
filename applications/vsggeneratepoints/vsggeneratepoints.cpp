#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <iostream>

#include "BrickBuilder.h"



vsg::ref_ptr<vsg::Node> create(vsg::ref_ptr<vsg::vec4Value> viewport, const vsg::dvec3& position, const vsg::dvec3& size, size_t numPoints, bool useBrickBuilder, bool perVertexNormals, bool perVertexColors, vsg::ref_ptr<const vsg::Options> options)
{
    bool usePositionScale = false;
    bool lighting = perVertexNormals;
    VkVertexInputRate normalInputRate = perVertexNormals ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
    VkVertexInputRate colorInputRate = perVertexColors ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;

    vsg::DataList arrays;

#if VERTEX_TYPE==4
    auto vertices = vsg::vec3Array::create(numPoints);
#else
    auto vertices = vsg::ubvec4Array::create(numPoints);
#endif

    arrays.push_back(vertices);

    auto normals = vsg::vec3Array::create(perVertexNormals ? numPoints : 1);
    arrays.push_back(normals);

    auto colors = vsg::ubvec4Array::create(perVertexColors ? numPoints : 1);
    arrays.push_back(colors);

    vsg::vec4 positionScale(0.0f, 0.0f, 0.0f, 1.0f);
    if (usePositionScale)
    {
        auto maxSize = std::max(std::max(size.x, size.y), size.z);
        positionScale.set(position.x, position.y, position.z, maxSize);
        arrays.push_back(vsg::vec4Value::create(positionScale));
    }
    else
    {
        arrays.push_back(vsg::vec4Value::create(vsg::vec4(0.0f, 0.0f, 0.0f, 1.0f)));
    }

    double area = size.x * size.y;
    double areaPerPoint = area / static_cast<double>(numPoints);
    double interval = sqrt(areaPerPoint);

    size_t numColumns = static_cast<size_t>(ceil(size.x / interval));
    size_t numRows = static_cast<size_t>(ceil(size.y / interval));

    vsg::vec3 origin(position);

    float dr = 1.0f / static_cast<float>(numRows-1);
    float dc = 1.0f / static_cast<float>(numColumns-1);

    auto computeZ = [&](float rr, float rc) -> float
    {
        return (sin(vsg::PIf * (1.0f+3.0f * rc + rr)) * 0.2f + 1.0f) * (size.z * 0.5f);
    };

    auto computePoint = [&](size_t c, size_t r) -> std::tuple<vsg::vec3, vsg::vec3, vsg::ubvec4>
    {
        float gradient_ratio = 0.01;
        float rc = static_cast<float>(c) * dc;
        float rr = static_cast<float>(r) * dr;
        float z = computeZ(rc, rr);
        float dz_dc = computeZ(rc + dc*gradient_ratio, rr) - z;
        float dz_dr = computeZ(rc, rr + dr*gradient_ratio) - z;

        vsg::vec3 vert(origin.x + interval * static_cast<float>(c), origin.y + interval * static_cast<float>(r), z);
        vsg::vec3 norm(-dz_dc / (gradient_ratio * interval), -dz_dr / (gradient_ratio * interval), 0.0f);
        norm.z = sqrt(1.0f - norm.x*norm.x - norm.y*norm.y);
        vsg::ubvec4 col(static_cast<uint8_t>(rc*255.0f), static_cast<uint8_t>(rr*255.0f), 255, 255);
        return {vert, norm, col};
    };


    size_t vi = 0;
    for(size_t r = 0; r < numRows; ++r)
    {
        for(size_t c = 0; (c < numColumns) && (vi < numPoints); ++c)
        {
            auto [vert, norm, col] = computePoint(c, r);
            if (usePositionScale)
            {
                vert.x = (vert.x - positionScale.x) / positionScale.w;
                vert.y = (vert.y - positionScale.y) / positionScale.w;
                vert.z = (vert.z - positionScale.z) / positionScale.w;
            }

#if VERTEX_TYPE==4
            vertices->set(vi, vert);
#else
            vertices->set(vi, vsg::ubvec4(static_cast<uint8_t>(vert.x*255.0f), static_cast<uint8_t>(vert.y*255.0f), static_cast<uint8_t>(vert.z*255.0f), 0));
#endif
            if (perVertexNormals) normals->set(vi, norm);
            if (perVertexColors) colors->set(vi, col);
            ++vi;
        }
    }

    if (!perVertexNormals) normals->set(0, vsg::vec3(0.0f, 0.0f, 1.0f));
    if (!perVertexColors) colors->set(0, vsg::ubvec4(255, 255, 255, 255));

    if (arrays.empty()) return {};

    if (useBrickBuilder)
    {
        auto brickBuilder = vsgPoints::BrickBuilder::create();
        brickBuilder->options = options;
        brickBuilder->viewport = viewport;
        brickBuilder->add(vertices, normals, colors);

        return brickBuilder->build();
    }

    auto pointSize = vsg::vec2Value::create();
    pointSize->value().set(interval*3.0f, interval);

    vsg::info("pointsSize = ", pointSize->value());
    vsg::info("viewport = ", viewport->value());

    auto bindVertexBuffers = vsg::BindVertexBuffers::create();
    bindVertexBuffers->assignArrays(arrays);

    auto commands = vsg::Commands::create();
    commands->addChild(bindVertexBuffers);
    commands->addChild(vsg::Draw::create(vertices->size(), 1, 0, 0));

    auto sg = vsgPoints::createStateGroup(options, viewport, pointSize, lighting, normalInputRate, colorInputRate);

    if (!sg) return commands;

    sg->addChild(commands);

    return sg;
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->sharedObjects = vsg::SharedObjects::create();

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsggeneratepoints";

    auto builder = vsg::Builder::create();
    builder->options = options;

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});

    vsg::dvec3 position(0.0, 0.0, 0.0);
    vsg::dvec3 size(256.0, 256.0, 256.0);

    arguments.read("--screen", windowTraits->screenNum);
    arguments.read("--display", windowTraits->display);
    auto numFrames = arguments.value(-1, "-f");
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (arguments.read("--double-buffer")) windowTraits->swapchainPreferences.imageCount = 2;
    if (arguments.read("--triple-buffer")) windowTraits->swapchainPreferences.imageCount = 3; // default
    if (arguments.read("-t"))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->width = 192, windowTraits->height = 108;
        windowTraits->decoration = false;
    }

    auto outputFilename = arguments.value<std::string>("", "-o");

    arguments.read({"-p", "--position"}, position);
    arguments.read({"-s", "--size"}, size);
    size_t numPoints = arguments.value<size_t>(1000000, "-n");
    bool useBrickBuilder  = arguments.read("--brick");
    bool colours = arguments.read({"-c", "--colors"});
    bool normals = arguments.read("--normals");
    auto viewportData = vsg::vec4Value::create(0.0f, 0.0f, 1920.0f, 1080.0f);
#if (VSG_VERSION_MAJOR >= 1) || (VSG_VERSION_MINOR >= 6) || ((VSG_VERSION_MINOR == 5) && (VSG_VERSION_PATCH >= 7))
    viewportData->getLayout().dynamic = true;
#endif

    auto scene = create(viewportData, position, size, numPoints, useBrickBuilder, normals, colours, options);

    if (!scene)
    {
        std::cout << "No scene graph created." << std::endl;
        return 1;
    }

    // write out scene if required
    if (!outputFilename.empty())
    {
        vsg::write(scene, outputFilename, options);
        return 0;
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create windows." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // compute the bounds of the scene graph to help position camera
#if 0
    vsg::ComputeBounds computeBounds;
    scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6 * 3.0;
#endif

    vsg::dvec3 centre = position + size * 0.5;
    double radius = vsg::length(size)*0.5;

    std::cout << "window size windowTraits width= " << windowTraits->width << ", "<<windowTraits->height<<std::endl;
    std::cout << "centre = " << centre << std::endl;
    std::cout << "radius = " << radius << std::endl;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    double nearFarRatio = 0.001;
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 10.0);

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    viewer->addEventHandler(vsg::Trackball::create(camera));

    auto commandGraph = vsg::createCommandGraphForView(window, camera, scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    auto& viewport = camera->viewportState->getViewport();
    viewportData->value().set(viewport.x, viewport.y, viewport.width, viewport.height);

    viewer->compile();

    auto startTime = vsg::clock::now();
    double numFramesCompleted = 0.0;

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        if (viewportData->value()[2] != viewport.width || viewportData->value()[3] != viewport.height)
        {
            viewportData->value().set(viewport.x, viewport.y, viewport.width, viewport.height);
            viewportData->dirty();
        }

        viewer->recordAndSubmit();

        viewer->present();

        numFramesCompleted += 1.0;
    }

    auto duration = std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - startTime).count();
    if (numFramesCompleted > 0.0)
    {
        std::cout << "Average frame rate = " << (numFramesCompleted / duration) << std::endl;
    }

    return 0;
}
