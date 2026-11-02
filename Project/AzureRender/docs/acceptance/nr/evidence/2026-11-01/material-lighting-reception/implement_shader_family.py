import difflib, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[3]
p=root/'shaders/mesh.frag'
old=subprocess.check_output(['git','show','e2c5bbb:Project/AzureRender/shaders/mesh.frag']).decode().splitlines(True)
new=p.read_text().splitlines(True)
parts=[]
for op,a,b,c,d in difflib.SequenceMatcher(None,old,new,autojunk=False).get_opcodes():
    if op=='equal':parts.extend(old[a:b])
    else:
        parts.append('#if defined(AZURE_EXTENDED_MATERIAL_LIGHTING)\n')
        parts.extend(new[c:d])
        if a!=b:
            parts.append('#else\n');parts.extend(old[a:b])
        parts.append('#endif\n')
p.write_text(''.join(parts),newline='\n')
p=root/'CMakeLists.txt';s=p.read_text();key='add_custom_target(AzureRenderShaders ALL DEPENDS ${COMPILED_SHADERS})'
block='''# Explicit material lighting policies have an isolated program family.
get_property(_lighting_base_programs GLOBAL PROPERTY AZURE_SHADER_DESCRIPTORS)
foreach(_lighting_base IN LISTS _lighting_base_programs)
    string(JSON _lighting_source GET "${_lighting_base}" source)
    if(NOT _lighting_source STREQUAL "mesh.frag")
        continue()
    endif()
    string(JSON _lighting_output GET "${_lighting_base}" output)
    string(REGEX REPLACE "\\\\.frag\\\\.spv$" "_lighting.frag" _lighting_output "${_lighting_output}")
    set(_lighting_defines AZURE_EXTENDED_MATERIAL_LIGHTING=1)
    string(JSON _lighting_define_count LENGTH "${_lighting_base}" defines)
    if(_lighting_define_count GREATER 0)
        math(EXPR _lighting_define_last "${_lighting_define_count}-1")
        foreach(_lighting_define_index RANGE 0 ${_lighting_define_last})
            string(JSON _lighting_define GET "${_lighting_base}" defines ${_lighting_define_index})
            list(APPEND _lighting_defines "${_lighting_define}")
        endforeach()
    endif()
    if("AZURE_VIEW_FOG=1" IN_LIST _lighting_defines)
        continue()
    endif()
    compile_shader("${SHADER_SOURCE_DIR}/mesh.frag" DEFINES ${_lighting_defines} OUTPUT_NAME "${_lighting_output}")
endforeach()
'''
assert s.count(key)==1;s=s.replace(key,block+key);p.write_text(s,newline='\n')
p=root/'src/scenes/CharacterMaterialSampling.inc';s=p.read_text();s=s.replace('rhi_->createGraphicsPipeline(', 'createMaterialGraphicsPipeline(')
s=s.replace('color=rhi_->createShaderModule(readBinaryFile(shaderDirectory_+sceneFragmentVariant(', 'color=createMaterialShaderModule(shaderDirectory_+sceneFragmentVariant(')
s=s.replace('snapshot=rhi_->createShaderModule(readBinaryFile(shaderDirectory_+sceneFragmentVariant(', 'snapshot=createMaterialShaderModule(shaderDirectory_+sceneFragmentVariant(')
s=s.replace('probeFieldActive_)));','probeFieldActive_));')
s=s.replace('rhi_->destroyShaderModule(', 'destroyMaterialShaderModule(');p.write_text(s,newline='\n')
p=root/'src/scenes/CharacterSceneRenderer.cpp';s=p.read_text()
s=s.replace('    const auto fragmentCode =\n        mainFragment(', '    const auto materialFragmentPath=shaderDirectory+sceneFragmentVariant(')
# Replace the expression tail only for this declaration.
start=s.index('    const auto materialFragmentPath=')
end=s.index('    const auto outlineVertexCode',start)
fragment=s[start:end].replace(')),true);',')),rasterizationSamples_==VK_SAMPLE_COUNT_4_BIT,irradianceClipmapActive_,probeFieldActive_);')
s=s[:start]+fragment+s[end:]
s=s.replace('    const VkShaderModule fragmentModule =\n        rhi_->createShaderModule(fragmentCode);','    prepareMaterialLightingPipelines();\n    const VkShaderModule fragmentModule = createMaterialShaderModule(materialFragmentPath);')
s=s.replace('pipelines[facing]=rhi_->createGraphicsPipeline(desc);}', 'pipelines[facing]=createMaterialGraphicsPipeline(desc);}')
s=s.replace('auto depthModule=rhi_->createShaderModule(mainFragment((objectShadowPrepared_?', 'auto depthModule=createMaterialShaderModule(shaderDirectory+sceneFragmentVariant((objectShadowPrepared_?',1)
start=s.index('auto depthModule=createMaterialShaderModule(');end=s.index('\n',start)
line=s[start:end].replace(')),true));',')),rasterizationSamples_==VK_SAMPLE_COUNT_4_BIT,irradianceClipmapActive_,probeFieldActive_));')
s=s[:start]+line+s[end:]
s=s.replace('rhi_->destroyShaderModule(depthModule);throw;}\n        rhi_->destroyShaderModule(depthModule);\n        const auto simpleDepthModule', 'destroyMaterialShaderModule(depthModule);throw;}\n        destroyMaterialShaderModule(depthModule);\n        const auto simpleDepthModule',1)
s=s.replace('sceneTintPipelines_[mirrored]=rhi_->createGraphicsPipeline(tintDesc);','sceneTintPipelines_[mirrored]=createMaterialGraphicsPipeline(tintDesc);')
s=s.replace('rhi_->destroyShaderModule(fragmentModule);','destroyMaterialShaderModule(fragmentModule);')
s=s.replace('    destroyMaterialClassPipelines();','    destroyMaterialLightingPipelines();\n    destroyMaterialClassPipelines();')
s=s.replace('commands.bindPipeline(prefill?prefillPipelines', 'const auto depthPipeline=prefill?prefillPipelines',1)
s=s.replace(':overlayDepthPipelines_[surfaceFacingIndex(m.doubleSided,mirrored)]);', ':overlayDepthPipelines_[surfaceFacingIndex(m.doubleSided,mirrored)];\n            commands.bindPipeline(prefill||simpleDepth?depthPipeline:materialLightingPipeline(m,depthPipeline));',1)
s=s.replace('    commands.bindPipeline(pipeline);\n    if (!bindlessTextures_)', '    commands.bindPipeline(materialLightingPipeline(material,pipeline));\n    if (!bindlessTextures_)',1)
p.write_text(s,newline='\n')
print('Installed isolated shader family source')
