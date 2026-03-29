#include "HttpRequest.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace miniCDN{

bool HttpRequest::parse(const std::string &rawData){
    std::istringstream stream(rawData);
    std::string line;

    // 1. Parse Request Line ("GET /index.html HTTP/1.1")
    if(std::getline(stream, line)){
        if(!line.empty() && line.back() == '\r') line.pop_back();

        std::istringstream lineStream(line);
        lineStream >> m_method >> m_url >> m_version;

        if(m_method.empty() || m_url.empty()) return false;
    }
    else return false;


    // 2. Parse Headers
    while(std::getline(stream, line)){
        if(!line.empty() && line.back() == '\r') line.pop_back();
        if(line.empty()) break; // end of headers

        size_t colonPos = line.find(':');
        if(colonPos != std::string::npos){
            std::string key = trim(line.substr(0, colonPos));
            std::string value = trim(line.substr(colonPos + 1));
            m_headers[key] = value;
        }
    }

    return true;
}

std::string HttpRequest::getHeader(const std::string &key) const{
    if(m_headers.count(key)){
        return m_headers.at(key);
    }
    return "";
}

std::string HttpRequest::getHost() const{
    // 1. check if URL is absolute (http://google.com/...)
    size_t protocolPos = m_url.find("://");
    if(protocolPos != std::string::npos){
        size_t hostStart = protocolPos + 3;
        size_t pathStart = m_url.find('/', hostStart);
        if(pathStart == std::string::npos){
            return m_url.substr(hostStart);
        }
        return m_url.substr(hostStart, pathStart - hostStart);
    }

    // 2. check host header
    return getHeader("Host");
}

std::string HttpRequest::trim(const std::string &str){
    size_t first = str.find_first_not_of(" \t\r\n");
    if(first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

void HttpRequest::printInfo() const{
    std::cout << "--- Parsed Request ---\n";
    std::cout << "Method: " << m_method << "\n";
    std::cout << "Target URL: " << m_url << "\n";
    std::cout << "Detected Host: " << getHost() << "\n";
    std::cout << "----------------------\n";
}

void HttpRequest::setHeader(const std::string &key, const std::string &value){
    m_headers[key] = value;
}

std::string HttpRequest::toString() const{
    std::ostringstream oss;

    std::string path = m_url;
    size_t protocolPos = path.find("://");
    if(protocolPos != std::string::npos){
        size_t pathStart = path.find('/', protocolPos + 3);
        path = (pathStart != std::string::npos) ? path.substr(pathStart) : "/";
    }

    oss << m_method << " " << path << " " << m_version << "\r\n";

    for(const auto &pair : m_headers){
        oss << pair.first << ": " << pair.second << "\r\n";
    }

    oss << "\r\n";
    return oss.str();
}

} // namespace miniCDN