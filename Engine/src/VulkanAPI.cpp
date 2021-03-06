#include "VulkanAPI.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stb_image.h>

#include <chrono>

#include "App.h"
#include "Components.h"
#include "Renderer.h"

namespace Ash {

VkCommandBuffer VulkanAPI::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanAPI::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    } else {
        ASH_ERROR("Couldn't load function vkDestroyDebugUtilsMessengerEXT");
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallBack(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
              void* pUserData) {
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::string type;
        switch (messageType) {
            case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
                type = "General";
                break;
            case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
                type = "Specification Violation";
                break;
            case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
                type = "Performance";
                break;
            default:
                type = "Other";
                break;
        }
        ASH_ERROR("{} Type Validation Layer: {} ", type,
                  pCallbackData->pMessage);
        if (pUserData) {
            ASH_WARN("User data is provided");
        }
    }
    return VK_FALSE;
}

VulkanAPI::VulkanAPI() {}

VulkanAPI::~VulkanAPI() {}

VulkanAPI::QueueFamilyIndices VulkanAPI::findQueueFamilies(
    VkPhysicalDevice device) {
    VulkanAPI::QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                             nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                             queueFamilies.data());

    uint32_t i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface,
                                             &presentSupport);

        if (presentSupport) {
            indices.presentsFamily = i;
        }

        if (indices.isComplete()) break;

        i++;
    }

    return indices;
}

VulkanAPI::SwapchainSupportDetails VulkanAPI::querySwapchainSupport(
    VkPhysicalDevice device) {
    SwapchainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                              &details.capabilities);
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                         nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                             details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
                                              &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR VulkanAPI::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    // Consider ranking formats and choosing best option if desired format isn't
    // found
    return availableFormats[0];
}

VkPresentModeKHR VulkanAPI::chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        // vsync : VK_PRESENT_MODE_MAILBOX_KHR
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            return availablePresentMode;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanAPI::chooseSwapExtent(
    const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(App::getWindow()->get(), &width, &height);

        VkExtent2D actualExtent = {static_cast<uint32_t>(width),
                                   static_cast<uint32_t>(height)};

        actualExtent.width =
            std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                       capabilities.maxImageExtent.width);
        actualExtent.height =
            std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                       capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallBack;
}

int rateDeviceSuitability(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    int score = 0;

    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    score += deviceProperties.limits.maxImageDimension2D;

    return score;
}

bool VulkanAPI::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionsCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionsCount,
                                         nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionsCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionsCount,
                                         availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(),
                                             deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

bool VulkanAPI::isDeviceSuitable(VkPhysicalDevice device) {
    VulkanAPI::QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapchainSupportDetails swapChainSupport =
            querySwapchainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() &&
                            !swapChainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return indices.isComplete() && extensionsSupported && swapChainAdequate &&
           supportedFeatures.samplerAnisotropy;
}

void VulkanAPI::pickPhysicalDevice() {
    physicalDevice = VK_NULL_HANDLE;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    ASH_ASSERT(deviceCount != 0, "No devices with Vulkan support found");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    std::multimap<int, VkPhysicalDevice> candidates;

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            physicalDevice = device;
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(device, &properties);
            ASH_INFO("Selecting {} as the physical device",
                     properties.deviceName);
            break;
        }
    }

    ASH_ASSERT(physicalDevice != VK_NULL_HANDLE, "No suitable device found");
}

void VulkanAPI::createInstance() {
    if (enableValidationLayers && !checkValidationSupport())
        ASH_ERROR("Enabled validation layers, but not supported");

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Game";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.pEngineName = "Ash";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.apiVersion = VULKAN_VERSION;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions,
                                        glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        createInfo.enabledExtensionCount =
            static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        ASH_INFO("Enabling validation layers");
    } else {
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;
    }

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (enableValidationLayers) {
        createInfo.enabledLayerCount =
            static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext =
            (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    ASH_ASSERT(vkCreateInstance(&createInfo, nullptr, &instance) == VK_SUCCESS,
               "Failed to initialize vulkan");

    ASH_INFO("Initialized Vulkan instance");
}

void VulkanAPI::setupDebugMessenger() {
    if (!enableValidationLayers) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    ASH_ASSERT(CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr,
                                            &debugMessenger) == VK_SUCCESS,
               "Failed to setup Debug Messenger");
}

void VulkanAPI::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                              indices.presentsFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount =
        static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount =
        static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount =
            static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    ASH_ASSERT(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) ==
                   VK_SUCCESS,
               "Couldn't create logical device");

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentsFamily.value(), 0, &presentQueue);
}

void VulkanAPI::createAllocator() {
    VmaAllocatorCreateInfo allocInfo{};
    allocInfo.vulkanApiVersion = VULKAN_VERSION;
    allocInfo.device = device;
    allocInfo.physicalDevice = physicalDevice;
    allocInfo.instance = instance;

    ASH_ASSERT(vmaCreateAllocator(&allocInfo, &allocator) == VK_SUCCESS,
               "Failed to create allocator");
}

void VulkanAPI::createSwapchain() {
    ASH_INFO("Creating swapchain");
    SwapchainSupportDetails swapchainSupport =
        querySwapchainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat =
        chooseSwapSurfaceFormat(swapchainSupport.formats);
    VkPresentModeKHR presentMode =
        chooseSwapPresentMode(swapchainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapchainSupport.capabilities);

    uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
    if (swapchainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapchainSupport.capabilities.maxImageCount) {
        imageCount = swapchainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndicies[] = {indices.graphicsFamily.value(),
                                      indices.presentsFamily.value()};
    if (indices.graphicsFamily != indices.presentsFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndicies;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    ASH_ASSERT(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) ==
                   VK_SUCCESS,
               "Failed to create swapchain");

    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount,
                            swapchainImages.data());

    swapchainImageFormat = surfaceFormat.format;
    swapchainExtent = extent;
}

void VulkanAPI::createImageViews() {
    ASH_INFO("Creating image views");
    swapchainImageViews.resize(swapchainImages.size());
    for (size_t i = 0; i < swapchainImages.size(); i++) {
        swapchainImageViews[i] =
            createImageView(swapchainImages[i], swapchainImageFormat,
                            VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void VulkanAPI::createRenderPass() {
    ASH_INFO("Creating render pass");

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment,
                                                          depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    ASH_ASSERT(vkCreateRenderPass(device, &renderPassInfo, nullptr,
                                  &renderPass) == VK_SUCCESS,
               "Failed to create render pass");
}

void VulkanAPI::createDescriptorSetLayout() {
    ASH_INFO("Creating descriptor set layout");
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 1> bindings = {uboLayoutBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    ASH_ASSERT(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                           &descriptorSetLayout) == VK_SUCCESS,
               "Failed to create descriptor set layout");

    bindings[0] = samplerLayoutBinding;

    ASH_ASSERT(
        vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                    &imageDescriptorSetLayout) == VK_SUCCESS,
        "Failed to create descriptor set layout");
}

void VulkanAPI::createPipelineCache() {
    ASH_INFO("Creating pipeline cache");
    VkPipelineCacheCreateInfo cacheInfo{};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    ASH_ASSERT(vkCreatePipelineCache(device, &cacheInfo, nullptr,
                                     &pipelineCache) == VK_SUCCESS,
               "Failed to create pipeline cache");
}

void VulkanAPI::createGraphicsPipelines(
    const std::vector<Pipeline>& pipelines) {
    ASH_INFO("Creating graphics pipelines");

    pipelineObjects = pipelines;

    std::vector<char> vert =
        Helper::readBinaryFile("assets/shaders/shader.vert.spv");
    std::vector<char> frag =
        Helper::readBinaryFile("assets/shaders/shader.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vert);
    VkShaderModule fragShaderModule = createShaderModule(frag);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";
    vertShaderStageInfo.pSpecializationInfo = nullptr;

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";
    fragShaderStageInfo.pSpecializationInfo = nullptr;

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                      fragShaderStageInfo};

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescription();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                      VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts = {
        descriptorSetLayout, imageDescriptorSetLayout};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount =
        static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();

    ASH_ASSERT(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                                      &pipelineLayout) == VK_SUCCESS,
               "Failed to create pipeline layout");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;

    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;

    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;

    pipelineInfo.layout = pipelineLayout;

    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    ASH_ASSERT(vkCreateGraphicsPipelines(
                   device, pipelineCache, 1, &pipelineInfo, nullptr,
                   &graphicsPipelines["main"]) == VK_SUCCESS,
               "Failed to create graphics pipeline");

    pipelineInfo.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
    pipelineInfo.basePipelineHandle = graphicsPipelines["main"];
    pipelineInfo.basePipelineIndex = -1;

    size_t j = 0;
    for (const Pipeline& pipeline : pipelines) {
        std::vector<VkPipelineShaderStageCreateInfo> shaderStageInfos;
        std::vector<VkShaderModule> shaderModules;
        for (size_t i = 0; i < pipeline.stages.size(); i++) {
            std::vector<char> code =
                Helper::readBinaryFile(pipeline.paths[i].c_str());
            shaderModules.push_back(createShaderModule(code));

            VkPipelineShaderStageCreateInfo shaderStageInfo{};
            shaderStageInfo.sType =
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

            switch (pipeline.stages[i]) {
                case ShaderStages::VERTEX_SHADER_STAGE:
                    shaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
                    break;
                case ShaderStages::FRAGMENT_SHADER_STAGE:
                    shaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                    break;
            }

            shaderStageInfo.module = shaderModules.back();
            shaderStageInfo.pName = "main";
            shaderStageInfo.pSpecializationInfo = nullptr;
            shaderStageInfos.push_back(shaderStageInfo);
        }

        pipelineInfo.stageCount = static_cast<uint32_t>(pipeline.stages.size());
        pipelineInfo.pStages = shaderStageInfos.data();

        ASH_ASSERT(vkCreateGraphicsPipelines(
                       device, pipelineCache, 1, &pipelineInfo, nullptr,
                       &graphicsPipelines[pipeline.name]) == VK_SUCCESS,
                   "Failed to create user pipeline");

        for (auto& module : shaderModules)
            vkDestroyShaderModule(device, module, nullptr);

        j++;
    }

    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
}

void VulkanAPI::createFramebuffers() {
    ASH_INFO("Creating framebuffers");

    swapchainFramebuffers.resize(swapchainImageViews.size());

    for (size_t i = 0; i < swapchainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {swapchainImageViews[i],
                                                  depthImageView};

        VkFramebufferCreateInfo frameBufferInfo{};
        frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferInfo.renderPass = renderPass;
        frameBufferInfo.attachmentCount =
            static_cast<uint32_t>(attachments.size());
        frameBufferInfo.pAttachments = attachments.data();
        frameBufferInfo.width = swapchainExtent.width;
        frameBufferInfo.height = swapchainExtent.height;
        frameBufferInfo.layers = 1;

        ASH_ASSERT(vkCreateFramebuffer(device, &frameBufferInfo, nullptr,
                                       &swapchainFramebuffers[i]) == VK_SUCCESS,
                   "Failed to create framebuffer {}", i);
    }
}

size_t calculateDynamicAllignment(size_t minUniformBufferAllignment,
                                  size_t dynamicAllignment) {
    if (minUniformBufferAllignment > 0)
        dynamicAllignment =
            (dynamicAllignment + minUniformBufferAllignment - 1) &
            ~(minUniformBufferAllignment - 1);
    return dynamicAllignment;
}

void VulkanAPI::createUniformBuffers() {
    ASH_INFO("Creating uniform buffers");

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    size_t minUniformBufferAllignment =
        properties.limits.minUniformBufferOffsetAlignment;
    size_t dynamicAllignment = sizeof(UniformBufferObject);

    dynamicAllignment = calculateDynamicAllignment(minUniformBufferAllignment,
                                                   dynamicAllignment);

    VkDeviceSize bufferSize = dynamicAllignment * MAX_INSTANCES;

    uniformBuffers.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainImages.size(); i++) {
        createBuffer(bufferSize, VMA_MEMORY_USAGE_CPU_TO_GPU,
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     uniformBuffers[i].uniformBuffer,
                     uniformBuffers[i].uniformBufferAllocation);
    }
}

void VulkanAPI::createDescriptorPool(uint32_t maxSets) {
    ASH_INFO("Creating descriptor pool");

    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    poolSizes[0].descriptorCount =
        static_cast<uint32_t>(swapchainImages.size() * maxSets);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount =
        static_cast<uint32_t>(swapchainImages.size());

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(swapchainImages.size() * maxSets);

    ASH_ASSERT(vkCreateDescriptorPool(device, &poolInfo, nullptr,
                                      &descriptorPool) == VK_SUCCESS,
               "Failed to create descriptor pool");
}

void VulkanAPI::createDescriptorSets(std::vector<VkDescriptorSet>& sets,
                                     const Texture& texture) {
    ASH_INFO("Creating descriptor sets");

    std::vector<VkDescriptorSetLayout> layouts(swapchainImages.size(),
                                               descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount =
        static_cast<uint32_t>(swapchainImages.size());
    allocInfo.pSetLayouts = layouts.data();

    sets.resize(swapchainImages.size());
    ASH_ASSERT(
        vkAllocateDescriptorSets(device, &allocInfo, sets.data()) == VK_SUCCESS,
        "Failed to allocate descriptor sets");

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    for (size_t i = 0; i < swapchainImages.size(); i++) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = texture.imageView;
        imageInfo.sampler = textureSampler;

        std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = sets[i];
        descriptorWrites[0].dstBinding = 1;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device,
                               static_cast<uint32_t>(descriptorWrites.size()),
                               descriptorWrites.data(), 0, nullptr);
    }
}

void VulkanAPI::createDescriptorSets() {
    ASH_INFO("Creating descriptor sets");

    if (uboDescriptorSets.size() == 0) {
        std::vector<VkDescriptorSetLayout> layouts(swapchainImages.size(),
                                                   descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount =
            static_cast<uint32_t>(swapchainImages.size());
        allocInfo.pSetLayouts = layouts.data();

        uboDescriptorSets.resize(swapchainImages.size());
        ASH_ASSERT(
            vkAllocateDescriptorSets(device, &allocInfo,
                                     uboDescriptorSets.data()) == VK_SUCCESS,
            "Failed to allocate descriptor sets");
    }

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    size_t minUniformBufferAllignment =
        properties.limits.minUniformBufferOffsetAlignment;
    size_t dynamicAllignment = sizeof(UniformBufferObject);

    dynamicAllignment = calculateDynamicAllignment(minUniformBufferAllignment,
                                                   dynamicAllignment);

    for (size_t i = 0; i < swapchainImages.size(); i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i].uniformBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range = dynamicAllignment;

        //        VkDescriptorImageInfo imageInfo{};
        //        imageInfo.imageLayout =
        //        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; imageInfo.imageView
        //        = texture.imageView; imageInfo.sampler = textureSampler;

        std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = uboDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType =
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        //        descriptorWrites[1].sType =
        //        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        //        descriptorWrites[1].dstSet = descriptorSets[i];
        //        descriptorWrites[1].dstBinding = 1;
        //        descriptorWrites[1].dstArrayElement = 0;
        //        descriptorWrites[1].descriptorType =
        //            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        //        descriptorWrites[1].descriptorCount = 1;
        //        descriptorWrites[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device,
                               static_cast<uint32_t>(descriptorWrites.size()),
                               descriptorWrites.data(), 0, nullptr);
    }
}

void VulkanAPI::createCommandPools() {
    ASH_INFO("Creating command pool");
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    poolInfo.flags = 0;

    ASH_ASSERT(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) ==
                   VK_SUCCESS,
               "Failed to create command pool");

    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    ASH_ASSERT(vkCreateCommandPool(device, &poolInfo, nullptr,
                                   &transferCommandPool) == VK_SUCCESS,
               "Failed to create command pool");
}

uint32_t VulkanAPI::findMemoryType(uint32_t typeFilter,
                                   VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (typeFilter & (1 << i) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) ==
                properties)
            return i;
    }

    ASH_ASSERT(false, "Failed to find suitable memory type");
    return -1;
}

void VulkanAPI::createCommandBuffers() {
    ASH_INFO("Creating command buffers");

    commandBuffers.resize(swapchainFramebuffers.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    ASH_ASSERT(vkAllocateCommandBuffers(device, &allocInfo,
                                        commandBuffers.data()) == VK_SUCCESS,
               "Failed to allocate command buffers");

    recordCommandBuffers();
}

void VulkanAPI::recordCommandBuffers() {
    for (size_t i = 0; i < commandBuffers.size(); i++) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = nullptr;

        ASH_ASSERT(
            vkBeginCommandBuffer(commandBuffers[i], &beginInfo) == VK_SUCCESS,
            "Failed to begin command buffer {}", i);

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapchainFramebuffers[i];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapchainExtent;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {
            {clearColor.r, clearColor.g, clearColor.b, clearColor.a}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount =
            static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo,
                             VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapchainExtent.width;
        viewport.height = (float)swapchainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapchainExtent;

        vkCmdSetViewport(commandBuffers[i], 0, 1, &viewport);
        vkCmdSetScissor(commandBuffers[i], 0, 1, &scissor);

        VkDeviceSize offsets[] = {0};

        std::shared_ptr<Scene> scene = Renderer::getScene();
        if (scene) {
            auto renderables = scene->registry.view<Renderable>();

            uint32_t h = 0;
            for (auto entity : renderables) {
                auto& renderable = renderables.get(entity);

                Model& model = Renderer::getModel(renderable.model);
                for (uint32_t j = 0; j < model.meshes.size(); j++) {
                    Mesh& mesh = Renderer::getMesh(model.meshes[j]);
                    VkBuffer vb[] = {mesh.ivb.buffer};

                    // Each model should have their own pipeline
                    vkCmdBindPipeline(commandBuffers[i],
                                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      graphicsPipelines[renderable.pipeline]);

                    // Each model has their own mesh and thus their own vertex
                    // and index buffers
                    vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vb,
                                           offsets);

                    vkCmdBindIndexBuffer(commandBuffers[i], mesh.ivb.buffer,
                                         mesh.ivb.vertSize,
                                         VK_INDEX_TYPE_UINT32);

                    VkPhysicalDeviceProperties properties;
                    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

                    size_t minUniformBufferAllignment =
                        properties.limits.minUniformBufferOffsetAlignment;
                    size_t dynamicAllignment = sizeof(UniformBufferObject);

                    dynamicAllignment = calculateDynamicAllignment(
                        minUniformBufferAllignment, dynamicAllignment);

                    uint32_t dynamicOffset =
                        h * static_cast<uint32_t>(dynamicAllignment);

                    // Each entity has their own transform and thus their
                    // own UBO transform matrix
                    vkCmdBindDescriptorSets(commandBuffers[i],
                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            pipelineLayout, 0, 1,
                                            &uboDescriptorSets[i], 0, nullptr);

                    vkCmdBindDescriptorSets(
                        commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                        pipelineLayout, 0, 1, &renderable.descriptorSets[j][i],
                        1, &dynamicOffset);

                    vkCmdDrawIndexed(commandBuffers[i], mesh.ivb.numIndices, 1,
                                     0, 0, 0);
                }
                h++;
            }
        }

        vkCmdEndRenderPass(commandBuffers[i]);

        ASH_ASSERT(vkEndCommandBuffer(commandBuffers[i]) == VK_SUCCESS,
                   "Failed to record command buffer {}", i);
    }
}

void VulkanAPI::createSyncObjects() {
    ASH_INFO("Creating synchronization objects");

    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(swapchainImages.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        ASH_ASSERT(
            vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                              &imageAvailableSemaphores[i]) == VK_SUCCESS,
            "Failed to create semaphore");
        ASH_ASSERT(
            vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                              &renderFinishedSemaphores[i]) == VK_SUCCESS,
            "Failed to create semaphore");
        ASH_ASSERT(vkCreateFence(device, &fenceInfo, nullptr,
                                 &inFlightFences[i]) == VK_SUCCESS,
                   "Failed to create fence");
    }

    ASH_ASSERT(vkCreateFence(device, &fenceInfo, nullptr, &copyFinishedFence) ==
                   VK_SUCCESS,
               "Failed to create fence");
}

void VulkanAPI::cleanupSwapchain() {
    vkDestroyImageView(device, depthImageView, nullptr);
    vmaDestroyImage(allocator, depthImage, depthImageAllocation);

    for (auto framebuffer : swapchainFramebuffers)
        vkDestroyFramebuffer(device, framebuffer, nullptr);

    vkFreeCommandBuffers(device, commandPool,
                         static_cast<uint32_t>(commandBuffers.size()),
                         commandBuffers.data());

    vkDestroyRenderPass(device, renderPass, nullptr);

    for (auto imageView : swapchainImageViews)
        vkDestroyImageView(device, imageView, nullptr);

    vkDestroySwapchainKHR(device, swapchain, nullptr);

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
}

void VulkanAPI::recreateSwapchain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(App::getWindow()->get(), &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(App::getWindow()->get(), &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);

    ASH_INFO("Recreating swapchain");

    cleanupSwapchain();

    createSwapchain();
    createImageViews();
    createRenderPass();
    createDepthResources();
    createFramebuffers();
    createDescriptorPool(MAX_INSTANCES);

    std::shared_ptr<Scene> scene = Renderer::getScene();
    auto renderables = scene->registry.view<Renderable>();
    for (auto entity : renderables) {
        auto& renderable = renderables.get(entity);
        for (uint32_t i = 0;
             i < Renderer::getModel(renderable.model).meshes.size(); i++) {
            createDescriptorSets(
                renderable.descriptorSets[i],
                Renderer::getTexture(
                    Renderer::getModel(renderable.model).textures[i]));
        }
    }

    createCommandBuffers();
}

void VulkanAPI::createBuffer(VkDeviceSize size, VmaMemoryUsage memUsage,
                             VkBufferUsageFlags usage, VkBuffer& buffer,
                             VmaAllocation& allocation) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = memUsage;

    ASH_ASSERT(vmaCreateBuffer(allocator, &bufferInfo, &allocationInfo, &buffer,
                               &allocation, nullptr) == VK_SUCCESS,
               "Failed to create buffer and allocation");
}

void VulkanAPI::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer,
                           VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

void VulkanAPI::copyBufferToImage(VkBuffer buffer, VkImage image,
                                  uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

VkShaderModule VulkanAPI::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module;
    ASH_ASSERT(vkCreateShaderModule(device, &createInfo, nullptr, &module) ==
                   VK_SUCCESS,
               "Failed to create shader module");

    return module;
}

void VulkanAPI::createImage(uint32_t width, uint32_t height,
                            VmaMemoryUsage memUsage, VkFormat format,
                            VkImageTiling tiling, VkImageUsageFlags usage,
                            VkImage& image, VmaAllocation& allocation) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = static_cast<uint32_t>(width);
    imageInfo.extent.height = static_cast<uint32_t>(height);
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = memUsage;

    ASH_ASSERT(vmaCreateImage(allocator, &imageInfo, &allocCreateInfo, &image,
                              &allocation, nullptr) == VK_SUCCESS,
               "Failed to create device image");
}

void VulkanAPI::transitionImageLayout(VkImage image, VkFormat format,
                                      VkImageLayout oldLayout,
                                      VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;

    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        if (hasStencilComponent(format))
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    } else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;  // TODO
    barrier.dstAccessMask = 0;  // TODO

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
               newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else {
        ASH_ASSERT(false, "Unsupported image layout transition");
        return;
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
                         nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommands(commandBuffer);
}

void VulkanAPI::createTextureImage(const std::string& path, Texture& texture) {
    ASH_INFO("Loading texture {}", path);

    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight,
                                &texChannels, STBI_rgb_alpha);

    VkDeviceSize imageSize = texWidth * texHeight * 4;

    ASH_ASSERT(pixels, "Failed to load image from disk");

    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferAllocation;

    createBuffer(imageSize, VMA_MEMORY_USAGE_CPU_ONLY,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer,
                 stagingBufferAllocation);

    void* data;
    vmaMapMemory(allocator, stagingBufferAllocation, &data);
    std::memcpy(data, pixels, static_cast<size_t>(imageSize));
    vmaUnmapMemory(allocator, stagingBufferAllocation);

    stbi_image_free(pixels);

    createImage(texWidth, texHeight, VMA_MEMORY_USAGE_GPU_ONLY,
                VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                texture.image, texture.imageAllocation);

    transitionImageLayout(texture.image, VK_FORMAT_R8G8B8A8_SRGB,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, texture.image,
                      static_cast<uint32_t>(texWidth),
                      static_cast<uint32_t>(texHeight));
    transitionImageLayout(texture.image, VK_FORMAT_R8G8B8A8_SRGB,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAllocation);

    createTextureImageView(texture);

    textures.push_back(texture);
}

VkImageView VulkanAPI::createImageView(VkImage image, VkFormat format,
                                       VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    ASH_ASSERT(
        vkCreateImageView(device, &viewInfo, nullptr, &imageView) == VK_SUCCESS,
        "Failed to create image view");
    return imageView;
}

void VulkanAPI::createTextureImageView(Texture& texture) {
    texture.imageView = createImageView(texture.image, VK_FORMAT_R8G8B8A8_SRGB,
                                        VK_IMAGE_ASPECT_COLOR_BIT);
}

void VulkanAPI::createTextureSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    ASH_ASSERT(vkCreateSampler(device, &samplerInfo, nullptr,
                               &textureSampler) == VK_SUCCESS,
               "Failed to create texture sampler");
}

void VulkanAPI::createSurface() {
    GLFWwindow* window = Ash::App::getWindow()->get();
    VkResult result =
        glfwCreateWindowSurface(instance, window, nullptr, &surface);
    ASH_ASSERT(result == VK_SUCCESS, "Failed to create window surface, {}",
               result);

    ASH_INFO("Created Vulkan surface");
}

void VulkanAPI::init(const std::vector<Pipeline>& pipelines) {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createAllocator();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createPipelineCache();
    createDescriptorSetLayout();
    createGraphicsPipelines(pipelines);
    createDescriptorPool(MAX_INSTANCES);
    createUniformBuffers();
    createDescriptorSets();
    createCommandPools();
    createDepthResources();
    createFramebuffers();
    createCommandBuffers();
    createTextureSampler();
    createSyncObjects();
}

void VulkanAPI::updateUniformBuffers(uint32_t currentImage) {
    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), 0.0f, glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view =
        glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(
        glm::radians(45.0f),
        swapchainExtent.width / (float)swapchainExtent.height, 0.1f, 10.0f);

    ubo.proj[1][1] *= -1;

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    size_t minUniformBufferAllignment =
        properties.limits.minUniformBufferOffsetAlignment;
    size_t dynamicAllignment = sizeof(UniformBufferObject);

    dynamicAllignment = calculateDynamicAllignment(minUniformBufferAllignment,
                                                   dynamicAllignment);

    std::shared_ptr<Scene> scene = Renderer::getScene();
    if (scene) {
        auto renderables = scene->registry.view<Renderable, Transform>();
        int i = 0;
        for (auto entity : renderables) {
            auto [renderable, transform] =
                renderables.get<Renderable, Transform>(entity);

            ubo.model = transform.getTransform();

            char* data;
            vmaMapMemory(allocator,
                         uniformBuffers[currentImage].uniformBufferAllocation,
                         (void**)&data);

            data += i * dynamicAllignment;

            std::memcpy(data, &ubo, sizeof(ubo));

            vmaUnmapMemory(
                allocator,
                uniformBuffers[currentImage].uniformBufferAllocation);
            i++;
        }
    }
}

void VulkanAPI::render() {
    updateCommandBuffers();

    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE,
                    UINT64_MAX);
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame],
        VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }

    ASH_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR,
               "Failed to acquire swapchain image");

    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE)
        vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE,
                        UINT64_MAX);

    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    updateUniformBuffers(imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    ASH_ASSERT(vkQueueSubmit(graphicsQueue, 1, &submitInfo,
                             inFlightFences[currentFrame]) == VK_SUCCESS,
               "Failed to submit render command buffer");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        App::getWindow()->framebufferResized) {
        App::getWindow()->framebufferResized = false;
        recreateSwapchain();
    } else {
        ASH_ASSERT(result == VK_SUCCESS, "Failed to present swapchain image");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanAPI::cleanup() {
    vkDeviceWaitIdle(device);

    ASH_INFO("Cleaning up graphics API");

    vkDestroyPipelineCache(device, pipelineCache, nullptr);

    cleanupSwapchain();

    vkDestroySampler(device, textureSampler, nullptr);

    for (auto texture : textures) {
        vkDestroyImageView(device, texture.imageView, nullptr);
        vmaDestroyImage(allocator, texture.image, texture.imageAllocation);
    }

    for (auto pipeline : graphicsPipelines)
        vkDestroyPipeline(device, pipeline.second, nullptr);

    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

    for (auto buffer : uniformBuffers) {
        vmaDestroyBuffer(allocator, buffer.uniformBuffer,
                         buffer.uniformBufferAllocation);
    }

    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

    for (IndexedVertexBuffer ivb : indexedVertexBuffers) {
        vmaDestroyBuffer(allocator, ivb.buffer, ivb.bufferAllocation);
    }

    vmaDestroyAllocator(allocator);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    vkDestroyFence(device, copyFinishedFence, nullptr);

    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyCommandPool(device, transferCommandPool, nullptr);

    vkDestroyDevice(device, nullptr);

    if (enableValidationLayers)
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
}

void VulkanAPI::updateCommandBuffers() {
    vkQueueWaitIdle(graphicsQueue);
    vkResetCommandPool(device, commandPool, 0);
    recordCommandBuffers();
}

VkFormat VulkanAPI::findSupportedFormat(const std::vector<VkFormat>& candidates,
                                        VkImageTiling tiling,
                                        VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR &&
            (props.linearTilingFeatures & features) == features)
            return format;
        else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
                 (props.optimalTilingFeatures & features) == features)
            return format;
    }

    ASH_ASSERT(false, "Failed to find supported format");
}

bool VulkanAPI::hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
           format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkFormat VulkanAPI::findDepthFormat() {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
         VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void VulkanAPI::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();

    createImage(swapchainExtent.width, swapchainExtent.height,
                VMA_MEMORY_USAGE_GPU_ONLY, depthFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImage,
                depthImageAllocation);
    depthImageView =
        createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

/*
 *
 *      Renderer API
 *
 */

void VulkanAPI::setClearColor(const glm::vec4& color) { clearColor = color; }

IndexedVertexBuffer VulkanAPI::createIndexedVertexArray(
    const std::vector<Vertex>& verts, const std::vector<uint32_t>& indices) {
    IndexedVertexBuffer ret{};
    ret.numIndices = indices.size();

    VkDeviceSize vertSize = sizeof(verts[0]) * verts.size();
    ret.vertSize = vertSize;

    VkDeviceSize indicesSize = sizeof(indices[0]) * indices.size();
    VkDeviceSize bufferSize = vertSize + indicesSize;

    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferAllocation;
    createBuffer(bufferSize, VMA_MEMORY_USAGE_CPU_ONLY,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer,
                 stagingBufferAllocation);

    void* data;
    vmaMapMemory(allocator, stagingBufferAllocation, &data);
    std::memcpy(data, verts.data(), static_cast<size_t>(vertSize));
    std::memcpy(static_cast<Vertex*>(data) + verts.size(), indices.data(),
                static_cast<size_t>(indicesSize));
    vmaUnmapMemory(allocator, stagingBufferAllocation);

    createBuffer(bufferSize, VMA_MEMORY_USAGE_GPU_ONLY,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 ret.buffer, ret.bufferAllocation);

    copyBuffer(stagingBuffer, ret.buffer, bufferSize);

    vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAllocation);

    indexedVertexBuffers.push_back(ret);

    return ret;
}

/*
 *
 *      Renderer API
 *
 */

bool VulkanAPI::checkValidationSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

}  // namespace Ash
