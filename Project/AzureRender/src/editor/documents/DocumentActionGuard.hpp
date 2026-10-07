#pragma once
#include <functional>
#include <string>
namespace azurerender {
enum class DocumentAction { Close, Reload };
enum class DocumentDecision { Save, Discard, Cancel };
enum class DocumentActionState { Idle, AwaitingDecision, Ready, Failed };
class DocumentActionGuard {
public:
    DocumentActionGuard(std::function<bool()> dirty,std::function<void()> save):dirty_(std::move(dirty)),save_(std::move(save)){}
    DocumentActionState request(DocumentAction action);
    DocumentActionState resolve(DocumentDecision decision);
    DocumentActionState state() const {return state_;}
    DocumentAction action() const {return action_;}
    const std::string& diagnostic() const {return diagnostic_;}
    void reset(){state_=DocumentActionState::Idle;diagnostic_.clear();}
private:
    std::function<bool()> dirty_;
    std::function<void()> save_;
    DocumentActionState state_=DocumentActionState::Idle;
    DocumentAction action_=DocumentAction::Close;
    std::string diagnostic_;
};
}
