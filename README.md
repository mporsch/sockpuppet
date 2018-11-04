# sockpuppet
Minimalistic and easy to use C++ socket and address classes without external dependencies.

## Welcome
So you want to have network communication in your little program but boost seems just too much? And how hard can this socket stuff be anyhow, eh? - Or at least these were my thoughts before looking into this abyss.

So welcome! Have a look around, give it a try and if something is horribly wrong, please add an issue ticket. Mind that this is a work in progress, though.

## Library features
- [x] supports Unix and Windows OS
- [x] IPv6 and IPv4 address handling (lookup using *getaddrinfo* and storage using *sockaddr_storage*)
- [x] UDP and TCP socket classes
- [X] UDP broadcast (but not automatically on multiple network interfaces)
- [x] blocking and non-blocking IO using optional timeout parameter
- [x] optional internal resource pool eliminating the need for pre-allocated buffers
- [x] exceptions with meaningful system-provided error messages
- [x] library includes do not pull any system headers
- [x] static and dynamic library build targets
- [ ] exhaustive tests :cold_sweat:
- [ ] UDP multicast
- [ ] asynchronous receipt using trigger thread interface (and some thread safety, maybe)

## Build
Configure and build library/examples/demo/tests using CMake.
