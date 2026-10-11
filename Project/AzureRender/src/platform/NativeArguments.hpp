#pragma once
#include <string>
#include <vector>
namespace azurerender {
// Includes the executable name; all returned strings use UTF-8.
std::vector<std::string> nativeArguments(int argc,char** argv);
}
