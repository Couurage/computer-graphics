#include "resources.hpp"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../third_party/stb_image.h"
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
void check(VkResult result,const char* message) {
    if (result!=VK_SUCCESS) throw std::runtime_error(message);
}
struct Staging {
    VkBuffer buffer{};
    VmaAllocation allocation{};
    Staging(const void* pixels,VkDeviceSize size) {
        auto& c=graphics::internal::context;
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size=size; info.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VmaAllocationCreateInfo memory{};
        memory.usage=VMA_MEMORY_USAGE_AUTO;
        memory.flags=VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT|VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VmaAllocationInfo details{};
        check(vmaCreateBuffer(c.allocator,&info,&memory,&buffer,&allocation,&details),"Cannot create staging buffer");
        std::memcpy(details.pMappedData,pixels,size);
        auto result=vmaFlushAllocation(c.allocator,allocation,0,size);
        if (result!=VK_SUCCESS) {
            vmaDestroyBuffer(c.allocator,buffer,allocation);
            throw std::runtime_error("Cannot flush staging buffer");
        }
    }
    ~Staging() { vmaDestroyBuffer(graphics::internal::context.allocator,buffer,allocation); }
};
void transfer(const std::function<void(VkCommandBuffer)>& record) {
    auto& c=graphics::internal::context;
    VkCommandPool pool{};
    VkCommandPoolCreateInfo info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    info.queueFamilyIndex=c.graphics_queue_index; info.flags=VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    check(vkCreateCommandPool(c.device,&info,nullptr,&pool),"Cannot create transfer pool");
    try {
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate.commandPool=pool; allocate.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; allocate.commandBufferCount=1;
        VkCommandBuffer cmd{};
        check(vkAllocateCommandBuffers(c.device,&allocate,&cmd),"Cannot allocate transfer commands");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(vkBeginCommandBuffer(cmd,&begin),"Cannot begin transfer");
        record(cmd);
        check(vkEndCommandBuffer(cmd),"Cannot end transfer");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount=1; submit.pCommandBuffers=&cmd;
        check(vkQueueSubmit(c.graphics_queue,1,&submit,VK_NULL_HANDLE),"Cannot submit transfer");
        check(vkQueueWaitIdle(c.graphics_queue),"Cannot wait for transfer");
    } catch (...) { vkDestroyCommandPool(c.device,pool,nullptr); throw; }
    vkDestroyCommandPool(c.device,pool,nullptr);
}
}
void destroyDeviceBuffer(DeviceBuffer& buffer) {
    if (buffer.handle) vmaDestroyBuffer(graphics::internal::context.allocator,buffer.handle,buffer.allocation);
    buffer={};
}
void uploadDeviceBuffer(DeviceBuffer& buffer,const void* data,VkDeviceSize size,VkBufferUsageFlags usage) {
    if (!size) return;
    auto& c=graphics::internal::context;
    if (size>buffer.capacity) {
        destroyDeviceBuffer(buffer);
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size=size; info.usage=usage|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        VmaAllocationCreateInfo memory{};
        memory.usage=VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        memory.requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        check(vmaCreateBuffer(c.allocator,&info,&memory,&buffer.handle,&buffer.allocation,nullptr),"Cannot create GPU buffer");
        buffer.capacity=size;
    }
    Staging staging(data,size);
    transfer([&](VkCommandBuffer cmd) {
        VkBufferCopy region{0,0,size};
        vkCmdCopyBuffer(cmd,staging.buffer,buffer.handle,1,&region);
        VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask=usage&VK_BUFFER_USAGE_INDEX_BUFFER_BIT ? VK_ACCESS_INDEX_READ_BIT : VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer=buffer.handle; barrier.size=size;
        vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,0,0,nullptr,1,&barrier,0,nullptr);
    });
}
Texture loadTexture(const std::string& path,bool srgb) {
    int w=0,h=0,channels=0;
    auto* pixels=stbi_load(path.c_str(),&w,&h,&channels,4);
    if (!pixels) throw std::runtime_error("Cannot load texture: "+path+" ("+stbi_failure_reason()+")");
    Texture texture{};
    auto& c=graphics::internal::context;
    try {
        Staging staging(pixels,VkDeviceSize(w)*h*4);
        stbi_image_free(pixels); pixels=nullptr;
        texture.width=w; texture.height=h;
        VkFormat format=srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
        VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        image.imageType=VK_IMAGE_TYPE_2D; image.format=format; image.extent={uint32_t(w),uint32_t(h),1};
        image.mipLevels=image.arrayLayers=1; image.samples=VK_SAMPLE_COUNT_1_BIT;
        image.tiling=VK_IMAGE_TILING_OPTIMAL; image.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
        VmaAllocationCreateInfo memory{};
        memory.usage=VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE; memory.requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        check(vmaCreateImage(c.allocator,&image,&memory,&texture.image,&texture.allocation,nullptr),"Cannot create texture image");
        transfer([&](VkCommandBuffer cmd) {
            VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED; barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            barrier.image=texture.image; barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
            vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
            VkBufferImageCopy region{};
            region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; region.imageExtent=image.extent;
            vkCmdCopyBufferToImage(cmd,staging.buffer,texture.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region);
            barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
        });
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image=texture.image; view.viewType=VK_IMAGE_VIEW_TYPE_2D; view.format=format;
        view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        check(vkCreateImageView(c.device,&view,nullptr,&texture.view),"Cannot create texture view");
        VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler.magFilter=sampler.minFilter=VK_FILTER_LINEAR; sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.maxLod=0;
        check(vkCreateSampler(c.device,&sampler,nullptr,&texture.sampler),"Cannot create sampler");
    } catch (...) { stbi_image_free(pixels); destroyTexture(texture); throw; }
    return texture;
}
void destroyTexture(Texture& texture) {
    auto& c=graphics::internal::context;
    vkDestroySampler(c.device,texture.sampler,nullptr);
    vkDestroyImageView(c.device,texture.view,nullptr);
    if (texture.image) vmaDestroyImage(c.allocator,texture.image,texture.allocation);
    texture={};
}
