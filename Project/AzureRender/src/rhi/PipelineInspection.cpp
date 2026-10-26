#include "rhi/PipelineInspection.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <nlohmann/json.hpp>

namespace azurerender::rhi {

PipelineInspection::PipelineInspection(VkDevice device, bool supported, bool requested,
                                     PFN_vkGetPipelineExecutablePropertiesKHR query,
                                     PipelineStatisticsOptions statistics) noexcept
    : device_(device), query_(query),statistics_(statistics.query) {
    report_.supported = supported;
    report_.requested = requested;
    report_.entryPointAvailable = query != nullptr;
    report_.enabled = supported && requested && query && device;
    report_.statisticsRequested=statistics.requested;
    report_.statisticsEntryPointAvailable=statistics.query!=nullptr;
    report_.statisticsEnabled=report_.enabled&&statistics.requested&&statistics.query;
}

void PipelineInspection::inspectStatistics(VkPipeline pipeline,PipelineInspectionRecord& record) noexcept {
    try {
        for(std::uint32_t index=0;index<record.executableCount;++index){
            if(record.statistics.size()==64){record.statisticsTruncated=true;break;}
            VkPipelineExecutableInfoKHR info{VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR};
            info.pipeline=pipeline;info.executableIndex=index;
            std::uint32_t count=0;auto result=statistics_(device_,&info,&count,nullptr);
            if(result==VK_SUCCESS&&count){
                std::array<VkPipelineExecutableStatisticKHR,16> values{};
                const auto capacity=std::min<std::uint32_t>({count,16,static_cast<std::uint32_t>(64-record.statistics.size())});
                record.statisticsTruncated|=count>capacity;count=capacity;
                for(auto& value:values)value={VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR};
                result=statistics_(device_,&info,&count,values.data());
                if((result==VK_SUCCESS||result==VK_INCOMPLETE)&&count<=capacity){
                    record.statisticsTruncated|=result==VK_INCOMPLETE;
                    for(std::uint32_t i=0;i<count;++i){
                        auto value=values[i];value.pNext=nullptr;
                        value.name[VK_MAX_DESCRIPTION_SIZE-1]='\0';
                        value.description[VK_MAX_DESCRIPTION_SIZE-1]='\0';
                        record.statistics.push_back({index,value});
                    }
                    continue;
                }
                if(count>capacity)result=VK_ERROR_UNKNOWN;
            }
            if(result!=VK_SUCCESS){++report_.statisticsFailed;report_.statisticsLastResult=result;}
        }
    }catch(...){++report_.statisticsFailed;report_.statisticsLastResult=VK_ERROR_OUT_OF_HOST_MEMORY;}
}

void PipelineInspection::inspect(VkPipeline pipeline,VkPipelineCreateFlags flags) noexcept {
    if (!report_.enabled) { ++report_.skipped; return; }
    VkResult result = VK_ERROR_UNKNOWN;
    if (pipeline) {
        VkPipelineInfoKHR info{VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR};
        info.pipeline = pipeline;
        PipelineInspectionRecord record;
        for (unsigned attempt = 0; attempt < 2; ++attempt) {
            std::uint32_t count = 0;
            result = query_(device_, &info, &count, nullptr);
            if (result != VK_SUCCESS) break;
            if (!count || count > record.executables.size()) { result = VK_ERROR_UNKNOWN; break; }
            for (auto& executable : record.executables)
                executable = {VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR};
            const auto capacity = count;
            result = query_(device_, &info, &count, record.executables.data());
            if (result == VK_INCOMPLETE) continue;
            if (result != VK_SUCCESS) break;
            if (!count || count > capacity) { result = VK_ERROR_UNKNOWN; break; }
            record.executableCount = count;
            for (std::uint32_t i = 0; i < count; ++i) {
                // No driver-owned pointer enters the durable report.
                record.executables[i].pNext = nullptr;
                record.executables[i].name[VK_MAX_DESCRIPTION_SIZE - 1] = '\0';
                record.executables[i].description[VK_MAX_DESCRIPTION_SIZE - 1] = '\0';
            }
            if (report_.recordCount < 64) {
                try {
                    record.statisticsCaptured=report_.statisticsEnabled&&(flags&VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR);
                    if(record.statisticsCaptured)inspectStatistics(pipeline,record);
                    if (report_.records.empty()) report_.records.reserve(64);
                    report_.records.push_back(std::move(record));
                    ++report_.recordCount;
                } catch (...) { result = VK_ERROR_OUT_OF_HOST_MEMORY; break; }
            }
            ++report_.inspected;
            return;
        }
    }
    ++report_.failed;
    report_.lastResult = result;
}

std::string PipelineInspection::jsonReport() const {
    nlohmann::json records = nlohmann::json::array();
    for (std::size_t i = 0; i < report_.recordCount; ++i) {
        nlohmann::json executables = nlohmann::json::array();
        const auto& record = report_.records[i];
        for (std::uint32_t j = 0; j < record.executableCount; ++j) {
            const auto& item = record.executables[j];
            auto statistics=nlohmann::json::array();
            for(const auto& entry:record.statistics)if(entry.executableIndex==j){
                const auto& stat=entry.statistic;nlohmann::json value=nullptr;const char* format="unsupported";
                switch(stat.format){
                case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_BOOL32_KHR:format="bool32";
                    if(stat.value.b32<=VK_TRUE)value=stat.value.b32==VK_TRUE;break;
                case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_INT64_KHR:format="int64";value=stat.value.i64;break;
                case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR:format="uint64";value=stat.value.u64;break;
                case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_FLOAT64_KHR:format="float64";
                    if(std::isfinite(stat.value.f64))value=stat.value.f64;break;
                default:break;
                }
                statistics.push_back({{"name",stat.name},{"description",stat.description},{"format",format},{"value",value}});
            }
            executables.push_back({{"name", item.name}, {"description", item.description},
                                   {"stages", item.stages}, {"subgroupSize", item.subgroupSize},{"statistics",statistics}});
        }
        records.push_back({{"executables", executables},{"statisticsCaptured",record.statisticsCaptured},
            {"statisticsTruncated",record.statisticsTruncated}});
    }
    return nlohmann::json{{"supported", report_.supported}, {"requested", report_.requested},
        {"entryPointAvailable", report_.entryPointAvailable}, {"enabled", report_.enabled},
        {"inspected", report_.inspected}, {"failed", report_.failed}, {"skipped", report_.skipped},
        {"lastResult", report_.lastResult},{"statisticsEnabled",report_.statisticsEnabled},
        {"statisticsRequested",report_.statisticsRequested},{"statisticsEntryPointAvailable",report_.statisticsEntryPointAvailable},
        {"statisticsFailed",report_.statisticsFailed},{"statisticsLastResult",report_.statisticsLastResult},
        {"records", records}}.dump();
}

} // namespace azurerender::rhi
