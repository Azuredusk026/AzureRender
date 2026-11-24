#include "AppChrome.hpp"
#include <algorithm>
namespace azurerender::ui {
float toolbarGroupStart(float width,float leftEnd,float groupWidth,float rightWidth){
    return std::max(leftEnd,std::min((width-groupWidth)*.5F,width-rightWidth-groupWidth));
}
}
