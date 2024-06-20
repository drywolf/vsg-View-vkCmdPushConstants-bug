// based on the code of:
// https://github.com/vsg-dev/vsgExamples/blob/9d3ae99f4362d271d3a16e3f4ad50a627ada6667/examples/app/vsgheadless/vsgheadless.cpp

#include <vsg/all.h>

#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

const char* const fullscreen_vertSrc = R"(
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec2 texcoords;

void main()
{
  // https://www.saschawillems.de/blog/2016/08/13/vulkan-tutorial-on-rendering-a-fullscreen-quad-without-buffers/
  texcoords = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  gl_Position = vec4(texcoords * 2.0f + -1.0f, 0.0f, 1.0f);
}
)";

const char* const fullscreen_Simple_fragSrc = R"(
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_color;

void main()
{
    out_color = vec4(1,0,0,1);
}
)";

const char* const fullscreen_PushConstants_fragSrc = R"(
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform _pc {
    vec4 drawColor;
} push_constants;

layout(location = 0) out vec4 out_color;

void main()
{
    out_color = push_constants.drawColor;
}
)";

vsg::ref_ptr<vsg::Device> initVSG(int argc, char** argv, int& out_queueFamily);
vsg::ref_ptr<vsg::ImageView> createColorImageView(vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent, VkFormat imageFormat, VkSampleCountFlagBits samples);
vsg::ref_ptr<vsg::RenderPass> createColorRenderPass(vsg::Device* device, VkFormat imageFormat);

vsg::ref_ptr<vsg::RenderGraph> scene_RenderPass(vsg::ref_ptr<vsg::Device> device, vsg::ref_ptr<vsg::View> view);
vsg::ref_ptr<vsg::RenderGraph> fullscreen_SimpleShader_Pass(vsg::ref_ptr<vsg::Device> device);
vsg::ref_ptr<vsg::RenderGraph> fullscreen_PushConstShader_Pass(vsg::ref_ptr<vsg::Device> device);

const VkExtent2D extent{ 2048, 1024 };
const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;

template<typename EnumT>
constexpr EnumT operator|(EnumT a, EnumT b)
{
    static_assert(std::is_enum_v<EnumT>);
    return static_cast<EnumT>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

int main(int argc, char** argv)
{
    int queueFamily = -1;
    vsg::ref_ptr<vsg::Device> device = initVSG(argc, argv, /*out*/ queueFamily);

    if (device == nullptr)
        return -1;

    auto commandGraph = vsg::CommandGraph::create(device, queueFamily);

    vsg::CommandGraphs commandGraphs;
    commandGraphs.push_back(commandGraph);

    // NOTE: our actual scene render-graph contains StateGroups & Draw-Calls, but for the bug repro-case it isn't necessary!
    auto camera_0 = vsg::Camera::create(vsg::Perspective::create(), vsg::LookAt::create(), vsg::ViewportState::create(extent));
    auto vsg_scene_0 = vsg::MatrixTransform::create(); // empty scene
    auto view_0 = vsg::View::create(camera_0, vsg_scene_0);

    // NOTE: our actual scene render-graph contains StateGroups & Draw-Calls, but for the bug repro-case it isn't necessary!
    auto camera_1 = vsg::Camera::create(vsg::Perspective::create(), vsg::LookAt::create(), vsg::ViewportState::create(extent));
    auto vsg_scene_1 = vsg::MatrixTransform::create(); // empty scene
    auto view_1 = vsg::View::create(camera_1, vsg_scene_1);

    // NOTE: there are 4 possible RenderPasses that can be drawn
    enum RenderPasses
    {
        draw_First3DScene           = (1 << 0),
        draw_Simple_Fullscreen      = (1 << 1),
        draw_Second3DScene          = (1 << 2),
        draw_PushConst_Fullscreen   = (1 << 3),
        draw_NONE                   = 0x00,
        draw_ALL                    = 0xFF,
    };

    struct BugScenario
    {
        const RenderPasses passes = draw_NONE;
      
        vsg::ref_ptr<vsg::View> scene_0_view; // vsg::View used for `draw_scene_0` RenderPass
        vsg::ref_ptr<vsg::View> scene_1_view; // vsg::View used for `draw_scene_1` RenderPass
    };

    const auto scenario =
#if true
    // VVL-01795 once and no crash
    BugScenario {
        draw_PushConst_Fullscreen,
        view_0, view_0 // same view for both scenes
    };
#elif false
    // VVL-01795 and crash in VVL vkCmdBeginRenderPass() hook
    BugScenario {
        draw_ALL,
        view_0, view_0 // same view for both scenes
    };
#elif false
    // VVL-01795 and crash in VVL vkCmdCopyBuffer() hook
    BugScenario {
        draw_First3DScene | draw_PushConst_Fullscreen,
        view_0, view_0 // same view for both scenes
    };
#elif false
    // crash in BindGraphicsPipeline::record() because the GraphicsPipeline::Implementation for viewID = 1 was not compiled
    BugScenario {
        draw_ALL,
        view_0, view_1 // NOTE: using distinct views
    };
#elif false
    // just for testing/experimentation
    BugScenario{
        draw_X,
        view_X, view_X
    };
#else
#   error "no BugScenario selected"
#endif

    if (scenario.passes == draw_NONE)
        throw std::exception(); // need to draw at least something

    // primary RenderGraph + 3D Scene
    if (scenario.passes & draw_First3DScene)
    {
        commandGraph->addChild(scene_RenderPass(device, scenario.scene_0_view));
    }

    // Simple Fullscreen Pass (no push constants)
    if (scenario.passes & draw_Simple_Fullscreen)
    {
        commandGraph->addChild(fullscreen_SimpleShader_Pass(device));
    }

    // another RenderGraph + 3D Scene
    if (scenario.passes & draw_Second3DScene)
    {
        commandGraph->addChild(scene_RenderPass(device, scenario.scene_1_view));
    }

    // Push-Const Fullscreen Pass
    if (scenario.passes & draw_PushConst_Fullscreen)
    {
        commandGraph->addChild(fullscreen_PushConstShader_Pass(device));
    }

    // create the viewer
    auto viewer = vsg::Viewer::create();
    viewer->assignRecordAndSubmitTaskAndPresentation(commandGraphs);

    // compile all Vulkan objects and transfer image, vertex and primitive data to GPU
    viewer->compile();

    auto start = std::chrono::high_resolution_clock::now();
    uint32_t rendered_frames = 0;

    // rendering main loop
    while (viewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();

        ++rendered_frames;

        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::microseconds duration_us = std::chrono::duration_cast<std::chrono::microseconds>(now - start);
        if (duration_us >= 1s)
        {
            double fps = rendered_frames / double(duration_us.count()) * 1000000.0;
            std::cout << "FPS: " << fps << std::endl;

            rendered_frames = 0;
            start = std::chrono::high_resolution_clock::now();
        }
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Passes -----------------------------------------------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------

vsg::ref_ptr<vsg::RenderGraph> scene_RenderPass(vsg::ref_ptr<vsg::Device> device, vsg::ref_ptr<vsg::View> view)
{
    vsg::ref_ptr<vsg::ImageView> colorImageView = createColorImageView(device, extent, imageFormat, VK_SAMPLE_COUNT_1_BIT);

    auto renderPass = createColorRenderPass(device, imageFormat);
    vsg::ref_ptr<vsg::Framebuffer> framebuffer = vsg::Framebuffer::create(renderPass, vsg::ImageViews{ colorImageView }, extent.width, extent.height, 1);

    auto renderGraph = vsg::RenderGraph::create();

    renderGraph->framebuffer = framebuffer;
    renderGraph->renderArea.offset = { 0, 0 };
    renderGraph->renderArea.extent = extent;
    renderGraph->setClearValues({ {1.0f, 1.0f, 0.0f, 0.0f} }, VkClearDepthStencilValue{ 0.0f, 0 });

    renderGraph->addChild(view);

    return renderGraph;
}

vsg::ref_ptr<vsg::RenderGraph> fullscreen_SimpleShader_Pass(vsg::ref_ptr<vsg::Device> device)
{
    vsg::ref_ptr<vsg::ImageView> colorImageView = createColorImageView(device, extent, imageFormat, VK_SAMPLE_COUNT_1_BIT);

    auto renderPass = createColorRenderPass(device, imageFormat);
    vsg::ref_ptr<vsg::Framebuffer> framebuffer = vsg::Framebuffer::create(renderPass, vsg::ImageViews{ colorImageView }, extent.width, extent.height, 1);

    auto renderGraph = vsg::RenderGraph::create();

    renderGraph->framebuffer = framebuffer;
    renderGraph->renderArea.offset = { 0, 0 };
    renderGraph->renderArea.extent = extent;
    renderGraph->setClearValues({ {1.0f, 1.0f, 0.0f, 0.0f} }, VkClearDepthStencilValue{ 0.0f, 0 });

    auto vertShaderModule = vsg::ShaderModule::create(fullscreen_vertSrc);
    auto fragShaderModule = vsg::ShaderModule::create(fullscreen_Simple_fragSrc);

    auto vertShaderStage = vsg::ShaderStage::create(VK_SHADER_STAGE_VERTEX_BIT, "main", vertShaderModule);
    auto fragShaderStage = vsg::ShaderStage::create(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderModule);

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{ vertShaderStage, fragShaderStage });

    auto config = vsg::GraphicsPipelineConfigurator::create(shaderSet);

    struct SetPipelineStates : public vsg::Visitor
    {
        void apply(vsg::Object& object) override { object.traverse(*this); }
        void apply(vsg::RasterizationState& rs) override { rs.cullMode = VK_CULL_MODE_NONE; }
        void apply(vsg::ColorBlendState& cbs) override { cbs.attachments[0].blendEnable = false; }
        void apply(vsg::DepthStencilState& ds) override { ds.depthTestEnable = false; }
    } sps;
    config->accept(sps);

    config->init();

    auto stateGroup = vsg::StateGroup::create();
    renderGraph->addChild(stateGroup);

    config->copyTo(stateGroup);

    stateGroup->setValue("debug_name", "fullscreen_Simple");
    config->graphicsPipeline->layout->setValue("debug_name", "fullscreen_Simple");

    auto vertexDraw = vsg::Draw::create(3, 1, 0, 0); // fullscreen triangle
    stateGroup->addChild(vertexDraw);

    return renderGraph;
}

vsg::ref_ptr<vsg::RenderGraph> fullscreen_PushConstShader_Pass(vsg::ref_ptr<vsg::Device> device)
{
    vsg::ref_ptr<vsg::ImageView> colorImageView = createColorImageView(device, extent, imageFormat, VK_SAMPLE_COUNT_1_BIT);

    auto renderPass = createColorRenderPass(device, imageFormat);
    vsg::ref_ptr<vsg::Framebuffer> framebuffer = vsg::Framebuffer::create(renderPass, vsg::ImageViews{ colorImageView }, extent.width, extent.height, 1);

    auto renderGraph = vsg::RenderGraph::create();

    renderGraph->framebuffer = framebuffer;
    renderGraph->renderArea.offset = { 0, 0 };
    renderGraph->renderArea.extent = extent;
    renderGraph->setClearValues({ {1.0f, 1.0f, 0.0f, 0.0f} }, VkClearDepthStencilValue{ 0.0f, 0 });

    auto vertShaderModule = vsg::ShaderModule::create(fullscreen_vertSrc);
    auto fragShaderModule = vsg::ShaderModule::create(fullscreen_PushConstants_fragSrc);

    auto vertShaderStage = vsg::ShaderStage::create(VK_SHADER_STAGE_VERTEX_BIT, "main", vertShaderModule);
    auto fragShaderStage = vsg::ShaderStage::create(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderModule);

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{ vertShaderStage, fragShaderStage });
    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(vsg::vec4));

    auto config = vsg::GraphicsPipelineConfigurator::create(shaderSet);

    struct SetPipelineStates : public vsg::Visitor
    {
        void apply(vsg::Object& object) override { object.traverse(*this); }
        void apply(vsg::RasterizationState& rs) override { rs.cullMode = VK_CULL_MODE_NONE; }
        void apply(vsg::ColorBlendState& cbs) override { cbs.attachments[0].blendEnable = false; }
        void apply(vsg::DepthStencilState& ds) override { ds.depthTestEnable = false; }
    } sps;
    config->accept(sps);

    config->init();

    auto stateGroup = vsg::StateGroup::create();
    renderGraph->addChild(stateGroup);

    config->copyTo(stateGroup);

    stateGroup->setValue("debug_name", "fullscreen_PushConstants");
    config->graphicsPipeline->layout->setValue("debug_name", "fullscreen_PushConstants");

    auto pushConst_drawColor_value = vsg::vec4Value::create(vsg::vec4{ 0, 1, 0, 1 });
    stateGroup->addChild(vsg::PushConstants::create(VK_SHADER_STAGE_FRAGMENT_BIT, 0, pushConst_drawColor_value));

    auto vertexDraw = vsg::Draw::create(3, 1, 0, 0); // fullscreen triangle
    stateGroup->addChild(vertexDraw);

    return renderGraph;
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------
// VSG Utils --------------------------------------------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------

vsg::ref_ptr<vsg::ImageView> createColorImageView(vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent, VkFormat imageFormat, VkSampleCountFlagBits samples)
{
    auto colorImage = vsg::Image::create();
    colorImage->imageType = VK_IMAGE_TYPE_2D;
    colorImage->format = imageFormat;
    colorImage->extent = VkExtent3D{ extent.width, extent.height, 1 };
    colorImage->mipLevels = 1;
    colorImage->arrayLayers = 1;
    colorImage->samples = samples;
    colorImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    colorImage->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    colorImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorImage->flags = 0;
    colorImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    return vsg::createImageView(device, colorImage, VK_IMAGE_ASPECT_COLOR_BIT);
}

vsg::ref_ptr<vsg::RenderPass> createColorRenderPass(vsg::Device* device, VkFormat imageFormat)
{
    vsg::AttachmentDescription colorAttachment = {};
    colorAttachment.format = imageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    vsg::RenderPass::Attachments attachments{colorAttachment};

    vsg::AttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    vsg::SubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachments.emplace_back(colorAttachmentRef);

    vsg::RenderPass::Subpasses subpasses{subpass};

    // image layout transition
    vsg::SubpassDependency colorDependency = {};
    colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    colorDependency.dstSubpass = 0;
    colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDependency.srcAccessMask = 0;
    colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    colorDependency.dependencyFlags = 0;

    vsg::RenderPass::Dependencies dependencies{colorDependency};

    return vsg::RenderPass::create(device, attachments, subpasses, dependencies);
}

vsg::ref_ptr<vsg::Device> initVSG(int argc, char** argv, int& out_queueFamily)
{
    vsg::CommandLine arguments(&argc, argv);
    auto debugLayer = true; // always use validation-layer for this bug repro code

    if (arguments.errors())
    {
        arguments.writeErrorMessages(std::cerr);
        return nullptr;
    }

    uint32_t vulkanVersion = VK_API_VERSION_1_3;

    // create instance
    vsg::Names instanceExtensions;
    vsg::Names requestedLayers;
    if (debugLayer)
    {
        instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        requestedLayers.push_back("VK_LAYER_KHRONOS_validation");
    }

    vsg::Names validatedNames = vsg::validateInstancelayerNames(requestedLayers);

    auto instance = vsg::Instance::create(instanceExtensions, validatedNames, vulkanVersion);
    auto [physicalDevice, queueFamily] = instance->getPhysicalDeviceAndQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    if (!physicalDevice || queueFamily < 0)
    {
        std::cout << "Could not create PhysicalDevice" << std::endl;
        return nullptr;
    }

    out_queueFamily = queueFamily;

    vsg::Names deviceExtensions;
    vsg::QueueSettings queueSettings{vsg::QueueSetting{queueFamily, { 1.0 }}};

    auto deviceFeatures = vsg::DeviceFeatures::create();
    deviceFeatures->get().samplerAnisotropy = VK_TRUE;

    auto device = vsg::Device::create(physicalDevice, queueSettings, validatedNames, deviceExtensions, deviceFeatures);
    return device;
}
