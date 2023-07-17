#pragma once
#include <stdint.h>
#include <map>

#define XLLN_SHOW_HIDE 0
#define XLLN_SHOW_HOME 1
#define XLLN_SHOW_LOGIN 2

#define MYMENU_EXIT					(WM_APP + 101)
#define MYMENU_ABOUT				(WM_APP + 102)
#define MYMENU_ALWAYSTOP			(WM_APP + 103)
#define MYMENU_LOGIN1				(WM_APP + 110)
#define MYMENU_LOGIN2				(WM_APP + 111)
#define MYMENU_LOGIN3				(WM_APP + 112)
#define MYMENU_LOGIN4				(WM_APP + 113)
#define MYMENU_DEBUG_SOCKETS		(WM_APP + 114)
#define MYMENU_DEBUG_CONNECTIONS	(WM_APP + 115)
#define MYWINDOW_BTN_LOGIN			(WM_APP + 120)
#define MYWINDOW_TBX_USERNAME		(WM_APP + 121)
#define MYWINDOW_CHK_LIVEENABLE		(WM_APP + 123)
#define MYWINDOW_CHK_AUTOLOGIN		(WM_APP + 124)
#define MYWINDOW_BTN_LOGOUT			(WM_APP + 125)
#define MYWINDOW_BTN_TEST			(WM_APP + 126)
#define MYWINDOW_TBX_DIRECT_IP_CONNECT_IP_PORT		(WM_APP + 127)
#define MYWINDOW_TBX_DIRECT_IP_CONNECT_PASSWORD		(WM_APP + 128)
#define MYWINDOW_BTN_DIRECT_IP_CONNECT				(WM_APP + 129)
#define MYWINDOW_TBX_BROADCAST		(WM_APP + 130)
#define MYWINDOW_LST_SOCKETS		(WM_APP + 140)
#define MYWINDOW_TRE_CONNECTIONS	(WM_APP + 141)
#define MYWINDOW_BTN_REFRESH_CONNS	(WM_APP + 142)
#define MYWINDOW_BTN_DELETE_CONNS	(WM_APP + 143)
#define MYWINDOW_TBX_TEST			(WM_APP + 150)
#define MYWINDOW_CHK_DBG_CTX_XLIVE		(WM_APP + 151)
#define MYWINDOW_CHK_DBG_CTX_XLLN		(WM_APP + 152)
#define MYWINDOW_CHK_DBG_CTX_XLLN_MOD	(WM_APP + 153)
#define MYWINDOW_CHK_DBG_CTX_OTHER		(WM_APP + 154)
#define MYWINDOW_CHK_DBG_LVL_TRACE		(WM_APP + 155)
#define MYWINDOW_CHK_DBG_LVL_DEBUG		(WM_APP + 156)
#define MYWINDOW_CHK_DBG_LVL_INFO		(WM_APP + 157)
#define MYWINDOW_CHK_DBG_LVL_WARN		(WM_APP + 158)
#define MYWINDOW_CHK_DBG_LVL_ERROR		(WM_APP + 159)
#define MYWINDOW_CHK_DBG_LVL_FATAL		(WM_APP + 160)
#define MYMENU_NETWORK_ADAPTER_REFRESH			(WM_APP + 166)
#define MYMENU_NETWORK_ADAPTER_AUTO_SELECT		(WM_APP + 167)
#define MYMENU_NETWORK_ADAPTER_IGNORE_TITLE		(WM_APP + 168)
#define MYMENU_NETWORK_ADAPTER_SEPARATOR		(WM_APP + 169)
#define MYMENU_NETWORK_ADAPTERS					(WM_APP + 170)

namespace XLLNModifyPropertyTypes {
	const char* const TypeNames[] {
		"UNKNOWN",
		"FPS_LIMIT",
		"LiveOverLan_BROADCAST_HANDLER",
		"RECVFROM_CUSTOM_HANDLER_REGISTER",
		"RECVFROM_CUSTOM_HANDLER_UNREGISTER",
		"BASE_PORT",
		"GUIDE_UI_HANDLER",
	};
	typedef enum : BYTE {
		tUNKNOWN = 0,
		tFPS_LIMIT,
		tLiveOverLan_BROADCAST_HANDLER,
		tRECVFROM_CUSTOM_HANDLER_REGISTER,
		tRECVFROM_CUSTOM_HANDLER_UNREGISTER,
		tBASE_PORT,
		tGUIDE_UI_HANDLER,
	} TYPE;

#pragma pack(push, 1) // Save then set byte alignment setting.

	typedef struct {
		char *Identifier;
		uint32_t *FuncPtr;
	} RECVFROM_CUSTOM_HANDLER_REGISTER;

#pragma pack(pop) // Return to original alignment setting.

}

struct BASE_PORT_OFFSET_MAPPING {
	int8_t priority = 0;
	uint8_t offset = 0;
	uint16_t original = 0;
};

extern HINSTANCE xlln_hModule;
extern uint32_t xlln_local_instance_index;
extern HMENU hMenu_network_adapters;
extern uint32_t xlln_login_player;
extern uint32_t xlln_login_player_h[];
extern BOOL xlln_debug;
extern char *broadcastAddrInput;
extern char *xlln_direct_ip_connect_password;
extern char *xlln_direct_ip_connect_ip_port;
extern CRITICAL_SECTION xlln_critsec_base_port_offset_mappings;
extern std::map<uint8_t, BASE_PORT_OFFSET_MAPPING*> xlln_base_port_mappings_offset;
extern std::map<uint16_t, BASE_PORT_OFFSET_MAPPING*> xlln_base_port_mappings_original;

// #41140
typedef uint32_t(WINAPI *tXLLNLogin)(uint32_t dwUserIndex, BOOL bLiveEnabled, uint32_t dwUserId, const CHAR *szUsername);
// #41141
typedef uint32_t(WINAPI *tXLLNLogout)(uint32_t dwUserIndex);
// #41142
typedef uint32_t(WINAPI *tXLLNModifyProperty)(XLLNModifyPropertyTypes::TYPE propertyId, uint32_t *newValue, uint32_t *oldValue);

uint32_t WINAPI XLLNLogin(uint32_t dwUserIndex, BOOL bLiveEnabled, uint32_t dwUserId, const CHAR *szUsername);
uint32_t WINAPI XLLNLogout(uint32_t dwUserIndex);
void InitCriticalSections();
void UninitCriticalSections();
bool InitXLLN(HMODULE hModule);
bool UninitXLLN();
INT WINAPI XSocketRecvFromCustomHelper(INT result, SOCKET s, char *buf, int len, int flags, sockaddr *from, int *fromlen);
void ParseBroadcastAddrInput(char *jlbuffer);
