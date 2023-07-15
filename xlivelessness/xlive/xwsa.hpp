#pragma once

extern CRITICAL_SECTION xlive_critsec_overlapped_sockets;

INT WINAPI XWSAStartup(WORD wVersionRequested, LPWSADATA lpWSAData);
INT WINAPI XWSACleanup();
void XWSADeleteOverlappedSocket_(SOCKET perpetual_socket, SOCKET transitory_socket);
