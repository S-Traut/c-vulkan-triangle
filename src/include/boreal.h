#ifndef BRL_APP
#define BRL_APP

#ifdef BRL_IMPLEMENTATION

#define VK_USE_PLATFORM_XCB_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BRL_FILE_IMPLEMENTATION
#include <file.h>

// Using debug might take a longer time to initialize the
// instance because of the validation layers.
#ifdef BRL_NODEBUG
const int enableValidationLayers = 0;
#else
const int enableValidationLayers = 1;
#endif

const char *validation_layers[] = {
    "VK_LAYER_KHRONOS_validation",
};
#define validation_layers_count sizeof(validation_layers) / sizeof(validation_layers[0])

const char *device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};
#define brl_extensions_count sizeof(device_extensions) / sizeof(device_extensions[0])

typedef struct brl_app
{
  void (*init)();
  void (*loop)();
  void (*clean)();
  GLFWwindow *window;
  VkInstance vk_instance;
  VkDevice vk_device;
  VkQueue vk_queue;
  VkQueue vk_present_queue;
  VkSurfaceKHR vk_window_surface;
  VkSwapchainKHR vk_swp;
  VkImage *vk_swp_images;
  VkFormat vk_swp_image_format;
  VkExtent2D vk_swp_extent;
  VkImageView *vk_swp_image_views;
  VkRenderPass vk_render_pass;
  VkPipelineLayout vk_pipeline_layout;
  VkPipeline vk_pipeline;
  VkFramebuffer *vk_frame_buffers;
  VkCommandPool vk_command_pool;
  VkCommandBuffer vk_command_buffer;
  VkSemaphore sema_image_available;
  VkSemaphore sema_render_finished;
  VkFence fence_in_flight;
  uint32_t vk_swp_images_count;
  int width;
  int height;
} brl_app;

typedef struct brl_swp_sup_details
{
  VkSurfaceCapabilitiesKHR capabilities;
  VkSurfaceFormatKHR *formats;
  VkPresentModeKHR *present_modes;
  uint32_t formats_count;
  uint32_t present_modes_counts;
} brl_swp_sup_details;

typedef struct brl_queue_family_indices
{
  int graphics_family;
  int present_family;
} brl_queue_family_indices;

int brl_clamp(int value, int min, int max)
{
  const int t = value < min ? min : value;
  return t > max ? max : t;
}

void brl_exit_error(char *message)
{
  printf("BOREAL_ERROR: %s\n", message);
  exit(1);
}

brl_swp_sup_details brl_query_swp_support(brl_app *app, VkPhysicalDevice device)
{
  brl_swp_sup_details details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, app->vk_window_surface, &details.capabilities);

  uint32_t format_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, app->vk_window_surface, &format_count, NULL);
  if (format_count != 0)
  {
    VkSurfaceFormatKHR *formats = malloc(sizeof(VkSurfaceFormatKHR) * format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, app->vk_window_surface, &format_count, formats);
    details.formats = formats;
    details.formats_count = format_count;
  }

  uint32_t present_mode_count;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, app->vk_window_surface, &present_mode_count, NULL);
  if (format_count != 0)
  {
    VkPresentModeKHR *present_modes = malloc(sizeof(VkPresentModeKHR) * present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, app->vk_window_surface, &present_mode_count, present_modes);
    details.present_modes = present_modes;
    details.present_modes_counts = present_mode_count;
  }
  return details;
}

int brl_device_extension_support(VkPhysicalDevice device)
{
  uint32_t extension_count;
  vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);

  VkExtensionProperties *properties = malloc(sizeof(VkExtensionProperties) * extension_count);
  vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, properties);

  for (int i = 0; i < brl_extensions_count; i++)
  {
    int found = 0;
    for (int j = 0; j < extension_count; j++)
    {
      VkExtensionProperties property = properties[j];
      if (strcmp(property.extensionName, device_extensions[i]))
      {
        found = 1;
        break;
      }
    }
    if (!found)
      return 0;
  }

  return 1;
}

int brl_check_validation_layer_support()
{
  uint32_t property_count = 0;
  vkEnumerateInstanceLayerProperties(&property_count, NULL);
  VkLayerProperties *properties = malloc(sizeof(VkLayerProperties) * property_count);
  vkEnumerateInstanceLayerProperties(&property_count, properties);

  for (int i = 0; i < validation_layers_count; i++)
  {
    uint32_t layer_found = 0;
    for (int j = 0; j < property_count; j++)
    {
      VkLayerProperties property = properties[j];
      if (strcmp(validation_layers[i], property.layerName) == 0)
      {
        layer_found = 1;
        printf("Validation layer found! - %s\n", validation_layers[i]);
        break;
      }
    }

    if (!layer_found)
    {
      printf("Validation layer '%s' not found.\n");
      return 0;
    }
  }
  return 1;
}

/**
 * Create a new VkInstance and insert it into the brl_app
 * this include validation layers setup and extension listings
 **/
void brl_create_instance(brl_app *app)
{
  VkApplicationInfo app_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "Boreal",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_0,
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_0,
  };

  uint32_t glfw_extensions_count = 0;
  const char **required_extensions = glfwGetRequiredInstanceExtensions(&glfw_extensions_count);

  // Adding validations layers
  if (!brl_check_validation_layer_support())
    exit(1);

  VkInstanceCreateInfo instance_create_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app_info,
      .enabledExtensionCount = glfw_extensions_count,
      .ppEnabledExtensionNames = required_extensions,
      .enabledLayerCount = 0,
      .enabledLayerCount = enableValidationLayers ? validation_layers_count : 0,
      .ppEnabledLayerNames = enableValidationLayers ? validation_layers : NULL,
  };

  VkInstance instance = malloc(sizeof(VkInstance));
  VkResult result = vkCreateInstance(&instance_create_info, NULL, &instance);
  if (result != VK_SUCCESS)
  {
    printf("Couldn't create VkInstance.\n");
    exit(1);
  }

  printf("-> Created VkInstance\n");
  app->vk_instance = instance;
}

brl_queue_family_indices brl_find_queue_families(brl_app *app, VkPhysicalDevice device)
{
  brl_queue_family_indices indices = {-1, -1};

  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);

  VkQueueFamilyProperties *queue_families = malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);

  for (int i = 0; i < queue_family_count; i++)
  {
    VkQueueFamilyProperties queue_family = queue_families[i];
    if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
      indices.graphics_family = i;

    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, app->vk_window_surface, &presentSupport);
    if (presentSupport)
      indices.present_family = i;

    i++;
  }
  return indices;
}

int brl_is_queue_family_complete(brl_queue_family_indices indices)
{
  return indices.graphics_family != -1 && indices.present_family != -1;
}

/**
 * This function checks for the physical device suitability
 * of the application.
 * The rules that are being set are the followings:
 *
 * - The physical device must have a graphical queue family
 * - The physical device must be a discrete GPU type
 * - The physical device must have a geometry shader feature
 **/
int brl_is_physical_device_suitable(brl_app *app, VkPhysicalDevice device)
{
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(device, &properties);

  VkPhysicalDeviceFeatures features;
  vkGetPhysicalDeviceFeatures(device, &features);

  brl_queue_family_indices families_indices = brl_find_queue_families(app, device);
  if (!brl_is_queue_family_complete(families_indices))
    return 0;

  if (!brl_device_extension_support(device))
    return 0;

  brl_swp_sup_details swp_support = brl_query_swp_support(app, device);
  if (swp_support.formats == NULL || swp_support.present_modes == NULL)
    return 0;

  return 1;
}

VkPresentModeKHR brl_pick_swp_present_mode(VkPresentModeKHR *present_modes, uint32_t present_modes_count)
{
  for (int i = 0; i < present_modes_count; i++)
  {
    VkPresentModeKHR present_mode = present_modes[i];
    if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
      return present_mode;
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D brl_pick_swp_extent(brl_app *app, VkSurfaceCapabilitiesKHR capabilities)
{
  if (capabilities.currentExtent.width != UINT32_MAX)
    return capabilities.currentExtent;

  int width, height;
  glfwGetFramebufferSize(app->window, &width, &height);

  VkExtent2D actual_extent = {
      .width = width,
      .height = height,
  };

  actual_extent.width = brl_clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
  actual_extent.height = brl_clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

  return actual_extent;
}

VkSurfaceFormatKHR brl_pick_swp_surface_format(VkSurfaceFormatKHR *formats, uint32_t formats_count)
{
  for (int i = 0; i < formats_count; i++)
  {
    VkSurfaceFormatKHR format = formats[i];
    if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      return format;
  }

  return formats[0];
}
/**
 * Picking a physical device involve to loop through all the
 * computer's physical devices and pick one that is suitable
 * to our needs.
 *
 * The check of a physical device suitability is done
 * through the brl_is_physical_device_suitable function
 **/
VkPhysicalDevice brl_pick_physical_device(brl_app *app)
{
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(app->vk_instance, &device_count, NULL);
  if (device_count == 0)
    brl_exit_error("Failed to find any GPU with Vulkan support.");

  VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * device_count);
  vkEnumeratePhysicalDevices(app->vk_instance, &device_count, devices);
  for (int i = 0; i < device_count; i++)
  {
    VkPhysicalDevice device = devices[i];
    if (brl_is_physical_device_suitable(app, device))
    {
      physical_device = device;
      break;
    }

    if (physical_device == VK_NULL_HANDLE)
      brl_exit_error("No suitable physical device.");
  }

  return physical_device;
}

void brl_list_available_extensions()
{
  uint32_t extension_count = 0;
  vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);

  VkExtensionProperties *properties = malloc(sizeof(VkExtensionProperties) * extension_count);
  vkEnumerateInstanceExtensionProperties(NULL, &extension_count, properties);

  printf("Available VK extensions:\n");
  for (int i = 0; i < extension_count; i++)
  {
    VkExtensionProperties property = properties[i];
    printf("%s, ", property.extensionName);
  }
  printf("\n\n");

  free(properties);
}

/**
 * Creating a logical device from a physical device
 * This do a slight check on the queues to be sure we do not
 * create 2 devices queues that have the same family index.
 **/
void brl_create_logical_device(brl_app *app, VkPhysicalDevice physical_device)
{
  brl_queue_family_indices indices = brl_find_queue_families(app, physical_device);

  VkPhysicalDeviceFeatures device_features = {0};
  VkDeviceCreateInfo device_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pEnabledFeatures = &device_features,
  };

  float queue_priority = 1.0f;
  VkBool32 is_multiple = indices.graphics_family != indices.present_family;

  VkDeviceQueueCreateInfo queue_create_infos[2];
  VkDeviceQueueCreateInfo queue_create_info;

  if (is_multiple)
  {
    uint32_t unique_queue_families[] = {indices.graphics_family, indices.present_family};
    uint32_t info_sizes = sizeof(unique_queue_families) / sizeof(unique_queue_families[0]);

    for (int i = 0; i < info_sizes; i++)
    {
      queue_create_infos[i] = (VkDeviceQueueCreateInfo){
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = unique_queue_families[i],
          .queueCount = 1,
          .pQueuePriorities = &queue_priority,
      };
    }

    device_info.queueCreateInfoCount = info_sizes;
    device_info.pQueueCreateInfos = queue_create_infos;
  }
  else
  {
    queue_create_info = (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = indices.graphics_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };

    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_create_info;
  }

  if (enableValidationLayers)
  {
    device_info.enabledLayerCount = validation_layers_count;
    device_info.ppEnabledLayerNames = validation_layers;
  }
  else
  {
    device_info.enabledLayerCount = 0;
  }

  device_info.enabledExtensionCount = brl_extensions_count;
  device_info.ppEnabledExtensionNames = device_extensions;

  VkDevice device = malloc(sizeof(VkDevice));
  if (vkCreateDevice(physical_device, &device_info, NULL, &device) != VK_SUCCESS)
    brl_exit_error("Failed to create logical device.");

  printf("-> Created VkDevice\n");
  app->vk_device = device;
}

void brl_set_device_queue(brl_app *app, VkPhysicalDevice physical_device)
{
  brl_queue_family_indices indices = brl_find_queue_families(app, physical_device);
  VkQueue queue = malloc(sizeof(VkQueue));
  vkGetDeviceQueue(app->vk_device, indices.graphics_family, 0, &queue);
  app->vk_queue = queue;
  printf("SET: vk_queue (Device queue)\n");
}

void brl_set_present_queue(brl_app *app, VkPhysicalDevice physical_device)
{
  brl_queue_family_indices indices = brl_find_queue_families(app, physical_device);
  VkQueue queue = malloc(sizeof(VkQueue));
  vkGetDeviceQueue(app->vk_device, indices.present_family, 0, &queue);
  app->vk_present_queue = queue;
  printf("SET: vk_present_queue (Presentation queue)\n");
}

void brl_create_window_surface(brl_app *app)
{

  VkSurfaceKHR window_surface = malloc(sizeof(VkSurfaceKHR));
  VkResult result = glfwCreateWindowSurface(app->vk_instance, app->window, NULL, &window_surface);
  if (result != VK_SUCCESS)
    brl_exit_error("Failed to create window surface.");

  printf("-> Created VkSurfaceKHR (window surface)\n");
  app->vk_window_surface = window_surface;
}

void brl_create_swp(brl_app *app, VkPhysicalDevice physical_device)
{
  brl_swp_sup_details swp_support = brl_query_swp_support(app, physical_device);
  VkSurfaceFormatKHR surface_format = brl_pick_swp_surface_format(swp_support.formats, swp_support.formats_count);
  VkPresentModeKHR present_mode = brl_pick_swp_present_mode(swp_support.present_modes, swp_support.present_modes_counts);
  VkExtent2D extent = brl_pick_swp_extent(app, swp_support.capabilities);

  uint32_t image_count = swp_support.capabilities.minImageCount + 1;

  if (swp_support.capabilities.maxImageCount > 0 && image_count > swp_support.capabilities.maxImageCount)
    image_count = swp_support.capabilities.maxImageCount;

  VkSwapchainCreateInfoKHR create_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = app->vk_window_surface,
      .minImageCount = image_count,
      .imageFormat = surface_format.format,
      .imageColorSpace = surface_format.colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .preTransform = swp_support.capabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = present_mode,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
  };

  brl_queue_family_indices indices = brl_find_queue_families(app, physical_device);
  uint32_t indices_arr[] = {indices.graphics_family, indices.present_family};

  if (indices.graphics_family != indices.present_family)
  {
    create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount = 2;
    create_info.pQueueFamilyIndices = indices_arr;
  }
  else
  {
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = NULL;
  }

  VkSwapchainKHR swapchain = malloc(sizeof(VkSwapchainKHR));
  VkResult result = vkCreateSwapchainKHR(app->vk_device, &create_info, NULL, &swapchain);
  if (result != VK_SUCCESS)
    brl_exit_error("Couldn't create the swap chain.");

  printf("-> Created VkSwapchainKHR (swapchain)\n");

  vkGetSwapchainImagesKHR(app->vk_device, swapchain, &image_count, NULL);
  VkImage *images = malloc(sizeof(VkImage) * image_count);
  vkGetSwapchainImagesKHR(app->vk_device, swapchain, &image_count, images);

  app->vk_swp_image_format = surface_format.format;
  app->vk_swp_extent = extent;
  app->vk_swp = swapchain;
  app->vk_swp_images = images;
  app->vk_swp_images_count = image_count;
}

void brl_create_image_views(brl_app *app)
{
  VkImageView *image_views = malloc(sizeof(VkImageView) * app->vk_swp_images_count);
  for (size_t i = 0; i < app->vk_swp_images_count; i++)
  {
    VkImageViewCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = app->vk_swp_images[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = app->vk_swp_image_format,
        .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1,
    };

    VkResult result = vkCreateImageView(app->vk_device, &create_info, NULL, &image_views[i]);
    if (result != VK_SUCCESS)
      brl_exit_error("Failed to create image views.");
  }
  app->vk_swp_image_views = image_views;
}

void brl_free_app(brl_app app)
{
  vkDestroySemaphore(app.vk_device, app.sema_image_available, NULL);
  vkDestroySemaphore(app.vk_device, app.sema_render_finished, NULL);
  vkDestroyFence(app.vk_device, app.fence_in_flight, NULL);

  vkDestroyCommandPool(app.vk_device, app.vk_command_pool, NULL);
  for (size_t i = 0; i < app.vk_swp_images_count; i++)
    vkDestroyFramebuffer(app.vk_device, app.vk_frame_buffers[i], NULL);

  vkDestroyPipeline(app.vk_device, app.vk_pipeline, NULL);
  vkDestroyPipelineLayout(app.vk_device, app.vk_pipeline_layout, NULL);
  vkDestroyRenderPass(app.vk_device, app.vk_render_pass, NULL);
  for (size_t i = 0; i < app.vk_swp_images_count; i++)
    vkDestroyImageView(app.vk_device, app.vk_swp_image_views[i], NULL);

  vkDestroySwapchainKHR(app.vk_device, app.vk_swp, NULL);
  vkDestroySurfaceKHR(app.vk_instance, app.vk_window_surface, NULL);
  vkDestroyDevice(app.vk_device, NULL);
  vkDestroyInstance(app.vk_instance, NULL);
}

VkShaderModule brl_create_shader_module(brl_app *app, brl_file file)
{
  VkShaderModuleCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = file.size,
      .pCode = (uint32_t *)file.data,
  };

  VkShaderModule module;
  VkResult result = vkCreateShaderModule(app->vk_device, &create_info, NULL, &module);
  if (result != VK_SUCCESS)
    brl_exit_error("Failed to create shader module.");

  return module;
}

void brl_create_gfx_pipeline(brl_app *app)
{
  brl_file vshader_file = brl_read("./src/shaders/vertex.spv");
  brl_file fshader_file = brl_read("./src/shaders/fragment.spv");

  VkShaderModule vshader = brl_create_shader_module(app, vshader_file);
  VkShaderModule fshader = brl_create_shader_module(app, fshader_file);

  brl_file_close(vshader_file);
  brl_file_close(fshader_file);

  VkPipelineShaderStageCreateInfo vshader_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vshader,
      .pName = "main",
  };

  VkPipelineShaderStageCreateInfo fshader_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = fshader,
      .pName = "main",
  };

  VkPipelineShaderStageCreateInfo shader_stages[] = {vshader_info, fshader_info};

  VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = 2,
      .pDynamicStates = dynamic_states,
  };

  VkPipelineVertexInputStateCreateInfo vertex_input_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 0,
      .pVertexBindingDescriptions = NULL,
      .vertexAttributeDescriptionCount = 0,
      .pVertexAttributeDescriptions = NULL,
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  VkViewport viewport = {
      .x = 0.0f,
      .y = 0.0f,
      .width = app->vk_swp_extent.width,
      .height = app->vk_swp_extent.height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  VkRect2D scissor = {
      .offset = {0, 0},
      .extent = app->vk_swp_extent,
  };

  VkPipelineViewportStateCreateInfo viewport_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor,
  };

  VkPipelineRasterizationStateCreateInfo rasterizer = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .lineWidth = 1.0f,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
  };

  VkPipelineMultisampleStateCreateInfo multi_sampling = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .sampleShadingEnable = VK_FALSE,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
  };

  VkPipelineColorBlendAttachmentState color_blend_attachment = {
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      .blendEnable = VK_FALSE,
  };

  VkPipelineColorBlendStateCreateInfo color_blending = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attachment,
  };

  VkPipelineLayoutCreateInfo pipeline_layout_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
  };

  VkPipelineLayout pipeline_layout = malloc(sizeof(VkPipelineLayout));
  VkResult pipeline_result = vkCreatePipelineLayout(app->vk_device, &pipeline_layout_info, NULL, &pipeline_layout);
  if (pipeline_result != VK_SUCCESS)
    brl_exit_error("Failed to create pipeline layout.");

  printf("-> Created pipeline layout\n");
  app->vk_pipeline_layout = pipeline_layout;

  VkGraphicsPipelineCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = shader_stages,
      .pVertexInputState = &vertex_input_info,
      .pInputAssemblyState = &input_assembly,
      .pViewportState = &viewport_state,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multi_sampling,
      .pDepthStencilState = NULL,
      .pColorBlendState = &color_blending,
      .pDynamicState = &dynamic_state,
      .layout = app->vk_pipeline_layout,
      .renderPass = app->vk_render_pass,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
  };

  VkPipeline pipeline = malloc(sizeof(VkPipeline));
  VkResult result = vkCreateGraphicsPipelines(app->vk_device, VK_NULL_HANDLE, 1, &create_info, NULL, &pipeline);
  if (result != VK_SUCCESS)
    brl_exit_error("Failed to create the render pipeline.");

  printf("-> Created VkPipeline (Graphics pipeline)\n");

  app->vk_pipeline = pipeline;
  vkDestroyShaderModule(app->vk_device, vshader, NULL);
  vkDestroyShaderModule(app->vk_device, fshader, NULL);
}

void brl_create_render_pass(brl_app *app)
{
  VkAttachmentDescription color_attachment = {
      .format = app->vk_swp_image_format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };

  VkAttachmentReference color_attachment_ref = {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpass = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment_ref,
  };

  VkRenderPassCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &color_attachment,
      .subpassCount = 1,
      .pSubpasses = &subpass,
  };

  VkRenderPass render_pass = malloc(sizeof(VkRenderPass));
  VkResult result = vkCreateRenderPass(app->vk_device, &create_info, NULL, &render_pass);
  if (result != VK_SUCCESS)
    brl_exit_error("Failed to create render pass.");

  printf("-> Created VkRenderPass\n");
  app->vk_render_pass = render_pass;
}

void brl_create_frame_buffer(brl_app *app)
{
  VkFramebuffer *frame_buffers = malloc(sizeof(frame_buffers) * app->vk_swp_images_count);
  for (size_t i = 0; i < app->vk_swp_images_count; i++)
  {
    VkImageView attachments[] = {app->vk_swp_image_views[i]};
    VkFramebufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = app->vk_render_pass,
        .attachmentCount = 1,
        .pAttachments = attachments,
        .width = app->vk_swp_extent.width,
        .height = app->vk_swp_extent.height,
        .layers = 1,
    };

    VkResult result = vkCreateFramebuffer(app->vk_device, &create_info, NULL, &frame_buffers[i]);
    if (result != VK_SUCCESS)
      brl_exit_error("Couldn't create framebuffers.");
  }
  printf("-> Created VkFrameBuffer (Frame buffers)\n");
  app->vk_frame_buffers = frame_buffers;
}

void brl_create_command_pool(brl_app *app, VkPhysicalDevice physical_device)
{
  brl_queue_family_indices indices = brl_find_queue_families(app, physical_device);

  VkCommandPoolCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = indices.graphics_family,
  };

  VkCommandPool command_pool = malloc(sizeof(VkCommandPool));
  VkResult result = vkCreateCommandPool(app->vk_device, &create_info, NULL, &command_pool);
  if (result != VK_SUCCESS)
    brl_exit_error("Failed to create command pool.");

  printf("-> Created VkCommandPool\n");
  app->vk_command_pool = command_pool;
}

void brl_create_command_buffer(brl_app *app)
{
  VkCommandBufferAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = app->vk_command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  VkCommandBuffer command_buffer = malloc(sizeof(VkCommandBuffer));
  VkResult result = vkAllocateCommandBuffers(app->vk_device, &alloc_info, &command_buffer);
  if (result != VK_SUCCESS)
    brl_exit_error("Failed to allocate command buffer");

  printf("-> Allocated VkCommandBuffer\n");
  app->vk_command_buffer = command_buffer;
}

void brl_record_command_buffer(brl_app *app, VkCommandBuffer command_buffer, uint32_t image_index)
{
  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };

  VkResult result = vkBeginCommandBuffer(command_buffer, &begin_info);
  if (result != VK_SUCCESS)
    brl_exit_error("Failed to begin recording command buffer");

  VkRenderPassBeginInfo render_pass_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = app->vk_render_pass,
      .framebuffer = app->vk_frame_buffers[image_index],
      .renderArea.offset = {0, 0},
      .renderArea.extent = app->vk_swp_extent,
  };

  VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  render_pass_info.clearValueCount = 1;
  render_pass_info.pClearValues = &clear_color;

  vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->vk_pipeline);

  VkViewport viewport = {
      .x = 0.0f,
      .y = 0.0f,
      .width = app->vk_swp_extent.width,
      .height = app->vk_swp_extent.height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  vkCmdSetViewport(command_buffer, 0, 1, &viewport);

  VkRect2D scissor = {
      .offset = {0, 0},
      .extent = app->vk_swp_extent,
  };
  vkCmdSetScissor(command_buffer, 0, 1, &scissor);
  vkCmdDraw(command_buffer, 3, 1, 0, 0);
  vkCmdEndRenderPass(command_buffer);
  VkResult end_result = vkEndCommandBuffer(command_buffer);
  if (end_result != VK_SUCCESS)
    brl_exit_error("Failed to record command buffer");
}

void brl_create_sync_objects(brl_app *app)
{
  VkSemaphoreCreateInfo semaphore_create_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  VkFenceCreateInfo fence_create_info = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };

  VkSemaphore sema_image_available = malloc(sizeof(VkSemaphore));
  VkSemaphore sema_render_finished = malloc(sizeof(VkSemaphore));
  VkFence fence = malloc(sizeof(VkFence));
  int fail = 0;
  if (vkCreateSemaphore(app->vk_device, &semaphore_create_info, NULL, &sema_image_available) != VK_SUCCESS)
    fail = 1;

  if (vkCreateSemaphore(app->vk_device, &semaphore_create_info, NULL, &sema_render_finished) != VK_SUCCESS)
    fail = 1;

  if (vkCreateFence(app->vk_device, &fence_create_info, NULL, &fence) != VK_SUCCESS)
    fail = 1;

  if (fail)
    brl_exit_error("Failed to create semaphores / fences");

  app->sema_image_available = sema_image_available;
  app->sema_render_finished = sema_render_finished;
  app->fence_in_flight = fence;
}

void brl_recreate_swp(brl_app *app, VkPhysicalDevice physical_device)
{
  vkDeviceWaitIdle(app->vk_device);
  brl_create_swp(app, physical_device);
  brl_create_image_views(app);
  brl_create_frame_buffer(app);
}

void brl_create_app(brl_app app)
{
  printf("Welcome to Boreal!\n\n");
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  app.window = glfwCreateWindow(app.width, app.height, "BOREAL APP", NULL, NULL);

#ifndef BRL_NDEBUG
  brl_list_available_extensions();
#endif

  brl_create_instance(&app);
  brl_create_window_surface(&app);
  VkPhysicalDevice physical_device = brl_pick_physical_device(&app);
  brl_create_logical_device(&app, physical_device);
  brl_set_device_queue(&app, physical_device);
  brl_set_present_queue(&app, physical_device);
  brl_create_swp(&app, physical_device);
  brl_create_image_views(&app);
  brl_create_render_pass(&app);
  brl_create_gfx_pipeline(&app);
  brl_create_frame_buffer(&app);
  brl_create_command_pool(&app, physical_device);
  brl_create_command_buffer(&app);
  brl_create_sync_objects(&app);

  if (app.init)
    app.init(&app);

  while (!glfwWindowShouldClose(app.window))
  {
    glfwPollEvents();
    if (app.loop)
      app.loop(&app);
  }

  brl_free_app(app);
  glfwDestroyWindow(app.window);
  glfwTerminate();

  if (app.clean)
    app.clean(&app);
}
#endif
#endif