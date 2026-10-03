#include "runtime/GameUi.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include <RmlUi/Core.h>
#include <cmath>
#include <stdexcept>
namespace azurerender {
namespace {
struct System: Rml::SystemInterface {
    double time=0;
    double GetElapsedTime() override{return time;}
    bool LogMessage(Rml::Log::Type type,const Rml::String& message) override{
        if(type<=Rml::Log::LT_WARNING)RuntimeDiagnostics::instance().warning("game-ui",message);return true;
    }
};
System systemInterface;
unsigned users=0,contextCounter=0;
}
struct GameUi::Impl: Rml::EventListener {
    Rml::Context* context=nullptr;
    Rml::ElementDocument* document=nullptr;
    std::function<void(std::string)> action;
    bool down=false,registered=false;
    Impl(const std::filesystem::path& path,const std::filesystem::path& font,Rml::RenderInterface& renderer){
        if(!std::filesystem::is_regular_file(path)||!std::filesystem::is_regular_file(font))throw std::runtime_error("Game UI document or font missing");
        if(users==0){Rml::SetSystemInterface(&systemInterface);Rml::SetRenderInterface(&renderer);if(!Rml::Initialise())throw std::runtime_error("RmlUi initialization failed");}
        ++users;registered=true;
        try{
            if(!Rml::LoadFontFace(font.generic_string()))throw std::runtime_error("Game UI font failed to load");
            context=Rml::CreateContext("azure-ui-"+std::to_string(++contextCounter),{640,360},&renderer);
            if(!context)throw std::runtime_error("Game UI context failed to initialize");
            document=context->LoadDocument(path.generic_string());if(!document)throw std::runtime_error("Game UI document failed to load");
            document->AddEventListener("click",this);document->Show();
        }catch(...){clear();throw;}
    }
    void clear(){if(context){Rml::RemoveContext(context->GetName());context=nullptr;document=nullptr;}if(registered){registered=false;if(--users==0)Rml::Shutdown();}}
    ~Impl(){clear();}
    void ProcessEvent(Rml::Event& event) override {
        for(auto* target=event.GetTargetElement();target;target=target->GetParentNode()){
            const auto command=target->GetAttribute<Rml::String>("data-action","");if(!command.empty()){if(action)action(command);break;}
        }
    }
    Rml::Element* element(const std::string& id)const{auto* value=document->GetElementById(id);if(!value)throw std::invalid_argument("Unknown game UI element: "+id);return value;}
};
GameUi::GameUi(const std::filesystem::path& document,const std::filesystem::path& font,Rml::RenderInterface& renderer):impl_(std::make_unique<Impl>(document,font,renderer)){}
GameUi::~GameUi()=default;
void GameUi::resize(int width,int height,float density){if(width<=0||height<=0||!std::isfinite(density)||density<=0)throw std::invalid_argument("Invalid game UI dimensions");impl_->context->SetDimensions({width,height});impl_->context->SetDensityIndependentPixelRatio(density);}
void GameUi::setText(const std::string& id,const std::string& text){auto* element=impl_->element(id);const auto encoded=Rml::StringUtilities::EncodeRml(text);if(element->GetInnerRML()!=encoded)element->SetInnerRML(encoded);}
std::string GameUi::text(const std::string& id)const{return impl_->element(id)->GetInnerRML();}
std::array<float,4> GameUi::bounds(const std::string& id)const{auto* element=impl_->element(id);const auto offset=element->GetAbsoluteOffset(Rml::BoxArea::Border);const auto size=element->GetBox().GetSize(Rml::BoxArea::Border);return {offset.x,offset.y,size.x,size.y};}
void GameUi::setActionHandler(std::function<void(std::string)> handler){impl_->action=std::move(handler);}
bool GameUi::pointer(int x,int y,bool down){impl_->context->ProcessMouseMove(x,y,0);if(down!=impl_->down){if(down)impl_->context->ProcessMouseButtonDown(0,0);else impl_->context->ProcessMouseButtonUp(0,0);impl_->down=down;}
    for(auto* element=impl_->context->GetElementAtPoint({static_cast<float>(x),static_cast<float>(y)});element;element=element->GetParentNode())if(element->GetTagName()=="button"||element->GetTagName()=="input")return true;
    return false;
}
void GameUi::update(double delta){if(!std::isfinite(delta)||delta<0)throw std::invalid_argument("Invalid game UI delta");systemInterface.time+=delta;impl_->context->Update();}
void GameUi::render(){impl_->context->Render();}
bool GameUi::wantsKeyboard()const{auto* element=impl_->context->GetFocusElement();return element&&(element->GetTagName()=="input"||element->GetTagName()=="textarea"||element->GetTagName()=="select");}
void GameUi::key(int key,bool down){Rml::Input::KeyIdentifier code=Rml::Input::KI_UNKNOWN;if(key==257)code=Rml::Input::KI_RETURN;else if(key==259)code=Rml::Input::KI_BACK;else if(key==258)code=Rml::Input::KI_TAB;else if(key==263)code=Rml::Input::KI_LEFT;else if(key==262)code=Rml::Input::KI_RIGHT;else if(key==32)code=Rml::Input::KI_SPACE;if(down)impl_->context->ProcessKeyDown(code,0);else impl_->context->ProcessKeyUp(code,0);}
void GameUi::character(char32_t value){impl_->context->ProcessTextInput(static_cast<Rml::Character>(value));}
} // namespace azurerender
