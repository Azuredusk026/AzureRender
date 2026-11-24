#include "rhi/PipelineInspection.hpp"
#include "rhi/PresentationPolicy.hpp"

#include <iostream>
#include <stdexcept>
#include <cstring>
#include <limits>
#include <nlohmann/json.hpp>

using namespace azurerender::rhi;
namespace {
unsigned calls = 0;
unsigned mode = 0;
unsigned statisticCalls=0,statisticMode=0;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
VkResult VKAPI_CALL query(VkDevice, const VkPipelineInfoKHR* info,
                         uint32_t* count, VkPipelineExecutablePropertiesKHR* properties) {
    ++calls;
    require(info->sType == VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR && info->pipeline,
            "Query requires a live borrowed pipeline");
    if (mode == 1) return VK_ERROR_UNKNOWN;
    if (!properties) { *count = mode == 2 ? 0 : mode == 3 ? 33 : mode==7?32:1; return VK_SUCCESS; }
    require(*count == (mode==7?32U:1U) && properties[0].sType == VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR,
            "Properties must have initialized structure types");
    if (mode == 4) return VK_INCOMPLETE;
    if (mode == 6 && calls % 4 == 2) return VK_INCOMPLETE;
    if (mode == 5) { *count = 2; return VK_SUCCESS; }
    for(unsigned i=0;i<*count;++i){
        properties[i].stages = VK_SHADER_STAGE_COMPUTE_BIT;
        properties[i].subgroupSize = 32;
        std::strcpy(properties[i].name, "compute \"main\"");
        std::strcpy(properties[i].description, "driver executable");
    }
    return VK_SUCCESS;
}
VkResult VKAPI_CALL queryStatistics(VkDevice,const VkPipelineExecutableInfoKHR* info,
    uint32_t* count,VkPipelineExecutableStatisticKHR* statistics) {
    ++statisticCalls;
    require(info->sType==VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR&&info->pipeline
        &&info->executableIndex<(mode==7?32U:1U),
        "Statistics must identify their borrowed executable");
    if(statisticMode==1)return VK_ERROR_UNKNOWN;
    if(!statistics){*count=statisticMode==2?100:statisticMode==4?0:4;return VK_SUCCESS;}
    const auto capacity=*count;
    for(unsigned i=0;i<capacity;++i){
        require(statistics[i].sType==VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR,
            "Statistics need initialized structure types");
        require(statistics[i].value.u64==0,"Statistics must initialize the full value storage before driver output");
        std::strcpy(statistics[i].name,"statistic \"value\"");
        std::strcpy(statistics[i].description,"driver-owned compile metric");
        statistics[i].format=static_cast<VkPipelineExecutableStatisticFormatKHR>(i%4);
        statistics[i].pNext=reinterpret_cast<void*>(uintptr_t{1});
        switch(i%4){case 0:statistics[i].value.b32=VK_TRUE;break;case 1:statistics[i].value.i64=-17;break;
            case 2:statistics[i].value.u64=(uint64_t{1}<<40)+9;break;default:statistics[i].value.f64=.125;break;}
    }
    if(statisticMode==5){statistics[0].format=VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_FLOAT64_KHR;
        statistics[0].value.f64=std::numeric_limits<double>::quiet_NaN();}
    if(statisticMode==6)statistics[0].format=VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_MAX_ENUM_KHR;
    if(statisticMode==3){*count=capacity+1;return VK_SUCCESS;}
    *count=capacity;
    return statisticMode==2?VK_INCOMPLETE:VK_SUCCESS;
}
}
int main() {
    try {
        const std::vector<VkPresentModeKHR> modes{VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR};
        require(selectPresentMode(modes, true) == VK_PRESENT_MODE_FIFO_KHR,
                "Synchronized presentation uses FIFO even when MAILBOX is available");
        require(selectPresentMode(modes, false) == VK_PRESENT_MODE_MAILBOX_KHR,
                "Asynchronous presentation prefers MAILBOX");
        require(selectPresentMode({VK_PRESENT_MODE_FIFO_KHR}, false) == VK_PRESENT_MODE_FIFO_KHR,
                "Unsupported MAILBOX falls back to the supported FIFO mode");
        for (const auto& invalid : {std::vector<VkPresentModeKHR>{},
                                   std::vector<VkPresentModeKHR>{VK_PRESENT_MODE_MAILBOX_KHR}}) {
            bool rejected = false;
            try { static_cast<void>(selectPresentMode(invalid, true)); }
            catch (const std::invalid_argument&) { rejected = true; }
            require(rejected, "Invalid surface capabilities cannot select an unadvertised mode");
        }
        const auto device = reinterpret_cast<VkDevice>(uintptr_t{1});
        const auto pipeline = reinterpret_cast<VkPipeline>(uintptr_t{2});
        PipelineInspection unsupported(device, false, true, query);
        unsupported.inspect(pipeline);
        require(calls == 0 && !unsupported.report().enabled && !unsupported.report().supported,
                "Unsupported devices must skip the query");
        PipelineInspection disabled(device, true, false, query);
        disabled.inspect(pipeline);
        require(calls == 0 && disabled.report().supported && !disabled.report().requested,
                "Explicitly disabled inspection must skip queries");
        PipelineInspection missing(device, true, true, nullptr);
        missing.inspect(pipeline);
        require(calls == 0 && !missing.report().enabled && !missing.report().entryPointAvailable,
                "A missing device entry point must be observable");
        PipelineInspection inspector(device, true, true, query);
        inspector.inspect(pipeline);
        require(calls == 2 && inspector.report().inspected == 1 && inspector.report().failed == 0,
                "Supported pipeline must be inspected");
        const auto json = nlohmann::json::parse(inspector.jsonReport());
        require(json.at("records").at(0).at("executables").at(0).at("name") == "compute \"main\"",
                "Metadata must be bounded and JSON escaped");
        inspector.inspect(VK_NULL_HANDLE);
        require(calls == 2 && inspector.report().failed == 1, "Invalid handles must fail before querying");
        for (mode = 1; mode <= 5; ++mode) {
            const auto before = calls;
            PipelineInspection failing(device, true, true, query);
            failing.inspect(pipeline);
            require(failing.report().failed == 1 && failing.report().inspected == 0,
                    "Errors, empty, excessive and changing counts must be reported");
            require(calls - before <= 4, "Enumeration retries must be bounded");
        }
        mode = 0;
        for (unsigned i = 0; i < 100; ++i) inspector.inspect(pipeline);
        require(inspector.report().inspected == 101 && inspector.report().recordCount == 64,
                "Record limits must not prevent later pipeline inspection");
        require(nlohmann::json::parse(inspector.jsonReport()).at("records").size() == 64,
                "Report must remain bounded");
        calls = 0; mode = 6;
        PipelineInspection changing(device, true, true, query);
        changing.inspect(pipeline);
        require(calls == 4 && changing.report().inspected == 1 && changing.report().failed == 0,
                "A changing enumeration must succeed after one bounded retry");
        mode=0;
        require(inspector.creationFlags()==0,"Metadata-only inspection must preserve creation flags");
        PipelineInspection statistics(device,true,true,query,PipelineStatisticsOptions{true,queryStatistics});
        const auto flags=statistics.creationFlags();
        require(flags==VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR,"Compile statistics require explicit capture flags");
        statistics.inspect(pipeline);
        require(statisticCalls==0,"Pipelines without capture flags must retain metadata-only inspection");
        statistics.inspect(pipeline,flags);
        const auto detailed=nlohmann::json::parse(statistics.jsonReport());
        const auto& values=detailed.at("records").at(1).at("executables").at(0).at("statistics");
        require(values.size()==4&&values.at(0).at("value")==true&&values.at(1).at("value")==-17
            &&values.at(2).at("value")==((uint64_t{1}<<40)+9)&&values.at(3).at("value")==.125,
            "All statistic formats must preserve their typed values");
        require(values.at(0).at("name")=="statistic \"value\"","Driver strings must remain JSON escaped");
        require(statistics.report().records.at(1).statistics.at(0).statistic.pNext==nullptr,
            "Durable statistics must never retain driver pointers");
        for(statisticMode=1;statisticMode<=4;++statisticMode){
            statisticCalls=0;PipelineInspection bounded(device,true,true,query,PipelineStatisticsOptions{true,queryStatistics});
            bounded.inspect(pipeline,bounded.creationFlags());
            const auto result=nlohmann::json::parse(bounded.jsonReport());
            require(result.at("inspected")==1&&result.at("failed")==0,"Optional statistics must preserve valid metadata");
            const auto& list=result.at("records").at(0).at("executables").at(0).at("statistics");
            if(statisticMode==2)require(list.size()==16&&result.at("records").at(0).at("statisticsTruncated")==true,
                "Oversized statistic enumeration must remain bounded and observable");
            else if(statisticMode==4)require(list.empty()&&result.at("statisticsFailed")==0,"Empty statistics are legal");
            else require(list.empty()&&result.at("statisticsFailed")==1,"Statistic errors must be independent of metadata errors");
            require(statisticCalls<=2,"Statistic enumeration must have a fixed call budget");
        }
        for(statisticMode=5;statisticMode<=6;++statisticMode){
            PipelineInspection exceptional(device,true,true,query,PipelineStatisticsOptions{true,queryStatistics});
            exceptional.inspect(pipeline,exceptional.creationFlags());
            const auto output=nlohmann::json::parse(exceptional.jsonReport());
            require(output.at("records").at(0).at("executables").at(0).at("statistics").at(0).at("value").is_null(),
                "Unsupported or nonfinite statistics must remain valid JSON");
        }
        mode=7;statisticMode=0;statisticCalls=0;
        PipelineInspection manyExecutables(device,true,true,query,PipelineStatisticsOptions{true,queryStatistics});
        manyExecutables.inspect(pipeline,manyExecutables.creationFlags());
        require(manyExecutables.report().records.at(0).statistics.size()==64&&statisticCalls==32
            &&manyExecutables.report().records.at(0).statisticsTruncated,
            "A pipeline must retain at most 64 statistics with bounded query work");
        PipelineInspection missingStatistics(device,true,true,query,PipelineStatisticsOptions{true,nullptr});
        missingStatistics.inspect(pipeline,missingStatistics.creationFlags());
        require(missingStatistics.report().statisticsRequested&&!missingStatistics.report().statisticsEntryPointAvailable
            &&missingStatistics.report().inspected==1&&missingStatistics.creationFlags()==0,
            "Missing statistic entry points must preserve metadata and expose the request");
        PipelineInspection noStatistics(device,true,false,query,PipelineStatisticsOptions{true,queryStatistics});
        require(noStatistics.creationFlags()==0,"Disabled inspection must keep pipeline creation unchanged");
        std::cout << "Pipeline inspection capability, errors, bounds and borrowed ownership passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
