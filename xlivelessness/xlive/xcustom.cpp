#include <winsock2.h>
#include "xdefs.hpp"
#include "xcontent.hpp"
#include "xlive.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/xlln.hpp"

CRITICAL_SECTION xlive_critsec_custom_actions;
bool xlive_custom_dynamic_actions_registered = false;
uint16_t xlive_custom_dynamic_actions_count = 0;

// #472
VOID WINAPI XCustomSetAction(DWORD dwActionIndex, LPCWSTR lpwszActionText, DWORD dwFlags)
{
	TRACE_FX();
	if (dwActionIndex > 3) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwActionIndex > 3) (%u).", __func__, dwActionIndex);
		return;
	}
	if (dwFlags && dwFlags != 1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwFlags && dwFlags != 1) (%u).", __func__, dwFlags);
		return;
	}
	{
		EnterCriticalSection(&xlive_critsec_custom_actions);
		if (xlive_custom_dynamic_actions_registered) {
			LeaveCriticalSection(&xlive_critsec_custom_actions);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xlive_custom_dynamic_actions_registered.", __func__);
			return;
		}


		LeaveCriticalSection(&xlive_critsec_custom_actions);
	}
}

// #473
BOOL WINAPI XCustomGetLastActionPress(DWORD *pdwUserIndex, DWORD *pdwActionIndex, XUID *pXuid)
{
	TRACE_FX();

	if (pdwUserIndex) {
		*pdwUserIndex = 0;
	}
	if (pdwActionIndex) {
		*pdwActionIndex = 0;
	}
	// if XUID on the last action (not parameter) is empty then return false;
	if (pXuid) {
		*pXuid = 0;
	}

	return FALSE;
}

// #476
DWORD WINAPI XCustomGetLastActionPressEx(DWORD *pdwUserIndex, DWORD *pdwActionId, XUID *pXuid, BYTE *pbPayload, WORD *pcbPayload)
{
	TRACE_FX();
	if (pcbPayload) {
		// check that the buffer is big enough (and internally it should not be bigger than 1kb aka 1000).
		if (*pcbPayload < 0) {
			*pcbPayload = 0;
			return ERROR_INSUFFICIENT_BUFFER;
		}
	}

	uint32_t result = ERROR_SUCCESS;

	if (pdwUserIndex) {
		*pdwUserIndex = 0;
	}
	if (pdwActionId) {
		*pdwActionId = 0;
	}
	// if XUID on the last action (not parameter) is empty then return no data. otherwise return succeess and also clear that action xuid.
	if (pXuid) {
		*pXuid = 0;
	}
	if (pcbPayload) {
		*pcbPayload = 0;
	}
	result = ERROR_NO_DATA;

	return result;
}

// #474
DWORD WINAPI XCustomSetDynamicActions(DWORD dwUserIndex, XUID xuid, CONST XCUSTOMACTION *pCustomActions, WORD cCustomActions)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_SignedInToLive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in to LIVE.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (cCustomActions > 3) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (cCustomActions > 3) (%hu).", __func__, cCustomActions);
		return ERROR_INVALID_PARAMETER;
	}

	{
		EnterCriticalSection(&xlive_critsec_custom_actions);
		if (!xlive_custom_dynamic_actions_registered) {
			LeaveCriticalSection(&xlive_critsec_custom_actions);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s !xlive_custom_dynamic_actions_registered.", __func__);
			return ERROR_ACCESS_DENIED;
		}

		xlive_custom_dynamic_actions_count = cCustomActions;

		LeaveCriticalSection(&xlive_critsec_custom_actions);
	}

	return ERROR_SUCCESS;
}

// #477
VOID WINAPI XCustomRegisterDynamicActions()
{
	TRACE_FX();
	EnterCriticalSection(&xlive_critsec_custom_actions);
	if (!xlive_custom_dynamic_actions_registered) {
		xlive_custom_dynamic_actions_registered = true;

	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s already xlive_custom_dynamic_actions_registered.", __func__);
	}
	LeaveCriticalSection(&xlive_critsec_custom_actions);
}

// #478
VOID WINAPI XCustomUnregisterDynamicActions()
{
	TRACE_FX();
	EnterCriticalSection(&xlive_critsec_custom_actions);
	if (xlive_custom_dynamic_actions_registered) {
		xlive_custom_dynamic_actions_registered = false;
		xlive_custom_dynamic_actions_count = 0;
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s already !xlive_custom_dynamic_actions_registered.", __func__);
	}
	LeaveCriticalSection(&xlive_critsec_custom_actions);
}

// #479
BOOL WINAPI XCustomGetCurrentGamercard(DWORD *pdwUserIndex, XUID *pXuid)
{
	TRACE_FX();
	if (!pdwUserIndex && !pXuid) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!pdwUserIndex && !pXuid).", __func__);
		return FALSE;
	}
	{
		EnterCriticalSection(&xlive_critsec_custom_actions);
		if (!xlive_custom_dynamic_actions_registered) {
			LeaveCriticalSection(&xlive_critsec_custom_actions);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s !xlive_custom_dynamic_actions_registered.", __func__);
			return FALSE;
		}

		*pdwUserIndex = 0;
		*pXuid = 0;

		LeaveCriticalSection(&xlive_critsec_custom_actions);
	}

	return FALSE;
	// If xuid is set.
	return TRUE;
}
