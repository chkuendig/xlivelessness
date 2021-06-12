#include <winsock2.h>
#include "xdefs.hpp"
#include "../xlln/debug-text.hpp"
#include "xlive.hpp"
#include "xrender.hpp"
#include "xnet.hpp"
#include "../xlln/xlln.hpp"
#include "../utils/utils.hpp"
#include "xsocket.hpp"
#include "xwsa.hpp"
#include "xlocator.hpp"
#include "xsession.hpp"
#include "xpresence.hpp"
#include "xmarketplace.hpp"
#include "xcontent.hpp"
#include "net-entity.hpp"
#include "xuser.hpp"
#include <time.h>
#include <d3d9.h>
#include <string>
#include <vector>
// Link with iphlpapi.lib
#include <iphlpapi.h>

BOOL xlive_debug_pause = FALSE;

BOOL xlive_users_info_changed[XLIVE_LOCAL_USER_COUNT];
BOOL xlive_users_auto_login[XLIVE_LOCAL_USER_COUNT];
BOOL xlive_users_live_enabled[XLIVE_LOCAL_USER_COUNT];
CHAR xlive_users_username[XLIVE_LOCAL_USER_COUNT][XUSER_NAME_SIZE];
XUSER_SIGNIN_INFO* xlive_users_info[XLIVE_LOCAL_USER_COUNT];

struct NOTIFY_LISTENER {
	HANDLE id;
	ULONGLONG area;
};
static NOTIFY_LISTENER g_listener[50];
static int g_dwListener = 0;

bool xlive_invite_to_game = false;

uint32_t xlive_title_id = 0;
uint32_t xlive_title_version = 0;

CRITICAL_SECTION xlive_critsec_xnotify;

CRITICAL_SECTION xlive_critsec_xfriends_enumerators;
// Key: enumerator handle (id).
// Value: Vector of ??? that have already been returned for that enumerator.
std::map<HANDLE, std::vector<uint32_t>> xlive_xfriends_enumerators;

CRITICAL_SECTION xlive_critsec_network_adapter;
char* xlive_init_preferred_network_adapter_name = NULL;
char* xlive_config_preferred_network_adapter_name = NULL;
bool xlive_ignore_title_network_adapter = false;
EligibleAdapter *xlive_network_adapter = NULL;
std::vector<EligibleAdapter*> xlive_eligible_network_adapters;

BOOL xlive_online_initialized = FALSE;
static BOOL xlive_initialised = FALSE;

CRITICAL_SECTION xlive_critsec_title_server_enumerators;
// Key: enumerator handle (id).
// Value: Vector of ??? that have already been returned for that enumerator.
std::map<HANDLE, std::vector<uint32_t>> xlive_title_server_enumerators;

static XLIVE_DEBUG_LEVEL xlive_xdlLevel = XLIVE_DEBUG_LEVEL_OFF;

INT RefreshNetworkAdapters()
{
	INT result = ERROR_UNIDENTIFIED_ERROR;
	DWORD dwRetVal = 0;
	// Set the flags to pass to GetAdaptersAddresses.
	ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
	ULONG family = AF_INET;
	PIP_ADAPTER_ADDRESSES pAddresses = NULL;
	// Allocate a 15 KB buffer to start with.
	ULONG outBufLen = 15000;
	ULONG Iterations = 0;
	PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
	PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;

	do {
		pAddresses = (IP_ADAPTER_ADDRESSES*)HeapAlloc(GetProcessHeap(), 0, (outBufLen));
		if (pAddresses == NULL) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s memory allocation failed for IP_ADAPTER_ADDRESSES struct.", __func__);
			dwRetVal = ERROR_NOT_ENOUGH_MEMORY;
			break;
		}

		dwRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

		if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
			HeapFree(GetProcessHeap(), 0, pAddresses);
			pAddresses = NULL;
		}
		else {
			break;
		}

		Iterations++;
		// 3 attempts max.
	} while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < 3));

	if (dwRetVal != NO_ERROR && dwRetVal != ERROR_NO_DATA) {
		result = ERROR_UNIDENTIFIED_ERROR;
		XLLN_DEBUG_LOG_ECODE(dwRetVal, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s GetAdaptersAddresses(AF_INET, ...) failed with error:", __func__);
	}

	{
		EnterCriticalSection(&xlive_critsec_network_adapter);

		xlive_network_adapter = NULL;
		int menuCount = GetMenuItemCount(hMenu_network_adapters);
		for (EligibleAdapter* ea : xlive_eligible_network_adapters) {
			RemoveMenu(hMenu_network_adapters, --menuCount, MF_BYPOSITION);
			if (ea->name) {
				delete[] ea->name;
			}
			if (ea->description) {
				delete[] ea->description;
			}
			delete ea;
		}
		xlive_eligible_network_adapters.clear();

		if (dwRetVal == NO_ERROR) {
			pCurrAddresses = pAddresses;
			while (pCurrAddresses) {

				if (pCurrAddresses->OperStatus == 1) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
						, "%s Checking elegibility of adapter: %s - %ls."
						, __func__
						, pCurrAddresses->AdapterName
						, pCurrAddresses->Description
					);

					pUnicast = pCurrAddresses->FirstUnicastAddress;

					for (int i = 0; pUnicast != NULL; i++)
					{
						if (pUnicast->Address.lpSockaddr->sa_family == AF_INET)
						{
							sockaddr_in *sa_in = (sockaddr_in *)pUnicast->Address.lpSockaddr;
							ULONG dwMask = 0;
							dwRetVal = ConvertLengthToIpv4Mask(pUnicast->OnLinkPrefixLength, &dwMask);
							if (dwRetVal == NO_ERROR) {
								EligibleAdapter *ea = new EligibleAdapter;
								ea->name = CloneString(pCurrAddresses->AdapterName);
								ea->description = CloneString(pCurrAddresses->Description);
								ea->unicastHAddr = ntohl(((sockaddr_in *)pUnicast->Address.lpSockaddr)->sin_addr.s_addr);
								ea->unicastHMask = ntohl(dwMask);
								ea->hBroadcast = ~(ea->unicastHMask) | ea->unicastHAddr;
								ea->minLinkSpeed = pCurrAddresses->ReceiveLinkSpeed;
								if (pCurrAddresses->TransmitLinkSpeed < pCurrAddresses->ReceiveLinkSpeed) {
									ea->minLinkSpeed = pCurrAddresses->TransmitLinkSpeed;
								}
								ea->hasDnsServer = pCurrAddresses->FirstDnsServerAddress ? TRUE : FALSE;
								xlive_eligible_network_adapters.push_back(ea);
								XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
									, "%s Adding eligible adapter: %s - %ls - 0x%08x - 0x%08x."
									, __func__
									, ea->name
									, ea->description
									, ea->unicastHAddr
									, ea->unicastHMask
								);
							}
						}
						pUnicast = pUnicast->Next;
					}
				}

				pCurrAddresses = pCurrAddresses->Next;
			}

			for (EligibleAdapter* ea : xlive_eligible_network_adapters) {
				if (xlive_config_preferred_network_adapter_name && strncmp(ea->name, xlive_config_preferred_network_adapter_name, MAX_ADAPTER_NAME_LENGTH + 1) == 0) {
					xlive_network_adapter = ea;
					break;
				}
			}

			if (!xlive_network_adapter) {
				for (EligibleAdapter* ea : xlive_eligible_network_adapters) {
					if (!xlive_ignore_title_network_adapter && xlive_init_preferred_network_adapter_name && strncmp(ea->name, xlive_init_preferred_network_adapter_name, MAX_ADAPTER_NAME_LENGTH + 1) == 0) {
						xlive_network_adapter = ea;
						break;
					}
					if (ea->unicastHAddr == INADDR_LOOPBACK || ea->unicastHAddr == INADDR_BROADCAST || ea->unicastHAddr == INADDR_NONE) {
						continue;
					}
					if (!xlive_network_adapter) {
						xlive_network_adapter = ea;
						continue;
					}
					if ((ea->hasDnsServer && !xlive_network_adapter->hasDnsServer) ||
						(ea->minLinkSpeed > xlive_network_adapter->minLinkSpeed)) {
						xlive_network_adapter = ea;
					}
				}
			}

			if (xlive_network_adapter) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_INFO
					, "%s Selected network adapter: %s - %ls."
					, __func__
					, xlive_network_adapter->name
					, xlive_network_adapter->description
				);

				result = ERROR_SUCCESS;
			}
			else {
				result = ERROR_NETWORK_UNREACHABLE;
			}
		}

		if (!xlive_network_adapter) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Failed to find any eligible network adapters. Using Loopback Address.", __func__);
			EligibleAdapter *ea = new EligibleAdapter;
			ea->name = NULL;
			ea->description = NULL;
			ea->unicastHAddr = INADDR_LOOPBACK;
			ea->unicastHMask = 0;
			ea->hBroadcast = INADDR_BROADCAST;
			ea->minLinkSpeed = 0;
			ea->hasDnsServer = FALSE;
			xlive_eligible_network_adapters.push_back(ea);
			xlive_network_adapter = ea;
		}

		for (size_t iEA = 0; iEA < xlive_eligible_network_adapters.size(); iEA++) {
			EligibleAdapter* ea = xlive_eligible_network_adapters[iEA];
			AppendMenu(hMenu_network_adapters, ea == xlive_network_adapter ? MF_CHECKED : MF_UNCHECKED, MYMENU_NETWORK_ADAPTERS + iEA, ea->description ? ea->description : L"NULL");
		}

		LeaveCriticalSection(&xlive_critsec_network_adapter);
	}

	if (pAddresses) {
		HeapFree(GetProcessHeap(), 0, pAddresses);
	}

	return result;
}

void CreateLocalUser()
{
	XNADDR *pAddr = &xlive_local_xnAddr;

	uint32_t instanceId = ntohl((rand() + (rand() << 16)) << 8);
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "%s instanceId: 0x%08x."
		, __func__
		, instanceId
	);

	DWORD mac_fix = 0x00131000;

	pAddr->ina.s_addr = htonl(instanceId);
	pAddr->wPortOnline = htons(xlive_base_port);
	pAddr->inaOnline.s_addr = htonl(instanceId);

	memset(&(pAddr->abEnet), 0, 6);
	memset(&(pAddr->abOnline), 0, 6);

	memcpy(&(pAddr->abEnet), &instanceId, 4);
	memcpy(&(pAddr->abOnline), &instanceId, 4);

	memcpy((BYTE*)&(pAddr->abEnet) + 3, (BYTE*)&mac_fix + 1, 3);
	memcpy((BYTE*)&(pAddr->abOnline) + 17, (BYTE*)&mac_fix + 1, 3);
}

void Check_Overlapped(PXOVERLAPPED pOverlapped)
{
	if (!pOverlapped)
		return;

	if (pOverlapped->hEvent) {
		SetEvent(pOverlapped->hEvent);
	}

	if (pOverlapped->pCompletionRoutine) {
		pOverlapped->pCompletionRoutine(pOverlapped->InternalLow, pOverlapped->InternalHigh, pOverlapped->dwCompletionContext);
	}
}

BOOL XNotifyGetNextHelper(ULONGLONG notificationArea, PDWORD pdwId, PULONG_PTR pParam)
{
	if (notificationArea & XNOTIFY_SYSTEM) {

		*pParam = 0x00000000;

		for (int i = 0; i < XLIVE_LOCAL_USER_COUNT; i++) {
			if (xlive_users_info_changed[i]) {
				xlive_users_info_changed[i] = FALSE;
				*pParam |= 1 << i;
			}
		}

		if (*pParam) {
			*pdwId = XN_SYS_SIGNINCHANGED;
			return TRUE;
		}
	}
	if (false && notificationArea & XNOTIFY_SYSTEM) {

		//*pParam = XONLINE_S_LOGON_CONNECTION_ESTABLISHED;
		*pParam = XONLINE_S_LOGON_DISCONNECTED;

		if (*pParam) {
			*pdwId = XN_LIVE_CONNECTIONCHANGED;
			return TRUE;
		}
	}
	if (notificationArea & XNOTIFY_LIVE) {
		if (xlive_invite_to_game) {
			*pdwId = XN_LIVE_INVITE_ACCEPTED;
			*pParam = 0x00000000;
			return TRUE;
		}
	}
	return FALSE;
}

// #651
BOOL WINAPI XNotifyGetNext(HANDLE hNotification, DWORD dwMsgFilter, PDWORD pdwId, PULONG_PTR pParam)
{
	TRACE_FX();
	EnterCriticalSection(&xlive_critsec_xnotify);
	ResetEvent(hNotification);

	BOOL result = FALSE;

	int noteId = 0;
	for (; noteId < g_dwListener; noteId++) {
		if (g_listener[noteId].id == hNotification) {
			break;
		}
	}
	if (noteId == g_dwListener) {
		//noteId = -1;
		__debugbreak();
	}
	else {
		result = XNotifyGetNextHelper(dwMsgFilter ? dwMsgFilter : g_listener[noteId].area, pdwId, pParam);
	}

	if (result) {
		SetEvent(hNotification);
	}
	LeaveCriticalSection(&xlive_critsec_xnotify);
	return result;
}

#define XNOTIFYUI_POS_TOPLEFT ?
#define XNOTIFYUI_POS_TOPCENTER ?
#define XNOTIFYUI_POS_TOPRIGHT ?
#define XNOTIFYUI_POS_CENTERLEFT ?
#define XNOTIFYUI_POS_CENTER ?
#define XNOTIFYUI_POS_CENTERRIGHT ?
#define XNOTIFYUI_POS_BOTTOMLEFT ?
#define XNOTIFYUI_POS_BOTTOMCENTER ?
#define XNOTIFYUI_POS_BOTTOMRIGHT ?

// #652
VOID WINAPI XNotifyPositionUI(DWORD dwPosition)
{
	TRACE_FX();
	// Invalid dwPos--check XNOTIFYUI_POS_* bits.  Do not specify both TOP and BOTTOM or both LEFT and RIGHT.
	if (dwPosition & 0xFFFFFFF0 || dwPosition & 1 && dwPosition & 2 || dwPosition & 8 && dwPosition & 4) {
		return;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
}

// #653
DWORD WINAPI XNotifyDelayUI(ULONG ulMilliSeconds)
{
	TRACE_FX();
	return ERROR_SUCCESS;
}

// #1082
DWORD WINAPI XGetOverlappedExtendedError(PXOVERLAPPED pOverlapped)
{
	TRACE_FX();
	if (!pOverlapped) {
		return GetLastError();
	}
	if (pOverlapped->InternalLow != ERROR_IO_PENDING) {
		return pOverlapped->dwExtendedError;
	}
	return ERROR_IO_INCOMPLETE;
}

// #1083
DWORD WINAPI XGetOverlappedResult(PXOVERLAPPED pOverlapped, LPDWORD pdwResult, BOOL bWait)
{
	TRACE_FX();
	if (!pOverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pOverlapped is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (pOverlapped->InternalLow == ERROR_IO_PENDING) {
		DWORD waitResult;
		if (bWait && pOverlapped->hEvent != 0) {
			waitResult = WaitForSingleObject(pOverlapped->hEvent, 0xFFFFFFFF);
		}
		else {
			waitResult = WAIT_TIMEOUT;
		}
		if (waitResult == WAIT_TIMEOUT) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Handle %08x is incomplete.", __func__, pOverlapped->hEvent);
			return ERROR_IO_INCOMPLETE;
		}
		if (waitResult) {
			return GetLastError();
		}
	}

	if (pdwResult) {
		*pdwResult = pOverlapped->InternalHigh;
	}

	return pOverlapped->InternalLow;
}

// #5000
HRESULT WINAPI XLiveInitialize(XLIVE_INITIALIZE_INFO *pPii)
{
	TRACE_FX();
	HRESULT WINAPI XLiveInitializeEx(XLIVE_INITIALIZE_INFO *pPii, DWORD dwTitleXLiveVersion);
	return XLiveInitializeEx(pPii, 0);
}

// #5001
HRESULT WINAPI XLiveInput(XLIVE_INPUT_INFO *pPii)
{
	TRACE_FX();
	if (!pPii) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pPii is NULL.", __func__);
		return E_POINTER;
	}
	pPii->fHandled = FALSE;
	return S_OK;
}

// #5003
VOID WINAPI XLiveUninitialize()
{
	TRACE_FX();

	xlive_initialised = FALSE;

	INT errorXSession = UninitXSession();
	INT errorXRender = UninitXRender();
	INT errorNetEntity = UninitNetEntity();
	INT errorXSocket = UninitXSocket();
}

// #5005
HRESULT WINAPI XLiveOnCreateDevice(IUnknown *pD3D, VOID *pD3DPP)
{
	TRACE_FX();
	if (!pD3D) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pD3D is NULL.", __func__);
		return E_POINTER;
	}
	if (!pD3DPP) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pD3DPP is NULL.", __func__);
		return E_POINTER;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return S_OK;
}

// #5006
HRESULT WINAPI XLiveOnDestroyDevice()
{
	TRACE_FX();
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return S_OK;
}

// #5007
HRESULT WINAPI XLiveOnResetDevice(void *pD3DPP)
{
	TRACE_FX();
	D3DPRESENT_PARAMETERS *pD3DPP2 = (D3DPRESENT_PARAMETERS*)pD3DPP;
	//ID3D10Device *pD3DPP2 = (ID3D10Device*)pD3DPP;
	return S_OK;
}

// #5010: This function is deprecated.
HRESULT WINAPI XLiveRegisterDataSection(DWORD a1, DWORD a2, DWORD a3)
{
	TRACE_FX();
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return S_OK;
	//if (XLivepGetTitleXLiveVersion() < 0x20000000)
	return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
}

// #5011: This function is deprecated.
HRESULT WINAPI XLiveUnregisterDataSection(DWORD a1)
{
	TRACE_FX();
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return S_OK;
	//if (XLivepGetTitleXLiveVersion() < 0x20000000)
	return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
}

// #5012: This function is deprecated.
HRESULT WINAPI XLiveUpdateHashes(DWORD a1, DWORD a2)
{
	TRACE_FX();
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return S_OK;
	//if (XLivepGetTitleXLiveVersion() < 0x20000000)
	return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);// 0x80070032;
}

// #5022
HRESULT WINAPI XLiveGetUpdateInformation(PXLIVEUPDATE_INFORMATION pXLiveUpdateInfo)
{
	TRACE_FX();
	if (!pXLiveUpdateInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXLiveUpdateInfo is NULL.", __func__);
		return E_POINTER;
	}
	if (pXLiveUpdateInfo->cbSize != sizeof(XLIVEUPDATE_INFORMATION)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pXLiveUpdateInfo->cbSize != sizeof(XLIVEUPDATE_INFORMATION)) Invalid buffer.", __func__);
		return HRESULT_FROM_WIN32(ERROR_INVALID_USER_BUFFER);//0x800706F8;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	// No update?
	return S_FALSE;
}

// #5024
HRESULT WINAPI XLiveUpdateSystem(LPCWSTR lpwszRelaunchCmdLine)
{
	TRACE_FX();
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return S_OK;
	// No update?
	return S_FALSE;
}

// #5025
HRESULT WINAPI XLiveGetLiveIdError(HRESULT *phrAuthState, HRESULT *phrRequestState, LPWSTR wszWebFlowUrl, DWORD *pdwUrlLen)
{
	TRACE_FX();
	if (!phrAuthState) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phrAuthState is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!phrRequestState) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phrRequestState is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!wszWebFlowUrl) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s wszWebFlowUrl is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!pdwUrlLen) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwUrlLen is NULL.", __func__);
		return E_INVALIDARG;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	*phrAuthState = 0;
	*phrRequestState = 0;
	*wszWebFlowUrl = 0;
	*pdwUrlLen = 0;
	return S_OK;
	return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
	return E_UNEXPECTED;
}

// #5026
HRESULT WINAPI XLiveSetSponsorToken(LPCWSTR lpwszToken, DWORD dwTitleId)
{
	TRACE_FX();
	if (!lpwszToken) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s lpwszToken is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (wcsnlen_s(lpwszToken, 30) != 29) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (wcsnlen_s(lpwszToken, 30) != 29) Invalid string length.", __func__);
		return E_INVALIDARG;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return S_OK;
}

// #5027
HRESULT WINAPI XLiveUninstallTitle(DWORD dwTitleId)
{
	TRACE_FX();
	if (!dwTitleId) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwTitleId is 0.", __func__);
		return E_INVALIDARG;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return S_OK;
}

// #5028
DWORD WINAPI XLiveLoadLibraryEx(LPCWSTR lpwszModuleFileName, HINSTANCE *phModule, DWORD dwFlags)
{
	TRACE_FX();
	if (!lpwszModuleFileName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s lpwszModuleFileName is NULL.", __func__);
		return E_POINTER;
	}
	if (!*lpwszModuleFileName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *lpwszModuleFileName is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!phModule) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phModule is NULL.", __func__);
		return E_INVALIDARG;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
		, "XLiveLoadLibraryEx \"%ls\"."
		, lpwszModuleFileName
	);

	HINSTANCE hInstance = LoadLibraryExW(lpwszModuleFileName, NULL, dwFlags);
	if (!hInstance) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hInstance is NULL.", __func__);
		return E_INVALIDARG;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "XLiveLoadLibraryEx 0x%08x \"%ls\"."
		, *phModule
		, lpwszModuleFileName
	);

	*phModule = hInstance;
	return S_OK;
}

// #5029
HRESULT WINAPI XLiveFreeLibrary(HMODULE hModule)
{
	TRACE_FX();
	if (!hModule) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hModule is NULL.", __func__);
		return E_INVALIDARG;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "XLiveFreeLibrary 0x%08x."
		, hModule
	);

	if (!FreeLibrary(hModule)) {
		signed int last_error = GetLastError();
		if (last_error > 0) {
			last_error = (unsigned __int16)last_error | 0x80070000;
		}
		return last_error;
	}
	return S_OK;
}

// #5030
BOOL WINAPI XLivePreTranslateMessage(const LPMSG lpMsg)
{
	TRACE_FX();
	if (!lpMsg) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s lpMsg is NULL.", __func__);
		return FALSE;
	}
	return FALSE; // The message was not consumed. Process in the title.
	//return TRUE; // The message has been consumed.
}

// #5031
HRESULT WINAPI XLiveSetDebugLevel(XLIVE_DEBUG_LEVEL xdlLevel, XLIVE_DEBUG_LEVEL *pxdlOldLevel)
{
	TRACE_FX();
	if (xdlLevel != XLIVE_DEBUG_LEVEL_OFF && xdlLevel != XLIVE_DEBUG_LEVEL_ERROR && xdlLevel != XLIVE_DEBUG_LEVEL_WARNING && xdlLevel != XLIVE_DEBUG_LEVEL_INFO && xdlLevel != XLIVE_DEBUG_LEVEL_DEFAULT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xdlLevel value (0x%08x) does not exist.", __func__, xdlLevel);
		return E_INVALIDARG;
	}
	if (pxdlOldLevel) {
		*pxdlOldLevel = xlive_xdlLevel;
	}
	xlive_xdlLevel = xdlLevel;
	return S_OK;
}

// #5032
HRESULT WINAPI XLiveVerifyArcadeLicense(XLIVE_PROTECTED_BUFFER *xebBuffer, ULONG ulOffset)
{
	TRACE_FX();
	if (!xebBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xebBuffer is NULL.", __func__);
		return E_POINTER;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return S_OK;
}

// #5251
BOOL WINAPI XCloseHandle(HANDLE hObject)
{
	TRACE_FX();
	if (!hObject) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hObject is NULL.", __func__);
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	if (hObject == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hObject is INVALID_HANDLE_VALUE.", __func__);
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	bool foundEnumerator = false;
	
	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xlocator_enumerators);
		if (xlive_xlocator_enumerators.count(hObject)) {
			foundEnumerator = true;
			xlive_xlocator_enumerators.erase(hObject);
		}
		LeaveCriticalSection(&xlive_critsec_xlocator_enumerators);
	}
	
	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xuser_achievement_enumerators);
		if (xlive_xuser_achievement_enumerators.count(hObject)) {
			foundEnumerator = true;
			xlive_xuser_achievement_enumerators.erase(hObject);
		}
		LeaveCriticalSection(&xlive_critsec_xuser_achievement_enumerators);
	}
	
	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xuser_stats);
		if (xlive_xuser_achievement_enumerators.count(hObject)) {
			foundEnumerator = true;
			xlive_xuser_achievement_enumerators.erase(hObject);
		}
		LeaveCriticalSection(&xlive_critsec_xuser_stats);
	}

	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xfriends_enumerators);
		if (xlive_xfriends_enumerators.count(hObject)) {
			foundEnumerator = true;
			xlive_xfriends_enumerators.erase(hObject);
		}
		LeaveCriticalSection(&xlive_critsec_xfriends_enumerators);
	}

	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_title_server_enumerators);
		if (xlive_title_server_enumerators.count(hObject)) {
			foundEnumerator = true;
			xlive_title_server_enumerators.erase(hObject);
		}
		LeaveCriticalSection(&xlive_critsec_title_server_enumerators);
	}

	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_presence_enumerators);
		if (xlive_presence_enumerators.count(hObject)) {
			foundEnumerator = true;
			xlive_presence_enumerators.erase(hObject);
		}
		LeaveCriticalSection(&xlive_critsec_presence_enumerators);
	}

	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xmarketplace);
		if (xlive_xmarketplace_enumerators.count(hObject)) {
			foundEnumerator = true;
			xlive_xmarketplace_enumerators.erase(hObject);
		}
		LeaveCriticalSection(&xlive_critsec_xmarketplace);
	}

	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xcontent);
		if (xlive_xcontent_enumerators.count(hObject)) {
			foundEnumerator = true;
			xlive_xcontent_enumerators.erase(hObject);
		}
		LeaveCriticalSection(&xlive_critsec_xcontent);
	}

	if (!foundEnumerator) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s unknown handle 0x%08x.", __func__, (uint32_t)hObject);
	}

	if (!CloseHandle(hObject)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Failed to close handle %08x.", __func__, hObject);
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	return TRUE;
}

// #5254
DWORD WINAPI XCancelOverlapped(XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!pXOverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXOverlapped is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	SetLastError(ERROR_SUCCESS);
	return ERROR_SUCCESS;
}

// #5255
DWORD WINAPI XEnumerateBack(HANDLE hEnum, void *pvBuffer, DWORD cbBuffer, DWORD *pcItemsReturned, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!hEnum) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hEnum is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pvBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pvBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!cbBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cbBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (pcItemsReturned && pXOverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pcItemsReturned && pXOverlapped).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pcItemsReturned && !pXOverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!pcItemsReturned && !pXOverlapped).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	if (pcItemsReturned) {
		*pcItemsReturned = 0;
	}
	return ERROR_SUCCESS;
}

// #5256
DWORD WINAPI XEnumerate(HANDLE hEnum, void *pvBuffer, DWORD cbBuffer, DWORD *pcItemsReturned, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!hEnum) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hEnum is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pvBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pvBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!cbBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cbBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (pcItemsReturned && pXOverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pcItemsReturned && pXOverlapped).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pcItemsReturned && !pXOverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!pcItemsReturned && !pXOverlapped).", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	bool foundEnumerator = false;

	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xlocator_enumerators);
		if (xlive_xlocator_enumerators.count(hEnum)) {
			foundEnumerator = true;

			DWORD max_result_len = cbBuffer / sizeof(XLOCATOR_SEARCHRESULT);
			DWORD total_server_count = 0;

			EnterCriticalSection(&xlln_critsec_liveoverlan_sessions);
			for (auto const &session : liveoverlan_sessions) {
				if (total_server_count >= max_result_len) {
					break;
				}
				if (std::find(xlive_xlocator_enumerators[hEnum].begin(), xlive_xlocator_enumerators[hEnum].end(), session.first) != xlive_xlocator_enumerators[hEnum].end()) {
					continue;
				}
				XLOCATOR_SEARCHRESULT* server = &((XLOCATOR_SEARCHRESULT*)pvBuffer)[total_server_count++];
				xlive_xlocator_enumerators[hEnum].push_back(session.first);
				LiveOverLanClone(&server, session.second->searchresult);
			}
			LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
			LeaveCriticalSection(&xlive_critsec_xlocator_enumerators);

			if (pXOverlapped) {
				//pXOverlapped->InternalHigh = ERROR_IO_INCOMPLETE;
				//pXOverlapped->InternalLow = ERROR_IO_INCOMPLETE;
				//pXOverlapped->dwExtendedError = ERROR_SUCCESS;

				if (total_server_count) {
					pXOverlapped->InternalHigh = total_server_count;
					pXOverlapped->InternalLow = ERROR_SUCCESS;
				}
				else {
					pXOverlapped->InternalHigh = ERROR_SUCCESS;
					pXOverlapped->InternalLow = ERROR_NO_MORE_FILES;
					XCloseHandle(hEnum);
				}
				Check_Overlapped(pXOverlapped);

				return ERROR_IO_PENDING;
			}
			else {
				*pcItemsReturned = total_server_count;
				return ERROR_SUCCESS;
			}
		}
		LeaveCriticalSection(&xlive_critsec_xlocator_enumerators);
	}

	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xuser_achievement_enumerators);
		if (xlive_xuser_achievement_enumerators.count(hEnum)) {
			foundEnumerator = true;

			LeaveCriticalSection(&xlive_critsec_xuser_achievement_enumerators);

			if (pXOverlapped) {
				if (true) {
					pXOverlapped->InternalHigh = 0;
					pXOverlapped->InternalLow = ERROR_SUCCESS;
				}
				else {
					pXOverlapped->InternalHigh = ERROR_SUCCESS;
					pXOverlapped->InternalLow = ERROR_NO_MORE_FILES;
					XCloseHandle(hEnum);
				}
				Check_Overlapped(pXOverlapped);

				return ERROR_IO_PENDING;
			}
			else {
				*pcItemsReturned = 0;
				return ERROR_SUCCESS;
			}
		}
		LeaveCriticalSection(&xlive_critsec_xuser_achievement_enumerators);
	}

	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xuser_stats);
		if (xlive_xuser_achievement_enumerators.count(hEnum)) {
			foundEnumerator = true;

			LeaveCriticalSection(&xlive_critsec_xuser_stats);

			if (pXOverlapped) {
				if (true) {
					pXOverlapped->InternalHigh = 0;
					pXOverlapped->InternalLow = ERROR_SUCCESS;
				}
				else {
					pXOverlapped->InternalHigh = ERROR_SUCCESS;
					pXOverlapped->InternalLow = ERROR_NO_MORE_FILES;
					XCloseHandle(hEnum);
				}
				Check_Overlapped(pXOverlapped);

				return ERROR_IO_PENDING;
			}
			else {
				*pcItemsReturned = 0;
				return ERROR_SUCCESS;
			}
		}
		LeaveCriticalSection(&xlive_critsec_xuser_stats);
	}

	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xfriends_enumerators);
		if (xlive_xfriends_enumerators.count(hEnum)) {
			foundEnumerator = true;

			LeaveCriticalSection(&xlive_critsec_xfriends_enumerators);

			if (pXOverlapped) {
				if (true) {
					pXOverlapped->InternalHigh = 0;
					pXOverlapped->InternalLow = ERROR_SUCCESS;
				}
				else {
					pXOverlapped->InternalHigh = ERROR_SUCCESS;
					pXOverlapped->InternalLow = ERROR_NO_MORE_FILES;
					XCloseHandle(hEnum);
				}
				Check_Overlapped(pXOverlapped);

				return ERROR_IO_PENDING;
			}
			else {
				*pcItemsReturned = 0;
				return ERROR_SUCCESS;
			}
		}
		LeaveCriticalSection(&xlive_critsec_xfriends_enumerators);
	}

	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_title_server_enumerators);
		if (xlive_title_server_enumerators.count(hEnum)) {
			foundEnumerator = true;

			LeaveCriticalSection(&xlive_critsec_title_server_enumerators);

			if (pXOverlapped) {
				if (true) {
					pXOverlapped->InternalHigh = 0;
					pXOverlapped->InternalLow = ERROR_SUCCESS;
				}
				else {
					pXOverlapped->InternalHigh = ERROR_SUCCESS;
					pXOverlapped->InternalLow = ERROR_NO_MORE_FILES;
					XCloseHandle(hEnum);
				}
				Check_Overlapped(pXOverlapped);

				return ERROR_IO_PENDING;
			}
			else {
				*pcItemsReturned = 0;
				return ERROR_SUCCESS;
			}
		}
		LeaveCriticalSection(&xlive_critsec_title_server_enumerators);
	}

	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_presence_enumerators);
		if (xlive_presence_enumerators.count(hEnum)) {
			foundEnumerator = true;
			auto xuidSet = xlive_presence_enumerators[hEnum];
			uint32_t maxResults = cbBuffer / sizeof(XONLINE_PRESENCE);
			uint32_t numberOfResults = 0;

			while (xuidSet.size() && numberOfResults < maxResults) {
				XUID xuid = *xuidSet.begin();
				xuidSet.erase(xuid);

				XONLINE_PRESENCE* userPresence = &((XONLINE_PRESENCE*)pvBuffer)[numberOfResults++];
				userPresence->xuid = xuid;
				userPresence->dwState = XONLINE_FRIENDSTATE_ENUM_ONLINE | XONLINE_FRIENDSTATE_FLAG_ONLINE;
				userPresence->dwTitleID = DASHBOARD_TITLE_ID;

				memset(&userPresence->sessionID, 0, sizeof(userPresence->sessionID));

				const wchar_t rpText[] = L"Using XLiveLessNess";
				memcpy(userPresence->wszRichPresence, rpText, sizeof(rpText));
				userPresence->cchRichPresence = sizeof(rpText) / sizeof(wchar_t);

				SYSTEMTIME systemTime;
				GetSystemTime(&systemTime);
				FILETIME fileTime;
				SystemTimeToFileTime(&systemTime, &fileTime);
				userPresence->ftUserTime = fileTime;
			}

			LeaveCriticalSection(&xlive_critsec_presence_enumerators);

			if (pXOverlapped) {
				if (numberOfResults) {
					pXOverlapped->InternalHigh = numberOfResults;
					pXOverlapped->InternalLow = ERROR_SUCCESS;
				}
				else {
					pXOverlapped->InternalHigh = ERROR_SUCCESS;
					pXOverlapped->InternalLow = ERROR_NO_MORE_FILES;
					XCloseHandle(hEnum);
				}
				Check_Overlapped(pXOverlapped);

				return ERROR_IO_PENDING;
			}
			else {
				*pcItemsReturned = numberOfResults;
				return ERROR_SUCCESS;
			}
		}
		LeaveCriticalSection(&xlive_critsec_presence_enumerators);
	}

	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xmarketplace);
		if (xlive_xmarketplace_enumerators.count(hEnum)) {
			foundEnumerator = true;

			LeaveCriticalSection(&xlive_critsec_xmarketplace);

			if (pXOverlapped) {
				if (true) {
					pXOverlapped->InternalHigh = 0;
					pXOverlapped->InternalLow = ERROR_SUCCESS;
				}
				else {
					pXOverlapped->InternalHigh = ERROR_SUCCESS;
					pXOverlapped->InternalLow = ERROR_NO_MORE_FILES;
					XCloseHandle(hEnum);
				}
				Check_Overlapped(pXOverlapped);

				return ERROR_IO_PENDING;
			}
			else {
				*pcItemsReturned = 0;
				return ERROR_SUCCESS;
			}
		}
		LeaveCriticalSection(&xlive_critsec_xmarketplace);
	}

	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xcontent);
		if (xlive_xcontent_enumerators.count(hEnum)) {
			foundEnumerator = true;

			LeaveCriticalSection(&xlive_critsec_xcontent);

			if (pXOverlapped) {
				if (true) {
					pXOverlapped->InternalHigh = 0;
					pXOverlapped->InternalLow = ERROR_SUCCESS;
				}
				else {
					pXOverlapped->InternalHigh = ERROR_SUCCESS;
					pXOverlapped->InternalLow = ERROR_NO_MORE_FILES;
					XCloseHandle(hEnum);
				}
				Check_Overlapped(pXOverlapped);

				return ERROR_IO_PENDING;
			}
			else {
				*pcItemsReturned = 0;
				return ERROR_SUCCESS;
			}
		}
		LeaveCriticalSection(&xlive_critsec_xcontent);
	}

	if (!foundEnumerator) {
		if (pcItemsReturned) {
			*pcItemsReturned = 0;
		}
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s unknown enumerator handle.", __func__);
	}

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = ERROR_SUCCESS;
		pXOverlapped->InternalHigh = ERROR_SUCCESS;
		pXOverlapped->dwExtendedError = ERROR_SUCCESS;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return ERROR_SUCCESS;
}

// #5257
HRESULT WINAPI XLiveManageCredentials(LPCWSTR lpwszLiveIdName, LPCWSTR lpszLiveIdPassword, DWORD dwCredFlags, PXOVERLAPPED pXOverlapped)
{
	TRACE_FX();
	if (dwCredFlags & XLMGRCREDS_FLAG_SAVE && dwCredFlags & XLMGRCREDS_FLAG_DELETE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwCredFlags & XLMGRCREDS_FLAG_SAVE && dwCredFlags & XLMGRCREDS_FLAG_DELETE).", __func__);
		return E_INVALIDARG;
	}
	if (!(dwCredFlags & XLMGRCREDS_FLAG_SAVE) && !(dwCredFlags & XLMGRCREDS_FLAG_DELETE)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!(dwCredFlags & XLMGRCREDS_FLAG_SAVE) && !(dwCredFlags & XLMGRCREDS_FLAG_DELETE).", __func__);
		return E_INVALIDARG;
	}
	if (!lpwszLiveIdName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s lpwszLiveIdName is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!*lpwszLiveIdName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *lpwszLiveIdName is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (dwCredFlags & XLMGRCREDS_FLAG_SAVE) {
		if (!lpszLiveIdPassword) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwCredFlags & XLMGRCREDS_FLAG_SAVE) and lpszLiveIdPassword is NULL.", __func__);
			return E_INVALIDARG;
		}
		if (!*lpszLiveIdPassword) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwCredFlags & XLMGRCREDS_FLAG_SAVE) and *lpszLiveIdPassword is NULL.", __func__);
			return E_INVALIDARG;
		}
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = ERROR_SUCCESS;
		pXOverlapped->InternalHigh = ERROR_SUCCESS;
		pXOverlapped->dwExtendedError = ERROR_SUCCESS;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return S_OK;
}

// #5258
HRESULT WINAPI XLiveSignout(PXOVERLAPPED pXOverlapped)
{
	TRACE_FX();

	XLLNLogout(0);

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = ERROR_SUCCESS;
		pXOverlapped->InternalHigh = ERROR_SUCCESS;
		pXOverlapped->dwExtendedError = ERROR_SUCCESS;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return S_OK;
}

// #5259
HRESULT WINAPI XLiveSignin(PWSTR pszLiveIdName, PWSTR pszLiveIdPassword, DWORD dwFlags, PXOVERLAPPED pXOverlapped)
{
	TRACE_FX();
	if (!pszLiveIdName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pszLiveIdName is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!*pszLiveIdName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *pszLiveIdName is NULL.", __func__);
		return E_INVALIDARG;
	}
	//FIXME password isn't being passed in.
	//if (!pszLiveIdPassword || !*pszLiveIdPassword)
	//	return E_INVALIDARG;

	if (dwFlags & XLSIGNIN_FLAG_SAVECREDS) {

	}
	//XLSIGNIN_FLAG_ALLOWTITLEUPDATES XLSIGNIN_FLAG_ALLOWSYSTEMUPDATES

	size_t usernameSize = wcslen(pszLiveIdName) + 1;
	char *username = new char[usernameSize];
	wcstombs2(username, pszLiveIdName, usernameSize);
	ReplaceFilePathSensitiveChars(username);

	uint32_t result = XLLNLogin(0, TRUE, 0, username);
	if (result) {
		XLLN_DEBUG_LOG_ECODE(result, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLLNLogin(..., \"%s\") failed with error:", __func__, username);
		result = E_INVALIDARG;
	}

	delete[] username;

	if (result) {
		return result;
	}

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = ERROR_SUCCESS;
		pXOverlapped->InternalHigh = ERROR_SUCCESS;
		pXOverlapped->dwExtendedError = ERROR_SUCCESS;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return S_OK;
}

// #5270: Requires XNotifyGetNext to process the listener.
HANDLE WINAPI XNotifyCreateListener(ULONGLONG qwAreas)
{
	TRACE_FX();
	if (HIDWORD(qwAreas) | qwAreas & 0xFFFFFF10) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (HIDWORD(qwAreas) | qwAreas & 0xFFFFFF10).", __func__);
		return NULL;
	}

	HANDLE g_dwFakeListener = CreateMutex(NULL, NULL, NULL);

	g_listener[g_dwListener].id = g_dwFakeListener;
	g_listener[g_dwListener].area = qwAreas;
	g_dwListener++;

	SetEvent(g_dwFakeListener);
	return g_dwFakeListener;
}

// #5297
HRESULT WINAPI XLiveInitializeEx(XLIVE_INITIALIZE_INFO *pPii, DWORD dwTitleXLiveVersion)
{
	TRACE_FX();

	// if (IsDebuggerPresent())
	// return E_DEBUGGER_PRESENT;

	while (xlive_debug_pause && !IsDebuggerPresent()) {
		Sleep(500L);
	}

	srand((unsigned int)time(NULL));

	if (pPii->wLivePortOverride > 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO pPii->wLivePortOverride.", __func__);
	}

	EnterCriticalSection(&xlive_critsec_network_adapter);
	if (pPii->pszAdapterName && pPii->pszAdapterName[0]) {
		xlive_init_preferred_network_adapter_name = CloneString(pPii->pszAdapterName);
	}
	LeaveCriticalSection(&xlive_critsec_network_adapter);

	INT errorNetworkAdapter = RefreshNetworkAdapters();

	if (broadcastAddrInput) {
		char *temp = CloneString(broadcastAddrInput);
		ParseBroadcastAddrInput(temp);
		delete[] temp;
	}

	if (IsUsingBasePort(xlive_base_port)) {
		wchar_t mutex_name[40];
		DWORD mutex_last_error;
		HANDLE mutex = NULL;
		xlive_base_port -= 1000;
		do {
			if (mutex) {
				mutex_last_error = CloseHandle(mutex);
			}
			xlive_base_port += 1000;
			if (xlive_base_port > 65000) {
				xlive_netsocket_abort = TRUE;
				xlive_base_port = 1000;
				break;
			}
			swprintf(mutex_name, 40, L"Global\\XLiveLessNessBasePort#%hu", xlive_base_port);
			mutex = CreateMutexW(0, TRUE, mutex_name);
			mutex_last_error = GetLastError();
		} while (mutex_last_error != ERROR_SUCCESS);

		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_INFO
			, "XLive Base Port %hu."
			, xlive_base_port
		);
	}

	INT errorXSocket = InitXSocket();
	CreateLocalUser();
	INT errorNetEntity = InitNetEntity();
	//TODO If the title's graphics system has not yet been initialized, D3D will be passed in XLiveOnCreateDevice(...).
	INT errorXRender = InitXRender(pPii);
	INT errorXSession = InitXSession();

	xlive_initialised = TRUE;
	return S_OK;
}

// #5303
DWORD WINAPI XStringVerify(DWORD dwFlags, const CHAR *szLocale, DWORD dwNumStrings, const STRING_DATA *pStringData, DWORD cbResults, STRING_VERIFY_RESPONSE *pResults, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwFlags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwFlags is not officially implemented and should be NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!szLocale) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s szLocale is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (strlen(szLocale) >= XSTRING_MAX_LENGTH) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (strlen(szLocale) >= XSTRING_MAX_LENGTH) (%u >= %u).", __func__, (uint32_t)strlen(szLocale), XSTRING_MAX_LENGTH);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwNumStrings > XSTRING_MAX_STRINGS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwNumStrings > XSTRING_MAX_STRINGS) (%u > %u).", __func__, dwNumStrings, XSTRING_MAX_STRINGS);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pStringData) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pStringData is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pResults) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pResults is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	pResults->wNumStrings = (WORD)dwNumStrings;
	pResults->pStringResult = (HRESULT*)((BYTE*)pResults + sizeof(STRING_VERIFY_RESPONSE));

	for (int lcv = 0; lcv < (WORD)dwNumStrings; lcv++) {
		pResults->pStringResult[lcv] = S_OK;
	}
	
	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = ERROR_SUCCESS;
		pXOverlapped->InternalHigh = ERROR_SUCCESS;
		pXOverlapped->dwExtendedError = ERROR_SUCCESS;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return ERROR_SUCCESS;
}

// #5310
DWORD WINAPI XOnlineStartup()
{
	TRACE_FX();
	if (!xlive_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}

	WSADATA wsaData;
	DWORD result = XWSAStartup(2, &wsaData);
	xlive_online_initialized = (result == ERROR_SUCCESS);
	return result;
}

// #5311
DWORD WINAPI XOnlineCleanup()
{
	TRACE_FX();
	if (!xlive_online_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s WSANOTINITIALISED.", __func__);
		return WSANOTINITIALISED;
	}

	return XWSACleanup();
}

// #5312
DWORD WINAPI XFriendsCreateEnumerator(DWORD dwUserIndex, DWORD dwStartingIndex, DWORD dwFriendsToReturn, DWORD *pcbBuffer, HANDLE *ph)
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
	if (dwFriendsToReturn > XFRIENDS_MAX_C_RESULT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwFriendsToReturn > XFRIENDS_MAX_C_RESULT) (%u >= %u).", __func__, dwFriendsToReturn, XFRIENDS_MAX_C_RESULT);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwStartingIndex >= XFRIENDS_MAX_C_RESULT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwStartingIndex >= XFRIENDS_MAX_C_RESULT) (%u >= %u).", __func__, dwStartingIndex, XFRIENDS_MAX_C_RESULT);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwStartingIndex + dwFriendsToReturn < dwStartingIndex) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwStartingIndex + dwFriendsToReturn < dwStartingIndex) (%u + %u >= %u).", __func__, dwStartingIndex, dwFriendsToReturn, dwStartingIndex);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwStartingIndex + dwFriendsToReturn > XFRIENDS_MAX_C_RESULT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwStartingIndex + dwFriendsToReturn > XFRIENDS_MAX_C_RESULT) (%u + %u > %u).", __func__, dwStartingIndex, dwFriendsToReturn, XFRIENDS_MAX_C_RESULT);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pcbBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!ph) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ph is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	*pcbBuffer = dwFriendsToReturn * sizeof(XCONTENT_DATA);
	*ph = CreateMutex(NULL, NULL, NULL);
	EnterCriticalSection(&xlive_critsec_xfriends_enumerators);
	xlive_xfriends_enumerators[*ph];
	LeaveCriticalSection(&xlive_critsec_xfriends_enumerators);

	return ERROR_SUCCESS;
}

// #5315
DWORD WINAPI XInviteGetAcceptedInfo(DWORD dwUserIndex, XINVITE_INFO *pInfo)
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
	if (!pInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pInfo is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	if (xlive_invite_to_game) {
		xlive_invite_to_game = false;

		//pInfo->hostInfo.hostAddress.ina.s_addr = resolvedNetAddr;
		pInfo->hostInfo.hostAddress.wPortOnline = htons(2000);

		XUID host_xuid = 1234561000000032;
		pInfo->hostInfo.hostAddress.inaOnline.s_addr = 8192;

		DWORD user_id = host_xuid % 1000000000;
		DWORD mac_fix = 0x00131000;

		memset(&(pInfo->hostInfo.hostAddress.abEnet), 0, 6);
		memset(&(pInfo->hostInfo.hostAddress.abOnline), 0, 6);

		memcpy(&(pInfo->hostInfo.hostAddress.abEnet), &user_id, 4);
		memcpy(&(pInfo->hostInfo.hostAddress.abOnline), &user_id, 4);

		memcpy((BYTE*)&(pInfo->hostInfo.hostAddress.abEnet) + 3, (BYTE*)&mac_fix + 1, 3);
		memcpy((BYTE*)&(pInfo->hostInfo.hostAddress.abOnline) + 17, (BYTE*)&mac_fix + 1, 3);

		pInfo->fFromGameInvite = TRUE;
		pInfo->dwTitleID = 0x4D53080F;
		XNetCreateKey(&(pInfo->hostInfo.sessionID), &(pInfo->hostInfo.keyExchangeKey));
		pInfo->xuidInvitee = xlive_users_info[dwUserIndex]->xuid;
		pInfo->xuidInviter = host_xuid;

		return ERROR_SUCCESS;
	}
	return ERROR_FUNCTION_FAILED;
}

// #5316
DWORD XInviteSend(DWORD dwUserIndex, DWORD cInvitees, const XUID *pXuidInvitees, const WCHAR *pszText, XOVERLAPPED *pXOverlapped)
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
	if (!cInvitees) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cInvitees is 0.", __func__);
		return E_INVALIDARG;
	}
	if (cInvitees > 0x64) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cInvitees (%u) is greater than 0x64.", __func__, cInvitees);
		return E_INVALIDARG;
	}
	if (!pXuidInvitees) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXuidInvitees is NULL.", __func__);
		return E_POINTER;
	}
	if (pszText && wcsnlen_s(pszText, 0x100+1) > 0x100) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Length of pszText is > 0x100.", __func__);
		return E_INVALIDARG;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = ERROR_SUCCESS;
		pXOverlapped->InternalHigh = ERROR_SUCCESS;
		pXOverlapped->dwExtendedError = ERROR_SUCCESS;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return ERROR_SUCCESS;
}

// #5324
XONLINE_NAT_TYPE WINAPI XOnlineGetNatType()
{
	TRACE_FX();
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return XONLINE_NAT_OPEN;
}

// #5334
DWORD WINAPI XOnlineGetServiceInfo(DWORD dwServiceID, XONLINE_SERVICE_INFO *pServiceInfo)
{
	TRACE_FX();
	if (!pServiceInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pServiceInfo is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return ERROR_SERVICE_NOT_FOUND;
	return ERROR_SUCCESS;
	return ERROR_FUNCTION_FAILED;
	return ERROR_CONNECTION_INVALID;
}

// #5335
DWORD WINAPI XTitleServerCreateEnumerator(LPCSTR pszServerInfo, DWORD cItem, DWORD *pcbBuffer, HANDLE *phEnum)
{
	TRACE_FX();
	if (pszServerInfo && strnlen_s(pszServerInfo, XTITLE_SERVER_MAX_SERVER_INFO_SIZE) > XTITLE_SERVER_MAX_SERVER_INFO_LEN) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s length of pszServerInfo is > XTITLE_SERVER_MAX_SERVER_INFO_LEN (%u).", __func__, XTITLE_SERVER_MAX_SERVER_INFO_LEN);
		return ERROR_INVALID_PARAMETER;
	}
	if (!cItem) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cItem is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cItem > XTITLE_SERVER_MAX_LSP_INFO) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cItem (%u) is greater than XTITLE_SERVER_MAX_LSP_INFO (%u).", __func__, cItem, XTITLE_SERVER_MAX_LSP_INFO);
		return ERROR_INVALID_PARAMETER;
	}
	if (!phEnum) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phEnum is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	if (pcbBuffer) {
		*pcbBuffer = sizeof(XTITLE_SERVER_INFO) * cItem;
	}

	*phEnum = CreateMutex(NULL, NULL, NULL);
	EnterCriticalSection(&xlive_critsec_title_server_enumerators);
	xlive_title_server_enumerators[*phEnum];
	LeaveCriticalSection(&xlive_critsec_title_server_enumerators);

	return ERROR_SUCCESS;
}

// #5359
HRESULT WINAPI XLiveGetUPnPState(DWORD *peState)
{
	TRACE_FX();
	if (!peState) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s peState is NULL.", __func__);
		return E_INVALIDARG;
	}

	// FIXME not sure the type of this variable or the correct values.
	*peState = XONLINE_NAT_OPEN;

	return S_OK;
}
