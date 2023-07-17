#pragma once
#include <map>
#include <vector>

extern BOOL xlive_debug_pause;

// Official values.
#define XUSER_MAX_COUNT 4
#define XUSER_INDEX_NONE 0x000000FE
// XLLN version.
#define XLIVE_LOCAL_USER_COUNT 4
#define XLIVE_LOCAL_USER_INVALID 0xFFFFFFFF

extern BOOL xlive_users_auto_login[XLIVE_LOCAL_USER_COUNT];
extern BOOL xlive_users_live_enabled[XLIVE_LOCAL_USER_COUNT];
extern char xlive_users_username[XLIVE_LOCAL_USER_COUNT][XUSER_NAME_SIZE];
extern XUSER_SIGNIN_INFO* xlive_users_info[XLIVE_LOCAL_USER_COUNT];

void Check_Overlapped(XOVERLAPPED *pXOverlapped);

struct EligibleAdapter {
	// Can be null.
	char* name;
	// Can be null.
	wchar_t* description;
	ULONG unicastHAddr;
	ULONG unicastHMask;
	ULONG hBroadcast;
	UINT64 minLinkSpeed;
	BOOL hasDnsServer;
};

typedef struct {
	uint32_t joinRequestSignature = 0;
	__time64_t timeoutAt = 0;
	uint32_t localPlayerId = 0;
	uint32_t remoteInstanceId = 0;
	XUID remoteXuid = 0;
	uint32_t remoteTitleId = 0;
	XNKID remoteSessionId;
	XNKEY remoteKeyExchangeKey;
} DIRECT_IP_CONNECT;

extern DIRECT_IP_CONNECT xlln_direct_ip_connect;

extern CRITICAL_SECTION xlive_critsec_xfriends_enumerators;
extern std::map<HANDLE, std::vector<uint32_t>> xlive_xfriends_enumerators;

extern uint32_t xlive_title_id;
extern uint32_t xlive_title_version;
extern CRITICAL_SECTION xlive_critsec_network_adapter;
extern char* xlive_init_preferred_network_adapter_name;
extern char* xlive_config_preferred_network_adapter_name;
extern bool xlive_ignore_title_network_adapter;
extern EligibleAdapter *xlive_network_adapter;
extern std::vector<EligibleAdapter*> xlive_eligible_network_adapters;
extern BOOL xlive_online_initialized;

extern CRITICAL_SECTION xlive_critsec_title_server_enumerators;

extern bool xlive_auto_login_on_xliveinitialize;

INT RefreshNetworkAdapters();
void XllnDirectIpConnectCancel();
void XllnDirectIpConnectTo(uint32_t local_player_id, SOCKADDR_STORAGE *remote_sockaddr, const char *remote_password);
