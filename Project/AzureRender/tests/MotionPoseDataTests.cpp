#include "render/MotionPoseData.hpp"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace azurerender;
void checkMotionPose(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class F>void rejectMotionPose(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}checkMotionPose(rejected,"Malformed motion input must be rejected");}
float readMotionFloat(const PackedMotionPoseFrame& data,std::size_t word){float value;const auto bits=data.words.at(word);std::memcpy(&value,&bits,4);return value;}
MotionPoseState objectPose(const char* name,unsigned geometry,float x,unsigned weights){
    MotionPoseState pose;pose.identity.node=name;pose.identity.geometry=geometry;pose.vertexCount=3;
    pose.model=motionIdentityMatrix();pose.model[12]=x;pose.joints={motionIdentityMatrix()};
    pose.morphWeights.resize(weights,.25F);pose.morphTableBase=weights?4U+geometry:0U;
    pose.legacyMorph={.1F,.2F};return pose;
}
int main()try{
    MotionPoseHistory history;MotionPoseView view;view.cameraPosition={1,2,3};
    view.gizmo=motionIdentityMatrix();view.gizmo[14]=.4F;view.selectedPrimitive=2;
    view.outlinePixels=2.25F;
    const auto a=objectPose("node-a",1,10,2),b=objectPose("node-b",2,20,1);
    history.stage({a,b},view);auto first=history.pack(motionIdentityMatrix(),0,4096);
    checkMotionPose(first.words.size()==108&&first.validObjects==0,"First frame contains records without previous-pose reads");
    checkMotionPose(!history.commit(0)&&history.commit(3)&&!history.commit(3),"Only a new successful submission publishes poses");
    auto movedA=a,movedB=b;movedA.model[12]=11;movedB.model[12]=22;
    auto c=objectPose("new-node",1,30,0);history.stage({movedB,movedA,c},{});
    auto previousView=motionIdentityMatrix();previousView[0]=2;
    const auto packed=history.pack(previousView,3,4096);
    checkMotionPose(packed.words.size()==183&&packed.validObjects==2,"Only matching previous objects contribute joints and morph weights");
    checkMotionPose(packed.words[0]==1&&packed.words[1]==3&&packed.words[2]==28&&packed.words[3]==40,"Versioned header and record stride are explicit");
    checkMotionPose(readMotionFloat(packed,28+12)==20&&readMotionFloat(packed,68+12)==10,"Identity survives instance reordering");
    checkMotionPose(readMotionFloat(packed,28+16+12)==40,"Previous view projection multiplies the previous model");
    checkMotionPose(packed.words[28+35]==1&&packed.words[68+35]==1&&packed.words[108+35]==0,"New objects cannot sample another object's history");
    checkMotionPose(readMotionFloat(packed,4)==1&&readMotionFloat(packed,6)==3&&readMotionFloat(packed,8+14)==.4F&&packed.words[27]==2,"Previous camera and gizmo remain frozen");
    checkMotionPose(readMotionFloat(packed,7)==2.25F,"Previous fixed-pixel outline width remains part of the rendered view");
    checkMotionPose(packed.words[24]==183&&packed.words[25]==148&&packed.words[26]==180,"Absolute addresses cover records, joints and weights");
    checkMotionPose(packed.words[28+32]==148&&packed.words[28+33]==180&&packed.words[68+32]==164&&packed.words[68+33]==181,"Per-object offsets remain independent");
    checkMotionPose(packed.words[28+34]==3&&packed.words[28+38]==1&&packed.words[28+39]==1,
        "GPU records declare vertex, joint and morph weight bounds");
    rejectMotionPose([&]{history.pack(previousView,3,731);});
    checkMotionPose(history.pack(previousView,2,4096).validObjects==0,"Mismatched camera and object submission disables previous reads");
    history.cancel();history.stage({a},{});
    checkMotionPose(history.pack(previousView,3,4096).validObjects==1,"Culling another object does not remove this object's previous pose");
    checkMotionPose(history.commit(19),"Skipped global frames commit exactly one object frame");
    history.stage({b,a},{});checkMotionPose(history.pack(previousView,19,4096).validObjects==1,"Reappearing objects require an actually rendered previous pose");
    history.cancel();auto duplicate=a;duplicate.identity.replica=1;history.stage({a,duplicate},{});
    checkMotionPose(history.pack(previousView,19,4096).validObjects==1,"Explicit replica identities keep repeated nodes independent");
    for(int kind=0;kind<6;++kind){auto invalid=a;
        if(kind==0)invalid.identity.node.clear();if(kind==1)invalid.vertexCount=0;if(kind==2)invalid.joints.clear();
        if(kind==3)invalid.model[1]=std::numeric_limits<float>::quiet_NaN();
        if(kind==4)invalid.morphWeights[0]=std::numeric_limits<float>::infinity();if(kind==5)invalid.morphTableBase=0;
        rejectMotionPose([&]{history.stage({invalid},{});});
    }
    rejectMotionPose([&]{history.stage({a,a},{});});
    auto invalidView=view;invalidView.cameraPosition[0]=std::numeric_limits<float>::quiet_NaN();
    rejectMotionPose([&]{history.stage({a},invalidView);});
    checkMotionPose(history.commit(20),"Invalid stages preserve the preceding valid pending frame");
    auto topology=a;topology.vertexCount=4;history.stage({topology},{});
    checkMotionPose(history.pack(previousView,20,4096).validObjects==0,"Topology changes invalidate object motion");
    history.clear();history.stage({a},{});checkMotionPose(history.pack(previousView,20,4096).validObjects==0,"Resource recreation drops pose history");
    std::cout<<"Motion pose identity, layout, bounds and transaction checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
