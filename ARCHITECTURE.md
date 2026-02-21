# miniCDN — Architecture

> modules 1–3 implemented (TCP server, HTTP parser, proxy forwarder).  
> Modules 4–5 (LRU cache, thread pool) not yet implemented.

---

## High-Level Design

```
Client
  │
  │  HTTP/1.1 request (TCP)
  ▼
┌─────────────────────────────────┐
│           TcpServer             │  ← listens on port 8080 (blocking accept loop)
│  accept() → handleClient()      │
└──────────────┬──────────────────┘
               │ raw bytes (char[4096])
               ▼
┌─────────────────────────────────┐
│          HttpRequest            │  ← parses method, URL, HTTP version, headers
│  parse() → getHost() / getUrl() │
└──────────────┬──────────────────┘
               │ parsed request object
               ▼
┌─────────────────────────────────┐
│          ProxyHandler           │  ← connects to origin, forwards request, changes header
│  handleRequest() / connectToHost│
└──────────────┬──────────────────┘
               │ raw response bytes  (forwarding not yet complete)
               ▼
            Client
```

---

## Module Wise

### 1. Entry Point — `src/main/main.cpp`

Constructs a `TcpServer` on port **8080** and calls `start()`. Errors are handled gracefully

---

### 2. `TcpServer` — `src/server/`

| Member | Role |
|--------|------|
| `TcpServer(int port)` | Calls `setupSocket()` in the constructor. Throws `std::runtime_error` on any Winsock failure. |
| `setupSocket()` | Initialises Winsock 2.2, creates an `AF_INET / SOCK_STREAM` socket, binds to `INADDR_ANY`, and begins listening with `SOMAXCONN` backlog. |
| `start()` | Infinite `accept()` loop. Blocks on each call; a single thread services one client at a time. |
| `handleClient(SOCKET)` | Reads up to 4096 bytes, feeds raw data to `HttpRequest::parse()`, and dispatches GET requests to `ProxyHandler`. |
| `cleanup()` | Closes the server socket and calls `WSACleanup()`. Called by the destructor. |

**Design choices & trade-offs:**

- **Blocking I/O / single-threaded** — simplest correct implementation; one client blocks all others. The planned thread pool (module 5) will fix this.
- **Fixed 4096-byte receive buffer** — sufficient for typical HTTP/1.1 request headers; will silently truncate very large headers.
- **Winsock 2.2** — targets Windows only. A future POSIX layer would be needed for cross-platform support.

---

### 3. `HttpRequest` — `src/http/`

A pull-parser built on `std::istringstream`.

| Member | Role |
|--------|------|
| `parse(rawData)` | Reads the request line (method, URL, version) then iterates over `key: value` header lines until an empty line. Returns `false` on malformed input. |
| `getHost()` | First checks if the URL is absolute (`http://host/path`); falls back to the `Host` header. Handles both proxy-style and direct requests. |
| `toString()` | Serialises the (potentially mutated) request back to a wire-format string for forwarding. |
| `setHeader(key, value)` | Allows the proxy layer to mutate headers (e.g. `Connection: close`) before forwarding. |

**Design choices & trade-offs:**

- **`std::unordered_map` for headers** — O(1) average lookup; header names are treated case-sensitively (RFC 7230 says they are case-insensitive — a known limitation).
- **No body parsing** — only `GET` is forwarded currently; body support will be needed for `POST`/`PUT`.
- **`trim()` helper** — strips `\r\n`, spaces, and tabs from both ends of keys and values, making the parser resilient to CRLF line endings.

---

### 4. `ProxyHandler` — `src/proxy/`

| Member | Role |
|--------|------|
| `handleRequest(client_socket, request)` | Sets `Connection: close`, resolves the target host, and opens a TCP socket to port 80. |
| `connectToHost(host, port)` | Uses `getaddrinfo()` for DNS resolution, creates a socket, and calls `connect()`. Returns `INVALID_SOCKET` on failure. |

**Design choices & trade-offs:**

- **`getaddrinfo()`** — portable DNS resolution; returns the first resolved address without fallback to subsequent results.
- **HTTP only (port 80)** — HTTPS (port 443 / TLS) is not yet supported; a planned improvement is OpenSSL integration.
- **Response forwarding is incomplete** — `connectToHost()` returns a socket but the current code does not yet send the request or pipe the response back to the client. This is the next implementation step.

---

### 5. `Cache` — `src/cache/` *(planned — module 4)*

Thread-safe LRU cache. Will sit between `TcpServer::handleClient()` and `ProxyHandler` to serve cached responses for repeated identical requests, avoiding redundant origin connections.

---

### 6. `ThreadPool` — `src/threadpool/` *(planned — module 5)*

Will replace the blocking single-client loop in `TcpServer::start()` with a fixed-size worker-thread pool, allowing concurrent client handling without the cost of spawning a thread per connection.

---

## Data Flow (current state)

```
accept(client)
    └─► recv(4096 bytes)
            └─► HttpRequest::parse()
                    ├─ success + GET → ProxyHandler::handleRequest()
                    │       └─► setHeader("Connection","close")
                    │       └─► connectToHost()   [DNS + TCP connect]
                    │       └─► [response piping — NOT YET IMPLEMENTED]
                    └─ fallback → send "Hello from miniCDN!" stub response
```

---

## Build System

CMake 3.10+, C++17, single executable target `miniCDN`.  
Windows only: links `ws2_32` (Winsock).

```
cmake -B build
cmake --build build
./build/miniCDN.exe      # listens on :8080
```

---

## Planned Improvements

| Priority | Item |
|----------|------|
| High | Complete proxy response piping in `ProxyHandler` |
| High | Integrate thread pool for concurrent client handling |
| High | Implement thread-safe LRU cache |
| Medium | OpenSSL integration for HTTPS (port 443) |
| Medium | Case-insensitive header map |
| Low | Custom Redis-style cache backend |
| Low | Firewall / access-control layer |
| Low | Non-blocking I/O (e.g. I/O completion ports on Windows) |
| Low | Crosse Platform support |