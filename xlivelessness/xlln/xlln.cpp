#include <winsock2.h>
#include "windows.h"
#include "xlln.hpp"
#include "debug-text.hpp"
#include "xlln-config.hpp"
#include "wnd-main.hpp"
#include "wnd-sockets.hpp"
#include "wnd-connections.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"
#include "../utils/util-checksum.hpp"
#include "../xlive/xdefs.hpp"
#include "../xlive/xlive.hpp"
#include "../xlive/xlocator.hpp"
#include "../xlive/xrender.hpp"
#include "../xlive/net-entity.hpp"
#include "../xlive/xsocket.hpp"
#include "../xlive/xwsa.hpp"
#include "../xlive/xuser.hpp"
#include "../xlive/xsession.hpp"
#include "../xlive/xnet.hpp"
#include "../xlive/xnetqos.hpp"
#include "../xlive/xcustom.hpp"
#include "../xlive/xpresence.hpp"
#include "../xlive/xmarketplace.hpp"
#include "../xlive/xcontent.hpp"
#include "../xlive/xnotify.hpp"
#include <ws2tcpip.h>
#include "../third-party/rapidxml/rapidxml.hpp"
#include "../third-party/fantasyname/namegen.h"

HINSTANCE xlln_hModule = NULL;
// 0 - unassigned. Counts from 1.
uint32_t xlln_local_instance_index = 0;
HMENU hMenu_network_adapters = 0;

uint32_t xlln_login_player = 0;
uint32_t xlln_login_player_h[] = { MYMENU_LOGIN1, MYMENU_LOGIN2, MYMENU_LOGIN3, MYMENU_LOGIN4 };

BOOL xlln_debug = FALSE;

static CRITICAL_SECTION xlive_critsec_recvfrom_handler_funcs;
static std::map<DWORD, char*> xlive_recvfrom_handler_funcs;

char *broadcastAddrInput = 0;
char *xlln_direct_ip_connect_password = 0;
char *xlln_direct_ip_connect_ip_port = 0;

CRITICAL_SECTION xlln_critsec_base_port_offset_mappings;
std::map<uint8_t, BASE_PORT_OFFSET_MAPPING*> xlln_base_port_mappings_offset;
std::map<uint16_t, BASE_PORT_OFFSET_MAPPING*> xlln_base_port_mappings_original;

INT WINAPI XSocketRecvFromCustomHelper(INT result, SOCKET s, char *buf, int len, int flags, sockaddr *from, int *fromlen)
{
	EnterCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
	typedef INT(WINAPI *tXliveRecvfromHandler)(INT result, SOCKET s, char *buf, int len, int flags, sockaddr *from, int *fromlen);
	for (std::map<DWORD, char*>::iterator it = xlive_recvfrom_handler_funcs.begin(); it != xlive_recvfrom_handler_funcs.end(); it++) {
		if (result <= 0) {
			break;
		}
		tXliveRecvfromHandler handlerFunc = (tXliveRecvfromHandler)it->first;
		result = handlerFunc(result, s, buf, len, flags, from, fromlen);
	}
	LeaveCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
	if (result != 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "XSocketRecvFromCustomHelper handler returned non zero value 0x%08x."
			, result
		);
	}
	return 0;
}

// #41140
uint32_t WINAPI XLLNLogin(uint32_t dwUserIndex, BOOL bLiveEnabled, uint32_t dwUserId, const CHAR *szUsername)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		return ERROR_NO_SUCH_USER;
	}
	if (szUsername && (!*szUsername || strlen(szUsername) > XUSER_MAX_NAME_LENGTH)) {
		return ERROR_INVALID_ACCOUNT_NAME;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_NotSignedIn) {
		return ERROR_ALREADY_ASSIGNED;
	}
	
	if (szUsername) {
		memcpy(xlive_users_info[dwUserIndex]->szUserName, szUsername, XUSER_NAME_SIZE);
		xlive_users_info[dwUserIndex]->szUserName[XUSER_NAME_SIZE] = 0;
	}
	else {
		unsigned long seedNamegen = rand();
		int resultNamegen;
		do {
			resultNamegen = namegen(xlive_users_info[dwUserIndex]->szUserName, XUSER_NAME_SIZE, "<!DdM|!DdV|!Dd|!m|!BVC|!BdC !DdM|!DdV|!Dd|!m|!BVC|!BdC>|<!DdM|!DdV|!Dd|!m|!BVC|!BdC>", &seedNamegen);
		} while (resultNamegen == NAMEGEN_TRUNCATED || xlive_users_info[dwUserIndex]->szUserName[0] == 0);
	}
	
	if (!dwUserId) {
		// Not including the null terminator.
		uint32_t usernameSize = strlen(xlive_users_info[dwUserIndex]->szUserName);
		dwUserId = crc32buf((uint8_t*)xlive_users_info[dwUserIndex]->szUserName, usernameSize);
	}
	
	xlive_users_info[dwUserIndex]->UserSigninState = bLiveEnabled ? eXUserSigninState_SignedInToLive : eXUserSigninState_SignedInLocally;
	//xlive_users_info[dwUserIndex]->xuid = 0xE000007300000000 + dwUserId;
	xlive_users_info[dwUserIndex]->xuid = (bLiveEnabled ? XUID_LIVE_ENABLED_FLAG : 0xE000000000000000) + dwUserId;
	xlive_users_info[dwUserIndex]->dwInfoFlags = bLiveEnabled ? XUSER_INFO_FLAG_LIVE_ENABLED : 0;
	
	xlive_users_auto_login[dwUserIndex] = FALSE;
	XLiveNotifyAddEvent(XN_SYS_SIGNINCHANGED, 1 << dwUserIndex);
	
	bool othersSignedIntoLive = false;
	for (int i = 0; i < XLIVE_LOCAL_USER_COUNT; i++) {
		if (i != dwUserIndex && xlive_users_info[i]->UserSigninState == eXUserSigninState_SignedInToLive) {
			othersSignedIntoLive = true;
			break;
		}
	}
	if (!othersSignedIntoLive && xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_SignedInToLive) {
		XLiveNotifyAddEvent(XN_LIVE_CONNECTIONCHANGED, XONLINE_S_LOGON_CONNECTION_ESTABLISHED);
	}
	
	if (dwUserIndex == xlln_login_player) {
		SetDlgItemTextA(xlln_window_hwnd, MYWINDOW_TBX_USERNAME, xlive_users_info[dwUserIndex]->szUserName);
		CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_LIVEENABLE, bLiveEnabled ? BST_CHECKED : BST_UNCHECKED);
		
		CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_AUTOLOGIN, BST_UNCHECKED);
		
		BOOL checked = TRUE;//GetMenuState(xlln_window_hMenu, xlln_login_player_h[xlln_login_player], 0) != MF_CHECKED;
		//CheckMenuItem(xlln_window_hMenu, xlln_login_player_h[xlln_login_player], checked ? MF_CHECKED : MF_UNCHECKED);
		
		ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGIN), checked ? SW_HIDE : SW_SHOWNORMAL);
		ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGOUT), checked ? SW_SHOWNORMAL : SW_HIDE);
	}
	
	return ERROR_SUCCESS;
}

// #41141
uint32_t WINAPI XLLNLogout(uint32_t dwUserIndex)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	
	bool wasSignedIntoLive = xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_SignedInToLive;
	xlive_users_info[dwUserIndex]->UserSigninState = eXUserSigninState_NotSignedIn;
	XLiveNotifyAddEvent(XN_SYS_SIGNINCHANGED, 1 << dwUserIndex);
	
	bool othersSignedIntoLive = false;
	for (int i = 0; i < XLIVE_LOCAL_USER_COUNT; i++) {
		if (i != dwUserIndex && xlive_users_info[i]->UserSigninState == eXUserSigninState_SignedInToLive) {
			othersSignedIntoLive = true;
			break;
		}
	}
	if (!othersSignedIntoLive && wasSignedIntoLive) {
		XLiveNotifyAddEvent(XN_LIVE_CONNECTIONCHANGED, XONLINE_S_LOGON_DISCONNECTED);
	}
	
	if (dwUserIndex == xlln_login_player) {
		BOOL checked = FALSE;//GetMenuState(xlln_window_hMenu, xlln_login_player_h[xlln_login_player], 0) != MF_CHECKED;
		//CheckMenuItem(xlln_window_hMenu, xlln_login_player_h[xlln_login_player], checked ? MF_CHECKED : MF_UNCHECKED);
		
		ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGIN), checked ? SW_HIDE : SW_SHOWNORMAL);
		ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGOUT), checked ? SW_SHOWNORMAL : SW_HIDE);
	}
	
	return ERROR_SUCCESS;
}

// #41142
uint32_t WINAPI XLLNModifyProperty(XLLNModifyPropertyTypes::TYPE propertyId, uint32_t *new_value, uint32_t *old_value)
{
	TRACE_FX();
	switch (propertyId) {
		case XLLNModifyPropertyTypes::tUNKNOWN: {
			return ERROR_INVALID_PARAMETER;
		}
		case XLLNModifyPropertyTypes::tFPS_LIMIT: {
			if (old_value && !new_value) {
				*old_value = xlive_fps_limit;
			}
			else if (new_value) {
				uint32_t old_value2 = SetFPSLimit(*new_value);
				if (old_value) {
					*old_value = old_value2;
				}
			}
			else {
				return ERROR_NOT_SUPPORTED;
			}
			return ERROR_SUCCESS;
		}
		case XLLNModifyPropertyTypes::tBASE_PORT: {
			if (old_value && !new_value) {
				*(uint16_t*)old_value = xlive_base_port;
			}
			else if (new_value) {
				if (old_value) {
					*(uint16_t*)old_value = xlive_base_port;
				}
				xlive_base_port = *(uint16_t*)new_value;
			}
			else {
				return ERROR_NOT_SUPPORTED;
			}
			return ERROR_SUCCESS;
		}
		case XLLNModifyPropertyTypes::tRECVFROM_CUSTOM_HANDLER_REGISTER: {
			// TODO
			return ERROR_NOT_SUPPORTED;
			XLLNModifyPropertyTypes::RECVFROM_CUSTOM_HANDLER_REGISTER *handler = (XLLNModifyPropertyTypes::RECVFROM_CUSTOM_HANDLER_REGISTER*)new_value;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
				, "XLLN-Module is registering a recvfrom handler 0x%08x."
				, handler->Identifier
			);
			uint32_t idLen = strlen(handler->Identifier) + 1;
			char *identifier = (char*)malloc(sizeof(char) * idLen);
			strcpy_s(identifier, idLen, handler->Identifier);
			
			if (old_value && new_value) {
				return ERROR_INVALID_PARAMETER;
			}
			EnterCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
			if (old_value) {
				if (!xlive_recvfrom_handler_funcs.count((uint32_t)old_value)) {
					LeaveCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
					return ERROR_NOT_FOUND;
				}
				xlive_recvfrom_handler_funcs.erase((uint32_t)old_value);
			}
			else {
				if (xlive_recvfrom_handler_funcs.count((uint32_t)handler->FuncPtr)) {
					LeaveCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
					return ERROR_ALREADY_REGISTERED;
				}
				xlive_recvfrom_handler_funcs[(uint32_t)handler->FuncPtr] = identifier;
			}
			LeaveCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
			return ERROR_SUCCESS;
		}
		case XLLNModifyPropertyTypes::tRECVFROM_CUSTOM_HANDLER_UNREGISTER: {
			// TODO
			return ERROR_NOT_SUPPORTED;
		}
		case XLLNModifyPropertyTypes::tGUIDE_UI_HANDLER: {
			if (old_value && !new_value) {
				// TODO
				*old_value = 0;
				return ERROR_NOT_SUPPORTED;
			}
			else if (new_value) {
				GUIDE_UI_HANDLER_INFO handlerInfoNew;
				handlerInfoNew.xllnModule = 0;
				handlerInfoNew.guideUiHandler = (tGuideUiHandler)*new_value;
				
				EnterCriticalSection(&xlln_critsec_guide_ui_handlers);
				GUIDE_UI_HANDLER_INFO *handlerInfo = 0;
				for (auto itrGuideUiHandlerInfo = xlln_guide_ui_handlers.begin(); itrGuideUiHandlerInfo != xlln_guide_ui_handlers.end(); itrGuideUiHandlerInfo++) {
					if (itrGuideUiHandlerInfo->xllnModule == handlerInfoNew.xllnModule) {
						if (!handlerInfoNew.guideUiHandler) {
							xlln_guide_ui_handlers.erase(itrGuideUiHandlerInfo);
							break;
						}
						handlerInfo = &(*itrGuideUiHandlerInfo);
						break;
					}
				}
				if (old_value) {
					// TODO
					*old_value = 0;
				}
				if (handlerInfo) {
					*handlerInfo = handlerInfoNew;
				}
				else if (handlerInfoNew.guideUiHandler) {
					xlln_guide_ui_handlers.push_back(handlerInfoNew);
				}
				LeaveCriticalSection(&xlln_critsec_guide_ui_handlers);
			}
			else {
				return ERROR_NOT_SUPPORTED;
			}
			return ERROR_SUCCESS;
		}
	}
	
	return ERROR_UNKNOWN_PROPERTY;
}

// #41145
uint32_t __stdcall XLLNGetXLLNStoragePath(uint32_t module_handle, uint32_t *result_local_instance_index, wchar_t *result_storage_path_buffer, size_t *result_storage_path_buffer_size)
{
	if (!result_local_instance_index && !result_storage_path_buffer && !result_storage_path_buffer_size) {
		return ERROR_INVALID_PARAMETER;
	}
	if (result_storage_path_buffer && !result_storage_path_buffer_size) {
		return ERROR_INVALID_PARAMETER;
	}
	if (result_storage_path_buffer) {
		result_storage_path_buffer[0] = 0;
	}
	if (result_local_instance_index) {
		*result_local_instance_index = 0;
	}
	if (!module_handle) {
		return ERROR_INVALID_PARAMETER;
	}
	if (result_local_instance_index) {
		*result_local_instance_index = xlln_local_instance_index;
	}
	if (result_storage_path_buffer_size) {
		wchar_t *configPath = PathFromFilename(xlln_file_config_path);
		if (!configPath) {
			*result_storage_path_buffer_size = 0;
			return ERROR_PATH_NOT_FOUND;
		}
		
		size_t configPathLen = wcslen(configPath);
		size_t configPathBufSize = (configPathLen + 1) * sizeof(wchar_t);
		
		if (configPathBufSize > *result_storage_path_buffer_size) {
			*result_storage_path_buffer_size = configPathBufSize;
			delete[] configPath;
			return ERROR_INSUFFICIENT_BUFFER;
		}
		if (*result_storage_path_buffer_size == 0) {
			*result_storage_path_buffer_size = configPathBufSize;
		}
		if (result_storage_path_buffer) {
			memcpy(result_storage_path_buffer, configPath, configPathBufSize);
		}
		delete[] configPath;
	}
	return ERROR_SUCCESS;
}

// #41146
uint32_t __stdcall XLLNSetBasePortOffsetMapping(uint8_t *port_offsets, uint16_t *port_originals, uint8_t port_mappings_size)
{
	if (!port_offsets) {
		return ERROR_INVALID_PARAMETER;
	}
	if (!port_originals) {
		return ERROR_INVALID_PARAMETER;
	}
	if (port_mappings_size >= 100) {
		// There cannot be more than 0-99 offset mappings.
		return ERROR_INVALID_PARAMETER;
	}
	
	EnterCriticalSection(&xlln_critsec_base_port_offset_mappings);
	
	for (uint8_t i = 0; i < port_mappings_size; i++) {
		uint8_t &portOffset = port_offsets[i];
		uint16_t &portOriginal = port_originals[i];
		
		BASE_PORT_OFFSET_MAPPING *mappingOffset = 0;
		if (xlln_base_port_mappings_offset.count(portOffset)) {
			mappingOffset = xlln_base_port_mappings_offset[portOffset];
		}
		else {
			mappingOffset = xlln_base_port_mappings_offset[portOffset] = new BASE_PORT_OFFSET_MAPPING;
		}
		
		if (xlln_base_port_mappings_original.count(portOriginal)) {
			BASE_PORT_OFFSET_MAPPING *mappingOriginal = xlln_base_port_mappings_original[portOriginal];
			if (mappingOffset != mappingOriginal) {
				xlln_base_port_mappings_offset.erase(mappingOriginal->offset);
				delete mappingOriginal;
			}
		}
		
		xlln_base_port_mappings_original[portOriginal] = mappingOffset;
		
		mappingOffset->offset = portOffset;
		mappingOffset->original = portOriginal;
	}
	
	LeaveCriticalSection(&xlln_critsec_base_port_offset_mappings);
	
	return ERROR_SUCCESS;
}

static bool IsBroadcastAddress(const SOCKADDR_STORAGE *sockaddr)
{
	if (sockaddr->ss_family != AF_INET) {
		return false;
	}
	
	const uint32_t ipv4NBO = ((struct sockaddr_in*)sockaddr)->sin_addr.s_addr;
	const uint32_t ipv4HBO = ntohl(ipv4NBO);
	
	if (ipv4HBO == INADDR_BROADCAST) {
		return true;
	}
	if (ipv4HBO == INADDR_ANY) {
		return true;
	}
	
	for (EligibleAdapter* ea : xlive_eligible_network_adapters) {
		if (ea->hBroadcast == ipv4HBO) {
			return true;
		}
	}
	
	return false;
}

/// Mutates the input buffer.
void ParseBroadcastAddrInput(char *jlbuffer)
{
	EnterCriticalSection(&xlive_critsec_broadcast_addresses);
	xlive_broadcast_addresses.clear();
	XLLNBroadcastEntity::BROADCAST_ENTITY broadcastEntity;
	char *current = jlbuffer;
	while (1) {
		char *comma = strchr(current, ',');
		if (comma) {
			comma[0] = 0;
		}
		
		char *colon = strrchr(current, ':');
		if (colon) {
			colon[0] = 0;
			
			if (current[0] == '[') {
				current = &current[1];
				if (colon[-1] == ']') {
					colon[-1] = 0;
				}
			}
			
			uint16_t portHBO = 0;
			if (sscanf_s(&colon[1], "%hu", &portHBO) == 1) {
				if (current[0] == 0) {
					broadcastEntity.lastComm = 0;
					broadcastEntity.entityType = XLLNBroadcastEntity::TYPE::tBROADCAST_ADDR;
					memset(&broadcastEntity.sockaddr, 0, sizeof(broadcastEntity.sockaddr));
					(*(struct sockaddr_in*)&broadcastEntity.sockaddr).sin_family = AF_INET;
					(*(struct sockaddr_in*)&broadcastEntity.sockaddr).sin_addr.s_addr = htonl(INADDR_BROADCAST);
					(*(struct sockaddr_in*)&broadcastEntity.sockaddr).sin_port = htons(portHBO);
					xlive_broadcast_addresses.push_back(broadcastEntity);
				}
				else {
					addrinfo hints;
					memset(&hints, 0, sizeof(hints));
					
					hints.ai_family = PF_UNSPEC;
					hints.ai_socktype = SOCK_DGRAM;
					hints.ai_protocol = IPPROTO_UDP;
					
					struct in6_addr serveraddr;
					int rc = WS2_32_inet_pton(AF_INET, current, &serveraddr);
					if (rc == 1) {
						hints.ai_family = AF_INET;
						hints.ai_flags |= AI_NUMERICHOST;
					}
					else {
						rc = WS2_32_inet_pton(AF_INET6, current, &serveraddr);
						if (rc == 1) {
							hints.ai_family = AF_INET6;
							hints.ai_flags |= AI_NUMERICHOST;
						}
					}
					
					addrinfo *res;
					int error = getaddrinfo(current, NULL, &hints, &res);
					if (!error) {
						memset(&broadcastEntity.sockaddr, 0, sizeof(broadcastEntity.sockaddr));
						broadcastEntity.entityType = XLLNBroadcastEntity::TYPE::tUNKNOWN;
						broadcastEntity.lastComm = 0;
						
						addrinfo *nextRes = res;
						while (nextRes) {
							if (nextRes->ai_family == AF_INET) {
								memcpy(&broadcastEntity.sockaddr, res->ai_addr, res->ai_addrlen);
								(*(struct sockaddr_in*)&broadcastEntity.sockaddr).sin_port = htons(portHBO);
								broadcastEntity.entityType = IsBroadcastAddress(&broadcastEntity.sockaddr) ? XLLNBroadcastEntity::TYPE::tBROADCAST_ADDR : XLLNBroadcastEntity::TYPE::tUNKNOWN;
								xlive_broadcast_addresses.push_back(broadcastEntity);
								break;
							}
							else if (nextRes->ai_family == AF_INET6) {
								memcpy(&broadcastEntity.sockaddr, res->ai_addr, res->ai_addrlen);
								(*(struct sockaddr_in6*)&broadcastEntity.sockaddr).sin6_port = htons(portHBO);
								xlive_broadcast_addresses.push_back(broadcastEntity);
								break;
							}
							else {
								nextRes = nextRes->ai_next;
							}
						}
						
						freeaddrinfo(res);
					}
				}
			}
		}
		
		if (comma) {
			current = &comma[1];
		}
		else {
			break;
		}
	}
	LeaveCriticalSection(&xlive_critsec_broadcast_addresses);
}

static void ReadTitleConfig(const wchar_t *titleExecutableFilePath)
{
	wchar_t *liveConfig = FormMallocString(L"%s.cfg", titleExecutableFilePath);
	FILE *fpLiveConfig;
	errno_t errorFileOpen = _wfopen_s(&fpLiveConfig, liveConfig, L"r");
	if (errorFileOpen) {
		XLLN_DEBUG_LOG_ECODE(errorFileOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Live Config fopen(\"%ls\", \"r\") error:", __func__, liveConfig);
		free(liveConfig);
	}
	else {
		free(liveConfig);
		
		fseek(fpLiveConfig, (long)0, SEEK_END);
		uint32_t fileSize = ftell(fpLiveConfig);
		fseek(fpLiveConfig, (long)0, SEEK_SET);
		fileSize -= ftell(fpLiveConfig);
		// Add a null sentinel to make the buffer a valid c string.
		fileSize += 1;
		
		uint8_t *buffer = (uint8_t*)malloc(sizeof(uint8_t) * fileSize);
		size_t readC = fread(buffer, sizeof(uint8_t), fileSize / sizeof(uint8_t), fpLiveConfig);
		
		buffer[readC] = 0;
		
		fclose(fpLiveConfig);
		
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "Parsing TITLE.exe.cfg.");
		rapidxml::xml_document<> liveConfigXml;
		liveConfigXml.parse<0>((char*)buffer);
		
		rapidxml::xml_node<> *rootNode = liveConfigXml.first_node();
		while (rootNode) {
			if (strcmp(rootNode->name(), "Liveconfig") == 0) {
				rapidxml::xml_node<> *configNode = rootNode->first_node();
				while (configNode) {
					if (strcmp(configNode->name(), "titleid") == 0) {
						if (sscanf_s(configNode->value(), "%x", &xlive_title_id) == 1) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "Title ID: %X.", xlive_title_id);
						}
					}
					else if (strcmp(configNode->name(), "titleversion") == 0) {
						if (sscanf_s(configNode->value(), "%x", &xlive_title_version) == 1) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "Title Version: 0x%08x.", xlive_title_version);
						}
					}
					configNode = configNode->next_sibling();
				}
				break;
			}
			rootNode = rootNode->next_sibling();
		}
		
		liveConfigXml.clear();
		
		free(buffer);
	}
}

void InitCriticalSections()
{
	InitializeCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
	InitializeCriticalSection(&xlive_critsec_network_adapter);
	InitializeCriticalSection(&xlln_critsec_net_entity);
	InitializeCriticalSection(&xlive_critsec_xlocator_enumerators);
	InitializeCriticalSection(&xlive_critsec_xuser_achievement_enumerators);
	InitializeCriticalSection(&xlive_critsec_xuser_stats);
	InitializeCriticalSection(&xlive_critsec_xfriends_enumerators);
	InitializeCriticalSection(&xlive_critsec_custom_actions);
	InitializeCriticalSection(&xlive_critsec_title_server_enumerators);
	InitializeCriticalSection(&xlive_critsec_presence_enumerators);
	InitializeCriticalSection(&xlive_critsec_xnotify);
	InitializeCriticalSection(&xlive_critsec_xsession);
	InitializeCriticalSection(&xlive_critsec_qos_listeners);
	InitializeCriticalSection(&xlive_critsec_qos_lookups);
	InitializeCriticalSection(&xlive_critsec_xmarketplace);
	InitializeCriticalSection(&xlive_critsec_xcontent);
	InitializeCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	InitializeCriticalSection(&xlln_critsec_liveoverlan_sessions);
	InitializeCriticalSection(&xlive_critsec_fps_limit);
	InitializeCriticalSection(&xlln_critsec_guide_ui_handlers);
	InitializeCriticalSection(&xlive_critsec_sockets);
	InitializeCriticalSection(&xlive_critsec_overlapped_sockets);
	InitializeCriticalSection(&xlive_critsec_xnet_session_keys);
	InitializeCriticalSection(&xlive_critsec_broadcast_addresses);
	InitializeCriticalSection(&xlln_critsec_base_port_offset_mappings);
	InitializeCriticalSection(&xlln_critsec_debug_log);
}

void UninitCriticalSections()
{
	DeleteCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
	DeleteCriticalSection(&xlive_critsec_network_adapter);
	DeleteCriticalSection(&xlln_critsec_net_entity);
	DeleteCriticalSection(&xlive_critsec_xlocator_enumerators);
	DeleteCriticalSection(&xlive_critsec_xuser_achievement_enumerators);
	DeleteCriticalSection(&xlive_critsec_xuser_stats);
	DeleteCriticalSection(&xlive_critsec_xfriends_enumerators);
	DeleteCriticalSection(&xlive_critsec_custom_actions);
	DeleteCriticalSection(&xlive_critsec_title_server_enumerators);
	DeleteCriticalSection(&xlive_critsec_presence_enumerators);
	DeleteCriticalSection(&xlive_critsec_xnotify);
	DeleteCriticalSection(&xlive_critsec_xsession);
	DeleteCriticalSection(&xlive_critsec_qos_listeners);
	DeleteCriticalSection(&xlive_critsec_qos_lookups);
	DeleteCriticalSection(&xlive_critsec_xmarketplace);
	DeleteCriticalSection(&xlive_critsec_xcontent);
	DeleteCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	DeleteCriticalSection(&xlln_critsec_liveoverlan_sessions);
	DeleteCriticalSection(&xlive_critsec_fps_limit);
	DeleteCriticalSection(&xlln_critsec_guide_ui_handlers);
	DeleteCriticalSection(&xlive_critsec_sockets);
	DeleteCriticalSection(&xlive_critsec_overlapped_sockets);
	DeleteCriticalSection(&xlive_critsec_xnet_session_keys);
	DeleteCriticalSection(&xlive_critsec_broadcast_addresses);
	DeleteCriticalSection(&xlln_critsec_base_port_offset_mappings);
	DeleteCriticalSection(&xlln_critsec_debug_log);
}

bool InitXLLN(HMODULE hModule)
{
	BOOL xlln_debug_pause = FALSE;
	
	int nArgs;
	// GetCommandLineW() does not need de-allocating but ToArgv does.
	LPWSTR* lpwszArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	if (lpwszArglist == NULL) {
		uint32_t errorCmdLineToArgv = GetLastError();
		char *messageDescription = FormMallocString("CommandLineToArgvW(...) error 0x%08x.", errorCmdLineToArgv);
		MessageBoxA(NULL, messageDescription, "XLLN CommandLineToArgvW(...) Failed", MB_OK);
		free(messageDescription);
		return false;
	}
	for (int i = 1; i < nArgs; i++) {
		if (wcscmp(lpwszArglist[i], L"-xllndebug") == 0) {
			xlln_debug_pause = TRUE;
		}
		else if (wcscmp(lpwszArglist[i], L"-xllndebuglog") == 0) {
#ifdef XLLN_DEBUG
			xlln_debug = TRUE;
#endif
		}
	}
	
	while (xlln_debug_pause && !IsDebuggerPresent()) {
		Sleep(500L);
	}
	
	for (int i = 1; i < nArgs; i++) {
		if (wcscmp(lpwszArglist[i], L"-xlivedebug") == 0) {
			xlive_debug_pause = TRUE;
		}
		else if (wcscmp(lpwszArglist[i], L"-xlivenetdisable") == 0) {
			xlive_netsocket_abort = TRUE;
		}
		else if (wcsstr(lpwszArglist[i], L"-xllnconfig=") == lpwszArglist[i]) {
			wchar_t *configFilePath = &lpwszArglist[i][12];
			if (xlln_file_config_path) {
				free(xlln_file_config_path);
			}
			xlln_file_config_path = CloneString(configFilePath);
		}
		else if (wcsstr(lpwszArglist[i], L"-xllnlocalinstanceindex=") != NULL) {
			uint32_t tempuint32 = 0;
			if (swscanf_s(lpwszArglist[i], L"-xllnlocalinstanceindex=%u", &tempuint32) == 1) {
				xlln_local_instance_index = tempuint32;
			}
		}
	}
	
	if (xlln_local_instance_index) {
		wchar_t *mutexName = FormMallocString(L"Global\\XLiveLessNessInstanceIndex#%u", xlln_local_instance_index);
		HANDLE mutex = CreateMutexW(0, FALSE, mutexName);
		free(mutexName);
		uint32_t mutex_last_error = GetLastError();
		if (mutex_last_error != ERROR_SUCCESS) {
			char *messageDescription = FormMallocString("Failed to get XLiveLessNess Local Instance Index %u.", xlln_local_instance_index);
			MessageBoxA(NULL, messageDescription, "XLLN Local Instance Index Fail", MB_OK);
			free(messageDescription);
			return false;
		}
	}
	else {
		uint32_t mutex_last_error;
		HANDLE mutex = NULL;
		do {
			if (mutex) {
				mutex_last_error = CloseHandle(mutex);
			}
			wchar_t *mutexName = FormMallocString(L"Global\\XLiveLessNessInstanceIndex#%u", ++xlln_local_instance_index);
			mutex = CreateMutexW(0, FALSE, mutexName);
			free(mutexName);
			mutex_last_error = GetLastError();
		} while (mutex_last_error != ERROR_SUCCESS);
	}
	
	if (!broadcastAddrInput) {
		broadcastAddrInput = new char[1]{ "" };
	}
	
	if (!xlln_direct_ip_connect_password) {
		xlln_direct_ip_connect_password = new char[1]{ "" };
	}
	
	if (!xlln_direct_ip_connect_ip_port) {
		xlln_direct_ip_connect_ip_port = new char[1]{ "" };
	}
	
	for (int i = 0; i < XLIVE_LOCAL_USER_COUNT; i++) {
		xlive_users_info[i] = (XUSER_SIGNIN_INFO*)malloc(sizeof(XUSER_SIGNIN_INFO));
		memset(xlive_users_info[i], 0, sizeof(XUSER_SIGNIN_INFO));
		memset(xlive_users_username[i], 0, sizeof(xlive_users_username[i]));
		xlive_users_auto_login[i] = FALSE;
	}
	
	uint32_t errorXllnConfig = InitXllnConfig();
	uint32_t errorXllnDebugLog = InitDebugLog();
	
	WSADATA wsaData;
	INT result_wsaStartup = WSAStartup(2, &wsaData);
	
	ReadTitleConfig(lpwszArglist[0]);
	
	for (int i = 1; i < nArgs; i++) {
		if (wcsstr(lpwszArglist[i], L"-xlivefps=") != NULL) {
			uint32_t tempuint32 = 0;
			if (swscanf_s(lpwszArglist[i], L"-xlivefps=%u", &tempuint32) == 1) {
				SetFPSLimit(tempuint32);
			}
		}
		else if (wcsstr(lpwszArglist[i], L"-xliveportbase=") != NULL) {
			uint16_t tempuint16 = 0;
			if (swscanf_s(lpwszArglist[i], L"-xliveportbase=%hu", &tempuint16) == 1) {
				if (tempuint16 == 0) {
					tempuint16 = 0xFFFF;
				}
				xlive_base_port = tempuint16;
			}
		}
		else if (wcsstr(lpwszArglist[i], L"-xllnbroadcastaddr=") == lpwszArglist[i]) {
			wchar_t *broadcastAddrInputTemp = &lpwszArglist[i][19];
			size_t bufferLen = wcslen(broadcastAddrInputTemp) + 1;
			if (broadcastAddrInput) {
				delete[] broadcastAddrInput;
			}
			broadcastAddrInput = new char[bufferLen];
			wcstombs2(broadcastAddrInput, broadcastAddrInputTemp, bufferLen);
		}
		else if (wcsstr(lpwszArglist[i], L"-xlivenetworkadapter=") == lpwszArglist[i]) {
			wchar_t *networkAdapterTemp = &lpwszArglist[i][21];
			size_t bufferLen = wcslen(networkAdapterTemp) + 1;
			{
				EnterCriticalSection(&xlive_critsec_network_adapter);
				if (xlive_config_preferred_network_adapter_name) {
					delete[] xlive_config_preferred_network_adapter_name;
				}
				xlive_config_preferred_network_adapter_name = new char[bufferLen];
				wcstombs2(xlive_config_preferred_network_adapter_name, networkAdapterTemp, bufferLen);
				LeaveCriticalSection(&xlive_critsec_network_adapter);
			}
		}
	}
	LocalFree(lpwszArglist);
	
	xlln_hModule = hModule;
	
	uint32_t errorXllnWndMain = InitXllnWndMain();
	uint32_t errorXllnWndSockets = InitXllnWndSockets();
	uint32_t errorXllnWndConnections = InitXllnWndConnections();
	
	return true;
}

bool UninitXLLN()
{
	uint32_t errorXllnWndConnections = UninitXllnWndConnections();
	uint32_t errorXllnWndSockets = UninitXllnWndSockets();
	uint32_t errorXllnWndMain = UninitXllnWndMain();
	
	INT resultWsaCleanup = WSACleanup();
	
	{
		EnterCriticalSection(&xlln_critsec_base_port_offset_mappings);
		
		for (const auto &mappingElement : xlln_base_port_mappings_offset) {
			BASE_PORT_OFFSET_MAPPING *mapping = mappingElement.second;
			delete mapping;
		}
		xlln_base_port_mappings_offset.empty();
		xlln_base_port_mappings_original.empty();
		
		LeaveCriticalSection(&xlln_critsec_base_port_offset_mappings);
	}
	
	uint32_t errorXllnConfig = UninitXllnConfig();
	
	uint32_t errorXllnDebugLog = UninitDebugLog();
	
	if (broadcastAddrInput) {
		delete[] broadcastAddrInput;
		broadcastAddrInput = 0;
	}
	
	if (xlln_direct_ip_connect_password) {
		delete[] xlln_direct_ip_connect_password;
		xlln_direct_ip_connect_password = 0;
	}
	
	if (xlln_direct_ip_connect_ip_port) {
		delete[] xlln_direct_ip_connect_ip_port;
		xlln_direct_ip_connect_ip_port = 0;
	}
	
	return true;
}
