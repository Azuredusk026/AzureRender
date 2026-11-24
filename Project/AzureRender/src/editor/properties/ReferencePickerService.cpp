#include "ReferencePickerService.hpp"
#include "editor/commands/EditContracts.hpp"
namespace azurerender {
void ReferencePickerService::begin(const EditorContext& context,const nlohmann::json& args) {
    ReferencePickRequest value;value.version=context.documentVersion();value.owners=args.at("nodes").get<std::vector<std::string>>();
    value.type=args.at("type").get<std::string>();value.field=args.at("field").get<std::string>();
    const auto& property=PropertyEditorRegistry::field(value.type,value.field);
    const auto fields=PropertyEditorRegistry::selection(context,value.owners,value.type);
    if(property.reference.empty()||!fields.contains(value.field)||!fields.at(value.field).at("editable").get<bool>())throw EditRejection("Reference field is not writable for the captured owners");
    value.kind=property.reference;value.assetTypes=property.assetTypes;request_=std::move(value);
}
void ReferencePickerService::deliver(EditorContext& candidate,const DocumentVersion& version,const std::string& kind,const std::string& id) {
    if(!request_)throw EditRejection("No reference picker is active");
    if(request_->version!=version){cancel();throw EditRejection("Reference picker belongs to an expired document version");}
    if(kind!=request_->kind)throw EditRejection("Reference candidate has an incompatible kind");
    PropertyEditorRegistry::apply(candidate,{{"nodes",request_->owners},{"type",request_->type},{"field",request_->field},{"value",id}});
    cancel();
}
}
