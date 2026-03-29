#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <unordered_map>

namespace miniCDN{

class HttpRequest{

public:
    // main function of the class for abstraction
    bool parse(const std::string &rawData);

    // getters
    std::string getMethod() const { return m_method; }
    std::string getUrl() const { return m_url; }
    std::string getHttpVersion() const { return m_version; }
    std::string getHeader(const std::string &key) const;
    std::string getHost() const;

    void printInfo() const;

    void setHeader(const std::string &key, const std::string &value);
    std::string toString() const;

private:
    std::string m_method;
    std::string m_url;
    std::string m_version;
    std::unordered_map<std::string, std::string> m_headers;

    // trim whitespace
    std::string trim(const std::string &str);

    // lowercase a string, case-insenstitive header
    // because header field names are case-insenstive
    std::string toLower(const std::string &str) const;
};

} // namespace miniCDN

#endif