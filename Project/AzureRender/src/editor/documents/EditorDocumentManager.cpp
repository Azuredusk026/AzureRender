#include "EditorDocumentManager.hpp"
namespace azurerender {
std::string EditorDocumentManager::add(Document document){
    if(!document.context||!document.edits||!document.selection||!document.gizmo)throw std::invalid_argument("Document services must be complete");
    const auto existing=find(document.context->scenePath());if(!existing.empty())return existing;
    const auto id=document.context->documentId();
    if(documents_.size()>=32)throw std::invalid_argument("Document workspace capacity is 32");
    if(!documents_.emplace(id,std::move(document)).second)throw std::invalid_argument("Duplicate document identity");return id;
}
EditorDocumentManager::Document& EditorDocumentManager::at(const std::string& id){return const_cast<Document&>(static_cast<const EditorDocumentManager&>(*this).at(id));}
const EditorDocumentManager::Document& EditorDocumentManager::at(const std::string& id) const{
    const auto found=documents_.find(id);if(found==documents_.end())throw std::invalid_argument("Unknown open document: "+id);return found->second;
}
std::string EditorDocumentManager::find(const std::filesystem::path& path) const{
    const auto normalized=std::filesystem::weakly_canonical(path);
    for(const auto& [id,document]:documents_)if(std::filesystem::weakly_canonical(document.context->scenePath())==normalized)return id;return {};
}
nlohmann::json EditorDocumentManager::describe(const std::string& active,bool versions) const{
    auto result=nlohmann::json::array();for(const auto& [id,document]:documents_){nlohmann::json row={{"id",id},{"path",document.context->scenePath().u8string()},{"active",id==active},{"dirty",document.context->dirty()},{"undoCount",document.context->undoCount()},{"redoCount",document.context->redoCount()}};if(versions)row["version"]=document.context->documentVersion().describe();result.push_back(std::move(row));}return result;
}
void EditorDocumentManager::save(const std::string& id){at(id).context->save();}
void EditorDocumentManager::erase(const std::string& id){at(id);documents_.erase(id);}
bool EditorDocumentManager::dirtyExcept(const std::string& id) const{for(const auto& [other,document]:documents_)if(other!=id&&document.context->dirty())return true;return false;}
void EditorDocumentManager::saveAll(){for(auto& [id,document]:documents_)if(document.context->dirty())document.context->save();}
}
