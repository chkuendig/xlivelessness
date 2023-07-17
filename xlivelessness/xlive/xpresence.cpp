#include <winsock2.h>
#include "xdefs.hpp"
#include "xpresence.hpp"
#include "xlive.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/xlln.hpp"

// idk real macro name.
#define XPRESENCE_MAX_SUBSCRIPTIONS 400

CRITICAL_SECTION xlive_critsec_presence_enumerators;
static uint32_t MaxPeerSubscriptions = 0;
// Key: enumerator handle (id).
// Value: Set of XUIDs that need to be returned in the enumeration.
std::map<HANDLE, std::set<XUID>> xlive_presence_enumerators;
// FIXME the enumeration does not check if the XUIDs exist in this subscription list (if they are not already friends).
std::set<XUID> xlive_presence_subscriptions;

// #5313
DWORD WINAPI XPresenceInitialize(DWORD cPeerSubscriptions)
{
	TRACE_FX();
	if (!cPeerSubscriptions) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cPeerSubscriptions is 0.", __func__);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (cPeerSubscriptions > XPRESENCE_MAX_SUBSCRIPTIONS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (cPeerSubscriptions > XPRESENCE_MAX_SUBSCRIPTIONS) (%u > %u).", __func__, cPeerSubscriptions, XPRESENCE_MAX_SUBSCRIPTIONS);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}

	EnterCriticalSection(&xlive_critsec_presence_enumerators);
	if (MaxPeerSubscriptions) {
		LeaveCriticalSection(&xlive_critsec_presence_enumerators);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s already initialised.", __func__);
		SetLastError(E_ABORT);
		return ERROR_FUNCTION_FAILED;
	}
	MaxPeerSubscriptions = cPeerSubscriptions;
	LeaveCriticalSection(&xlive_critsec_presence_enumerators);

	return ERROR_SUCCESS;
}

// #5338
DWORD WINAPI XPresenceSubscribe(DWORD dwUserIndex, DWORD cPeers, const XUID *pPeers)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_SignedInToLive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in to LIVE.", __func__, dwUserIndex);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (!cPeers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cPeers is 0.", __func__);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (!pPeers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pPeers is NULL.", __func__);
		SetLastError(E_POINTER);
		return ERROR_FUNCTION_FAILED;
	}

	EnterCriticalSection(&xlive_critsec_presence_enumerators);
	if (!MaxPeerSubscriptions) {
		LeaveCriticalSection(&xlive_critsec_presence_enumerators);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XOnline not initialised.", __func__);
		SetLastError(XONLINE_E_NOT_INITIALIZED);
		return ERROR_FUNCTION_FAILED;
	}
	for (uint32_t iXuid = 0; iXuid < cPeers; iXuid++) {
		xlive_presence_subscriptions.insert(pPeers[iXuid]);
		if (xlive_presence_subscriptions.size() > MaxPeerSubscriptions) {
			xlive_presence_subscriptions.erase(pPeers[iXuid]);
			LeaveCriticalSection(&xlive_critsec_presence_enumerators);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s too many XUIDs were registered.", __func__);
			SetLastError(XONLINE_E_MATCH_TOO_MANY_USERS);
			return ERROR_FUNCTION_FAILED;
		}
	}
	LeaveCriticalSection(&xlive_critsec_presence_enumerators);

	return ERROR_SUCCESS;
}

// #5340
DWORD WINAPI XPresenceCreateEnumerator(DWORD dwUserIndex, DWORD cPeers, const XUID *pPeers, DWORD dwStartingIndex, DWORD dwPeersToReturn, DWORD *pcbBuffer, HANDLE *phEnum)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_SignedInToLive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in to LIVE.", __func__, dwUserIndex);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (!cPeers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cPeers is 0.", __func__);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (!pPeers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pPeers is NULL.", __func__);
		SetLastError(E_POINTER);
		return ERROR_FUNCTION_FAILED;
	}
	if (!dwPeersToReturn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwPeersToReturn is 0.", __func__);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (dwPeersToReturn > cPeers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwPeersToReturn > cPeers) (%u > %u).", __func__, dwPeersToReturn, cPeers);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (dwStartingIndex >= cPeers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwStartingIndex >= cPeers) (%u >= %u).", __func__, dwStartingIndex, cPeers);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (!pcbBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbBuffer is NULL.", __func__);
		SetLastError(E_POINTER);
		return ERROR_FUNCTION_FAILED;
	}
	if (!phEnum) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phEnum is NULL.", __func__);
		SetLastError(E_POINTER);
		return ERROR_FUNCTION_FAILED;
	}

	*pcbBuffer = sizeof(XONLINE_PRESENCE) * dwPeersToReturn;
	*phEnum = CreateMutex(NULL, NULL, NULL);

	EnterCriticalSection(&xlive_critsec_presence_enumerators);
	auto xuidSet = xlive_presence_enumerators[*phEnum];
	for (uint32_t iXuid = dwStartingIndex; iXuid < cPeers; iXuid++) {
		xuidSet.insert(pPeers[iXuid]);
	}
	LeaveCriticalSection(&xlive_critsec_presence_enumerators);

	return ERROR_SUCCESS;
}

// #5341
DWORD WINAPI XPresenceUnsubscribe(DWORD dwUserIndex, DWORD cPeers, const XUID *pPeers)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_SignedInToLive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in to LIVE.", __func__, dwUserIndex);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (!cPeers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cPeers is 0.", __func__);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (!pPeers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pPeers is NULL.", __func__);
		SetLastError(E_POINTER);
		return ERROR_FUNCTION_FAILED;
	}

	EnterCriticalSection(&xlive_critsec_presence_enumerators);
	if (!MaxPeerSubscriptions) {
		LeaveCriticalSection(&xlive_critsec_presence_enumerators);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XOnline not initialised.", __func__);
		SetLastError(XONLINE_E_NOT_INITIALIZED);
		return ERROR_FUNCTION_FAILED;
	}
	for (uint32_t iXuid = 0; iXuid < cPeers; iXuid++) {
		xlive_presence_subscriptions.erase(pPeers[iXuid]);
	}
	LeaveCriticalSection(&xlive_critsec_presence_enumerators);

	return ERROR_SUCCESS;
}
