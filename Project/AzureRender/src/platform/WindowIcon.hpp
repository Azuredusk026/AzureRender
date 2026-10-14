#pragma once
#include <vector>
struct GLFWwindow;
namespace azurerender { void setWindowIcon(GLFWwindow* window,int size,const std::vector<unsigned char>& rgba); }
