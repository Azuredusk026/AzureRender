#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#endif
#include "validation/ValidationTransport.hpp"
#include <stdexcept>
#include <array>
namespace azurerender {
namespace {
#ifdef _WIN32
using Socket=SOCKET;
constexpr Socket invalid=INVALID_SOCKET;
void closeSocket(Socket socket) { closesocket(socket); }
#else
using Socket=int;
constexpr Socket invalid=-1;
void closeSocket(Socket socket) { close(socket); }
#endif
bool readable(Socket socket) {
    fd_set set;FD_ZERO(&set);FD_SET(socket,&set);timeval timeout{0,50000};
    return select(static_cast<int>(socket+1),&set,nullptr,nullptr,&timeout)>0;
}
bool sendLine(Socket socket,const nlohmann::json& value) {
    auto text=value.dump()+"\n";
    while(!text.empty()) {
#ifdef _WIN32
        const auto count=send(socket,text.data(),static_cast<int>(text.size()),0);
#else
        const auto count=send(socket,text.data(),text.size(),MSG_NOSIGNAL);
#endif
        if(count<=0)return false;text.erase(0,static_cast<std::size_t>(count));
    }
    return true;
}
nlohmann::json rejected(const char* code) { return {{"schemaVersion",1},{"passed",false},{"code",code}}; }
}
ValidationTransport::ValidationTransport(ValidationService& service,ValidationEndpoint endpoint)
    :service_(service),token_(std::move(endpoint.token)) {
    if(endpoint.address!="127.0.0.1"||token_.empty()||token_.size()>256)throw std::invalid_argument("Control endpoint requires IPv4 loopback and a nonempty token");
#ifdef _WIN32
    WSADATA data;if(WSAStartup(MAKEWORD(2,2),&data)!=0)throw std::runtime_error("Winsock startup failed");
#endif
    Socket listener=invalid;
    try {
        listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
        if(listener==invalid)throw std::runtime_error("Control socket failed");
#ifdef _WIN32
        BOOL exclusive=TRUE;setsockopt(listener,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<const char*>(&exclusive),sizeof(exclusive));
#endif
        sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=htons(endpoint.port);
        if(bind(listener,reinterpret_cast<sockaddr*>(&address),sizeof(address))!=0||listen(listener,8)!=0)throw std::runtime_error("Control bind failed");
#ifdef _WIN32
        int size=sizeof(address);
#else
        socklen_t size=sizeof(address);
#endif
        if(getsockname(listener,reinterpret_cast<sockaddr*>(&address),&size)!=0)throw std::runtime_error("Control endpoint discovery failed");
        port_=ntohs(address.sin_port);listener_=static_cast<std::uintptr_t>(listener);thread_=std::thread([this]{serve();});
    }catch(...) {
        if(listener!=invalid)closeSocket(listener);
#ifdef _WIN32
        WSACleanup();
#endif
        throw;
    }
}
ValidationTransport::~ValidationTransport() { stop(); }
void ValidationTransport::stop() {
    if(stopped_.exchange(true))return;
    service_.stop();
    if(thread_.joinable())thread_.join();
    closeSocket(static_cast<Socket>(listener_));
#ifdef _WIN32
    WSACleanup();
#endif
}
void ValidationTransport::serve() {
    const auto listener=static_cast<Socket>(listener_);
    struct Worker { std::thread thread;std::shared_ptr<std::atomic<bool>> done; };
    std::array<Worker,8> workers;
    while(!stopped_) {
        if(!readable(listener))continue;
        const auto client=accept(listener,nullptr,nullptr);if(client==invalid)continue;
        Worker* available=nullptr;
        for(auto& worker:workers) {
            if(worker.thread.joinable()&&worker.done->load())worker.thread.join();
            if(!worker.thread.joinable()&&!available)available=&worker;
        }
        if(!available) { sendLine(client,rejected("ConnectionsFull"));closeSocket(client);continue; }
        try {
            available->done=std::make_shared<std::atomic<bool>>(false);const auto done=available->done;
            available->thread=std::thread([this,client,done] { try { serveClient(static_cast<std::uintptr_t>(client)); }catch(const std::exception&) {}done->store(true); });
        }catch(const std::exception&) { closeSocket(client); }
    }
    for(auto& worker:workers)if(worker.thread.joinable())worker.thread.join();
}
void ValidationTransport::serveClient(std::uintptr_t handle) {
        const auto client=static_cast<Socket>(handle);
        struct OwnedSocket { Socket value;~OwnedSocket(){closeSocket(value);} } owned{client};
#ifdef _WIN32
        DWORD timeout=500;setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
#else
        timeval timeout{0,500000};setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
#endif
        auto start=ValidationService::Clock::now();std::string text;bool connected=true;
        while(!stopped_&&connected) {
            if(ValidationService::Clock::now()-start>std::chrono::seconds(2)) { sendLine(client,rejected("FrameTimeout"));break; }
            if(!readable(client))continue;
            char buffer[4096];const auto count=recv(client,buffer,sizeof(buffer),0);
            if(count<=0)break;text.append(buffer,static_cast<std::size_t>(count));
            if(text.size()>65536) { sendLine(client,rejected("RequestTooLarge"));break; }
            auto newline=text.find('\n');
            while(newline!=std::string::npos&&!stopped_) {
                const auto line=text.substr(0,newline);text.erase(0,newline+1);
                nlohmann::json response;
                try {
                    auto request=nlohmann::json::parse(line);
                    if(request.value("token",std::string())!=token_)response=rejected("Unauthorized");
                    else {
                        request.erase("token");auto future=service_.submit(std::move(request));
                        while(!stopped_&&future.wait_for(std::chrono::milliseconds(20))!=std::future_status::ready) {}
                        if(stopped_)break;
                        response=future.get();response["schemaVersion"]=1;
                    }
                }catch(const std::exception&) { response=rejected("InvalidFrame"); }
                if(!sendLine(client,response)) { connected=false;break; }
                start=ValidationService::Clock::now();newline=text.find('\n');
            }
        }
}
}
