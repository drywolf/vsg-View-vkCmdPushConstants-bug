# About

This repository contains a minimal bug repro-case for Vulkan-Validation-Layer error [VUID-vkCmdPushConstants-offset-01795](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCmdPushConstants.html#VUID-vkCmdPushConstants-offset-01795) happening in VulkanSceneGraph [vsg/vk/State.h (line 151)](https://github.com/vsg-dev/VulkanSceneGraph/blob/971e16413bd79bb74040563b167ee6009cd9106e/include/vsg/vk/State.h#L150-L152)

<u>State.h code:</u>

```
    mat4 newmatrix(matrixStack.top());
    vkCmdPushConstants(commandBuffer, pipeline, stageFlags, offset, sizeof(newmatrix), newmatrix.data());
    dirty = false;
```

<u>Vulkan Validation-Layer errors:</u>

```
VUID-vkCmdPushConstants-offset-01795(ERROR / SPEC): msgNum: 666667206 - Validation Error: [ VUID-vkCmdPushConstants-offset-01795 ] Object 0: handle = 0x1ea94cfd920, type = VK_OBJECT_TYPE_COMMAND_BUFFER; | MessageID = 0x27bc88c6 | vkCmdPushConstants(): VK_SHADER_STAGE_FRAGMENT_BIT, VkPushConstantRange in VkPipelineLayout 0x84c0580000000017[] overlapping offset = 0 and size = 64, do not contain VK_SHADER_STAGE_FRAGMENT_BIT. The Vulkan spec states: For each byte in the range specified by offset and size and for each shader stage in stageFlags, there must be a push constant range in layout that includes that byte and that stage (https://vulkan.lunarg.com/doc/view/1.3.250.0/windows/1.3-extensions/vkspec.html#VUID-vkCmdPushConstants-offset-01795)
    Objects: 1
        [0] 0x1ea94cfd920, type: 6, name: NULL
VUID-vkCmdPushConstants-offset-01795(ERROR / SPEC): msgNum: 666667206 - Validation Error: [ VUID-vkCmdPushConstants-offset-01795 ] Object 0: handle = 0x1ea94cfd920, type = VK_OBJECT_TYPE_COMMAND_BUFFER; | MessageID = 0x27bc88c6 | vkCmdPushConstants(): VK_SHADER_STAGE_FRAGMENT_BIT, VkPushConstantRange in VkPipelineLayout 0x84c0580000000017[] overlapping offset = 64 and size = 64, do not contain VK_SHADER_STAGE_FRAGMENT_BIT. The Vulkan spec states: For each byte in the range specified by offset and size and for each shader stage in stageFlags, there must be a push constant range in layout that includes that byte and that stage (https://vulkan.lunarg.com/doc/view/1.3.250.0/windows/1.3-extensions/vkspec.html#VUID-vkCmdPushConstants-offset-01795)
    Objects: 1
        [0] 0x1ea94cfd920, type: 6, name: NULL
```

# Bug repro code
<sup>(vsg-View-PushConst-bug.cpp)</sup>

The code can be used to reproduce two issues:
* the `VUID-vkCmdPushConstants-offset-01795` error being caused by `vkCmdPushConstants()` in State.h
    * see BugScenario <u>**"VVL-01795 once and no crash"**</u> in vsg-View-PushConst-bug.cpp
* vsg::GraphicsPipelineState not being properly compiled when two distinct `vsg::Views` are used for rendering
    * i.e. `viewer->compile();` only compiles the GraphicsPipeline for the first view (`viewID=0`) but not for the secondary view (`viewID=1`)
    * see BugScenario <u>**"crash in BindGraphicsPipeline::record()"**</u> in vsg-View-PushConst-bug.cpp

# Initial Setup
<sup>(should only be needed once)</sup>

```
git submodule update --init --recursive
.\vcpkg\bootstrap-vcpkg.bat
configure.bat
```

# Building the applications

```
build.bat
OR
cmake --build build --config Debug
```

# Running the applications

```
cd build\Debug
vsg-View-PushConst-bug.exe
```
