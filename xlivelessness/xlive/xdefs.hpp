#pragma once
#ifndef XLIVEDEFS_H
#define XLIVEDEFS_H
#pragma comment(lib, "Ws2_32.lib")
#include <windows.h>
#include <stdint.h>

#pragma pack(push, 8) // Save then set byte alignment setting.

typedef ULONGLONG XUID;
typedef XUID *PXUID;

#define INVALID_XUID                    ((XUID) 0)
#define XUID_LIVE_ENABLED_FLAG (0x0009000000000000)
#define XUID_LIVE_GUEST_FLAG (0x00C0000000000000)
#define XUID_LIVE_ENABLED(xuid) ((xuid & 0x000F000000000000) == XUID_LIVE_ENABLED_FLAG)
#define XUID_LIVE_GUEST(xuid) ((xuid & XUID_LIVE_GUEST_FLAG) > 0)

#define LODWORD(ll) ((DWORD)(ll))
#define HIDWORD(ll) ((DWORD)(((ULONGLONG)(ll) >> 32) & 0xFFFFFFFF))
//#define LOWORD(l) ((WORD)(l))
//#define HIWORD(l) ((WORD)(((DWORD)(l) >> 16) & 0xFFFF))
//#define LOBYTE(w) ((BYTE)(w))
//#define HIBYTE(w) ((BYTE)(((WORD)(w) >> 8) & 0xFF))

#define E_DEBUGGER_PRESENT 0x80040317;

//typedef unsigned __int64 ULONG_PTR, *PULONG_PTR;
//typedef unsigned long DWORD_PTR;


// Xbox Secure Network Library ------------------------------------------------

//
// XNetStartup is called to load the Xbox Secure Network Library.  It takes an
// optional pointer to a parameter structure.  To initialize the library with
// the default set of parameters, simply pass NULL for this argument.  To
// initialize the library with a different set of parameters, place an
// XNetStartupParams on the stack, zero it out, set the cfgSizeOfStruct to
// sizeof(XNetStartupParams), set any of the parameters you want to configure
// (leaving the remaining ones zeroed), and pass a pointer to this structure to
// XNetStartup.
//
// By default the Xbox Secure Network Library operates in secure mode, which
// means that communication to untrusted hosts (such as a PC) is disallowed.
// However, the devkit version of the library allows you to bypass this
// security by specifying the XNET_STARTUP_BYPASS_SECURITY flag in the cfgFlags
// parameter.  Here is an example of how to do this:
//
//      XNetStartupParams xnsp;
//      memset(&xnsp, 0, sizeof(xnsp));
//      xnsp.cfgSizeOfStruct = sizeof(XNetStartupParams);
//      xnsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
//      INT err = XNetStartup(&xnsp);
//
// Attempting to bypass security when not using the devkit version of the
// library does not return an error, it is simply ignored.  Attempts to send or
// receive packets from untrusted hosts will fail.
//


//
// This devkit-only flag tells the XNet stack to allow insecure
// communication to untrusted hosts (such as a PC).  This flag is silently
// ignored by the secure versions of the library.
//
#define XNET_STARTUP_BYPASS_SECURITY                0x01

//
// This flag instructs XNet to pre-allocate memory for the maximum number of
// datagram (UDP and VDP) sockets during the 'XNetStartup' call and store the
// objects in an internal pool.  Otherwise, sockets are allocated on demand (by
// the 'socket' function).  In either case, SOCK_DGRAM sockets are returned to
// the internal pool once closed.  The memory will remain allocated until
// XNetCleanup is called.
//
#define XNET_STARTUP_ALLOCATE_MAX_DGRAM_SOCKETS     0x02

//
// This flag instructs XNet to pre-allocate memory for the maximum number of
// stream (TCP) sockets during the 'XNetStartup' call and store the objects in
// an internal pool.  Otherwise, sockets are allocated on demand (by the
// 'socket', 'listen', and 'accept' functions).  Note that 'listen' will still
// attempt to pre-allocate the specified maximum backlog number of sockets even
// without this flag set.  The 'accept' function will always return a socket
// retrieved from the pool, though it will also attempt to allocate a
// replacement if the cfgSockMaxStreamSockets limit and memory permit.
// In all cases, SOCK_STREAM sockets are returned to the internal pool once
// closed. The memory will remain allocated until XNetCleanup is called.
//
#define XNET_STARTUP_ALLOCATE_MAX_STREAM_SOCKETS    0x04

//
// This devkit-only flag tells the XNet stack to disable encryption for
// communication between peers.  This flag is silently ignored by the secure
// versions of the library.
//
#define XNET_STARTUP_DISABLE_PEER_ENCRYPTION        0x08




typedef struct _XLIVE_INITIALIZE_INFO {
	UINT cbSize;
	DWORD dwFlags;
	IUnknown *pD3D;
	VOID *pD3DPP;
	LANGID langID;
	WORD wReserved1;
	PCHAR pszAdapterName;
	WORD wLivePortOverride;
	WORD wReserved2;
} XLIVE_INITIALIZE_INFO;

typedef struct XLIVE_INPUT_INFO {
	UINT cbSize;
	HWND hWnd;
	UINT uMSG;
	WPARAM wParam;
	LPARAM lParam;
	BOOL fHandled;
	LRESULT lRet;
} XLIVE_INPUT_INFO;



typedef struct _XLIVEUPDATE_INFORMATION {
	DWORD cbSize;
	BOOL bSystemUpdate;
	DWORD dwFromVersion;
	DWORD dwToVersion;
	WCHAR szUpdateDownloadPath[MAX_PATH];
} XLIVEUPDATE_INFORMATION, *PXLIVEUPDATE_INFORMATION;


#define XLIVE_PROTECTED_DATA_FLAG_OFFLINE_ONLY 0x00000001

#pragma pack(push, 1) // Save then set byte alignment setting.

typedef struct {
	uint32_t dwSize;
	// bData is first element of array of length/size dwSize.
	uint8_t bData;
} XLIVE_PROTECTED_BUFFER;

#pragma pack(pop) // Return to original alignment setting.

typedef struct _XLIVE_PROTECTED_DATA_INFORMATION {
	DWORD cbSize;
	DWORD dwFlags;
} XLIVE_PROTECTED_DATA_INFORMATION, *PXLIVE_PROTECTED_DATA_INFORMATION;


#define XUSER_DATA_TYPE_CONTEXT     ((BYTE)0)
#define XUSER_DATA_TYPE_INT32       ((BYTE)1)
#define XUSER_DATA_TYPE_INT64       ((BYTE)2)
#define XUSER_DATA_TYPE_DOUBLE      ((BYTE)3)
#define XUSER_DATA_TYPE_UNICODE     ((BYTE)4)
#define XUSER_DATA_TYPE_FLOAT       ((BYTE)5)
#define XUSER_DATA_TYPE_BINARY      ((BYTE)6)
#define XUSER_DATA_TYPE_DATETIME    ((BYTE)7)
#define XUSER_DATA_TYPE_NULL        ((BYTE)0xFF)

// Creation Flags
#define XSESSION_CREATE_USES_MASK                       0x0000003F

#define XSESSION_CREATE_HOST                            0x00000001  // Set only on the host of a multiplayer session. The user who sets the host flag is the user that interacts with Live
#define XSESSION_CREATE_USES_PRESENCE                   0x00000002  // Session is used across games to keep players together. Advertises state via Presence
#define XSESSION_CREATE_USES_STATS                      0x00000004  // Session is used for stats tracking
#define XSESSION_CREATE_USES_MATCHMAKING                0x00000008  // Session is advertised in matchmaking for searching
#define XSESSION_CREATE_USES_ARBITRATION                0x00000010  // Session stats are arbitrated (and therefore tracked for everyone in the game)
#define XSESSION_CREATE_USES_PEER_NETWORK               0x00000020  // Session XNKey is registered and PC settings are enforced

// Optional modifiers to sessions that are created with XSESSION_CREATE_USES_PRESENCE
#define XSESSION_CREATE_MODIFIERS_MASK                  0x00000F80

#define XSESSION_CREATE_SOCIAL_MATCHMAKING_ALLOWED      0x00000080  // Session may be converted to an Social Matchmaking session
#define XSESSION_CREATE_INVITES_DISABLED                0x00000100  // Game Invites cannot be sent by the HUD for this session
#define XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED      0x00000200  // Session will not ever be displayed as joinable via Presence
#define XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED       0x00000400  // Session will not be joinable between XSessionStart and XSessionEnd
#define XSESSION_CREATE_JOIN_VIA_PRESENCE_FRIENDS_ONLY  0x00000800  // Session is only joinable via presence for friends of the host


#define XSESSION_CREATE_SINGLEPLAYER_WITH_STATS		XSESSION_CREATE_USES_PRESENCE | XSESSION_CREATE_USES_STATS | XSESSION_CREATE_INVITES_DISABLED | XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED | XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED
#define XSESSION_CREATE_LIVE_MULTIPLAYER_STANDARD	XSESSION_CREATE_USES_PRESENCE | XSESSION_CREATE_USES_STATS | XSESSION_CREATE_USES_MATCHMAKING | XSESSION_CREATE_USES_PEER_NETWORK
#define XSESSION_CREATE_LIVE_MULTIPLAYER_RANKED		XSESSION_CREATE_LIVE_MULTIPLAYER_STANDARD | XSESSION_CREATE_USES_ARBITRATION
#define XSESSION_CREATE_SYSTEMLINK					XSESSION_CREATE_USES_PEER_NETWORK
#define XSESSION_CREATE_GROUP_LOBBY					XSESSION_CREATE_USES_PRESENCE | XSESSION_CREATE_USES_PEER_NETWORK
#define XSESSION_CREATE_GROUP_GAME					XSESSION_CREATE_USES_STATS | XSESSION_CREATE_USES_MATCHMAKING | XSESSION_CREATE_USES_PEER_NETWORK



typedef struct
{

	//
	// Must be set to sizeof(XNetStartupParams).  There is no default.
	//
	BYTE        cfgSizeOfStruct;

	//
	// One or more of the XNET_STARTUP_xxx flags OR'd together.
	//
	// The default is 0 (no flags specified).
	BYTE        cfgFlags;

	//
	// The maximum number of SOCK_DGRAM (UDP or VDP) sockets that can be
	// opened at once.
	//
	// The default is 8 sockets.
	//
	BYTE        cfgSockMaxDgramSockets;

	//
	// The maximum number of SOCK_STREAM (TCP) sockets that can be opened at
	// once, including those sockets created as a result of incoming connection
	// requests.  Remember that a TCP socket may not be closed immediately
	// after 'closesocket' is called, depending on the linger options in place
	// (by default a TCP socket will linger).
	//
	// The default is 32 sockets.
	//
	BYTE        cfgSockMaxStreamSockets;

	//
	// The default receive buffer size for a socket, in units of K (1024 bytes).
	//
	// The default is 16 units (16K).
	//
	BYTE        cfgSockDefaultRecvBufsizeInK;

	//
	// The default send buffer size for a socket, in units of K (1024 bytes).
	//
	// The default is 16 units (16K).
	//
	BYTE        cfgSockDefaultSendBufsizeInK;

	//
	// The maximum number of XNKID / XNKEY pairs that can be registered at the
	// same time by calling XNetRegisterKey.
	//
	// The default is 8 key pair registrations.
	//
	BYTE        cfgKeyRegMax;

	//
	// The maximum number of security associations that can be registered at
	// the same time.  Security associations are created for each unique
	// XNADDR / XNKID pair passed to XNetXnAddrToInAddr.  Security associations
	// are also implicitly created for each secure host that establishes an
	// incoming connection with this host on a given registered XNKID.  Note
	// that there will only be one security association between a pair of hosts
	// on a given XNKID no matter how many sockets are actively communicating
	// on that secure connection.
	//
	// The default is 32 security associations.
	//
	BYTE        cfgSecRegMax;

	//
	// The maximum amount of QoS data, in units of DWORD (4 bytes), that can be
	// supplied to a call to XNetQosListen or returned in the result set of a
	// call to XNetQosLookup.
	//
	// The default is 64 (256 bytes).
	//
	BYTE        cfgQosDataLimitDiv4;

	//
	// The amount of time to wait for a response after sending a QoS packet
	// before sending it again (or giving up).  This should be set to the same
	// value on clients (XNetQosLookup callers) and servers (XNetQosListen
	// callers).
	//
	// The default is 2 seconds.
	//
	BYTE        cfgQosProbeTimeoutInSeconds;

	//
	// The maximum number of times to retry a given QoS packet when no response
	// is received.  This should be set to the same value on clients
	// (XNetQosLookup callers) and servers (XNetQosListen callers).
	//
	// The default is 3 retries.
	//
	BYTE        cfgQosProbeRetries;

	//
	// The maximum number of simultaneous QoS lookup responses that a QoS
	// listener supports.  Note that the bandwidth throttling parameter passed
	// to XNetQosListen may impact the number of responses queued, and thus
	// affects how quickly this limit is reached.
	//
	// The default is 8 responses.
	//
	BYTE        cfgQosSrvMaxSimultaneousResponses;

	//
	// The maximum amount of time for QoS listeners to wait for the second
	// packet in a packet pair.
	//
	// The default is 2 seconds.
	//
	BYTE        cfgQosPairWaitTimeInSeconds;

} XNetStartupParams;

typedef struct
{
	IN_ADDR     ina;                            // IP address (zero if not static/DHCP)
	IN_ADDR     inaOnline;                      // Secure Addr. Online IP address (zero if not online)
	WORD        wPortOnline;                    // Online port
	BYTE        abEnet[6];                      // Ethernet MAC address
	BYTE        abOnline[20];                   // Online identification
} XNADDR;

typedef struct
{
	BYTE        ab[8];                          // xbox to xbox key identifier
} XNKID;

typedef XNADDR TSADDR;

#define XONLINE_E_OVERFLOW                                          0x80150001
#define XONLINE_E_NO_SESSION                                        0x80150002
#define XONLINE_E_USER_NOT_LOGGED_ON                                0x80150003
#define XONLINE_E_NO_GUEST_ACCESS                                   0x80150004
#define XONLINE_E_NOT_INITIALIZED                                   0x80150005
#define XONLINE_E_NO_USER                                           0x80150006
#define XONLINE_E_INTERNAL_ERROR                                    0x80150007
#define XONLINE_E_OUT_OF_MEMORY                                     0x80150008
#define XONLINE_E_TASK_BUSY                                         0x80150009
#define XONLINE_E_SERVER_ERROR                                      0x8015000A
#define XONLINE_E_IO_ERROR                                          0x8015000B
#define XONLINE_E_USER_NOT_PRESENT                                  0x8015000D
#define XONLINE_E_INVALID_REQUEST                                   0x80150010
#define XONLINE_E_TASK_THROTTLED                                    0x80150011
#define XONLINE_E_TASK_ABORTED_BY_DUPLICATE                         0x80150012
#define XONLINE_E_INVALID_TITLE_ID                                  0x80150013
#define XONLINE_E_SERVER_CONFIG_ERROR                               0x80150014
#define XONLINE_E_END_OF_STREAM                                     0x80150015
#define XONLINE_E_ACCESS_DENIED                                     0x80150016
#define XONLINE_E_GEO_DENIED                                        0x80150017
#define XONLINE_E_LOGON_NO_NETWORK_CONNECTION                       0x80151000
#define XONLINE_E_LOGON_CANNOT_ACCESS_SERVICE                       0x80151001
#define XONLINE_E_LOGON_UPDATE_REQUIRED                             0x80151002
#define XONLINE_E_LOGON_SERVERS_TOO_BUSY                            0x80151003
#define XONLINE_E_LOGON_CONNECTION_LOST                             0x80151004
#define XONLINE_E_LOGON_KICKED_BY_DUPLICATE_LOGON                   0x80151005
#define XONLINE_E_LOGON_INVALID_USER                                0x80151006
#define XONLINE_E_LOGON_FLASH_UPDATE_REQUIRED                       0x80151007
#define XONLINE_E_LOGON_SG_CONNECTION_TIMEDOUT                      0x8015100C
#define XONLINE_E_ACCOUNTS_GRADUATE_USER_NO_PRIVILEGE               0x8015102A
#define XONLINE_E_ACCOUNTS_GRADUATE_USER_NOT_CHILD                  0x8015102B
#define XONLINE_E_ACCOUNTS_GRADUATE_USER_NOT_ADULT                  0x8015102C
#define XONLINE_E_ACCOUNTS_GRADUATE_USER_NO_PI                      0x8015102D
#define XONLINE_E_ACCOUNTS_GRADUATE_USER_PI_MISMATCH                0x8015102E
#define XONLINE_E_ACCOUNTS_GRADUATE_USER_ALREADY                    0x8015102F
#define XONLINE_E_ACCOUNTS_GRADUATE_USER_QUEUED                     0x80151031
#define XONLINE_E_ACCOUNTS_SWITCH_PASSPORT_NEW_PASSPORT_INELIGIBLE  0x80151032
#define XONLINE_E_ACCOUNTS_NO_AUTHENTICATION_DATA                   0x80151033
#define XONLINE_E_ACCOUNTS_CLIENT_TYPE_CONFIG_ERROR                 0x80151034
#define XONLINE_E_ACCOUNTS_CLIENT_TYPE_MISSING                      0x80151035
#define XONLINE_E_ACCOUNTS_CLIENT_TYPE_INVALID                      0x80151036
#define XONLINE_E_ACCOUNTS_COUNTRY_NOT_AUTHORIZED                   0x80151037
#define XONLINE_E_ACCOUNTS_TAG_CHANGE_REQUIRED                      0x80151038
#define XONLINE_E_ACCOUNTS_ACCOUNT_SUSPENDED                        0x80151039
#define XONLINE_E_ACCOUNTS_TERMS_OF_SERVICE_NOT_ACCEPTED            0x8015103A
#define XONLINE_S_LOGON_CONNECTION_ESTABLISHED                      0x001510F0
#define XONLINE_S_LOGON_DISCONNECTED                                0x001510F1
#define XONLINE_E_LOGON_SERVICE_NOT_REQUESTED                       0x80151100
#define XONLINE_E_LOGON_SERVICE_NOT_AUTHORIZED                      0x80151101
#define XONLINE_E_LOGON_SERVICE_TEMPORARILY_UNAVAILABLE             0x80151102
#define XONLINE_E_LOGON_PPLOGIN_FAILED                              0x80151103
#define XONLINE_E_LOGON_SPONSOR_TOKEN_INVALID                       0x80151104
#define XONLINE_E_LOGON_SPONSOR_TOKEN_BANNED                        0x80151105
#define XONLINE_E_LOGON_SPONSOR_TOKEN_USAGE_EXCEEDED                0x80151106
#define XONLINE_E_LOGON_FLASH_UPDATE_NOT_DOWNLOADED                 0x80151107
#define XONLINE_E_LOGON_UPDATE_NOT_DOWNLOADED                       0x80151108
#define XONLINE_E_LOGON_NOT_LOGGED_ON                               0x80151802
#define XONLINE_E_LOGON_DNS_LOOKUP_FAILED                           0x80151903
#define XONLINE_E_LOGON_DNS_LOOKUP_TIMEDOUT                         0x80151904
#define XONLINE_E_LOGON_MACS_FAILED                                 0x80151906
#define XONLINE_E_LOGON_MACS_TIMEDOUT                               0x80151907
#define XONLINE_E_LOGON_AUTHENTICATION_TIMEDOUT                     0x80151909
#define XONLINE_E_LOGON_UNKNOWN_TITLE                               0x80151912
#define XONLINE_E_OFFERING_INVALID_CONSUME_ITEMS                    0x80153013
#define XONLINE_E_ACCOUNTS_NAME_TAKEN                               0x80154000
#define XONLINE_E_ACCOUNTS_INVALID_KINGDOM                          0x80154001
#define XONLINE_E_ACCOUNTS_INVALID_USER                             0x80154002
#define XONLINE_E_ACCOUNTS_BAD_CREDIT_CARD                          0x80154003
#define XONLINE_E_ACCOUNTS_BAD_BILLING_ADDRESS                      0x80154004
#define XONLINE_E_ACCOUNTS_ACCOUNT_BANNED                           0x80154005
#define XONLINE_E_ACCOUNTS_PERMISSION_DENIED                        0x80154006
#define XONLINE_E_ACCOUNTS_INVALID_VOUCHER                          0x80154007
#define XONLINE_E_ACCOUNTS_DATA_CHANGED                             0x80154008
#define XONLINE_E_ACCOUNTS_VOUCHER_ALREADY_USED                     0x80154009
#define XONLINE_E_ACCOUNTS_OPERATION_BLOCKED                        0x8015400A
#define XONLINE_E_ACCOUNTS_POSTAL_CODE_REQUIRED                     0x8015400B
#define XONLINE_E_ACCOUNTS_TRY_AGAIN_LATER                          0x8015400C
#define XONLINE_E_ACCOUNTS_NOT_A_RENEWAL_OFFER                      0x8015400D
#define XONLINE_E_ACCOUNTS_RENEWAL_IS_LOCKED                        0x8015400E
#define XONLINE_E_ACCOUNTS_VOUCHER_REQUIRED                         0x8015400F
#define XONLINE_E_ACCOUNTS_ALREADY_DEPROVISIONED                    0x80154010
#define XONLINE_E_ACCOUNTS_INVALID_PRIVILEGE                        0x80154011
#define XONLINE_E_ACCOUNTS_INVALID_SIGNED_PASSPORT_PUID             0x80154012
#define XONLINE_E_ACCOUNTS_PASSPORT_ALREADY_LINKED                  0x80154013
#define XONLINE_E_ACCOUNTS_MIGRATE_NOT_XBOX1_USER                   0x80154014
#define XONLINE_E_ACCOUNTS_MIGRATE_BAD_SUBSCRIPTION                 0x80154015
#define XONLINE_E_ACCOUNTS_PASSPORT_NOT_LINKED                      0x80154016
#define XONLINE_E_ACCOUNTS_NOT_XENON_USER                           0x80154017
#define XONLINE_E_ACCOUNTS_CREDIT_CARD_REQUIRED                     0x80154018
#define XONLINE_E_ACCOUNTS_MIGRATE_NOT_XBOXCOM_USER                 0x80154019
#define XONLINE_E_ACCOUNTS_NOT_A_VOUCHER_OFFER                      0x8015401A
#define XONLINE_E_ACCOUNTS_REACHED_TRIAL_OFFER_LIMIT                0x8015401B
#define XONLINE_E_ACCOUNTS_XBOX1_MANAGEMENT_BLOCKED                 0x8015401C
#define XONLINE_E_ACCOUNTS_OFFLINE_XUID_ALREADY_USED                0x8015401D
#define XONLINE_E_ACCOUNTS_BILLING_PROVIDER_TIMEOUT                 0x8015401E
#define XONLINE_E_ACCOUNTS_MIGRATION_OFFER_NOT_FOUND                0x8015401F
#define XONLINE_E_ACCOUNTS_UNDER_AGE                                0x80154020
#define XONLINE_E_ACCOUNTS_XBOX1_LOGON_BLOCKED                      0x80154021
#define XONLINE_E_ACCOUNTS_VOUCHER_INVALID_FOR_TIER                 0x80154022
#define XONLINE_E_ACCOUNTS_SWITCH_PASSPORT_QUEUED                   0x80154023
#define XONLINE_E_ACCOUNTS_SERVICE_NOT_PROVISIONED                  0x80154024
#define XONLINE_E_ACCOUNTS_ACCOUNT_UNBAN_BLOCKED                    0x80154025
#define XONLINE_E_ACCOUNTS_SWITCH_PASSPORT_INELIGIBLE               0x80154026
#define XONLINE_E_ACCOUNTS_ADDITIONAL_DATA_REQUIRED                 0x80154027
#define XONLINE_E_ACCOUNTS_SWITCH_PASSPORT_SCS_PENDING              0x80154028
#define XONLINE_E_ACCOUNTS_SWITCH_PASSPORT_NO_BIRTHDATE             0x80154029
#define XONLINE_E_ACCOUNTS_SWITCH_PASSPORT_ADULT_TO_CHILD           0x80154030
//#define XONLINE_E_ACCOUNTS_ WHAT ???                               0x80154038
#define XONLINE_E_ACCOUNTS_USER_GET_ACCOUNT_INFO_ERROR              0x80154098
#define XONLINE_E_ACCOUNTS_USER_OPTED_OUT                           0x80154099
#define XONLINE_E_MATCH_INVALID_SESSION_ID                          0x80155100
#define XONLINE_E_MATCH_INVALID_TITLE_ID                            0x80155101
#define XONLINE_E_MATCH_INVALID_DATA_TYPE                           0x80155102
#define XONLINE_E_MATCH_REQUEST_TOO_SMALL                           0x80155103
#define XONLINE_E_MATCH_REQUEST_TRUNCATED                           0x80155104
#define XONLINE_E_MATCH_INVALID_SEARCH_REQ                          0x80155105
#define XONLINE_E_MATCH_INVALID_OFFSET                              0x80155106
#define XONLINE_E_MATCH_INVALID_ATTR_TYPE                           0x80155107
#define XONLINE_E_MATCH_INVALID_VERSION                             0x80155108
#define XONLINE_E_MATCH_OVERFLOW                                    0x80155109
#define XONLINE_E_MATCH_INVALID_RESULT_COL                          0x8015510A
#define XONLINE_E_MATCH_INVALID_STRING                              0x8015510B
#define XONLINE_E_MATCH_STRING_TOO_LONG                             0x8015510C
#define XONLINE_E_MATCH_BLOB_TOO_LONG                               0x8015510D
#define XONLINE_E_MATCH_INVALID_ATTRIBUTE_ID                        0x80155110
#define XONLINE_E_MATCH_SESSION_ALREADY_EXISTS                      0x80155112
#define XONLINE_E_MATCH_CRITICAL_DB_ERR                             0x80155115
#define XONLINE_E_MATCH_NOT_ENOUGH_COLUMNS                          0x80155116
#define XONLINE_E_MATCH_PERMISSION_DENIED                           0x80155117
#define XONLINE_E_MATCH_INVALID_PART_SCHEME                         0x80155118
#define XONLINE_E_MATCH_INVALID_PARAM                               0x80155119
#define XONLINE_E_MATCH_DATA_TYPE_MISMATCH                          0x8015511D
#define XONLINE_E_MATCH_SERVER_ERROR                                0x8015511E
#define XONLINE_E_MATCH_NO_USERS                                    0x8015511F
#define XONLINE_E_MATCH_INVALID_BLOB                                0x80155120
#define XONLINE_E_MATCH_TOO_MANY_USERS                              0x80155121
#define XONLINE_E_MATCH_INVALID_FLAGS                               0x80155122
#define XONLINE_E_MATCH_PARAM_MISSING                               0x80155123
#define XONLINE_E_MATCH_TOO_MANY_PARAM                              0x80155124
#define XONLINE_E_MATCH_DUPLICATE_PARAM                             0x80155125
#define XONLINE_E_MATCH_TOO_MANY_ATTR                               0x80155126
#define XONLINE_E_SESSION_NOT_FOUND                                 0x80155200
#define XONLINE_E_SESSION_INSUFFICIENT_PRIVILEGES                   0x80155201
#define XONLINE_E_SESSION_FULL                                      0x80155202
#define XONLINE_E_SESSION_INVITES_DISABLED                          0x80155203
#define XONLINE_E_SESSION_INVALID_FLAGS                             0x80155204
#define XONLINE_E_SESSION_REQUIRES_ARBITRATION                      0x80155205
#define XONLINE_E_SESSION_WRONG_STATE                               0x80155206
#define XONLINE_E_SESSION_INSUFFICIENT_BUFFER                       0x80155207
#define XONLINE_E_SESSION_REGISTRATION_ERROR                        0x80155208
#define XONLINE_E_SESSION_NOT_LOGGED_ON                             0x80155209
#define XONLINE_E_SESSION_JOIN_ILLEGAL                              0x8015520A
#define XONLINE_E_SESSION_CREATE_KEY_FAILED                         0x8015520B
#define XONLINE_E_SESSION_NOT_REGISTERED                            0x8015520C
#define XONLINE_E_SESSION_REGISTER_KEY_FAILED                       0x8015520D
#define XONLINE_E_SESSION_UNREGISTER_KEY_FAILED                     0x8015520E
#define XONLINE_E_QUERY_QUOTA_FULL                                  0x80156101
#define XONLINE_E_QUERY_ENTITY_NOT_FOUND                            0x80156102
#define XONLINE_E_QUERY_PERMISSION_DENIED                           0x80156103
#define XONLINE_E_QUERY_ATTRIBUTE_TOO_LONG                          0x80156104
#define XONLINE_E_QUERY_UNEXPECTED_ATTRIBUTE                        0x80156105
#define XONLINE_E_QUERY_INVALID_ACTION                              0x80156107
#define XONLINE_E_QUERY_SPEC_COUNT_MISMATCH                         0x80156108
#define XONLINE_E_QUERY_DATASET_NOT_FOUND                           0x80156109
#define XONLINE_E_QUERY_PROCEDURE_NOT_FOUND                         0x8015610A
#define XONLINE_E_COMP_ACCESS_DENIED                                0x80156202
#define XONLINE_E_COMP_REGISTRATION_CLOSED                          0x80156203
#define XONLINE_E_COMP_FULL                                         0x80156204
#define XONLINE_E_COMP_NOT_REGISTERED                               0x80156205
#define XONLINE_E_COMP_CANCELLED                                    0x80156206
#define XONLINE_E_COMP_CHECKIN_TIME_INVALID                         0x80156207
#define XONLINE_E_COMP_CHECKIN_BAD_EVENT                            0x80156208
#define XONLINE_E_COMP_EVENT_SCORED                                 0x80156209
#define XONLINE_S_COMP_EVENT_SCORED                                 0x00156209
#define XONLINE_E_COMP_UNEXPECTED                                   0x80156210
#define XONLINE_E_COMP_TOPOLOGY_ERROR                               0x80156216
#define XONLINE_E_COMP_TOPOLOGY_PENDING                             0x80156217
#define XONLINE_E_COMP_CHECKIN_TOO_EARLY                            0x80156218
#define XONLINE_E_COMP_ALREADY_REGISTERED                           0x80156219
#define XONLINE_E_COMP_INVALID_ENTRANT_TYPE                         0x8015621A
#define XONLINE_E_COMP_TOO_LATE                                     0x8015621B
#define XONLINE_E_COMP_TOO_EARLY                                    0x8015621C
#define XONLINE_E_COMP_NO_BYES_AVAILABLE                            0x8015621D
#define XONLINE_E_COMP_SERVICE_OUTAGE                               0x8015621E
#define XONLINE_E_MSGSVR_INVALID_REQUEST                            0x80157001
#define XONLINE_E_STRING_TOO_LONG                                   0x80157101
#define XONLINE_E_STRING_OFFENSIVE_TEXT                             0x80157102
#define XONLINE_E_STRING_NO_DEFAULT_STRING                          0x80157103
#define XONLINE_E_STRING_INVALID_LANGUAGE                           0x80157104
#define XONLINE_E_FEEDBACK_NULL_TARGET                              0x80158001
#define XONLINE_E_FEEDBACK_BAD_TYPE                                 0x80158002
#define XONLINE_E_FEEDBACK_CANNOT_LOG                               0x80158006
#define XONLINE_E_STAT_BAD_REQUEST                                  0x80159001
#define XONLINE_E_STAT_INVALID_TITLE_OR_LEADERBOARD                 0x80159002
#define XONLINE_E_STAT_TOO_MANY_SPECS                               0x80159004
#define XONLINE_E_STAT_TOO_MANY_STATS                               0x80159005
#define XONLINE_E_STAT_USER_NOT_FOUND                               0x80159003
#define XONLINE_E_STAT_SET_FAILED_0                                 0x80159100
#define XONLINE_E_STAT_PERMISSION_DENIED                            0x80159200
#define XONLINE_E_STAT_LEADERBOARD_WAS_RESET                        0x80159201
#define XONLINE_E_STAT_INVALID_ATTACHMENT                           0x80159202
#define XONLINE_S_STAT_CAN_UPLOAD_ATTACHMENT                        0x00159203
#define XONLINE_E_STAT_TOO_MANY_PARAMETERS                          0x80159204
#define XONLINE_E_STAT_TOO_MANY_PROCEDURES                          0x80159205
#define XONLINE_E_STAT_STAT_POST_PROC_ERROR                         0x80159206
#define XONLINE_E_STAT_NOT_ENOUGH_PARAMETERS                        0x80159208
#define XONLINE_E_STAT_INVALID_PROCEDURE                            0x80159209
#define XONLINE_E_STAT_EXCEEDED_WRITE_READ_LIMIT                    0x8015920A
#define XONLINE_E_STAT_LEADERBOARD_READONLY                         0x8015920B
#define XONLINE_E_STAT_MUSIGMA_ARITHMETIC_OVERFLOW                  0x8015920C
#define XONLINE_E_STAT_READ_NO_SPEC                                 0x8015920D
#define XONLINE_E_STAT_MUSIGMA_NO_GAME_MODE                         0x8015920E
#define XONLINE_E_SIGNATURE_ERROR                                   0x8015B000
#define XONLINE_E_SIGNATURE_VER_INVALID_SIGNATURE                   0x8015B001
#define XONLINE_E_SIGNATURE_VER_UNKNOWN_KEY_VER                     0x8015B002
#define XONLINE_E_SIGNATURE_VER_UNKNOWN_SIGNATURE_VER               0x8015B003
#define XONLINE_E_SIGNATURE_BANNED_XBOX                             0x8015B004
#define XONLINE_E_SIGNATURE_BANNED_USER                             0x8015B005
#define XONLINE_E_SIGNATURE_BANNED_TITLE                            0x8015B006
#define XONLINE_E_SIGNATURE_BANNED_DIGEST                           0x8015B007
#define XONLINE_E_SIGNATURE_GET_BAD_AUTH_DATA                       0x8015B008
#define XONLINE_E_SIGNATURE_SERVICE_UNAVAILABLE                     0x8015B009
#define XONLINE_E_ARBITRATION_SERVICE_UNAVAILABLE                   0x8015B101
#define XONLINE_E_ARBITRATION_INVALID_REQUEST                       0x8015B102
#define XONLINE_E_ARBITRATION_SESSION_NOT_FOUND                     0x8015B103
#define XONLINE_E_ARBITRATION_REGISTRATION_FLAGS_MISMATCH           0x8015B104
#define XONLINE_E_ARBITRATION_REGISTRATION_SESSION_TIME_MISMATCH    0x8015B105
#define XONLINE_E_ARBITRATION_REGISTRATION_TOO_LATE                 0x8015B106
#define XONLINE_E_ARBITRATION_NEED_TO_REGISTER_FIRST                0x8015B107
#define XONLINE_E_ARBITRATION_TIME_EXTENSION_NOT_ALLOWED            0x8015B108
#define XONLINE_E_ARBITRATION_INCONSISTENT_FLAGS                    0x8015B109
#define XONLINE_E_ARBITRATION_INCONSISTENT_COMPETITION_STATUS       0x8015B10A
#define XONLINE_E_ARBITRATION_REPORT_ALREADY_CALLED                 0x8015B10B
#define XONLINE_E_ARBITRATION_TOO_MANY_XBOXES_IN_SESSION            0x8015B10C
#define XONLINE_E_ARBITRATION_1_XBOX_1_USER_SESSION_NOT_ALLOWED     0x8015B10D
#define XONLINE_E_ARBITRATION_REPORT_TOO_LARGE                      0x8015B10E
#define XONLINE_E_ARBITRATION_INVALID_TEAMTICKET                    0x8015B10F
#define XONLINE_S_ARBITRATION_INVALID_XBOX_SPECIFIED                0x0015B1F0
#define XONLINE_S_ARBITRATION_INVALID_USER_SPECIFIED                0x0015B1F1
#define XONLINE_S_ARBITRATION_DIFFERENT_RESULTS_DETECTED            0x0015B1F2
#define XONLINE_E_STORAGE_INVALID_REQUEST                           0x8015C001
#define XONLINE_E_STORAGE_ACCESS_DENIED                             0x8015C002
#define XONLINE_E_STORAGE_FILE_IS_TOO_BIG                           0x8015C003
#define XONLINE_E_STORAGE_FILE_NOT_FOUND                            0x8015C004
#define XONLINE_E_STORAGE_INVALID_ACCESS_TOKEN                      0x8015C005
#define XONLINE_E_STORAGE_CANNOT_FIND_PATH                          0x8015C006
#define XONLINE_E_STORAGE_FILE_IS_ELSEWHERE                         0x8015C007
#define XONLINE_E_STORAGE_INVALID_STORAGE_PATH                      0x8015C008
#define XONLINE_E_STORAGE_INVALID_FACILITY                          0x8015C009
#define XONLINE_E_STORAGE_UNKNOWN_DOMAIN                            0x8015C00A
#define XONLINE_E_STORAGE_SYNC_TIME_SKEW                            0x8015C00B
#define XONLINE_E_STORAGE_SYNC_TIME_SKEW_LOCALTIME                  0x8015C00C
#define XONLINE_E_STORAGE_QUOTA_EXCEEDED                            0x8015C00D
#define XONLINE_E_STORAGE_FILE_ALREADY_EXISTS                       0x8015C011
#define XONLINE_E_STORAGE_DATABASE_ERROR                            0x8015C012
#define XONLINE_S_STORAGE_FILE_NOT_MODIFIED                         0x0015C013
#define XONLINE_E_STORAGE_UNSUPPORTED_CONTENT_TYPE                  0x8015CA0E

// Errors returned by Messages                          = 0x80155AXX
#define XONLINE_E_MESSAGE_INVALID_MESSAGE_ID            _HRESULT_TYPEDEF_(0x80155A01L)  // the specified message was not found
#define XONLINE_E_MESSAGE_PROPERTY_DOWNLOAD_REQUIRED    _HRESULT_TYPEDEF_(0x80155A02L)  // the property was too large to fit into the details block, it must be retrieved separately using XOnlineMessageDownloadAttachmentxxx
#define XONLINE_E_MESSAGE_PROPERTY_NOT_FOUND            _HRESULT_TYPEDEF_(0x80155A03L)  // the specified property tag was not found
#define XONLINE_E_MESSAGE_NO_VALID_SENDS_TO_REVOKE      _HRESULT_TYPEDEF_(0x80155A04L)  // no valid sends to revoke were found
#define XONLINE_E_MESSAGE_NO_MESSAGE_DETAILS            _HRESULT_TYPEDEF_(0x80155A05L)  // the specified message does not have any details
#define XONLINE_E_MESSAGE_INVALID_TITLE_ID              _HRESULT_TYPEDEF_(0x80155A06L)  // an invalid title ID was specified
#define XONLINE_E_MESSAGE_SENDER_BLOCKED                _HRESULT_TYPEDEF_(0x80155A07L)  // a send failed because the recipient has blocked the sender
#define XONLINE_E_MESSAGE_MAX_DETAILS_SIZE_EXCEEDED     _HRESULT_TYPEDEF_(0x80155A08L)  // the property couldn't be added because the maximum details size would be exceeded
#define XONLINE_E_MESSAGE_INVALID_MESSAGE_TYPE          _HRESULT_TYPEDEF_(0x80155A09L)
#define XONLINE_E_MESSAGE_USER_OPTED_OUT                _HRESULT_TYPEDEF_(0x80155A0AL)  // a send failed because the message is marketing and the recipient has opted-out for the sending title
// Success codes returned by Messages                   = 0x00155AXX
#define XONLINE_S_MESSAGE_PENDING_SYNC                  _HRESULT_TYPEDEF_(0x00155A01L)  // updated message list is currently being retrieved (after logon or disabling summary refresh), returned results may be out of date


typedef enum
{
	XONLINE_NAT_OPEN = 1,
	XONLINE_NAT_MODERATE,
	XONLINE_NAT_STRICT
} XONLINE_NAT_TYPE;

typedef struct _XONLINE_SERVICE_INFO {
	DWORD dwServiceID;
	IN_ADDR serviceIP;
	WORD wServicePort;
	WORD wReserved;
} XONLINE_SERVICE_INFO, *PXONLINE_SERVICE_INFO;

#define XONLINE_NOTIFICATION_TYPE_FRIENDREQUEST    0x00000001
#define XONLINE_NOTIFICATION_TYPE_FRIENDSTATUS     0x00000002
#define XONLINE_NOTIFICATION_TYPE_GAMEINVITE       0x00000004
#define XONLINE_NOTIFICATION_TYPE_GAMEINVITEANSWER 0x00000008
#define XONLINE_NOTIFICATION_TYPE_ALL              0xFFFFFFFF

#define XONLINE_FRIENDSTATE_FLAG_NONE              0x00000000
#define XONLINE_FRIENDSTATE_FLAG_ONLINE            0x00000001
#define XONLINE_FRIENDSTATE_FLAG_PLAYING           0x00000002
#define XONLINE_FRIENDSTATE_FLAG_CLOAKED           0x00000004
#define XONLINE_FRIENDSTATE_FLAG_VOICE             0x00000008
#define XONLINE_FRIENDSTATE_FLAG_JOINABLE          0x00000010
#define XONLINE_FRIENDSTATE_MASK_GUESTS            0x00000060
#define XONLINE_FRIENDSTATE_FLAG_RESERVED0         0x00000080
#define XONLINE_FRIENDSTATE_FLAG_SENTINVITE        0x04000000
#define XONLINE_FRIENDSTATE_FLAG_RECEIVEDINVITE    0x08000000
#define XONLINE_FRIENDSTATE_FLAG_INVITEACCEPTED    0x10000000
#define XONLINE_FRIENDSTATE_FLAG_INVITEREJECTED    0x20000000
#define XONLINE_FRIENDSTATE_FLAG_REQUEST           0x40000000
#define XONLINE_FRIENDSTATE_FLAG_PENDING           0x80000000

// idk the value for any of these.
#define XONLINE_FRIENDSTATE_ENUM_ONLINE 0
#define XONLINE_FRIENDSTATE_ENUM_AWAY 0
#define XONLINE_FRIENDSTATE_ENUM_BUSY 0
#define XONLINE_FRIENDSTATE_MASK_USER_STATE 0
#define XOnlineIsUserAway(dwState) 0
#define XOnlineIsUserBusy(dwState) 0

#define XONLINE_FRIENDSTATE_GET_GUESTS(dwState) ((dwState & XONLINE_FRIENDSTATE_MASK_GUESTS) >> 5)
#define XONLINE_FRIENDSTATE_SET_GUESTS(dwState, bGuests) (dwState &= (bGuests << 5) & XONLINE_FRIENDSTATE_MASK_GUESTS)

typedef enum {
	XONLINE_NOTIFICATION_EVENT_SERVICE,
	NUM_XONLINE_NOTIFICATION_EVENT_TYPES
} XONLINE_NOTIFICATION_EVENT_TYPE;

typedef struct {
	XONLINE_NOTIFICATION_EVENT_TYPE     type;
	union {
		struct {
			DWORD                       dwCode;
		} service;
	} info;
} XONLINE_NOTIFICATION_MSG, *PXONLINE_NOTIFICATION_MSG;

#define MAX_RICHPRESENCE_SIZE 22

typedef struct _XONLINE_PRESENCE {
	XUID xuid;
	DWORD dwState;
	XNKID sessionID;
	DWORD dwTitleID;
	FILETIME ftUserTime;
	DWORD cchRichPresence;
	WCHAR wszRichPresence[MAX_RICHPRESENCE_SIZE];
} XONLINE_PRESENCE, *PXONLINE_PRESENCE;

#define XNET_XNKID_MASK                 0xF0    // Mask of flag bits in first byte of XNKID
#define XNET_XNKID_SYSTEM_LINK          0x00    // Peer to peer system link session
#define XNET_XNKID_SYSTEM_LINK_XPLAT    0x40    // Peer to peer system link session for cross-platform
#define XNET_XNKID_ONLINE_PEER          0x80    // Peer to peer online session
#define XNET_XNKID_ONLINE_SERVER        0xC0    // Client to server online session
#define XNET_XNKID_ONLINE_TITLESERVER   0xE0    // Client to title server online session

#define XNetXnKidFlag(pxnkid)       ((pxnkid)->ab[0] & 0xE0)
#define XNetXnKidIsSystemLinkXbox(pxnkid)       (((pxnkid)->ab[0] & 0xE0) == XNET_XNKID_SYSTEM_LINK)
#define XNetXnKidIsSystemLinkXPlat(pxnkid)      (((pxnkid)->ab[0] & 0xE0) == XNET_XNKID_SYSTEM_LINK_XPLAT)
#define XNetXnKidIsSystemLink(pxnkid)           (XNetXnKidIsSystemLinkXbox(pxnkid) || XNetXnKidIsSystemLinkXPlat(pxnkid))
#define XNetXnKidIsOnlinePeer(pxnkid)           (((pxnkid)->ab[0] & 0xE0) == XNET_XNKID_ONLINE_PEER)
#define XNetXnKidIsOnlineServer(pxnkid)         (((pxnkid)->ab[0] & 0xE0) == XNET_XNKID_ONLINE_SERVER)
#define XNetXnKidIsOnlineTitleServer(pxnkid)    (((pxnkid)->ab[0] & 0xE0) == XNET_XNKID_ONLINE_TITLESERVER)

#define XTITLE_SERVER_MAX_LSP_INFO 1000
#define XTITLE_SERVER_MAX_SERVER_INFO_LEN 199
#define XTITLE_SERVER_MAX_SERVER_INFO_SIZE 200

typedef struct _XTITLE_SERVER_INFO {
	// Actual IP address.
	IN_ADDR inaServer;
	DWORD dwFlags;
	CHAR szServerInfo[XTITLE_SERVER_MAX_SERVER_INFO_SIZE];
} XTITLE_SERVER_INFO, *PXTITLE_SERVER_INFO;

typedef struct
{
	BYTE        ab[16];                         // xbox to xbox key exchange key
} XNKEY;

typedef struct
{
	INT         iStatus;                        // WSAEINPROGRESS if pending; 0 if success; error if failed
	UINT        cina;                           // Count of IP addresses for the given host
	IN_ADDR     aina[8];                        // Vector of IP addresses for the given host
} XNDNS;



#define XNET_XNQOSINFO_COMPLETE         0x01    // Qos has finished processing this entry
#define XNET_XNQOSINFO_TARGET_CONTACTED 0x02    // Target host was successfully contacted
#define XNET_XNQOSINFO_TARGET_DISABLED  0x04    // Target host has disabled its Qos listener
#define XNET_XNQOSINFO_DATA_RECEIVED    0x08    // Target host supplied Qos data
#define XNET_XNQOSINFO_PARTIAL_COMPLETE 0x10    // Qos has unfinished estimates for this entry
#define XNET_XNQOSINFO_UNK	0x11


#define XNET_QOS_LISTEN_ENABLE              0x00000001 // Responds to queries on the given XNKID
#define XNET_QOS_LISTEN_DISABLE             0x00000002 // Rejects queries on the given XNKID
#define XNET_QOS_LISTEN_SET_DATA            0x00000004 // Sets the block of data to send back to queriers
#define XNET_QOS_LISTEN_SET_BITSPERSEC      0x00000008 // Sets max bandwidth that query reponses may consume
#define XNET_QOS_LISTEN_RELEASE             0x00000010 // Stops listening on given XNKID and releases memory

#define XNET_QOS_LOOKUP_RESERVED            0x00000000 // No flags defined yet for XNetQosLookup

#define XNET_QOS_SERVICE_LOOKUP_RESERVED    0x00000000 // No flags defined yet for XNetQosServiceLookup




typedef struct
{
	BYTE        bFlags;                         // See XNET_XNQOSINFO_*
	BYTE        bReserved;                      // Reserved
	WORD        cProbesXmit;                    // Count of Qos probes transmitted
	WORD        cProbesRecv;                    // Count of Qos probes successfully received
	WORD        cbData;                         // Size of Qos data supplied by target (may be zero)
	BYTE *      pbData;                         // Qos data supplied by target (may be NULL)
	WORD        wRttMinInMsecs;                 // Minimum round-trip time in milliseconds
	WORD        wRttMedInMsecs;                 // Median round-trip time in milliseconds
	DWORD       dwUpBitsPerSec;                 // Upstream bandwidth in bits per second
	DWORD       dwDnBitsPerSec;                 // Downstream bandwidth in bits per second
} XNQOSINFO;

typedef struct
{
	UINT        cxnqos;                         // Count of items in axnqosinfo[] array
	UINT        cxnqosPending;                  // Count of items still pending
	XNQOSINFO   axnqosinfo[1];                  // Vector of Qos results
} XNQOS;

typedef struct
{
	DWORD       dwSizeOfStruct;                 // Structure size, must be set prior to calling XNetQosGetListenStats
	DWORD       dwNumDataRequestsReceived;      // Number of client data request probes received
	DWORD       dwNumProbesReceived;            // Number of client probe requests received
	DWORD       dwNumSlotsFullDiscards;         // Number of client requests discarded because all slots are full
	DWORD       dwNumDataRepliesSent;           // Number of data replies sent
	DWORD       dwNumDataReplyBytesSent;        // Number of data reply bytes sent
	DWORD       dwNumProbeRepliesSent;          // Number of probe replies sent
} XNQOSLISTENSTATS;




#define XNET_XNADDR_PLATFORM_XBOX1          0x00000000 // Platform type is original Xbox
#define XNET_XNADDR_PLATFORM_XBOX360        0x00000001 // Platform type is Xbox 360
#define XNET_XNADDR_PLATFORM_WINPC          0x00000002 // Platform type is Windows PC

//INT   WINAPI XNetGetXnAddrPlatform(const XNADDR * pxnaddr, DWORD * pdwPlatform);


#define XNET_CONNECT_STATUS_IDLE            0x00000000 // Connection not started; use XNetConnect or send packet
#define XNET_CONNECT_STATUS_PENDING         0x00000001 // Connecting in progress; not complete yet
#define XNET_CONNECT_STATUS_CONNECTED       0x00000002 // Connection is established
#define XNET_CONNECT_STATUS_LOST            0x00000003 // Connection was lost

//INT   WINAPI XNetConnect(const IN_ADDR ina);
//INT WINAPI XNetGetConnectStatus(const IN_ADDR ina);


#define XNET_GET_XNADDR_PENDING             0x00000000 // Address acquisition is not yet complete
#define XNET_GET_XNADDR_NONE                0x00000001 // XNet is uninitialized or no debugger found
#define XNET_GET_XNADDR_ETHERNET            0x00000002 // Host has ethernet address (no IP address)
#define XNET_GET_XNADDR_STATIC              0x00000004 // Host has statically assigned IP address
#define XNET_GET_XNADDR_DHCP                0x00000008 // Host has DHCP assigned IP address
#define XNET_GET_XNADDR_PPPOE               0x00000010 // Host has PPPoE assigned IP address
#define XNET_GET_XNADDR_GATEWAY             0x00000020 // Host has one or more gateways configured
#define XNET_GET_XNADDR_DNS                 0x00000040 // Host has one or more DNS servers configured
#define XNET_GET_XNADDR_ONLINE              0x00000080 // Host is currently connected to online service
#define XNET_GET_XNADDR_TROUBLESHOOT        0x00008000 // Network configuration requires troubleshooting

//DWORD WINAPI XNetGetTitleXnAddr(XNADDR * pxna);
//DWORD WINAPI XNetGetDebugXnAddr(XNADDR * pxna);


#define XNET_ETHERNET_LINK_INACTIVE           0x00000000
#define XNET_ETHERNET_LINK_ACTIVE           0x00000001 // Ethernet cable is connected and active
#define XNET_ETHERNET_LINK_100MBPS          0x00000002 // Ethernet link is set to 100 Mbps
#define XNET_ETHERNET_LINK_10MBPS           0x00000004 // Ethernet link is set to 10 Mbps
#define XNET_ETHERNET_LINK_FULL_DUPLEX      0x00000008 // Ethernet link is in full duplex mode
#define XNET_ETHERNET_LINK_HALF_DUPLEX      0x00000010 // Ethernet link is in half duplex mode
#define XNET_ETHERNET_LINK_WIRELESS         0x00000020 // Ethernet link is wireless (802.11 based)

//DWORD WINAPI XNetGetEthernetLinkStatus();


#define XNET_BROADCAST_VERSION_OLDER        0x00000001 // Got broadcast packet(s) from incompatible older version of title
#define XNET_BROADCAST_VERSION_NEWER        0x00000002 // Got broadcast packet(s) from incompatible newer version of title

//DWORD WINAPI XNetGetBroadcastVersionStatus(BOOL fReset);


//
// Value = XNetStartupParams
// Get   = Returns the XNetStartupParams values that were used at
//         initialization time.
// Set   = Not allowed.
//
#define XNET_OPTID_STARTUP_PARAMS                   1

//
// Value = ULONGLONG
// Get   = Returns total number of bytes sent by the NIC hardware since system
//         boot, including sizes of all protocol headers.
// Set   = Not allowed.
//
#define XNET_OPTID_NIC_XMIT_BYTES                   2

//
// Value = DWORD
// Get   = Returns total number of frames sent by the NIC hardware since system
//         boot.
// Set   = Not allowed.
//
#define XNET_OPTID_NIC_XMIT_FRAMES                  3

//
// Value = ULONGLONG
// Get   = Returns total number of bytes received by the NIC hardware since
//         system boot, including sizes of all protocol headers.
// Set   = Not allowed.
//
#define XNET_OPTID_NIC_RECV_BYTES                   4

//
// Value = DWORD
// Get   = Returns total number of frames received by the NIC hardware since
//         system boot.
// Set   = Not allowed.
//
#define XNET_OPTID_NIC_RECV_FRAMES                  5

//
// Value = ULONGLONG
// Get   = Returns the number of bytes sent by the caller since XNetStartup/
//         WSAStartup, including sizes of all protocol headers.
// Set   = Not allowed.
//
#define XNET_OPTID_CALLER_XMIT_BYTES                6

//
// Value = DWORD
// Get   = Returns total number of frames sent by the caller since XNetStartup/
//         WSAStartup.
// Set   = Not allowed.
//
#define XNET_OPTID_CALLER_XMIT_FRAMES               7

//
// Value = ULONGLONG
// Get   = Returns total number of bytes received by the caller since
//         XNetStartup/WSAStartup, including sizes of all protocol headers.
// Set   = Not allowed.
//
#define XNET_OPTID_CALLER_RECV_BYTES                8

//
// Value = DWORD
// Get   = Returns total number of frames received by the caller since
//         XNetStartup/WSAStartup.
// Set   = Not allowed.
//
#define XNET_OPTID_CALLER_RECV_FRAMES               9

// Get   = Retrieves 0 for default behavior
// Set   = Set 0 for default behavior

//INT    WINAPI XNetGetOpt(DWORD dwOptId, BYTE * pbValue, DWORD * pdwValueSize);
//INT    WINAPI XNetSetOpt(DWORD dwOptId, const BYTE * pbValue, DWORD dwValueSize);

#define XNET_CONNECT_STATUS_IDLE            0x00000000 // Connection not started; use XNetConnect or send packet
#define XNET_CONNECT_STATUS_PENDING         0x00000001 // Connecting in progress; not complete yet
#define XNET_CONNECT_STATUS_CONNECTED       0x00000002 // Connection is established
#define XNET_CONNECT_STATUS_LOST            0x00000003 // Connection was lost


//
// Since our socket handles are not file handles, apps can NOT call CancelIO API to cancel
// outstanding overlapped I/O requests. Apps must call WSACancelOverlappedIO function instead.
//

#define XUSER_NAME_SIZE                 16
#define XUSER_MAX_NAME_LENGTH           (XUSER_NAME_SIZE - 1)

#define XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY     0x00000001
#define XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY      0x00000002
#define XUSER_GET_SIGNIN_INFO_UNKNOWN_XUID_ONLY      0x00000004

#define XUSER_INFO_FLAG_LIVE_ENABLED    0x00000001
#define XUSER_INFO_FLAG_GUEST           0x00000002

typedef struct _XUSER_DATA
{
	BYTE                                type;

	union
	{
		LONG                            nData;     // XUSER_DATA_TYPE_INT32
		LONGLONG                        i64Data;   // XUSER_DATA_TYPE_INT64
		double                          dblData;   // XUSER_DATA_TYPE_DOUBLE
		struct                                     // XUSER_DATA_TYPE_UNICODE
		{
			DWORD                       cbData;    // Includes null-terminator
			LPWSTR                      pwszData;
		} string;
		FLOAT                           fData;     // XUSER_DATA_TYPE_FLOAT
		struct                                     // XUSER_DATA_TYPE_BINARY
		{
			DWORD                       cbData;
			PBYTE                       pbData;
		} binary;
		FILETIME                        ftData;    // XUSER_DATA_TYPE_DATETIME
	};
} XUSER_DATA, *PXUSER_DATA;

typedef enum _XPRIVILEGE_TYPE
{
	XPRIVILEGE_MULTIPLAYER_SESSIONS = 254, // on|off

	XPRIVILEGE_COMMUNICATIONS = 252, // on (communicate w/everyone) | off (check _FO)
	XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY = 251, // on (communicate w/friends only) | off (blocked)

	XPRIVILEGE_PROFILE_VIEWING = 249, // on (viewing allowed) | off (check _FO)
	XPRIVILEGE_PROFILE_VIEWING_FRIENDS_ONLY = 248, // on (view friend's only) | off (no details)

	XPRIVILEGE_USER_CREATED_CONTENT = 247, // on (allow viewing of UCC) | off (check _FO)
	XPRIVILEGE_USER_CREATED_CONTENT_FRIENDS_ONLY = 246, // on (view UCC from friends only) | off (blocked)

	XPRIVILEGE_PURCHASE_CONTENT = 245, // on (allow purchase) | off (blocked)

	XPRIVILEGE_PRESENCE = 244, // on (share presence info) | off (check _FO)
	XPRIVILEGE_PRESENCE_FRIENDS_ONLY = 243, // on (share w/friends only | off (don't share)

	XPRIVILEGE_TRADE_CONTENT = 238, // on (allow trading) | off (blocked)

	XPRIVILEGE_VIDEO_COMMUNICATIONS = 235, // on (communicate w/everyone) | off (check _FO)
	XPRIVILEGE_VIDEO_COMMUNICATIONS_FRIENDS_ONLY = 234, // on (communicate w/friends only) | off (blocked)

	XPRIVILEGE_MULTIPLAYER_DEDICATED_SERVER = 226, // on (allow) | off (disallow)
} XPRIVILEGE_TYPE;


typedef struct _XUSER_PROPERTY
{
	DWORD                               dwPropertyId;
	XUSER_DATA                          value;
} XUSER_PROPERTY, *PXUSER_PROPERTY;

typedef enum _XUSER_SIGNIN_STATE
{
	eXUserSigninState_NotSignedIn,
	eXUserSigninState_SignedInLocally,
	eXUserSigninState_SignedInToLive
} XUSER_SIGNIN_STATE;

typedef struct {
	XUID                 xuid;
	DWORD                dwInfoFlags;
	XUSER_SIGNIN_STATE   UserSigninState;
	DWORD                dwGuestNumber;
	DWORD                dwSponsorUserIndex;
	CHAR                 szUserName[XUSER_NAME_SIZE];
} XUSER_SIGNIN_INFO, *PXUSER_SIGNIN_INFO;

typedef struct {
	DWORD dwViewId;
	LONGLONG i64Rating;
} XUSER_RANK_REQUEST;

typedef struct {
	DWORD dwNumRanks;
	DWORD *pdwRanks;
} XUSER_ESTIMATE_RANK_RESULTS;

typedef struct _FIND_USER_INFO {
	ULONGLONG qwUserId;
	CHAR szGamerTag;
} FIND_USER_INFO;

typedef struct _FIND_USERS_RESPONSE {
	DWORD dwResults;
	FIND_USER_INFO *pUsers;
} FIND_USERS_RESPONSE;

// Xbox-specific Overlapped

typedef VOID (WINAPI *PXOVERLAPPED_COMPLETION_ROUTINE)(
	DWORD dwErrorCode,
	DWORD dwNumberOfBytesTransfered,
	DWORD pOverlapped
	);


typedef struct {
	ULONG_PTR                           InternalLow;
	ULONG_PTR                           InternalHigh;
	ULONG_PTR                           InternalContext;
	HANDLE                              hEvent;
	PXOVERLAPPED_COMPLETION_ROUTINE     pCompletionRoutine;
	DWORD_PTR                           dwCompletionContext;
	DWORD                               dwExtendedError;
} XOVERLAPPED, *PXOVERLAPPED;

typedef enum {
	XSOURCE_NO_VALUE = 0, // There is no value to read.
	XSOURCE_DEFAULT, // The value was read from the defaults. It has not been overwritten by the user at a system, or title, level.
	XSOURCE_TITLE, // 	The value was overwritten by the title and was read from the stored profile.
	XSOURCE_PERMISSION_DENIED // The requesting user does not have permission to read this value.
} XUSER_PROFILE_SOURCE;

typedef struct _XUSER_PROFILE_SETTING {
	XUSER_PROFILE_SOURCE source;

	union {
		DWORD dwUserIndex;
		XUID xuid;
	}user;

	DWORD dwSettingId;

	XUSER_DATA data;
} XUSER_PROFILE_SETTING, *PXUSER_PROFILE_SETTING;

typedef struct _XUSER_READ_PROFILE_SETTING_RESULT {
	DWORD dwSettingsLen;
	XUSER_PROFILE_SETTING *pSettings;
} XUSER_READ_PROFILE_SETTING_RESULT, *PXUSER_READ_PROFILE_SETTING_RESULT;

#define XPROFILE_GAMER_YAXIS_INVERSION 0x10040002
#define XPROFILE_OPTION_CONTROLLER_VIBRATION 0x10040003
#define XPROFILE_GAMERCARD_ZONE 0x10040004
#define XPROFILE_GAMERCARD_REGION 0x10040005
#define XPROFILE_GAMERCARD_CRED 0x10040006
#define XPROFILE_GAMERCARD_REP 0x5004000B
#define XPROFILE_OPTION_VOICE_MUTED 0x1004000C
#define XPROFILE_OPTION_VOICE_THRU_SPEAKERS 0x1004000D
#define XPROFILE_OPTION_VOICE_VOLUME 0x1004000E
#define XPROFILE_GAMERCARD_MOTTO 0x402C0011
#define XPROFILE_GAMERCARD_TITLES_PLAYED 0x10040012
#define XPROFILE_GAMERCARD_ACHIEVEMENTS_EARNED 0x10040013
#define XPROFILE_GAMER_DIFFICULTY 0x10040015
#define XPROFILE_GAMER_CONTROL_SENSITIVITY 0x10040018
#define XPROFILE_GAMER_PREFERRED_COLOR_FIRST 0x1004001D
#define XPROFILE_GAMER_PREFERRED_COLOR_SECOND 0x1004001E
#define XPROFILE_GAMER_ACTION_AUTO_AIM 0x10040022
#define XPROFILE_GAMER_ACTION_AUTO_CENTER 0x10040023
#define XPROFILE_GAMER_ACTION_MOVEMENT_CONTROL 0x10040024
#define XPROFILE_GAMER_RACE_TRANSMISSION 0x10040026
#define XPROFILE_GAMER_RACE_CAMERA_LOCATION 0x10040027
#define XPROFILE_GAMER_RACE_BRAKE_CONTROL 0x10040028
#define XPROFILE_GAMER_RACE_ACCELERATOR_CONTROL 0x10040029
#define XPROFILE_GAMERCARD_TITLE_CRED_EARNED 0x10040038
#define XPROFILE_GAMERCARD_TITLE_ACHIEVEMENTS_EARNED 0x10040039
#define XPROFILE_GAMERCARD_PICTURE_KEY 0x4064000F

#define XPROFILE_TITLE_SPECIFIC1 0x63E83FFF
#define XPROFILE_TITLE_SPECIFIC2 0x63E83FFE
#define XPROFILE_TITLE_SPECIFIC3 0x63E83FFD

#pragma pack(push, 1) // Save then set byte alignment setting.
typedef struct {
	unsigned int id : 16;
	//unsigned int id : 14;
	//unsigned int unknown : 2; // These bits are not used afaik. Probably part of id. Could also be an 8 and 8 devide with some other important setting.
	unsigned int data_size : 12; // Is the exact data size, or the max (UNICODE, BINARY).
	unsigned int data_type : 4; // XUSER_DATA_TYPE_
} SETTING_ID;
#pragma pack(pop) // Return to original alignment setting.

//#define SETTING_ID_GET_DATA_TYPE(settingId) (((settingId) >> 28) & 0xF)
//#define SETTING_ID_GET_DATA_MAX_LENGTH(settingId) (((settingId) >> 16) & 0xFFF)

#define DASHBOARD_TITLE_ID 0xFFFE07D1


//TODO Check this guess
#define XLIVE_CONTENT_FLAG_RETRIEVE_FOR_ALL_CONTENT_TYPES 0x2
//TODO Check this guess
#define XLIVE_CONTENT_FLAG_RETRIEVE_FOR_ALL_TITLES 0x4

#define XLIVE_CONTENT_FLAG_RETRIEVE_FOR_ALL_USERS 0x00000001

#define XLIVE_CONTENT_FLAG_RETRIEVE_BY_XUID 0x00000008

// XContent

// Content types
#define XCONTENTTYPE_SAVEDGAME                      0x00000001
#define XCONTENTTYPE_MARKETPLACE                    0x00000002
#define XCONTENTTYPE_PUBLISHER                      0x00000003

#define XCONTENTTYPE_GAMEDEMO                       0x00080000
#define XCONTENTTYPE_ARCADE                         0x000D0000

//  Content creation/open flags
#define XCONTENTFLAG_NONE                           0x00000000
#define XCONTENTFLAG_CREATENEW                      CREATE_NEW
#define XCONTENTFLAG_CREATEALWAYS                   CREATE_ALWAYS
#define XCONTENTFLAG_OPENEXISTING                   OPEN_EXISTING
#define XCONTENTFLAG_OPENALWAYS                     OPEN_ALWAYS
#define XCONTENTFLAG_TRUNCATEEXISTING               TRUNCATE_EXISTING

//  Content attributes
#define XCONTENTFLAG_NOPROFILE_TRANSFER             0x00000010
#define XCONTENTFLAG_NODEVICE_TRANSFER              0x00000020
#define XCONTENTFLAG_STRONG_SIGNED                  0x00000040
#define XCONTENTFLAG_ALLOWPROFILE_TRANSFER          0x00000080
#define XCONTENTFLAG_MOVEONLY_TRANSFER              0x00000800

//  Device selector flags
#define XCONTENTFLAG_MANAGESTORAGE                  0x00000100
#define XCONTENTFLAG_FORCE_SHOW_UI                  0x00000200

//  Enumeration scoping
#define XCONTENTFLAG_ENUM_EXCLUDECOMMON             0x00001000


#define XCONTENT_MAX_DISPLAYNAME_LENGTH 128
#define XCONTENT_MAX_FILENAME_LENGTH    42
#define XCONTENTDEVICE_MAX_NAME_LENGTH  27

#define XLIVE_CONTENT_ID_SIZE 20

typedef DWORD                           XCONTENTDEVICEID, *PXCONTENTDEVICEID;

typedef struct _XCONTENT_DATA {
	DWORD ContentNum;
	DWORD TitleId;
	DWORD ContentPackageType;
	BYTE ContentId[20];
} XCONTENT_DATA, *PXCONTENT_DATA;

typedef struct _XLIVE_CONTENT_INFO {
	DWORD dwContentAPIVersion;
	DWORD dwTitleID;
	DWORD dwContentType;
	BYTE abContentID[XLIVE_CONTENT_ID_SIZE];
} XLIVE_CONTENT_INFO, *PXLIVE_CONTENT_INFO;

typedef enum _XLIVE_CONTENT_INSTALL_NOTIFICATION
{
	XLIVE_CONTENT_INSTALL_NOTIFY_TICKS_REQUIRED = 0,
	XLIVE_CONTENT_INSTALL_NOTIFY_STARTCOPY,
	XLIVE_CONTENT_INSTALL_NOTIFY_ENDCOPY,
	XLIVE_CONTENT_INSTALL_NOTIFY_STARTLICENSEVERIFY,
	XLIVE_CONTENT_INSTALL_NOTIFY_ENDLICENSEVERIFY,
	XLIVE_CONTENT_INSTALL_NOTIFY_STARTDELETE,
	XLIVE_CONTENT_INSTALL_NOTIFY_ENDDELETE,
	XLIVE_CONTENT_INSTALL_NOTIFY_STARTMOVE,
	XLIVE_CONTENT_INSTALL_NOTIFY_ENDMOVE,
	XLIVE_CONTENT_INSTALL_NOTIFY_STARTFILEVERIFY,
	XLIVE_CONTENT_INSTALL_NOTIFY_ENDFILEVERIFY
} XLIVE_CONTENT_INSTALL_NOTIFICATION, *PXLIVE_CONTENT_INSTALL_NOTIFICATION;

typedef HRESULT(*XLIVE_CONTENT_INSTALL_CALLBACK)(
	VOID *pContext,
	XLIVE_CONTENT_INSTALL_NOTIFICATION Notification,
	DWORD dwCurrentTick,
	UINT_PTR Param1,
	UINT_PTR Param2,
	HRESULT hrCurrentStatus
	);

typedef struct _XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS {
	DWORD cbSize;
	XLIVE_CONTENT_INSTALL_CALLBACK *pInstallCallback;
	VOID *pInstallCallbackContext;
	// idk function signature.
	void *pInstallCallbackEx;
} XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS, *PXLIVE_CONTENT_INSTALL_CALLBACK_PARAMS, XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V2;

typedef struct {
	DWORD cbSize;
	XLIVE_CONTENT_INSTALL_CALLBACK *pInstallCallback;
	VOID *pInstallCallbackContext;
} XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V1;

typedef struct _XUSER_ACHIEVEMENT {
	DWORD dwUserIndex;
	DWORD dwAchievementId;
} XUSER_ACHIEVEMENT, *PXUSER_ACHIEVEMENT;


typedef struct {
	XNKID sessionID;
	XNADDR hostAddress;
	XNKEY keyExchangeKey;
} XSESSION_INFO, *PXSESSION_INFO;

typedef enum _XSESSION_STATE
{
	XSESSION_STATE_LOBBY = 0,
	XSESSION_STATE_REGISTRATION,
	XSESSION_STATE_INGAME,
	XSESSION_STATE_REPORTING,
	XSESSION_STATE_DELETED
} XSESSION_STATE;


typedef struct {
	XUID xuidOnline;
	DWORD dwUserIndex;
	DWORD dwFlags;
} XSESSION_MEMBER;

// Unknown actual value.
#define XSESSION_MEMBER_FLAGS_PRIVATE_SLOT  0x1
// Unknown actual value.
#define XSESSION_MEMBER_FLAGS_ZOMBIE        0x2


typedef struct {
	DWORD dwUserIndexHost;
	DWORD dwGameType;
	DWORD dwGameMode;
	DWORD dwFlags;
	DWORD dwMaxPublicSlots;
	DWORD dwMaxPrivateSlots;
	DWORD dwAvailablePublicSlots;
	DWORD dwAvailablePrivateSlots;
	DWORD dwActualMemberCount;
	DWORD dwReturnedMemberCount;
	XSESSION_STATE eState;
	ULONGLONG qwNonce;
	XSESSION_INFO sessionInfo;
	XNKID xnkidArbitration;
	XSESSION_MEMBER *pSessionMembers;
} XSESSION_LOCAL_DETAILS, *PXSESSION_LOCAL_DETAILS;


#pragma pack(push, 1) // Save then set byte alignment setting.
typedef struct _XUSER_CONTEXT
{
	DWORD                               dwContextId;
	DWORD                               dwValue;
} XUSER_CONTEXT, *PXUSER_CONTEXT;
#pragma pack(pop) // Return to original alignment setting.

typedef struct _XSESSION_SEARCHRESULT
{
	XSESSION_INFO   info;
	DWORD           dwOpenPublicSlots;
	DWORD           dwOpenPrivateSlots;
	DWORD           dwFilledPublicSlots;
	DWORD           dwFilledPrivateSlots;
	DWORD           cProperties;
	DWORD           cContexts;
	PXUSER_PROPERTY pProperties;
	PXUSER_CONTEXT  pContexts;
} XSESSION_SEARCHRESULT, *PXSESSION_SEARCHRESULT;

typedef struct _XSESSION_SEARCHRESULT_HEADER
{
	DWORD dwSearchResults;
	XSESSION_SEARCHRESULT *pResults;
} XSESSION_SEARCHRESULT_HEADER, *PXSESSION_SEARCHRESULT_HEADER;

typedef struct _XSESSION_REGISTRANT
{
	ULONGLONG qwMachineID;
	DWORD bTrustworthiness;
	DWORD bNumUsers;
	XUID *rgUsers;
} XSESSION_REGISTRANT;

typedef struct _XSESSION_REGISTRATION_RESULTS
{
	DWORD wNumRegistrants;
	XSESSION_REGISTRANT *rgRegistrants;
} XSESSION_REGISTRATION_RESULTS, *PXSESSION_REGISTRATION_RESULTS;

typedef struct {
	DWORD dwViewId;
	DWORD dwNumProperties;
	XUSER_PROPERTY *pProperties;
} XSESSION_VIEW_PROPERTIES;

typedef struct _MESSAGEBOX_RESULT {
	union {
		DWORD dwButtonPressed;
		WORD rgwPasscode[4];
	};
}	MESSAGEBOX_RESULT, *PMESSAGEBOX_RESULT;


typedef enum _XSTORAGE_FACILITY
{
	XSTORAGE_FACILITY_GAME_CLIP = 1,
	XSTORAGE_FACILITY_PER_TITLE = 2,
	XSTORAGE_FACILITY_PER_USER_TITLE = 3
} XSTORAGE_FACILITY;
#define XSTORAGE_FACILITY_INFO_GAME_CLIP DWORD
#define MAX_STORAGE_RESULTS 0x100
#define XONLINE_MAX_PATHNAME_LENGTH 255

#pragma pack(push, 1) // Save then set byte alignment setting.

typedef struct _XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS {
	DWORD dwBytesTotal;
	XUID xuidOwner;
	FILETIME ftCreated;
} XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS;

typedef struct _XSTORAGE_FILE_INFO {
	DWORD dwTitleID;
	DWORD dwTitleVersion;
	ULONGLONG qwOwnerPUID;
	BYTE bCountryID;
	ULONGLONG qwReserved;
	DWORD dwContentType;
	DWORD dwStorageSize;
	DWORD dwInstalledSize;
	FILETIME ftCreated;
	FILETIME ftLastModified;
	WORD wAttributesSize;
	WORD cchPathName;
	WCHAR *pwszPathName;
	BYTE *pbAttributes;
} XSTORAGE_FILE_INFO, *PXSTORAGE_FILE_INFO;

typedef struct _XSTORAGE_ENUMERATE_RESULTS {
	DWORD dwTotalNumItems;
	DWORD dwNumItemsReturned;
	XSTORAGE_FILE_INFO *pItems;
} XSTORAGE_ENUMERATE_RESULTS;

#pragma pack(pop) // Return to original alignment setting.


#define XMARKETPLACE_CONTENT_ID_LEN 20
#define XMARKETPLACE_MAX_OFFERIDS 20
// idk real macro name
#define XMARKETPLACE_ASSET_ID_MAX 0x7FFFFFFF
#define XMARKETPLACE_ASSET_SIGNATURE_SIZE
// idk value.
#define XMARKETPLACE_IMAGE_URL_MINIMUM_WCHARCOUNT 1

typedef struct {
    DWORD dwAssetID;
    DWORD dwQuantity;
} XMARKETPLACE_ASSET, *PXMARKETPLACE_ASSET;

// typedef struct {
//     BYTE signature[XMARKETPLACE_ASSET_SIGNATURE_SIZE];
//     XMARKETPLACE_ASSET_PACKAGE assetPackage;
// } XMARKETPLACE_ASSET_ENUMERATE_REPLY, *PXMARKETPLACE_ASSET_ENUMERATE_REPLY;

typedef struct {
    FILETIME ftEnumerate;
    DWORD cAssets;
    DWORD cTotalAssets;
    XMARKETPLACE_ASSET aAssets[1];
} XMARKETPLACE_ASSET_PACKAGE, *PXMARKETPLACE_ASSET_PACKAGE;
//sizeof(XMARKETPLACE_ASSET_PACKAGE) + ((cAssets - 1) * sizeof(XMARKETPLACE_ASSET));

typedef struct {
	ULONGLONG qwOfferID;
	ULONGLONG qwPreviewOfferID;
	DWORD dwOfferNameLength;
	WCHAR *wszOfferName;
	DWORD dwOfferType;
	BYTE contentId[XMARKETPLACE_CONTENT_ID_LEN];
	BOOL fIsUnrestrictedLicense;
	DWORD dwLicenseMask;
	DWORD dwTitleID;
	DWORD dwContentCategory;
	DWORD dwTitleNameLength;
	WCHAR *wszTitleName;
	BOOL fUserHasPurchased;
	DWORD dwPackageSize;
	DWORD dwInstallSize;
	DWORD dwSellTextLength;
	WCHAR *wszSellText;
	DWORD dwAssetID;
	DWORD dwPurchaseQuantity;
	DWORD dwPointsPrice;
} XMARKETPLACE_CONTENTOFFER_INFO, *PXMARKETPLACE_CONTENTOFFER_INFO;

typedef struct {
    DWORD dwNewOffers;
    DWORD dwTotalOffers;
} XOFFERING_CONTENTAVAILABLE_RESULT;

typedef enum
{
	XMARKETPLACE_OFFERING_TYPE_CONTENT = 0x00000002,
	XMARKETPLACE_OFFERING_TYPE_GAME_DEMO = 0x00000020,
	XMARKETPLACE_OFFERING_TYPE_GAME_TRAILER = 0x00000040,
	XMARKETPLACE_OFFERING_TYPE_THEME = 0x00000080,
	XMARKETPLACE_OFFERING_TYPE_TILE = 0x00000800,
	XMARKETPLACE_OFFERING_TYPE_ARCADE = 0x00002000,
	XMARKETPLACE_OFFERING_TYPE_VIDEO = 0x00004000,
	XMARKETPLACE_OFFERING_TYPE_CONSUMABLE = 0x00010000,
	XMARKETPLACE_OFFERING_TYPE_AVATARITEM = 0x00100000
} XMARKETPLACE_OFFERING_TYPE;

#define XLIVE_MAX_RETURNED_OFFER_INFO 50
#define XLIVE_OFFER_INFO_VERSION 2
// including null terminator.
#define XLIVE_OFFER_INFO_TITLE_LENGTH 50
// including null terminator.
#define XLIVE_OFFER_INFO_DESCRIPTION_LENGTH 500
// including null terminator.
#define XLIVE_OFFER_INFO_IMAGEURL_LENGTH 1024

typedef struct _XLIVE_OFFER_INFO {
	ULONGLONG qwOfferID;
	WCHAR pszName[XLIVE_OFFER_INFO_TITLE_LENGTH];
	WCHAR pszDescription[XLIVE_OFFER_INFO_DESCRIPTION_LENGTH];
	WCHAR pszImageUrl[XLIVE_OFFER_INFO_IMAGEURL_LENGTH];
	DWORD dwPointsCost;
	WCHAR pszGameTitle[XLIVE_OFFER_INFO_TITLE_LENGTH];
	WCHAR pszMediaType[XLIVE_OFFER_INFO_TITLE_LENGTH];
} XLIVE_OFFER_INFO, *PXLIVE_OFFER_INFO, XLIVE_OFFER_INFO_V2;

typedef struct {
	ULONGLONG qwOfferID;
	WCHAR pszName[XLIVE_OFFER_INFO_TITLE_LENGTH];
	WCHAR pszDescription[XLIVE_OFFER_INFO_DESCRIPTION_LENGTH];
	WCHAR pszImageUrl[XLIVE_OFFER_INFO_IMAGEURL_LENGTH];
	DWORD dwPointsCost;
	WCHAR pszGameTitle[XLIVE_OFFER_INFO_TITLE_LENGTH];
} XLIVE_OFFER_INFO_V1;

typedef struct _STRING_DATA {
	WORD wStringSize;
	WCHAR *pszString;
} STRING_DATA;


#pragma pack(push, 1) // Save then set byte alignment setting.
typedef struct _STRING_VERIFY_RESPONSE {
	WORD wNumStrings;
	HRESULT *pStringResult;
} STRING_VERIFY_RESPONSE;
#pragma pack(pop) // Return to original alignment setting.



#define XNID(Version, Area, Index)      (DWORD)( (WORD)(Area) << 25 | (WORD)(Version) << 16 | (WORD)(Index))
#define XNID_VERSION(msgid)             (((msgid) >> 16) & 0x1FF)
#define XNID_AREA(msgid)                (((msgid) >> 25) & 0x3F)
#define XNID_INDEX(msgid)               ((msgid) & 0xFFFF)


//
// Notification Areas
//

#define XNOTIFY_SYSTEM                  (0x00000001)
#define XNOTIFY_LIVE                    (0x00000002)
#define XNOTIFY_FRIENDS                 (0x00000004)
#define XNOTIFY_CUSTOM                  (0x00000008)
#define XNOTIFY_XMP                     (0x00000020)
#define XNOTIFY_MSGR                    (0x00000040)
#define XNOTIFY_PARTY                   (0x00000080)
#define XNOTIFY_ALL                     (XNOTIFY_SYSTEM | XNOTIFY_LIVE | XNOTIFY_FRIENDS | XNOTIFY_CUSTOM | XNOTIFY_XMP | XNOTIFY_MSGR | XNOTIFY_PARTY)

//
// Bit numbers of each area (bit 0 is the least significant bit)
//

#define _XNAREA_SYSTEM                  (0)
#define _XNAREA_LIVE                    (1)
#define _XNAREA_FRIENDS                 (2)
#define _XNAREA_CUSTOM                  (3)
#define _XNAREA_XMP                     (5)
#define _XNAREA_MSGR                    (6)
#define _XNAREA_PARTY                   (7)

//
// System notifications
//

#define XN_SYS_FIRST                    XNID(0, _XNAREA_SYSTEM, 0x0001)
#define XN_SYS_UI                       XNID(0, _XNAREA_SYSTEM, 0x0009)
#define XN_SYS_SIGNINCHANGED            XNID(0, _XNAREA_SYSTEM, 0x000a)
#define XN_SYS_STORAGEDEVICESCHANGED    XNID(0, _XNAREA_SYSTEM, 0x000b)
#define XN_SYS_PROFILESETTINGCHANGED    XNID(0, _XNAREA_SYSTEM, 0x000e)
#define XN_SYS_MUTELISTCHANGED          XNID(0, _XNAREA_SYSTEM, 0x0011)
#define XN_SYS_INPUTDEVICESCHANGED      XNID(0, _XNAREA_SYSTEM, 0x0012)
#define XN_SYS_INPUTDEVICECONFIGCHANGED XNID(1, _XNAREA_SYSTEM, 0x0013)
#define XN_SYS_PLAYTIMERNOTICE          XNID(3, _XNAREA_SYSTEM, 0x0015)
#define XN_SYS_AVATARCHANGED            XNID(4, _XNAREA_SYSTEM, 0x0017)
#define XN_SYS_NUIHARDWARESTATUSCHANGED XNID(6, _XNAREA_SYSTEM, 0x0019)
#define XN_SYS_NUIPAUSE                 XNID(6, _XNAREA_SYSTEM, 0x001a)
#define XN_SYS_NUIUIAPPROACH            XNID(6, _XNAREA_SYSTEM, 0x001b)
#define XN_SYS_DEVICEREMAP              XNID(6, _XNAREA_SYSTEM, 0x001c)
#define XN_SYS_NUIBINDINGCHANGED        XNID(6, _XNAREA_SYSTEM, 0x001d)
#define XN_SYS_AUDIOLATENCYCHANGED      XNID(8, _XNAREA_SYSTEM, 0x001e)
#define XN_SYS_NUICHATBINDINGCHANGED    XNID(8, _XNAREA_SYSTEM, 0x001f)
#define XN_SYS_INPUTACTIVITYCHANGED     XNID(9, _XNAREA_SYSTEM, 0x0020)
#define XN_SYS_LAST                     XNID(0, _XNAREA_SYSTEM, 0x0023)

#define XN_SYS_XLIVETITLEUPDATE ?
#define XN_SYS_XLIVESYSTEMUPDATE ?

//
// Live notifications
//

#define XN_LIVE_FIRST                   XNID(0, _XNAREA_LIVE, 0x0001)
#define XN_LIVE_CONNECTIONCHANGED       XNID(0, _XNAREA_LIVE, 0x0001)
#define XN_LIVE_INVITE_ACCEPTED         XNID(0, _XNAREA_LIVE, 0x0002)
#define XN_LIVE_LINK_STATE_CHANGED      XNID(0, _XNAREA_LIVE, 0x0003)
#define XN_LIVE_CONTENT_INSTALLED       XNID(0, _XNAREA_LIVE, 0x0007)
#define XN_LIVE_MEMBERSHIP_PURCHASED    XNID(0, _XNAREA_LIVE, 0x0008)
#define XN_LIVE_VOICECHAT_AWAY          XNID(0, _XNAREA_LIVE, 0x0009)
#define XN_LIVE_PRESENCE_CHANGED        XNID(0, _XNAREA_LIVE, 0x000A)
#define XN_LIVE_LAST                    XNID(XNID_CURRENTVERSION+1, _XNAREA_LIVE, 0x0014)

//
// Friends notifications
//

#define XN_FRIENDS_FIRST                XNID(0, _XNAREA_FRIENDS, 0x0001)
#define XN_FRIENDS_PRESENCE_CHANGED     XNID(0, _XNAREA_FRIENDS, 0x0001)
#define XN_FRIENDS_FRIEND_ADDED         XNID(0, _XNAREA_FRIENDS, 0x0002)
#define XN_FRIENDS_FRIEND_REMOVED       XNID(0, _XNAREA_FRIENDS, 0x0003)
#define XN_FRIENDS_LAST                 XNID(XNID_CURRENTVERSION+1, _XNAREA_FRIENDS, 0x0009)

//
// Custom notifications
//

#define XN_CUSTOM_FIRST                 XNID(0, _XNAREA_CUSTOM, 0x0001)
#define XN_CUSTOM_ACTIONPRESSED         XNID(0, _XNAREA_CUSTOM, 0x0003)
#define XN_CUSTOM_GAMERCARD             XNID(1, _XNAREA_CUSTOM, 0x0004)
#define XN_CUSTOM_LAST                  XNID(XNID_CURRENTVERSION+1, _XNAREA_CUSTOM, 0x0005)


//
// XMP notifications
//

#define XN_XMP_FIRST                                     XNID(0, _XNAREA_XMP, 0x0001)
#define XN_XMP_STATECHANGED                              XNID(0, _XNAREA_XMP, 0x0001)
#define XN_XMP_PLAYBACKBEHAVIORCHANGED                   XNID(0, _XNAREA_XMP, 0x0002)
#define XN_XMP_PLAYBACKCONTROLLERCHANGED                 XNID(0, _XNAREA_XMP, 0x0003)
#define XN_XMP_LAST                                      XNID(XNID_CURRENTVERSION+1, _XNAREA_XMP, 0x000D)


//
// Party notifications
//

#define XN_PARTY_FIRST                                   XNID(0, _XNAREA_PARTY, 0x0001)
#define XN_PARTY_MEMBERS_CHANGED                         XNID(4, _XNAREA_PARTY, 0x0002)
#define XN_PARTY_LAST                                    XNID(XNID_CURRENTVERSION+1, _XNAREA_PARTY, 0x0006)






// xbox.h

#define CUSTOMACTION_FLAG_CLOSESUI      1

typedef enum
{
	XMSG_FLAG_DISABLE_EDIT_RECIPIENTS = 0x00000001
} XMSG_FLAGS;

#define XMSG_MAX_CUSTOM_IMAGE_SIZE      (36*1024)

typedef enum
{
	XCUSTOMACTION_FLAG_CLOSES_GUIDE = 0x00000001,
	XCUSTOMACTION_FLAG_DELETES_MESSAGE = 0x00000002
} XCUSTOMACTION_FLAGS;

#define XMSG_MAX_CUSTOMACTION_TRANSLATIONS      11


#define XCUSTOMACTION_MAX_PAYLOAD_SIZE  1024

#define XPLAYERLIST_CUSTOMTEXT_MAX_LENGTH   31
#define XPLAYERLIST_TITLE_MAX_LENGTH        36
#define XPLAYERLIST_DESCRIPTION_MAX_LENGTH  83
#define XPLAYERLIST_IMAGE_MAX_SIZE          36864
#define XPLAYERLIST_MAX_PLAYERS             100
#define XPLAYERLIST_BUTTONTEXT_MAX_LENGTH   23

typedef struct
{
	XUID        xuid;
	WCHAR       wszCustomText[XPLAYERLIST_CUSTOMTEXT_MAX_LENGTH];
} XPLAYERLIST_USER;

typedef struct {
	DWORD dwType;
	WCHAR wszCustomText[2000];
} XPLAYERLIST_BUTTON;

typedef struct {
	XUID xuidSelected;
	DWORD dwKeyCode;
} XPLAYERLIST_RESULT;

typedef enum
{
	XPLAYERLIST_FLAG_CUSTOMTEXT = 0x00000001,
} XPLAYERLIST_FLAGS;

typedef enum _XSHOWMARKETPLACEUI_ENTRYPOINTS {
	XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTLIST,
	XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTITEM,
	XSHOWMARKETPLACEUI_ENTRYPOINT_MEMBERSHIPLIST,
	XSHOWMARKETPLACEUI_ENTRYPOINT_MEMBERSHIPITEM,
	XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTLIST_BACKGROUND,
	XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTITEM_BACKGROUND,
	XSHOWMARKETPLACEUI_ENTRYPOINT_MAX
} XSHOWMARKETPLACEUI_ENTRYPOINTS;

#define MPDI_E_CANCELLED            _HRESULT_TYPEDEF_(0x8057F001)
#define MPDI_E_INVALIDARG           _HRESULT_TYPEDEF_(0x8057F002)
#define MPDI_E_OPERATION_FAILED     _HRESULT_TYPEDEF_(0x8057F003)

typedef enum _XSHOWMARKETPLACEDOWNLOADITEMSUI_ENTRYPOINTS {
	XSHOWMARKETPLACEDOWNLOADITEMS_ENTRYPOINT_FREEITEMS = 1000,
	XSHOWMARKETPLACEDOWNLOADITEMS_ENTRYPOINT_PAIDITEMS,
	XSHOWMARKETPLACEDOWNLOADITEMS_ENTRYPOINT_MAX
} XSHOWMARKETPLACEDOWNLOADITEMSUI_ENTRYPOINTS;

#define XMB_NOICON 0x00000000
#define XMB_ERRORICON 0x00000001
#define XMB_WARNINGICON 0x00000002
#define XMB_ALERTICON 0x00000003

#define XMB_PASSCODEMODE 0x00010000
#define XMB_VERIFYPASSCODEMODE 0x00020000

#define XMB_MAXBUTTONS 3

#define XMB_OK 1
#define XMB_CANCEL 2

#define VKBD_DEFAULT                    0x00000000 // Character set selected in the Guide.
#define VKBD_LATIN_FULL                 0x00000001
#define VKBD_LATIN_EMAIL                0x00000002
#define VKBD_LATIN_GAMERTAG             0x00000004
#define VKBD_LATIN_PHONE                0x00000008
#define VKBD_LATIN_IP_ADDRESS           0x00000010
#define VKBD_LATIN_NUMERIC              0x00000020
#define VKBD_LATIN_ALPHABET             0x00000040
#define VKBD_LATIN_PASSWORD             0x00000080 // Subset of Latin character set appropriate for valid passwords.
#define VKBD_LATIN_SUBSCRIPTION         0x00000100
#define VKBD_JAPANESE_FULL              0x00001000
#define VKBD_KOREAN_FULL                0x00002000
#define VKBD_TCH_FULL                   0x00004000
#define VKBD_RUSSIAN_FULL               0x00008000
// VKBD_LATIN_EXTENDED provides support for Eastern and Central European
// characters
#define VKBD_LATIN_EXTENDED             0x00010000
#define VKBD_SCH_FULL                   0x00020000
#define VKBD_JAPANESE_HIRAGANA          0x00040000
#define VKBD_JAPANESE_KATAKANA          0x00080000
// VKBD_GREEK_FULL and VKBD_CSH_FULL are LiveSignup-Only keyboards
#define VKBD_GREEK_FULL                 0x00100000
#define VKBD_CSH_FULL                   0x00200000 // Czech, Slovak, Hungarian

#define VKBD_SELECT_OK                  0x10000000
#define VKBD_HIGHLIGHT_TEXT             0x20000000 // When set, outputs highlighted characters.


#define VKBD_ENABLEIME ? // When set, enables the input method editor on the keyboard.
#define VKBD_MULTILINE ? // When set, enables multi-line mode. When not set, the return key is considered the OK button.

typedef struct _XINPUT_KEYSTROKE {
	WORD VirtualKey;
	WCHAR Unicode;
	WORD Flags;
	BYTE UserIndex;
	BYTE HidCode;
} XINPUT_KEYSTROKE, *PXINPUT_KEYSTROKE;

typedef struct
{
	WORD                                wActionId;
	WCHAR                               wszActionText[23];
	DWORD                               dwFlags;
} XCUSTOMACTION;

#define XSSUI_FLAGS_LOCALSIGNINONLY                 0x00000001
#define XSSUI_FLAGS_SHOWONLYONLINEENABLED           0x00000002

#define X_STATS_COLUMN_SKILL_SKILL              61
#define X_STATS_COLUMN_SKILL_GAMESPLAYED        62
#define X_STATS_COLUMN_SKILL_MU                 63
#define X_STATS_COLUMN_SKILL_SIGMA              64

#define X_STATS_SKILL_SKILL_DEFAULT             1
#define X_STATS_SKILL_MU_DEFAULT                3.0
#define X_STATS_SKILL_SIGMA_DEFAULT             1.0

#define X_STATS_COLUMN_ATTACHMENT_SIZE          ((WORD)0xFFFA)


#define XUSER_STATS_ATTRS_IN_SPEC       64

typedef struct _XUSER_STATS_COLUMN
{
	WORD                                wColumnId;
	XUSER_DATA                          Value;
} XUSER_STATS_COLUMN, *PXUSER_STATS_COLUMN;

typedef struct _XUSER_STATS_ROW
{
	XUID                                xuid;
	DWORD                               dwRank;
	LONGLONG                            i64Rating;
	CHAR                                szGamertag[XUSER_NAME_SIZE];
	DWORD                               dwNumColumns;
	PXUSER_STATS_COLUMN                 pColumns;
} XUSER_STATS_ROW, *PXUSER_STATS_ROW;

typedef struct _XUSER_STATS_VIEW
{
	DWORD                               dwViewId;
	DWORD                               dwTotalViewRows;
	DWORD                               dwNumRows;
	PXUSER_STATS_ROW                    pRows;
} XUSER_STATS_VIEW, *PXUSER_STATS_VIEW;

typedef struct _XUSER_STATS_READ_RESULTS
{
	DWORD                               dwNumViews;
	PXUSER_STATS_VIEW                   pViews;
} XUSER_STATS_READ_RESULTS, *PXUSER_STATS_READ_RESULTS;

typedef struct _XUSER_STATS_SPEC
{
	DWORD                               dwViewId;
	DWORD                               dwNumColumnIds;
	WORD                                rgwColumnIds[XUSER_STATS_ATTRS_IN_SPEC];
} XUSER_STATS_SPEC, *PXUSER_STATS_SPEC;


#define X_CONTEXT_GAME_TYPE_RANKED      0
#define X_CONTEXT_GAME_TYPE_STANDARD    1

#define X_CONTEXT_PRESENCE              XCONTEXTID(1, 0x1)
#define X_CONTEXT_GAME_TYPE             XCONTEXTID(1, 0xA)
#define X_CONTEXT_GAME_MODE             XCONTEXTID(1, 0xB)
#define X_CONTEXT_SESSION_JOINABLE      XCONTEXTID(1, 0xC)

#define X_PROPERTY_RANK                 XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0x1)
#define X_PROPERTY_GAMERNAME            XPROPERTYID(1, XUSER_DATA_TYPE_UNICODE, 0x2)
#define X_PROPERTY_SESSION_ID           XPROPERTYID(1, XUSER_DATA_TYPE_INT64,   0x3)

#define X_PROPERTY_TYPE_MASK            0xF0000000
#define X_PROPERTY_SCOPE_MASK           0x00008000
#define X_PROPERTY_ID_MASK              0x00007FFF


#define XPROPERTYID(global, type, id)   ((global ? X_PROPERTY_SCOPE_MASK : 0) | ((type << 28) & X_PROPERTY_TYPE_MASK) | (id & X_PROPERTY_ID_MASK))
#define XCONTEXTID(global, id)          XPROPERTYID(global, XUSER_DATA_TYPE_CONTEXT, id)
#define XPROPERTYTYPEFROMID(id)         ((id >> 28) & 0xf)
#define XISSYSTEMPROPERTY(id)           (id & X_PROPERTY_SCOPE_MASK)





//Achievements

#define XACHIEVEMENT_MAX_COUNT 200

#define XACHIEVEMENT_TYPE_COMPLETION            1
#define XACHIEVEMENT_TYPE_LEVELING              2
#define XACHIEVEMENT_TYPE_UNLOCK                3
#define XACHIEVEMENT_TYPE_EVENT                 4
#define XACHIEVEMENT_TYPE_TOURNAMENT            5
#define XACHIEVEMENT_TYPE_CHECKPOINT            6
#define XACHIEVEMENT_TYPE_OTHER                 7

#define XACHIEVEMENT_DETAILS_ALL                0xFFFFFFFF
#define XACHIEVEMENT_DETAILS_LABEL              0x00000001
#define XACHIEVEMENT_DETAILS_DESCRIPTION        0x00000002
#define XACHIEVEMENT_DETAILS_UNACHIEVED         0x00000004
#define XACHIEVEMENT_DETAILS_TFC                0x00000020

#define XACHIEVEMENT_DETAILS_MASK_TYPE          0x00000007
#define XACHIEVEMENT_DETAILS_SHOWUNACHIEVED     0x00000008
#define XACHIEVEMENT_DETAILS_ACHIEVED_ONLINE    0x00010000
#define XACHIEVEMENT_DETAILS_ACHIEVED           0x00020000

#define AchievementType(dwFlags)           (dwFlags & XACHIEVEMENT_DETAILS_MASK_TYPE)
#define AchievementShowUnachieved(dwFlags) (dwFlags & XACHIEVEMENT_DETAILS_SHOWUNACHIEVED ? TRUE : FALSE)
#define AchievementEarnedOnline(dwFlags)   (dwFlags & XACHIEVEMENT_DETAILS_ACHIEVED_ONLINE ? TRUE : FALSE)
#define AchievementEarned(dwFlags)         (dwFlags & XACHIEVEMENT_DETAILS_ACHIEVED ? TRUE : FALSE)

#define XACHIEVEMENT_MAX_LABEL_LENGTH   33
#define XACHIEVEMENT_MAX_DESC_LENGTH    101
#define XACHIEVEMENT_MAX_UNACH_LENGTH   101

#define XACHIEVEMENT_SIZE_BASE          (sizeof(XACHIEVEMENT_DETAILS))
#define XACHIEVEMENT_SIZE_STRINGS       (sizeof(WCHAR) * (XACHIEVEMENT_MAX_LABEL_LENGTH  + XACHIEVEMENT_MAX_DESC_LENGTH + XACHIEVEMENT_MAX_UNACH_LENGTH))
#define XACHIEVEMENT_SIZE_FULL          (XACHIEVEMENT_SIZE_BASE + XACHIEVEMENT_SIZE_STRINGS)

#define XACHIEVEMENT_INVALID_ID         ((DWORD)0xFFFFFFFF)

typedef struct
{
	DWORD                               dwId;
	LPWSTR                              pwszLabel;
	LPWSTR                              pwszDescription;
	LPWSTR                              pwszUnachieved;
	DWORD                               dwImageId;
	DWORD                               dwCred;
	FILETIME                            ftAchieved;
	DWORD                               dwFlags;
} XACHIEVEMENT_DETAILS, *PXACHIEVEMENT_DETAILS;



#define XSTRING_MAX_LENGTH 512
#define XSTRING_MAX_STRINGS 10


#define XFRIENDS_MAX_C_RESULT 100


typedef struct {
	XUID xuidInvitee;
	XUID xuidInviter;
	DWORD dwTitleID;
	XSESSION_INFO hostInfo;
	BOOL fFromGameInvite;
} XINVITE_INFO, *PXINVITE_INFO;


#define X_PROPERTY_ATTACHMENT_SIZE                  XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0x11)

#define X_PROPERTY_PLAYER_PARTIAL_PLAY_PERCENTAGE   XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0xC)
#define X_PROPERTY_PLAYER_SKILL_UPDATE_WEIGHTING_FACTOR XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0xD)
#define X_PROPERTY_SESSION_SKILL_BETA               XPROPERTYID(1, XUSER_DATA_TYPE_DOUBLE,  0xE)
#define X_PROPERTY_SESSION_SKILL_TAU                XPROPERTYID(1, XUSER_DATA_TYPE_DOUBLE,  0xF)
#define X_PROPERTY_SESSION_SKILL_DRAW_PROBABILITY   XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0x10)

#define X_PROPERTY_RELATIVE_SCORE                   XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0xA)
#define X_PROPERTY_SESSION_TEAM                     XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0xB)

#define X_PROPERTY_GAMER_ZONE           XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0x101)
#define X_PROPERTY_GAMER_COUNTRY        XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0x102)
#define X_PROPERTY_GAMER_LANGUAGE       XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   0x103)
#define X_PROPERTY_GAMER_RATING         XPROPERTYID(1, XUSER_DATA_TYPE_FLOAT,   0x104)
#define X_PROPERTY_GAMER_MU             XPROPERTYID(1, XUSER_DATA_TYPE_DOUBLE,  0x105)
#define X_PROPERTY_GAMER_SIGMA          XPROPERTYID(1, XUSER_DATA_TYPE_DOUBLE,  0x106)
#define X_PROPERTY_GAMER_PUID           XPROPERTYID(1, XUSER_DATA_TYPE_INT64,   0x107)
#define X_PROPERTY_AFFILIATE_SCORE      XPROPERTYID(1, XUSER_DATA_TYPE_INT64,   0x108)
#define X_PROPERTY_GAMER_HOSTNAME       XPROPERTYID(1, XUSER_DATA_TYPE_UNICODE, 0x109)

#define XUSER_PROPERTY_RANK		0x10008001
#define XUSER_PROPERTY_GAMERNAME		0x40008002
#define XUSER_PROPERTY_GAMERZONE		0x10008101
#define XUSER_PROPERTY_GAMERCOUNTRY		0x10008102
#define XUSER_PROPERTY_LANGUAGE		0x10008103
#define XUSER_PROPERTY_GAMERRATING		0x50008104
#define XUSER_PROPERTY_GAMERMU		0x30008105
#define XUSER_PROPERTY_GAMERSIGMA		0x30008106
#define XUSER_PROPERTY_GAMERPUID		0x20008107
#define XUSER_PROPERTY_AFFILIATEVALUE		0x20008108
#define XUSER_PROPERTY_GAMERHOSTNAME		0x40008109
#define XUSER_PROPERTY_PLATFORMTYPE		0x10008201
#define XUSER_PROPERTY_PLATFORMLOCK		0x10008202
#define XUSER_PROPERTY_MP_Players_Playing		0x10000003
// Find more values from here: https://github.com/ValveSoftware/steam-for-linux/issues/5062

#define XUSER_PROPERTY_GAMETYPE_NAME		0x40008228
#define XUSER_PROPERTY_GAMETYPE_NAME_2		0x4000822B
#define XUSER_PROPERTY_SERVER_NAME			0x40008230
#define XUSER_PROPERTY_SERVER_DESC			0x40008225
#define XUSER_PROPERTY_MAP_ID				0x10008207
#define XUSER_PROPERTY_MAP_ID_2				0x1000820A
#define XUSER_PROPERTY_MAP_NAME				0x40008226
#define XUSER_PROPERTY_MAP_NAME_2			0x40008229
#define XUSER_PROPERTY_GAMETYPE_ID			0x10008209
#define XUSER_PROPERTY_GAME_STATUS			0x10008211
#define XUSER_PROPERTY_VERSION_1			0x1000820E
#define XUSER_PROPERTY_VERSION_2			0x1000820F
#define XUSER_PROPERTY_UNKNOWN_INT64		0x2000821B
#define XUSER_PROPERTY_MAP_HASH_1			0x40008227
#define XUSER_PROPERTY_MAP_HASH_2			0x4000822A
#define XUSER_PROPERTY_USER_INT				0x10008202
#define XUSER_PROPERTY_UNKNOWN_INT32_1		0x10008208
#define XUSER_PROPERTY_UNKNOWN_INT32_2		0x1000820B
#define XUSER_PROPERTY_UNKNOWN_INT32_3		0x1000820C
#define XUSER_PROPERTY_UNKNOWN_INT32_4		0x1000820D
#define XUSER_PROPERTY_PARTY_PRIVACY		0x10008210
#define XUSER_PROPERTY_UNKNOWN_INT32_6		0x10008212
#define XUSER_PROPERTY_UNKNOWN_INT32_7		0x10008213
#define XUSER_PROPERTY_USERNAME_2			0x4000822C

#define XLOCATOR_DEDICATEDSERVER_PROPERTY_START     0x200

// These properties are used for search only.
// The search result header should already contains the information, and the query should not request these properties again.
#define X_PROPERTY_DEDICATEDSERVER_IDENTITY             XPROPERTYID(1, XUSER_DATA_TYPE_INT64,  XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_ServerIdentity)   // server id. supports '=' operator onl$
#define X_PROPERTY_DEDICATEDSERVER_TYPE                 XPROPERTYID(1, XUSER_DATA_TYPE_INT32,  XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_ServerType)
#define X_PROPERTY_DEDICATEDSERVER_MAX_PUBLIC_SLOTS     XPROPERTYID(1, XUSER_DATA_TYPE_INT32,  XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_MaxPublicSlots)
#define X_PROPERTY_DEDICATEDSERVER_MAX_PRIVATE_SLOTS    XPROPERTYID(1, XUSER_DATA_TYPE_INT32,  XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_MaxPrivateSlots)
#define X_PROPERTY_DEDICATEDSERVER_AVAILABLE_PUBLIC_SLOTS   XPROPERTYID(1, XUSER_DATA_TYPE_INT32,  XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_AvailablePublicSlots)
#define X_PROPERTY_DEDICATEDSERVER_AVAILABLE_PRIVATE_SLOTS  XPROPERTYID(1, XUSER_DATA_TYPE_INT32,  XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_AvailablePrivateSlots)
#define X_PROPERTY_DEDICATEDSERVER_FILLED_PUBLIC_SLOTS      XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_FilledPublicSlots)
#define X_PROPERTY_DEDICATEDSERVER_FILLED_PRIVATE_SLOTS     XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_FilledPrivateSlots)


// the following properties only support XTS_FILTER_COMPARE_OPERATOR_Equals operator
#define X_PROPERTY_DEDICATEDSERVER_OWNER_XUID           XPROPERTYID(1, XUSER_DATA_TYPE_INT64,   XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_OwnerXuid)
#define X_PROPERTY_DEDICATEDSERVER_OWNER_GAMERTAG       XPROPERTYID(1, XUSER_DATA_TYPE_UNICODE,   XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_OwnerGamerTag)
#define X_PROPERTY_DEDICATEDSERVER_REGIONID             XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_RegionID)
#define X_PROPERTY_DEDICATEDSERVER_LANGUAGEID           XPROPERTYID(1, XUSER_DATA_TYPE_INT32,   XLOCATOR_DEDICATEDSERVER_PROPERTY_START + XTS_SEARCH_FIELD_LanguageID)

// Predefined dedicated server types
#define XLOCATOR_SERVERTYPE_PUBLIC          0   // dedicated server is for all players.
#define XLOCATOR_SERVERTYPE_GOLD_ONLY       1   // dedicated server is for Gold players only.
#define XLOCATOR_SERVERTYPE_PEER_HOSTED     2   // dedicated server is a peer-hosted game server.
#define XLOCATOR_SERVERTYPE_PEER_HOSTED_GOLD_ONLY   3   // dedicated server is a peer-hosted game server (gold only).
#define XLOCATOR_SERVICESTATUS_PROPERTY_START     0x100

typedef struct _XLOCATOR_SEARCHRESULT {
	ULONGLONG serverID;                     // the ID of dedicated server
	DWORD dwServerType;                     // see XLOCATOR_SERVERTYPE_PUBLIC, etc
	XNADDR serverAddress;                   // the address dedicated server
	XNKID xnkid;
	XNKEY xnkey;
	DWORD dwMaxPublicSlots;
	DWORD dwMaxPrivateSlots;
	DWORD dwFilledPublicSlots;
	DWORD dwFilledPrivateSlots;
	DWORD cProperties;                      // number of custom properties.
	PXUSER_PROPERTY pProperties;            // an array of custom properties.

} XLOCATOR_SEARCHRESULT, *PXLOCATOR_SEARCHRESULT;



#define XLMGRCREDS_FLAG_SAVE 1
#define XLMGRCREDS_FLAG_DELETE 2

#define XLSIGNIN_FLAG_SAVECREDS 1
#define XLSIGNIN_FLAG_ALLOWTITLEUPDATES 2
#define XLSIGNIN_FLAG_ALLOWSYSTEMUPDATES 4

//TODO unknown number (guessed)
#define XLIVE_LICENSE_ID_SIZE 8

#define XLIVE_LICENSE_INFO_VERSION 1

typedef struct _XLIVE_CONTENT_RETRIEVAL_INFO {
	DWORD dwContentAPIVersion;
	DWORD dwRetrievalMask;
	DWORD dwUserIndex;
	XUID xuidUser;
	DWORD dwTitleID;
	DWORD dwContentType;
	BYTE abContentID[XLIVE_LICENSE_ID_SIZE];
} XLIVE_CONTENT_RETRIEVAL_INFO, *PXLIVE_CONTENT_RETRIEVAL_INFO;



typedef enum _XLIVE_DEBUG_LEVEL
{
	XLIVE_DEBUG_LEVEL_OFF = 0, // No debug output.
	XLIVE_DEBUG_LEVEL_ERROR, // Error only debug output.
	XLIVE_DEBUG_LEVEL_WARNING, // Warning and error debug output.
	XLIVE_DEBUG_LEVEL_INFO, // Information, warning and error debug output.
	XLIVE_DEBUG_LEVEL_DEFAULT // Default level of debug output, set as the current registry value.
} XLIVE_DEBUG_LEVEL;


#pragma pack(pop) // Return to original alignment setting.

#endif