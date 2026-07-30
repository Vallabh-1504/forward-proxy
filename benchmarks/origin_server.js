// origin_server.js
// The origin server has been checked for becoming a bottlework in proxy's testing, and it does not.

const http = require("http");

const BODY = Buffer.from("pong");

const server = http.createServer((req, res) => {
  res.writeHead(200, {
    "Content-Type": "text/plain",
    "Content-Length": BODY.length,
  });
  res.end(BODY);
});

// Increase the number of simultaneous keep-alive sockets the server accepts.
// Default is 5 which would bottleneck under high concurrency.
server.maxConnections = 1024;

const PORT = 8000;
const HOST = "127.0.0.1";

server.listen(PORT, HOST, () => {
  console.log(`Origin server running on http://${HOST}:${PORT}`);
  console.log("Press Ctrl+C to stop.");
});
