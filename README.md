# sockpuppet
Minimalistic and easy-to-use C++ socket and address library.

## About
So you want to have network communication in your little program but boost seems just too much? And how hard can this socket stuff be anyhow, eh? - After being there more than once, I write this library with the hope that it spares me from future coding of half-assed single-use sockets.

## Library features
- [x] supports Unix, OSX and Windows OS
- [x] IPv6 and IPv4 address handling (lookup using *getaddrinfo* and storage using *sockaddr_storage*)
- [x] multi-interface-aware (list local host interface addresses before selecting one to bind to)
- [x] UDP and TCP socket classes
- [x] UDP broadcast (but not automatically on multiple network interfaces)
- [x] basic sockets with blocking and non-blocking IO using optional timeout parameter
- [x] extended sockets with configurable internal resource pool eliminating the need for pre-allocated buffers
- [x] extended sockets for asynchronous operation using driver thread interface (event handling using *poll*)
- [x] TCP sockets can be augmented with TLS encryption
- [x] scheduled tasks to be run at given point in time by driver thread
- [x] exceptions with meaningful system-provided error messages
- [x] library includes do not pull any system or external headers
- [x] static and dynamic library build targets
- [x] some functional tests
- [x] examples and demo project included
- [ ] exhaustive unit tests :cold_sweat:
- [ ] UDP multicast
- [ ] address arithmetic/lookup for network/broadcast addresses

## Build [![Build Status](https://app.travis-ci.com/mporsch/sockpuppet.svg?branch=master)](https://app.travis-ci.com/mporsch/sockpuppet)
Configure and build library/examples/demo/tests using CMake.

## Quickstart
The CMake install target prepares a CMake-based demo project suitable as a starting point for own applications.

## Library interface
The `Address` class represents localhost or remote UDP/TCP addresses and is used to create local and send/connect to remote sockets.

The socket classes `Socket*`, `Socket*Buffered`and `Socket*Async` provide different levels of convenience around the raw OS socket representation:
* `SocketUdp` and `SocketTcp` allow basic functions like connect, send and receive, while `Acceptor` listens for incoming TCP connections
* `SocketUdpBuffered` and `SocketTcpBuffered` add an internal receive buffer pool
* `SocketUdpAsync` and `SocketTcpAsync` as well as `AcceptorAsync` are run by a `Driver` (i.e. a thread) providing asynchronous operation to one or multiple sockets

If built with TLS support, all TCP socket classes can be instantiated with an SSL certificate and private key file to run encrypted connections.

The `ToDo` class is used for scheduling tasks to be run by a `Driver` at a given point in time, e.g. periodic heartbeat packet transmissions or reconnect attemts.

## Design rationale
* most user-visible classes employ a bridge/PIMPL pattern to avoid forwarding the internally included system headers
* the address class employs *getaddrinfo* when created by user input and *sockaddr_storage* when created by a socket; for users this distinction is transparent
* while the user-visible socket classes distinguish between UDP/TCP, the socket PIMPL classes provide generic functions that may have redundancies for some use cases
* *poll* is used prior to socket IO if a limited timeout is given, to honor the deadline
* the augmenting sockets consume pre-constructed basic sockets to avoid aggregating all base socket constructor arguments and funnelling all their exceptions
* threads are not created internally to avoid messy shared library shutdown on Windows
