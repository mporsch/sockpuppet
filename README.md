# sockpuppet
Minimalistic and easy-to-use C++ socket and address classes without external dependencies.

## About
So you want to have network communication in your little program but boost seems just too much? And how hard can this socket stuff be anyhow, eh? - After being there more than once, I write this library with the hope that it spares me from future coding of half-assed single-use sockets.

## Library features
- [x] supports Unix and Windows OS
- [x] IPv6 and IPv4 address handling (lookup using *getaddrinfo* and storage using *sockaddr_storage*)
- [x] UDP and TCP socket classes
- [X] UDP broadcast (but not automatically on multiple network interfaces)
- [x] basic sockets with blocking and non-blocking IO using optional timeout parameter
- [x] extended sockets with internal resource pool eliminating the need for pre-allocated buffers
- [x] extended sockets for asynchronous operation using trigger thread interface
- [x] exceptions with meaningful system-provided error messages
- [x] library includes do not pull any system headers
- [x] static and dynamic library build targets
- [x] some functional tests
- [x] examples and demo project included
- [ ] exhaustive unit tests :cold_sweat:
- [ ] UDP multicast

## Build
Configure and build library/examples/demo/tests using CMake.

## Quickstart
The `SocketAddress` class represents localhost or remote UDP/TCP addresses and is used to create local and send/connect to remote sockets.

The socket classes `Socket*`, `Socket*Buffered`and `Socket*Async` provide different levels of convenience around the raw OS socket representation:
* `SocketUdp`, `SocketTcpClient` and `SocketTcpServer` allow basic functions like connect, send and receive
* `SocketUdpBuffered` and `SocketTcpBuffered` add an internal receive buffer pool
* `SocketUdpAsync`, `SocketTcpAsyncClient` and `SocketTcpAsyncServer` are driven by a `SocketDriver` (i.e. a thread) providing asynchronous operation to one or multiple sockets
