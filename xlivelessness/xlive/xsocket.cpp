#include <winsock2.h>
#include "xdefs.hpp"
#include "xsocket.hpp"
#include "xlive.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/wnd-sockets.hpp"
#include "../xlln/xlln-keep-alive.hpp"
#include "xnet.hpp"
#include "xlocator.hpp"
#include "xnetqos.hpp"
#include "packet-handler.hpp"
#include "net-entity.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"

#define IPPROTO_VDP 254

uint16_t xlive_base_port = 0;
uint16_t xlive_base_port_broadcast_spacing_start = 2000;
uint16_t xlive_base_port_broadcast_spacing_increment = 100;
uint16_t xlive_base_port_broadcast_spacing_end = 2400;
HANDLE xlive_base_port_mutex = 0;
uint16_t xlive_port_online = 3074;
uint16_t xlive_port_system_link = 0;
BOOL xlive_netsocket_abort = FALSE;

CRITICAL_SECTION xlive_critsec_sockets;
// Key: transitorySocket.
std::map<SOCKET, SOCKET_MAPPING_INFO*> xlive_socket_info;
// Value: transitorySocket.
static std::map<uint16_t, SOCKET> xlive_port_offset_sockets;
// Key: perpetualSocket. Value: transitorySocket.
std::map<SOCKET, SOCKET> xlive_xsocket_perpetual_to_transitory_socket;

SOCKET xlive_xsocket_perpetual_core_socket = INVALID_SOCKET;

static void XLLNCreateCoreSocket();

// Get transitorySocket (real socket) from perpetualSocket (fake handle).
SOCKET XSocketGetTransitorySocket_(SOCKET perpetual_socket)
{
	SOCKET transitorySocket = INVALID_SOCKET;
	
	if (xlive_xsocket_perpetual_to_transitory_socket.count(perpetual_socket)) {
		transitorySocket = xlive_xsocket_perpetual_to_transitory_socket[perpetual_socket];
	}
	
	if (transitorySocket == INVALID_SOCKET) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s unknown perpetual_socket (0x%08x)."
			, __func__
			, perpetual_socket
		);
	}
	
	return transitorySocket;
}
// Get transitorySocket (real socket) from perpetualSocket (fake handle).
SOCKET XSocketGetTransitorySocket(SOCKET perpetual_socket, bool *connectionless_socket)
{
	SOCKET transitorySocket = INVALID_SOCKET;
	
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		
		if (xlive_xsocket_perpetual_to_transitory_socket.count(perpetual_socket)) {
			transitorySocket = xlive_xsocket_perpetual_to_transitory_socket[perpetual_socket];
			
			if (connectionless_socket) {
				*connectionless_socket = xlive_socket_info[transitorySocket]->protocol == IPPROTO_UDP;
			}
		}
		
		LeaveCriticalSection(&xlive_critsec_sockets);
	}
	
	if (transitorySocket == INVALID_SOCKET) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s unknown perpetual_socket (0x%08x)."
			, __func__
			, perpetual_socket
		);
	}
	
	return transitorySocket;
}
SOCKET XSocketGetTransitorySocket(SOCKET perpetual_socket)
{
	return XSocketGetTransitorySocket(perpetual_socket, 0);
}

bool XSocketPerpetualSocketChangedError_(int errorSocket, SOCKET perpetual_socket, SOCKET transitory_socket)
{
	bool result = false;
	
	if (
		(errorSocket == WSAEINTR || errorSocket == WSAENOTSOCK)
		&& xlive_xsocket_perpetual_to_transitory_socket.count(perpetual_socket)
		&& xlive_xsocket_perpetual_to_transitory_socket[perpetual_socket] != transitory_socket
	) {
		result = true;
	}
	
	return result;
}

bool XSocketPerpetualSocketChangedError(int errorSocket, SOCKET perpetual_socket, SOCKET transitory_socket)
{
	bool result = false;
	
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		
		result = XSocketPerpetualSocketChangedError_(errorSocket, perpetual_socket, transitory_socket);
		
		LeaveCriticalSection(&xlive_critsec_sockets);
	}
	
	return result;
}

uint8_t XSocketPerpetualSocketChanged_(SOCKET perpetual_socket, SOCKET transitory_socket)
{
	uint8_t result = 0;
	
	if (!xlive_xsocket_perpetual_to_transitory_socket.count(perpetual_socket)) {
		result = 1;
	}
	else if (xlive_xsocket_perpetual_to_transitory_socket[perpetual_socket] != transitory_socket) {
		result = 2;
	}
	
	return result;
}

void XSocketRebindAllSockets()
{
	EnterCriticalSection(&xlive_critsec_sockets);
	
	{
		std::map<SOCKET_MAPPING_INFO*, SOCKET_MAPPING_INFO*> oldToNew;
		
		for (const auto &socketInfo : xlive_socket_info) {
			SOCKET_MAPPING_INFO &socketMappingInfoOld = *socketInfo.second;
			if (socketMappingInfoOld.protocol != IPPROTO_UDP) {
				// TODO support more than just UDP (like TCP).
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
					, "%s transitorySocket (0x%08x) protocol (%d) unsupported for rebinding."
					, __func__
					, socketMappingInfoOld.transitorySocket
					, socketMappingInfoOld.protocol
				);
				continue;
			}
			
			if (socketMappingInfoOld.portBindHBO == 0) {
				// The port was never binded so doesn't have to be re-binded.
				continue;
			}
			
			SOCKET_MAPPING_INFO *socketMappingInfoNew = (oldToNew[&socketMappingInfoOld] = new SOCKET_MAPPING_INFO);
			*socketMappingInfoNew = socketMappingInfoOld;
			socketMappingInfoNew->transitorySocket = socket(AF_INET, socketMappingInfoNew->type, socketMappingInfoNew->protocol);
			if (socketMappingInfoNew->transitorySocket == INVALID_SOCKET) {
				int errorSocket = WSAGetLastError();
				XLLN_DEBUG_LOG_ECODE(errorSocket, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s create socket error on perpetualSocket 0x%08x."
					, __func__
					, socketMappingInfoOld.perpetualSocket
				);
			}
			
			// If it was a random port, make sure when we rebind that it will be another random port since portBindHBO will be the port we will attempt to remake.
			if (socketMappingInfoNew->portOgHBO == 0) {
				socketMappingInfoNew->portBindHBO = 0;
			}
		}
		
		for (const auto &socketInfos : oldToNew) {
			SOCKET_MAPPING_INFO &socketMappingInfoOld = *socketInfos.first;
			SOCKET_MAPPING_INFO &socketMappingInfoNew = *socketInfos.second;
			
			xlive_socket_info[socketMappingInfoNew.transitorySocket] = &socketMappingInfoNew;
			xlive_xsocket_perpetual_to_transitory_socket[socketMappingInfoNew.perpetualSocket] = socketMappingInfoNew.transitorySocket;
			xlive_socket_info.erase(socketMappingInfoOld.transitorySocket);
			
			int resultShutdown = shutdown(socketMappingInfoOld.transitorySocket, SD_RECEIVE);
			if (resultShutdown) {
				int errorShutdown = WSAGetLastError();
				XLLN_DEBUG_LOG_ECODE(errorShutdown, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s shutdown error on transitorySocket 0x%08x."
					, __func__
					, socketMappingInfoOld.transitorySocket
				);
			}
			
			int resultCloseSocket = closesocket(socketMappingInfoOld.transitorySocket);
			if (resultCloseSocket) {
				int errorCloseSocket = WSAGetLastError();
				XLLN_DEBUG_LOG_ECODE(errorCloseSocket, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s close socket error on transitorySocket 0x%08x."
					, __func__
					, socketMappingInfoOld.transitorySocket
				);
			}
		}
		
		for (const auto &socketInfos : oldToNew) {
			SOCKET_MAPPING_INFO &socketMappingInfoOld = *socketInfos.first;
			SOCKET_MAPPING_INFO &socketMappingInfoNew = *socketInfos.second;
			
			for (const auto &optionNameValue : *socketMappingInfoNew.socketOptions) {
				int resultSetSockOpt = setsockopt(socketMappingInfoNew.transitorySocket, socketMappingInfoNew.protocol == IPPROTO_TCP ? IPPROTO_TCP : SOL_SOCKET, optionNameValue.first, (const char*)&optionNameValue.second, sizeof(uint32_t));
				if (resultSetSockOpt) {
					int errorSetSockOpt = WSAGetLastError();
					XLLN_DEBUG_LOG_ECODE(errorSetSockOpt, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s setsockopt failed to set option (0x%08x, 0x%08x) on new transitorySocket 0x%08x with error:"
						, __func__
						, optionNameValue.first
						, optionNameValue.second
						, socketMappingInfoNew.transitorySocket
					);
				}
			}
			
			for (const auto &optionNameValue : *socketMappingInfoNew.socketIoctl) {
				u_long cmdValue = optionNameValue.second;
				int resultIoctlSocket = ioctlsocket(socketMappingInfoNew.transitorySocket, optionNameValue.first, &cmdValue);
				if (resultIoctlSocket) {
					int errorIoctlSocket = WSAGetLastError();
					XLLN_DEBUG_LOG_ECODE(errorIoctlSocket, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s ioctlsocket failed to set command (0x%08x, 0x%08x) on new transitorySocket 0x%08x with error:"
						, __func__
						, optionNameValue.first
						, optionNameValue.second
						, socketMappingInfoNew.transitorySocket
					);
				}
			}
			
			sockaddr_in socketBindAddress;
			socketBindAddress.sin_family = AF_INET;
			socketBindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
			socketBindAddress.sin_port = htons(socketMappingInfoNew.portBindHBO);
			{
				EnterCriticalSection(&xlive_critsec_network_adapter);
				// TODO Should we perhaps not do this unless the user really wants to like if (xlive_init_preferred_network_adapter_name or config) is set?
				if (xlive_network_adapter && xlive_network_adapter->unicastHAddr != INADDR_LOOPBACK) {
					socketBindAddress.sin_addr.s_addr = htonl(xlive_network_adapter->unicastHAddr);
				}
				LeaveCriticalSection(&xlive_critsec_network_adapter);
			}
			SOCKET resultSocketBind = bind(socketMappingInfoNew.transitorySocket, (sockaddr*)&socketBindAddress, sizeof(socketBindAddress));
			if (resultSocketBind) {
				int errorBind = WSAGetLastError();
				XLLN_DEBUG_LOG_ECODE(errorBind, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s bind error on new transitorySocket 0x%08x."
					, __func__
					, socketMappingInfoNew.transitorySocket
				);
			}
			
			delete &socketMappingInfoOld;
		}
	}
	
	XllnWndSocketsInvalidateSockets();
	
	LeaveCriticalSection(&xlive_critsec_sockets);
}

// #3
SOCKET WINAPI XSocketCreate(int af, int type, int protocol)
{
	TRACE_FX();
	if (af != AF_INET) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s af (%d) must be AF_INET (%d)."
			, __func__
			, af
			, AF_INET
		);
		WSASetLastError(WSAEAFNOSUPPORT);
		return INVALID_SOCKET;
	}
	if (type == 0 && protocol == 0) {
		type = SOCK_STREAM;
		protocol = IPPROTO_TCP;
	}
	else if (type == SOCK_DGRAM && protocol == 0) {
		protocol = IPPROTO_UDP;
	}
	else if (type == SOCK_STREAM && protocol == 0) {
		protocol = IPPROTO_TCP;
	}
	bool vdp = false;
	if (protocol == IPPROTO_VDP) {
		vdp = true;
		// We can't support VDP (Voice / Data Protocol) it's some encrypted crap which isn't standard.
		protocol = IPPROTO_UDP;
	}
	if (type != SOCK_STREAM && type != SOCK_DGRAM) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s type (%d) must be either SOCK_STREAM (%d) or SOCK_DGRAM (%d)."
			, __func__
			, type
			, SOCK_STREAM
			, SOCK_DGRAM
		);
		WSASetLastError(WSAEOPNOTSUPP);
		return INVALID_SOCKET;
	}

	SOCKET transitorySocket = socket(af, type, protocol);
	int errorSocketCreate = WSAGetLastError();

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
		, "%s 0x%08x. AF: %d. Type: %d. Protocol: %d."
		, __func__
		, transitorySocket
		, af
		, type
		, protocol
	);

	if (transitorySocket == INVALID_SOCKET) {
		XLLN_DEBUG_LOG_ECODE(errorSocketCreate, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s create socket failed AF: %d. Type: %d. Protocol: %d."
			, __func__
			, af
			, type
			, protocol
		);
		
		WSASetLastError(errorSocketCreate);
		return INVALID_SOCKET;
	}
	
	if (vdp) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
			, "%s VDP socket created: 0x%08x."
			, __func__
			, transitorySocket
		);
	}
	
	SOCKET perpetualSocket = (SOCKET)CreateMutexA(NULL, false, NULL);
	
	SOCKET_MAPPING_INFO* socketMappingInfo = new SOCKET_MAPPING_INFO;
	socketMappingInfo->socketOptions = new std::map<int32_t, uint32_t>();
	socketMappingInfo->socketIoctl = new std::map<int32_t, uint32_t>();
	socketMappingInfo->transitorySocket = transitorySocket;
	socketMappingInfo->perpetualSocket = perpetualSocket;
	socketMappingInfo->type = type;
	socketMappingInfo->protocol = protocol;
	socketMappingInfo->isVdpProtocol = vdp;
	
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		
		xlive_socket_info[transitorySocket] = socketMappingInfo;
		xlive_xsocket_perpetual_to_transitory_socket[perpetualSocket] = transitorySocket;
		
		XllnWndSocketsInvalidateSockets();
		
		LeaveCriticalSection(&xlive_critsec_sockets);
	}
	
	WSASetLastError(ERROR_SUCCESS);
	return perpetualSocket;
}

// #4
INT WINAPI XSocketClose(SOCKET perpetual_socket)
{
	TRACE_FX();
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
		, "%s perpetual_socket 0x%08x."
		, __func__
		, perpetual_socket
	);
	
	while (1) {
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		INT resultCloseSocket = closesocket(transitorySocket);
		int errorCloseSocket = WSAGetLastError();
		
		{
			EnterCriticalSection(&xlive_critsec_sockets);
			
			if (resultCloseSocket) {
				if (XSocketPerpetualSocketChangedError_(errorCloseSocket, perpetual_socket, transitorySocket)) {
					LeaveCriticalSection(&xlive_critsec_sockets);
					continue;
				}
				LeaveCriticalSection(&xlive_critsec_sockets);
				
				XLLN_DEBUG_LOG_ECODE(errorCloseSocket, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s close socket error on socket (P,T) (0x%08x,0x%08x)."
					, __func__
					, perpetual_socket
					, transitorySocket
				);
				
				WSASetLastError(errorCloseSocket);
				return SOCKET_ERROR;
			}
			
			uint8_t resultPerpetualChanged = XSocketPerpetualSocketChanged_(perpetual_socket, transitorySocket);
			if (resultPerpetualChanged) {
				LeaveCriticalSection(&xlive_critsec_sockets);
				if (resultPerpetualChanged == 2) {
					continue;
				}
				WSASetLastError(WSAENOTSOCK);
				return SOCKET_ERROR;
			}
			
			SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[transitorySocket];
			xlive_socket_info.erase(transitorySocket);
			if (socketMappingInfo->portOffsetHBO != -1) {
				xlive_port_offset_sockets.erase(socketMappingInfo->portOffsetHBO);
			}
			
			bool createCoreSocket = false;
			if (xlive_xsocket_perpetual_core_socket != INVALID_SOCKET && perpetual_socket != xlive_xsocket_perpetual_core_socket && transitorySocket == xlive_xsocket_perpetual_to_transitory_socket[xlive_xsocket_perpetual_core_socket]) {
				createCoreSocket = true;
				xlive_xsocket_perpetual_to_transitory_socket.erase(xlive_xsocket_perpetual_core_socket);
				CloseHandle((HANDLE)xlive_xsocket_perpetual_core_socket);
				xlive_xsocket_perpetual_core_socket = INVALID_SOCKET;
			}
			
			xlive_xsocket_perpetual_to_transitory_socket.erase(perpetual_socket);
			CloseHandle((HANDLE)perpetual_socket);
			
			XllnWndSocketsInvalidateSockets();
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			if (socketMappingInfo->socketOptions) {
				delete socketMappingInfo->socketOptions;
			}
			if (socketMappingInfo->socketIoctl) {
				delete socketMappingInfo->socketIoctl;
			}
			delete socketMappingInfo;
			
			if (createCoreSocket) {
				XLLNCreateCoreSocket();
			}
		}
		
		WSASetLastError(errorCloseSocket);
		return resultCloseSocket;
	}
}

// #5
INT WINAPI XSocketShutdown(SOCKET perpetual_socket, int how)
{
	TRACE_FX();
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
		, "%s perpetual_socket 0x%08x. how: %d."
		, __func__
		, perpetual_socket
		, how
	);
	
	while (1) {
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		INT resultShutdown = shutdown(transitorySocket, how);
		int errorShutdown = WSAGetLastError();
		
		{
			EnterCriticalSection(&xlive_critsec_sockets);
			
			if (resultShutdown) {
				if (XSocketPerpetualSocketChangedError_(errorShutdown, perpetual_socket, transitorySocket)) {
					LeaveCriticalSection(&xlive_critsec_sockets);
					continue;
				}
				LeaveCriticalSection(&xlive_critsec_sockets);
				
				XLLN_DEBUG_LOG_ECODE(errorShutdown, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s shutdown socket error on socket (P,T) (0x%08x,0x%08x) how (%d)."
					, __func__
					, perpetual_socket
					, transitorySocket
					, how
				);
				
				WSASetLastError(errorShutdown);
				return SOCKET_ERROR;
			}
			
			uint8_t resultPerpetualChanged = XSocketPerpetualSocketChanged_(perpetual_socket, transitorySocket);
			if (resultPerpetualChanged) {
				LeaveCriticalSection(&xlive_critsec_sockets);
				if (resultPerpetualChanged == 2) {
					continue;
				}
				WSASetLastError(WSAENOTSOCK);
				return SOCKET_ERROR;
			}
			
			SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[transitorySocket];
			if (socketMappingInfo->portOffsetHBO != -1) {
				xlive_port_offset_sockets.erase(socketMappingInfo->portOffsetHBO);
			}
			socketMappingInfo->portBindHBO = 0;
			
			XllnWndSocketsInvalidateSockets();
			
			LeaveCriticalSection(&xlive_critsec_sockets);
		}
		
		WSASetLastError(errorShutdown);
		return resultShutdown;
	}
}

// #6
INT WINAPI XSocketIOCTLSocket(SOCKET perpetual_socket, __int32 cmd, ULONG *argp)
{
	TRACE_FX();
	
	while (1) {
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		INT resultIoctlSocket = ioctlsocket(transitorySocket, cmd, argp);
		int errorIoctlSocket = WSAGetLastError();
		
		bool ignoreLogging = false;
		
		{
			EnterCriticalSection(&xlive_critsec_sockets);
			
			if (resultIoctlSocket) {
				if (XSocketPerpetualSocketChangedError_(errorIoctlSocket, perpetual_socket, transitorySocket)) {
					LeaveCriticalSection(&xlive_critsec_sockets);
					continue;
				}
				LeaveCriticalSection(&xlive_critsec_sockets);
				
				XLLN_DEBUG_LOG_ECODE(errorIoctlSocket, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s ioctlsocket error on socket (P,T) (0x%08x,0x%08x)."
					, __func__
					, perpetual_socket
					, transitorySocket
				);
				
				WSASetLastError(errorIoctlSocket);
				return SOCKET_ERROR;
			}
			
			uint8_t resultPerpetualChanged = XSocketPerpetualSocketChanged_(perpetual_socket, transitorySocket);
			if (resultPerpetualChanged) {
				LeaveCriticalSection(&xlive_critsec_sockets);
				if (resultPerpetualChanged == 2) {
					continue;
				}
				WSASetLastError(WSAENOTSOCK);
				return SOCKET_ERROR;
			}
			
			SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[transitorySocket];
			
			switch (cmd) {
				// Ignore read commands.
				case FIONREAD:
				case SIOCGHIWAT:
				case SIOCGLOWAT:
				case SIOCATMARK:
					ignoreLogging = true;
					break;
				default: {
					(*socketMappingInfo->socketIoctl)[cmd] = *argp;
					
					XllnWndSocketsInvalidateSockets();
					break;
				}
			}
			
			LeaveCriticalSection(&xlive_critsec_sockets);
		}
		
		if (!ignoreLogging) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s cmd (0x%08x) value (0x%08x)."
				, __func__
				, cmd
				, *argp
			);
		}
		
		WSASetLastError(errorIoctlSocket);
		return resultIoctlSocket;
	}
}

// #7
INT WINAPI XSocketSetSockOpt(SOCKET perpetual_socket, int level, int optname, const char *optval, int optlen)
{
	TRACE_FX();
	
	while (1) {
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		INT resultSetSockOpt = setsockopt(transitorySocket, level, optname, optval, optlen);
		int errorSetSockOpt = WSAGetLastError();
		
		{
			EnterCriticalSection(&xlive_critsec_sockets);
			
			if (resultSetSockOpt) {
				if (XSocketPerpetualSocketChangedError_(errorSetSockOpt, perpetual_socket, transitorySocket)) {
					LeaveCriticalSection(&xlive_critsec_sockets);
					continue;
				}
				LeaveCriticalSection(&xlive_critsec_sockets);
				
				XLLN_DEBUG_LOG_ECODE(errorSetSockOpt, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s setsockopt error on socket (P,T) (0x%08x,0x%08x)."
					, __func__
					, perpetual_socket
					, transitorySocket
				);
				
				WSASetLastError(errorSetSockOpt);
				return SOCKET_ERROR;
			}
			
			uint8_t resultPerpetualChanged = XSocketPerpetualSocketChanged_(perpetual_socket, transitorySocket);
			if (resultPerpetualChanged) {
				LeaveCriticalSection(&xlive_critsec_sockets);
				if (resultPerpetualChanged == 2) {
					continue;
				}
				WSASetLastError(WSAENOTSOCK);
				return SOCKET_ERROR;
			}
			
			SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[transitorySocket];
			
			uint32_t optionValue = 0;
			memcpy(&optionValue, optval, optlen > 4 ? 4 : optlen);
			(*socketMappingInfo->socketOptions)[optname] = optionValue;
			
			if (level == SOL_SOCKET && optname == SO_BROADCAST) {
				socketMappingInfo->broadcast = optionValue > 0;
			}
			
			XllnWndSocketsInvalidateSockets();
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s level (0x%08x) option name (0x%08x) value (0x%08x)."
				, __func__
				, level
				, optname
				, optionValue
			);
		}
		
		WSASetLastError(errorSetSockOpt);
		return resultSetSockOpt;
	}
}

// #8
INT WINAPI XSocketGetSockOpt(SOCKET perpetual_socket, int level, int optname, char *optval, int *optlen)
{
	TRACE_FX();
	
	while (1) {
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		INT resultGetsockopt = getsockopt(transitorySocket, level, optname, optval, optlen);
		int errorGetsockopt = WSAGetLastError();
		
		if (resultGetsockopt && XSocketPerpetualSocketChangedError(errorGetsockopt, perpetual_socket, transitorySocket)) {
			continue;
		}
		
		if (resultGetsockopt) {
			XLLN_DEBUG_LOG_ECODE(errorGetsockopt, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s getsockopt error on socket (P,T) (0x%08x,0x%08x)."
				, __func__
				, perpetual_socket
				, transitorySocket
			);
		}
		
		WSASetLastError(errorGetsockopt);
		return resultGetsockopt;
	}
}

// #9
INT WINAPI XSocketGetSockName(SOCKET perpetual_socket, struct sockaddr *name, int *namelen)
{
	TRACE_FX();
	
	while (1) {
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		INT resultGetsockname = getsockname(transitorySocket, name, namelen);
		int errorGetsockname = WSAGetLastError();
		
		if (resultGetsockname && XSocketPerpetualSocketChangedError(errorGetsockname, perpetual_socket, transitorySocket)) {
			continue;
		}
		
		if (resultGetsockname) {
			XLLN_DEBUG_LOG_ECODE(errorGetsockname, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s getsockname error on socket (P,T) (0x%08x,0x%08x)."
				, __func__
				, perpetual_socket
				, transitorySocket
			);
		}
		
		WSASetLastError(errorGetsockname);
		return resultGetsockname;
	}
}

// #10
INT WINAPI XSocketGetPeerName(SOCKET perpetual_socket, struct sockaddr *name, int *namelen)
{
	TRACE_FX();
	
	while (1) {
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		INT resultGetpeername = getsockname(transitorySocket, name, namelen);
		int errorGetpeername = WSAGetLastError();
		
		if (resultGetpeername && XSocketPerpetualSocketChangedError(errorGetpeername, perpetual_socket, transitorySocket)) {
			continue;
		}
		
		if (resultGetpeername) {
			XLLN_DEBUG_LOG_ECODE(errorGetpeername, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s getpeername error on socket (P,T) (0x%08x,0x%08x)."
				, __func__
				, perpetual_socket
				, transitorySocket
			);
		}
		
		WSASetLastError(errorGetpeername);
		return resultGetpeername;
	}
}

// #11
SOCKET WINAPI XSocketBind(SOCKET perpetual_socket, const struct sockaddr *name, int namelen)
{
	TRACE_FX();
	
	SOCKADDR_STORAGE sockAddrExternal;
	int sockAddrExternalLen = sizeof(sockAddrExternal);
	memcpy(&sockAddrExternal, name, sockAddrExternalLen < namelen ? sockAddrExternalLen : namelen);
	
	if (sockAddrExternal.ss_family != AF_INET && sockAddrExternal.ss_family != AF_INET6) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s Perpetual Socket 0x%08x bind on unsupported socket address family 0x%04hx."
			, __func__
			, perpetual_socket
			, sockAddrExternal.ss_family
		);
		
		WSASetLastError(WSAEAFNOSUPPORT);
		return SOCKET_ERROR;
	}
	
	uint16_t portHBO = GetSockAddrPort(&sockAddrExternal);
	const uint16_t portOffset = portHBO % 100;
	const uint16_t portBase = portHBO - portOffset;
	uint16_t portShiftedHBO = 0;
	
	// If not a random port (0), and we are using base ports, then shift the port.
	if (portHBO && IsUsingBasePort(xlive_base_port)) {
		portShiftedHBO = xlive_base_port + portOffset;
		SetSockAddrPort(&sockAddrExternal, portShiftedHBO);
	}
	
	if (portShiftedHBO == (IsUsingBasePort(xlive_base_port) ? xlive_base_port : xlive_port_online) && perpetual_socket != xlive_xsocket_perpetual_core_socket) {
		XLLNCloseCoreSocket();
		
		EnterCriticalSection(&xlive_critsec_sockets);
		
		xlive_xsocket_perpetual_core_socket = (SOCKET)CreateMutexA(NULL, false, NULL);
		
		SOCKET transitorySocketTitleSocket = xlive_xsocket_perpetual_to_transitory_socket[perpetual_socket];
		xlive_xsocket_perpetual_to_transitory_socket[xlive_xsocket_perpetual_core_socket] = transitorySocketTitleSocket;
		
		XllnWndSocketsInvalidateSockets();
		
		LeaveCriticalSection(&xlive_critsec_sockets);
		
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_INFO
			, "%s Perpetual Core Socket 0x%08x now points to socket (P,T) (0x%08x,0x%08x)."
			, __func__
			, xlive_xsocket_perpetual_core_socket
			, perpetual_socket
			, transitorySocketTitleSocket
		);
		
		uint32_t sockOptValue = 1;
		INT resultSetSockOpt = XSocketSetSockOpt(xlive_xsocket_perpetual_core_socket, SOL_SOCKET, SO_BROADCAST, (const char *)&sockOptValue, sizeof(sockOptValue));
		if (resultSetSockOpt == SOCKET_ERROR) {
			int errorSetSockOpt = WSAGetLastError();
			XLLN_DEBUG_LOG_ECODE(errorSetSockOpt, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
				, "%s Failed to set Broadcast option on Core Socket with error:"
				, __func__
			);
		}
	}
	
	bool networkAddrAny = false;
	while (1) {
		if (networkAddrAny) {
			((struct sockaddr_in*)&sockAddrExternal)->sin_addr.s_addr = htonl(INADDR_ANY);
		}
		
		if (sockAddrExternal.ss_family == AF_INET && ((struct sockaddr_in*)&sockAddrExternal)->sin_addr.s_addr == htonl(INADDR_ANY)) {
			EnterCriticalSection(&xlive_critsec_network_adapter);
			// TODO Should we perhaps not do this unless the user really wants to like if (xlive_init_preferred_network_adapter_name or config) is set?
			if (xlive_network_adapter && xlive_network_adapter->unicastHAddr != INADDR_LOOPBACK) {
				((struct sockaddr_in*)&sockAddrExternal)->sin_addr.s_addr = htonl(xlive_network_adapter->unicastHAddr);
				networkAddrAny = true;
			}
			LeaveCriticalSection(&xlive_critsec_network_adapter);
		}
	
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		SOCKET resultSocketBind = bind(transitorySocket, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);
		int errorSocketBind = WSAGetLastError();
		
		{
			EnterCriticalSection(&xlive_critsec_sockets);
			
			if (resultSocketBind) {
				if (XSocketPerpetualSocketChangedError_(errorSocketBind, perpetual_socket, transitorySocket)) {
					LeaveCriticalSection(&xlive_critsec_sockets);
					continue;
				}
				LeaveCriticalSection(&xlive_critsec_sockets);
				
				if (errorSocketBind == WSAEADDRINUSE) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s socket (P,T) (0x%08x,0x%08x) bind error, another program has taken port. Base: %hu. Offset: %hu. New-shifted: %hu. Original: %hu."
						, __func__
						, perpetual_socket
						, transitorySocket
						, xlive_base_port
						, portOffset
						, portShiftedHBO
						, portHBO
					);
				}
				else {
					XLLN_DEBUG_LOG_ECODE(errorSocketBind, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s bind error on socket (P,T) (0x%08x,0x%08x). Base: %hu. Offset: %hu. New-shifted: %hu. Original: %hu."
						, __func__
						, perpetual_socket
						, transitorySocket
						, xlive_base_port
						, portOffset
						, portShiftedHBO
						, portHBO
					);
				}
				
				WSASetLastError(errorSocketBind);
				return SOCKET_ERROR;
			}
			
			uint8_t resultPerpetualChanged = XSocketPerpetualSocketChanged_(perpetual_socket, transitorySocket);
			if (resultPerpetualChanged) {
				LeaveCriticalSection(&xlive_critsec_sockets);
				if (resultPerpetualChanged == 2) {
					continue;
				}
				WSASetLastError(WSAENOTSOCK);
				return SOCKET_ERROR;
			}
			
			SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[transitorySocket];
			uint16_t portBindHBO = portHBO;
			
			INT resultGetsockname = getsockname(transitorySocket, (sockaddr*)&sockAddrExternal, &sockAddrExternalLen);
			int errorGetsockname = WSAGetLastError();
			if (!resultGetsockname) {
				portBindHBO = GetSockAddrPort(&sockAddrExternal);
			}
			
			socketMappingInfo->portBindHBO = portBindHBO;
			socketMappingInfo->portOgHBO = portHBO;
			if (portShiftedHBO) {
				socketMappingInfo->portOffsetHBO = portOffset;
				xlive_port_offset_sockets[portOffset] = perpetual_socket;
			}
			else {
				socketMappingInfo->portOffsetHBO = -1;
			}
			
			int protocol = socketMappingInfo->protocol;
			bool isVdp = socketMappingInfo->isVdpProtocol;
			
			XllnWndSocketsInvalidateSockets();
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			if (resultGetsockname) {
				XLLN_DEBUG_LOG_ECODE(errorGetsockname, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s socket (P,T) (0x%08x,0x%08x) getsockname error bind port mapped from %hu to unknown address from getsockname."
					, __func__
					, perpetual_socket
					, transitorySocket
					, portHBO
				);
			}
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
				, "%s socket (P,T) (0x%08x,0x%08x) bind. Protocol: %s%d%s. Port: %hu -> %hu."
				, __func__
				, perpetual_socket
				, transitorySocket
				, protocol == IPPROTO_UDP ? "UDP:" : (protocol == IPPROTO_TCP ? "TCP:" : "")
				, protocol
				, isVdp ? " was VDP" : ""
				, portHBO
				, portBindHBO
			);
		}
		
		WSASetLastError(errorSocketBind);
		return resultSocketBind;
	}
}

// #12
INT WINAPI XSocketConnect(SOCKET perpetual_socket, const struct sockaddr *name, int namelen)
{
	TRACE_FX();
	
	SOCKADDR_STORAGE sockAddrExternal;
	int sockAddrExternalLen = sizeof(sockAddrExternal);
	memcpy(&sockAddrExternal, name, sockAddrExternalLen < namelen ? sockAddrExternalLen : namelen);
	
	if (sockAddrExternal.ss_family != AF_INET && sockAddrExternal.ss_family != AF_INET6) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s Perpetual Socket 0x%08x connect on unsupported socket address family 0x%04hx."
			, __func__
			, perpetual_socket
			, sockAddrExternal.ss_family
		);
		
		WSASetLastError(WSAEAFNOSUPPORT);
		return SOCKET_ERROR;
	}
	
	if (sockAddrExternal.ss_family == AF_INET) {
		const uint32_t ipv4NBO = ((struct sockaddr_in*)&sockAddrExternal)->sin_addr.s_addr;
		// This address may (hopefully) be an instanceId.
		const uint32_t ipv4HBO = ntohl(ipv4NBO);
		const uint16_t portHBO = GetSockAddrPort(&sockAddrExternal);
		
		uint32_t resultNetter = NetterEntityGetAddrByInstanceIdPort(&sockAddrExternal, ipv4HBO, portHBO);
		if (resultNetter == ERROR_PORT_NOT_SET) {
			char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s on Perpetual Socket 0x%08x NetterEntityGetAddrByInstanceIdPort ERROR_PORT_NOT_SET address/instanceId 0x%08x:%hu assuming %s."
				, __func__
				, perpetual_socket
				, ipv4HBO
				, portHBO
				, sockAddrInfo ? sockAddrInfo : ""
			);
			if (sockAddrInfo) {
				free(sockAddrInfo);
			}
		}
		else if (resultNetter) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s on Perpetual Socket 0x%08x NetterEntityGetAddrByInstanceIdPort failed to find address 0x%08x:%hu with error 0x%08x."
				, __func__
				, perpetual_socket
				, ipv4HBO
				, portHBO
				, resultNetter
			);
		}
		else {
			char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s on Perpetual Socket 0x%08x NetterEntityGetAddrByInstanceIdPort found address/instanceId 0x%08x:%hu as %s."
				, __func__
				, perpetual_socket
				, ipv4HBO
				, portHBO
				, sockAddrInfo ? sockAddrInfo : ""
			);
			if (sockAddrInfo) {
				free(sockAddrInfo);
			}
		}
	}
	else {
		char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "%s on Perpetual Socket 0x%08x connecting to address %s."
			, __func__
			, perpetual_socket
			, sockAddrInfo ? sockAddrInfo : ""
		);
		if (sockAddrInfo) {
			free(sockAddrInfo);
		}
	}
	
	while (1) {
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		INT resultConnect = connect(transitorySocket, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);
		int errorConnect = WSAGetLastError();
		
		if (resultConnect && XSocketPerpetualSocketChangedError(errorConnect, perpetual_socket, transitorySocket)) {
			continue;
		}
		
		if (resultConnect) {
			char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
			XLLN_DEBUG_LOG_ECODE(errorConnect, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s connect error on socket (P,T) (0x%08x,0x%08x) to address %s."
				, __func__
				, perpetual_socket
				, transitorySocket
				, sockAddrInfo ? sockAddrInfo : ""
			);
			if (sockAddrInfo) {
				free(sockAddrInfo);
			}
		}
		
		WSASetLastError(errorConnect);
		return resultConnect;
	}
}

// #13
INT WINAPI XSocketListen(SOCKET perpetual_socket, int backlog)
{
	TRACE_FX();
	
	while (1) {
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		INT resultListen = listen(transitorySocket, backlog);
		int errorListen = WSAGetLastError();
		
		if (resultListen && XSocketPerpetualSocketChangedError(errorListen, perpetual_socket, transitorySocket)) {
			continue;
		}
		
		if (resultListen) {
			XLLN_DEBUG_LOG_ECODE(errorListen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s listen error on socket (P,T) (0x%08x,0x%08x)."
				, __func__
				, perpetual_socket
				, transitorySocket
			);
		}
		
		WSASetLastError(errorListen);
		return resultListen;
	}
}

// #14
SOCKET WINAPI XSocketAccept(SOCKET perpetual_socket, struct sockaddr *addr, int *addrlen)
{
	TRACE_FX();
	
	while (1) {
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		SOCKET resultAccept = accept(transitorySocket, addr, addrlen);
		int errorAccept = WSAGetLastError();
		
		if (resultAccept == INVALID_SOCKET && XSocketPerpetualSocketChangedError(errorAccept, perpetual_socket, transitorySocket)) {
			continue;
		}
		
		if (resultAccept == INVALID_SOCKET) {
			if (errorAccept != WSAEWOULDBLOCK) {
				XLLN_DEBUG_LOG_ECODE(errorAccept, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s accept error on socket (P,T) (0x%08x,0x%08x)."
					, __func__
					, perpetual_socket
					, transitorySocket
				);
			}
		}
		
		WSASetLastError(errorAccept);
		return resultAccept;
	}
}

// #15
INT WINAPI XSocketSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timeval *timeout)
{
	TRACE_FX();
	
	INT resultSelect = select(nfds, readfds, writefds, exceptfds, timeout);
	int errorSelect = WSAGetLastError();
	if (resultSelect) {
		XLLN_DEBUG_LOG_ECODE(errorSelect, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s select error."
			, __func__
		);
	}
	
	WSASetLastError(errorSelect);
	return resultSelect;
}

// #18
INT WINAPI XSocketRecv(SOCKET perpetual_socket, char *data_buffer, int data_buffer_size, int flags)
{
	TRACE_FX();
	if (flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s flags value (0x%08x) must be 0 on Perpetual Socket 0x%08x."
			, __func__
			, flags
			, perpetual_socket
		);
		
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	
	while (1) {
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		INT resultDataRecvSize = recv(transitorySocket, data_buffer, data_buffer_size, flags);
		int errorRecv = WSAGetLastError();
		
		if (resultDataRecvSize == SOCKET_ERROR && XSocketPerpetualSocketChangedError(errorRecv, perpetual_socket, transitorySocket)) {
			continue;
		}
		
		if (resultDataRecvSize == SOCKET_ERROR) {
			XLLN_DEBUG_LOG_ECODE(errorRecv, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s recv error on socket (P,T) (0x%08x,0x%08x)."
				, __func__
				, perpetual_socket
				, transitorySocket
			);
		}
		else {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s socket (P,T) (0x%08x,0x%08x) data received size 0x%08x."
				, __func__
				, perpetual_socket
				, transitorySocket
				, resultDataRecvSize
			);
		}
		
		WSASetLastError(errorRecv);
		return resultDataRecvSize;
	}
}

static uint16_t xlive_xsocket_large_data_buffer_size = 1024;

// #20
INT WINAPI XSocketRecvFrom(SOCKET perpetual_socket, char *data_buffer, int data_buffer_size, int flags, sockaddr *from, int *fromlen)
{
	TRACE_FX();
	if (flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s flags value (0x%08x) must be 0 on Perpetual Socket 0x%08x."
			, __func__
			, flags
			, perpetual_socket
		);
		
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	
	uint8_t *largeDataBuffer = 0;
	if (xlive_xsocket_large_data_buffer_size && data_buffer_size > 0 && data_buffer_size < xlive_xsocket_large_data_buffer_size) {
		largeDataBuffer = new uint8_t[xlive_xsocket_large_data_buffer_size];
	}
	
	while (1) {
		bool connectionlessSocket = false;
		SOCKADDR_STORAGE sockAddrExternal;
		int sockAddrExternalLen = sizeof(sockAddrExternal);
		
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket, &connectionlessSocket);
		if (transitorySocket == INVALID_SOCKET) {
			if (largeDataBuffer) {
				delete[] largeDataBuffer;
				largeDataBuffer = 0;
			}
			
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		INT resultDataRecvSize = recvfrom(
			transitorySocket
			, (connectionlessSocket && largeDataBuffer) ? (char*)largeDataBuffer : data_buffer
			, (connectionlessSocket && largeDataBuffer) ? xlive_xsocket_large_data_buffer_size : data_buffer_size
			, flags
			, connectionlessSocket ? (sockaddr*)&sockAddrExternal : from
			, connectionlessSocket ? &sockAddrExternalLen : fromlen
		);
		int errorRecvFrom = WSAGetLastError();
		
		if (resultDataRecvSize == SOCKET_ERROR && XSocketPerpetualSocketChangedError(errorRecvFrom, perpetual_socket, transitorySocket)) {
			continue;
		}
		
		if (resultDataRecvSize <= 0) {
			if (errorRecvFrom != WSAEWOULDBLOCK) {
				XLLN_DEBUG_LOG_ECODE(errorRecvFrom, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s recv error on socket (P,T) (0x%08x,0x%08x)."
					, __func__
					, perpetual_socket
					, transitorySocket
				);
			}
			
			if (largeDataBuffer) {
				delete[] largeDataBuffer;
				largeDataBuffer = 0;
			}
			
			WSASetLastError(errorRecvFrom);
			return resultDataRecvSize;
		}
		
		if (!connectionlessSocket) {
			if (largeDataBuffer) {
				delete[] largeDataBuffer;
				largeDataBuffer = 0;
			}
			
			WSASetLastError(errorRecvFrom);
			return resultDataRecvSize;
		}
		
		INT resultDataRecvSizeFromHelper = XSocketRecvFromHelper(
			resultDataRecvSize
			, perpetual_socket
			, largeDataBuffer ? (char*)largeDataBuffer : data_buffer
			, largeDataBuffer ? xlive_xsocket_large_data_buffer_size : data_buffer_size
			, flags
			, &sockAddrExternal
			, sockAddrExternalLen
			, from
			, fromlen
		);
		if (resultDataRecvSizeFromHelper == 0) {
			continue;
		}
		
		if (resultDataRecvSizeFromHelper > 0 && largeDataBuffer) {
			if (resultDataRecvSizeFromHelper > data_buffer_size) {
				delete[] largeDataBuffer;
				largeDataBuffer = 0;
				WSASetLastError(WSAEMSGSIZE);
				return SOCKET_ERROR;
			}
			
			memcpy(data_buffer, largeDataBuffer, resultDataRecvSizeFromHelper);
		}
		
		if (largeDataBuffer) {
			delete[] largeDataBuffer;
			largeDataBuffer = 0;
		}
		
		WSASetLastError(errorRecvFrom);
		return resultDataRecvSizeFromHelper;
	}
}

// #22
INT WINAPI XSocketSend(SOCKET perpetual_socket, const char *data_buffer, int data_buffer_size, int flags)
{
	TRACE_FX();
	if (flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s flags value (0x%08x) must be 0 on Perpetual Socket 0x%08x."
			, __func__
			, flags
			, perpetual_socket
		);
		
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	
	while (1) {
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		INT resultDataSendSize = send(transitorySocket, data_buffer, data_buffer_size, flags);
		int errorSend = WSAGetLastError();
		
		if (resultDataSendSize == SOCKET_ERROR && XSocketPerpetualSocketChangedError(errorSend, perpetual_socket, transitorySocket)) {
			continue;
		}
		
		if (resultDataSendSize == SOCKET_ERROR) {
			XLLN_DEBUG_LOG_ECODE(errorSend, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s send error on socket (P,T) (0x%08x,0x%08x)."
				, __func__
				, perpetual_socket
				, transitorySocket
			);
		}
		else {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s socket (P,T) (0x%08x,0x%08x) data to send size 0x%08x sent size 0x%08x."
				, __func__
				, perpetual_socket
				, transitorySocket
				, data_buffer_size
				, resultDataSendSize
			);
		}
		
		WSASetLastError(errorSend);
		return resultDataSendSize;
	}
}

// #24
INT WINAPI XSocketSendTo(SOCKET perpetual_socket, const char *data_buffer, int data_buffer_size, int flags, sockaddr *to, int tolen)
{
	TRACE_FX();
	if (flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s flags value (0x%08x) must be 0 on Perpetual Socket 0x%08x."
			, __func__
			, flags
			, perpetual_socket
		);
		
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	
	while (1) {
		bool connectionlessSocket = false;
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket, &connectionlessSocket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		if (connectionlessSocket && to && tolen) {
			break;
		}
		
		INT resultDataSendToSize = sendto(transitorySocket, data_buffer, data_buffer_size, flags, to, tolen);
		int errorSendTo = WSAGetLastError();
		
		if (resultDataSendToSize == SOCKET_ERROR && XSocketPerpetualSocketChangedError(errorSendTo, perpetual_socket, transitorySocket)) {
			continue;
		}
		
		if (resultDataSendToSize == SOCKET_ERROR) {
			XLLN_DEBUG_LOG_ECODE(errorSendTo, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s sendto error on socket (P,T) (0x%08x,0x%08x)."
				, __func__
				, perpetual_socket
				, transitorySocket
			);
		}
		else {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s socket (P,T) (0x%08x,0x%08x) data to send size 0x%08x sent size 0x%08x."
				, __func__
				, perpetual_socket
				, transitorySocket
				, data_buffer_size
				, resultDataSendToSize
			);
		}
		
		WSASetLastError(errorSendTo);
		return resultDataSendToSize;
	}
	
	const int packetSizeHeaderType = sizeof(XLLNNetPacketType::TYPE);
	
	int newPacketBufferTitleDataSize = packetSizeHeaderType + data_buffer_size;
	
	// Check overflow condition.
	if (newPacketBufferTitleDataSize < 0) {
		WSASetLastError(WSAEMSGSIZE);
		return SOCKET_ERROR;
	}
	
	const uint32_t ipv4NBO = ((struct sockaddr_in*)to)->sin_addr.s_addr;
	// This address may (hopefully) be an instanceId.
	const uint32_t ipv4HBO = ntohl(ipv4NBO);
	
	uint8_t *newPacketBufferTitleData = new uint8_t[newPacketBufferTitleDataSize];
	int newPacketBufferTitleDataAlreadyProcessedOffset = 0;
	
	XLLNNetPacketType::TYPE &newPacketTitleDataType = *(XLLNNetPacketType::TYPE*)&newPacketBufferTitleData[newPacketBufferTitleDataAlreadyProcessedOffset];
	newPacketBufferTitleDataAlreadyProcessedOffset += packetSizeHeaderType;
	newPacketTitleDataType = (((SOCKADDR_STORAGE*)to)->ss_family == AF_INET && (ipv4HBO == INADDR_BROADCAST || ipv4HBO == INADDR_ANY)) ? XLLNNetPacketType::tTITLE_BROADCAST_PACKET : XLLNNetPacketType::tTITLE_PACKET;
	
	memcpy(&newPacketBufferTitleData[newPacketBufferTitleDataAlreadyProcessedOffset], data_buffer, data_buffer_size);
	
	INT resultDataXllnSendToSize = XllnSocketSendTo(perpetual_socket, (char*)newPacketBufferTitleData, newPacketBufferTitleDataSize, flags, to, tolen);
	int errorXllnSendTo = WSAGetLastError();
	
	delete[] newPacketBufferTitleData;
	
	if (resultDataXllnSendToSize > 0) {
		int32_t unsentDataSize = newPacketBufferTitleDataSize - resultDataXllnSendToSize;
		int32_t newPacketDataSizeDifference = newPacketBufferTitleDataSize - data_buffer_size;
		resultDataXllnSendToSize = data_buffer_size - unsentDataSize;
		if (resultDataXllnSendToSize <= newPacketDataSizeDifference) {
			resultDataXllnSendToSize = 0;
		}
	}
	
	WSASetLastError(errorXllnSendTo);
	return resultDataXllnSendToSize;
}

// #26
long WINAPI XSocketInet_Addr(const char FAR *cp)
{
	TRACE_FX();
	uint8_t iIpParts = 0;
	char *ipStr = CloneString(cp);
	char* ipParts[4];
	ipParts[0] = ipStr;
	for (char *iIpStr = ipStr; *iIpStr != 0; iIpStr++) {
		if (*iIpStr == '.') {
			if (++iIpParts == 4) {
				delete[] ipStr;
				return htonl(INADDR_NONE);
			}
			ipParts[iIpParts] = iIpStr + 1;
			*iIpStr = 0;
			continue;
		}
		if (!(
				(*iIpStr >= '0' && *iIpStr <= '9')
				|| (*iIpStr >= 'a' && *iIpStr <= 'f')
				|| (*iIpStr >= 'A' && *iIpStr <= 'F')
				|| *iIpStr == 'x'
				|| *iIpStr == 'X'
			)
		) {
			delete[] ipStr;
			return htonl(INADDR_NONE);
		}
	}
	if (iIpParts != 3) {
		delete[] ipStr;
		return htonl(INADDR_NONE);
	}

	uint32_t result = 0;

	for (iIpParts = 0; iIpParts < 4; iIpParts++) {
		size_t partLen = strlen(ipParts[iIpParts]);
		if (partLen == 0 || partLen > 4) {
			delete[] ipStr;
			return htonl(INADDR_NONE);
		}
		uint32_t partValue = 0;
		if (ipParts[iIpParts][0] == '0' && (ipParts[iIpParts][1] == 'x' || ipParts[iIpParts][1] == 'X') && ipParts[iIpParts][2] != 0) {
			if (sscanf_s(ipParts[iIpParts], "%x", &partValue) != 1) {
				continue;
			}
		}
		if (ipParts[iIpParts][0] == '0' && ipParts[iIpParts][1] != 0) {
			if (sscanf_s(ipParts[iIpParts], "%o", &partValue) != 1) {
				continue;
			}
		}
		else {
			if (sscanf_s(ipParts[iIpParts], "%u", &partValue) != 1) {
				continue;
			}
		}
		if (partValue > 0xFF) {
			continue;
		}
		result |= (partValue << ((3 - iIpParts) * 8));
	}

	delete[] ipStr;
	return htonl(result);
}

// #27
INT WINAPI XSocketWSAGetLastError()
{
	TRACE_FX();
	INT result = WSAGetLastError();
	return result;
}

// #37
ULONG WINAPI XSocketHTONL(ULONG hostlong)
{
	TRACE_FX();
	ULONG result = htonl(hostlong);
	return result;
}

// #38
USHORT WINAPI XSocketNTOHS(USHORT netshort)
{
	TRACE_FX();
	USHORT result = ntohs(netshort);
	return result;
}

// #39
ULONG WINAPI XSocketNTOHL(ULONG netlong)
{
	TRACE_FX();
	ULONG result = ntohl(netlong);
	return result;
}

// #40
USHORT WINAPI XSocketHTONS(USHORT hostshort)
{
	TRACE_FX();
	USHORT result = htons(hostshort);
	return result;
}

void XLLNCloseCoreSocket()
{
	TRACE_FX();
	
	EnterCriticalSection(&xlive_critsec_sockets);

	if (xlive_xsocket_perpetual_core_socket == INVALID_SOCKET) {
		LeaveCriticalSection(&xlive_critsec_sockets);
		return;
	}

	SOCKET transitorySocket = xlive_xsocket_perpetual_to_transitory_socket[xlive_xsocket_perpetual_core_socket];

	SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[transitorySocket];
	if (socketMappingInfo->perpetualSocket != xlive_xsocket_perpetual_core_socket) {
		xlive_xsocket_perpetual_to_transitory_socket.erase(xlive_xsocket_perpetual_core_socket);
		XllnWndSocketsInvalidateSockets();
		LeaveCriticalSection(&xlive_critsec_sockets);
		return;
	}

	LeaveCriticalSection(&xlive_critsec_sockets);

	INT resultSocketShutdown = XSocketShutdown(xlive_xsocket_perpetual_core_socket, SD_RECEIVE);
	if (resultSocketShutdown) {
		int errorSocketShutdown = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorSocketShutdown, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
			, "%s Failed to shutdown Core Socket with error:"
			, __func__
		);
	}

	INT resultSocketClose = XSocketClose(xlive_xsocket_perpetual_core_socket);
	if (resultSocketClose) {
		int errorSocketClose = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorSocketClose, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
			, "%s Failed to close Core Socket with error:"
			, __func__
		);
	}

	xlive_xsocket_perpetual_core_socket = INVALID_SOCKET;
}

static void XLLNCreateCoreSocket()
{
	TRACE_FX();
	
	XLLNCloseCoreSocket();
	
	if (xlive_netsocket_abort) {
		return;
	}
	
	xlive_xsocket_perpetual_core_socket = XSocketCreate(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (xlive_xsocket_perpetual_core_socket == INVALID_SOCKET) {
		int errorSocketCreate = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorSocketCreate, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
			, "%s Failed to create Core Socket with error:"
			, __func__
		);
		return;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_INFO
		, "%s Perpetual Core Socket created as 0x%08x."
		, __func__
		, xlive_xsocket_perpetual_core_socket
	);
	
	uint32_t sockOptValue = 1;
	INT resultSetSockOpt = XSocketSetSockOpt(xlive_xsocket_perpetual_core_socket, SOL_SOCKET, SO_BROADCAST, (const char *)&sockOptValue, sizeof(sockOptValue));
	if (resultSetSockOpt == SOCKET_ERROR) {
		int errorSetSockOpt = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorSetSockOpt, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
			, "%s Failed to set Broadcast option on Core Socket with error:"
			, __func__
		);
		return;
	}
	
	sockaddr_in socketBindAddress;
	socketBindAddress.sin_family = AF_INET;
	socketBindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	if (IsUsingBasePort(xlive_base_port)) {
		socketBindAddress.sin_port = htons(xlive_base_port);
	}
	else {
		socketBindAddress.sin_port = htons(xlive_port_online);
	}
	SOCKET resultSocketBind = XSocketBind(xlive_xsocket_perpetual_core_socket, (sockaddr*)&socketBindAddress, sizeof(socketBindAddress));
	if (resultSocketBind) {
		int errorSocketBind = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorSocketBind, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
			, "%s Failed to bind Core Socket to port %hu with error:"
			, __func__
			, ntohs(socketBindAddress.sin_port)
		);
		return;
	}
}

BOOL InitXSocket()
{
	TRACE_FX();
	
	XLLNCreateCoreSocket();
	XLLNKeepAliveStart();
	XLiveThreadQosStart();
	
	return TRUE;
}

BOOL UninitXSocket()
{
	TRACE_FX();
	
	XLiveThreadQosStop();
	XLLNCloseCoreSocket();
	XLLNKeepAliveStop();
	
	return TRUE;
}
