#include "xdefs.hpp"
#include "xshow.hpp"
#include "../xlln/debug-text.hpp"
#include "xlive.hpp"
#include "../xlln/xlln.hpp"

// #5206
DWORD WINAPI XShowMessagesUI(DWORD dwUserIndex)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_SignedInToLive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in to LIVE.", __func__, dwUserIndex);
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_SignedInToLive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in to LIVE.", __func__, dwUserIndex);
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cRecipients (%d) is greater than 0x64.", __func__, cRecipients);
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_SignedInToLive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in to LIVE.", __func__, dwUserIndex);
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cRecipients (%d) is greater than 0x64.", __func__, cRecipients);
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_SignedInToLive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in to LIVE.", __func__, dwUserIndex);
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in.", __func__, dwUserIndex);
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cbImage (%d) is greater than 0x9000.", __func__, cbImage);
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cPlayers (%d) is greater than 0x64.", __func__, cPlayers);
		return ERROR_INVALID_PARAMETER;
	}
	if (pXButton) {
		if (pXButton->dwType < 1 || pXButton->dwType > 6) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXButton->dwType (%d) is must be between 1 to 6.", __func__, pXButton->dwType);
			return ERROR_INVALID_PARAMETER;
		}
		if (!pXButton->wszCustomText && (pXButton->dwType == 5 || pXButton->dwType == 6)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXButton->wszCustomText cannot be NULL when pXButton->dwType (%d) is 5 or 6.", __func__, pXButton->dwType);
			return ERROR_INVALID_PARAMETER;
		}
	}
	if (pYButton) {
		if (pYButton->dwType < 1 || pYButton->dwType > 6) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pYButton->dwType (%d) is must be between 1 to 6.", __func__, pYButton->dwType);
			return ERROR_INVALID_PARAMETER;
		}
		if (!pYButton->wszCustomText && (pYButton->dwType == 5 || pYButton->dwType == 6)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pYButton->wszCustomText cannot be NULL when pYButton->dwType (%d) is 5 or 6.", __func__, pYButton->dwType);
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_SignedInToLive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in to LIVE.", __func__, dwUserIndex);
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in.", __func__, dwUserIndex);
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
VOID XShowArcadeUI()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5250
DWORD WINAPI XShowAchievementsUI(DWORD dwUserIndex)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in.", __func__, dwUserIndex);
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in.", __func__, dwUserIndex);
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cPanes (%d) cannot sign in more than %d users.", __func__, cPanes, XLIVE_LOCAL_USER_COUNT);
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in.", __func__, dwUserIndex);
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cButtons (%d) must be between 1 and %d.", __func__, cButtons, XMB_MAXBUTTONS);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwFocusButton > cButtons - 1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwFocusButton > cButtons - 1) (%d > %d - 1).", __func__, dwFocusButton, cButtons);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pwszButtons) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pwszButtons is NULL when cButtons > 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	for (DWORD i = 0; i < cButtons; i++) {
		if (!pwszButtons[i]) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pwszButtons[%d] is NULL.", __func__, i);
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_SignedInToLive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in to LIVE.", __func__, dwUserIndex);
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
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_SignedInToLive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %d is not signed in to LIVE.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}

	ShowXLLN(XLLN_SHOW_HOME);
	return ERROR_SUCCESS;
}

// #5299
VOID XShowGuideKeyRemapUI()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5365
VOID XShowMarketplaceUI()
{
	TRACE_FX();
	FUNC_STUB();
}

// #5366
VOID XShowMarketplaceDownloadItemsUI()
{
	TRACE_FX();
	FUNC_STUB();
}
