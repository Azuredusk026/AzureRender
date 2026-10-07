#pragma once
#include <cstddef>
#include <cstdint>
#include <stdexcept>
namespace azurerender {
struct ProposalBudget {
    std::size_t operations=128,sourceBytes=2*1024*1024,historyCount=24,historyBytes=128*1024;
    unsigned repairs=1;
    std::uint32_t timeoutMs=300000;
    void validate() const {
        if(operations<1||operations>128||sourceBytes<1||sourceBytes>2*1024*1024||historyCount<1||historyCount>24
            ||historyBytes<2||historyBytes>128*1024||repairs>1||timeoutMs<1||timeoutMs>300000)
            throw std::invalid_argument("Proposal budget exceeds the validated envelope");
    }
};
}
