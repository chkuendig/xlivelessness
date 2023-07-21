#include <winsock2.h>
#include "xdefs.hpp"
#include "xsession.hpp"
#include "live-over-lan.hpp"
#include "../xlln/debug-text.hpp"
#include "xlive.hpp"
#include "xnet.hpp"
#include "../xlln/xlln.hpp"
#include "net-entity.hpp"

CRITICAL_SECTION xlive_critsec_xsession;
// Key: session handle (id).
// Value: the details of the session.
std::map<HANDLE, LIVE_SESSION_XSESSION*> xlive_xsession_local_sessions;

BOOL xlive_xsession_initialised = FALSE;

INT InitXSession()
{
	TRACE_FX();
	if (xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is already initialised.", __func__);
		return E_UNEXPECTED;
	}

	xlive_xsession_initialised = TRUE;

	return S_OK;
}

INT UninitXSession()
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return E_UNEXPECTED;
	}

	xlive_xsession_initialised = FALSE;

	return S_OK;
}

// #5300
DWORD WINAPI XSessionCreate(DWORD dwFlags, DWORD dwUserIndex, DWORD dwMaxPublicSlots, DWORD dwMaxPrivateSlots, ULONGLONG *pqwSessionNonce, XSESSION_INFO *pSessionInfo, XOVERLAPPED *pXOverlapped, HANDLE *phEnum)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!pqwSessionNonce) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pqwSessionNonce is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pSessionInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pSessionInfo is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!phEnum) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phEnum is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	//FIXME unknown macro or their mistake?
	if (dwFlags & ~(XSESSION_CREATE_USES_MASK | XSESSION_CREATE_MODIFIERS_MASK | 0x1000)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s FIXME.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if ((dwFlags & XSESSION_CREATE_USES_MATCHMAKING) && !(dwFlags & XSESSION_CREATE_USES_PEER_NETWORK)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((dwFlags & XSESSION_CREATE_USES_MATCHMAKING) && !(dwFlags & XSESSION_CREATE_USES_PEER_NETWORK)).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if ((dwFlags & XSESSION_CREATE_USES_ARBITRATION) && !(dwFlags & XSESSION_CREATE_USES_STATS)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((dwFlags & XSESSION_CREATE_USES_ARBITRATION) && !(dwFlags & XSESSION_CREATE_USES_STATS)).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if ((dwFlags & XSESSION_CREATE_USES_ARBITRATION) && !(dwFlags & XSESSION_CREATE_USES_PEER_NETWORK)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((dwFlags & XSESSION_CREATE_USES_ARBITRATION) && !(dwFlags & XSESSION_CREATE_USES_PEER_NETWORK)).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if ((dwFlags & XSESSION_CREATE_HOST) && !(dwFlags & (XSESSION_CREATE_USES_PEER_NETWORK | XSESSION_CREATE_USES_STATS | XSESSION_CREATE_USES_MATCHMAKING))) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((dwFlags & XSESSION_CREATE_HOST) && !(dwFlags & (XSESSION_CREATE_USES_PEER_NETWORK | XSESSION_CREATE_USES_STATS | XSESSION_CREATE_USES_MATCHMAKING))).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwFlags & XSESSION_CREATE_MODIFIERS_MASK) {
		if (!(dwFlags & (XSESSION_CREATE_USES_PRESENCE | XSESSION_CREATE_USES_MATCHMAKING))) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((dwFlags & XSESSION_CREATE_MODIFIERS_MASK) && !(dwFlags & (XSESSION_CREATE_USES_PRESENCE | XSESSION_CREATE_USES_MATCHMAKING))).", __func__);
			return ERROR_INVALID_PARAMETER;
		}
		if (!(dwFlags & XSESSION_CREATE_USES_PRESENCE) && (dwFlags & XSESSION_CREATE_USES_MATCHMAKING) && (dwFlags & XSESSION_CREATE_MODIFIERS_MASK) != (dwFlags & XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((dwFlags & XSESSION_CREATE_MODIFIERS_MASK) && !(dwFlags & XSESSION_CREATE_USES_PRESENCE) && (dwFlags & XSESSION_CREATE_USES_MATCHMAKING) && (dwFlags & XSESSION_CREATE_MODIFIERS_MASK) != (dwFlags & XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED)).", __func__);
			return ERROR_INVALID_PARAMETER;
		}
		if ((dwFlags & XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED) && (dwFlags & XSESSION_CREATE_JOIN_VIA_PRESENCE_FRIENDS_ONLY)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((dwFlags & XSESSION_CREATE_MODIFIERS_MASK) && (dwFlags & XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED) && (dwFlags & XSESSION_CREATE_JOIN_VIA_PRESENCE_FRIENDS_ONLY)).", __func__);
			return ERROR_INVALID_PARAMETER;
		}
	}
	
	LIVE_SESSION_XSESSION *xsessionDetails = new LIVE_SESSION_XSESSION;
	xsessionDetails->liveSession = new LIVE_SESSION;
	xsessionDetails->liveSession->sessionType = XLLN_LIVEOVERLAN_SESSION_TYPE_XSESSION;

	if (dwFlags & XSESSION_CREATE_HOST) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
			, "%s User %u is hosting a session."
			, __func__
			, dwUserIndex
		);
		
		xsessionDetails->liveSession->xuid = xlive_users_info[dwUserIndex]->xuid;
		xsessionDetails->qwNonce = xlive_users_info[dwUserIndex]->xuid;
		
		xsessionDetails->xnAddr = xlive_local_xnAddr;
		
		// TODO XNKEY
		memset(&xsessionDetails->liveSession->xnkey, 0xAA, sizeof(XNKEY));
		
		// TODO XNKID
		memset(&xsessionDetails->liveSession->xnkid, 0x8B, sizeof(XNKID));
		xsessionDetails->liveSession->xnkid.ab[0] &= ~XNET_XNKID_MASK;
		xsessionDetails->liveSession->xnkid.ab[0] |= XNET_XNKID_ONLINE_PEER;
		
		pSessionInfo->hostAddress = xsessionDetails->xnAddr;
		pSessionInfo->keyExchangeKey = xsessionDetails->liveSession->xnkey;
		pSessionInfo->sessionID = xsessionDetails->liveSession->xnkid;
		
		*pqwSessionNonce = xsessionDetails->qwNonce;

		std::map<uint32_t, XUSER_PROPERTY*> xuserProperties;

		if (!xuserProperties.count(XUSER_PROPERTY_GAMERHOSTNAME)) {
			XUSER_PROPERTY *property = new XUSER_PROPERTY;
			property->dwPropertyId = XUSER_PROPERTY_GAMERHOSTNAME;
			property->value.type = XUSER_DATA_TYPE_UNICODE;
			uint32_t usernameLenSize = strnlen(xlive_users_info[dwUserIndex]->szUserName, XUSER_MAX_NAME_LENGTH) + 1;
			property->value.string.cbData = usernameLenSize * sizeof(wchar_t);
			property->value.string.pwszData = new wchar_t[usernameLenSize];
			swprintf_s(property->value.string.pwszData, usernameLenSize, L"%hs", xlive_users_info[dwUserIndex]->szUserName);

			xuserProperties[XUSER_PROPERTY_GAMERHOSTNAME] = property;
		}

		xsessionDetails->liveSession->propertiesCount = xuserProperties.size();
		xsessionDetails->liveSession->pProperties = new XUSER_PROPERTY[xsessionDetails->liveSession->propertiesCount];
		{
			uint32_t iProperty = 0;
			for (auto const &entry : xuserProperties) {
				XUSER_PROPERTY *propertyListCopy = entry.second;
				XUSER_PROPERTY &propertyCopy = xsessionDetails->liveSession->pProperties[iProperty++];
				memcpy(&propertyCopy, propertyListCopy, sizeof(propertyCopy));
				delete propertyListCopy;
			}
		}
		xuserProperties.clear();

	}
	else {
		xsessionDetails->liveSession->xuid = INVALID_XUID;
		
		xsessionDetails->xnAddr = pSessionInfo->hostAddress;
		xsessionDetails->liveSession->xnkey = pSessionInfo->keyExchangeKey;
		xsessionDetails->liveSession->xnkid = pSessionInfo->sessionID;
		
		xsessionDetails->qwNonce = *pqwSessionNonce;
	}

	// already filled - SetContext
	//xsessionDetails->dwGameType = 0;
	//xsessionDetails->dwGameMode = 0;
	
	memset(&xsessionDetails->xnkidArbitration, 0x8B, sizeof(XNKID));
	
	xsessionDetails->liveSession->sessionFlags = dwFlags;

	xsessionDetails->liveSession->slotsPublicMaxCount = dwMaxPublicSlots;
	xsessionDetails->liveSession->slotsPrivateMaxCount = dwMaxPrivateSlots;
	xsessionDetails->liveSession->slotsPublicFilledCount = 0;
	xsessionDetails->liveSession->slotsPrivateFilledCount = 0;

	xsessionDetails->dwActualMemberCount = 0;
	xsessionDetails->dwReturnedMemberCount = 0;

	uint32_t maxMembers = xsessionDetails->liveSession->slotsPublicMaxCount > xsessionDetails->liveSession->slotsPrivateMaxCount ? xsessionDetails->liveSession->slotsPublicMaxCount : xsessionDetails->liveSession->slotsPrivateMaxCount;
	xsessionDetails->pSessionMembers = new XSESSION_MEMBER[maxMembers];

	for (uint32_t i = 0; i < maxMembers; i++) {
		xsessionDetails->pSessionMembers[i].xuidOnline = INVALID_XUID;
		xsessionDetails->pSessionMembers[i].dwUserIndex = XLIVE_LOCAL_USER_INVALID;
		xsessionDetails->pSessionMembers[i].dwFlags = 0;
	}

	*phEnum = CreateMutex(NULL, NULL, NULL);
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		xlive_xsession_local_sessions[*phEnum] = xsessionDetails;
		LeaveCriticalSection(&xlive_critsec_xsession);
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

// #5317
DWORD WINAPI XSessionWriteStats(HANDLE hSession, XUID xuid, DWORD dwNumViews, CONST XSESSION_VIEW_PROPERTIES *pViews, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!dwNumViews) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumViews is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pViews) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pViews is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
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

// #5318
DWORD WINAPI XSessionStart(HANDLE hSession, DWORD dwFlags, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (dwFlags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwFlags (0x%08x) is not 0.", __func__, dwFlags);
		return ERROR_INVALID_PARAMETER;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t result = ERROR_SUCCESS;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		if (xlive_xsession_local_sessions.count(hSession)) {
			LIVE_SESSION_XSESSION *xsessionDetails = xlive_xsession_local_sessions[hSession];

			xsessionDetails->eState = XSESSION_STATE_INGAME;
		}
		else {
			result = XONLINE_E_SESSION_NOT_FOUND;
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = result;
		pXOverlapped->InternalHigh = result;
		pXOverlapped->dwExtendedError = result;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return result;
}

// #5319
DWORD WINAPI XSessionSearchEx(
	DWORD dwProcedureIndex,
	DWORD dwUserIndex,
	DWORD dwNumResults,
	DWORD dwNumUsers,
	WORD wNumProperties,
	WORD wNumContexts,
	XUSER_PROPERTY *pSearchProperties,
	XUSER_CONTEXT *pSearchContexts,
	DWORD *pcbResultsBuffer,
	XSESSION_SEARCHRESULT_HEADER *pSearchResults,
	XOVERLAPPED *pXOverlapped
)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!dwNumResults) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumResults is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!dwNumUsers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumUsers is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (wNumProperties) {
		if (!pSearchProperties) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pSearchProperties is NULL.", __func__);
			return ERROR_INVALID_PARAMETER;
		}
		if (wNumProperties > 0x40) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (wNumProperties > 0x40) (0x%08x > 0x40).", __func__, wNumProperties);
			return ERROR_INVALID_PARAMETER;
		}
	}
	else if (pSearchProperties) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pSearchProperties is not NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (wNumContexts) {
		if (!pSearchContexts) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pSearchContexts is NULL.", __func__);
			return ERROR_INVALID_PARAMETER;
		}
	}
	else if (pSearchContexts) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pSearchContexts is not NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pcbResultsBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbResultsBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (pSearchResults && *pcbResultsBuffer >= sizeof(XSESSION_SEARCHRESULT_HEADER)) {
		pSearchResults->dwSearchResults = 0;
		pSearchResults->pResults = 0;
	}
	
	{
		EnterCriticalSection(&xlln_critsec_liveoverlan_sessions);
		
		// Calculate the space required.
		uint32_t bufferSizeRequired = sizeof(XSESSION_SEARCHRESULT_HEADER);
		uint32_t searchResultCount = 0;
		for (auto const &session : liveoverlan_remote_sessions_xsession) {
			
			// Ensure this is an XSession item.
			if (session.second->liveSession->sessionType != XLLN_LIVEOVERLAN_SESSION_TYPE_XSESSION) {
				continue;
			}
			
			// TODO search criteria.
			
			searchResultCount++;
			bufferSizeRequired += sizeof(XSESSION_SEARCHRESULT);
			bufferSizeRequired += session.second->liveSession->contextsCount * sizeof(*session.second->liveSession->pContexts);
			bufferSizeRequired += session.second->liveSession->propertiesCount * sizeof(*session.second->liveSession->pProperties);
			for (uint32_t iProperty = 0; iProperty < session.second->liveSession->propertiesCount; iProperty++) {
				XUSER_PROPERTY &property = session.second->liveSession->pProperties[iProperty];
				switch (property.value.type) {
					case XUSER_DATA_TYPE_BINARY: {
						bufferSizeRequired += property.value.binary.cbData;
						break;
					}
					case XUSER_DATA_TYPE_UNICODE: {
						bufferSizeRequired += property.value.string.cbData;
						break;
					}
				}
			}
		}
		
		if (*pcbResultsBuffer < bufferSizeRequired || !pSearchResults) {
			*pcbResultsBuffer = bufferSizeRequired;
			LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
			return ERROR_INSUFFICIENT_BUFFER;
		}
		
		if (searchResultCount > 0) {
			pSearchResults->pResults = (XSESSION_SEARCHRESULT*)&((uint8_t*)pSearchResults)[sizeof(XSESSION_SEARCHRESULT_HEADER)];
			uint8_t *searchResultsData = &((uint8_t*)pSearchResults->pResults)[searchResultCount * sizeof(*pSearchResults->pResults)];
			
			uint32_t iSearchResult = 0;
			for (auto const &session : liveoverlan_remote_sessions_xsession) {
				// Ensure this is an XSession item.
				if (session.second->liveSession->sessionType != XLLN_LIVEOVERLAN_SESSION_TYPE_XSESSION) {
					continue;
				}
				
				// TODO search criteria.
				
				// Copy over all the memory into the buffer.
				XSESSION_SEARCHRESULT &searchResult = pSearchResults->pResults[iSearchResult++];
				searchResult.info.sessionID = session.second->liveSession->xnkid;
				searchResult.info.hostAddress = session.second->xnAddr;
				searchResult.info.keyExchangeKey = session.second->liveSession->xnkey;
				searchResult.dwOpenPublicSlots = session.second->liveSession->slotsPublicMaxCount;
				searchResult.dwOpenPrivateSlots = session.second->liveSession->slotsPrivateMaxCount;
				searchResult.dwFilledPublicSlots = session.second->liveSession->slotsPublicFilledCount;
				searchResult.dwFilledPrivateSlots = session.second->liveSession->slotsPrivateFilledCount;

				searchResult.cContexts = session.second->liveSession->contextsCount;
				if (searchResult.cContexts == 0) {
					searchResult.pContexts = 0;
				}
				else {
					searchResult.pContexts = (XUSER_CONTEXT*)searchResultsData;
					uint32_t contextsSize = searchResult.cContexts * sizeof(*searchResult.pContexts);
					searchResultsData += contextsSize;
					memcpy(searchResult.pContexts, session.second->liveSession->pContexts, contextsSize);
				}

				searchResult.cProperties = session.second->liveSession->propertiesCount;
				if (searchResult.cProperties == 0) {
					searchResult.pProperties = 0;
				}
				else {
					searchResult.pProperties = (XUSER_PROPERTY*)searchResultsData;
					uint32_t propertiesSize = searchResult.cProperties * sizeof(*searchResult.pProperties);
					searchResultsData += propertiesSize;
					memcpy(searchResult.pProperties, session.second->liveSession->pProperties, propertiesSize);

					for (uint32_t iProperty = 0; iProperty < searchResult.cProperties; iProperty++) {
						XUSER_PROPERTY &propertyOrig = session.second->liveSession->pProperties[iProperty];
						XUSER_PROPERTY &propertyCopy = searchResult.pProperties[iProperty];
						switch (propertyCopy.value.type) {
							case XUSER_DATA_TYPE_BINARY: {
								propertyCopy.value.binary.pbData = searchResultsData;
								memcpy(propertyCopy.value.binary.pbData, propertyOrig.value.binary.pbData, propertyCopy.value.binary.cbData);
								searchResultsData += propertyCopy.value.binary.cbData;
								break;
							}
							case XUSER_DATA_TYPE_UNICODE: {
								propertyCopy.value.string.pwszData = (wchar_t*)searchResultsData;
								memcpy(propertyCopy.value.string.pwszData, propertyOrig.value.string.pwszData, propertyCopy.value.string.cbData);
								searchResultsData += propertyCopy.value.string.cbData;
								break;
							}
						}
					}
				}
			}
			
			if (searchResultsData != &((uint8_t*)pSearchResults)[bufferSizeRequired]) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_FATAL
					, "%s the end result of searchResultData (0x%08x) should not be different from pSearchResults (0x%08x) plus bufferSizeRequired (0x%08x) giving 0x%08x."
					, __func__
					, searchResultsData
					, (uint8_t*)pSearchResults
					, bufferSizeRequired
					, &((uint8_t*)pSearchResults)[bufferSizeRequired]
				);
				__debugbreak();
				LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
				return ERROR_FATAL_APP_EXIT;
			}

			pSearchResults->dwSearchResults = searchResultCount;
		}
		
		LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
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

// #5320
DWORD WINAPI XSessionSearchByID(XNKID sessionID, DWORD dwUserIndex, DWORD *pcbResultsBuffer, XSESSION_SEARCHRESULT_HEADER *pSearchResults, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!pcbResultsBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbResultsBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	if (*pcbResultsBuffer < 0x536 || !pSearchResults) {
		*pcbResultsBuffer = 0x536;
		return ERROR_INSUFFICIENT_BUFFER;
	}

	pSearchResults->dwSearchResults = 0;
	pSearchResults->pResults = 0;

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

// #5321
DWORD WINAPI XSessionSearch(
	DWORD dwProcedureIndex,
	DWORD dwUserIndex,
	DWORD dwNumResults,
	WORD wNumProperties,
	WORD wNumContexts,
	XUSER_PROPERTY *pSearchProperties,
	XUSER_CONTEXT *pSearchContexts,
	DWORD *pcbResultsBuffer,
	XSESSION_SEARCHRESULT_HEADER *pSearchResults,
	XOVERLAPPED *pXOverlapped
)
{
	TRACE_FX();
	return XSessionSearchEx(dwProcedureIndex, dwUserIndex, dwNumResults, 1, wNumProperties, wNumContexts, pSearchProperties, pSearchContexts, pcbResultsBuffer, pSearchResults, pXOverlapped);
}

// #5322
DWORD WINAPI XSessionModify(HANDLE hSession, DWORD dwFlags, DWORD dwMaxPublicSlots, DWORD dwMaxPrivateSlots, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if ((dwFlags & XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED) && (dwFlags & XSESSION_CREATE_JOIN_VIA_PRESENCE_FRIENDS_ONLY)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwFlags cannot set XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED and XSESSION_CREATE_JOIN_VIA_PRESENCE_FRIENDS_ONLY.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t result = ERROR_SUCCESS;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		if (xlive_xsession_local_sessions.count(hSession)) {
			LIVE_SESSION_XSESSION *xsessionDetails = xlive_xsession_local_sessions[hSession];
			
			const DWORD maskModifiers = XSESSION_CREATE_MODIFIERS_MASK - XSESSION_CREATE_SOCIAL_MATCHMAKING_ALLOWED; // not sure if social property is modifiable.
			xsessionDetails->liveSession->sessionFlags = (xsessionDetails->liveSession->sessionFlags & (~maskModifiers)) | (maskModifiers & dwFlags);
			
			uint32_t maxMembersBefore = xsessionDetails->liveSession->slotsPublicMaxCount > xsessionDetails->liveSession->slotsPrivateMaxCount ? xsessionDetails->liveSession->slotsPublicMaxCount : xsessionDetails->liveSession->slotsPrivateMaxCount;
			uint32_t maxMembersAfter = dwMaxPublicSlots > dwMaxPrivateSlots ? dwMaxPublicSlots : dwMaxPrivateSlots;
			
			if (maxMembersAfter < maxMembersBefore) {
				// TODO and if the number of INVALID_XUID dont make up for it then error.
				result = XONLINE_E_SESSION_FULL;
			}
			else {
				xsessionDetails->liveSession->slotsPublicMaxCount = dwMaxPublicSlots;
				xsessionDetails->liveSession->slotsPrivateMaxCount = dwMaxPrivateSlots;
				
				XSESSION_MEMBER *sessionMembersBefore = xsessionDetails->pSessionMembers;
				xsessionDetails->pSessionMembers = new XSESSION_MEMBER[maxMembersAfter];
				
				for (uint32_t i = 0; i < maxMembersBefore; i++) {
					xsessionDetails->pSessionMembers[i].xuidOnline = sessionMembersBefore[i].xuidOnline;
					xsessionDetails->pSessionMembers[i].dwUserIndex = sessionMembersBefore[i].dwUserIndex;
					xsessionDetails->pSessionMembers[i].dwFlags = sessionMembersBefore[i].dwFlags;
				}
				for (uint32_t i = maxMembersBefore; i < maxMembersAfter; i++) {
					xsessionDetails->pSessionMembers[i].xuidOnline = INVALID_XUID;
					xsessionDetails->pSessionMembers[i].dwUserIndex = XLIVE_LOCAL_USER_INVALID;
					xsessionDetails->pSessionMembers[i].dwFlags = 0;
				}
				
				delete[] sessionMembersBefore;
			}
		}
		else {
			result = XONLINE_E_SESSION_NOT_FOUND;
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = result;
		pXOverlapped->InternalHigh = result;
		pXOverlapped->dwExtendedError = result;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return result;
}

// #5323
DWORD WINAPI XSessionMigrateHost(HANDLE hSession, DWORD dwUserIndex, XSESSION_INFO *pSessionInfo, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!pSessionInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pSessionInfo is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t result = ERROR_FUNCTION_FAILED;

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = result;
		pXOverlapped->InternalHigh = result;
		pXOverlapped->dwExtendedError = result;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return result;
}

// #5325
DWORD WINAPI XSessionLeaveLocal(HANDLE hSession, DWORD dwUserCount, const DWORD *pdwUserIndexes, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwUserCount == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwUserCount is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwUserCount > XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwUserCount (%u) is greater than XLIVE_LOCAL_USER_COUNT (%u).", __func__, dwUserCount, XLIVE_LOCAL_USER_COUNT);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pdwUserIndexes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwUserIndexes is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
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

// #5326
DWORD WINAPI XSessionJoinRemote(HANDLE hSession, DWORD dwXuidCount, const XUID *pXuids, const BOOL *pfPrivateSlots, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwXuidCount == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwXuidCount is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pXuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXuids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pfPrivateSlots) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pfPrivateSlots is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t result = ERROR_SUCCESS;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		if (xlive_xsession_local_sessions.count(hSession)) {
			LIVE_SESSION_XSESSION *xsessionDetails = xlive_xsession_local_sessions[hSession];

			uint32_t requestedSlotCountPublic = 0;
			uint32_t requestedSlotCountPrivate = 0;
			for (uint32_t iUser = 0; iUser < dwXuidCount; iUser++) {
				if (pfPrivateSlots[iUser]) {
					requestedSlotCountPrivate++;
				}
				else {
					requestedSlotCountPublic++;
				}
			}

			if (requestedSlotCountPrivate > xsessionDetails->liveSession->slotsPrivateMaxCount - xsessionDetails->liveSession->slotsPrivateFilledCount) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Not enough private slots are available in session 0x%08x. (need > available) (%u > %u - %u)"
					, __func__
					, hSession
					, requestedSlotCountPrivate
					, xsessionDetails->liveSession->slotsPrivateMaxCount
					, xsessionDetails->liveSession->slotsPrivateFilledCount
				);
				result = XONLINE_E_SESSION_JOIN_ILLEGAL;
			}
			else if (requestedSlotCountPublic > xsessionDetails->liveSession->slotsPublicMaxCount - xsessionDetails->liveSession->slotsPublicFilledCount) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Not enough public slots are available in session 0x%08x. (need > available) (%u > %u - %u)"
					, __func__
					, hSession
					, requestedSlotCountPublic
					, xsessionDetails->liveSession->slotsPublicMaxCount
					, xsessionDetails->liveSession->slotsPublicFilledCount
				);
				result = XONLINE_E_SESSION_JOIN_ILLEGAL;
			}
			else {
				uint32_t maxMembers = xsessionDetails->liveSession->slotsPublicMaxCount > xsessionDetails->liveSession->slotsPrivateMaxCount ? xsessionDetails->liveSession->slotsPublicMaxCount : xsessionDetails->liveSession->slotsPrivateMaxCount;

				for (uint32_t iXuid = 0; iXuid < dwXuidCount; iXuid++) {
					const XUID &remoteXuid = pXuids[iXuid];
					uint32_t iMemberEmpty = maxMembers;
					uint32_t iMember = 0;
					for (; iMember < maxMembers; iMember++) {
						if (xsessionDetails->pSessionMembers[iMember].xuidOnline == remoteXuid) {
							break;
						}
						if (xsessionDetails->pSessionMembers[iMember].dwUserIndex == XLIVE_LOCAL_USER_INVALID && xsessionDetails->pSessionMembers[iMember].xuidOnline == INVALID_XUID) {
							iMemberEmpty = iMember;
						}
					}
					if (iMemberEmpty == maxMembers && iMember == maxMembers) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s No more member slots are available.", __func__);
						result = XONLINE_E_SESSION_JOIN_ILLEGAL;
						break;
					}
					else if (iMember == maxMembers) {
						xsessionDetails->pSessionMembers[iMemberEmpty].xuidOnline = remoteXuid;
						xsessionDetails->pSessionMembers[iMemberEmpty].dwUserIndex = XLIVE_LOCAL_USER_INVALID;
						xsessionDetails->pSessionMembers[iMemberEmpty].dwFlags = 0;
						if (pfPrivateSlots[iXuid]) {
							xsessionDetails->liveSession->slotsPrivateFilledCount++;
							xsessionDetails->pSessionMembers[iMemberEmpty].dwFlags = XSESSION_MEMBER_FLAGS_PRIVATE_SLOT;
						}
						else {
							xsessionDetails->liveSession->slotsPublicFilledCount++;
						}
						xsessionDetails->dwActualMemberCount++;
						xsessionDetails->dwReturnedMemberCount++;
					}
					else {
						xsessionDetails->pSessionMembers[iMember].xuidOnline = remoteXuid;
						xsessionDetails->pSessionMembers[iMember].dwUserIndex = XLIVE_LOCAL_USER_INVALID;
						if (xsessionDetails->pSessionMembers[iMember].dwFlags & XSESSION_MEMBER_FLAGS_ZOMBIE) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "%s Remote XUID 0x%016I64x was previously in the session.", __func__, remoteXuid);
						}
						else {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Remote XUID 0x%016I64x is already in the session.", __func__, remoteXuid);
						}
						if (xsessionDetails->pSessionMembers[iMember].dwFlags & XSESSION_MEMBER_FLAGS_PRIVATE_SLOT) {
							xsessionDetails->liveSession->slotsPrivateFilledCount--;
						}
						else {
							xsessionDetails->liveSession->slotsPublicFilledCount--;
						}
						xsessionDetails->pSessionMembers[iMember].dwFlags = 0;
						if (pfPrivateSlots[iXuid]) {
							xsessionDetails->liveSession->slotsPrivateFilledCount++;
							xsessionDetails->pSessionMembers[iMemberEmpty].dwFlags = XSESSION_MEMBER_FLAGS_PRIVATE_SLOT;
						}
						else {
							xsessionDetails->liveSession->slotsPublicFilledCount++;
						}
					}
				}
			}
		}
		else {
			result = XONLINE_E_SESSION_JOIN_ILLEGAL;
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = result;
		pXOverlapped->InternalHigh = result;
		pXOverlapped->dwExtendedError = result;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return result;
}

// #5327
DWORD WINAPI XSessionJoinLocal(HANDLE hSession, DWORD dwUserCount, const DWORD *pdwUserIndexes, const BOOL *pfPrivateSlots, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwUserCount == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwUserCount is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwUserCount > XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwUserCount (%u) is greater than XLIVE_LOCAL_USER_COUNT (%u).", __func__, dwUserCount, XLIVE_LOCAL_USER_COUNT);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pdwUserIndexes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwUserIndexes is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pfPrivateSlots) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pfPrivateSlots is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t result = ERROR_SUCCESS;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		if (xlive_xsession_local_sessions.count(hSession)) {
			LIVE_SESSION_XSESSION *xsessionDetails = xlive_xsession_local_sessions[hSession];

			uint32_t requestedSlotCountPublic = 0;
			uint32_t requestedSlotCountPrivate = 0;
			for (uint32_t iUser = 0; iUser < dwUserCount; iUser++) {
				if (pfPrivateSlots[iUser]) {
					requestedSlotCountPrivate++;
				}
				else {
					requestedSlotCountPublic++;
				}
			}

			if (requestedSlotCountPrivate > xsessionDetails->liveSession->slotsPrivateMaxCount - xsessionDetails->liveSession->slotsPrivateFilledCount) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Not enough private slots are available in session 0x%08x. (need > available) (%u > %u - %u)"
					, __func__
					, hSession
					, requestedSlotCountPrivate
					, xsessionDetails->liveSession->slotsPrivateMaxCount
					, xsessionDetails->liveSession->slotsPrivateFilledCount
				);
				result = XONLINE_E_SESSION_JOIN_ILLEGAL;
			}
			else if (requestedSlotCountPublic > xsessionDetails->liveSession->slotsPublicMaxCount - xsessionDetails->liveSession->slotsPublicFilledCount) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Not enough public slots are available in session 0x%08x. (need > available) (%u > %u - %u)"
					, __func__
					, hSession
					, requestedSlotCountPublic
					, xsessionDetails->liveSession->slotsPublicMaxCount
					, xsessionDetails->liveSession->slotsPublicFilledCount
				);
				result = XONLINE_E_SESSION_JOIN_ILLEGAL;
			}
			else {
				uint32_t maxMembers = xsessionDetails->liveSession->slotsPublicMaxCount > xsessionDetails->liveSession->slotsPrivateMaxCount ? xsessionDetails->liveSession->slotsPublicMaxCount : xsessionDetails->liveSession->slotsPrivateMaxCount;

				for (uint32_t iUser = 0; iUser < dwUserCount; iUser++) {
					const uint32_t &localUserIndex = pdwUserIndexes[iUser];
					uint32_t iMemberEmpty = maxMembers;
					uint32_t iMember = 0;
					for (; iMember < maxMembers; iMember++) {
						if (xsessionDetails->pSessionMembers[iMember].dwUserIndex == localUserIndex) {
							break;
						}
						if (xsessionDetails->pSessionMembers[iMember].dwUserIndex == XLIVE_LOCAL_USER_INVALID && xsessionDetails->pSessionMembers[iMember].xuidOnline == INVALID_XUID) {
							iMemberEmpty = iMember;
						}
					}
					if (iMemberEmpty == maxMembers && iMember == maxMembers) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s No more member slots are available.", __func__);
						result = XONLINE_E_SESSION_JOIN_ILLEGAL;
						break;
					}
					else if (iMember == maxMembers) {
						xsessionDetails->pSessionMembers[iMemberEmpty].xuidOnline = xlive_users_info[localUserIndex]->xuid;
						xsessionDetails->pSessionMembers[iMemberEmpty].dwUserIndex = localUserIndex;
						xsessionDetails->pSessionMembers[iMemberEmpty].dwFlags = 0;
						if (pfPrivateSlots[iUser]) {
							xsessionDetails->liveSession->slotsPrivateFilledCount++;
							xsessionDetails->pSessionMembers[iMemberEmpty].dwFlags = XSESSION_MEMBER_FLAGS_PRIVATE_SLOT;
						}
						else {
							xsessionDetails->liveSession->slotsPublicFilledCount++;
						}
						xsessionDetails->dwActualMemberCount++;
						xsessionDetails->dwReturnedMemberCount++;
					}
					else {
						xsessionDetails->pSessionMembers[iMember].xuidOnline = xlive_users_info[localUserIndex]->xuid;
						xsessionDetails->pSessionMembers[iMember].dwUserIndex = localUserIndex;
						if (xsessionDetails->pSessionMembers[iMember].dwFlags & XSESSION_MEMBER_FLAGS_ZOMBIE) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "%s Local user %u was previously in the session.", __func__, localUserIndex);
						}
						else {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Local user %u is already in the session.", __func__, localUserIndex);
						}
						if (xsessionDetails->pSessionMembers[iMember].dwFlags & XSESSION_MEMBER_FLAGS_PRIVATE_SLOT) {
							xsessionDetails->liveSession->slotsPrivateFilledCount--;
						}
						else {
							xsessionDetails->liveSession->slotsPublicFilledCount--;
						}
						xsessionDetails->pSessionMembers[iMember].dwFlags = 0;
						if (pfPrivateSlots[iUser]) {
							xsessionDetails->liveSession->slotsPrivateFilledCount++;
							xsessionDetails->pSessionMembers[iMemberEmpty].dwFlags = XSESSION_MEMBER_FLAGS_PRIVATE_SLOT;
						}
						else {
							xsessionDetails->liveSession->slotsPublicFilledCount++;
						}
					}
				}
			}
		}
		else {
			result = XONLINE_E_SESSION_JOIN_ILLEGAL;
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = result;
		pXOverlapped->InternalHigh = result;
		pXOverlapped->dwExtendedError = result;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return result;
}

// #5328
DWORD WINAPI XSessionGetDetails(HANDLE hSession, DWORD *pcbResultsBuffer, XSESSION_LOCAL_DETAILS *pSessionDetails, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pcbResultsBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbResultsBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (*pcbResultsBuffer < sizeof(XSESSION_LOCAL_DETAILS)) {
		*pcbResultsBuffer = sizeof(XSESSION_LOCAL_DETAILS);
		return ERROR_INSUFFICIENT_BUFFER;
	}
	if (!pSessionDetails) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pSessionDetails is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t result = ERROR_SUCCESS;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		if (xlive_xsession_local_sessions.count(hSession)) {
			LIVE_SESSION_XSESSION *xsessionDetails = xlive_xsession_local_sessions[hSession];
			pSessionDetails->dwUserIndexHost = XLIVE_LOCAL_USER_INVALID;
			for (size_t iUser = 0; iUser < XLIVE_LOCAL_USER_COUNT; iUser++) {
				if (xlive_users_info[iUser]->xuid == xsessionDetails->liveSession->xuid) {
					pSessionDetails->dwUserIndexHost = iUser;
					break;
				}
			}
			pSessionDetails->dwGameType = xsessionDetails->dwGameType;
			pSessionDetails->dwGameMode = xsessionDetails->dwGameMode;
			pSessionDetails->dwFlags = xsessionDetails->liveSession->sessionFlags;
			pSessionDetails->dwMaxPublicSlots = xsessionDetails->liveSession->slotsPublicMaxCount;
			pSessionDetails->dwMaxPrivateSlots = xsessionDetails->liveSession->slotsPrivateMaxCount;
			pSessionDetails->dwAvailablePublicSlots = xsessionDetails->liveSession->slotsPublicMaxCount - xsessionDetails->liveSession->slotsPublicFilledCount;
			pSessionDetails->dwAvailablePrivateSlots = xsessionDetails->liveSession->slotsPrivateMaxCount - xsessionDetails->liveSession->slotsPrivateFilledCount;
			pSessionDetails->dwActualMemberCount = xsessionDetails->dwActualMemberCount;
			pSessionDetails->dwReturnedMemberCount = xsessionDetails->dwReturnedMemberCount;
			pSessionDetails->eState = xsessionDetails->eState;
			pSessionDetails->qwNonce = xsessionDetails->qwNonce;
			pSessionDetails->sessionInfo.sessionID = xsessionDetails->liveSession->xnkid;
			pSessionDetails->sessionInfo.hostAddress = xsessionDetails->xnAddr;
			pSessionDetails->sessionInfo.keyExchangeKey = xsessionDetails->liveSession->xnkey;
			pSessionDetails->xnkidArbitration = xsessionDetails->xnkidArbitration;
			
			// FIXME need to copy this in to buffer if there is enough space.
			pSessionDetails->pSessionMembers = 0;
		}
		else {
			result = XONLINE_E_SESSION_NOT_FOUND;
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = result;
		pXOverlapped->InternalHigh = result;
		pXOverlapped->dwExtendedError = result;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return result;
}

// #5329
DWORD WINAPI XSessionFlushStats(HANDLE hSession, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
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

// #5330
DWORD WINAPI XSessionDelete(HANDLE hSession, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
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

// #5332
DWORD WINAPI XSessionEnd(HANDLE hSession, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
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

// #5333
DWORD WINAPI XSessionArbitrationRegister(HANDLE hSession, DWORD dwFlags, ULONGLONG qwSessionNonce, DWORD *pcbResultsBuffer, XSESSION_REGISTRATION_RESULTS *pRegistrationResults, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwFlags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwFlags (0x%08x) is not 0.", __func__, dwFlags);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pcbResultsBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbResultsBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (*pcbResultsBuffer < 3592) {
		*pcbResultsBuffer = 3592;
		//*pcbResultsBuffer = sizeof(XSESSION_REGISTRATION_RESULTS) + sizeof(XSESSION_REGISTRANT) + sizeof(XUID);
		return ERROR_INSUFFICIENT_BUFFER;
	}
	if (!pRegistrationResults) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pRegistrationResults is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t result = ERROR_FUNCTION_FAILED;
	pRegistrationResults->wNumRegistrants = 0;
	pRegistrationResults->rgRegistrants = 0;
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);

	if (pXOverlapped) {
		//asynchronous

		pXOverlapped->InternalLow = result;
		pXOverlapped->InternalHigh = result;
		pXOverlapped->dwExtendedError = result;

		Check_Overlapped(pXOverlapped);

		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return result;
}

// #5336
DWORD WINAPI XSessionLeaveRemote(HANDLE hSession, DWORD dwXuidCount, const XUID *pXuids, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwXuidCount == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwXuidCount is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pXuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXuids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
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

// #5342
DWORD WINAPI XSessionModifySkill(HANDLE hSession, DWORD dwXuidCount, const XUID *pXuids, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!hSession) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s hSession is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwXuidCount == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwXuidCount is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pXuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXuids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
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

// #5343
uint32_t WINAPI XSessionCalculateSkill(uint32_t dwNumSkills, double *rgMu, double *rgSigma, double *pdblAggregateMu, double *pdblAggregateSigma)
{
	TRACE_FX();
	if (!rgMu) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s rgMu is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!rgSigma) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s rgSigma is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pdblAggregateMu) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdblAggregateMu is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pdblAggregateSigma) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdblAggregateSigma is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	*pdblAggregateMu = 0.0;
	*pdblAggregateSigma = 0.0;
	if (dwNumSkills) {
		for (uint32_t i = dwNumSkills; i < dwNumSkills; i++) {
			*pdblAggregateMu += rgMu[i];
			*pdblAggregateSigma += rgSigma[i] * rgSigma[i];
		}
		*pdblAggregateMu /= dwNumSkills;
		*pdblAggregateSigma = sqrt(*pdblAggregateSigma / dwNumSkills);
	}
	return ERROR_SUCCESS;
}
