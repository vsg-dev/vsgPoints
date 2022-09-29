#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <iostream>

struct FormatLayout
{
    int vertex = 0;
    int normal = -1;
    int rgb = -1;
    int rgba = -1;
};

vsg::ref_ptr<vsg::Data> createParticleImage(uint32_t dim)
{
    auto data = vsg::ubvec4Array2D::create(dim, dim);
    data->getLayout().format = VK_FORMAT_R8G8B8A8_UNORM;
    float div = 2.0f / static_cast<float>(dim - 1);
    float distance_at_one = 0.5f;
    float distance_at_zero = 1.0f;

    vsg::vec2 v;
    for (uint32_t r = 0; r < dim; ++r)
    {
        v.y = static_cast<float>(r) * div - 1.0f;
        for (uint32_t c = 0; c < dim; ++c)
        {
            v.x = static_cast<float>(c) * div - 1.0f;
            float distance_from_center = vsg::length(v);
            float intensity = 1.0f - (distance_from_center - distance_at_one) / (distance_at_zero - distance_at_one);
            if (intensity > 1.0f) intensity = 1.0f;
            if (intensity < 0.0f) intensity = 0.0f;
            uint8_t alpha = static_cast<uint8_t>(intensity * 255);
            data->set(c, r, vsg::ubvec4(255, 255, 255, alpha));
        }
    }
    return data;
}

vsg::ref_ptr<vsg::StateGroup> createStateGroup(vsg::ref_ptr<const vsg::Options> options)
{
    bool lighting = true;
    VkVertexInputRate normalInputRate = VK_VERTEX_INPUT_RATE_VERTEX; // VK_VERTEX_INPUT_RATE_INSTANCE
    VkVertexInputRate colorInputRate = VK_VERTEX_INPUT_RATE_VERTEX;  // VK_VERTEX_INPUT_RATE_INSTANCE

    for (auto& path : options->paths)
    {
        std::cout << "path = " << path << std::endl;
    }

    // load shaders
    auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/pointsprites.vert", options);
    //if (!vertexShader) vertexShader = assimp_vert(); // fallback to shaders/assimp_vert.cpp

    vsg::ref_ptr<vsg::ShaderStage> fragmentShader;
    if (lighting)
    {
        fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/assimp_phong.frag", options);
        //if (!fragmentShader) fragmentShader = assimp_phong_frag();
    }
    else
    {
        fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/assimp_flat_shaded.frag", options);
        //if (!fragmentShader) fragmentShader = assimp_flat_shaded_frag();
    }

    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        std::cout << "vertexShader = " << vertexShader << std::endl;
        std::cout << "fragmentShader = " << fragmentShader << std::endl;

        return {};
    }

    vsg::info("vertexShader = ", vertexShader);
    vsg::info("fragmentShader = ", fragmentShader);

    auto shaderHints = vsg::ShaderCompileSettings::create();
    auto& defines = shaderHints->defines;

    vertexShader->module->hints = shaderHints;
    vertexShader->module->code = {};

    fragmentShader->module->hints = shaderHints;
    fragmentShader->module->code = {};

    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings;

    // enable the point sprite code paths
    defines.push_back("VSG_POINT_SPRITE");

    vsg::ref_ptr<vsg::Data> textureData;
    //textureData = vsg::read_cast<vsg::Data>("textures/lz.vsgb", options);
    textureData = createParticleImage(64);
    if (textureData)
    {
        std::cout << "textureData = " << textureData << std::endl;

        // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
        descriptorBindings.push_back(VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
        defines.push_back("VSG_DIFFUSE_MAP");
    }

    {
        descriptorBindings.push_back(VkDescriptorSetLayoutBinding{10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}); // Material
    }

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::DescriptorSetLayouts descriptorSetLayouts{descriptorSetLayout};

    if (lighting)
    {
        auto viewDescriptorSetLayout = vsg::ViewDescriptorSetLayout::create();
        descriptorSetLayouts.push_back(viewDescriptorSetLayout);
    }

    vsg::PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls automatically provided by the VSG's DispatchTraversal
    };

    auto pipelineLayout = vsg::PipelineLayout::create(descriptorSetLayouts, pushConstantRanges);

    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
    };

    vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{1, sizeof(vsg::vec3), normalInputRate});  // normal data
    vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}); // normal data

    vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{2, 4, colorInputRate});                 // color data
    vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R8G8B8A8_UNORM, 0}); // color data

    auto rasterState = vsg::RasterizationState::create();

    bool blending = true;
    auto colorBlendState = vsg::ColorBlendState::create();
    colorBlendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{
        {blending, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_SUBTRACT, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT}};

    auto inputAssemblyState = vsg::InputAssemblyState::create();
    inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    vsg::GraphicsPipelineStates pipelineStates{
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        inputAssemblyState,
        rasterState,
        vsg::MultisampleState::create(),
        colorBlendState,
        vsg::DepthStencilState::create()};

    auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);

    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    // create texture image and associated DescriptorSets and binding

    vsg::Descriptors descriptors;
    if (textureData)
    {
        auto sampler = vsg::Sampler::create();
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        auto texture = vsg::DescriptorImage::create(sampler, textureData, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        descriptors.push_back(texture);
    }

    auto mat = vsg::PhongMaterialValue::create();
    mat->value().alphaMask = 1.0f;
    mat->value().alphaMaskCutoff = 0.5f;

    auto material = vsg::DescriptorBuffer::create(mat, 10);
    descriptors.push_back(material);

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, descriptors);
    auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, vsg::DescriptorSets{descriptorSet});

    auto sg = vsg::StateGroup::create();
    sg->add(bindGraphicsPipeline);
    sg->add(bindDescriptorSets);

    if (lighting)
    {
        sg->add(vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1));
    }

    return sg;
}

vsg::ref_ptr<vsg::Node> create(const vsg::vec3& position, const vsg::vec3& size, size_t numPoints, bool perVertexNormals, bool perVertexColors, vsg::ref_ptr<const vsg::Options> options)
{
    vsg::DataList arrays;

    auto vertices = vsg::vec3Array::create(numPoints);
    arrays.push_back(vertices);

    auto normals = vsg::vec3Array::create(perVertexNormals ? numPoints : 1);
    arrays.push_back(normals);

    auto colors = vsg::ubvec4Array::create(perVertexColors ? numPoints : 1);
    arrays.push_back(colors);

    double area = size.x * size.y;
    double areaPerPoint = area / static_cast<double>(numPoints);
    double interval = sqrt(areaPerPoint);

    size_t numColumns = static_cast<size_t>(ceil(size.x / interval));
    size_t numRows = static_cast<size_t>(ceil(size.y / interval));

    size_t vi = 0;
    for(size_t r = 0; r < numRows; ++r)
    {
        for(size_t c = 0; (c < numColumns) && (vi < numPoints); ++c)
        {
            float z = 0.0;
            vsg::vec3 vert(position.x + interval * static_cast<float>(c), position.y + interval * static_cast<float>(r), z);
            vsg::vec3 norm(0.0f, 0.0f, 1.0f);
            vsg::ubvec4 col(128, 255, 255, 255);
            vertices->set(vi, vert);
            if (perVertexNormals) normals->set(vi, norm);
            if (perVertexColors) colors->set(vi, col);
            ++vi;
        }
    }

    if (!perVertexNormals) normals->set(0, vsg::vec3(0.0f, 0.0f, 1.0f));
    if (!perVertexColors) colors->set(0, vsg::ubvec4(255, 128, 255, 255));

    if (arrays.empty()) return {};

    auto bindVertexBuffers = vsg::BindVertexBuffers::create();
    bindVertexBuffers->assignArrays(arrays);

    auto commands = vsg::Commands::create();
    commands->addChild(bindVertexBuffers);
    commands->addChild(vsg::Draw::create(vertices->size(), 1, 0, 0));

    auto sg = createStateGroup(options);

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
    windowTraits->windowTitle = "vsgpoints";

    auto builder = vsg::Builder::create();
    builder->options = options;

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});

    vsg::vec3 position(0.0, 0.0, 0.0);
    vsg::vec3 size(10.0, 10.0, 10.0);

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
    bool colours = arguments.read({"-c", "--colors"});
    bool normals = arguments.read("--normals");

    auto scene = create(position, size, numPoints, normals, colours, options);

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
    vsg::ComputeBounds computeBounds;
    scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6 * 3.0;

    std::cout << "centre = " << centre << std::endl;
    std::cout << "radius = " << radius << std::endl;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    double nearFarRatio = 0.001;
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 10.0);

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // set up the compilation support in builder to allow us to interactively create and compile subgraphs from within the IntersectionHandler
    // builder->setup(window, camera->viewportState);

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    viewer->addEventHandler(vsg::Trackball::create(camera));

    auto commandGraph = vsg::createCommandGraphForView(window, camera, scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    viewer->compile();

    auto startTime = vsg::clock::now();
    double numFramesCompleted = 0.0;

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

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
