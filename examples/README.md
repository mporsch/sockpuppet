# sockpuppet example programs

File | Content
-----|--------
[sockpuppet_udp_server.cpp](sockpuppet_udp_server.cpp) | Simple UDP-based server that uses `Address` to select a local port to bind to and `SocketUdp` to receive incoming text messages into a pre-allocated buffer before printing them. <br />The received text content can be piped/redirected to a file.
[sockpuppet_udp_client.cpp](sockpuppet_udp_client.cpp) | Simple UDP-based interactive client using `SocketUdp` to send user-entered text to a corresponding server given via `Address`. <br />Text input can be piped/redirected from a file.
[sockpuppet_tcp_server.cpp](sockpuppet_tcp_server.cpp) | Simple TCP-based server that uses `Address` to select a local port to bind to as `Acceptor` waiting for incoming connections (one at a time) accepted as `SocketTcp`. While this connection is alive, incoming text messages are  received into a pre-allocated buffer before being printed to the command line. <br />The received text content can be piped/redirected to a file.
[sockpuppet_tcp_client.cpp](sockpuppet_tcp_client.cpp) | Simple TCP-based interactive client using `SocketTcp` to send user-entered text to a corresponding server given via `Address`. <br />Text input can be piped/redirected from a file.
[sockpuppet_http_server.cpp](sockpuppet_http_server.cpp) | Proof-of-concept HTTP server that creates multiple `AcceptorAsync` (one for each local network interface determined via `Address`) and prints the resulting HTTP URIs. If opened **in a regular web browser**, this establishes a `SocketTcp` over which a rudimentary HTML page is sent that will be displayed by the browser. The multiple sockets are run by one thread mutiplexed via `Driver` and can serve one client at a time.
[sockpuppet_http_client.cpp](sockpuppet_http_client.cpp) | HTTP client that uses a `SocketTcpBuffered` to fetch and print an HTML page from a hard-coded HTTP address.
[sockpuppet_chat_server.cpp](sockpuppet_chat_server.cpp) | TCP-based chat server using `AcceptorAsync` and `SocketTcpAsync` that accepts incoming connections from **multiple** corresponding chat clients and relays messages between them.
[sockpuppet_chat_client.cpp](sockpuppet_chat_client.cpp) (and sockpuppet_chat_io_print.h) | Actual duplex TCP-based chat client that uses `SocketTcpAsync`to  connect to a corresponding server and then receives+prints incoming text messages and interactively sends user-entered ones. A periodic reconnect after connection loss is implemented using `ToDo` timed actions.
[sockpuppet_silent_song.cpp](sockpuppet_silent_song.cpp) | Non-network-related example for using `ToDo` objects run by a `Driver` for timed printouts based on karaoke lyrics files (.lrc).

All TCP-based client/server examples can easily be modified to use TLS encryption by adding `SocketTcp*`/`Acceptor*` constructor arguments for certificate and private key file.
