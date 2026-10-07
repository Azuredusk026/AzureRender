#pragma once
#include "validation/ValidationService.hpp"
#include <atomic>
namespace azurerender {
struct ValidationEndpoint { std::string address="127.0.0.1",token;std::uint16_t port=0; };
class ValidationTransport {
public:
    ValidationTransport(ValidationService& service,ValidationEndpoint endpoint);
    ~ValidationTransport();
    ValidationTransport(const ValidationTransport&)=delete;
    ValidationTransport& operator=(const ValidationTransport&)=delete;
    std::uint16_t port() const { return port_; }
    void stop();
private:
    ValidationService& service_;std::string token_;
    std::atomic<bool> stopped_{false};
    std::uintptr_t listener_=0;
    std::uint16_t port_=0;
    std::thread thread_;
    void serve();
    void serveClient(std::uintptr_t client);
};
}
