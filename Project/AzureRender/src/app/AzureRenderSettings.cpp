#include "AzureRenderApp.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#if AZURE_WITH_EDITOR
#include "editor/EditorSession.hpp"
#include "editor/EditorWorkspace.hpp"
#endif
void AzureRenderApp::initializeSettings() {
    using namespace azurerender;
#if AZURE_WITH_EDITOR
    if(runOptions_.editorSession)engineSettings_=&runOptions_.editorSession->settings();
    else
#endif
    {registerEngineSettings(playerSettings_);engineSettings_=&playerSettings_;}
    const auto load=[this](const std::string& path,SettingSource source,bool required) {
        if(path.empty())return;
        const auto result=engineSettings_->load(std::filesystem::u8path(path),source);
        if(!result.passed) {
            if(required)throw std::invalid_argument(result.diagnostic);
            RuntimeDiagnostics::instance().info("settings","User settings rejected: "+result.diagnostic);
        }
    };
    load(runOptions_.defaultSettingsFile,SettingSource::DefaultFile,true);
    auto user=runOptions_.settingsFile;
#if AZURE_WITH_EDITOR
    if(user.empty()&&runOptions_.editorSession)user=(EditorWorkspace::configDirectory()/"settings.json").u8string();
#endif
#if AZURE_WITH_EDITOR
    if(runOptions_.editorSession)runOptions_.editorSession->setUserSettingsPath(std::filesystem::u8path(user));
#endif
    load(user,SettingSource::UserFile,false);
    load(runOptions_.projectSettingsFile,SettingSource::Project,true);
    const auto result=engineSettings_->replaceLayer(runOptions_.settingOverrides,SettingSource::CommandLine);
    if(!result.passed)throw std::invalid_argument(result.diagnostic);
    engineSettings_->start();
#if AZURE_WITH_EDITOR
    if(runOptions_.editorSession&&engineSettings_->get("ai.enabled").get<bool>()){
        ModelProcessOptions process;
        process.executable=std::filesystem::u8path(runOptions_.aiPython);
        const auto tool=ResourceLocator().publicAsset("test_model.gltf").parent_path().parent_path()/"tools/ai_bridge.py";
        process.arguments={tool.u8string()};
        if(!runOptions_.aiConfig.empty()){process.arguments.push_back("--config");process.arguments.push_back(std::filesystem::absolute(std::filesystem::u8path(runOptions_.aiConfig)).u8string());}
        if(!runOptions_.aiFixture.empty()){process.arguments.push_back("--fixture");process.arguments.push_back(std::filesystem::absolute(std::filesystem::u8path(runOptions_.aiFixture)).u8string());}
        process.workingDirectory=tool.parent_path();
        runOptions_.editorSession->configureModel(std::make_shared<ModelProcess>(std::move(process)));
    }
#endif
}
