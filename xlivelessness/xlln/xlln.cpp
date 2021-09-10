#include <winsock2.h>
#include "windows.h"
#include "xlln.hpp"
#include "debug-text.hpp"
#include "xlln-config.hpp"
#include "wnd-sockets.hpp"
#include "wnd-connections.hpp"
#include "xlln-keep-alive.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-checksum.hpp"
#include "../xlive/xdefs.hpp"
#include "../xlive/xlive.hpp"
#include "../xlive/xlocator.hpp"
#include "../xlive/xrender.hpp"
#include "../xlive/packet-handler.hpp"
#include "../xlive/live-over-lan.hpp"
#include "../xlive/xsocket.hpp"
#include "../xlive/net-entity.hpp"
#include "../xlive/xuser.hpp"
#include "../xlive/xsession.hpp"
#include "../xlive/xcustom.hpp"
#include "../xlive/xpresence.hpp"
#include "../xlive/xmarketplace.hpp"
#include "../xlive/xcontent.hpp"
#include "../resource.h"
#include <ws2tcpip.h>
#include <time.h>
#include <CommCtrl.h>
#include "../third-party/rapidxml/rapidxml.hpp"
#include "../third-party/fantasyname/namegen.h"

static LRESULT CALLBACK DLLWindowProc(HWND, UINT, WPARAM, LPARAM);

HINSTANCE xlln_hModule = NULL;
HWND xlln_window_hwnd = NULL;
static HMENU xlln_window_hMenu = NULL;
// 0 - unassigned. Counts from 1.
uint32_t xlln_local_instance_index = 0;
HMENU hMenu_network_adapters = 0;

static DWORD xlln_login_player = 0;
static DWORD xlln_login_player_h[] = { MYMENU_LOGIN1, MYMENU_LOGIN2, MYMENU_LOGIN3, MYMENU_LOGIN4 };

BOOL xlln_debug = FALSE;

static CRITICAL_SECTION xlive_critsec_recvfrom_handler_funcs;
static std::map<DWORD, char*> xlive_recvfrom_handler_funcs;

char *broadcastAddrInput = 0;

int CreateColumn(HWND hwndLV, int iCol, const wchar_t *text, int iWidth)
{
	LVCOLUMN lvc;

	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = iWidth;
	lvc.pszText = (wchar_t*)text;
	lvc.iSubItem = iCol;
	return ListView_InsertColumn(hwndLV, iCol, &lvc);
}

int CreateItem(HWND hwndListView, int iItem)
{
	LV_ITEM item;
	item.mask = LVIF_TEXT;
	item.iItem = iItem;
	item.iIndent = 0;
	item.iSubItem = 0;
	item.state = 0;
	item.cColumns = 0;
	item.pszText = (wchar_t*)L"";
	return ListView_InsertItem(hwndListView, &item);
	/*{
		LV_ITEM item;
		item.mask = LVIF_TEXT;
		item.iItem = 0;
		item.iIndent = 0;
		item.iSubItem = 1;
		item.state = 0;
		item.cColumns = 0;
		item.pszText = (wchar_t*)L"Toothbrush";
		ListView_SetItem(hwndList, &item);
	}*/
}

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


static HMENU CreateDLLWindowMenu(HINSTANCE hModule)
{
	HMENU hMenu;
	hMenu = CreateMenu();
	HMENU hMenuPopup;
	if (hMenu == NULL)
		return FALSE;

	hMenuPopup = CreatePopupMenu();
	AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hMenuPopup, TEXT("File"));
	AppendMenu(hMenuPopup, MF_STRING, MYMENU_EXIT, TEXT("Exit"));

	hMenuPopup = CreatePopupMenu();
	AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hMenuPopup, TEXT("Login"));
	AppendMenu(hMenuPopup, MF_UNCHECKED, MYMENU_LOGIN1, TEXT("Login P1"));
	AppendMenu(hMenuPopup, MF_UNCHECKED, MYMENU_LOGIN2, TEXT("Login P2"));
	AppendMenu(hMenuPopup, MF_UNCHECKED, MYMENU_LOGIN3, TEXT("Login P3"));
	AppendMenu(hMenuPopup, MF_UNCHECKED, MYMENU_LOGIN4, TEXT("Login P4"));

	hMenu_network_adapters = hMenuPopup = CreatePopupMenu();
	AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hMenuPopup, TEXT("Network Adapters"));
	AppendMenu(hMenu_network_adapters, MF_UNCHECKED, MYMENU_NETWORK_ADAPTER_REFRESH, TEXT("Refresh"));
	AppendMenu(hMenu_network_adapters
		, xlive_config_preferred_network_adapter_name ? MF_UNCHECKED : MF_CHECKED
		, MYMENU_NETWORK_ADAPTER_AUTO_SELECT
		, TEXT("Auto Select Adapter")
	);
	AppendMenu(hMenu_network_adapters
		, (xlive_ignore_title_network_adapter ? MF_CHECKED : MF_UNCHECKED) | (xlive_config_preferred_network_adapter_name ? MF_DISABLED : MF_ENABLED)
		, MYMENU_NETWORK_ADAPTER_IGNORE_TITLE
		, TEXT("Ignore Title Preferred Adapter")
	);
	AppendMenu(hMenu_network_adapters, MF_SEPARATOR, MYMENU_NETWORK_ADAPTER_SEPARATOR, NULL);

	hMenuPopup = CreatePopupMenu();
	AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hMenuPopup, TEXT("Debug"));
	AppendMenu(hMenuPopup, MF_UNCHECKED, MYMENU_DEBUG_SOCKETS, TEXT("Sockets"));
	AppendMenu(hMenuPopup, MF_UNCHECKED, MYMENU_DEBUG_CONNECTIONS, TEXT("Connections"));

	hMenuPopup = CreatePopupMenu();
	AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hMenuPopup, TEXT("Help"));
	AppendMenu(hMenuPopup, MF_UNCHECKED, MYMENU_ALWAYSTOP, TEXT("Always on top"));
	AppendMenu(hMenuPopup, MF_STRING, MYMENU_ABOUT, TEXT("About"));

	CheckMenuItem(hMenu, xlln_login_player_h[0], MF_CHECKED);
	//EnableMenuItem(hMenu, MYMENU_LOGIN2, MF_GRAYED);
	//EnableMenuItem(hMenu, MYMENU_LOGIN3, MF_GRAYED);
	//EnableMenuItem(hMenu, MYMENU_LOGIN4, MF_GRAYED);


	return hMenu;
}

static DWORD WINAPI ThreadProc(LPVOID lpParam)
{
	srand((unsigned int)time(NULL));

	// HINSTANCE hModule = reinterpret_cast<HINSTANCE>(lpParam);

	const wchar_t* windowclassname = L"XLLNDLLWindowClass";
	xlln_window_hMenu = CreateDLLWindowMenu(xlln_hModule);

	// Register the windows Class.
	WNDCLASSEXW wc;
	wc.hInstance = xlln_hModule;
	wc.lpszClassName = windowclassname;
	wc.lpfnWndProc = DLLWindowProc;
	wc.style = CS_DBLCLKS;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszMenuName = NULL;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hbrBackground = (HBRUSH)COLOR_BACKGROUND;
	if (!RegisterClassExW(&wc)) {
		return FALSE;
	}

	wchar_t *windowTitle = FormMallocString(L"XLLN #%d v%d.%d.%d.%d", xlln_local_instance_index, DLL_VERSION);

	HWND hwdParent = NULL;// FindWindowW(L"Window Injected Into ClassName", L"Window Injected Into Caption");
	xlln_window_hwnd = CreateWindowExW(0, windowclassname, windowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, xlln_debug ? 700 : 225, hwdParent, xlln_window_hMenu, xlln_hModule, NULL);

	free(windowTitle);

	for (int iUser = 0; iUser < XLIVE_LOCAL_USER_COUNT; iUser++) {
		memcpy(xlive_users_info[iUser]->szUserName, xlive_users_username[iUser], XUSER_NAME_SIZE);
		UpdateUserInputBoxes(iUser);
	}

	CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_DBG_CTX_XLIVE, (xlln_debuglog_level & XLLN_LOG_CONTEXT_XLIVE) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_DBG_CTX_XLLN, (xlln_debuglog_level & XLLN_LOG_CONTEXT_XLIVELESSNESS) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_DBG_CTX_XLLN_MOD, (xlln_debuglog_level & XLLN_LOG_CONTEXT_XLLN_MODULE) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_DBG_CTX_OTHER, (xlln_debuglog_level & XLLN_LOG_CONTEXT_OTHER) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_DBG_LVL_TRACE, (xlln_debuglog_level & XLLN_LOG_LEVEL_TRACE) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_DBG_LVL_DEBUG, (xlln_debuglog_level & XLLN_LOG_LEVEL_DEBUG) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_DBG_LVL_INFO, (xlln_debuglog_level & XLLN_LOG_LEVEL_INFO) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_DBG_LVL_WARN, (xlln_debuglog_level & XLLN_LOG_LEVEL_WARN) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_DBG_LVL_ERROR, (xlln_debuglog_level & XLLN_LOG_LEVEL_ERROR) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_DBG_LVL_FATAL, (xlln_debuglog_level & XLLN_LOG_LEVEL_FATAL) ? BST_CHECKED : BST_UNCHECKED);

	ShowWindow(xlln_window_hwnd, xlln_debug ? SW_NORMAL : SW_HIDE);

	int textBoxes[] = { MYWINDOW_TBX_USERNAME, MYWINDOW_TBX_TEST, MYWINDOW_TBX_BROADCAST };

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		bool consume = false;
		if (msg.message == WM_KEYDOWN) {
			HWND wndItem;
			if (msg.wParam == 'A' && GetKeyState(VK_CONTROL) < 0 && (wndItem = GetFocus())) {
				for (uint8_t i = 0; i < sizeof(textBoxes) / sizeof(textBoxes[0]); i++) {
					if (wndItem == GetDlgItem(xlln_window_hwnd, textBoxes[i])) {
						SendDlgItemMessage(xlln_window_hwnd, textBoxes[i], EM_SETSEL, 0, -1);
						consume = true;
						break;
					}
				}
			}
		}
		if (consume) {
			continue;
		}
		// Handle tab ordering
		if (!IsDialogMessage(xlln_window_hwnd, &msg)) {
			// Translate virtual-key msg into character msg
			TranslateMessage(&msg);
			// Send msg to WindowProcedure(s)
			DispatchMessage(&msg);
		}
	}
	return ERROR_SUCCESS;
}


// #41140
DWORD WINAPI XLLNLogin(DWORD dwUserIndex, BOOL bLiveEnabled, DWORD dwUserId, const CHAR *szUsername)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT)
		return ERROR_NO_SUCH_USER;
	if (szUsername && (!*szUsername || strlen(szUsername) > XUSER_MAX_NAME_LENGTH))
		return ERROR_INVALID_ACCOUNT_NAME;
	if (xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_NotSignedIn)
		return ERROR_ALREADY_ASSIGNED;

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

	xlive_users_info_changed[dwUserIndex] = TRUE;
	xlive_users_auto_login[dwUserIndex] = FALSE;

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
DWORD WINAPI XLLNLogout(DWORD dwUserIndex)
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

	xlive_users_info[dwUserIndex]->UserSigninState = eXUserSigninState_NotSignedIn;
	xlive_users_info_changed[dwUserIndex] = TRUE;

	if (dwUserIndex == xlln_login_player) {
		BOOL checked = FALSE;//GetMenuState(xlln_window_hMenu, xlln_login_player_h[xlln_login_player], 0) != MF_CHECKED;
		//CheckMenuItem(xlln_window_hMenu, xlln_login_player_h[xlln_login_player], checked ? MF_CHECKED : MF_UNCHECKED);

		ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGIN), checked ? SW_HIDE : SW_SHOWNORMAL);
		ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGOUT), checked ? SW_SHOWNORMAL : SW_HIDE);
	}

	return ERROR_SUCCESS;
}

// #41142
DWORD WINAPI XLLNModifyProperty(XLLNModifyPropertyTypes::TYPE propertyId, DWORD *newValue, DWORD *oldValue)
{
	TRACE_FX();
	if (propertyId == XLLNModifyPropertyTypes::tUNKNOWN)
		return ERROR_INVALID_PARAMETER;

	if (propertyId == XLLNModifyPropertyTypes::tFPS_LIMIT) {
		if (oldValue && !newValue) {
			*oldValue = xlive_fps_limit;
		}
		else if (newValue) {
			DWORD old_value = SetFPSLimit(*newValue);
			if (oldValue) {
				*oldValue = old_value;
			}
		}
		else {
			return ERROR_NOT_SUPPORTED;
		}
		return ERROR_SUCCESS;
	}
	/*else if (propertyId == XLLNModifyPropertyTypes::tLiveOverLan_BROADCAST_HANDLER) {
		EnterCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
		if (!newValue && !oldValue) {
			if (liveoverlan_broadcast_handler == NULL) {
				LeaveCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
				return ERROR_NOT_FOUND;
			}
			liveoverlan_broadcast_handler = NULL;
			LeaveCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
			return ERROR_SUCCESS;
		}
		if (newValue && !oldValue && liveoverlan_broadcast_handler != NULL) {
			LeaveCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
			return ERROR_ALREADY_REGISTERED;
		}
		if (oldValue) {
			*oldValue = (DWORD)liveoverlan_broadcast_handler;
		}
		if (newValue) {
			liveoverlan_broadcast_handler = (VOID(WINAPI*)(LIVE_SERVER_DETAILS*))*newValue;
		}
		LeaveCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
		return ERROR_SUCCESS;
	}*/
	else if (propertyId == XLLNModifyPropertyTypes::tRECVFROM_CUSTOM_HANDLER_REGISTER) {
		// TODO
		XLLNModifyPropertyTypes::RECVFROM_CUSTOM_HANDLER_REGISTER *handler = (XLLNModifyPropertyTypes::RECVFROM_CUSTOM_HANDLER_REGISTER*)newValue;
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "XLLN-Module is registering a recvfrom handler 0x%08x."
			, handler->Identifier
		);
		DWORD idLen = strlen(handler->Identifier) + 1;
		char *identifier = (char*)malloc(sizeof(char) * idLen);
		strcpy_s(identifier, idLen, handler->Identifier);

		if (oldValue && newValue) {
			return ERROR_INVALID_PARAMETER;
		}
		EnterCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
		if (oldValue) {
			if (!xlive_recvfrom_handler_funcs.count((DWORD)oldValue)) {
				LeaveCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
				return ERROR_NOT_FOUND;
			}
			xlive_recvfrom_handler_funcs.erase((DWORD)oldValue);
		}
		else {
			if (xlive_recvfrom_handler_funcs.count((DWORD)handler->FuncPtr)) {
				LeaveCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
				return ERROR_ALREADY_REGISTERED;
			}
			xlive_recvfrom_handler_funcs[(DWORD)handler->FuncPtr] = identifier;
		}
		LeaveCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
		return ERROR_SUCCESS;
	}
	else if (propertyId == XLLNModifyPropertyTypes::tRECVFROM_CUSTOM_HANDLER_UNREGISTER) {
		// TODO
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

void UpdateUserInputBoxes(DWORD dwUserIndex)
{
	for (DWORD i = 0; i < 4; i++) {
		BOOL checked = i == xlln_login_player ? TRUE : FALSE;//GetMenuState(xlln_window_hMenu, xlln_login_player_h[xlln_login_player], 0) != MF_CHECKED;
		CheckMenuItem(xlln_window_hMenu, xlln_login_player_h[i], checked ? MF_CHECKED : MF_UNCHECKED);
	}

	if (dwUserIndex != xlln_login_player) {
		return;
	}

	SetDlgItemTextA(xlln_window_hwnd, MYWINDOW_TBX_USERNAME, xlive_users_info[dwUserIndex]->szUserName);

	bool liveEnabled = xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_SignedInToLive || xlive_users_live_enabled[dwUserIndex];
	CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_LIVEENABLE, liveEnabled ? BST_CHECKED : BST_UNCHECKED);

	CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_AUTOLOGIN, xlive_users_auto_login[dwUserIndex] ? BST_CHECKED : BST_UNCHECKED);

	bool loggedIn = xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_NotSignedIn;
	ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGIN), loggedIn ? SW_HIDE : SW_SHOWNORMAL);
	ShowWindow(GetDlgItem(xlln_window_hwnd, MYWINDOW_BTN_LOGOUT), loggedIn ? SW_SHOWNORMAL : SW_HIDE);

	InvalidateRect(xlln_window_hwnd, NULL, FALSE);
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
				addrinfo hints;
				memset(&hints, 0, sizeof(hints));

				hints.ai_family = PF_UNSPEC;
				hints.ai_socktype = SOCK_DGRAM;
				hints.ai_protocol = IPPROTO_UDP;

				struct in6_addr serveraddr;
				int rc = inet_pton(AF_INET, current, &serveraddr);
				if (rc == 1) {
					hints.ai_family = AF_INET;
					hints.ai_flags |= AI_NUMERICHOST;
				}
				else {
					rc = inet_pton(AF_INET6, current, &serveraddr);
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

		if (comma) {
			current = &comma[1];
		}
		else {
			break;
		}
	}
	LeaveCriticalSection(&xlive_critsec_broadcast_addresses);
}

static LRESULT CALLBACK DLLWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(xlln_window_hwnd, &ps);
		SetTextColor(hdc, RGB(0, 0, 0));
		SetBkColor(hdc, 0x00C8C8C8);

		{
			char *textLabel = FormMallocString("Player %u username:", xlln_login_player + 1);
			TextOutA(hdc, 140, 10, textLabel, strlen(textLabel));
			free(textLabel);
		}
		{
			char textLabel[] = "Broadcast address:";
			TextOutA(hdc, 5, 120, textLabel, strlen(textLabel));
		}

		EndPaint(xlln_window_hwnd, &ps);
		break;
	}
	case WM_SYSCOMMAND: {
		if (wParam == SC_CLOSE) {
			ShowXLLN(XLLN_SHOW_HIDE);
			return 0;
		}
		break;
	}
	case WM_CTLCOLORSTATIC: {
		HDC hdc = reinterpret_cast<HDC>(wParam);
		SetTextColor(hdc, RGB(0, 0, 0));
		SetBkColor(hdc, 0x00C8C8C8);
		return (INT_PTR)CreateSolidBrush(0x00C8C8C8);
	}
	case WM_CREATE: {

		CreateWindowA(WC_EDITA, "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
			140, 28, 260, 22, hwnd, (HMENU)MYWINDOW_TBX_USERNAME, xlln_hModule, NULL);

		CreateWindowA(WC_BUTTONA, "Live Enabled", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			140, 51, 110, 22, hwnd, (HMENU)MYWINDOW_CHK_LIVEENABLE, xlln_hModule, NULL);

		CreateWindowA(WC_BUTTONA, "Auto Login", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			280, 51, 100, 22, hwnd, (HMENU)MYWINDOW_CHK_AUTOLOGIN, xlln_hModule, NULL);

		CreateWindowA(WC_BUTTONA, "Login", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			140, 74, 75, 25, hwnd, (HMENU)MYWINDOW_BTN_LOGIN, xlln_hModule, NULL);

		CreateWindowA(WC_BUTTONA, "Logout", WS_CHILD | WS_TABSTOP,
			140, 74, 75, 25, hwnd, (HMENU)MYWINDOW_BTN_LOGOUT, xlln_hModule, NULL);

		CreateWindowA(WC_EDITA, broadcastAddrInput, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
			5, 140, 475, 22, hwnd, (HMENU)MYWINDOW_TBX_BROADCAST, xlln_hModule, NULL);

		if (xlln_debug) {
			CreateWindowA(WC_BUTTONA, "XLIVE", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				5, 170, 70, 20, hwnd, (HMENU)MYWINDOW_CHK_DBG_CTX_XLIVE, xlln_hModule, NULL);

			CreateWindowA(WC_BUTTONA, "XLIVELESSNESS", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				85, 170, 140, 20, hwnd, (HMENU)MYWINDOW_CHK_DBG_CTX_XLLN, xlln_hModule, NULL);

			CreateWindowA(WC_BUTTONA, "XLLN MODULE", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				235, 170, 140, 20, hwnd, (HMENU)MYWINDOW_CHK_DBG_CTX_XLLN_MOD, xlln_hModule, NULL);

			CreateWindowA(WC_BUTTONA, "OTHER", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				375, 170, 70, 20, hwnd, (HMENU)MYWINDOW_CHK_DBG_CTX_OTHER, xlln_hModule, NULL);

			CreateWindowA(WC_BUTTONA, "TRACE", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				5, 190, 70, 20, hwnd, (HMENU)MYWINDOW_CHK_DBG_LVL_TRACE, xlln_hModule, NULL);

			CreateWindowA(WC_BUTTONA, "DEBUG", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				85, 190, 70, 20, hwnd, (HMENU)MYWINDOW_CHK_DBG_LVL_DEBUG, xlln_hModule, NULL);

			CreateWindowA(WC_BUTTONA, "INFO", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				165, 190, 70, 20, hwnd, (HMENU)MYWINDOW_CHK_DBG_LVL_INFO, xlln_hModule, NULL);

			CreateWindowA(WC_BUTTONA, "WARN", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				245, 190, 70, 20, hwnd, (HMENU)MYWINDOW_CHK_DBG_LVL_WARN, xlln_hModule, NULL);

			CreateWindowA(WC_BUTTONA, "ERROR", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				325, 190, 70, 20, hwnd, (HMENU)MYWINDOW_CHK_DBG_LVL_ERROR, xlln_hModule, NULL);

			CreateWindowA(WC_BUTTONA, "FATAL", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				405, 190, 70, 20, hwnd, (HMENU)MYWINDOW_CHK_DBG_LVL_FATAL, xlln_hModule, NULL);

			CreateWindowA(WC_EDITA, "", WS_CHILD | (xlln_debug ? WS_VISIBLE : 0) | WS_BORDER | ES_MULTILINE | WS_SIZEBOX | WS_TABSTOP | WS_HSCROLL,
				5, 210, 475, 420, hwnd, (HMENU)MYWINDOW_TBX_TEST, xlln_hModule, NULL);

			CreateWindowA(WC_BUTTONA, "Test", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				5, 74, 75, 25, hwnd, (HMENU)MYWINDOW_BTN_TEST, xlln_hModule, NULL);
		}

		break;
	}
	case WM_DESTROY: {
		PostQuitMessage(0);
		return 0;
	}
	case WM_CLOSE: {
		// Stupid textbox causes the window to close.
		return 0;
	}
	case WM_COMMAND: {
		bool notHandled = false;

		switch (wParam) {
		case MYMENU_EXIT: {
			// Kill any threads and kill the program.
			StopThreadHotkeys();
			LiveOverLanAbort();
			XLLNKeepAliveAbort();
			exit(EXIT_SUCCESS);
			break;
		}
		case MYMENU_ALWAYSTOP: {
			BOOL checked = GetMenuState(xlln_window_hMenu, MYMENU_ALWAYSTOP, 0) != MF_CHECKED;
			if (checked) {
				SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			}
			else {
				SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			}
			CheckMenuItem(xlln_window_hMenu, MYMENU_ALWAYSTOP, checked ? MF_CHECKED : MF_UNCHECKED);
			break;
		}
		case MYMENU_ABOUT: {
			MessageBoxA(hwnd,
				"Created by Glitchy Scripts,\n\
with thanks to PermaNulled.\n\
\n\
Executable Launch Parameters:\n\
-xlivefps=<uint> ? 0 to disable fps limiter (default 60).\n\
-xllndebug ? Sleep until debugger attach.\n\
-xllndebuglog ? Enable debug log.\n\
-xlivedebug ? Sleep XLiveInitialize until debugger attach.\n\
-xlivenetdisable ? Disable all network functionality.\n\
-xliveportbase=<ushort> ? Change the Base Port (default 2000).\n\
-xllnbroadcastaddr=<string> ? Set the broadcast address.\n\
-xllnconfig=<string> ? Sets the location of the config file.\n\
-xllnlocalinstanceindex=<uint> ? 0 (default) to automatically assign the Local Instance Index."
, "About", MB_OK);
			break;
		}
		case MYMENU_LOGIN1: {
			xlln_login_player = 0;
			UpdateUserInputBoxes(xlln_login_player);
			break;
		}
		case MYMENU_LOGIN2: {
			xlln_login_player = 1;
			UpdateUserInputBoxes(xlln_login_player);
			break;
		}
		case MYMENU_LOGIN3: {
			xlln_login_player = 2;
			UpdateUserInputBoxes(xlln_login_player);
			break;
		}
		case MYMENU_LOGIN4: {
			xlln_login_player = 3;
			UpdateUserInputBoxes(xlln_login_player);
			break;
		}
		case MYMENU_NETWORK_ADAPTER_REFRESH: {
			INT errorNetworkAdapter = RefreshNetworkAdapters();
			break;
		}
		case MYMENU_NETWORK_ADAPTER_AUTO_SELECT: {
			bool checked = GetMenuState(hMenu_network_adapters, MYMENU_NETWORK_ADAPTER_AUTO_SELECT, 0) != MF_CHECKED;
			CheckMenuItem(hMenu_network_adapters, MYMENU_NETWORK_ADAPTER_AUTO_SELECT, checked ? MF_CHECKED : MF_UNCHECKED);
			if (xlive_config_preferred_network_adapter_name) {
				delete[] xlive_config_preferred_network_adapter_name;
				xlive_config_preferred_network_adapter_name = NULL;
			}
			EnableMenuItem(hMenu_network_adapters, MYMENU_NETWORK_ADAPTER_IGNORE_TITLE, checked ? MF_ENABLED : MF_DISABLED);
			if (!checked) {
				if (xlive_network_adapter && xlive_network_adapter->name) {
					xlive_config_preferred_network_adapter_name = CloneString(xlive_network_adapter->name);
				}
			}
			break;
		}
		case MYMENU_NETWORK_ADAPTER_IGNORE_TITLE: {
			bool checked = GetMenuState(hMenu_network_adapters, MYMENU_NETWORK_ADAPTER_IGNORE_TITLE, 0) != MF_CHECKED;
			CheckMenuItem(hMenu_network_adapters, MYMENU_NETWORK_ADAPTER_IGNORE_TITLE, checked ? MF_CHECKED : MF_UNCHECKED);
			xlive_ignore_title_network_adapter = checked;
			break;
		}
		case MYMENU_DEBUG_SOCKETS: {
			XllnWndSocketsShow(true);
			break;
		}
		case MYMENU_DEBUG_CONNECTIONS: {
			XllnWndConnectionsShow(true);
			break;
		}
		case MYWINDOW_CHK_LIVEENABLE: {
			bool checked = IsDlgButtonChecked(xlln_window_hwnd, MYWINDOW_CHK_LIVEENABLE) != BST_CHECKED;
			CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_LIVEENABLE, checked ? BST_CHECKED : BST_UNCHECKED);
			xlive_users_live_enabled[xlln_login_player] = checked;
			break;
		}
		case MYWINDOW_CHK_AUTOLOGIN: {
			bool checked = IsDlgButtonChecked(xlln_window_hwnd, MYWINDOW_CHK_AUTOLOGIN) != BST_CHECKED;
			CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_AUTOLOGIN, checked ? BST_CHECKED : BST_UNCHECKED);
			xlive_users_auto_login[xlln_login_player] = checked;
			break;
		}
		case MYWINDOW_BTN_LOGIN: {
			char jlbuffer[32];
			GetDlgItemTextA(xlln_window_hwnd, MYWINDOW_TBX_USERNAME, jlbuffer, 32);
			TrimRemoveConsecutiveSpaces(jlbuffer);
			jlbuffer[XUSER_NAME_SIZE - 1] = 0;
			SetDlgItemTextA(xlln_window_hwnd, MYWINDOW_TBX_USERNAME, jlbuffer);

			BOOL liveEnabled = IsDlgButtonChecked(xlln_window_hwnd, MYWINDOW_CHK_LIVEENABLE) == BST_CHECKED;
			BOOL autoLoginChecked = xlive_users_auto_login[xlln_login_player];
			if (jlbuffer[0] != 0) {
				memcpy(xlive_users_username[xlln_login_player], jlbuffer, XUSER_NAME_SIZE);
			}
			DWORD result_login = XLLNLogin(xlln_login_player, liveEnabled, NULL, jlbuffer[0] == 0 ? NULL : jlbuffer);
			xlive_users_auto_login[xlln_login_player] = autoLoginChecked;
			CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_AUTOLOGIN, autoLoginChecked ? BST_CHECKED : BST_UNCHECKED);
			break;
		}
		case MYWINDOW_BTN_LOGOUT: {
			DWORD result_logout = XLLNLogout(xlln_login_player);
			break;
		}
#define XLLN_CHK_DBG_TOGGLE(checkbox_menu_btn_id, log_level_flag) {\
bool checked = IsDlgButtonChecked(xlln_window_hwnd, checkbox_menu_btn_id) != BST_CHECKED;\
CheckDlgButton(xlln_window_hwnd, checkbox_menu_btn_id, checked ? BST_CHECKED : BST_UNCHECKED);\
xlln_debuglog_level = checked ? (xlln_debuglog_level | log_level_flag) : (xlln_debuglog_level & ~(log_level_flag));\
}
		case MYWINDOW_CHK_DBG_CTX_XLIVE: {
			XLLN_CHK_DBG_TOGGLE(MYWINDOW_CHK_DBG_CTX_XLIVE, XLLN_LOG_CONTEXT_XLIVE);
			break;
		}
		case MYWINDOW_CHK_DBG_CTX_XLLN: {
			XLLN_CHK_DBG_TOGGLE(MYWINDOW_CHK_DBG_CTX_XLLN, XLLN_LOG_CONTEXT_XLIVELESSNESS);
			break;
		}
		case MYWINDOW_CHK_DBG_CTX_XLLN_MOD: {
			XLLN_CHK_DBG_TOGGLE(MYWINDOW_CHK_DBG_CTX_XLLN_MOD, XLLN_LOG_CONTEXT_XLLN_MODULE);
			break;
		}
		case MYWINDOW_CHK_DBG_CTX_OTHER: {
			XLLN_CHK_DBG_TOGGLE(MYWINDOW_CHK_DBG_CTX_OTHER, XLLN_LOG_CONTEXT_OTHER);
			break;
		}
		case MYWINDOW_CHK_DBG_LVL_TRACE: {
			XLLN_CHK_DBG_TOGGLE(MYWINDOW_CHK_DBG_LVL_TRACE, XLLN_LOG_LEVEL_TRACE);
			break;
		}
		case MYWINDOW_CHK_DBG_LVL_DEBUG: {
			XLLN_CHK_DBG_TOGGLE(MYWINDOW_CHK_DBG_LVL_DEBUG, XLLN_LOG_LEVEL_DEBUG);
			break;
		}
		case MYWINDOW_CHK_DBG_LVL_INFO: {
			XLLN_CHK_DBG_TOGGLE(MYWINDOW_CHK_DBG_LVL_INFO, XLLN_LOG_LEVEL_INFO);
			break;
		}
		case MYWINDOW_CHK_DBG_LVL_WARN: {
			XLLN_CHK_DBG_TOGGLE(MYWINDOW_CHK_DBG_LVL_WARN, XLLN_LOG_LEVEL_WARN);
			break;
		}
		case MYWINDOW_CHK_DBG_LVL_ERROR: {
			XLLN_CHK_DBG_TOGGLE(MYWINDOW_CHK_DBG_LVL_ERROR, XLLN_LOG_LEVEL_ERROR);
			break;
		}
		case MYWINDOW_CHK_DBG_LVL_FATAL: {
			XLLN_CHK_DBG_TOGGLE(MYWINDOW_CHK_DBG_LVL_FATAL, XLLN_LOG_LEVEL_FATAL);
			break;
		}
		case MYWINDOW_BTN_TEST: {
			// WIP.
			extern bool xlive_invite_to_game;
			xlive_invite_to_game = true;
			break;
		}
		default: {
			if (wParam == ((EN_CHANGE << 16) | MYWINDOW_TBX_BROADCAST)) {
				if (xlln_window_hwnd) {
					char jlbuffer[500];
					GetDlgItemTextA(xlln_window_hwnd, MYWINDOW_TBX_BROADCAST, jlbuffer, 500);
					size_t buflen = strlen(jlbuffer) + 1;
					delete[] broadcastAddrInput;
					broadcastAddrInput = new char[buflen];
					memcpy(broadcastAddrInput, jlbuffer, buflen);
					broadcastAddrInput[buflen - 1] = 0;
					char *temp = CloneString(broadcastAddrInput);
					ParseBroadcastAddrInput(temp);
					delete[] temp;
				}
				break;
			}

			{
				int numOfNetAdapterMenuItems = GetMenuItemCount(hMenu_network_adapters);
				const uint32_t numOfItemsBefore = 4;
				if (numOfNetAdapterMenuItems > numOfItemsBefore && wParam >= MYMENU_NETWORK_ADAPTERS && wParam < MYMENU_NETWORK_ADAPTERS + (uint32_t)numOfNetAdapterMenuItems - numOfItemsBefore) {
					const uint32_t networkAdapterIndex = wParam - MYMENU_NETWORK_ADAPTERS;
					{
						EnterCriticalSection(&xlive_critsec_network_adapter);
						if (networkAdapterIndex < xlive_eligible_network_adapters.size()) {
							xlive_network_adapter = xlive_eligible_network_adapters[networkAdapterIndex];
							if (xlive_config_preferred_network_adapter_name) {
								delete[] xlive_config_preferred_network_adapter_name;
							}
							xlive_config_preferred_network_adapter_name = CloneString(xlive_network_adapter->name);
							CheckMenuItem(hMenu_network_adapters, MYMENU_NETWORK_ADAPTER_AUTO_SELECT, MF_UNCHECKED);
							EnableMenuItem(hMenu_network_adapters, MYMENU_NETWORK_ADAPTER_IGNORE_TITLE, MF_DISABLED);
							for (size_t iEA = 0; iEA < xlive_eligible_network_adapters.size(); iEA++) {
								EligibleAdapter* ea = xlive_eligible_network_adapters[iEA];
								CheckMenuItem(hMenu_network_adapters, MYMENU_NETWORK_ADAPTERS + iEA, ea == xlive_network_adapter ? MF_CHECKED : MF_UNCHECKED);
							}
						}
						LeaveCriticalSection(&xlive_critsec_network_adapter);
					}
					break;
				}
			}

			notHandled = true;
			break;
		}
		}
		if (!notHandled) {
			return 0;
		}
	}
	default: {
		break;
	}
	}

	return DefWindowProcW(hwnd, message, wParam, lParam);
}

static DWORD WINAPI ThreadShowXlln(LPVOID lpParam)
{
	DWORD *threadArgs = (DWORD*)lpParam;
	DWORD mainThreadId = threadArgs[0];
	DWORD dwShowType = threadArgs[1];
	delete[] threadArgs;

	// Wait a moment for some Titles to process user input that triggers this event before poping the menu.
	// Otherwise the Title might have issues consuming input correctly and for example repeat the action over and over.
	Sleep(200);

	DWORD currentThreadId = GetCurrentThreadId();

	// Attach to the main thread as secondary threads cannot use GUI functions.
	AttachThreadInput(currentThreadId, mainThreadId, TRUE);

	if (dwShowType == XLLN_SHOW_HIDE) {
		ShowWindow(xlln_window_hwnd, SW_HIDE);
	}
	else if (dwShowType == XLLN_SHOW_HOME) {
		ShowWindow(xlln_window_hwnd, SW_SHOWNORMAL);
		SetWindowPos(xlln_window_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}
	else if (dwShowType == XLLN_SHOW_LOGIN) {
		ShowWindow(xlln_window_hwnd, SW_SHOWNORMAL);
		SetWindowPos(xlln_window_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		SendMessage(xlln_window_hwnd, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(xlln_window_hwnd, MYWINDOW_TBX_USERNAME), TRUE);
	}

	AttachThreadInput(currentThreadId, mainThreadId, FALSE);

	return ERROR_SUCCESS;
}

uint32_t ShowXLLN(DWORD dwShowType, DWORD threadId)
{
	if (dwShowType == XLLN_SHOW_LOGIN) {
		bool anyGotAutoLogged = false;

		// Check if there are any accounts already logged in.
		for (int i = 0; i < XLIVE_LOCAL_USER_COUNT; i++) {
			if (xlive_users_info[i]->UserSigninState != eXUserSigninState_NotSignedIn) {
				anyGotAutoLogged = true;
				break;
			}
		}

		// If no accounts are logged in then auto login the ones that have the flag.
		if (!anyGotAutoLogged) {
			for (int i = 0; i < XLIVE_LOCAL_USER_COUNT; i++) {
				if (xlive_users_auto_login[i]) {
					anyGotAutoLogged = true;
					BOOL autoLoginChecked = xlive_users_auto_login[i];
					XLLNLogin(i, xlive_users_live_enabled[i], 0, strlen(xlive_users_info[i]->szUserName) > 0 ? xlive_users_info[i]->szUserName : 0);
					xlive_users_auto_login[i] = autoLoginChecked;
					if (i == xlln_login_player) {
						CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_AUTOLOGIN, autoLoginChecked ? BST_CHECKED : BST_UNCHECKED);
					}
				}
			}

			// If there were accounts that were auto logged in then do not pop the XLLN window.
			if (anyGotAutoLogged) {
				return ERROR_SUCCESS;
			}
		}
	}

	DWORD *threadArgs = new DWORD[2]{ threadId, dwShowType };
	CreateThread(0, NULL, ThreadShowXlln, (LPVOID)threadArgs, NULL, NULL);

	return ERROR_SUCCESS;
}

uint32_t ShowXLLN(DWORD dwShowType)
{
	return ShowXLLN(dwShowType, GetCurrentThreadId());
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
	//InitializeCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
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
	InitializeCriticalSection(&xlive_critsec_xmarketplace);
	InitializeCriticalSection(&xlive_critsec_xcontent);
	InitializeCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	InitializeCriticalSection(&xlln_critsec_liveoverlan_sessions);
	InitializeCriticalSection(&xlive_critsec_fps_limit);
	InitializeCriticalSection(&xlive_critsec_sockets);
	InitializeCriticalSection(&xlive_critsec_broadcast_addresses);
	InitializeCriticalSection(&xlln_critsec_debug_log);
}

void UninitCriticalSections()
{
	DeleteCriticalSection(&xlive_critsec_recvfrom_handler_funcs);
	DeleteCriticalSection(&xlive_critsec_network_adapter);
	//DeleteCriticalSection(&xlive_critsec_LiveOverLan_broadcast_handler);
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
	DeleteCriticalSection(&xlive_critsec_xmarketplace);
	DeleteCriticalSection(&xlive_critsec_xcontent);
	DeleteCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	DeleteCriticalSection(&xlln_critsec_liveoverlan_sessions);
	DeleteCriticalSection(&xlive_critsec_fps_limit);
	DeleteCriticalSection(&xlive_critsec_sockets);
	DeleteCriticalSection(&xlive_critsec_broadcast_addresses);
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
		DWORD mutex_last_error = GetLastError();
		if (mutex_last_error != ERROR_SUCCESS) {
			char *messageDescription = FormMallocString("Failed to get XLiveLessNess Local Instance Index %u.", xlln_local_instance_index);
			MessageBoxA(NULL, messageDescription, "XLLN Local Instance Index Fail", MB_OK);
			free(messageDescription);
			return false;
		}
	}
	else {
		DWORD mutex_last_error;
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

	for (int i = 0; i < XLIVE_LOCAL_USER_COUNT; i++) {
		xlive_users_info[i] = (XUSER_SIGNIN_INFO*)malloc(sizeof(XUSER_SIGNIN_INFO));
		memset(xlive_users_info[i], 0, sizeof(XUSER_SIGNIN_INFO));
		memset(xlive_users_username[i], 0, sizeof(xlive_users_username[i]));
		xlive_users_info_changed[i] = FALSE;
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
	}
	LocalFree(lpwszArglist);

	xlln_hModule = hModule;
	CreateThread(0, NULL, ThreadProc, (LPVOID)NULL, NULL, NULL);

	uint32_t errorXllnWndSockets = InitXllnWndSockets();
	uint32_t errorXllnWndConnections = InitXllnWndConnections();

	return true;
}

bool UninitXLLN()
{
	uint32_t errorXllnWndConnections = UninitXllnWndConnections();
	uint32_t errorXllnWndSockets = UninitXllnWndSockets();

	INT resultWsaCleanup = WSACleanup();

	uint32_t errorXllnConfig = UninitXllnConfig();

	uint32_t errorXllnDebugLog = UninitDebugLog();

	if (broadcastAddrInput) {
		delete[] broadcastAddrInput;
		broadcastAddrInput = 0;
	}

	return true;
}
