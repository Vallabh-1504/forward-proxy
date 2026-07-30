#include <benchmark/benchmark.h>
#include "HttpRequest.hpp"

// Fixtures (reusable inputs)

// A typical proxy-style request (absolute URL)
static const std::string PROXY_REQUEST =
    "GET http://example.com/index.html HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "User-Agent: Mozilla/5.0\r\n"
    "Accept: text/html\r\n"
    "Proxy-Connection: Keep-Alive\r\n"
    "\r\n";

// A simple direct request (relative URL)
static const std::string DIRECT_REQUEST =
    "GET /index.html HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "\r\n";


// Benchmarks

// How long does parsing the full request take?
static void BM_Parse_ProxyRequest(benchmark::State& state){
    for(auto _ : state){
        miniCDN::HttpRequest req;
        req.parse(PROXY_REQUEST);
    }
}
BENCHMARK(BM_Parse_ProxyRequest);

// Compare: is a direct request faster to parse than a proxy request?
static void BM_Parse_DirectRequest(benchmark::State& state){
    for(auto _ : state){
        miniCDN::HttpRequest req;
        req.parse(DIRECT_REQUEST);
    }
}
BENCHMARK(BM_Parse_DirectRequest);

// How fast is getHost() after parsing?
static void BM_GetHost(benchmark::State& state){
    miniCDN::HttpRequest req;
    req.parse(PROXY_REQUEST);      // parse once outside the loop (setup cost)

    for(auto _ : state){
        benchmark::DoNotOptimize(req.getHost());
    }
}
BENCHMARK(BM_GetHost);

// How fast is case-insensitive header lookup?
static void BM_GetHeader(benchmark::State& state){
    miniCDN::HttpRequest req;
    req.parse(PROXY_REQUEST);

    for(auto _ : state){
        benchmark::DoNotOptimize(req.getHeader("User-Agent"));
    }
}
BENCHMARK(BM_GetHeader);

// How fast is toString() — the serialisation before forwarding?
static void BM_ToString(benchmark::State& state){
    miniCDN::HttpRequest req;
    req.parse(PROXY_REQUEST);

    for(auto _ : state){
        benchmark::DoNotOptimize(req.toString());
    }
}
BENCHMARK(BM_ToString);

// Full pipeline: parse -> getHost -> setHeader -> toString
static void BM_FullProxyPipeline(benchmark::State& state){
    for(auto _ : state){
        miniCDN::HttpRequest req;
        req.parse(PROXY_REQUEST);
        req.removeHeader("Proxy-Connection");
        req.setHeader("Host", req.getHost());
        req.setHeader("Connection", "close");
        benchmark::DoNotOptimize(req.toString());
    }
}
BENCHMARK(BM_FullProxyPipeline);

BENCHMARK_MAIN();
