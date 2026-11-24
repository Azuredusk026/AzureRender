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
    PFN_vkGetPipelineExecutablePropertiesKHR executableProperties;
} DeviceCache;
static SRWLOCK lock = SRWLOCK_INIT;
static DeviceCache devices[4];
static wchar_t path[2048];
static uint64_t graphicsCalls, computeCalls, loadedBytes, savedBytes, failedCaches;
static int enabled = 1;
static int inspect = 0;
static uint64_t inspectedPipelines, inspectionErrors, inspectionSupported;
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
    VkDeviceCreateInfo modified = *info;
    VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR feature = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR};
    VkPhysicalDeviceFeatures2 available = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    VkExtensionProperties *extensions = NULL;
    const char **names = NULL;
    uint32_t count = 0, index;
    int supported = 0, alreadyEnabled = 0;
    VkResult result;
    if (inspect && vkEnumerateDeviceExtensionProperties(physical, NULL, &count, NULL) == VK_SUCCESS && count <= 4096) {
        extensions = calloc(count, sizeof(*extensions));
        if (extensions && vkEnumerateDeviceExtensionProperties(physical, NULL, &count, extensions) == VK_SUCCESS)
            for (index = 0; index < count; ++index)
                if (!strcmp(extensions[index].extensionName, VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME)) supported = 1;
        if (supported) {
            available.pNext = &feature;
            vkGetPhysicalDeviceFeatures2(physical, &available);
            supported = feature.pipelineExecutableInfo;
        }
        if (supported) {
            for (index = 0; index < info->enabledExtensionCount; ++index)
                if (!strcmp(info->ppEnabledExtensionNames[index], VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME)) alreadyEnabled = 1;
            names = calloc(info->enabledExtensionCount+1, sizeof(*names));
            if (names) {
                for (index = 0; index < info->enabledExtensionCount; ++index) names[index] = info->ppEnabledExtensionNames[index];
                if (!alreadyEnabled) names[modified.enabledExtensionCount++] = VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME;
                modified.ppEnabledExtensionNames = names;
                feature.pNext = (void*)info->pNext;
                modified.pNext = &feature;
                ++inspectionSupported;
            } else supported = 0;
        }
    }
    result = realDevice(physical, supported ? &modified : info, allocator, device);
    free(names); free(extensions);
    if (result == VK_SUCCESS && (enabled || inspect)) {
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
            if (supported) entry->executableProperties = (PFN_vkGetPipelineExecutablePropertiesKHR)vkGetDeviceProcAddr(*device, "vkGetPipelineExecutablePropertiesKHR");
            if (enabled) {
                data = load(&entry->properties, &bytes);
                cacheInfo.initialDataSize = bytes; cacheInfo.pInitialData = data;
                if (vkCreatePipelineCache(*device, &cacheInfo, NULL, &entry->cache) != VK_SUCCESS) {
                    ++failedCaches;
                    cacheInfo.initialDataSize = 0; cacheInfo.pInitialData = NULL;
                    if (vkCreatePipelineCache(*device, &cacheInfo, NULL, &entry->cache) != VK_SUCCESS)
                        entry->cache = VK_NULL_HANDLE;
                } else loadedBytes += bytes;
                free(data);
            }
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
    fprintf(stderr, "AZURE_DIAGNOSTIC_INSPECTION supported=%llu queried=%llu errors=%llu\n",
            inspectionSupported, inspectedPipelines, inspectionErrors);
    realDestroy(device, allocator);
}

static VkPipelineCache cacheFor(VkDevice device, VkPipelineCache existing) {
    unsigned index;
    if (existing) return existing;
    for (index = 0; index < 4; ++index)
        if (devices[index].device == device) return devices[index].cache;
    return VK_NULL_HANDLE;
}

static void inspectPipelines(VkDevice device, uint32_t count, const VkPipeline *pipelines) {
    uint32_t index, slot;
    if (!inspect) return;
    for (slot = 0; slot < 4; ++slot) {
        DeviceCache *entry = &devices[slot];
        if (entry->device != device || !entry->executableProperties) continue;
        for (index = 0; index < count; ++index) {
            VkPipelineInfoKHR info = {VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR};
            VkPipelineExecutablePropertiesKHR properties[32];
            uint32_t number = 0, item;
            info.pipeline = pipelines[index];
            if (entry->executableProperties(device, &info, &number, NULL) != VK_SUCCESS || number > 32 || !number) {
                ++inspectionErrors; continue;
            }
            memset(properties, 0, sizeof(properties));
            for (item = 0; item < number; ++item) properties[item].sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR;
            if (entry->executableProperties(device, &info, &number, properties) != VK_SUCCESS) ++inspectionErrors;
            else ++inspectedPipelines;
        }
        break;
    }
}

static VkResult VKAPI_CALL graphics(VkDevice device, VkPipelineCache cache, uint32_t count,
                                   const VkGraphicsPipelineCreateInfo *info,
                                   const VkAllocationCallbacks *allocator, VkPipeline *pipelines) {
    VkResult result;
    AcquireSRWLockExclusive(&lock);
    ++graphicsCalls;
    result = realGraphics(device, cacheFor(device, cache), count, info, allocator, pipelines);
    if (result == VK_SUCCESS) inspectPipelines(device, count, pipelines);
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
    if (result == VK_SUCCESS) inspectPipelines(device, count, pipelines);
    ReleaseSRWLockExclusive(&lock);
    return result;
}

__declspec(dllexport) DWORD WINAPI AzureCacheStart(void *parameter) {
    HMODULE loader = GetModuleHandleW(L"vulkan-1.dll");
    wchar_t *cachePath = (wchar_t*)parameter;
    if (!parameter || !loader || wcslen((wchar_t*)parameter) >= _countof(path)) return 10;
    if (wcsncmp(cachePath, L"disabled:", 9) == 0) { enabled = 0; cachePath += 9; }
    if (wcsncmp(cachePath, L"inspect:", 8) == 0) { enabled = 0; inspect = 1; cachePath += 8; }
    wcscpy_s(path, _countof(path), cachePath);
    if (MH_Initialize() != MH_OK) return 20;
    if (MH_CreateHook(GetProcAddress(loader, "vkCreateDevice"), createDevice, (LPVOID*)&realDevice) != MH_OK) return 21;
    if (MH_CreateHook(GetProcAddress(loader, "vkDestroyDevice"), destroyDevice, (LPVOID*)&realDestroy) != MH_OK) return 22;
    if (MH_CreateHook(GetProcAddress(loader, "vkCreateGraphicsPipelines"), graphics, (LPVOID*)&realGraphics) != MH_OK) return 23;
    if (MH_CreateHook(GetProcAddress(loader, "vkCreateComputePipelines"), compute, (LPVOID*)&realCompute) != MH_OK) return 24;
    return MH_EnableHook(MH_ALL_HOOKS) == MH_OK ? 0 : 25;
}
