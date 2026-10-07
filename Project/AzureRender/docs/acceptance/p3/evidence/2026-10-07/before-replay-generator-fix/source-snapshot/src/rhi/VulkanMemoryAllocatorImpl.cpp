// Single translation unit that compiles the Vulkan Memory Allocator library.
//
// Vulkan entry points are resolved at runtime through vkGetInstanceProcAddr and
// vkGetDeviceProcAddr (see GpuAllocator::initialize) so the allocator does not
// depend on how the Vulkan loader was linked.
#define VMA_IMPLEMENTATION 1
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#include <vk_mem_alloc.h>
