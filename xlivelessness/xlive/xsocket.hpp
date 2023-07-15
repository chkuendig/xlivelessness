#pragma once
#include <stdint.h>
#include <map>

#define IsUsingBasePort(base_port) (base_port != 0 && base_port != 0xFFFF)
extern uint16_t xlive_base_port;
extern uint16_t xlive_base_port_broadcast_spacing_start;
extern uint16_t xlive_base_port_broadcast_spacing_increment;
extern uint16_t xlive_base_port_broadcast_spacing_end;
extern HANDLE xlive_base_port_mutex;
extern uint16_t xlive_port_online;
extern uint16_t xlive_port_system_link;
extern BOOL xlive_netsocket_abort;

struct SOCKET_MAPPING_INFO {
	SOCKET transitorySocket = INVALID_SOCKET;
	SOCKET perpetualSocket = INVALID_SOCKET;
	int32_t type = 0;
	int32_t protocol = 0;
	bool isVdpProtocol = false;
	uint16_t portOgHBO = 0;
	uint16_t portBindHBO = 0;
	int16_t portOffsetHBO = -1;
	bool broadcast = false;
	// Key: optname. Value: optval.
	// Note: optname keys will either be SO or TCP options depending on if this is a UDP or TCP socket.
	std::map<int32_t, uint32_t> *socketOptions = 0;
	// Key: cmd. Value: cmd value.
	std::map<int32_t, uint32_t> *socketIoctl = 0;
};

extern CRITICAL_SECTION xlive_critsec_sockets;
extern std::map<SOCKET, SOCKET_MAPPING_INFO*> xlive_socket_info;
extern std::map<SOCKET, SOCKET> xlive_xsocket_perpetual_to_transitory_socket;
extern SOCKET xlive_xsocket_perpetual_core_socket;

SOCKET XSocketGetTransitorySocket_(SOCKET perpetual_socket);
SOCKET XSocketGetTransitorySocket(SOCKET perpetual_socket, bool *connectionless_socket);
SOCKET XSocketGetTransitorySocket(SOCKET perpetual_socket);
bool XSocketPerpetualSocketChangedError_(int errorSocket, SOCKET perpetual_socket, SOCKET transitory_socket);
bool XSocketPerpetualSocketChangedError(int errorSocket, SOCKET perpetual_socket, SOCKET transitory_socket);
uint8_t XSocketPerpetualSocketChanged_(SOCKET perpetual_socket, SOCKET transitory_socket);
void XSocketRebindSockets_(SOCKET *perpetual_sockets, uint32_t perpetual_sockets_count);
void XSocketRebindAllSockets();
INT WINAPI XSocketRecvFrom(SOCKET perpetual_socket, char *dataBuffer, int dataBufferSize, int flags, sockaddr *from, int *fromlen);

void XLLNCloseCoreSocket();
BOOL InitXSocket();
BOOL UninitXSocket();
