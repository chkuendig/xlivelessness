#include <winsock2.h>
#include "xdefs.hpp"
#include "packet-handler.hpp"
#include "live-over-lan.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/wnd-main.hpp"
#include "../xlln/debug-text.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"
#include "../utils/sha256.hpp"
#include "net-entity.hpp"
#include "xsocket.hpp"
#include "xnet.hpp"
#include "xnetqos.hpp"
#include "xnotify.hpp"
#include "xlive.hpp"
#include "../resource.h"
#include "xlocator.hpp"
#include "xsession.hpp"
#include <ctime>
#include <WS2tcpip.h>
#include <set>

CRITICAL_SECTION xlive_critsec_broadcast_addresses;
std::vector<XLLNBroadcastEntity::BROADCAST_ENTITY> xlive_broadcast_addresses;

// sock_addr_forwarded is optional if is_forwarded is true.
// wrap_data_in is tUNKNOWN if packet data is already wrapped.
void SendUnknownUserPacket(SOCKET perpetual_socket, const char* data_buffer, int data_buffer_size, XLLNNetPacketType::TYPE wrap_data_in, bool is_unknown_user_ask, const SOCKADDR_STORAGE *sock_addr_origin, bool is_forwarded, const SOCKADDR_STORAGE *sock_addr_forwarded, uint32_t instanceId_consume_remaining)
{
	switch (wrap_data_in) {
		case XLLNNetPacketType::tTITLE_PACKET:
		case XLLNNetPacketType::tTITLE_BROADCAST_PACKET:
		case XLLNNetPacketType::tUNKNOWN:
			break;
		default: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s Unsupported wrap_data_in XLLNNetPacketType received (%s)."
				, __func__
				, XLLNNetPacketType::TYPE_NAMES[wrap_data_in]
			);
			return;
		}
	}
	
	const int packetSizeHeaderType = sizeof(XLLNNetPacketType::TYPE);
	const int packetSizeTypeNetUser = sizeof(XLLNNetPacketType::NET_USER_PACKET);
	const int packetSizeTypeHubRelay = sizeof(XLLNNetPacketType::HUB_RELAY);
	
	const int newPacketBufferHubRelaySizePart = (is_forwarded ? packetSizeHeaderType + packetSizeTypeHubRelay : 0);
	const int newPacketBufferUnknownUserSizePart = packetSizeHeaderType + packetSizeTypeNetUser;
	const int newPacketBufferOriginalPacketHeaderSizePart = ((wrap_data_in == XLLNNetPacketType::tUNKNOWN) ? 0 : packetSizeHeaderType);
	const int newPacketBufferOriginalPacketSizePart = newPacketBufferOriginalPacketHeaderSizePart + data_buffer_size;
	const int newPacketBufferSize = newPacketBufferHubRelaySizePart + newPacketBufferUnknownUserSizePart + newPacketBufferOriginalPacketSizePart;
	
	uint8_t *newPacketBuffer = new uint8_t[newPacketBufferSize];
	
	XLLNNetPacketType::NET_USER_PACKET *packetDataHubRelayNetter = 0;
	if (newPacketBufferHubRelaySizePart) {
		newPacketBuffer[0] = XLLNNetPacketType::tHUB_RELAY;
		XLLNNetPacketType::HUB_RELAY &packetDataHubRelay = *(XLLNNetPacketType::HUB_RELAY*)&newPacketBuffer[packetSizeHeaderType];
		
		packetDataHubRelay.destSockAddr = *sock_addr_origin;
		
		packetDataHubRelay.netterOrigin.instanceId = ntohl(xlive_local_xnAddr.inaOnline.s_addr);
		packetDataHubRelay.netterOrigin.portBaseHBO = xlive_base_port;
		packetDataHubRelay.netterOrigin.instanceIdConsumeRemaining = 0;// unused.
		
		packetDataHubRelayNetter = &packetDataHubRelay.netterOrigin;
	}
	
	if (is_unknown_user_ask) {
		newPacketBuffer[newPacketBufferHubRelaySizePart] = XLLNNetPacketType::tUNKNOWN_USER_ASK;
	}
	else {
		newPacketBuffer[newPacketBufferHubRelaySizePart] = XLLNNetPacketType::tUNKNOWN_USER_REPLY;
	}
	XLLNNetPacketType::NET_USER_PACKET &packetDataNetUser = *(XLLNNetPacketType::NET_USER_PACKET*)&newPacketBuffer[newPacketBufferHubRelaySizePart + packetSizeHeaderType];
	
	if (wrap_data_in != XLLNNetPacketType::tUNKNOWN) {
		newPacketBuffer[newPacketBufferHubRelaySizePart + newPacketBufferUnknownUserSizePart] = wrap_data_in;
	}
	memcpy(&newPacketBuffer[newPacketBufferHubRelaySizePart + newPacketBufferUnknownUserSizePart + newPacketBufferOriginalPacketHeaderSizePart], data_buffer, data_buffer_size);
	
	packetDataNetUser.instanceId = ntohl(xlive_local_xnAddr.inaOnline.s_addr);
	packetDataNetUser.portBaseHBO = xlive_base_port;
	packetDataNetUser.instanceIdConsumeRemaining = instanceId_consume_remaining;
	
	while (1) {
		SOCKET transitorySocket = INVALID_SOCKET;
		{
			EnterCriticalSection(&xlive_critsec_sockets);
			
			if (xlive_xsocket_perpetual_to_transitory_socket.count(perpetual_socket)) {
				transitorySocket = xlive_xsocket_perpetual_to_transitory_socket[perpetual_socket];
				
				SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[transitorySocket];
				packetDataNetUser.socketInternalPortHBO = socketMappingInfo->portOgHBO == 0 ? socketMappingInfo->portBindHBO : socketMappingInfo->portOgHBO;
				packetDataNetUser.socketInternalPortOffsetHBO = socketMappingInfo->portOffsetHBO;
				if (packetDataHubRelayNetter) {
					packetDataHubRelayNetter->socketInternalPortHBO = packetDataNetUser.socketInternalPortHBO;
					packetDataHubRelayNetter->socketInternalPortOffsetHBO = packetDataNetUser.socketInternalPortOffsetHBO;
				}
			}
			
			LeaveCriticalSection(&xlive_critsec_sockets);
		}
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			break;
		}
		
		INT resultDataSendSize = sendto(
			transitorySocket
			, (char*)newPacketBuffer
			, newPacketBufferSize
			, 0
			, (const sockaddr*)(is_forwarded ? sock_addr_forwarded : sock_addr_origin)
			, is_forwarded ? sizeof(*sock_addr_forwarded) : sizeof(*sock_addr_origin)
		);
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
		
		//WSASetLastError(errorSend);
		//resultDataSendSize;
		break;
	}
	
	delete[] newPacketBuffer;
}

// Process connectionless sockets only (UDP). sockAddrXlive as well as sockAddrXliveLen may be null.
INT WINAPI XSocketRecvFromHelper(
	const int dataRecvSize
	, const SOCKET perpetual_socket
	, char *dataBuffer
	, const int dataBufferSize
	, const int flags
	, const SOCKADDR_STORAGE *sockAddrExternal
	, const int sockAddrExternalLen
	, sockaddr *sockAddrXlive
	, int *sockAddrXliveLen
)
{
	TRACE_FX();
	
	const int packetSizeHeaderType = sizeof(XLLNNetPacketType::TYPE);
	const int packetSizeTypeForwarded = sizeof(XLLNNetPacketType::PACKET_FORWARDED);
	const int packetSizeTypeUnknownUser = sizeof(XLLNNetPacketType::UNKNOWN_USER);
	const int packetSizeTypeNetUser = sizeof(XLLNNetPacketType::NET_USER_PACKET);
	const int packetSizeTypeHubRequest = sizeof(XLLNNetPacketType::HUB_REQUEST_PACKET);
	const int packetSizeTypeHubReply = sizeof(XLLNNetPacketType::HUB_REPLY_PACKET);
	const int packetSizeTypeLiveOverLanUnadvertise = sizeof(XLLNNetPacketType::LIVE_OVER_LAN_UNADVERTISE);
	const int packetSizeTypeQosRequest = sizeof(XLLNNetPacketType::QOS_REQUEST);
	const int packetSizeTypeQosResponse = sizeof(XLLNNetPacketType::QOS_RESPONSE);
	const int packetSizeTypeHubRelay = sizeof(XLLNNetPacketType::HUB_RELAY);
	const int packetSizeTypeDirectIpRequest = sizeof(XLLNNetPacketType::DIRECT_IP_REQUEST);
	const int packetSizeTypeDirectIpResponse = sizeof(XLLNNetPacketType::DIRECT_IP_RESPONSE);
	
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
				memcpy(&packetForwardedNetter, &packetForwarded.netter, packetSizeTypeNetUser);
				
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
				bool thisInstanceConsumeDestroys = instanceIdConsumeRemaining == ntohl(xlive_local_xnAddr.inaOnline.s_addr);
				
				uint32_t originInstanceId = packetUnknownUser.netter.instanceId;
				uint16_t originPortBase = packetUnknownUser.netter.portBaseHBO;
				
				if (packetType == XLLNNetPacketType::tUNKNOWN_USER_ASK) {
					bool isForwarded = packetForwardedSockAddrSize != 0;
					SendUnknownUserPacket(
						perpetual_socket
						, &dataBuffer[packetSizeAlreadyProcessedOffset]
						, thisInstanceConsumeDestroys ? 0 : dataSizeRemaining
						, XLLNNetPacketType::tUNKNOWN
						, false
						, isForwarded ? &packetForwardedSockAddr : sockAddrExternal
						, isForwarded
						, sockAddrExternal
						, thisInstanceConsumeDestroys ? 0 : instanceIdConsumeRemaining
					);
				}
				
				// If the packet was not meant for this instance then discard the additional data.
				if (!(instanceIdConsumeRemaining == 0 || instanceIdConsumeRemaining == ntohl(xlive_local_xnAddr.inaOnline.s_addr))) {
					{
						char *sockAddrInfo = GET_SOCKADDR_INFO(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
							, "%s Received %s from Net Entity (0x%08x:%hu %s)"
							, __func__
							, XLLNNetPacketType::TYPE_NAMES[priorPacketType]
							, originInstanceId
							, originPortBase
							, sockAddrInfo ? sockAddrInfo : ""
						);
						if (sockAddrInfo) {
							free(sockAddrInfo);
						}
					}
					
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
				
				{
					char *sockAddrInfo = GET_SOCKADDR_INFO(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
						, "%s Consuming %s from Net Entity (0x%08x:%hu %s) with extra payload data (size %d)."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[priorPacketType]
						, originInstanceId
						, originPortBase
						, sockAddrInfo ? sockAddrInfo : ""
						, dataSizeRemaining
					);
					if (sockAddrInfo) {
						free(sockAddrInfo);
					}
				}
				
				break;
			}
			case XLLNNetPacketType::tLIVE_OVER_LAN_ADVERTISE:
			case XLLNNetPacketType::tLIVE_OVER_LAN_UNADVERTISE: {
				canSendUnknownUserAsk = false;
				
				// Verify that the original sender (if it was forwarded) is a trusted Hub server.
				if (packetForwardedSockAddrSize) {
					// TODO Verify that the original sender (if it was forwarded) is a trusted Hub server.
					if (false) {
						packetSizeAlreadyProcessedOffset = dataRecvSize;
						break;
					}
				}
				
				uint32_t instanceId = 0;
				uint16_t portHBO = 0;
				uint32_t resultNetter = NetterEntityGetInstanceIdPortByExternalAddr(&instanceId, &portHBO, packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
				if (resultNetter) {
					char *sockAddrInfo = GET_SOCKADDR_INFO(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
					XLLN_DEBUG_LOG_ECODE(resultNetter, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_ERROR
						, "%s NetterEntityGetInstanceIdPortByExternalAddr failed from %s with error:"
						, __func__
						, sockAddrInfo ? sockAddrInfo : "?"
					);
					if (sockAddrInfo) {
						free(sockAddrInfo);
					}
					
					int dataSizeRemaining = dataRecvSize - packetSizeAlreadyProcessedOffset + packetSizeHeaderType;
					
					SendUnknownUserPacket(
						perpetual_socket
						, &dataBuffer[packetSizeAlreadyProcessedOffset - packetSizeHeaderType]
						, dataSizeRemaining
						, XLLNNetPacketType::tUNKNOWN
						, true
						, packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal
						, false
						, 0
						, ntohl(xlive_local_xnAddr.inaOnline.s_addr)
					);
					
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
				
				if (packetType == XLLNNetPacketType::tLIVE_OVER_LAN_UNADVERTISE) {
					if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeTypeLiveOverLanUnadvertise) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
							, "%s Invalid %s received (insufficient size)."
							, __func__
							, XLLNNetPacketType::TYPE_NAMES[packetType]
						);
						packetSizeAlreadyProcessedOffset = dataRecvSize;
						break;
					}
					
					XLLNNetPacketType::LIVE_OVER_LAN_UNADVERTISE &packetLiveOverLanUnadvertise = *(XLLNNetPacketType::LIVE_OVER_LAN_UNADVERTISE*)&dataBuffer[packetSizeAlreadyProcessedOffset];
					LiveOverLanBroadcastRemoteSessionUnadvertise(instanceId, packetLiveOverLanUnadvertise.sessionType, packetLiveOverLanUnadvertise.xuid);
					
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
				
				const uint8_t *liveSessionBuffer = (uint8_t*)&dataBuffer[packetSizeAlreadyProcessedOffset];
				const int liveSessionBufferSize = dataRecvSize - packetSizeAlreadyProcessedOffset;
				LIVE_SESSION *liveSession;
				
				if (!LiveOverLanDeserialiseLiveSession(liveSessionBuffer, liveSessionBufferSize, &liveSession)) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s Invalid %s received."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[packetType]
					);
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
				
				LiveOverLanAddRemoteLiveSession(instanceId, liveSession->sessionType, liveSession);
				
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
				
				int bytesSent = SendToPerpetualSocket(
					perpetual_socket
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
							char *sockAddrInfo = GET_SOCKADDR_INFO(&broadcastEntity.sockaddr);
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
								, "%s HUB_REPLY_PACKET from %s recommends a different Instance ID (from 0x%08x to 0x%08x)."
								, __func__
								, sockAddrInfo ? sockAddrInfo : "?"
								, ntohl(xlive_local_xnAddr.inaOnline.s_addr)
								, packetHubReply.recommendedInstanceId
							);
							if (sockAddrInfo) {
								free(sockAddrInfo);
							}
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
			case XLLNNetPacketType::tQOS_REQUEST: {
				canSendUnknownUserAsk = false;
				
				if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeTypeQosRequest) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s Invalid %s received (insufficient size)."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[packetType]
					);
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
				
				XLLNNetPacketType::QOS_REQUEST &packetQosRequest = *(XLLNNetPacketType::QOS_REQUEST*)&dataBuffer[packetSizeAlreadyProcessedOffset];
				
				XLiveQosReceiveRequest(
					&packetQosRequest
					, perpetual_socket
					, packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal
					, packetForwardedSockAddrSize ? packetForwardedSockAddrSize : sockAddrExternalLen
				);
				
				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}
			case XLLNNetPacketType::tQOS_RESPONSE: {
				canSendUnknownUserAsk = false;
				
				if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeTypeQosResponse) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s Invalid %s received (insufficient size)."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[packetType]
					);
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
				
				XLLNNetPacketType::QOS_RESPONSE &packetQosResponse = *(XLLNNetPacketType::QOS_RESPONSE*)&dataBuffer[packetSizeAlreadyProcessedOffset];
				
				if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeTypeQosResponse + packetQosResponse.sizeData) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s Invalid %s received (insufficient size with extra data payload)."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[packetType]
					);
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
				
				XLiveQosReceiveResponse(&packetQosResponse);
				
				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}
			case XLLNNetPacketType::tDIRECT_IP_REQUEST: {
				if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeTypeDirectIpRequest) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
						, "%s Invalid %s received (insufficient size)."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[packetType]
					);
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
				
				XLLNNetPacketType::DIRECT_IP_REQUEST &packetDirectIpRequest = *(XLLNNetPacketType::DIRECT_IP_REQUEST*)&dataBuffer[packetSizeAlreadyProcessedOffset];
				
				bool correctPassword = false;
				{
					char hostPassword[500];
					GetDlgItemTextA(xlln_window_hwnd, MYWINDOW_TBX_DIRECT_IP_CONNECT_PASSWORD, hostPassword, 500);
					
					uint8_t hostPasswordSha256[32];
					memset(hostPasswordSha256, 0, sizeof(hostPasswordSha256));
					
					uint32_t hostPasswordLen = strlen(hostPassword);
					if (hostPasswordLen) {
						mbedtls_sha256((uint8_t*)hostPassword, hostPasswordLen, hostPasswordSha256, 0);
					}
					
					if (memcmp(hostPasswordSha256, packetDirectIpRequest.passwordSha256, sizeof(hostPasswordSha256)) == 0) {
						correctPassword = true;
					}
				}
				
				{
					char *sockAddrInfo = GET_SOCKADDR_INFO(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_DEBUG
						, "%s Sending %s to %s %s."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[XLLNNetPacketType::tDIRECT_IP_RESPONSE]
						, sockAddrInfo ? sockAddrInfo : ""
						, correctPassword ? "having correct password" : "having incorrect password"
					);
					if (sockAddrInfo) {
						free(sockAddrInfo);
					}
				}
				
				{
					uint8_t *liveSessionSerialisedPacket = 0;
					uint32_t liveSessionSerialisedPacketSize = 0;
					
					if (correctPassword) {
						EnterCriticalSection(&xlln_critsec_liveoverlan_broadcast);
						if (xlive_xlocator_local_session) {
							if (!LiveOverLanSerialiseLiveSessionIntoNetPacket(xlive_xlocator_local_session, &liveSessionSerialisedPacket, &liveSessionSerialisedPacketSize)) {
								XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_DEBUG
									, "%s Failed to serialise Live Session for %s packet."
									, __func__
									, XLLNNetPacketType::TYPE_NAMES[XLLNNetPacketType::tDIRECT_IP_RESPONSE]
								);
							}
						}
						else {
							for (auto const &entry : xlive_xsession_local_sessions) {
								if (!(entry.second->liveSession->sessionType == XLLN_LIVEOVERLAN_SESSION_TYPE_XSESSION && entry.second->liveSession->sessionFlags & XSESSION_CREATE_HOST)) {
									continue;
								}
								
								if (!LiveOverLanSerialiseLiveSessionIntoNetPacket(entry.second->liveSession, &liveSessionSerialisedPacket, &liveSessionSerialisedPacketSize)) {
									XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_DEBUG
										, "%s Failed to serialise Live Session for %s packet."
										, __func__
										, XLLNNetPacketType::TYPE_NAMES[XLLNNetPacketType::tDIRECT_IP_RESPONSE]
									);
								}
								
								break;
							}
						}
						LeaveCriticalSection(&xlln_critsec_liveoverlan_broadcast);
					}
					
					if (liveSessionSerialisedPacket) {
						const int packetSize = packetSizeHeaderType + packetSizeTypeDirectIpResponse + (liveSessionSerialisedPacketSize - packetSizeHeaderType);
						
						uint8_t *packetBuffer = new uint8_t[packetSize];
						packetBuffer[0] = XLLNNetPacketType::tDIRECT_IP_RESPONSE;
						XLLNNetPacketType::DIRECT_IP_RESPONSE &directIpResponse = *(XLLNNetPacketType::DIRECT_IP_RESPONSE*)&packetBuffer[packetSizeHeaderType];
						directIpResponse.joinRequestSignature = packetDirectIpRequest.joinRequestSignature;
						directIpResponse.instanceId = ntohl(xlive_local_xnAddr.inaOnline.s_addr);
						directIpResponse.titleId = xlive_title_id;
						
						memcpy(&packetBuffer[packetSizeHeaderType + packetSizeTypeDirectIpResponse], &liveSessionSerialisedPacket[packetSizeHeaderType], liveSessionSerialisedPacketSize - packetSizeHeaderType);
						
						delete[] liveSessionSerialisedPacket;
						liveSessionSerialisedPacket = 0;
						
						SendUnknownUserPacket(
							perpetual_socket
							, (char*)packetBuffer
							, packetSize
							, XLLNNetPacketType::tUNKNOWN
							, false
							, (packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal)
							, false
							, 0
							, 0
						);
						
						delete[] packetBuffer;
					}
					else {
						const int packetSize = packetSizeHeaderType + packetSizeTypeDirectIpResponse;
						
						uint8_t *packetBuffer = new uint8_t[packetSize];
						packetBuffer[0] = XLLNNetPacketType::tDIRECT_IP_RESPONSE;
						XLLNNetPacketType::DIRECT_IP_RESPONSE &directIpResponse = *(XLLNNetPacketType::DIRECT_IP_RESPONSE*)&packetBuffer[packetSizeHeaderType];
						directIpResponse.joinRequestSignature = packetDirectIpRequest.joinRequestSignature;
						directIpResponse.instanceId = 0;
						directIpResponse.titleId = 0;
						
						int bytesSent = SendToPerpetualSocket(
							perpetual_socket
							, (char*)packetBuffer
							, packetSize
							, 0
							, (const sockaddr*)(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal)
							, packetForwardedSockAddrSize ? packetForwardedSockAddrSize : sockAddrExternalLen
						);
						
						delete[] packetBuffer;
					}
				}
				
				packetSizeAlreadyProcessedOffset = dataRecvSize;
				break;
			}
			case XLLNNetPacketType::tDIRECT_IP_RESPONSE: {
				if (dataRecvSize < packetSizeAlreadyProcessedOffset + packetSizeTypeDirectIpResponse) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
						, "%s Invalid %s received (insufficient size)."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[packetType]
					);
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
				
				XLLNNetPacketType::DIRECT_IP_RESPONSE &packetDirectIpResponse = *(XLLNNetPacketType::DIRECT_IP_RESPONSE*)&dataBuffer[packetSizeAlreadyProcessedOffset];
				
				if (packetDirectIpResponse.joinRequestSignature != xlln_direct_ip_connect.joinRequestSignature) {
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
				if (!packetDirectIpResponse.instanceId) {
					char *sockAddrInfo = GET_SOCKADDR_INFO(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_DEBUG
						, "%s Recieved %s from %s stating no available lobby or password was wrong."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[XLLNNetPacketType::tDIRECT_IP_RESPONSE]
						, sockAddrInfo ? sockAddrInfo : ""
					);
					if (sockAddrInfo) {
						free(sockAddrInfo);
					}
					
					XllnDirectIpConnectCancel();
					MessageBoxW(xlln_window_hwnd, L"There is no available lobby or the password was wrong.", L"XLLN Direct IP Connect Error", MB_OK);
					
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
				
				{
					char *sockAddrInfo = GET_SOCKADDR_INFO(packetForwardedSockAddrSize ? &packetForwardedSockAddr : sockAddrExternal);
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_DEBUG
						, "%s Recieved %s from %s."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[packetType]
						, sockAddrInfo ? sockAddrInfo : ""
					);
					if (sockAddrInfo) {
						free(sockAddrInfo);
					}
				}
				
				xlln_direct_ip_connect.remoteInstanceId = packetDirectIpResponse.instanceId;
				xlln_direct_ip_connect.remoteTitleId = packetDirectIpResponse.titleId;
				
				packetSizeAlreadyProcessedOffset += packetSizeTypeDirectIpResponse;
				
				const uint8_t *liveSessionBuffer = (uint8_t*)&dataBuffer[packetSizeAlreadyProcessedOffset];
				const int liveSessionBufferSize = dataRecvSize - packetSizeAlreadyProcessedOffset;
				LIVE_SESSION *liveSession;
				
				if (!LiveOverLanDeserialiseLiveSession(liveSessionBuffer, liveSessionBufferSize, &liveSession)) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
						, "%s Invalid %s received when parsing Live Session."
						, __func__
						, XLLNNetPacketType::TYPE_NAMES[packetType]
					);
					packetSizeAlreadyProcessedOffset = dataRecvSize;
					break;
				}
				
				xlln_direct_ip_connect.remoteXuid = liveSession->xuid;
				xlln_direct_ip_connect.remoteSessionId = liveSession->xnkid;
				xlln_direct_ip_connect.remoteKeyExchangeKey = liveSession->xnkey;
				
				LiveOverLanAddRemoteLiveSession(packetDirectIpResponse.instanceId, liveSession->sessionType, liveSession);
				
				xlln_direct_ip_connect.timeoutAt = 0;
				
				XLiveNotifyAddEvent(XN_LIVE_INVITE_ACCEPTED, xlln_direct_ip_connect.localPlayerId);
				
				EnableWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_DIRECT_IP_CONNECT), true);
				
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
	
	{
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
				bool isForwarded = packetForwardedSockAddrSize != 0;
				SendUnknownUserPacket(
					perpetual_socket
					, dataBuffer
					, resultDataRecvSize
					, priorPacketType // either going to be XLLNNetPacketType::tTITLE_BROADCAST_PACKET or XLLNNetPacketType::tTITLE_PACKET
					, true
					, isForwarded ? &packetForwardedSockAddr : sockAddrExternal
					, isForwarded
					, sockAddrExternal
					, ntohl(xlive_local_xnAddr.inaOnline.s_addr)
				);
			}
			
			if (packetForwardedSockAddrSize && packetForwardedNetter.instanceId) {
				if (sockAddrXlive && sockAddrXliveLen) {
					sockaddr_in* sockAddrIpv4Xlive = ((struct sockaddr_in*)sockAddrXlive);
					sockAddrIpv4Xlive->sin_family = AF_INET;
					sockAddrIpv4Xlive->sin_addr.s_addr = htonl(packetForwardedNetter.instanceId);
					sockAddrIpv4Xlive->sin_port = htons(packetForwardedNetter.socketInternalPortHBO);
					*sockAddrXliveLen = sizeof(sockaddr_in);
				}
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
			
			if (sockAddrXlive && sockAddrXliveLen) {
				sockaddr_in* sockAddrIpv4Xlive = ((struct sockaddr_in*)sockAddrXlive);
				sockAddrIpv4Xlive->sin_family = AF_INET;
				sockAddrIpv4Xlive->sin_addr.s_addr = htonl(instanceId);
				sockAddrIpv4Xlive->sin_port = htons(portHBO);
				*sockAddrXliveLen = sizeof(sockaddr_in);
			}
		}
	}
	
	return resultDataRecvSize;
}

bool XllnSocketBroadcastTo(INT *resultBroadcastTo, SOCKET perpetual_socket, const char *dataBuffer, int dataSendSize, int flags, const sockaddr *to, int tolen)
{
	TRACE_FX();
	
	SOCKADDR_STORAGE sockAddrExternal;
	int sockAddrExternalLen = sizeof(sockAddrExternal);
	memcpy(&sockAddrExternal, to, sockAddrExternalLen < tolen ? sockAddrExternalLen : tolen);
	
	const uint32_t ipv4NBO = ((struct sockaddr_in*)&sockAddrExternal)->sin_addr.s_addr;
	// This address may (hopefully) be an instanceId.
	const uint32_t ipv4HBO = ntohl(ipv4NBO);
	const uint16_t portHBO = GetSockAddrPort(&sockAddrExternal);
	
	if (sockAddrExternal.ss_family == AF_INET && (ipv4HBO == INADDR_BROADCAST || ipv4HBO == INADDR_ANY)) {
		INT resultSend = SOCKET_ERROR;
		int errorSend = WSAGetLastError();
		
		{
			char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s Perpetual Socket (0x%08x) broadcasting packet for %s."
				, __func__
				, perpetual_socket
				, sockAddrInfo ? sockAddrInfo : ""
			);
			if (sockAddrInfo) {
				free(sockAddrInfo);
			}
		}
		
		{
			EnterCriticalSection(&xlive_critsec_broadcast_addresses);
			
			// Add broadcast address with wildcard port if no addresses exist.
			while (!xlive_broadcast_addresses.size()) {
				LeaveCriticalSection(&xlive_critsec_broadcast_addresses);
				SetDlgItemTextA(xlln_window_hwnd, MYWINDOW_TBX_BROADCAST, ":0");
				EnterCriticalSection(&xlive_critsec_broadcast_addresses);
			}
			
			for (const XLLNBroadcastEntity::BROADCAST_ENTITY &broadcastEntity : xlive_broadcast_addresses) {
				memcpy(&sockAddrExternal, &broadcastEntity.sockaddr, sockAddrExternalLen);
				
				uint16_t *portBroadcastNBO = 0;
				if (sockAddrExternal.ss_family == AF_INET) {
					portBroadcastNBO = (uint16_t*)&(((sockaddr_in*)&sockAddrExternal)->sin_port);
					
					if (((struct sockaddr_in*)&sockAddrExternal)->sin_addr.s_addr == htonl(INADDR_BROADCAST)) {
						EnterCriticalSection(&xlive_critsec_network_adapter);
						if (xlive_network_adapter) {
							((struct sockaddr_in*)&sockAddrExternal)->sin_addr.s_addr = htonl(xlive_network_adapter->hBroadcast);
						}
						LeaveCriticalSection(&xlive_critsec_network_adapter);
					}
				}
				else if (sockAddrExternal.ss_family == AF_INET6) {
					portBroadcastNBO = (uint16_t*)&(((sockaddr_in6*)&sockAddrExternal)->sin6_port);
				}
				else {
					// If it is not IPv4 or IPv6 then do not sent it.
					continue;
				}
				
				uint8_t portBaseOffset = portHBO % 100;
				{
					EnterCriticalSection(&xlln_critsec_base_port_offset_mappings);
					
					if (xlln_base_port_mappings_original.count(portHBO)) {
						BASE_PORT_OFFSET_MAPPING *mappingOriginal = xlln_base_port_mappings_original[portHBO];
						portBaseOffset = mappingOriginal->offset;
					}
					
					LeaveCriticalSection(&xlln_critsec_base_port_offset_mappings);
				}
				
				if (broadcastEntity.entityType == XLLNBroadcastEntity::tHUB_SERVER) {
					const uint16_t portBroadcastToHBO = ntohs(*portBroadcastNBO);
					
					bool isCoreSocket = false;
					if (perpetual_socket == xlive_xsocket_perpetual_core_socket) {
						isCoreSocket = true;
					}
					else {
						EnterCriticalSection(&xlive_critsec_sockets);
						
						if (
							xlive_xsocket_perpetual_to_transitory_socket.count(perpetual_socket)
							&& xlive_xsocket_perpetual_to_transitory_socket.count(xlive_xsocket_perpetual_core_socket)
							&& xlive_xsocket_perpetual_to_transitory_socket[perpetual_socket] == xlive_xsocket_perpetual_to_transitory_socket[xlive_xsocket_perpetual_core_socket]
						) {
							isCoreSocket = true;
						}
						
						LeaveCriticalSection(&xlive_critsec_sockets);
					}
					
					if (!isCoreSocket) {
						const int packetSizeHeaderType = sizeof(XLLNNetPacketType::TYPE);
						const int packetSizeTypeHubOutOfBand = sizeof(XLLNNetPacketType::HUB_OUT_OF_BAND);
						const int packetSize = packetSizeHeaderType + packetSizeTypeHubOutOfBand + dataSendSize;
						
						uint8_t *packetBuffer = new uint8_t[packetSize];
						packetBuffer[0] = XLLNNetPacketType::tHUB_OUT_OF_BAND;
						XLLNNetPacketType::HUB_OUT_OF_BAND &packetHubOutOfBand = *(XLLNNetPacketType::HUB_OUT_OF_BAND*)&packetBuffer[packetSizeHeaderType];
						packetHubOutOfBand.instanceId = ntohl(xlive_local_xnAddr.inaOnline.s_addr);
						packetHubOutOfBand.portOffsetHBO = portBaseOffset;
						packetHubOutOfBand.portOriginalHBO = portHBO;
						
						memcpy(&packetBuffer[packetSizeHeaderType + packetSizeTypeHubOutOfBand], dataBuffer, dataSendSize);
						
						resultSend = SendToPerpetualSocket(perpetual_socket, (const char*)packetBuffer, packetSize, 0, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);
						errorSend = WSAGetLastError();
						if (resultSend > 0) {
							resultSend = ERROR_SUCCESS;
						}
						
						delete[] packetBuffer;
						
						{
							char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
								, "%s Perpetual Socket (0x%08x) broadcasted OOB packet to XLLN-Hub %s."
								, __func__
								, perpetual_socket
								, sockAddrInfo ? sockAddrInfo : ""
							);
							if (sockAddrInfo) {
								free(sockAddrInfo);
							}
						}
						
						continue;
					}
					
					resultSend = SendToPerpetualSocket(perpetual_socket, dataBuffer, dataSendSize, 0, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);
					errorSend = WSAGetLastError();
					if (resultSend > 0) {
						resultSend = ERROR_SUCCESS;
					}
					
					{
						char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
							, "%s Perpetual Socket (0x%08x) broadcasted packet to XLLN-Hub %s."
							, __func__
							, perpetual_socket
							, sockAddrInfo ? sockAddrInfo : ""
						);
						if (sockAddrInfo) {
							free(sockAddrInfo);
						}
					}
					
					continue;
				}
				
				std::set<uint16_t> portsToBroadcastToHBO;
				
				portsToBroadcastToHBO.insert(portHBO);
				portsToBroadcastToHBO.insert(xlive_port_online);
				
				uint32_t portRange = xlive_base_port_broadcast_spacing_start;
				uint32_t portRangePrev = portRange;
				
				if (portBaseOffset != 0xFF) {
					while (1) {
						portsToBroadcastToHBO.insert(portRange + portBaseOffset);
						
						portRange += xlive_base_port_broadcast_spacing_increment;
						
						if (portRange < portRangePrev) {
							// Overflow.
							break;
						}
						if (portRange > xlive_base_port_broadcast_spacing_end) {
							// passed the end.
							break;
						}
					}
				}
				
				for (const uint16_t &portToBroadcastToHBO : portsToBroadcastToHBO) {
					*portBroadcastNBO = htons(portToBroadcastToHBO);
					
					resultSend = SendToPerpetualSocket(perpetual_socket, dataBuffer, dataSendSize, 0, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);
					errorSend = WSAGetLastError();
					if (resultSend > 0) {
						resultSend = ERROR_SUCCESS;
					}
					
					{
						char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
							, "%s Perpetual Socket (0x%08x) broadcasted packet to %s."
							, __func__
							, perpetual_socket
							, sockAddrInfo ? sockAddrInfo : ""
						);
						if (sockAddrInfo) {
							free(sockAddrInfo);
						}
					}
				}
			}
			
			LeaveCriticalSection(&xlive_critsec_broadcast_addresses);
		}
		
		*resultBroadcastTo = resultSend;
		WSASetLastError(errorSend);
		return true;
	}
	
	return false;
}

// Process connectionless sockets only (UDP).
// dataBuffer data must be wrapped in PacketType.
INT WINAPI XllnSocketSendTo(SOCKET perpetual_socket, const char *dataBuffer, int dataSendSize, int flags, const sockaddr *to, int tolen)
{
	TRACE_FX();
	if (flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s flags argument must be 0 on Perpetual Socket 0x%08x."
			, __func__
			, perpetual_socket
		);
		
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	
	SOCKADDR_STORAGE sockAddrExternal;
	int sockAddrExternalLen = sizeof(sockAddrExternal);
	memcpy(&sockAddrExternal, to, sockAddrExternalLen < tolen ? sockAddrExternalLen : tolen);
	
	const uint32_t ipv4NBO = ((struct sockaddr_in*)&sockAddrExternal)->sin_addr.s_addr;
	// This address may (hopefully) be an instanceId.
	const uint32_t ipv4HBO = ntohl(ipv4NBO);
	const uint16_t portHBO = GetSockAddrPort(&sockAddrExternal);
	
	INT resultDataSendSize = SOCKET_ERROR;
	int errorSend = WSAEINVAL;
	if (XllnSocketBroadcastTo(&resultDataSendSize, perpetual_socket, dataBuffer, dataSendSize, flags, to, tolen)) {
		errorSend = WSAGetLastError();
		
		if (resultDataSendSize == ERROR_SUCCESS) {
			resultDataSendSize = dataSendSize;
		}
		
		WSASetLastError(errorSend);
		return resultDataSendSize;
	}
	else if (sockAddrExternal.ss_family != AF_INET) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s Unsupported socket address family 0x%04hx on Perpetual Socket 0x%08x ."
			, __func__
			, sockAddrExternal.ss_family
			, perpetual_socket
		);
		
		WSASetLastError(WSAEAFNOSUPPORT);
		return SOCKET_ERROR;
	}
	
	uint32_t resultNetter = NetterEntityGetAddrByInstanceIdPort(&sockAddrExternal, ipv4HBO, portHBO);
	if (resultNetter) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s Perpetual Socket (0x%08x) NetterEntityGetAddrByInstanceIdPort failed to find address 0x%08x:%hu with error 0x%08x."
			, __func__
			, perpetual_socket
			, ipv4HBO
			, portHBO
			, resultNetter
		);
		
		if (resultNetter == ERROR_PORT_NOT_SET) {
			if (sockAddrExternal.ss_family == AF_INET || sockAddrExternal.ss_family == AF_INET6) {
				SendUnknownUserPacket(
					perpetual_socket
					, dataBuffer
					, dataSendSize
					, XLLNNetPacketType::tUNKNOWN
					, true
					, &sockAddrExternal
					, false
					, 0
					, ipv4HBO
				);
			}
		}
		
		WSASetLastError(ERROR_SUCCESS);
		return dataSendSize;
	}
	
	{
		char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "%s Perpetual Socket (0x%08x) NetterEntityGetAddrByInstanceIdPort found address/instanceId 0x%08x:%hu as %s."
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
	
	resultDataSendSize = SendToPerpetualSocket(perpetual_socket, dataBuffer, dataSendSize, flags, (const sockaddr*)&sockAddrExternal, sockAddrExternalLen);
	errorSend = WSAGetLastError();
	
	WSASetLastError(errorSend);
	return resultDataSendSize;
}

INT SendToPerpetualSocket(SOCKET perpetual_socket, const char *data_buffer, int data_buffer_size, int flags, const sockaddr *to, int tolen)
{
	while (1) {
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		INT resultDataSendSize = sendto(transitorySocket, data_buffer, data_buffer_size, flags, to, tolen);
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
		// else {
		// 	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		// 		, "%s socket (P,T) (0x%08x,0x%08x) data to send size 0x%08x sent size 0x%08x."
		// 		, __func__
		// 		, perpetual_socket
		// 		, transitorySocket
		// 		, dataSendSize
		// 		, resultDataSendSize
		// 	);
		// }
		
		WSASetLastError(errorSend);
		return resultDataSendSize;
	}
}
