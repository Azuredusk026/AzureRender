#include "assets/model/UsdaFormatAdapter.hpp"
#include "assets/AssetTransfer.hpp"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <set>
#include <cctype>
namespace azurerender {namespace {
class Tokens {
    std::vector<std::string> tokens_;std::size_t position_=0;
public:
    explicit Tokens(const std::string& text){
        for(std::size_t i=0;i<text.size();){
            const char c=text[i];if(std::isspace(static_cast<unsigned char>(c))||c==','){++i;continue;}
            if(c=='#'){while(i<text.size()&&text[i]!='\n')++i;continue;}
            if(std::string("[](){}=").find(c)!=std::string::npos){tokens_.emplace_back(1,c);++i;continue;}
            const auto start=i;
            if(c=='"'){++i;while(i<text.size()&&text[i]!='"'){if(text[i]=='\\'){if(++i==text.size())throw std::invalid_argument("Invalid USDA string");}++i;}if(i==text.size())throw std::invalid_argument("Unterminated USDA string");++i;}
            else{while(i<text.size()&&!std::isspace(static_cast<unsigned char>(text[i]))&&std::string(",[](){}=").find(text[i])==std::string::npos)++i;}
            tokens_.push_back(text.substr(start,i-start));if(tokens_.size()>4000000)throw std::invalid_argument("USDA token budget exceeded");
        }
    }
    bool end()const{return position_==tokens_.size();}
    std::string peek()const{return end()?std::string():tokens_[position_];}
    std::string take(){if(end())throw std::invalid_argument("Truncated USDA document");return tokens_[position_++];}
    void expect(const std::string& token){if(take()!=token)throw std::invalid_argument("Unsupported USDA syntax: expected "+token);}
    std::string text(){const auto token=take();if(token.empty()||token.front()!='"')throw std::invalid_argument("USDA string required");return nlohmann::json::parse(token).get<std::string>();}
    double number(){const auto token=take();std::size_t consumed=0;const double value=std::stod(token,&consumed);if(consumed!=token.size()||!std::isfinite(value)||std::abs(value)>1000000)throw std::invalid_argument("Invalid USDA numeric value");return value;}
    std::vector<double> array(){expect("[");std::vector<double> values;while(peek()!="]"){
        if(peek()=="("){take();for(unsigned i=0;i<3;++i)values.push_back(number());expect(")");}else values.push_back(number());
        if(values.size()>3000000)throw std::invalid_argument("USDA array budget exceeded");
    }take();return values;}
};
AssetTransferReport prepare(const AssetTransferRequest& request,const AssetCancellation& cancel){
    if(std::filesystem::file_size(request.source)>std::min<std::uint64_t>(request.maxSourceBytes,32*1024*1024))throw std::invalid_argument("USDA source exceeds budget");
    const auto before=assetTransferFingerprint(request.source,cancel);std::ifstream file(request.source,std::ios::binary);std::ostringstream source;source<<file.rdbuf();if(!file)throw std::runtime_error("USDA read failed");
    const auto content=source.str();if(content.rfind("#usda 1.0",0)!=0)throw std::invalid_argument("USDA 1.0 text required");Tokens tokens(content);
    double units=.01;std::string axis="y";
    if(tokens.peek()=="("){tokens.take();while(tokens.peek()!=")"){
        const auto field=tokens.take();tokens.expect("=");
        if(field=="metersPerUnit"){units=tokens.number();if(units<=0)throw std::invalid_argument("USDA units must be positive");}
        else if(field=="upAxis"){const auto value=tokens.text();if(value!="Y"&&value!="Z")throw std::invalid_argument("Unsupported USDA up axis");axis=value=="Z"?"z":"y";}
        else if(field=="defaultPrim")static_cast<void>(tokens.text());
        else throw std::invalid_argument("Unsupported USDA layer metadata: "+field);
    }tokens.take();}
    std::ostringstream obj;obj<<std::setprecision(9);std::size_t meshes=0,offset=0;
    while(!tokens.end()){
        if(cancel&&cancel())throw std::runtime_error("USDA conversion cancelled");
        tokens.expect("def");tokens.expect("Mesh");const auto name=tokens.text();tokens.expect("{");
        std::vector<double> points,counts,indices,normals;bool polygon=false,left=false;std::set<std::string> fields;
        while(tokens.peek()!="}"){
            auto type=tokens.take();if(type=="uniform"||type=="varying")type=tokens.take();
            const bool array=tokens.peek()=="[";if(array){tokens.take();tokens.expect("]");}
            const auto field=tokens.take();if(!fields.insert(field).second)throw std::invalid_argument("Duplicate USDA mesh field");tokens.expect("=");
            if(field=="points"&&type=="point3f"&&array)points=tokens.array();
            else if(field=="normals"&&type=="normal3f"&&array)normals=tokens.array();
            else if(field=="faceVertexCounts"&&type=="int"&&array)counts=tokens.array();
            else if(field=="faceVertexIndices"&&type=="int"&&array)indices=tokens.array();
            else if(field=="subdivisionScheme"&&type=="token"&&!array){polygon=tokens.text()=="none";}
            else if(field=="orientation"&&type=="token"&&!array){const auto value=tokens.text();if(value!="leftHanded"&&value!="rightHanded")throw std::invalid_argument("Invalid USDA orientation");left=value=="leftHanded";}
            else throw std::invalid_argument("Unsupported USDA mesh field: "+field);
        }tokens.take();
        if(!polygon||points.empty()||points.size()%3||counts.empty()||indices.empty()||(!normals.empty()&&normals.size()!=points.size()))throw std::invalid_argument("USDA requires static polygon meshes and vertex normals");
        obj<<"o mesh_"<<meshes++<<"\n";static_cast<void>(name);
        for(std::size_t i=0;i<points.size();i+=3)obj<<"v "<<points[i]<<' '<<points[i+1]<<' '<<points[i+2]<<'\n';
        for(std::size_t i=0;i<normals.size();i+=3)obj<<"vn "<<normals[i]<<' '<<normals[i+1]<<' '<<normals[i+2]<<'\n';
        std::size_t cursor=0;
        for(double count:counts){if(count<3||count>64||std::floor(count)!=count||count>indices.size()-cursor)throw std::invalid_argument("Invalid USDA face size");
            obj<<'f';for(std::size_t corner=0;corner<static_cast<std::size_t>(count);++corner){const auto position=left?static_cast<std::size_t>(count)-1-corner:corner;const auto index=indices[cursor+position];if(index<0||std::floor(index)!=index||index>=points.size()/3)throw std::invalid_argument("USDA index out of bounds");const auto vertex=offset+static_cast<std::size_t>(index)+1;obj<<' '<<vertex;if(!normals.empty())obj<<"//"<<vertex;}obj<<'\n';cursor+=static_cast<std::size_t>(count);}
        if(cursor!=indices.size())throw std::invalid_argument("USDA topology lengths differ");
        // Mixed normal presence would invalidate global OBJ normal numbering.
        if(meshes>1&&!normals.empty())throw std::invalid_argument("USDA multi-mesh normals require a separate profile");
        offset+=points.size()/3;if(offset>1000000)throw std::invalid_argument("USDA vertex budget exceeded");
    }
    if(!meshes)throw std::invalid_argument("USDA contains no supported meshes");
    const auto temporary=request.staging/(request.source.stem().u8string()+".obj");
    {std::ofstream output(temporary,std::ios::binary);output<<obj.str();if(!output)throw std::runtime_error("USDA intermediate write failed");}
    auto converted=request;converted.source=temporary;converted.formatId="obj";
    if(!converted.settings.contains("unitScale"))converted.settings["unitScale"]=units;
    if(!converted.settings.contains("upAxis"))converted.settings["upAxis"]=axis;
    auto report=assetFormatRegistry().require("obj").prepare(converted,cancel);std::filesystem::remove(temporary);
    if(before!=assetTransferFingerprint(request.source,cancel))throw std::runtime_error("USDA source changed during conversion");
    report.sourceFingerprints.clear();report.sourceFingerprints[request.source]=before;report.generation.inputHashes.clear();report.generation.inputHashes[request.source.filename().generic_u8string()]=before;
    report.losses={"USDA static polygon geometry and vertex normals become canonical GLB; source mesh names become stable indexed names"};report.summary["adapter"]="azure-usda-static-v1";report.summary["losses"]=report.losses;report.summary["sourceMetersPerUnit"]=units;report.summary["sourceUpAxis"]=axis;
    return report;
}
}
void registerUsdaFormat(AssetFormatRegistry& registry){AssetFormatDescriptor format;format.id="usda";format.label="USD ASCII static mesh";format.extensions={".usda",".usd"};format.canRead=true;format.transformsSource=true;format.features={"static-polygon-mesh","vertex-normals","axis-unit-settings","canonical-glb","strict-profile"};format.probe=[](const auto& bytes){const std::string text(bytes.begin(),bytes.end());return text.rfind("#usda 1.0",0)==0;};format.prepare=prepare;registry.add(std::move(format));}
}
