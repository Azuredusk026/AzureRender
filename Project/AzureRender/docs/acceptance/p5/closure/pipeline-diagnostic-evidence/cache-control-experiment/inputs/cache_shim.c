/* Diagnostic only: replace null Vulkan pipeline caches in the owned child. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <vulkan/vulkan.h>
#include "MinHook.h"

typedef struct CacheHeader {
    char magic[8];
    uint32_t version, vendor, device, driver;
    uint8_t uuid[VK_UUID_SIZE];
    uint64_t bytes;
} CacheHeader;
typedef struct DeviceCache {
    VkDevice device;
    VkPipelineCache cache;
    VkPhysicalDeviceProperties properties;
} DeviceCache;
static SRWLOCK lock = SRWLOCK_INIT;
static DeviceCache devices[4];
static wchar_t path[2048];
static uint64_t graphicsCalls, computeCalls, loadedBytes, savedBytes, failedCaches;
static int enabled = 1;
static PFN_vkCreateDevice realDevice;
static PFN_vkDestroyDevice realDestroy;
static PFN_vkCreateGraphicsPipelines realGraphics;
static PFN_vkCreateComputePipelines realCompute;
static const uint64_t maxBytes = 128ull*1024*1024;

static void *load(const VkPhysicalDeviceProperties *properties, size_t *bytes) {
    FILE *file = NULL;
    CacheHeader header;
    void *data = NULL;
    *bytes = 0;
    if (_wfopen_s(&file, path, L"rb") || !file) return NULL;
    if (fread(&header, 1, sizeof(header), file) != sizeof(header)
        || memcmp(header.magic, "AZCACHE1", 8) || header.version != 1
        || header.vendor != properties->vendorID || header.device != properties->deviceID
        || header.driver != properties->driverVersion
        || memcmp(header.uuid, properties->pipelineCacheUUID, VK_UUID_SIZE)
        || !header.bytes || header.bytes > maxBytes) {
        fclose(file); return NULL;
    }
    data = malloc((size_t)header.bytes);
    if (!data || fread(data, 1, (size_t)header.bytes, file) != header.bytes || fgetc(file) != EOF) {
        free(data); fclose(file); return NULL;
    }
    fclose(file);
    *bytes = (size_t)header.bytes;
    return data;
}

static void save(DeviceCache *entry) {
    size_t bytes = 0;
    void *data = NULL;
    FILE *file = NULL;
    CacheHeader header = {0};
    wchar_t temporary[2080];
    if (!entry->cache || vkGetPipelineCacheData(entry->device, entry->cache, &bytes, NULL) != VK_SUCCESS
        || !bytes || bytes > maxBytes) return;
    data = malloc(bytes);
    if (!data || vkGetPipelineCacheData(entry->device, entry->cache, &bytes, data) != VK_SUCCESS) {
        free(data); return;
    }
    memcpy(header.magic, "AZCACHE1", 8);
    header.version = 1; header.vendor = entry->properties.vendorID;
    header.device = entry->properties.deviceID; header.driver = entry->properties.driverVersion;
    memcpy(header.uuid, entry->properties.pipelineCacheUUID, VK_UUID_SIZE);
    header.bytes = bytes;
    _snwprintf_s(temporary, _countof(temporary), _TRUNCATE, L"%ls.tmp", path);
    if (!_wfopen_s(&file, temporary, L"wb") && file) {
        int ok = fwrite(&header, 1, sizeof(header), file) == sizeof(header)
                 && fwrite(data, 1, bytes, file) == bytes;
        ok = fclose(file) == 0 && ok;
        if (ok && MoveFileExW(temporary, path, MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            savedBytes += bytes;
    }
    free(data);
}

static VkResult VKAPI_CALL createDevice(VkPhysicalDevice physical, const VkDeviceCreateInfo *info,
                                        const VkAllocationCallbacks *allocator, VkDevice *device) {
    VkResult result = realDevice(physical, info, allocator, device);
    if (result == VK_SUCCESS && enabled) {
        unsigned index;
        AcquireSRWLockExclusive(&lock);
        for (index = 0; index < 4 && devices[index].device; ++index) {}
        if (index < 4) {
            DeviceCache *entry = &devices[index];
            VkPipelineCacheCreateInfo cacheInfo = {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
            void *data;
            size_t bytes;
            entry->device = *device;
            vkGetPhysicalDeviceProperties(physical, &entry->properties);
            data = load(&entry->properties, &bytes);
            cacheInfo.initialDataSize = bytes; cacheInfo.pInitialData = data;
            if (vkCreatePipelineCache(*device, &cacheInfo, NULL, &entry->cache) != VK_SUCCESS) {
                ++failedCaches;
                cacheInfo.initialDataSize = 0; cacheInfo.pInitialData = NULL;
                if (vkCreatePipelineCache(*device, &cacheInfo, NULL, &entry->cache) != VK_SUCCESS)
                    entry->cache = VK_NULL_HANDLE;
            } else loadedBytes += bytes;
            free(data);
        } else ++failedCaches;
        ReleaseSRWLockExclusive(&lock);
    }
    return result;
}

static void VKAPI_CALL destroyDevice(VkDevice device, const VkAllocationCallbacks *allocator) {
    unsigned index;
    AcquireSRWLockExclusive(&lock);
    for (index = 0; index < 4; ++index) {
        if (devices[index].device == device) {
            save(&devices[index]);
            if (devices[index].cache) vkDestroyPipelineCache(device, devices[index].cache, NULL);
            memset(&devices[index], 0, sizeof(devices[index]));
            break;
        }
    }
    ReleaseSRWLockExclusive(&lock);
    fprintf(stderr, "AZURE_DIAGNOSTIC_CACHE graphics=%llu compute=%llu loaded=%llu saved=%llu failed=%llu\n",
            graphicsCalls, computeCalls, loadedBytes, savedBytes, failedCaches);
    realDestroy(device, allocator);
}

static VkPipelineCache cacheFor(VkDevice device, VkPipelineCache existing) {
    unsigned index;
    if (existing) return existing;
    for (index = 0; index < 4; ++index)
        if (devices[index].device == device) return devices[index].cache;
    return VK_NULL_HANDLE;
}

static VkResult VKAPI_CALL graphics(VkDevice device, VkPipelineCache cache, uint32_t count,
                                   const VkGraphicsPipelineCreateInfo *info,
                                   const VkAllocationCallbacks *allocator, VkPipeline *pipelines) {
    VkResult result;
    AcquireSRWLockExclusive(&lock);
    ++graphicsCalls;
    result = realGraphics(device, cacheFor(device, cache), count, info, allocator, pipelines);
    ReleaseSRWLockExclusive(&lock);
    return result;
}
static VkResult VKAPI_CALL compute(VkDevice device, VkPipelineCache cache, uint32_t count,
                                  const VkComputePipelineCreateInfo *info,
                                  const VkAllocationCallbacks *allocator, VkPipeline *pipelines) {
    VkResult result;
    AcquireSRWLockExclusive(&lock);
    ++computeCalls;
    result = realCompute(device, cacheFor(device, cache), count, info, allocator, pipelines);
    ReleaseSRWLockExclusive(&lock);
    return result;
}

__declspec(dllexport) DWORD WINAPI AzureCacheStart(void *parameter) {
    HMODULE loader = GetModuleHandleW(L"vulkan-1.dll");
    wchar_t *cachePath = (wchar_t*)parameter;
    if (!parameter || !loader || wcslen((wchar_t*)parameter) >= _countof(path)) return 10;
    if (wcsncmp(cachePath, L"disabled:", 9) == 0) { enabled = 0; cachePath += 9; }
    wcscpy_s(path, _countof(path), cachePath);
    if (MH_Initialize() != MH_OK) return 20;
    if (MH_CreateHook(GetProcAddress(loader, "vkCreateDevice"), createDevice, (LPVOID*)&realDevice) != MH_OK) return 21;
    if (MH_CreateHook(GetProcAddress(loader, "vkDestroyDevice"), destroyDevice, (LPVOID*)&realDestroy) != MH_OK) return 22;
    if (MH_CreateHook(GetProcAddress(loader, "vkCreateGraphicsPipelines"), graphics, (LPVOID*)&realGraphics) != MH_OK) return 23;
    if (MH_CreateHook(GetProcAddress(loader, "vkCreateComputePipelines"), compute, (LPVOID*)&realCompute) != MH_OK) return 24;
    return MH_EnableHook(MH_ALL_HOOKS) == MH_OK ? 0 : 25;
}
