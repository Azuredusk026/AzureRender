#include "WindowIcon.hpp"
#include <GLFW/glfw3.h>
#include <stdexcept>
namespace azurerender {
void setWindowIcon(GLFWwindow* window,int size,const std::vector<unsigned char>& rgba){
    if(!window||size<1||size>256||rgba.size()!=static_cast<std::size_t>(size)*size*4)throw std::invalid_argument("Invalid window icon");
    GLFWimage image{size,size,const_cast<unsigned char*>(rgba.data())};glfwSetWindowIcon(window,1,&image);
}
}
