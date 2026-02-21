# Project Implementation of miniCDN by modules and phases

## Objectives of Project
1. Creating a socket opening a TCP connection, establishing the client
2. Making a HTTP parser to parse the incoming request from client and the Host server
3. Creating another socket to talk with host server with the client request and forwarding it
4. Creating a threadpool to serve the clients
5. Creating a thread-safe LRU cache for fast lookups

## module-1: creating a Socket to talk with the client
using Winsock library and following standard boilerplate, creating a socket, bind, listen (blocking), and then accept and receive.

## module-2: creating a custom HTTP parser
using sstream extracting Method, Url, Header and Host

## module-3: seting proxy to connect Host server
adding methods of setHeaders to set connection -> close

## module-4: Creating an abstract thread-safe LRU cache

## module-5: integrating the custom abstract threadpool Algorithm

## improvements
1. Integrating the openSSL to allow HTTPS and corresponding code changes
2. Upgrading LRU Cache to a real custom redis
3. Implementing a firewall
4. redesigning it as a non-blocking I/O