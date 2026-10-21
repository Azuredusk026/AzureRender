#pragma once
#include "editor/EditorRenderResourceState.hpp"
#include <stdexcept>

inline void checkEditorRenderResourceState() {
    using namespace azurerender;
    const auto require=[](bool value,const char* message){
        if(!value)throw std::runtime_error(message);
    };
    SceneDocument scene;
    scene.resources.push_back({"mesh-a","mesh","models/a.glb"});
    SceneNode node;node.id="actor";node.resourceId="mesh-a";
    node.resolvedMaterials["body"]={"materials/body.azmaterial","body-v1",{}};
    scene.nodes.push_back(node);
    EditorRenderResourceState state;
    require(state.needsReload(scene),"An unloaded renderer needs scene resources");
    state.load(scene,[]{});
    require(!state.needsReload(scene),"First synchronization after a successful load must be idle");
    auto changed=scene;changed.resources[0].path="models/b.glb";
    require(state.needsReload(changed),"A changed mesh path must request a reload");
    changed=scene;changed.nodes[0].resourceId="mesh-b";
    require(state.needsReload(changed),"A changed node resource binding must request a reload");
    changed=scene;changed.nodes[0].resolvedMaterials["body"].path="materials/other.azmaterial";
    require(state.needsReload(changed),"A changed material path must request a reload");
    changed=scene;changed.nodes[0].resolvedMaterials["body"].fingerprint="body-v2";
    require(state.needsReload(changed),"Changed material content must request a reload");
    bool loadFailed=false;
    try{state.load(changed,[]{throw std::runtime_error("load failed");});}
    catch(const std::runtime_error&){loadFailed=true;}
    require(loadFailed,"A failed resource load must propagate its diagnostic");
    require(state.needsReload(changed),"A failed load must retain the pending material change");
    require(!state.needsReload(scene),"A failed load must preserve the last successful baseline");
    state.load(changed,[]{});
    require(!state.needsReload(changed),"A successful retry must publish its loaded snapshot");
    changed=scene;changed.nodes[0].translation[0]=5;changed.nodes[0].visible=false;
    state.load(scene,[]{});
    require(!state.needsReload(changed),"Instance transforms and visibility use the per-frame scene path");
    SceneDocument empty;empty.renderSettings.sceneType=SceneType::Blackhole;
    state.load(empty,[]{});
    require(!state.needsReload(empty),"A second renderer without mesh resources has a valid baseline");
    empty.renderSettings.sceneType=SceneType::Sample;
    require(state.needsReload(empty),"Changing the renderer type must request a reload");
    auto duringLoad=scene;
    state.load(duringLoad,[&]{duringLoad.resources[0].path="models/newer.glb";});
    require(state.needsReload(duringLoad),"The baseline must describe the submitted load snapshot");
    require(!state.needsReload(scene),"Later edits must not be published by an earlier load");
    auto first=scene,second=scene;
    first.resources[0].id="mesh-a";first.resources[0].path="bc";
    second.resources[0].id="mesh-ab";second.resources[0].path="c";
    state.load(first,[]{});
    require(state.needsReload(second),"Resource field boundaries must not alias in the signature");
}
