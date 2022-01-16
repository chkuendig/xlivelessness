#pragma once
#include <stdint.h>
#include <vector>

namespace XLLNNetPacketType {
	const char* const TYPE_NAMES[] {
		"UNKNOWN",
		"TITLE_PACKET",
		"TITLE_BROADCAST_PACKET",
		"PACKET_FORWARDED",
		"UNKNOWN_USER_ASK",
		"UNKNOWN_USER_REPLY",
		"CUSTOM_OTHER",
		"LIVE_OVER_LAN_ADVERTISE",
		"LIVE_OVER_LAN_UNADVERTISE",
		"HUB_REQUEST",
		"HUB_REPLY",
		"QOS_REQUEST",
		"QOS_RESPONSE",
	};
	typedef enum : uint8_t {
		tUNKNOWN = 0,
		tTITLE_PACKET,
		tTITLE_BROADCAST_PACKET,
		tPACKET_FORWARDED,
		tUNKNOWN_USER_ASK,
		tUNKNOWN_USER_REPLY,
		tCUSTOM_OTHER,
		tLIVE_OVER_LAN_ADVERTISE,
		tLIVE_OVER_LAN_UNADVERTISE,
		tHUB_REQUEST,
		tHUB_REPLY,
		tQOS_REQUEST,
		tQOS_RESPONSE,
	} TYPE;

#pragma pack(push, 1) // Save then set byte alignment setting.

	typedef struct {
		char *Identifier;
		DWORD *FuncPtr;
	} RECVFROM_CUSTOM_HANDLER_REGISTER;

	typedef struct {
		uint32_t instanceId = 0; // a generated UUID for that instance.
		uint16_t portBaseHBO = 0; // Base Port of the instance. Host Byte Order.
		uint16_t socketInternalPortHBO = 0;
		int16_t socketInternalPortOffsetHBO = 0;
		uint32_t instanceIdConsumeRemaining = 0; // the instanceId that should be consuming the rest of the data on this packet.
	} NET_USER_PACKET;

	typedef struct {
		SOCKADDR_STORAGE originSockAddr;
		NET_USER_PACKET netter;
		// The data following this is the forwarded packet data.
	} PACKET_FORWARDED;

	typedef struct {
		NET_USER_PACKET netter;
		// The data following this is the forwarded packet data.
	} UNKNOWN_USER;

	typedef struct {
		uint32_t xllnVersion = 0; // version of the requester.
		uint32_t instanceId = 0; // Instance ID of the requester.
		uint32_t titleId = 0;
		uint32_t titleVersion = 0;
	} HUB_REQUEST_PACKET;

	typedef struct {
		uint8_t isHubServer = 0; // boolean.
		uint32_t xllnVersion = 0; // version of the replier.
		uint32_t recommendedInstanceId = 0; // the Instance ID that should be used instead (in case of collisions).
	} HUB_REPLY_PACKET;

	typedef struct {
		uint8_t sessionType = 0;
		XUID xuid = 0;
	} LIVE_OVER_LAN_UNADVERTISE;
	
	typedef struct {
		uint32_t qosLookupId = 0;
		uint64_t sessionId = 0; // XNKID
		uint32_t probeId = 0;
		uint32_t instanceId = 0; // Instance ID of the requester.
	} QOS_REQUEST;
	
	typedef struct {
		uint32_t qosLookupId = 0;
		uint64_t sessionId = 0; // XNKID
		uint32_t probeId = 0;
		uint32_t instanceId = 0; // Instance ID of the responder.
		uint8_t enabled = 0;
		uint16_t sizeData = 0; // the amount of data appended to the end of this packet type.
	} QOS_RESPONSE;

#pragma pack(pop) // Return to original alignment setting.

}

namespace XLLNBroadcastEntity {
	const char* const TYPE_NAMES[] {
		"UNKNOWN",
		"BROADCAST_ADDR",
		"HUB_SERVER",
		"OTHER_CLIENT",
	};
	typedef enum : uint8_t {
		tUNKNOWN = 0,
		tBROADCAST_ADDR,
		tHUB_SERVER,
		tOTHER_CLIENT,
	} TYPE;

	typedef struct {
		SOCKADDR_STORAGE sockaddr;
		__time64_t lastComm = 0;
		TYPE entityType = TYPE::tUNKNOWN;
	} BROADCAST_ENTITY;

}

extern CRITICAL_SECTION xlive_critsec_broadcast_addresses;
extern std::vector<XLLNBroadcastEntity::BROADCAST_ENTITY> xlive_broadcast_addresses;

INT WINAPI XSocketRecvFromHelper(const int dataRecvSize, const SOCKET socket, char *dataBuffer, const int dataBufferSize, const int flags, const SOCKADDR_STORAGE *sockAddrExternal, const int sockAddrExternalLen, sockaddr *sockAddrXlive, int *sockAddrXliveLen);
INT WINAPI XllnSocketSendTo(SOCKET socket, const char *dataBuffer, int dataSendSize, int flags, const sockaddr *to, int tolen);
VOID SendUnknownUserAskRequest(SOCKET socket, const char* data, int dataLen, const SOCKADDR_STORAGE *sockAddrExternal, const int sockAddrExternalLen, bool isAsking, uint32_t instanceIdConsumeRemaining);
