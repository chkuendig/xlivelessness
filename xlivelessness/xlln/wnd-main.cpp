#include <winsock2.h>
#include <Windows.h>
#include "./wnd-main.hpp"
#include "./debug-text.hpp"
#include "./xlln.hpp"
#include "../resource.h"
#include "./wnd-sockets.hpp"
#include "./wnd-connections.hpp"
#include "./xlln-keep-alive.hpp"
#include "../utils/utils.hpp"
#include "../xlive/xdefs.hpp"
#include "../xlive/xlive.hpp"
#include "../xlive/packet-handler.hpp"
#include "../xlive/live-over-lan.hpp"
#include "../xlive/xnetqos.hpp"
#include "../xlive/xrender.hpp"
#include "../xlive/xnotify.hpp"
#include <time.h>
#include <CommCtrl.h>

HWND xlln_window_hwnd = NULL;
static HMENU xlln_window_hMenu = NULL;

void UpdateUserInputBoxes(uint32_t dwUserIndex)
{
	for (uint32_t i = 0; i < 4; i++) {
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
			if (wParam == SC_RESTORE) {
				XLiveNotifyAddEvent(XN_SYS_UI, 1);
			}
			else if (wParam == SC_MINIMIZE || wParam == SC_CLOSE) {
				XLiveNotifyAddEvent(XN_SYS_UI, 0);
			}
			
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
					XLiveThreadQosAbort();
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
					MessageBoxA(
						hwnd
						, "Created by Glitchy Scripts,\n\
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
-xlivenetworkadapter=<string> ? Set the desired network adapter (via {GUID} format).\n\
-xllnconfig=<string> ? Sets the location of the config file.\n\
-xllnlocalinstanceindex=<uint> ? 0 (default) to automatically assign the Local Instance Index."
						, "About"
						, MB_OK
					);
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
					XSocketRebindAllSockets();
					break;
				}
				case MYMENU_NETWORK_ADAPTER_AUTO_SELECT: {
					bool checked = GetMenuState(hMenu_network_adapters, MYMENU_NETWORK_ADAPTER_AUTO_SELECT, 0) != MF_CHECKED;
					CheckMenuItem(hMenu_network_adapters, MYMENU_NETWORK_ADAPTER_AUTO_SELECT, checked ? MF_CHECKED : MF_UNCHECKED);
					EnableMenuItem(hMenu_network_adapters, MYMENU_NETWORK_ADAPTER_IGNORE_TITLE, checked ? MF_ENABLED : MF_DISABLED);
					{
						EnterCriticalSection(&xlive_critsec_network_adapter);
						if (xlive_config_preferred_network_adapter_name) {
							delete[] xlive_config_preferred_network_adapter_name;
							xlive_config_preferred_network_adapter_name = NULL;
						}
						if (!checked) {
							if (xlive_network_adapter && xlive_network_adapter->name) {
								xlive_config_preferred_network_adapter_name = CloneString(xlive_network_adapter->name);
							}
						}
						LeaveCriticalSection(&xlive_critsec_network_adapter);
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
					uint32_t result_login = XLLNLogin(xlln_login_player, liveEnabled, NULL, jlbuffer[0] == 0 ? NULL : jlbuffer);
					xlive_users_auto_login[xlln_login_player] = autoLoginChecked;
					CheckDlgButton(xlln_window_hwnd, MYWINDOW_CHK_AUTOLOGIN, autoLoginChecked ? BST_CHECKED : BST_UNCHECKED);
					break;
				}
				case MYWINDOW_BTN_LOGOUT: {
					uint32_t result_logout = XLLNLogout(xlln_login_player);
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
					// WIP. User index 0.
					XLiveNotifyAddEvent(XN_LIVE_INVITE_ACCEPTED, 0);
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
							XSocketRebindAllSockets();
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

static HMENU CreateDLLWindowMenu(HINSTANCE hModule)
{
	HMENU hMenu;
	hMenu = CreateMenu();
	HMENU hMenuPopup;
	if (hMenu == NULL) {
		return FALSE;
	}
	
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

static DWORD WINAPI ThreadShowXlln(LPVOID lpParam)
{
	uint32_t *threadArgs = (uint32_t*)lpParam;
	uint32_t mainThreadId = threadArgs[0];
	uint32_t dwShowType = threadArgs[1];
	delete[] threadArgs;
	
	// Wait a moment for some Titles to process user input that triggers this event before poping the menu.
	// Otherwise the Title might have issues consuming input correctly and for example repeat the action over and over.
	Sleep(200);
	
	uint32_t currentThreadId = GetCurrentThreadId();
	
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

uint32_t ShowXLLN(uint32_t dwShowType, uint32_t threadId)
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
				XLiveNotifyAddEvent(XN_SYS_UI, 0);
				return ERROR_SUCCESS;
			}
		}
	}
	
	if (dwShowType == XLLN_SHOW_HIDE) {
		XLiveNotifyAddEvent(XN_SYS_UI, 0);
	}
	else if (dwShowType == XLLN_SHOW_HOME || dwShowType == XLLN_SHOW_LOGIN) {
		XLiveNotifyAddEvent(XN_SYS_UI, 1);
	}
	
	uint32_t *threadArgs = new uint32_t[2]{ threadId, dwShowType };
	CreateThread(0, NULL, ThreadShowXlln, (LPVOID)threadArgs, NULL, NULL);
	
	return ERROR_SUCCESS;
}

uint32_t ShowXLLN(uint32_t dwShowType)
{
	return ShowXLLN(dwShowType, GetCurrentThreadId());
}

uint32_t InitXllnWndMain()
{
	CreateThread(0, NULL, ThreadProc, (LPVOID)NULL, NULL, NULL);
	
	return ERROR_SUCCESS;
}

uint32_t UninitXllnWndMain()
{
	return ERROR_SUCCESS;
}
