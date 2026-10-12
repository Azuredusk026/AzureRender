#include "assets/AssetTransfer.hpp"
#include "assets/GltfLoader.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
using namespace azurerender;
int main(){const auto root=std::filesystem::temp_directory_path()/("azure-usda-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));try{
    std::filesystem::create_directories(root);
    const std::string mesh="def Mesh \"Triangle\" { uniform token subdivisionScheme = \"none\" point3f[] points = [(0,0,0),(1,0,0),(0,1,0)] int[] faceVertexCounts = [3] int[] faceVertexIndices = [0,1,2] }";
    for(const auto* axis:{"Y","Z"}){
        const auto source=root/(std::string(axis)+".usda");{std::ofstream file(source);file<<"#usda 1.0\n( metersPerUnit = 2 upAxis = \""<<axis<<"\" )\n"<<mesh;}
        AssetTransferRequest request;request.formatId="usda";request.source=source;request.staging=root/(std::string("stage-")+axis);request.destination=root/(std::string("published-")+axis);
        auto report=prepareAssetTransfer(request,assetFormatRegistry(),{});
        const auto imported=loadGltfAsset((report.staging/report.entry).u8string());
        if(imported.indices.size()!=3||imported.vertices.size()!=3||report.sourceFingerprints.size()!=1||!report.sourceFingerprints.count(source))throw std::runtime_error("USDA source must become canonical geometry with original provenance");
        if(std::abs(imported.boundsMax[0]-2.F)>.001F)throw std::runtime_error("USDA units must scale runtime geometry");
        if(std::string(axis)=="Y"&&std::abs(imported.boundsMax[1]-2.F)>.001F)throw std::runtime_error("Y-up geometry must preserve its scaled vertical extent");
        if(std::string(axis)=="Z"&&(std::abs(imported.boundsMin[2]+2.F)>.001F||std::abs(imported.boundsMax[1])>.001F))throw std::runtime_error("Z-up geometry must rotate into engine Y-up space");
        publishAssetTransfer(report);
    }
    {std::ofstream file(root/"bad.usda");file<<"#usda 1.0\ndef Xform \"Unsupported\" {}";}
    AssetTransferRequest bad;bad.formatId="usda";bad.source=root/"bad.usda";bad.staging=root/"bad-stage";bad.destination=root/"bad-output";
    bool rejected=false;try{prepareAssetTransfer(bad,assetFormatRegistry(),{});}catch(const std::exception&){rejected=true;}
    if(!rejected)throw std::runtime_error("Unsupported USD composition must be refused");
    std::filesystem::remove_all(root);std::cout<<"USDA static mesh conversion, two axes, publication and rejection passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}}
