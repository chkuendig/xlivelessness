#pragma once
#include "xsocket.hpp"

#define XLLN_LIVEOVERLAN_SESSION_TYPE_XLOCATOR 0
#define XLLN_LIVEOVERLAN_SESSION_TYPE_XSESSION 1

#pragma pack(push, 1) // Save then set byte alignment setting.

typedef struct
{
	uint32_t                               propertyId;
	uint8_t                                type;

	union
	{
		LONG                            nData;     // XUSER_DATA_TYPE_INT32
		LONGLONG                        i64Data;   // XUSER_DATA_TYPE_INT64
		double                          dblData;   // XUSER_DATA_TYPE_DOUBLE
		struct                                     // XUSER_DATA_TYPE_UNICODE
		{
			DWORD                       cbData;    // Includes null-terminator
			// The address of this variable is the location of the string not the value of this variable (since it is serialised).
			LPWSTR                      pwszData;
		} string;
		FLOAT                           fData;     // XUSER_DATA_TYPE_FLOAT
		struct                                     // XUSER_DATA_TYPE_BINARY
		{
			DWORD                       cbData;
			// The address of this variable is the location of the string not the value of this variable (since it is serialised).
			PBYTE                       pbData;
		} binary;
		FILETIME                        ftData;    // XUSER_DATA_TYPE_DATETIME
	};
} XUSER_PROPERTY_SERIALISED;

typedef struct {
	// serverID
	XUID xuid = INVALID_XUID;
	// XLLN_LIVEOVERLAN_SESSION_TYPE_XLOCATOR or XLLN_LIVEOVERLAN_SESSION_TYPE_XSESSION.
	uint8_t sessionType = 0;
	// If sessionType is XLLN_LIVEOVERLAN_SESSION_TYPE_XLOCATOR then this contains XLOCATOR_SERVERTYPE_ macros. Otherwise if XLLN_LIVEOVERLAN_SESSION_TYPE_XSESSION then XSESSION_CREATE_ macros.
	uint32_t sessionFlags = 0;
	// sessionID
	XNKID xnkid;
	// keyExchangeKey
	XNKEY xnkey;
	uint32_t slotsPublicMaxCount = 0;
	uint32_t slotsPublicFilledCount = 0;
	uint32_t slotsPrivateMaxCount = 0;
	uint32_t slotsPrivateFilledCount = 0;
	uint32_t contextsCount = 0;
	uint32_t propertiesCount = 0;
	XUSER_CONTEXT *pContexts = 0;
	XUSER_PROPERTY *pProperties = 0;
} LIVE_SESSION;

#pragma pack(pop) // Return to original alignment setting.

typedef struct {
	LIVE_SESSION *liveSession;
	// hostAddress
	XNADDR xnAddr;
	uint64_t timeOfLastContact;
} LIVE_SESSION_REMOTE;

typedef struct {
	LIVE_SESSION *liveSession = 0;
	// hostAddress
	XNADDR xnAddr;
	DWORD dwGameType = 0;
	DWORD dwGameMode = 0;
	DWORD dwActualMemberCount = 0;
	DWORD dwReturnedMemberCount = 0;
	XSESSION_STATE eState = XSESSION_STATE_LOBBY;
	// Treating as XUID of the creator.
	ULONGLONG qwNonce = INVALID_XUID;
	XNKID xnkidArbitration;
	XSESSION_MEMBER *pSessionMembers = 0;
} LIVE_SESSION_XSESSION;

void LiveOverLanDestroyLiveSession(LIVE_SESSION **live_session);
bool LiveOverLanDeserialiseLiveSession(
	const uint8_t *live_session_buffer
	, const uint32_t live_session_buffer_size
	, LIVE_SESSION **result_live_session
);
bool LiveOverLanSerialiseLiveSessionIntoNetPacket(
	LIVE_SESSION *live_session
	, uint8_t **result_buffer
	, uint32_t *result_buffer_size
);
void LiveOverLanBroadcastLocalSessionUnadvertise(const XUID xuid);
void LiveOverLanBroadcastRemoteSessionUnadvertise(const uint32_t instance_id, const uint8_t session_type, const XUID xuid);
void LiveOverLanAddRemoteLiveSession(const uint32_t instance_id, const uint8_t session_type, LIVE_SESSION *live_session);

VOID LiveOverLanAbort();
VOID LiveOverLanStartBroadcast();
VOID LiveOverLanStopBroadcast();
VOID LiveOverLanStartEmpty();
VOID LiveOverLanStopEmpty();

extern CRITICAL_SECTION xlln_critsec_liveoverlan_sessions;
extern CRITICAL_SECTION xlln_critsec_liveoverlan_broadcast;

extern std::map<uint32_t, LIVE_SESSION_REMOTE*> liveoverlan_remote_sessions_xlocator;
extern std::map<uint32_t, LIVE_SESSION_REMOTE*> liveoverlan_remote_sessions_xsession;
