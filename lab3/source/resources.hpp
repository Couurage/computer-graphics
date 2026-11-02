#pragma once
#include "graphics_internal.hpp"
#include <string>

struct DeviceBuffer {
    VkBuffer handle{};
    VmaAllocation allocation{};
    VkDeviceSize capacity{};
};
struct Texture {
    VkImage image{};
    VmaAllocation allocation{};
    VkImageView view{};
    VkSampler sampler{};
    int width=0, height=0;
};
void uploadDeviceBuffer(DeviceBuffer& buffer, const void* data, VkDeviceSize size, VkBufferUsageFlags usage);
void destroyDeviceBuffer(DeviceBuffer& buffer);
Texture loadTexture(const std::string& path, bool srgb);
void destroyTexture(Texture& texture);
