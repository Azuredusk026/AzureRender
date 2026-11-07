#include "scene/SceneTransformCache.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace azurerender::scene;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void compare(SceneTransformCache& cache,const SceneDescription& scene){
    std::size_t expectedCycles=0,actualCycles=0;
    const auto expected=resolveNodeWorldTransforms(scene,&expectedCycles);
    const auto& actual=cache.resolve(scene,&actualCycles);
    check(expectedCycles==actualCycles&&expected.size()==actual.size(),"Cached topology must preserve complete traversal diagnostics");
    if(!expected.empty())check(std::memcmp(expected.data(),actual.data(),expected.size()*sizeof(expected[0]))==0,
        "Every cached world matrix must match the original arithmetic bytes");
}
int main()try{
    SceneDescription scene;
    for(unsigned index=0;index<100;++index){
        SceneNodeDesc node;node.id="node-"+std::to_string(index);
        if(index)node.parentId="node-"+std::to_string((index-1)/2);
        node.translation={float(index%7)*.1F,-float(index%11)*.2F,float(index%13)*.3F};
        node.rotation={float(index%9),float(index%15),-float(index%17)};
        node.scale={1.F+float(index%3)*.01F,1.F,1.F-float(index%5)*.01F};scene.nodes.push_back(node);
    }
    SceneTransformCache cache;compare(cache,scene);
    check(cache.statistics().worldEvaluations==100&&cache.statistics().topologyRebuilt,"First use must build complete independent transforms");
    compare(cache,scene);check(cache.statistics().localEvaluations==0&&cache.statistics().worldEvaluations==0
        &&cache.statistics().reusedWorld==100,"Identical input must reuse every world matrix");
    scene.nodes[99].translation[0]+=1;compare(cache,scene);
    check(cache.statistics().localEvaluations==1&&cache.statistics().worldEvaluations==1,"A leaf edit must recompute only its own transform");
    scene.nodes[0].rotation[1]+=5;compare(cache,scene);
    check(cache.statistics().localEvaluations==1&&cache.statistics().worldEvaluations==100,"Parent edits must propagate through the complete affected subtree");
    scene.nodes[99].translation[2]=0;compare(cache,scene);scene.nodes[99].translation[2]=-0.F;compare(cache,scene);
    check(cache.statistics().localEvaluations==1,"Signed-zero changes must participate in exact input matching");
    scene.nodes[3].visible=false;scene.nodes[4].resourceId="another-mesh";compare(cache,scene);
    check(cache.statistics().worldEvaluations==0,"Visibility and mesh bindings must remain independent of world transform inputs");
    scene.nodes[8].parentId="missing-parent";compare(cache,scene);
    check(cache.statistics().topologyRebuilt,"Parent identity edits must rebuild the dependency layout");
    std::reverse(scene.nodes.begin(),scene.nodes.end());compare(cache,scene);
    scene.nodes[10].id=scene.nodes[0].id;compare(cache,scene);
    scene.nodes[11].id.clear();compare(cache,scene);
    scene.nodes[12].parentId=scene.nodes[12].id;compare(cache,scene);
    SceneDescription cycle;SceneNodeDesc a,b;a.id="a";b.id="b";a.parentId="b";b.parentId="a";
    a.translation={1,2,3};b.translation={4,5,6};cycle.nodes={a,b};compare(cache,cycle);
    check(cache.statistics().legacyFallback,"Cyclic hierarchies must retain the complete traversal order");
    std::reverse(cycle.nodes.begin(),cycle.nodes.end());compare(cache,cycle);
    compare(cache,scene);compare(cache,scene);check(cache.statistics().reusedWorld==100,"Ordinary use must recover after a cyclic fallback");
    auto nonfinite=scene;nonfinite.nodes[0].translation[0]=std::numeric_limits<float>::quiet_NaN();compare(cache,nonfinite);
    check(cache.statistics().legacyFallback,"Nonfinite input must retain complete arithmetic evaluation");
    compare(cache,scene);compare(cache,scene);check(cache.statistics().reusedWorld==100,"Finite input must recover after nonfinite fallback");
    SceneTransformCache small(1);compare(small,scene);check(small.statistics().legacyFallback,"Oversized layouts must use the bounded fallback");
    scene.nodes.resize(1);compare(cache,scene);compare(cache,scene);check(cache.statistics().reusedWorld==1,"Shrinking layouts must retain only current identities");
    cache.clear();compare(cache,scene);check(cache.statistics().worldEvaluations==1,"Load epochs must own independent cached state");
    scene.nodes.clear();compare(cache,scene);check(cache.statistics().nodes==0,"Empty scenes retain an empty owned result");
    std::cout<<"Exact matrix reuse, hierarchy edits, identities, cycles, nonfinite and bounded fallback passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
