#include <winsock2.h>
#include "xdefs.hpp"
#include "xshow.hpp"
#include "../xlln/debug-text.hpp"
#include "xlive.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/wnd-main.hpp"
#include "xrender.hpp"

// #5206
DWORD WINAPI XShowMessagesUI(DWORD dwUserIndex)
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

	ShowXLLN(XLLN_SHOW_HOME);
	return ERROR_SUCCESS;
}

// #5208
DWORD WINAPI XShowGameInviteUI(DWORD dwUserIndex, const XUID *pXuidRecipients, DWORD cRecipients, LPCWSTR wszUnused)
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
	if (cRecipients && !pXuidRecipients) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (cRecipients && !pXuidRecipients).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!cRecipients && pXuidRecipients) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!cRecipients && pXuidRecipients).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cRecipients > 0x64) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cRecipients (%u) is greater than 0x64.", __func__, cRecipients);
		return ERROR_INVALID_PARAMETER;
	}
	if (wszUnused) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s wszUnused is not officially implemented.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	ShowXLLN(XLLN_SHOW_HOME);
	return ERROR_SUCCESS;
}

// #5209
DWORD WINAPI XShowMessageComposeUI(DWORD dwUserIndex, const XUID *pXuidRecipients, DWORD cRecipients, LPCWSTR wszText)
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
	if (cRecipients && !pXuidRecipients) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (cRecipients && !pXuidRecipients).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!cRecipients && pXuidRecipients) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!cRecipients && pXuidRecipients).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cRecipients > 0x64) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cRecipients (%u) is greater than 0x64.", __func__, cRecipients);
		return ERROR_INVALID_PARAMETER;
	}

	if (wszText) {}

	ShowXLLN(XLLN_SHOW_HOME);
	return ERROR_SUCCESS;
}

// #5210
DWORD WINAPI XShowFriendRequestUI(DWORD dwUserIndex, XUID xuidUser)
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
	if (!xuidUser) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuidUser is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	ShowXLLN(XLLN_SHOW_HOME);
	return ERROR_SUCCESS;
}

// #5212
DWORD WINAPI XShowCustomPlayerListUI(
	DWORD dwUserIndex,
	DWORD dwFlags,
	LPCWSTR lpwszTitle,
	LPCWSTR lpwszDescription,
	const BYTE *pbImage,
	DWORD cbImage,
	const XPLAYERLIST_USER *rgPlayers,
	DWORD cPlayers,
	const XPLAYERLIST_BUTTON *pXButton,
	const XPLAYERLIST_BUTTON *pYButton,
	XPLAYERLIST_RESULT *pResult,
	XOVERLAPPED *pXOverlapped)
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
	if (dwFlags & 0xFFFFFFFE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwFlags & 0xFFFFFFFE) dwFlags (0x%08x).", __func__, dwFlags);
		return ERROR_INVALID_PARAMETER;
	}
	if (!lpwszTitle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s lpwszTitle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!lpwszDescription) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s lpwszDescription is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cbImage && !pbImage) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (cbImage && !pbImage).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!cbImage && pbImage) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!cbImage && pbImage).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cbImage > 0x9000) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cbImage (%u) is greater than 0x9000.", __func__, cbImage);
		return ERROR_INVALID_PARAMETER;
	}
	if (cPlayers && !rgPlayers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (cPlayers && !rgPlayers).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!cPlayers && rgPlayers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!cPlayers && rgPlayers).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cPlayers > 0x64) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cPlayers (%u) is greater than 0x64.", __func__, cPlayers);
		return ERROR_INVALID_PARAMETER;
	}
	if (pXButton) {
		if (pXButton->dwType < 1 || pXButton->dwType > 6) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXButton->dwType (%u) is must be between 1 to 6.", __func__, pXButton->dwType);
			return ERROR_INVALID_PARAMETER;
		}
		if (!pXButton->wszCustomText && (pXButton->dwType == 5 || pXButton->dwType == 6)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXButton->wszCustomText cannot be NULL when pXButton->dwType (%u) is 5 or 6.", __func__, pXButton->dwType);
			return ERROR_INVALID_PARAMETER;
		}
	}
	if (pYButton) {
		if (pYButton->dwType < 1 || pYButton->dwType > 6) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pYButton->dwType (%u) is must be between 1 to 6.", __func__, pYButton->dwType);
			return ERROR_INVALID_PARAMETER;
		}
		if (!pYButton->wszCustomText && (pYButton->dwType == 5 || pYButton->dwType == 6)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pYButton->wszCustomText cannot be NULL when pYButton->dwType (%u) is 5 or 6.", __func__, pYButton->dwType);
			return ERROR_INVALID_PARAMETER;
		}
	}

	ShowXLLN(XLLN_SHOW_HOME);

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

// #5214
DWORD WINAPI XShowPlayerReviewUI(DWORD dwUserIndex, XUID XuidFeedbackTarget)
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
	if (!XuidFeedbackTarget) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XuidFeedbackTarget is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	ShowXLLN(XLLN_SHOW_HOME);
	return ERROR_SUCCESS;
}

// #5215
DWORD WINAPI XShowGuideUI(DWORD dwUserIndex)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}

	ShowXLLN(XLLN_SHOW_HOME);
	return ERROR_SUCCESS;
}

// #5216
DWORD WINAPI XShowKeyboardUI(DWORD dwUserIndex, DWORD dwFlags, LPCWSTR wseDefaultText, LPCWSTR wszTitleText, LPCWSTR wszDescriptionText, LPWSTR wszResultText, DWORD cchResultText, XOVERLAPPED *pXOverlapped)
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
	if (dwFlags && !(dwFlags & 0xE0000080)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwFlags && !(dwFlags & 0xE0000080)).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!wszResultText) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s wszResultText is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!cchResultText) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cchResultText is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pXOverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXOverlapped is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	//TODO XShowKeyboardUI dwFlags (like VKBD_DEFAULT)
	ShowXLLN(XLLN_SHOW_HOME);

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

// #5218
DWORD WINAPI XShowArcadeUI(DWORD dwUserIndex)
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

	ShowXLLN(XLLN_SHOW_HOME);
	return ERROR_SUCCESS;
}

// #5250
DWORD WINAPI XShowAchievementsUI(DWORD dwUserIndex)
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

	ShowXLLN(XLLN_SHOW_HOME);
	return ERROR_SUCCESS;
}

// #5252
DWORD WINAPI XShowGamerCardUI(DWORD dwUserIndex, XUID XuidPlayer)
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

	ShowXLLN(XLLN_SHOW_HOME);
	return ERROR_SUCCESS;
}

// #5260
// cPanes - Number of users to sign in.
DWORD WINAPI XShowSigninUI(DWORD cPanes, DWORD dwFlags)
{
	TRACE_FX();
	if (cPanes == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cPanes is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cPanes > XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cPanes (%u) cannot sign in more than %u users.", __func__, cPanes, XLIVE_LOCAL_USER_COUNT);
		return ERROR_INVALID_PARAMETER;
	}
	if (!(!(dwFlags & 0x40FFFC) && (!(dwFlags & XSSUI_FLAGS_LOCALSIGNINONLY) || !(dwFlags & XSSUI_FLAGS_SHOWONLYONLINEENABLED)))) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s !(!(dwFlags & 0x40FFFC) && (!(dwFlags & XSSUI_FLAGS_LOCALSIGNINONLY) || !(dwFlags & XSSUI_FLAGS_SHOWONLYONLINEENABLED))).", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	ShowXLLN(XLLN_SHOW_LOGIN);

	return ERROR_SUCCESS;
	// no users signed in with multiplayer privilege. if XSSUI_FLAGS_SHOWONLYONLINEENABLED is flagged?
	return ERROR_FUNCTION_FAILED;
}

// #5266
DWORD WINAPI XShowMessageBoxUI(
	DWORD dwUserIndex,
	LPCWSTR wszTitle,
	LPCWSTR wszText,
	DWORD cButtons,
	LPCWSTR *pwszButtons,
	DWORD dwFocusButton,
	DWORD dwFlags,
	MESSAGEBOX_RESULT *pResult,
	XOVERLAPPED *pXOverlapped)
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
	if (dwFlags & 0xFFF0FFFC) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwFlags & 0xFFF0FFFC) dwFlags is (0x%08x).", __func__, dwFlags);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pResult) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pResult is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pXOverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXOverlapped is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!wszTitle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s wszTitle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!wszText) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s wszText is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwFlags & (XMB_VERIFYPASSCODEMODE | XMB_PASSCODEMODE)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwFlags does not officially support XMB_VERIFYPASSCODEMODE or XMB_PASSCODEMODE.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cButtons <= 0 || cButtons > XMB_MAXBUTTONS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cButtons (%u) must be between 1 and %u.", __func__, cButtons, XMB_MAXBUTTONS);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwFocusButton > cButtons - 1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwFocusButton > cButtons - 1) (%u > %u - 1).", __func__, dwFocusButton, cButtons);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pwszButtons) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pwszButtons is NULL when cButtons > 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	for (DWORD i = 0; i < cButtons; i++) {
		if (!pwszButtons[i]) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pwszButtons[%u] is NULL.", __func__, i);
			return ERROR_INVALID_PARAMETER;
		}
	}
	
	pResult->dwButtonPressed = MessageBoxW(NULL, wszText, wszTitle, MB_OKCANCEL);

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
	return ERROR_CANCELLED;
}

// #5271
DWORD WINAPI XShowPlayersUI(DWORD dwUserIndex)
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

	ShowXLLN(XLLN_SHOW_HOME);
	return ERROR_SUCCESS;
}

// #5275
DWORD WINAPI XShowFriendsUI(DWORD dwUserIndex)
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

	ShowXLLN(XLLN_SHOW_HOME);
	return ERROR_SUCCESS;
}

// #5298
DWORD WINAPI XLiveGetGuideKey(XINPUT_KEYSTROKE *pKeystroke)
{
	TRACE_FX();
	if (!pKeystroke) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pKeystroke is NULL.", __func__);
		return E_INVALIDARG;
	}
	pKeystroke->VirtualKey = xlive_hotkey_id_guide;
	pKeystroke->Unicode = 0;
	return S_OK;
}

// #5299
DWORD WINAPI XShowGuideKeyRemapUI(DWORD dwUserIndex)
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

	ShowXLLN(XLLN_SHOW_HOME);
	return ERROR_SUCCESS;
}

// #5365
DWORD WINAPI XShowMarketplaceUI(DWORD dwUserIndex, DWORD dwEntryPoint, ULONGLONG qwOfferID, DWORD dwContentCategories)
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
	if (dwEntryPoint >= XSHOWMARKETPLACEUI_ENTRYPOINT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s unknown dwEntryPoint enum (0x%08x).", __func__, dwEntryPoint);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwEntryPoint == XSHOWMARKETPLACEUI_ENTRYPOINT_MEMBERSHIPLIST) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%sdwEntryPoint == XSHOWMARKETPLACEUI_ENTRYPOINT_MEMBERSHIPLIST.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwEntryPoint == XSHOWMARKETPLACEUI_ENTRYPOINT_MEMBERSHIPITEM) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%sdwEntryPoint == XSHOWMARKETPLACEUI_ENTRYPOINT_MEMBERSHIPITEM.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (qwOfferID) {
		if (dwEntryPoint == XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTLIST) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s qwOfferID (0x%08x) should be 0 when XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTLIST.", __func__, qwOfferID);
			return ERROR_INVALID_PARAMETER;
		}
		if (dwEntryPoint == XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTLIST_BACKGROUND) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s qwOfferID (0x%08x) should be 0 when XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTLIST_BACKGROUND.", __func__, qwOfferID);
			return ERROR_INVALID_PARAMETER;
		}
	}
	else {
		if (dwEntryPoint == XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTITEM) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s qwOfferID must not be 0 when XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTITEM.", __func__);
			return ERROR_INVALID_PARAMETER;
		}
		if (dwEntryPoint == XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTITEM_BACKGROUND) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s qwOfferID must not be 0 when XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTITEM_BACKGROUND.", __func__);
			return ERROR_INVALID_PARAMETER;
		}
	}

	ShowXLLN(XLLN_SHOW_HOME);
	return ERROR_SUCCESS;
}

// #5366
DWORD XShowMarketplaceDownloadItemsUI(DWORD dwUserIndex, DWORD dwEntryPoint, CONST ULONGLONG *pOfferIDs, DWORD dwOfferIdCount, HRESULT *phrResult, XOVERLAPPED *pXOverlapped)
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
	if (dwEntryPoint >= XSHOWMARKETPLACEUI_ENTRYPOINT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s unknown dwEntryPoint enum (0x%08x).", __func__, dwEntryPoint);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwEntryPoint != XSHOWMARKETPLACEDOWNLOADITEMS_ENTRYPOINT_FREEITEMS && dwEntryPoint != XSHOWMARKETPLACEDOWNLOADITEMS_ENTRYPOINT_PAIDITEMS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwEntryPoint enum (0x%08x) is not XSHOWMARKETPLACEDOWNLOADITEMS_ENTRYPOINT_FREEITEMS or XSHOWMARKETPLACEDOWNLOADITEMS_ENTRYPOINT_PAIDITEMS.", __func__, dwEntryPoint);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pOfferIDs) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pOfferIDs is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!dwOfferIdCount) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwOfferIdCount is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwOfferIdCount > XMARKETPLACE_MAX_OFFERIDS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwOfferIdCount > XMARKETPLACE_MAX_OFFERIDS) (0x%08x > 0x%08x).", __func__, dwOfferIdCount, XMARKETPLACE_MAX_OFFERIDS);
		return ERROR_INVALID_PARAMETER;
	}
	if (!phrResult) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phrResult is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	*phrResult = MPDI_E_INVALIDARG;
	*phrResult = MPDI_E_OPERATION_FAILED;
	*phrResult = S_OK;
	*phrResult = MPDI_E_CANCELLED;

	ShowXLLN(XLLN_SHOW_HOME);

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
