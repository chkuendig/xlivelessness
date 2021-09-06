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
#include "net-entity.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"
#include "../resource.h"
#include <ctime>
#include <WS2tcpip.h>

#define IPPROTO_VDP 254

WORD xlive_base_port = 0;
HANDLE xlive_base_port_mutex = 0;
BOOL xlive_netsocket_abort = FALSE;

CRITICAL_SECTION xlive_critsec_sockets;
std::map<SOCKET, SOCKET_MAPPING_INFO*> xlive_socket_info;
static std::map<uint16_t, SOCKET> xlive_port_offset_sockets;

CRITICAL_SECTION xlive_critsec_broadcast_addresses;
std::vector<XLLNBroadcastEntity::BROADCAST_ENTITY> xlive_broadcast_addresses;

VOID SendUnknownUserAskRequest(SOCKET socket, const char* data, int dataLen, const SOCKADDR_STORAGE *sockAddrExternal, const int sockAddrExternalLen, bool isAsking, uint32_t instanceIdConsumeRemaining)
{
	if (isAsking) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "Send UNKNOWN_USER_ASK."
		);
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "Send UNKNOWN_USER_REPLY."
		);
	}
	const int32_t cpHeaderLen = sizeof(XLLNNetPacketType::TYPE);
	const int32_t userAskRequestLen = cpHeaderLen + sizeof(XLLNNetPacketType::NET_USER_PACKET);
	const int32_t userAskPacketLen = userAskRequestLen + dataLen;

	uint8_t *packetBuffer = new uint8_t[userAskPacketLen];
	packetBuffer[0] = isAsking ? XLLNNetPacketType::tUNKNOWN_USER_ASK : XLLNNetPacketType::tUNKNOWN_USER_REPLY;
	XLLNNetPacketType::NET_USER_PACKET &nea = *(XLLNNetPacketType::NET_USER_PACKET*)&packetBuffer[cpHeaderLen];
	nea.instanceId = ntohl(xlive_local_xnAddr.inaOnline.s_addr);
	nea.portBaseHBO = xlive_base_port;
	nea.instanceIdConsumeRemaining = instanceIdConsumeRemaining;
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[socket];
		nea.socketInternalPortHBO = socketMappingInfo->portOgHBO;
		nea.socketInternalPortOffsetHBO = socketMappingInfo->portOffsetHBO;
		LeaveCriticalSection(&xlive_critsec_sockets);
	}

	// Copy the extra data if any onto the back.
	if (userAskPacketLen != userAskRequestLen) {
		memcpy_s(&packetBuffer[userAskRequestLen], dataLen, data, dataLen);
	}

	INT bytesSent = sendto(socket, (char*)packetBuffer, userAskPacketLen, 0, (const sockaddr*)sockAddrExternal, sockAddrExternalLen);

	if (bytesSent <= 0 && userAskPacketLen != userAskRequestLen) {
		// Send only UNKNOWN_USER_<?>.
		bytesSent = sendto(socket, (char*)packetBuffer, userAskRequestLen, 0, (const sockaddr*)sockAddrExternal, sockAddrExternalLen);
	}

	delete[] packetBuffer;
}

static VOID CustomMemCpy(void *dst, void *src, rsize_t len, bool directionAscending)
{
	if (directionAscending) {
		for (rsize_t i = 0; i < len; i++) {
			((BYTE*)dst)[i] = ((BYTE*)src)[i];
		}
	}
	else {
		if (len > 0) {
			for (rsize_t i = len - 1; true; i--) {
				((BYTE*)dst)[i] = ((BYTE*)src)[i];
				if (i == 0) {
					break;
				}
			}
		}
	}
}

// #3
SOCKET WINAPI XSocketCreate(int af, int type, int protocol)
{
	TRACE_FX();
	if (af != AF_INET) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s af (%d) must be AF_INET (%d).", __func__, af, AF_INET);
		WSASetLastError(WSAEAFNOSUPPORT);
		return SOCKET_ERROR;
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s type (%d) must be either SOCK_STREAM (%d) or SOCK_DGRAM (%d).", __func__, type, SOCK_STREAM, SOCK_DGRAM);
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}

	SOCKET result = socket(af, type, protocol);

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
		, "XSocketCreate: 0x%08x. AF: %d. Type: %d. Protocol: %d."
		, result
		, af
		, type
		, protocol
	);

	if (result == INVALID_SOCKET) {
		DWORD errorSocketCreate = GetLastError();
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "Socket create failed: 0x%08x. AF: %d. Type: %d. Protocol: %d."
			, errorSocketCreate
			, af
			, type
			, protocol
		);
	}
	else {
		if (vdp) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
				, "VDP socket created: 0x%08x."
				, result
			);
		}

		SOCKET_MAPPING_INFO* socketMappingInfo = new SOCKET_MAPPING_INFO;
		socketMappingInfo->socket = result;
		socketMappingInfo->type = type;
		socketMappingInfo->protocol = protocol;
		socketMappingInfo->isVdpProtocol = vdp;

		EnterCriticalSection(&xlive_critsec_sockets);
		xlive_socket_info[result] = socketMappingInfo;
		XllnWndSocketsInvalidateSockets();
		LeaveCriticalSection(&xlive_critsec_sockets);
	}

	return result;
}

// #4
INT WINAPI XSocketClose(SOCKET s)
{
	TRACE_FX();
	INT result = closesocket(s);

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
		, "XSocketClose Socket: 0x%08x."
		, s
	);

	if (result) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "XSocketClose error 0x%08x. Socket: 0x%08x."
			, result
			, s
		);
	}

	{
		SOCKET_MAPPING_INFO *socketMappingInfo = 0;
		EnterCriticalSection(&xlive_critsec_sockets);
		if (xlive_socket_info.count(s)) {
			socketMappingInfo = xlive_socket_info[s];
			xlive_socket_info.erase(s);
			if (socketMappingInfo->portOffsetHBO != -1) {
				xlive_port_offset_sockets.erase(socketMappingInfo->portOffsetHBO);
			}
		}
		XllnWndSocketsInvalidateSockets();
		LeaveCriticalSection(&xlive_critsec_sockets);

		if (socketMappingInfo) {
			delete socketMappingInfo;
		}
	}

	return result;
}

// #5
INT WINAPI XSocketShutdown(SOCKET s, int how)
{
	TRACE_FX();
	INT result = shutdown(s, how);

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
		, "XSocketShutdown Socket: 0x%08x. how: %d."
		, s
		, how
	);

	if (result) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "XSocketShutdown error 0x%08x. Socket: 0x%08x. how: %d."
			, result
			, s
			, how
		);
	}
	
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		if (xlive_socket_info.count(s)) {
			SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[s];
			if (socketMappingInfo->portOffsetHBO != -1) {
				xlive_port_offset_sockets.erase(socketMappingInfo->portOffsetHBO);
			}
			socketMappingInfo->portBindHBO = 0;
		}
		XllnWndSocketsInvalidateSockets();
		LeaveCriticalSection(&xlive_critsec_sockets);
	}

	return result;
}

// #6
INT WINAPI XSocketIOCTLSocket(SOCKET s, __int32 cmd, ULONG *argp)
{
	TRACE_FX();
	INT result = ioctlsocket(s, cmd, argp);
	return result;
}

// #7
INT WINAPI XSocketSetSockOpt(SOCKET s, int level, int optname, const char *optval, int optlen)
{
	TRACE_FX();

	if (level == SOL_SOCKET && optname == SO_BROADCAST && optlen) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "XSocketSetSockOpt SO_BROADCAST 0x%02hhx."
			, *optval
		);
		EnterCriticalSection(&xlive_critsec_sockets);
		SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[s];
		socketMappingInfo->broadcast = *optval > 0;
		XllnWndSocketsInvalidateSockets();
		LeaveCriticalSection(&xlive_critsec_sockets);
	}

	INT result = setsockopt(s, level, optname, optval, optlen);
	if (result == SOCKET_ERROR) {
		DWORD errorSocketOpt = GetLastError();
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "XSocketSetSockOpt error 0x%08x on socket 0x%08x."
			, s
			, errorSocketOpt
		);
	}

	return result;
}

// #8
INT WINAPI XSocketGetSockOpt(SOCKET s, int level, int optname, char *optval, int *optlen)
{
	TRACE_FX();
	INT result = getsockopt(s, level, optname, optval, optlen);
	return result;
}

// #9
INT WINAPI XSocketGetSockName(SOCKET s, struct sockaddr *name, int *namelen)
{
	TRACE_FX();
	INT result = getsockname(s, name, namelen);
	return result;
}

// #10
INT WINAPI XSocketGetPeerName(SOCKET s, struct sockaddr *name, int *namelen)
{
	TRACE_FX();
	INT result = getpeername(s, name, namelen);
	return result;
}

// #11
SOCKET WINAPI XSocketBind(SOCKET s, const struct sockaddr *name, int namelen)
{
	TRACE_FX();

	SOCKADDR_STORAGE sockAddrExternal;
	int sockAddrExternalLen = sizeof(sockAddrExternal);
	memcpy(&sockAddrExternal, name, sockAddrExternalLen < namelen ? sockAddrExternalLen : namelen);

	if (sockAddrExternal.ss_family != AF_INET && sockAddrExternal.ss_family != AF_INET6) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "Socket 0x%08x bind. Unknown socket address family 0x%04x."
			, s
			, sockAddrExternal.ss_family
		);

		SOCKET result = bind(s, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);

		if (result != ERROR_SUCCESS) {
			DWORD errorSocketBind = GetLastError();

			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "XSocketBind Socket 0x%08x with unknown socket address family 0x%04x had bind error: 0x%08x."
				, s
				, sockAddrExternal.ss_family
				, errorSocketBind
			);
		}

		return result;
	}

	uint16_t portHBO = GetSockAddrPort(&sockAddrExternal);
	const uint16_t portOffset = portHBO % 100;
	const uint16_t portBase = portHBO - portOffset;
	uint16_t portShiftedHBO = 0;

	if (IsUsingBasePort(xlive_base_port) && portBase % 1000 == 0) {
		portShiftedHBO = xlive_base_port + portOffset;
		SetSockAddrPort(&sockAddrExternal, portShiftedHBO);
	}

	if (sockAddrExternal.ss_family == AF_INET && ((struct sockaddr_in*)&sockAddrExternal)->sin_addr.s_addr == htonl(INADDR_ANY)) {
		EnterCriticalSection(&xlive_critsec_network_adapter);
		// TODO Should we perhaps not do this unless the user really wants to like if (xlive_init_preferred_network_adapter_name or config) is set?
		if (xlive_network_adapter && xlive_network_adapter->unicastHAddr != INADDR_LOOPBACK) {
			((struct sockaddr_in*)&sockAddrExternal)->sin_addr.s_addr = htonl(xlive_network_adapter->unicastHAddr);
		}
		LeaveCriticalSection(&xlive_critsec_network_adapter);
	}

	SOCKET result = bind(s, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);

	if (result == ERROR_SUCCESS) {
		uint16_t portBindHBO = portHBO;
		INT result = getsockname(s, (sockaddr*)&sockAddrExternal, &sockAddrExternalLen);
		if (result == ERROR_SUCCESS) {
			portBindHBO = GetSockAddrPort(&sockAddrExternal);
			if (portHBO != portBindHBO) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
					, "Socket 0x%08x bind port mapped from %hu to %hu."
					, s
					, portHBO
					, portBindHBO
				);
			}
		}
		else {
			INT errorGetsockname = WSAGetLastError();
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "Socket 0x%08x bind port mapped from %hu to unknown address from getsockname with error 0x%08x."
				, s
				, portHBO
				, errorGetsockname
			);
		}

		int protocol;
		bool isVdp;
		if (portShiftedHBO) {
			EnterCriticalSection(&xlive_critsec_sockets);

			SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[s];
			socketMappingInfo->portBindHBO = portBindHBO;
			socketMappingInfo->portOgHBO = portHBO;
			socketMappingInfo->portOffsetHBO = portOffset;
			protocol = socketMappingInfo->protocol;
			isVdp = socketMappingInfo->isVdpProtocol;
			xlive_port_offset_sockets[portOffset] = s;

			XllnWndSocketsInvalidateSockets();

			LeaveCriticalSection(&xlive_critsec_sockets);
		}
		else {
			EnterCriticalSection(&xlive_critsec_sockets);

			SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[s];
			socketMappingInfo->portBindHBO = portBindHBO;
			socketMappingInfo->portOgHBO = portHBO;
			socketMappingInfo->portOffsetHBO = -1;
			protocol = socketMappingInfo->protocol;
			isVdp = socketMappingInfo->isVdpProtocol;

			XllnWndSocketsInvalidateSockets();

			LeaveCriticalSection(&xlive_critsec_sockets);
		}

		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
			, "Socket 0x%08x bind. Protocol: %s%d%s. Port: %hu."
			, s
			, protocol == IPPROTO_UDP ? "UDP:" : (protocol == IPPROTO_TCP ? "TCP:" : "")
			, protocol
			, isVdp ? " was VDP" : ""
			, portHBO
		);
	}
	else {
		DWORD errorSocketBind = GetLastError();
		if (errorSocketBind == WSAEADDRINUSE) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "Socket 0x%08x bind error, another program has taken port:\nBase: %hu.\nOffset: %hu.\nNew-shifted: %hu.\nOriginal: %hu."
				, s
				, xlive_base_port
				, portOffset
				, portShiftedHBO
				, portHBO
			);
		}
		else {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "Socket 0x%08x bind error: 0x%08x.\nBase: %hu.\nOffset: %hu.\nNew-shifted: %hu.\nOriginal: %hu."
				, s
				, errorSocketBind
				, xlive_base_port
				, portOffset
				, portShiftedHBO
				, portHBO
			);
		}
	}

	return result;
}

// #12
INT WINAPI XSocketConnect(SOCKET s, const struct sockaddr *name, int namelen)
{
	TRACE_FX();

	SOCKADDR_STORAGE sockAddrExternal;
	int sockAddrExternalLen = sizeof(sockAddrExternal);
	memcpy(&sockAddrExternal, name, sockAddrExternalLen < namelen ? sockAddrExternalLen : namelen);

	if (sockAddrExternal.ss_family == AF_INET) {
		const uint32_t ipv4NBO = ((struct sockaddr_in*)&sockAddrExternal)->sin_addr.s_addr;
		// This address may (hopefully) be an instanceId.
		const uint32_t ipv4HBO = ntohl(ipv4NBO);
		const uint16_t portHBO = GetSockAddrPort(&sockAddrExternal);

		uint32_t resultNetter = NetterEntityGetAddrByInstanceIdPort(&sockAddrExternal, ipv4HBO, portHBO);
		if (resultNetter == ERROR_PORT_NOT_SET) {
			char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "XSocketConnect on socket 0x%08x NetterEntityGetAddrByInstanceIdPort ERROR_PORT_NOT_SET address/instanceId 0x%08x:%hu assuming %s."
				, s
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
				, "XSocketConnect on socket 0x%08x NetterEntityGetAddrByInstanceIdPort failed to find address 0x%08x:%hu with error 0x%08x."
				, s
				, ipv4HBO
				, portHBO
				, resultNetter
			);
		}
		else {
			char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "XSocketConnect on socket 0x%08x NetterEntityGetAddrByInstanceIdPort found address/instanceId 0x%08x:%hu as %s."
				, s
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
			, "XSocketConnect on socket 0x%08x connecting to address %s."
			, s
			, sockAddrInfo ? sockAddrInfo : ""
		);
		if (sockAddrInfo) {
			free(sockAddrInfo);
		}
	}

	INT result = connect(s, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);

	if (result) {
		INT errorSendTo = WSAGetLastError();
		char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "XllnSocketSendTo on socket 0x%08x sendto() failed to send to address %s with error 0x%08x."
			, s
			, sockAddrInfo ? sockAddrInfo : ""
			, errorSendTo
		);
		if (sockAddrInfo) {
			free(sockAddrInfo);
		}
	}

	return result;
}

// #13
INT WINAPI XSocketListen(SOCKET s, int backlog)
{
	TRACE_FX();
	INT result = listen(s, backlog);
	return result;
}

// #14
SOCKET WINAPI XSocketAccept(SOCKET s, struct sockaddr *addr, int *addrlen)
{
	TRACE_FX();
	SOCKET result = accept(s, addr, addrlen);
	return result;
}

// #15
INT WINAPI XSocketSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timeval *timeout)
{
	TRACE_FX();
	INT result = select(nfds, readfds, writefds, exceptfds, timeout);
	return result;
}

// #18
INT WINAPI XSocketRecv(SOCKET s, char * buf, int len, int flags)
{
	TRACE_FX();
	if (flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s flags must be 0.", __func__);
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	INT result = recv(s, buf, len, flags);
	if (result == SOCKET_ERROR) {
		int errorRecv = WSAGetLastError();
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s socket (0x%08x) recv failed with error 0x%08x."
			, __func__
			, s
			, errorRecv
		);
		WSASetLastError(errorRecv);
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "%s socket (0x%08x)."
			, __func__
			, s
		);
	}
	return result;
}

INT WINAPI XSocketRecvFromHelper(const int dataRecvSize, const SOCKET socket, char *dataBuffer, const int dataBufferSize, const int flags, const SOCKADDR_STORAGE *sockAddrExternal, const int sockAddrExternalLen, sockaddr *sockAddrXlive, int *sockAddrXliveLen)
{
	TRACE_FX();
	
	const int packetSizeHeaderType = sizeof(XLLNNetPacketType::TYPE);
	const int packetSizeTypeForwarded = sizeof(XLLNNetPacketType::PACKET_FORWARDED);
	const int packetSizeTypeUnknownUser = sizeof(XLLNNetPacketType::UNKNOWN_USER);
	const int packetSizeTypeHubRequest = sizeof(XLLNNetPacketType::HUB_REQUEST_PACKET);
	const int packetSizeTypeHubReply = sizeof(XLLNNetPacketType::HUB_REPLY_PACKET);
	
	int packetSizeAlreadyProcessedOffset = 0;
	int resultDataRecvSize = 0;
	// In use if packetForwardedSockAddrSize is set.
	SOCKADDR_STORAGE packetForwardedSockAddr;
	// In use if packetForwardedSockAddrSize is set.
	XLLNNetPacketType::NET_USER_PACKET packetForwardedNetter;
	int packetForwardedSockAddrSize = 0;
	bool canSendUnknownUserAsk = true;
	XLLNNetPacketType::TYPE priorPacketType = XLLNNetPacketType::tUNKNOWN;
	
	while (dataRecvSize > packetSizeAlreadyProcessedOffset) {
		if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeHeaderType) {
			packetSizeAlreadyProcessedOffset = dataRecvSize;
			break;
		}
		
		XLLNNetPacketType::TYPE &packetType = *(XLLNNetPacketType::TYPE*)&dataBuffer[packetSizeAlreadyProcessedOffset];
		packetSizeAlreadyProcessedOffset += packetSizeHeaderType;
		
		priorPacketType = packetType;
		switch (packetType) {
			case XLLNNetPacketType::tTITLE_BROADCAST_PACKET:
			case XLLNNetPacketType::tTITLE_PACKET: {
				const int dataSizeToShift = dataRecvSize - packetSizeAlreadyProcessedOffset;
				CustomMemCpy(dataBuffer, (void*)((int)dataBuffer + packetSizeAlreadyProcessedOffset), dataSizeToShift, true);
				
				resultDataRecvSize = dataSizeToShift;
				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}
			case XLLNNetPacketType::tPACKET_FORWARDED: {
				if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeTypeForwarded) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s Invalid %s received (insufficient size)."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[packetType]
					);
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
				
				XLLNNetPacketType::PACKET_FORWARDED &packetForwarded = *(XLLNNetPacketType::PACKET_FORWARDED*)&dataBuffer[packetSizeAlreadyProcessedOffset];
				packetSizeAlreadyProcessedOffset += packetSizeTypeForwarded;
				
				packetForwardedSockAddrSize = sizeof(SOCKADDR_STORAGE);
				memcpy(&packetForwardedSockAddr, &packetForwarded.originSockAddr, packetForwardedSockAddrSize);
				memcpy(&packetForwardedNetter, &packetForwarded.netter, sizeof(XLLNNetPacketType::NET_USER_PACKET));
				
				break;
			}
			case XLLNNetPacketType::tCUSTOM_OTHER: {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Unsupported %s received (unimplemented)."
					, __func__
					, XLLNNetPacketType::TYPE_NAMES[packetType]
				);
				
				// TODO re-implement this.
				//XSocketRecvFromCustomHelper();
				
				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}
			case XLLNNetPacketType::tUNKNOWN_USER_ASK:
			case XLLNNetPacketType::tUNKNOWN_USER_REPLY: {
				if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeTypeUnknownUser) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s Invalid %s received (insufficient size)."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[packetType]
					);
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
				
				canSendUnknownUserAsk = false;
				
				XLLNNetPacketType::UNKNOWN_USER &packetUnknownUser = *(XLLNNetPacketType::UNKNOWN_USER*)&dataBuffer[packetSizeAlreadyProcessedOffset];
				packetSizeAlreadyProcessedOffset += packetSizeTypeUnknownUser;
				
				uint32_t resultNetter = NetterEntityEnsureExists(packetUnknownUser.netter.instanceId, packetUnknownUser.netter.portBaseHBO);
				if (resultNetter) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s Error on received %s. Failed to create Net Entity: 0x%08x:%hu with error 0x%08x."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[packetType]
						, packetUnknownUser.netter.instanceId
						, packetUnknownUser.netter.portBaseHBO
						, resultNetter
					);
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
				
				resultNetter = NetterEntityAddAddrByInstanceId(
					packetUnknownUser.netter.instanceId
					, packetUnknownUser.netter.socketInternalPortHBO
					, packetUnknownUser.netter.socketInternalPortOffsetHBO
					, packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal
				);
				if (resultNetter) {
					char *sockAddrInfo = GET_SOCKADDR_INFO(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s Error on received %s. Failed to add addr (0x%04x:%hu %s) to Net Entity 0x%08x:%hu with error 0x%08x."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[packetType]
						, packetUnknownUser.netter.socketInternalPortHBO
						, packetUnknownUser.netter.socketInternalPortOffsetHBO
						, sockAddrInfo ? sockAddrInfo : ""
						, packetUnknownUser.netter.instanceId
						, packetUnknownUser.netter.portBaseHBO
						, resultNetter
					);
					if (sockAddrInfo) {
						free(sockAddrInfo);
					}
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
				
				const int dataSizeRemaining = dataRecvSize - packetSizeAlreadyProcessedOffset;
				uint32_t instanceIdConsumeRemaining = packetUnknownUser.netter.instanceIdConsumeRemaining;
				
				// Mutate/reuse the incomming packet buffer so we do not need to allocate a new one temporarily.
				if (packetType == XLLNNetPacketType::tUNKNOWN_USER_ASK) {
					packetType = XLLNNetPacketType::tUNKNOWN_USER_REPLY;
					
					packetUnknownUser.netter.instanceId = ntohl(xlive_local_xnAddr.inaOnline.s_addr);
					packetUnknownUser.netter.portBaseHBO = xlive_base_port;
					{
						EnterCriticalSection(&xlive_critsec_sockets);
						SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[socket];
						packetUnknownUser.netter.socketInternalPortHBO = socketMappingInfo->portOgHBO;
						packetUnknownUser.netter.socketInternalPortOffsetHBO = socketMappingInfo->portOffsetHBO;
						LeaveCriticalSection(&xlive_critsec_sockets);
					}
					
					int newPacketSize = packetSizeHeaderType + packetSizeTypeUnknownUser;
					bool withExtraPayload = false;
					// If the packet was only meant for this instance then discard the additional data.
					if (instanceIdConsumeRemaining == ntohl(xlive_local_xnAddr.inaOnline.s_addr)) {
						packetUnknownUser.netter.instanceIdConsumeRemaining = 0;
					}
					else {
						newPacketSize += dataSizeRemaining;
						withExtraPayload = true;
					}
					
					{
						char *sockAddrInfo = GET_SOCKADDR_INFO(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
							, "%s Sending %s to Net Entity (0x%08x:%hu %s)%s."
							, __func__
							, XLLNNetPacketType::TYPE_NAMES[packetType]
							, packetUnknownUser.netter.instanceId
							, packetUnknownUser.netter.portBaseHBO
							, sockAddrInfo ? sockAddrInfo : ""
							, withExtraPayload ? " with extra payload" : ""
						);
						if (sockAddrInfo) {
							free(sockAddrInfo);
						}
					}
					
					int bytesSent = sendto(
						socket
						, (char*)&packetType
						, newPacketSize
						, 0
						, (const sockaddr*)(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal)
						, packetForwardedSockAddrSize ? packetForwardedSockAddrSize : sockAddrExternalLen
					);
					
					packetType = XLLNNetPacketType::tUNKNOWN_USER_ASK;
				}
				
				// If the packet was not meant for this instance then discard the additional data.
				if (!(instanceIdConsumeRemaining == 0 || instanceIdConsumeRemaining == ntohl(xlive_local_xnAddr.inaOnline.s_addr))) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
						, "%s Received %s."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[packetType]
					);
					
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
				
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					, "%s Consuming %s extra payload data (size %d)."
					, __func__
					, XLLNNetPacketType::TYPE_NAMES[packetType]
					, dataSizeRemaining
				);
				
				break;
			}
			case XLLNNetPacketType::tLIVE_OVER_LAN_ADVERTISE:
			case XLLNNetPacketType::tLIVE_OVER_LAN_UNADVERTISE: {
				canSendUnknownUserAsk = false;
				int dataSizeRemaining = dataRecvSize - packetSizeAlreadyProcessedOffset + 1;
				
				// FIXME This function assumes the buffer includes the packet type.
				LiveOverLanRecieve(
					socket
					, packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal
					, packetForwardedSockAddrSize ? packetForwardedSockAddrSize : sockAddrExternalLen
					, (const LIVE_SERVER_DETAILS*)&dataBuffer[packetSizeAlreadyProcessedOffset - 1]
					, dataSizeRemaining
				);
				
				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}
			case XLLNNetPacketType::tHUB_REQUEST: {
				if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeTypeHubRequest) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s Invalid %s received (insufficient size)."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[packetType]
					);
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
				
				XLLNNetPacketType::HUB_REQUEST_PACKET &packetHubRequest = *(XLLNNetPacketType::HUB_REQUEST_PACKET*)&dataBuffer[packetSizeAlreadyProcessedOffset];
				packetSizeAlreadyProcessedOffset += packetSizeTypeHubRequest;
				
				int newPacketBufferHubReplySize = packetSizeHeaderType + packetSizeTypeHubReply;
				uint8_t *newPacketBufferHubReply = new uint8_t[newPacketBufferHubReplySize];
				int newPacketBufferHubReplyAlreadyProcessedOffset = 0;
				
				XLLNNetPacketType::TYPE &newPacketHubReplyType = *(XLLNNetPacketType::TYPE*)&newPacketBufferHubReply[newPacketBufferHubReplyAlreadyProcessedOffset];
				newPacketBufferHubReplyAlreadyProcessedOffset += packetSizeHeaderType;
				newPacketHubReplyType = XLLNNetPacketType::tHUB_REPLY;
				
				XLLNNetPacketType::HUB_REPLY_PACKET &newPacketHubReplyReply = *(XLLNNetPacketType::HUB_REPLY_PACKET*)&newPacketBufferHubReply[newPacketBufferHubReplyAlreadyProcessedOffset];
				newPacketBufferHubReplyAlreadyProcessedOffset += packetSizeTypeHubReply;
				newPacketHubReplyReply.isHubServer = false;
				newPacketHubReplyReply.xllnVersion = (DLL_VERSION_MAJOR << 24) + (DLL_VERSION_MINOR << 16) + (DLL_VERSION_REVISION << 8) + DLL_VERSION_BUILD;
				newPacketHubReplyReply.recommendedInstanceId = 0;
				
				if (newPacketBufferHubReplyAlreadyProcessedOffset != newPacketBufferHubReplySize) {
					__debugbreak();
				}
				
				{
					char *sockAddrInfo = GET_SOCKADDR_INFO(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
						, "%s Sending %s to %s."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[XLLNNetPacketType::tHUB_REPLY]
						, sockAddrInfo ? sockAddrInfo : ""
					);
					if (sockAddrInfo) {
						free(sockAddrInfo);
					}
				}
				
				int bytesSent = sendto(
					socket
					, (char*)newPacketBufferHubReply
					, newPacketBufferHubReplySize
					, 0
					, (const sockaddr*)(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal)
					, packetForwardedSockAddrSize ? packetForwardedSockAddrSize : sockAddrExternalLen
				);
				
				delete[] newPacketBufferHubReply;
				
				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}
			case XLLNNetPacketType::tHUB_REPLY: {
				EnterCriticalSection(&xlive_critsec_broadcast_addresses);
				for (XLLNBroadcastEntity::BROADCAST_ENTITY &broadcastEntity : xlive_broadcast_addresses) {
					if (SockAddrsMatch(&broadcastEntity.sockaddr, packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal)) {
						if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeTypeHubReply) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
								, "%s Invalid %s received (insufficient size)."
								, __func__
								, XLLNNetPacketType::TYPE_NAMES[packetType]
							);
							break;
						}
						
						XLLNNetPacketType::HUB_REPLY_PACKET &packetHubReply = *(XLLNNetPacketType::HUB_REPLY_PACKET*)&dataBuffer[packetSizeAlreadyProcessedOffset];
						packetSizeAlreadyProcessedOffset += packetSizeTypeHubReply;
						
						if (broadcastEntity.entityType != XLLNBroadcastEntity::TYPE::tBROADCAST_ADDR) {
							broadcastEntity.entityType = packetHubReply.isHubServer != 0 ? XLLNBroadcastEntity::TYPE::tHUB_SERVER : XLLNBroadcastEntity::TYPE::tOTHER_CLIENT;
						}
						_time64(&broadcastEntity.lastComm);
						
						if (packetHubReply.recommendedInstanceId && packetHubReply.recommendedInstanceId != ntohl(xlive_local_xnAddr.inaOnline.s_addr)) {
							char *sockAddrInfo = GetSockAddrInfo(&broadcastEntity.sockaddr);
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
								, "%s HUB_REPLY_PACKET from %s recommends a different Instance ID (from 0x%08x to 0x%08x)."
								, __func__
								, sockAddrInfo ? sockAddrInfo : "?"
								, ntohl(xlive_local_xnAddr.inaOnline.s_addr)
								, packetHubReply.recommendedInstanceId
							);
							free(sockAddrInfo);
						}
						
						{
							char *sockAddrInfo = GET_SOCKADDR_INFO(&broadcastEntity.sockaddr);
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
								, "%s HUB_REPLY_PACKET from %s entityType:%s."
								, __func__
								, sockAddrInfo ? sockAddrInfo : "?"
								, XLLNBroadcastEntity::TYPE_NAMES[broadcastEntity.entityType]
							);
							if (sockAddrInfo) {
								free(sockAddrInfo);
							}
						}
						
						break;
					}
				}
				LeaveCriticalSection(&xlive_critsec_broadcast_addresses);
				
				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}
			default: {
				const int dataSizeRemaining = dataRecvSize - packetSizeAlreadyProcessedOffset;
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Unknown packet (0x%02hhx) received with data content size (%d)."
					, __func__
					, packetType
					, dataSizeRemaining
				);
				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}
		}
	}
	
	if (resultDataRecvSize < 0) {
		__debugbreak();
	}
	if (resultDataRecvSize == 0) {
		return 0;
	}
	
	uint32_t instanceId = 0;
	uint16_t portHBO = 0;
	uint32_t resultNetter = NetterEntityGetInstanceIdPortByExternalAddr(&instanceId, &portHBO, packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
	if (resultNetter) {
		char *sockAddrInfo = GET_SOCKADDR_INFO(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s NetterEntityGetInstanceIdPortByExternalAddr failed to find external addr %s with error 0x%08x."
			, __func__
			, sockAddrInfo ? sockAddrInfo : ""
			, resultNetter
		);
		if (sockAddrInfo) {
			free(sockAddrInfo);
		}
		
		if (canSendUnknownUserAsk) {
			int newPacketBufferUnknownUserSize = packetSizeHeaderType + resultDataRecvSize;
			uint8_t *newPacketBufferUnknownUser = new uint8_t[newPacketBufferUnknownUserSize];
			int newPacketBufferUnknownUserAlreadyProcessedOffset = 0;
			
			XLLNNetPacketType::TYPE &newPacketUnknownUserType = *(XLLNNetPacketType::TYPE*)&newPacketBufferUnknownUser[newPacketBufferUnknownUserAlreadyProcessedOffset];
			newPacketBufferUnknownUserAlreadyProcessedOffset += packetSizeHeaderType;
			newPacketUnknownUserType = priorPacketType == XLLNNetPacketType::tTITLE_BROADCAST_PACKET ? XLLNNetPacketType::tTITLE_BROADCAST_PACKET : XLLNNetPacketType::tTITLE_PACKET;
			
			memcpy(&newPacketBufferUnknownUser[newPacketBufferUnknownUserAlreadyProcessedOffset], dataBuffer, resultDataRecvSize);
			
			SendUnknownUserAskRequest(
				socket
				, (char*)newPacketBufferUnknownUser
				, newPacketBufferUnknownUserSize
				, packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal
				, packetForwardedSockAddrSize ? packetForwardedSockAddrSize : sockAddrExternalLen
				, true
				, ntohl(xlive_local_xnAddr.inaOnline.s_addr)
			);
			
			delete[] newPacketBufferUnknownUser;
		}
		
		if (packetForwardedSockAddrSize && packetForwardedNetter.instanceId) {
			sockaddr_in* sockAddrIpv4Xlive = ((struct sockaddr_in*)sockAddrXlive);
			sockAddrIpv4Xlive->sin_family = AF_INET;
			sockAddrIpv4Xlive->sin_addr.s_addr = htonl(packetForwardedNetter.instanceId);
			sockAddrIpv4Xlive->sin_port = htons(packetForwardedNetter.socketInternalPortHBO);
			*sockAddrXliveLen = sizeof(sockaddr_in);
		}
		else {
			// We do not want whatever this was being passed to the Title.
			return 0;
		}
	}
	else {
		char *sockAddrInfo = GET_SOCKADDR_INFO(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "%s NetterEntityGetInstanceIdPortByExternalAddr found external addr %s as instanceId:0x%08x port:%hu."
			, __func__
			, sockAddrInfo ? sockAddrInfo : ""
			, instanceId
			, portHBO
		);
		if (sockAddrInfo) {
			free(sockAddrInfo);
		}
		
		sockaddr_in* sockAddrIpv4Xlive = ((struct sockaddr_in*)sockAddrXlive);
		sockAddrIpv4Xlive->sin_family = AF_INET;
		sockAddrIpv4Xlive->sin_addr.s_addr = htonl(instanceId);
		sockAddrIpv4Xlive->sin_port = htons(portHBO);
		*sockAddrXliveLen = sizeof(sockaddr_in);
	}
	
	return resultDataRecvSize;
}

// #20
INT WINAPI XSocketRecvFrom(SOCKET socket, char *dataBuffer, int dataBufferSize, int flags, sockaddr *from, int *fromlen)
{
	TRACE_FX();
	if (flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s flags must be 0.", __func__);
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	SOCKADDR_STORAGE sockAddrExternal;
	int sockAddrExternalLen = sizeof(sockAddrExternal);
	INT resultDataRecvSize = recvfrom(socket, dataBuffer, dataBufferSize, flags, (sockaddr*)&sockAddrExternal, &sockAddrExternalLen);
	if (resultDataRecvSize <= 0) {
		return resultDataRecvSize;
	}
	INT result = XSocketRecvFromHelper(resultDataRecvSize, socket, dataBuffer, dataBufferSize, flags, &sockAddrExternal, sockAddrExternalLen, from, fromlen);
	if (result == 0) { // && resultDataRecvSize > 0
		WSASetLastError(WSAEWOULDBLOCK);
		return SOCKET_ERROR;
	}
	return result;
}

// #22
INT WINAPI XSocketSend(SOCKET s, const char *buf, int len, int flags)
{
	TRACE_FX();
	if (flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s flags must be 0.", __func__);
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	INT result = send(s, buf, len, flags);
	if (result == SOCKET_ERROR) {
		int errorSend = WSAGetLastError();
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s socket (0x%08x) send failed with error 0x%08x."
			, __func__
			, s
			, errorSend
		);
		WSASetLastError(errorSend);
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "%s socket (0x%08x)."
			, __func__
			, s
		);
	}
	return result;
}

INT WINAPI XllnSocketSendTo(SOCKET s, const char *buf, int len, int flags, sockaddr *to, int tolen)
{
	TRACE_FX();
	if (flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s flags must be 0.", __func__);
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	INT result = SOCKET_ERROR;

	SOCKADDR_STORAGE sockAddrExternal;
	int sockAddrExternalLen = sizeof(sockAddrExternal);
	memcpy(&sockAddrExternal, to, sockAddrExternalLen < tolen ? sockAddrExternalLen : tolen);

	const uint32_t ipv4NBO = ((struct sockaddr_in*)&sockAddrExternal)->sin_addr.s_addr;
	// This address may (hopefully) be an instanceId.
	const uint32_t ipv4HBO = ntohl(ipv4NBO);
	const uint16_t portHBO = GetSockAddrPort(&sockAddrExternal);

	if (sockAddrExternal.ss_family == AF_INET && (ipv4HBO == INADDR_BROADCAST || ipv4HBO == INADDR_ANY)) {
		{
			char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s socket (0x%08x) broadcasting packet for %s."
				, __func__
				, s
				, sockAddrInfo ? sockAddrInfo : ""
			);
			if (sockAddrInfo) {
				free(sockAddrInfo);
			}
		}

		bool broadcastOverriden = false;
		{
			EnterCriticalSection(&xlive_critsec_broadcast_addresses);

			if (xlive_broadcast_addresses.size()) {
				broadcastOverriden = true;
				for (const XLLNBroadcastEntity::BROADCAST_ENTITY &broadcastEntity : xlive_broadcast_addresses) {
					memcpy(&sockAddrExternal, &broadcastEntity.sockaddr, sockAddrExternalLen);
					if (sockAddrExternal.ss_family == AF_INET) {
						if (ntohs(((sockaddr_in*)&sockAddrExternal)->sin_port) == 0) {
							((sockaddr_in*)&sockAddrExternal)->sin_port = htons(portHBO);
						}
					}
					else if (sockAddrExternal.ss_family == AF_INET6) {
						if (ntohs(((sockaddr_in6*)&sockAddrExternal)->sin6_port) == 0) {
							((sockaddr_in6*)&sockAddrExternal)->sin6_port = htons(portHBO);
						}
					}
					else {
						// If it's not IPv4 or IPv6 then do not sent it.
						continue;
					}

					{
						char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
							, "%s socket (0x%08x) broadcasting packet to %s."
							, __func__
							, s
							, sockAddrInfo ? sockAddrInfo : ""
						);
						if (sockAddrInfo) {
							free(sockAddrInfo);
						}
					}

					result = sendto(s, buf, len, 0, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);
				}
			}

			LeaveCriticalSection(&xlive_critsec_broadcast_addresses);
		}

		if (!broadcastOverriden) {
			sockAddrExternal.ss_family = AF_INET;
			{
				EnterCriticalSection(&xlive_critsec_network_adapter);
				if (xlive_network_adapter) {
					((struct sockaddr_in*)&sockAddrExternal)->sin_addr.s_addr = htonl(xlive_network_adapter->hBroadcast);
				}
				LeaveCriticalSection(&xlive_critsec_network_adapter);
			}
			if (IsUsingBasePort(xlive_base_port)) {
				const uint16_t portOffset = portHBO % 100;
				const uint16_t portBase = portHBO - portOffset;

				for (uint16_t portBaseInc = 1000; portBaseInc <= 6000; portBaseInc += 1000) {
					((struct sockaddr_in*)&sockAddrExternal)->sin_port = htons(portBaseInc + portOffset);

					{
						char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
							, "%s socket (0x%08x) broadcasting packet to %s."
							, __func__
							, s
							, sockAddrInfo ? sockAddrInfo : ""
						);
						if (sockAddrInfo) {
							free(sockAddrInfo);
						}
					}

					result = sendto(s, buf, len, 0, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);
				}
			}
			else {
				{
					char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
						, "%s socket (0x%08x) broadcasting packet to %s."
						, __func__
						, s
						, sockAddrInfo ? sockAddrInfo : ""
					);
					if (sockAddrInfo) {
						free(sockAddrInfo);
					}
				}
				
				result = sendto(s, buf, len, 0, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);
			}
		}

		result = len;
	}
	else if (sockAddrExternal.ss_family == AF_INET) {

		uint32_t resultNetter = NetterEntityGetAddrByInstanceIdPort(&sockAddrExternal, ipv4HBO, portHBO);
		if (resultNetter) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s socket (0x%08x) NetterEntityGetAddrByInstanceIdPort failed to find address 0x%08x:%hu with error 0x%08x."
				, __func__
				, s
				, ipv4HBO
				, portHBO
				, resultNetter
			);

			if (resultNetter == ERROR_PORT_NOT_SET) {
				if (sockAddrExternal.ss_family == AF_INET || sockAddrExternal.ss_family == AF_INET6) {
					SendUnknownUserAskRequest(s, buf, len, &sockAddrExternal, sockAddrExternalLen, true, ipv4HBO);
				}
			}

			result = len;
		}
		else {
			char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s socket (0x%08x) NetterEntityGetAddrByInstanceIdPort found address/instanceId 0x%08x:%hu as %s."
				, __func__
				, s
				, ipv4HBO
				, portHBO
				, sockAddrInfo ? sockAddrInfo : ""
			);

			result = sendto(s, buf, len, flags, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);

			if (result == SOCKET_ERROR) {
				INT errorSendTo = WSAGetLastError();
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s socket (0x%08x) sendto() failed to send to address %s with error 0x%08x."
					, __func__
					, s
					, sockAddrInfo ? sockAddrInfo : ""
					, errorSendTo
				);
			}

			if (sockAddrInfo) {
				free(sockAddrInfo);
			}
		}
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s socket (0x%08x) unsupported sendto socket family %hu."
			, __func__
			, s
			, sockAddrExternal.ss_family
		);
	}

	return result;
}

// #24
INT WINAPI XSocketSendTo(SOCKET s, const char *buf, int len, int flags, sockaddr *to, int tolen)
{
	TRACE_FX();
	INT result = SOCKET_ERROR;

	const size_t altBufLen = len + 1;
	// Check overflow condition.
	if (altBufLen < 0) {
		WSASetLastError(WSAEMSGSIZE);
		return SOCKET_ERROR;
	}

	char *altBuf = new char[altBufLen];

	const uint32_t ipv4NBO = ((struct sockaddr_in*)to)->sin_addr.s_addr;
	// This address may (hopefully) be an instanceId.
	const uint32_t ipv4HBO = ntohl(ipv4NBO);

	altBuf[0] = (((SOCKADDR_STORAGE*)to)->ss_family == AF_INET && (ipv4HBO == INADDR_BROADCAST || ipv4HBO == INADDR_ANY)) ? XLLNNetPacketType::tTITLE_BROADCAST_PACKET : XLLNNetPacketType::tTITLE_PACKET;
	memcpy_s(&altBuf[1], altBufLen-1, buf, len);

	result = XllnSocketSendTo(s, altBuf, altBufLen, flags, to, tolen);

	delete[] altBuf;

	if (result - (altBufLen - len) >= 0) {
		result -= (altBufLen - len);
	}

	return result;
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

BOOL InitXSocket()
{
	XLLNKeepAliveStart();
	return TRUE;
}

BOOL UninitXSocket()
{
	XLLNKeepAliveStop();
	return TRUE;
}
