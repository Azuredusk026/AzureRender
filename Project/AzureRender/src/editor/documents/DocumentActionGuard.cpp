#include "DocumentActionGuard.hpp"
#include <exception>
namespace azurerender {
DocumentActionState DocumentActionGuard::request(DocumentAction action){
    if(state_==DocumentActionState::AwaitingDecision || state_==DocumentActionState::Failed)return state_;
    action_=action;diagnostic_.clear();
    return state_=dirty_()?DocumentActionState::AwaitingDecision:DocumentActionState::Ready;
}
DocumentActionState DocumentActionGuard::resolve(DocumentDecision decision){
    if(state_!=DocumentActionState::AwaitingDecision && state_!=DocumentActionState::Failed)return state_;
    if(decision==DocumentDecision::Cancel){reset();return state_;}
    if(decision==DocumentDecision::Discard)return state_=DocumentActionState::Ready;
    try{save_();diagnostic_.clear();return state_=DocumentActionState::Ready;}
    catch(const std::exception& error){diagnostic_=error.what();return state_=DocumentActionState::Failed;}
}
}
