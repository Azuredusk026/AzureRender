#include <chrono>
#include <iostream>
#include <regex>
int main(){
 const std::string id="12345678-1234-1234-1234-123456789abc";
 const char* pattern="[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}";
 const auto start=std::chrono::steady_clock::now();
 for(int i=0;i<400;++i)if(!std::regex_match(id,std::regex(pattern)))return 1;
 const auto rebuilt=std::chrono::steady_clock::now();
 const std::regex compiled(pattern);
 for(int i=0;i<400;++i)if(!std::regex_match(id,compiled))return 1;
 const auto reused=std::chrono::steady_clock::now();
 std::cout<<"400 rebuilt UUID regex: "<<std::chrono::duration<double,std::milli>(rebuilt-start).count()<<" ms\n";
 std::cout<<"400 reused UUID regex: "<<std::chrono::duration<double,std::milli>(reused-rebuilt).count()<<" ms\n";
}
