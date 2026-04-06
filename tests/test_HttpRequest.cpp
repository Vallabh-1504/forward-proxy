#include <gtest/gtest.h>
#include "HttpRequest.hpp"

// 1. parse()

TEST(HttpRequestParse, ValidGetRequest){
    miniCDN::HttpRequest req;
    std::string raw = "GET /index.html HTTP/1.1\r\nHost: example.com\r\n\r\n";
    EXPECT_TRUE(req.parse(raw));
    EXPECT_EQ(req.getMethod(), "GET");
    EXPECT_EQ(req.getUrl(), "/index.html");
    EXPECT_EQ(req.getHttpVersion(), "HTTP/1.1");
}

TEST(HttpRequestParse, AbsoluteUrlRequest){
    // Browsers configured as proxies send the full URL in the request line
    miniCDN::HttpRequest req;
    std::string raw = "GET http://example.com/path HTTP/1.1\r\nHost: example.com\r\n\r\n";
    EXPECT_TRUE(req.parse(raw));
    EXPECT_EQ(req.getUrl(), "http://example.com/path");
}

TEST(HttpRequestParse, EmptyInputReturnsFalse){
    miniCDN::HttpRequest req;
    EXPECT_FALSE(req.parse(""));
}

TEST(HttpRequestParse, MissingMethodReturnsFalse){
    miniCDN::HttpRequest req;
    // A request line with only whitespace should fail
    EXPECT_FALSE(req.parse("  \r\nHost: example.com\r\n\r\n"));
}

// 2. getHost()

TEST(HttpRequestGetHost, FromAbsoluteUrl){
    miniCDN::HttpRequest req;
    req.parse("GET http://example.com/path HTTP/1.1\r\n\r\n");
    EXPECT_EQ(req.getHost(), "example.com");
}

TEST(HttpRequestGetHost, FromAbsoluteUrlNoPath){
    // URL has no trailing slash
    miniCDN::HttpRequest req;
    req.parse("GET http://example.com HTTP/1.1\r\n\r\n");
    EXPECT_EQ(req.getHost(), "example.com");
}

TEST(HttpRequestGetHost, StripsPortFromUrl){
    // port must be stripped so getaddrinfo gets a clean hostname
    miniCDN::HttpRequest req;
    req.parse("GET http://example.com:8080/path HTTP/1.1\r\n\r\n");
    EXPECT_EQ(req.getHost(), "example.com");
}

TEST(HttpRequestGetHost, FallbackToHostHeader){
    // fallback to get host from header
    miniCDN::HttpRequest req;
    req.parse("GET /path HTTP/1.1\r\nHost: example.com\r\n\r\n");
    EXPECT_EQ(req.getHost(), "example.com");
}

TEST(HttpRequestGetHost, StripsPortFromHostHeader){
    // Strip port from Host Header
    miniCDN::HttpRequest req;
    req.parse("GET /path HTTP/1.1\r\nHost: example.com:9090\r\n\r\n");
    EXPECT_EQ(req.getHost(), "example.com");
}

// 3. getPort()

TEST(HttpRequestGetPort, DefaultsTo80WhenNoPort){
    miniCDN::HttpRequest req;
    req.parse("GET http://example.com/path HTTP/1.1\r\n\r\n");
    EXPECT_EQ(req.getPort(), 80);
}

TEST(HttpRequestGetPort, ReadsPortFromUrl){
    miniCDN::HttpRequest req;
    req.parse("GET http://example.com:8080/path HTTP/1.1\r\n\r\n");
    EXPECT_EQ(req.getPort(), 8080);
}

TEST(HttpRequestGetPort, ReadsPortFromHostHeader){
    miniCDN::HttpRequest req;
    req.parse("GET /path HTTP/1.1\r\nHost: example.com:9090\r\n\r\n");
    EXPECT_EQ(req.getPort(), 9090);
}

// 4. getHeader() — case insensitivity

TEST(HttpRequestGetHeader, CaseInsensitiveLookup){
    // headers are case-insensitive (RFC-7230)
    miniCDN::HttpRequest req;
    req.parse("GET / HTTP/1.1\r\nContent-Type: text/html\r\n\r\n");

    EXPECT_EQ(req.getHeader("Content-Type"), "text/html");
    EXPECT_EQ(req.getHeader("content-type"), "text/html");
    EXPECT_EQ(req.getHeader("CONTENT-TYPE"), "text/html");
}

TEST(HttpRequestGetHeader, MissingHeaderReturnsEmpty){
    miniCDN::HttpRequest req;
    req.parse("GET / HTTP/1.1\r\n\r\n");
    EXPECT_EQ(req.getHeader("Authorization"), "");
}

// 5. setHeader() / removeHeader()

TEST(HttpRequestSetHeader, OverwritesExisting){
    miniCDN::HttpRequest req;
    req.parse("GET / HTTP/1.1\r\nConnection: keep-alive\r\n\r\n");
    req.setHeader("Connection", "close");
    EXPECT_EQ(req.getHeader("connection"), "close");
}

TEST(HttpRequestRemoveHeader, RemovesProxyConnection){
    miniCDN::HttpRequest req;
    req.parse("GET / HTTP/1.1\r\nProxy-Connection: Keep-Alive\r\n\r\n");
    req.removeHeader("Proxy-Connection");
    EXPECT_EQ(req.getHeader("proxy-connection"), "");
}

// 6. toString() — URL rewriting

TEST(HttpRequestToString, AbsoluteUrlRewrittenToRelative){
    // The proxy receives "GET http://example.com/path HTTP/1.1"
    // but must forward "GET /path HTTP/1.1" to the origin server
    miniCDN::HttpRequest req;
    req.parse("GET http://example.com/path HTTP/1.1\r\nHost: example.com\r\n\r\n");
    std::string forwarded = req.toString();

    // The request line must start with the relative path, not the full URL
    EXPECT_TRUE(forwarded.find("GET /path HTTP/1.1") != std::string::npos);
    EXPECT_FALSE(forwarded.find("http://example.com") != std::string::npos);
}

TEST(HttpRequestToString, RelativeUrlUnchanged){
    miniCDN::HttpRequest req;
    req.parse("GET /index.html HTTP/1.1\r\nHost: example.com\r\n\r\n");
    std::string forwarded = req.toString();
    EXPECT_TRUE(forwarded.find("GET /index.html HTTP/1.1") != std::string::npos);
}

