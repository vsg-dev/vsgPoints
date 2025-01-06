#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <vsgPoints/BIN.h>
#include <vsgPoints/AsciiPoints.h>
#include <vsgPoints/create.h>
#include <vsgPoints/utils/PointsPolytopeIntersector.h>
#include <vsgPoints/utils/PolygonIntersector.h>

#include <iomanip>
#include <iostream>
#include <random>

#define MAX_POLYGON_VERTICES 1000

vsg::ref_ptr<vsg::StateGroup> create_wirefame_stateGroup()
{
    std::string VERT{R"(
    #version 450
    layout(push_constant) uniform PushConstants { mat4 projection; mat4 modelView; };
    layout(location = 0) in vec3 vertex;
    out gl_PerVertex { vec4 gl_Position; };
    void main() { gl_Position = (projection * modelView) * vec4(vertex, 1.0); }
    )"};

    std::string FRAG{R"(
    #version 450
    layout(location = 0) out vec4 color;
    void main() { color = vec4(1, 1, 1, 1); }
    )"};

    auto vertexShader = vsg::ShaderStage::create(VK_SHADER_STAGE_VERTEX_BIT, "main", VERT);
    auto fragmentShader = vsg::ShaderStage::create(VK_SHADER_STAGE_FRAGMENT_BIT, "main", FRAG);
    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});
    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);
    shaderSet->addAttributeBinding("vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));

    auto stateGroup = vsg::StateGroup::create();
    auto gpConf = vsg::GraphicsPipelineConfigurator::create(shaderSet);

    gpConf->enableArray("vertex", VK_VERTEX_INPUT_RATE_VERTEX, sizeof(vsg::vec3), VK_FORMAT_R32G32B32_SFLOAT);

    struct SetPipelineStates : public vsg::Visitor
    {
        void apply(vsg::Object& object) { object.traverse(*this); }
        void apply(vsg::RasterizationState& rs)
        {
            rs.lineWidth = 1.0f;
            rs.cullMode = VK_CULL_MODE_NONE;
        }
        void apply(vsg::InputAssemblyState& ias)
        {
            ias.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        }
    } sps;

    gpConf->accept(sps);
    gpConf->init();
    gpConf->copyTo(stateGroup);
    return stateGroup;
}


class ResizeHandler : public vsg::Inherit<vsg::Visitor, ResizeHandler>
{
public:
    vsg::ref_ptr<vsg::Camera> camera;

    ResizeHandler(vsg::ref_ptr<vsg::Camera> in_camera)
        : camera(in_camera) {}

    void apply(vsg::ConfigureWindowEvent& configWindowEvent) override
    {
        auto extents = configWindowEvent.window.ref_ptr()->extent2D();
        auto projection = vsg::Orthographic::create(0, extents.width, extents.height, 0, 100.0, 0.0);
        auto lookAt = vsg::LookAt::create(vsg::dvec3(0.0, 0.0, 80.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 1.0, 0.0));
        camera->projectionMatrix = projection;
        camera->viewMatrix = lookAt;
    }
};


class SelectionHandler : public vsg::Inherit<vsg::Visitor, SelectionHandler>
{
public:

    enum SelectionMode
    {
        NONE,
        POLYGON,
        SQUARE
    };

    vsg::ref_ptr<vsg::Camera> camera;
    vsg::ref_ptr<vsg::Group> scenegraph;
    vsg::ref_ptr<vsg::Switch> polygon_switch;
    vsg::ref_ptr<vsg::Switch> square_switch;
    vsg::ref_ptr<vsg::vec3Array> poly_vertices;
    vsg::ref_ptr<vsg::VertexDraw> poly_vertexDraw;
    vsg::ref_ptr<vsg::MatrixTransform> square_transform;
    vsg::ref_ptr<vsg::PointerEvent> lastPointerEvent;

    SelectionMode selection_mode;
    float square_scale;
    bool dynamic_colours;

    // Find the color node and change its color
    struct ResetColor : public vsg::Visitor
    {
        void apply(vsg::Object& object) override
        {
            object.traverse(*this);
        }

        void apply(vsg::VertexDraw& vd) override
        {
            for (auto& buffer : vd.arrays)
            {
                auto data = buffer->data;
                if (data->is_compatible(typeid(vsg::ubvec4Array)))
                {
                    auto colors = data.cast<vsg::ubvec4Array>();
                    for (int i = 0; i < colors->size(); i++)
                        colors->at(i) = vsg::ubvec4(0, 255, 0, 255);

                    colors->dirty();
                }
            }
        }
    };

    SelectionHandler(vsg::ref_ptr<vsg::Camera> in_camera, vsg::ref_ptr<vsg::Group> in_scenegraph, bool in_dynamic_colours) :
        camera(in_camera), scenegraph(in_scenegraph), selection_mode(NONE), square_scale(40.f), dynamic_colours(in_dynamic_colours)
    {
        auto stateGroup = create_wirefame_stateGroup();
        scenegraph->addChild(stateGroup);

        // Polygon selection vertices
        poly_vertices = vsg::vec3Array::create(MAX_POLYGON_VERTICES);
        poly_vertices->properties.dataVariance = vsg::DYNAMIC_DATA;
        poly_vertexDraw = vsg::VertexDraw::create();
        poly_vertexDraw->assignArrays({poly_vertices});
        poly_vertexDraw->vertexCount = 0;
        poly_vertexDraw->instanceCount = 1;
        polygon_switch = vsg::Switch::create();
        polygon_switch->addChild(false, poly_vertexDraw);
        stateGroup->addChild(polygon_switch);

        // Square selection
        auto square_vertices = vsg::vec3Array::create({{-1.0f, -1.0f, 0.f}, {1.0f, -1.0f, 0.f}, {1.0f, 1.0f, 0.f}, {-1.0, 1.0, 0.f}, { -1.0f, -1.0, 0.f}});
        auto square_vertexDraw = vsg::VertexDraw::create();
        square_vertexDraw->assignArrays({square_vertices});
        square_vertexDraw->vertexCount = square_vertices->width();
        square_vertexDraw->instanceCount = 1;
        square_vertexDraw->vertexCount = 5;

        square_transform = vsg::MatrixTransform::create();
        square_transform->matrix = vsg::scale(square_scale);
        square_transform->addChild(square_vertexDraw);
        square_switch = vsg::Switch::create();
        square_switch->addChild(false, square_transform);
        stateGroup->addChild(square_switch);
    }

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        auto previous_mode = selection_mode;

        if (keyPress.keyBase == vsg::KEY_1)
        {
            selection_mode = selection_mode == SQUARE ? NONE : SQUARE;
            square_switch->setAllChildren(selection_mode == SQUARE ? true : false);
        }
        else if (keyPress.keyBase == vsg::KEY_2)
        {
            if (selection_mode == NONE && polygon_switch->children[0].mask == vsg::boolToMask(true))
            {
                poly_vertexDraw->vertexCount = 0;
                poly_vertices->dirty();
                polygon_switch->setAllChildren(false);
            }
            else
                selection_mode = selection_mode == POLYGON ? NONE : POLYGON;
        }
        else if (keyPress.keyBase == vsg::KEY_o)
        {
            if (polygon_switch->children[0].mask == vsg::boolToMask(true))
            {
                std::vector<vsg::dvec2> polygon(poly_vertexDraw->vertexCount);
                for (uint32_t i = 0; i < poly_vertexDraw->vertexCount; i++)
                    polygon[i] = vsg::dvec2(poly_vertices->at(i).x, poly_vertices->at(i).y);

                polygonIntersect(polygon, true);
            }
        }
        else if (keyPress.keyBase == vsg::KEY_r)
        {
            if (dynamic_colours)
            {
                ResetColor rc;
                scenegraph->accept(rc);
            }
        } 

        if (previous_mode != selection_mode && selection_mode != NONE)
        {
            polygon_switch->setAllChildren(selection_mode == POLYGON ? true : false);
            square_switch->setAllChildren(selection_mode == SQUARE ? true : false);
        }
    }

    void apply(vsg::MoveEvent& moveEvent) override
    {
        lastPointerEvent = &moveEvent;
        if (selection_mode == POLYGON)
        {
            if (poly_vertexDraw->vertexCount > 2)
            {
                poly_vertices->at(poly_vertexDraw->vertexCount - 2) = vsg::vec3(static_cast<float>(moveEvent.x), static_cast<float>(moveEvent.y), 0.f);
                poly_vertices->dirty();
                moveEvent.handled = true;
            }
        }
        square_transform->matrix = vsg::translate(vsg::vec3(static_cast<float>(moveEvent.x), static_cast<float>(moveEvent.y), 0.f)) * vsg::scale(square_scale);
    }

   void apply(vsg::ScrollWheelEvent& scrollWheel) override
    {
        if (selection_mode == SQUARE)
        {
            scrollWheel.handled = true;
            float scale = square_scale * (1.f + scrollWheel.delta.y * 0.1f);
            if (scale < 2.0 || scale > 300.0)
                return;

            square_scale = scale;
            square_transform->matrix = vsg::translate(vsg::vec3(static_cast<float>(lastPointerEvent->x), static_cast<float>(lastPointerEvent->y), 0.f)) * vsg::scale(square_scale);
        }
    }

    void apply(vsg::ButtonPressEvent& buttonPress) override
    {
        if (buttonPress.button == 1)
        {
            switch (selection_mode)
            {
            case NONE:
                if (poly_vertexDraw->vertexCount > 0)
                {
                    polygon_switch->setAllChildren(false);
                    poly_vertexDraw->vertexCount = 0;
                    poly_vertices->dirty();
                }
                break;

            case POLYGON:
                if (poly_vertexDraw->vertexCount == 0)
                {
                    poly_vertices->set(0, vsg::vec3(static_cast<float>(buttonPress.x), static_cast<float>(buttonPress.y), 0.f));
                    poly_vertices->set(1, vsg::vec3(static_cast<float>(buttonPress.x), static_cast<float>(buttonPress.y), 0.f));
                    poly_vertices->set(2, vsg::vec3(static_cast<float>(buttonPress.x), static_cast<float>(buttonPress.y), 0.f));
                    poly_vertexDraw->vertexCount = 3;
                    poly_vertices->dirty();
                }
                else
                {
                    poly_vertexDraw->vertexCount++;
                    poly_vertices->at(poly_vertexDraw->vertexCount - 2) = vsg::vec3(static_cast<float>(buttonPress.x), static_cast<float>(buttonPress.y), 0.f);
                    poly_vertices->at(poly_vertexDraw->vertexCount - 1) = poly_vertices->at(0);
                    poly_vertices->dirty();
                }
                break;
            }
        }
        else if (buttonPress.button == 3)
        {
           if (square_switch->children[0].mask == vsg::boolToMask(true))
            {
                auto m = square_transform->matrix;
                auto minPt = m * vsg::dvec3(-1.0f, -1.0f, 0.f);
                auto maxPt = m * vsg::dvec3(1.0f, 1.0f, 0.f);

                polytopeIntersect(std::round(minPt.x), std::round(minPt.y), std::round(maxPt.x), std::round(maxPt.y));
                buttonPress.handled = true;
            }
            else if (polygon_switch->children[0].mask == vsg::boolToMask(true))
            {
                std::vector<vsg::dvec2> polygon(poly_vertexDraw->vertexCount);
                for (uint32_t i = 0; i < poly_vertexDraw->vertexCount; i++)
                    polygon[i] = vsg::dvec2(poly_vertices->at(i).x, poly_vertices->at(i).y);

                polygonIntersect(polygon);

                buttonPress.handled = true;
            }
            else if (selection_mode == NONE)
            {
                cursorIntersect(static_cast<double>(buttonPress.x), static_cast<double>(buttonPress.y));

                buttonPress.handled = true;
            }
        }
    }

    void cursorIntersect(double x, double y)
    {
        double size = 5;
        auto intersector = vsgPoints::PointsPolytopeIntersector::create(*camera, x - size, y - size, x + size, y + size);
        auto before_t = vsg::clock::now();
        scenegraph->accept(*intersector);
        double duration = std::chrono::duration<double, std::chrono::milliseconds::period>(vsg::clock::now() - before_t).count();

        if (intersector->intersections.size() > 0)
            std::cout <<std::fixed<<std::setprecision(4)<< "Picked point: " << intersector->intersections[0]->worldIntersection << std::endl;
    }

    void polytopeIntersect(double minX, double minY, double maxX, double maxY)
    {
        auto intersector = vsgPoints::PointsPolytopeIntersector::create(*camera, minX, minY, maxX, maxY);

        auto before_t = vsg::clock::now();
        scenegraph->accept(*intersector);
        double duration = std::chrono::duration<double, std::chrono::milliseconds::period>(vsg::clock::now() - before_t).count();

        if (intersector->intersections.size() > 0)
            std::cout << std::fixed << std::setprecision(4) << "Polytope Intersected: " << intersector->intersections.size() << " points in " << duration << "ms"<<
            ", First: " << intersector->intersections[0]->worldIntersection << std::endl;
        else
            std::cout << "Polytope Intersected: " << intersector->intersections.size() << " points in " << duration << "ms" << std::endl;

        if (dynamic_colours)
        {
            for (auto& intersection : intersector->intersections)
            {
                intersection->arrays[2]->cast<vsg::ubvec4Array>()->set(intersection->indices[0], vsg::ubvec4(255, 0, 0, 255));
                intersection->arrays[2]->dirty();
            }
        }
    }

    void polygonIntersect(std::vector<vsg::dvec2> polygon, bool invert_selection = false)
    {
        auto intersector = vsgPoints::PolygonIntersector::create(*camera, polygon);
        intersector->invert_selection = invert_selection;
        auto before_t = vsg::clock::now();
        scenegraph->accept(*intersector);
        double duration = std::chrono::duration<double, std::chrono::milliseconds::period>(vsg::clock::now() - before_t).count();

        if (intersector->intersectionCount() > 0)
        {
            auto worldPos = intersector->intersections[0]->worldPos(intersector->intersections[0]->indices[0]);
            std::cout << std::fixed << std::setprecision(4) << "Polygon Intersected: " << intersector->intersectionCount() << " points in " << duration << "ms"
                << ", First: " << worldPos << std::endl;
        }
        else
            std::cout << "Polygon Intersected: " << intersector->intersectionCount() << " points in " << duration << "ms" << std::endl;

        if (dynamic_colours)
        {
            for (auto& intersection : intersector->intersections)
            {
                for (uint32_t i = 0; i < intersection->indices.size(); i++)
                {
                    intersection->arrays[2]->cast<vsg::ubvec4Array>()->set(intersection->indices[i], vsg::ubvec4(255, 0, 0, 255));
                    intersection->arrays[2]->dirty();
                }
            }
        }
    }
};


vsg::ref_ptr<vsg::Group> create_scene(vsg::ref_ptr<vsgPoints::Settings> settings, vsg::dvec3 offset = vsg::dvec3(0.0, 0.0, 0.0))
{
    auto group = vsg::Group::create();
    auto bricks = vsgPoints::Bricks::create(settings);
    double seperation = 20.0 * settings->precision;
    auto brick_size = settings->precision * pow(2.0, settings->bits);
    double separation = brick_size / 100.0;

    for (int x = -500; x < 500; ++x)
    {
        for (int y = -500; y < 500; ++y)
        {
            double z = (static_cast<double>(rand()%100)/10000.0);
            bricks->add(vsg::dvec3(offset.x + x * seperation, offset.y + y * seperation, offset.z + z), vsg::ubvec4(0, 255, 0, 255));
        }
    }

    group->addChild(vsgPoints::createSceneGraph(bricks, settings));
    return group;
}


int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

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

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgpoints";
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    if (arguments.read("--test"))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->fullscreen = true;
    }
    if (arguments.read({"--st", "--small-test"}))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->width = 192, windowTraits->height = 108;
        windowTraits->decoration = false;
    }
    auto maxPagedLOD = arguments.value(0, "--maxPagedLOD");

    // set up the Settings used to guide how point clouds are setup in the scene graph
    auto settings = vsgPoints::Settings::create();
    options->setObject("settings", settings);
    settings->options = vsg::Options::create(*options);

    auto vsg_scene = vsg::Group::create();

    if (arguments.argc() > 1)
    {
        vsg::Path filename = arguments[1];
        auto scene = vsg::read_cast<vsg::Node>(filename, options);
        if (scene)
            vsg_scene->addChild(scene);
    }

    bool dynamic_colours = false;

    if (vsg_scene->children.empty())
    {
        settings->bits = 10;
        settings->createType = vsgPoints::CREATE_FLAT;
        settings->precision = 0.001;
        vsg_scene = create_scene(settings);

        // Set color arrays as dynamic
        struct SetDynamicColor : public vsg::Visitor
        {
            void apply(vsg::Object& object) override
            {
                object.traverse(*this);
            }

            void apply(vsg::VertexDraw& vd) override
            {
                for (auto& buffer : vd.arrays)
                {
                    auto data = buffer->data;
                    if (data->is_compatible(typeid(vsg::ubvec4Array)))
                        data->properties.dataVariance = vsg::DYNAMIC_DATA;
                }
            }
        };
        SetDynamicColor dc;
        vsg_scene->accept(dc);
        dynamic_colours = true;
    }

    if (vsg_scene->children.empty())
    {
        std::cout<<"Error: no data loaded."<<std::endl;
        return 1;
    }

    auto viewer = vsg::Viewer::create();
    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create window." << std::endl;
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

    auto view = vsg::View::create(camera);
    view->addChild(vsg::createHeadlight());
    view->addChild(vsg_scene);

    // overlay camera
    auto overlay_projection = vsg::Orthographic::create(0, window->extent2D().width, window->extent2D().height, 0, 100.0, 0.0);
    auto overlay_lookAt = vsg::LookAt::create(vsg::dvec3(0.0, 0.0, 80.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 1.0, 0.0));
    auto overlay_camera = vsg::Camera::create(overlay_projection, overlay_lookAt, vsg::ViewportState::create(window->extent2D()));
    auto overlay_scene = vsg::Group::create();
    auto overlay_view = vsg::View::create(overlay_camera, overlay_scene);
    overlay_view->addChild(overlay_scene);

    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    auto selectionHandler = SelectionHandler::create(camera, vsg_scene, dynamic_colours); 
    overlay_scene->addChild(selectionHandler->polygon_switch);
    overlay_scene->addChild(selectionHandler->square_switch);
    viewer->addEventHandler(selectionHandler);

    auto resize_handler = ResizeHandler::create(overlay_camera);
    viewer->addEventHandler(resize_handler);

    viewer->addEventHandler(vsg::Trackball::create(camera));

    auto commandGraph = vsg::CommandGraph::create(window);
    auto renderGraph = vsg::RenderGraph::create(window);
    commandGraph->addChild(renderGraph);

    renderGraph->addChild(view);
    renderGraph->addChild(overlay_view);

    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    viewer->compile();

    if (maxPagedLOD > 0)
    {
        // set targetMaxNumPagedLODWithHighResSubgraphs after Viewer::compile() as it will assign any DatabasePager if required.
        for(auto& task : viewer->recordAndSubmitTasks)
        {
            if (task->databasePager) task->databasePager->targetMaxNumPagedLODWithHighResSubgraphs = maxPagedLOD;
        }
    }

    viewer->start_point() = vsg::clock::now();

    // rendering main loop
    while (viewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    auto fs = viewer->getFrameStamp();
    double fps = static_cast<double>(fs->frameCount) / std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - viewer->start_point()).count();
    std::cout<<"Average frame rate = "<<fps<<" fps"<<std::endl;

    return 0;
}
