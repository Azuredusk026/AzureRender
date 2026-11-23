#include "runtime/GameUi.hpp"
#include "assets/Scene2D.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <utility>
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
// RmlUi's file contract receives UTF-8 paths from project documents.
struct Files: Rml::FileInterface {
    std::filesystem::path documentDirectory;
    Rml::FileHandle Open(const Rml::String& path) override {
        auto file = std::filesystem::u8path(path);
        if (file.is_relative() && !documentDirectory.empty()) {
            const auto relative = documentDirectory / file;
            if (std::filesystem::is_regular_file(relative)) file = relative;
        }
#ifdef _WIN32
        return reinterpret_cast<Rml::FileHandle>(_wfopen(file.c_str(),L"rb"));
#else
        return reinterpret_cast<Rml::FileHandle>(std::fopen(file.c_str(),"rb"));
#endif
    }
    void Close(Rml::FileHandle file) override {std::fclose(reinterpret_cast<FILE*>(file));}
    size_t Read(void* buffer,size_t size,Rml::FileHandle file) override {return std::fread(buffer,1,size,reinterpret_cast<FILE*>(file));}
    bool Seek(Rml::FileHandle file,long offset,int origin) override {return std::fseek(reinterpret_cast<FILE*>(file),offset,origin)==0;}
    size_t Tell(Rml::FileHandle file) override {return std::ftell(reinterpret_cast<FILE*>(file));}
} fileInterface;
struct DocumentDirectoryScope {
    explicit DocumentDirectoryScope(const std::filesystem::path& directory)
        : previous(std::exchange(fileInterface.documentDirectory, directory)) {}
    ~DocumentDirectoryScope() { fileInterface.documentDirectory = std::move(previous); }
    std::filesystem::path previous;
};
unsigned users=0,contextCounter=0;
}
struct GameUi::Impl: Rml::EventListener {
    Rml::Context* context=nullptr;
    Rml::ElementDocument* document=nullptr;
    std::function<void(std::string)> action;
    bool down=false,registered=false;
    Impl(const std::filesystem::path& path,const std::filesystem::path& font,Rml::RenderInterface& renderer){
        if(!std::filesystem::is_regular_file(path)||!std::filesystem::is_regular_file(font))throw std::runtime_error("Game UI document or font missing");
        if(users==0){Rml::SetSystemInterface(&systemInterface);Rml::SetRenderInterface(&renderer);Rml::SetFileInterface(&fileInterface);if(!Rml::Initialise())throw std::runtime_error("RmlUi initialization failed");}
        ++users;registered=true;
        try{
            if(!Rml::LoadFontFace(font.generic_u8string()))throw std::runtime_error("Game UI font failed to load");
            if(path.extension()==".azure2d"){
                if(!Rml::LoadFontFace(font.generic_u8string(),"AzureGame",Rml::Style::FontStyle::Normal))throw std::runtime_error("2D font failed to load");
            }
            const auto cjk=font.parent_path()/"NotoSansCJKsc-Regular.otf";
            if(std::filesystem::is_regular_file(cjk)&&!Rml::LoadFontFace(cjk.generic_u8string(),true))throw std::runtime_error("Chinese fallback font failed to load");
            context=Rml::CreateContext("azure-ui-"+std::to_string(++contextCounter),{640,360},&renderer);
            if(!context)throw std::runtime_error("Game UI context failed to initialize");
            const DocumentDirectoryScope documentDirectory(path.parent_path());
            document=path.extension()==".azure2d"?context->LoadDocumentFromMemory(Scene2D::load(path).toRml(),Rml::StringUtilities::Replace(path.generic_u8string(), ':', '|')):context->LoadDocument(path.generic_u8string());if(!document)throw std::runtime_error("Game UI document failed to load");
            document->AddEventListener("click",this);
            document->AddEventListener("blur",this,true);
            document->Show();
        }catch(...){clear();throw;}
    }
    void clear(){if(context){Rml::RemoveContext(context->GetName());context=nullptr;document=nullptr;}if(registered){registered=false;if(--users==0)Rml::Shutdown();}}
    ~Impl(){clear();}
    void ProcessEvent(Rml::Event& event) override {
        const auto* attribute=event.GetType()=="blur"?"data-action-on-blur":"data-action";
        for(auto* target=event.GetTargetElement();target;target=target->GetParentNode()){
            const auto command=target->GetAttribute<Rml::String>(attribute,"");if(!command.empty()){if(action)action(command);break;}
        }
    }
    Rml::Element* element(const std::string& id)const{auto* value=document->GetElementById(id);if(!value)throw std::invalid_argument("Unknown game UI element: "+id);return value;}
};
GameUi::GameUi(const std::filesystem::path& document,const std::filesystem::path& font,Rml::RenderInterface& renderer):impl_(std::make_unique<Impl>(document,font,renderer)){}
GameUi::~GameUi()=default;
void GameUi::setImage(const std::string& id,const std::filesystem::path& image){
    auto* element=impl_->element(id);
    if(element->GetTagName()!="img")throw std::invalid_argument("Expected a game UI image: "+id);
    if(image.empty()){element->RemoveAttribute("src");return;}
    if(!std::filesystem::is_regular_file(image))throw std::invalid_argument("Game UI image is unavailable");
    auto source=image.generic_u8string();
#ifdef _WIN32
    // RmlUi normalizes URLs. Keep the Win32 namespace prefix at the native
    // decoder boundary, where it cannot be collapsed into a relative URL.
    if(source.rfind("//?/UNC/",0)==0)source="//"+source.substr(8);
    else if(source.rfind("//?/",0)==0)source=source.substr(4);
#endif
    if(element->GetAttribute<Rml::String>("src","")!=source)element->SetAttribute("src",source);
}
void GameUi::resize(int width,int height,float density){if(width<=0||height<=0||!std::isfinite(density)||density<=0)throw std::invalid_argument("Invalid game UI dimensions");impl_->context->SetDimensions({width,height});impl_->context->SetDensityIndependentPixelRatio(density);}
void GameUi::setText(const std::string& id,const std::string& text){auto* element=impl_->element(id);const auto encoded=Rml::StringUtilities::EncodeRml(text);if(element->GetInnerRML()!=encoded)element->SetInnerRML(encoded);}
std::string GameUi::text(const std::string& id)const{return impl_->element(id)->GetInnerRML();}
std::string GameUi::formValue(const std::string& id) const {
    auto* control = dynamic_cast<Rml::ElementFormControl*>(impl_->element(id));
    if (!control) throw std::invalid_argument("Expected a game UI form control: " + id);
    return control->GetValue();
}
void GameUi::setFormValue(const std::string& id, const std::string& value) {
    auto* control = dynamic_cast<Rml::ElementFormControl*>(impl_->element(id));
    if (!control) throw std::invalid_argument("Expected a game UI form control: " + id);
    if (control->GetValue() != value) control->SetValue(value);
}
void GameUi::setVisible(const std::string& id, bool visible) {
    auto* element = impl_->element(id);
    if (visible) element->RemoveProperty("display");
    else element->SetProperty("display", "none");
}
std::array<float,4> GameUi::bounds(const std::string& id)const{auto* element=impl_->element(id);const auto offset=element->GetAbsoluteOffset(Rml::BoxArea::Border);const auto size=element->GetBox().GetSize(Rml::BoxArea::Border);return {offset.x,offset.y,size.x,size.y};}
void GameUi::setActionHandler(std::function<void(std::string)> handler){impl_->action=std::move(handler);}
bool GameUi::wantsPointer(int x,int y)const{
    for(auto* element=impl_->context->GetElementAtPoint({static_cast<float>(x),static_cast<float>(y)});element;element=element->GetParentNode())
        if(element->GetTagName()=="button"||element->GetTagName()=="input"||element->GetTagName()=="textarea"
            ||element->GetTagName()=="select"||element->HasAttribute("data-action"))return true;
    return false;
}
bool GameUi::pointer(int x,int y,bool down){impl_->context->ProcessMouseMove(x,y,0);if(down!=impl_->down){if(down)impl_->context->ProcessMouseButtonDown(0,0);else impl_->context->ProcessMouseButtonUp(0,0);impl_->down=down;}return wantsPointer(x,y);}
void GameUi::update(double delta){if(!std::isfinite(delta)||delta<0)throw std::invalid_argument("Invalid game UI delta");systemInterface.time+=delta;impl_->context->Update();}
void GameUi::render(){impl_->context->Render();}
bool GameUi::wantsKeyboard()const{auto* element=impl_->context->GetFocusElement();return element&&(element->GetTagName()=="input"||element->GetTagName()=="textarea"||element->GetTagName()=="select");}
void GameUi::key(int key, bool down) {
    using namespace Rml::Input;
    KeyIdentifier code = KI_UNKNOWN;
    switch (key) {
    case 32: code = KI_SPACE; break;
    case 257: code = KI_RETURN; break;
    case 258: code = KI_TAB; break;
    case 259: code = KI_BACK; break;
    case 261: code = KI_DELETE; break;
    case 262: code = KI_RIGHT; break;
    case 263: code = KI_LEFT; break;
    case 264: code = KI_DOWN; break;
    case 265: code = KI_UP; break;
    case 268: code = KI_HOME; break;
    case 269: code = KI_END; break;
    case 335: code = KI_NUMPADENTER; break;
    }
    if (code == KI_RETURN || code == KI_NUMPADENTER) {
        if (auto* element = impl_->context->GetFocusElement()) {
            const auto command = element->GetAttribute<Rml::String>("data-action-on-blur", "");
            if (!command.empty()) {
                if (down && impl_->action) impl_->action(command);
                return;
            }
        }
    }
    if (down) {
        impl_->context->ProcessKeyDown(code, 0);
        // GLFW does not produce a character event for Enter.
        if (code == KI_RETURN || code == KI_NUMPADENTER) character(U'\n');
    } else {
        impl_->context->ProcessKeyUp(code, 0);
    }
}
void GameUi::character(char32_t value){impl_->context->ProcessTextInput(static_cast<Rml::Character>(value));}
} // namespace azurerender
