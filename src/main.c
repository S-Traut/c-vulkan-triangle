
#define BRL_IMPLEMENTATION
#define BRL_NODEBUG
#include <boreal.h>

void init(brl_app *app)
{
}

void loop(brl_app *app)
{
  VkFence fences[] = {app->fence_in_flight};
  vkWaitForFences(app->vk_device, 1, fences, VK_TRUE, UINT64_MAX);
  vkResetFences(app->vk_device, 1, fences);

  uint32_t image_index;
  vkAcquireNextImageKHR(app->vk_device, app->vk_swp, UINT64_MAX, app->sema_image_available, VK_NULL_HANDLE, &image_index);

  vkResetCommandBuffer(app->vk_command_buffer, 0);
  brl_record_command_buffer(app, app->vk_command_buffer, image_index);

  VkSemaphore wait_semaphores[] = {app->sema_image_available};
  VkSemaphore sign_semaphores[] = {app->sema_render_finished};
  VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  VkCommandBuffer buffers[] = {app->vk_command_buffer};
  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = buffers,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = sign_semaphores,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = wait_semaphores,
      .pWaitDstStageMask = wait_stages,
  };

  vkQueueSubmit(app->vk_queue, 1, &submit_info, app->fence_in_flight);

  VkSwapchainKHR swapchains[] = {app->vk_swp};
  VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .swapchainCount = 1,
      .pSwapchains = swapchains,
      .pImageIndices = &image_index,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = sign_semaphores,
  };

  vkQueuePresentKHR(app->vk_present_queue, &present_info);
}

void clean(brl_app *app)
{
}

int main()
{
  int t = 10;
  brl_app app = {
      .init = init,
      .loop = loop,
      .clean = clean,
      .width = 800,
      .height = 600,
  };

  brl_create_app(app);
  return 0;
}
