#pragma once
#include <stdint.h>

extern WORD xlive_base_port;
extern BOOL xlive_netsocket_abort;
extern SOCKET xlive_liveoverlan_socket;

#define XLLN_CUSTOM_PACKET_SENTINEL (BYTE)0x00
namespace XLLNCustomPacketType {
	enum Type : BYTE {
		UNKNOWN = 0x00,
		STOCK_PACKET,
		STOCK_PACKET_FORWARDED,
		CUSTOM_OTHER,
		UNKNOWN_USER_ASK,
		UNKNOWN_USER_REPLY,
		LIVE_OVER_LAN_ADVERTISE,
		LIVE_OVER_LAN_UNADVERTISE,

	};
}

namespace XLLNCustomPacketType {
	const char* const TypeNames[]{
	"UNKNOWN",
	"STOCK_PACKET",
	"STOCK_PACKET_FORWARDED",
	"CUSTOM_OTHER",
	"UNKNOWN_USER_ASK",
	"UNKNOWN_USER_REPLY",
	"LIVE_OVER_LAN_ADVERTISE",
	"LIVE_OVER_LAN_UNADVERTISE",
	};
	typedef enum : BYTE {
		tUNKNOWN = 0,
		tSTOCK_PACKET,
		tSTOCK_PACKET_FORWARDED,
		tCUSTOM_OTHER,
		tUNKNOWN_USER_ASK,
		tUNKNOWN_USER_REPLY,
		tLIVE_OVER_LAN_ADVERTISE,
		tLIVE_OVER_LAN_UNADVERTISE,
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

#pragma pack(pop) // Return to original alignment setting.
}

BOOL InitXSocket();
BOOL UninitXSocket();

VOID SendUnknownUserAskRequest(SOCKET socket, char* data, int dataLen, sockaddr *to, int tolen);

INT WINAPI XllnSocketSendTo(SOCKET s, const char *buf, int len, int flags, sockaddr *to, int tolen);
