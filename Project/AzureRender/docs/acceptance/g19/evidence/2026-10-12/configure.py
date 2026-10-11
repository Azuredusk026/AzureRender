from pathlib import Path
import json
root=Path(__file__).resolve().parents[2]
p=root/'vcpkg.json'
d=json.loads(p.read_text(encoding='utf-8'))
for name,version,port in [('tiff','4.7.2',0),('libwebp','1.6.0',3)]:
    d['dependencies'].append({'name':name,'version>=':version})
    override={'name':name,'version':version}
    if port:override['port-version']=port
    d['overrides'].append(override)
p.write_text(json.dumps(d,indent=2)+'\n',encoding='utf-8')
p=root/'CMakeLists.txt'
s=p.read_text(encoding='utf-8').replace('find_package(assimp 6.0.4 EXACT CONFIG REQUIRED)','find_package(assimp 6.0.4 EXACT CONFIG REQUIRED)\nfind_package(TIFF REQUIRED)\nfind_package(WebP CONFIG REQUIRED)')
s=s.replace('    src/assets/GltfLoader.cpp\n','    src/assets/GltfLoader.cpp\n    src/assets/image/ImageCodec.cpp\n')
s=s.replace('AzureRenderCore PRIVATE assimp::assimp','AzureRenderCore PRIVATE assimp::assimp TIFF::TIFF WebP::webp')
s=s.replace('OpenEXR::OpenEXR)','OpenEXR::OpenEXR TIFF::TIFF WebP::webp)')
s=s.replace('tests/EnvironmentAssetTests.cpp src/render/EnvironmentAsset.cpp','tests/EnvironmentAssetTests.cpp src/render/EnvironmentAsset.cpp src/assets/image/ImageCodec.cpp')
s=s.replace('foreach(_pkg assimp','foreach(_pkg tiff libwebp libjpeg-turbo liblzma libspng assimp')
s=s.replace('AzureImageCodecTests PRIVATE AzureRenderCore)','AzureImageCodecTests PRIVATE AzureRenderCore TIFF::TIFF WebP::webp)\n    target_include_directories(AzureImageCodecTests PRIVATE "${STB_INCLUDE_DIR}")')
p.write_text(s,encoding='utf-8')
