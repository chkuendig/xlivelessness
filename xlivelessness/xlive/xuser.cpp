#include <winsock2.h>
#include "xdefs.hpp"
#include "xuser.hpp"
#include "xlive.hpp"
#include "xsession.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/xlln-config.hpp"
#include "../utils/utils.hpp"
#include <stdio.h>

CRITICAL_SECTION xlive_critsec_xuser_achievement_enumerators;
// Key: enumerator handle (id).
// Value: Vector of XACHIEVEMENT_DETAILS that have already? been returned for that enumerator.
std::map<HANDLE, std::vector<uint32_t>> xlive_xuser_achievement_enumerators;

CRITICAL_SECTION xlive_critsec_xuser_stats;
// Key: enumerator handle (id).
// Value: Vector of ? that have already been returned for that enumerator.
std::map<HANDLE, DWORD> xlive_xuser_stats_enumerators;

static BOOL XLivepIsPropertyIdValid(DWORD dwPropertyId, BOOL a2)
{
	return !(dwPropertyId & X_PROPERTY_SCOPE_MASK)
		|| dwPropertyId == X_PROPERTY_RANK
		|| dwPropertyId == X_PROPERTY_SESSION_ID
		|| dwPropertyId == X_PROPERTY_GAMER_ZONE
		|| dwPropertyId == X_PROPERTY_GAMER_COUNTRY
		|| dwPropertyId == X_PROPERTY_GAMER_LANGUAGE
		|| dwPropertyId == X_PROPERTY_GAMER_RATING
		|| dwPropertyId == X_PROPERTY_GAMER_MU
		|| dwPropertyId == X_PROPERTY_GAMER_SIGMA
		|| dwPropertyId == X_PROPERTY_GAMER_PUID
		|| dwPropertyId == X_PROPERTY_AFFILIATE_SCORE
		|| dwPropertyId == X_PROPERTY_RELATIVE_SCORE
		|| dwPropertyId == X_PROPERTY_SESSION_TEAM
		|| !a2 && dwPropertyId == X_PROPERTY_GAMER_HOSTNAME;
}

static void GetXProfileSettingInfo(uint32_t setting_id, uint8_t *data_type, uint16_t *data_size, bool *is_title_setting, bool *can_write)
{
	switch (setting_id) {
	case XPROFILE_TITLE_SPECIFIC1:
	case XPROFILE_TITLE_SPECIFIC2:
	case XPROFILE_TITLE_SPECIFIC3:
	case XPROFILE_GAMERCARD_TITLE_ACHIEVEMENTS_EARNED:
	case XPROFILE_GAMERCARD_TITLE_CRED_EARNED:
	case XPROFILE_OPTION_CONTROLLER_VIBRATION:
		if (is_title_setting) {
			*is_title_setting = true;
		}
		if (can_write) {
			*can_write = true;
		}
		break;
	default:
		if (is_title_setting) {
			*is_title_setting = false;
		}
		if (can_write) {
			*can_write = false;
		}
		break;
	}
	SETTING_ID *setting_id_info = (SETTING_ID*)&setting_id;
	if (data_type) {
		*data_type = setting_id_info->data_type;
	}
	if (data_size) {
		*data_size = setting_id_info->data_size;
	}
}

static bool IsValidSettingId(uint32_t title_id, uint32_t setting_id)
{
	uint8_t dataType;
	uint16_t dataSize;
	GetXProfileSettingInfo(setting_id, &dataType, &dataSize, 0, 0);
	uint32_t requiredReadSizeMin = 0;
	switch (dataType) {
	case XUSER_DATA_TYPE_INT32:
		if (dataSize != sizeof(LONG)) {
			return false;
		}
		break;
	case XUSER_DATA_TYPE_INT64:
		if (dataSize != sizeof(LONGLONG)) {
			return false;
		}
		break;
	case XUSER_DATA_TYPE_DOUBLE:
		if (dataSize != sizeof(double)) {
			return false;
		}
		break;
	case XUSER_DATA_TYPE_FLOAT:
		if (dataSize != sizeof(FLOAT)) {
			return false;
		}
		break;
	case XUSER_DATA_TYPE_DATETIME:
		if (dataSize != sizeof(FILETIME)) {
			return false;
		}
		break;
	case XUSER_DATA_TYPE_UNICODE:
		if (dataSize < sizeof(wchar_t)) {
			return false;
		}
		break;
	}

	if (
		(*(SETTING_ID*)&setting_id).id < 0x50
		|| (
			setting_id == XPROFILE_TITLE_SPECIFIC1
			|| setting_id == XPROFILE_TITLE_SPECIFIC2
			|| setting_id == XPROFILE_TITLE_SPECIFIC3
			)
		&& title_id != DASHBOARD_TITLE_ID
		) {
		return true;
	}
	return false;
}

static uint32_t ValidateSettings(uint32_t title_id, uint32_t num_settings, const XUSER_PROFILE_SETTING *settings)
{
	if (num_settings == 0) {
		return ERROR_SUCCESS;
	}
	for (uint32_t i = 0; i < num_settings; i++) {
		if (!IsValidSettingId(title_id, settings[i].dwSettingId)) {
			return ERROR_INVALID_PARAMETER;
		}
	}
	return ERROR_SUCCESS;
}

static uint32_t ValidateSettingIds(uint32_t title_id, uint32_t num_settings, const uint32_t *setting_ids)
{
	if (num_settings == 0) {
		return ERROR_SUCCESS;
	}
	for (uint32_t i = 0; i < num_settings; i++) {
		if (!IsValidSettingId(title_id, setting_ids[i])) {
			return ERROR_INVALID_PARAMETER;
		}
	}
	return ERROR_SUCCESS;
}

// #5261
DWORD WINAPI XUserGetXUID(DWORD dwUserIndex, XUID *pxuid)
{
	TRACE_FX();
	if (!pxuid) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxuid is NULL.", __func__);
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

	*pxuid = xlive_users_info[dwUserIndex]->xuid;
	return ERROR_SUCCESS;
}

// #5262
XUSER_SIGNIN_STATE WINAPI XUserGetSigninState(DWORD dwUserIndex)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return eXUserSigninState_NotSignedIn;
	}
	return xlive_users_info[dwUserIndex]->UserSigninState;
}

// #5263
DWORD WINAPI XUserGetName(DWORD dwUserIndex, LPSTR szUserName, DWORD cchUserName)
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
	if (!szUserName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s szUserName is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!cchUserName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cchUserName is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	if (cchUserName > XUSER_NAME_SIZE) {
		cchUserName = XUSER_NAME_SIZE;
	}

	memcpy(szUserName, xlive_users_info[dwUserIndex]->szUserName, cchUserName);
	return ERROR_SUCCESS;
}

// #5264
DWORD WINAPI XUserAreUsersFriends(DWORD dwUserIndex, XUID *pXuids, DWORD dwXuidCount, BOOL *pfResult, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_SignedInToLive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in to a LIVE account.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!pXuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXuids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!dwXuidCount) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwXuidCount is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (pXOverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXOverlapped is not NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	*pfResult = FALSE;

	return ERROR_SUCCESS;
}

// #5265
DWORD WINAPI XUserCheckPrivilege(DWORD dwUserIndex, XPRIVILEGE_TYPE privilegeType, BOOL *pfResult)
{
	TRACE_FX();
	if (!pfResult) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pfResult is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	*pfResult = FALSE;
	if (privilegeType != XPRIVILEGE_MULTIPLAYER_SESSIONS
		&& privilegeType != XPRIVILEGE_COMMUNICATIONS
		&& privilegeType != XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY
		&& privilegeType != XPRIVILEGE_PROFILE_VIEWING
		&& privilegeType != XPRIVILEGE_PROFILE_VIEWING_FRIENDS_ONLY
		&& privilegeType != XPRIVILEGE_USER_CREATED_CONTENT
		&& privilegeType != XPRIVILEGE_USER_CREATED_CONTENT_FRIENDS_ONLY
		&& privilegeType != XPRIVILEGE_PURCHASE_CONTENT
		&& privilegeType != XPRIVILEGE_PRESENCE
		&& privilegeType != XPRIVILEGE_PRESENCE_FRIENDS_ONLY
		&& privilegeType != XPRIVILEGE_TRADE_CONTENT
		&& privilegeType != XPRIVILEGE_VIDEO_COMMUNICATIONS
		&& privilegeType != XPRIVILEGE_VIDEO_COMMUNICATIONS_FRIENDS_ONLY
		&& privilegeType != XPRIVILEGE_MULTIPLAYER_DEDICATED_SERVER
	) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s privilegeType (%u) does not exist.", __func__, dwUserIndex, privilegeType);
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

	*pfResult = TRUE;

	return ERROR_SUCCESS;
}

// #5267
DWORD WINAPI XUserGetSigninInfo(DWORD dwUserIndex, DWORD dwFlags, XUSER_SIGNIN_INFO *pSigninInfo)
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
	if (!pSigninInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pSigninInfo is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwFlags & ~(XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY | XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY | XUSER_GET_SIGNIN_INFO_UNKNOWN_XUID_ONLY)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwFlags (0x%08x) is invalid.", __func__, dwFlags);
		return ERROR_INVALID_PARAMETER;
	}
	if ((dwFlags & XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY) && (dwFlags & XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwFlags cannot be XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY and XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	memcpy_s(pSigninInfo, sizeof(XUSER_SIGNIN_INFO), xlive_users_info[dwUserIndex], sizeof(XUSER_SIGNIN_INFO));

	if (dwFlags & XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY && xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_SignedInToLive) {
		pSigninInfo->xuid = INVALID_XUID;
	}

	return ERROR_SUCCESS;
}

// #5274
DWORD WINAPI XUserAwardGamerPicture(DWORD dwUserIndex, DWORD dwPictureId, DWORD dwReserved, XOVERLAPPED *pXOverlapped)
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
	if (dwReserved) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pSigninInfo is not 0.", __func__);
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

// #5278
DWORD WINAPI XUserWriteAchievements(DWORD dwNumAchievements, CONST XUSER_ACHIEVEMENT *pAchievements, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwNumAchievements == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumAchievements is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pAchievements) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pAchievements is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	for (DWORD i = 0; i < dwNumAchievements; i++) {
		if (pAchievements[i].dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, pAchievements[i].dwUserIndex);
			return ERROR_NO_SUCH_USER;
		}
		if (xlive_users_info[pAchievements[i].dwUserIndex]->UserSigninState != eXUserSigninState_SignedInToLive) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in to a LIVE account.", __func__, pAchievements[i].dwUserIndex);
			return ERROR_INVALID_OPERATION;
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
	return ERROR_SUCCESS;
}

// #5279
DWORD WINAPI XUserReadAchievementPicture(DWORD dwUserIndex, DWORD dwTitleId, DWORD dwImageId, BYTE *pbTextureBuffer, DWORD dwPitch, DWORD dwHeight, XOVERLAPPED *pXOverlapped)
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
	if (!pbTextureBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pbTextureBuffer is NULL.", __func__);
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

// #5280
DWORD WINAPI XUserCreateAchievementEnumerator(DWORD dwTitleId, DWORD dwUserIndex, XUID xuid, DWORD dwDetailFlags, DWORD dwStartingIndex, DWORD cItem, DWORD *pcbBuffer, HANDLE *phEnum)
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
	if (cItem > XACHIEVEMENT_MAX_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cItem >= XACHIEVEMENT_MAX_COUNT) (%u >= %u).", __func__, cItem, XACHIEVEMENT_MAX_COUNT);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwStartingIndex >= XACHIEVEMENT_MAX_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwStartingIndex >= XACHIEVEMENT_MAX_COUNT) (%u >= %u).", __func__, dwStartingIndex, XACHIEVEMENT_MAX_COUNT);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwStartingIndex + cItem < dwStartingIndex) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwStartingIndex + cItem < XACHIEVEMENT_MAX_COUNT) (%u + %u < %u).", __func__, dwStartingIndex, cItem, dwStartingIndex);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwStartingIndex + cItem >= XACHIEVEMENT_MAX_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwStartingIndex + cItem >= XACHIEVEMENT_MAX_COUNT) (%u + %u >= %u).", __func__, dwStartingIndex, cItem, XACHIEVEMENT_MAX_COUNT);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwDetailFlags == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwDetailFlags is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwDetailFlags != XACHIEVEMENT_DETAILS_ALL && dwDetailFlags & ~(XACHIEVEMENT_DETAILS_LABEL | XACHIEVEMENT_DETAILS_DESCRIPTION | XACHIEVEMENT_DETAILS_UNACHIEVED | XACHIEVEMENT_DETAILS_TFC)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwDetailFlags (0x%08x) is invalid.", __func__, dwDetailFlags);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pcbBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!phEnum) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phEnum is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	if (xuid == INVALID_XUID) {
		//enumerate the local signed-in gamer's achievements.
	}

	for (DWORD i = dwStartingIndex; i < dwStartingIndex + cItem; i++) {
		//?
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);

	*pcbBuffer = cItem * sizeof(XACHIEVEMENT_DETAILS);
	*phEnum = CreateMutex(NULL, NULL, NULL);
	EnterCriticalSection(&xlive_critsec_xuser_achievement_enumerators);
	xlive_xuser_achievement_enumerators[*phEnum];
	LeaveCriticalSection(&xlive_critsec_xuser_achievement_enumerators);

	return ERROR_SUCCESS;
}

// #5281
DWORD WINAPI XUserReadStats(DWORD dwTitleId, DWORD dwNumXuids, CONST XUID *pXuids, DWORD dwNumStatsSpecs, CONST XUSER_STATS_SPEC *pSpecs, DWORD *pcbResults, XUSER_STATS_READ_RESULTS *pResults, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwNumXuids == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumXuids is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwNumXuids > 0x65) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumXuids (0x%08x) is greater than 0x65.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pXuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXuids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwNumStatsSpecs == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumStatsSpecs is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwNumStatsSpecs > 0x40) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumStatsSpecs (0x%08x) is greater than 0x40.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pSpecs) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pSpecs is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pcbResults) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbResults is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (*pcbResults && !pResults) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (*pcbResults && !pResults).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!*pcbResults && pResults) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!*pcbResults && pResults).", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	if (!pResults) {
		*pcbResults = sizeof(XUSER_STATS_READ_RESULTS);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "%s Requires buffer size %u.", __func__, *pcbResults);
		return ERROR_INSUFFICIENT_BUFFER;
	}

	pResults->dwNumViews = 0;
	pResults->pViews = 0;

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return ERROR_NOT_FOUND;

	DWORD *v9 = pcbResults;
	DWORD v10 = *pcbResults;
	DWORD v11 = dwNumStatsSpecs * (52 * dwNumXuids + 16) + 8;
	if (dwNumStatsSpecs)
	{
		DWORD *v12 = (DWORD*)((char*)pSpecs + 4);
		do
		{
			v11 += 28 * dwNumXuids * *v12;
			v12 += 34;
			--dwNumStatsSpecs;
		} while (dwNumStatsSpecs);
		v9 = pcbResults;
	}
	if (v11 > v10)
	{
		*v9 = v11;
		return ERROR_INSUFFICIENT_BUFFER;
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

// #5273
DWORD XUserReadGamerPictureByKey(CONST XUSER_DATA *pGamercardPictureKey, BOOL fSmall, BYTE *pbTextureBuffer, DWORD dwPitch, DWORD dwHeight, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!pGamercardPictureKey) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pGamercardPictureKey is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pbTextureBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pbTextureBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return ERROR_FUNCTION_FAILED;

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

// #5282
DWORD WINAPI XUserReadGamerPicture(DWORD dwUserIndex, BOOL fSmall, BYTE *pbTextureBuffer, DWORD dwPitch, DWORD dwHeight, XOVERLAPPED *pXOverlapped)
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
	if (!pbTextureBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pbTextureBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return ERROR_FUNCTION_FAILED;

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

DWORD __stdcall XUserCreateStatsEnumerator(DWORD dwRankStart, LONGLONG i64Rating, XUID XuidPivot, DWORD dwTitleId, DWORD dwNumRows, DWORD dwNumStatsSpecs, const XUSER_STATS_SPEC *pSpecs, DWORD *pcbBuffer, HANDLE *phEnum)
{
	TRACE_FX();
	if (dwNumRows == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumRows is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwNumRows > 0x64) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumRows (0x%08x) is greater than 0x64.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwNumStatsSpecs == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumStatsSpecs is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwNumStatsSpecs > 0x40) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumStatsSpecs (0x%08x) is greater than 0x40.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pSpecs) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pSpecs is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pcbBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!phEnum) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ph is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	DWORD v9 = dwNumStatsSpecs;
	DWORD v12 = v9 * (48 * dwNumRows + 16) + 8;
	if (v9)
	{
		DWORD *v13 = (DWORD*)((char *)pSpecs + 4);
		do
		{
			v12 += 28 * dwNumRows * *v13;
			v13 += 34;
			--v9;
		} while (v9);
	}
	*pcbBuffer = v12;

	*phEnum = CreateMutex(NULL, NULL, NULL);
	EnterCriticalSection(&xlive_critsec_xuser_stats);
	xlive_xuser_stats_enumerators[*phEnum];
	LeaveCriticalSection(&xlive_critsec_xuser_stats);

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return ERROR_SUCCESS;
}

// #5284
DWORD WINAPI XUserCreateStatsEnumeratorByRank(DWORD dwTitleId, DWORD dwRankStart, DWORD dwNumRows, DWORD dwNumStatsSpecs, const XUSER_STATS_SPEC *pSpecs, DWORD *pcbBuffer, HANDLE *phEnum)
{
	TRACE_FX();
	if (dwRankStart == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwRankStart is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	return XUserCreateStatsEnumerator(dwRankStart, 0, 0, dwTitleId, dwNumRows, dwNumStatsSpecs, pSpecs, pcbBuffer, phEnum);
}

// #5285
DWORD WINAPI XUserCreateStatsEnumeratorByRating(DWORD dwTitleId, LONGLONG i64Rating, DWORD dwNumRows, DWORD dwNumStatsSpecs, const XUSER_STATS_SPEC *pSpecs, DWORD *pcbBuffer, HANDLE *phEnum)
{
	TRACE_FX();

	return XUserCreateStatsEnumerator(0, i64Rating, 0, dwTitleId, dwNumRows, dwNumStatsSpecs, pSpecs, pcbBuffer, phEnum);
}

// #5286
DWORD WINAPI XUserCreateStatsEnumeratorByXuid(DWORD dwTitleId, XUID XuidPivot, DWORD dwNumRows, DWORD dwNumStatsSpecs, const XUSER_STATS_SPEC *pSpecs, DWORD *pcbBuffer, HANDLE *phEnum)
{
	TRACE_FX();
	if (!XuidPivot) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XuidPivot is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	return XUserCreateStatsEnumerator(0, 0, XuidPivot, dwTitleId, dwNumRows, dwNumStatsSpecs, pSpecs, pcbBuffer, phEnum);
}

// #5287
DWORD WINAPI XUserResetStatsView(DWORD dwUserIndex, DWORD dwViewID, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwUserIndex != -1) {
		if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
			return ERROR_NO_SUCH_USER;
		}
		if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
			return ERROR_NOT_LOGGED_ON;
		}
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return ERROR_FUNCTION_FAILED;

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

// #5288
DWORD WINAPI XUserGetProperty(DWORD dwUserIndex, DWORD *pcbActual, XUSER_PROPERTY *pProperty, XOVERLAPPED *pXOverlapped)
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
	if (!pcbActual) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbActual is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t result = ERROR_SUCCESS;

	if (*pcbActual < sizeof(XUSER_PROPERTY) || !pProperty) {
		*pcbActual = sizeof(XUSER_PROPERTY);
		result = ERROR_INSUFFICIENT_BUFFER;
	}
	else {
		uint8_t dataType;
		uint16_t dataSize;
		// I assume these properties are the same format as profile settings.
		GetXProfileSettingInfo(pProperty->dwPropertyId, &dataType, &dataSize, 0, 0);
		pProperty->value.type = dataType;

		switch (dataType) {
		case XUSER_DATA_TYPE_INT32:
			pProperty->value.nData = 0;
			break;
		case XUSER_DATA_TYPE_INT64:
			pProperty->value.i64Data = 0;
			break;
		case XUSER_DATA_TYPE_DOUBLE:
			pProperty->value.dblData = 0;
			break;
		case XUSER_DATA_TYPE_FLOAT:
			pProperty->value.fData = 0;
			break;
		case XUSER_DATA_TYPE_DATETIME: {
			SYSTEMTIME systemTime;
			GetSystemTime(&systemTime);
			FILETIME fileTime;
			SystemTimeToFileTime(&systemTime, &fileTime);
			pProperty->value.ftData = fileTime;
			break;
		}
		case XUSER_DATA_TYPE_BINARY:
			pProperty->value.binary.cbData = 0;
			break;
		case XUSER_DATA_TYPE_UNICODE: {
			if (*pcbActual < sizeof(XUSER_PROPERTY) + sizeof(wchar_t)) {
				pProperty->value.string.cbData = 0;
			}
			else {
				wchar_t *propStr = (wchar_t*)(((uint8_t*)pProperty) + sizeof(XUSER_PROPERTY));
				pProperty->value.string.cbData = 1;
				pProperty->value.string.pwszData = propStr;
				pProperty->value.string.pwszData[0] = 0;
			}
			break;
		}
		default:
			pProperty->value.type = XUSER_DATA_TYPE_NULL;
			break;
		}

		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
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

// #5289
DWORD WINAPI XUserGetContext(DWORD dwUserIndex, XUSER_CONTEXT *pContext, XOVERLAPPED *pXOverlapped)
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
	if (!pContext) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pContext is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t result = ERROR_FUNCTION_FAILED;

	pContext->dwContextId;
	pContext->dwValue = 0;
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

// #5290
float WINAPI XUserGetReputationStars(float fGamerRating)
{
	TRACE_FX();
	if (fGamerRating >= 100.0)
		return 5.0;
	if (fGamerRating > 0.0)
		return (float)(ceil((double)fGamerRating / 5.0) * 0.25);
	return 0.0;
}

// #5291
DWORD WINAPI XUserResetStatsViewAllUsers(DWORD dwViewId, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	return XUserResetStatsView(-1, dwViewId, pXOverlapped);
}

// #5292
DWORD WINAPI XUserSetContextEx(DWORD dwUserIndex, DWORD dwContextId, DWORD dwContextValue, XOVERLAPPED *pXOverlapped)
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
	if ((dwContextId & X_PROPERTY_SCOPE_MASK)
		&& dwContextId != X_CONTEXT_PRESENCE
		&& dwContextId != X_CONTEXT_GAME_TYPE
		&& dwContextId != X_CONTEXT_GAME_MODE
		&& dwContextId != X_CONTEXT_SESSION_JOINABLE
		&& dwContextId != X_CONTEXT_GAME_TYPE_RANKED
		&& dwContextId != X_CONTEXT_GAME_TYPE_STANDARD
	) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwContextId X_CONTEXT (0x%08x) is invalid.", __func__, dwContextId);
		return ERROR_INVALID_PARAMETER;
	}

	{
		EnterCriticalSection(&xlive_critsec_xsession);
		for (auto const &xsession : xlive_xsession_local_sessions) {
			LIVE_SESSION_XSESSION *xsessionDetails = xsession.second;

			uint32_t maxMembers = xsessionDetails->liveSession->slotsPublicMaxCount > xsessionDetails->liveSession->slotsPrivateMaxCount ? xsessionDetails->liveSession->slotsPublicMaxCount : xsessionDetails->liveSession->slotsPrivateMaxCount;
			uint32_t iMember = 0;
			for (; iMember < maxMembers; iMember++) {
				if (dwUserIndex == xsessionDetails->pSessionMembers[iMember].dwUserIndex) {
					break;
				}
			}
			if (iMember == maxMembers) {
				continue;
			}

			switch (dwContextId) {
			case X_CONTEXT_GAME_MODE:
				xsessionDetails->dwGameMode = dwContextValue;
				break;
			case X_CONTEXT_GAME_TYPE:
				xsessionDetails->dwGameType = dwContextValue;
				break;
			default:
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO dwContextId (0x%08x).", __func__, dwContextId);
				break;
			}
			break;
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
		// TODO if xsession not found should do something with it.
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

// #5277
VOID WINAPI XUserSetContext(DWORD dwUserIndex, DWORD dwContextId, DWORD dwContextValue)
{
	TRACE_FX();
	XUserSetContextEx(dwUserIndex, dwContextId, dwContextValue, NULL);
}

// #5293
DWORD WINAPI XUserSetPropertyEx(DWORD dwUserIndex, DWORD dwPropertyId, DWORD cbValue, CONST VOID *pvValue, XOVERLAPPED *pXOverlapped)
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
	if (cbValue == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cbValue is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pvValue) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pvValue is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!XLivepIsPropertyIdValid(dwPropertyId, TRUE)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwPropertyId (0x%08x) is invalid.", __func__, dwPropertyId);
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

// #5276
VOID WINAPI XUserSetProperty(DWORD dwUserIndex, DWORD dwPropertyId, DWORD cbValue, CONST VOID *pvValue)
{
	TRACE_FX();
	XUserSetPropertyEx(dwUserIndex, dwPropertyId, cbValue, pvValue, NULL);
}

static uint32_t GetProfileSettingsBufferSize(
	uint32_t title_id,
	uint32_t read_for_num_of_xuids,
	uint32_t num_setting_ids,
	const uint32_t *setting_ids,
	uint32_t *result_buffer_size,
	bool has_result_profile_settings)
{
	TRACE_FX();
	if (num_setting_ids == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s num_setting_ids is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (num_setting_ids > 0x20) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s num_setting_ids (0x%08x) is greater than 0x20.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!setting_ids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s setting_ids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_buffer_size is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (*result_buffer_size && !has_result_profile_settings) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (*result_buffer_size && !result_profile_settings).", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t result = ValidateSettingIds(title_id, num_setting_ids, setting_ids);
	if (result) {
		XLLN_DEBUG_LOG_ECODE(result, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ValidateSettingIds(...) error:", __func__);
		return result;
	}

	uint32_t settingBufferRequiredSize = 0;
	for (uint32_t i = 0; i < num_setting_ids; i++) {
		uint8_t dataType;
		uint16_t dataSize;
		GetXProfileSettingInfo(setting_ids[i], &dataType, &dataSize, 0, 0);
		settingBufferRequiredSize += sizeof(XUSER_PROFILE_SETTING);
		if (dataType == XUSER_DATA_TYPE_UNICODE || dataType == XUSER_DATA_TYPE_BINARY) {
			settingBufferRequiredSize += dataSize;
		}
	}
	settingBufferRequiredSize *= read_for_num_of_xuids;
	settingBufferRequiredSize += sizeof(XUSER_READ_PROFILE_SETTING_RESULT);

	if (!has_result_profile_settings || settingBufferRequiredSize > *result_buffer_size) {
		if (has_result_profile_settings) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "%s ERROR_INSUFFICIENT_BUFFER Had 0x%08x, requires 0x%08x Requires buffer size %u.", __func__, *result_buffer_size, settingBufferRequiredSize);
		}
		*result_buffer_size = settingBufferRequiredSize;
		return ERROR_INSUFFICIENT_BUFFER;
	}

	return ERROR_SUCCESS;
}

static const wchar_t *fileNameSetting = L"%s%08X.dat";

// #5337
DWORD WINAPI XUserWriteProfileSettings(DWORD dwUserIndex, DWORD dwNumSettings, const XUSER_PROFILE_SETTING *pSettings, XOVERLAPPED *pXOverlapped)
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
	if (dwNumSettings == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumSettings is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwNumSettings == 0xFFFFFFFF) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumSettings is -1.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pSettings) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pSettings is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	uint32_t result = ValidateSettings(0, dwNumSettings, pSettings);
	if (result) {
		XLLN_DEBUG_LOG_ECODE(result, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ValidateSettings(...) error:", __func__);
		return result;
	}
	
	if (!xlln_file_config_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XLLN Config is not set so the profile settings directories cannot be determined.");
		return ERROR_FUNCTION_FAILED;
	}
	
	wchar_t *pathConfig = PathFromFilename(xlln_file_config_path);
	wchar_t *pathUser = FormMallocString(L"%sprofile/user/%hs/", pathConfig, xlive_users_info[dwUserIndex]->szUserName);
	wchar_t *pathTitle = FormMallocString(L"%sprofile/title/%08X/%hs/", pathConfig, xlive_title_id, xlive_users_info[dwUserIndex]->szUserName);
	delete[] pathConfig;
	
	for (uint32_t iSetting = 0; iSetting < dwNumSettings; iSetting++) {
		uint8_t dataType;
		uint16_t dataSize;
		bool isTitleSetting;
		bool canWrite;
		GetXProfileSettingInfo(pSettings[iSetting].dwSettingId, &dataType, &dataSize, &isTitleSetting, &canWrite);
		
		const wchar_t *pathForSetting = isTitleSetting ? pathTitle : pathUser;
		wchar_t *filePathSetting = FormMallocString(fileNameSetting, pathForSetting, pSettings[iSetting].dwSettingId);
		
		uint32_t errorMkdir = EnsureDirectoryExists(pathForSetting);
		if (errorMkdir) {
			XLLN_DEBUG_LOG_ECODE(errorMkdir, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s EnsureDirectoryExists(...) error on path \"%ls\".", __func__, pathForSetting);
			result = errorMkdir;
			free(filePathSetting);
			break;
		}
		
		uint32_t fileSize = dataSize;
		uint8_t *dataLocation = 0;
		switch (dataType) {
			case XUSER_DATA_TYPE_INT32: {
				dataLocation = (uint8_t*)&pSettings[iSetting].data.nData;
				break;
			}
			case XUSER_DATA_TYPE_INT64: {
				dataLocation = (uint8_t*)&pSettings[iSetting].data.i64Data;
				break;
			}
			case XUSER_DATA_TYPE_DOUBLE: {
				dataLocation = (uint8_t*)&pSettings[iSetting].data.dblData;
				break;
			}
			case XUSER_DATA_TYPE_FLOAT: {
				dataLocation = (uint8_t*)&pSettings[iSetting].data.fData;
				break;
			}
			case XUSER_DATA_TYPE_DATETIME: {
				dataLocation = (uint8_t*)&pSettings[iSetting].data.ftData;
				break;
			}
			case XUSER_DATA_TYPE_UNICODE: {
				dataLocation = (uint8_t*)&pSettings[iSetting].data.string.pwszData;
				fileSize = pSettings[iSetting].data.string.cbData;
				break;
			}
		}
		
		if (fileSize > dataSize) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Setting (0x%08x) contents (size %u) is too big (need max size %u).", __func__, pSettings[iSetting].dwSettingId, fileSize, dataSize);
			result = ERROR_FILE_TOO_LARGE;
			break;
		}
		
		switch (dataType) {
			case XUSER_DATA_TYPE_BINARY: {
				dataLocation = (uint8_t*)&pSettings[iSetting].data.binary.pbData;
				fileSize = pSettings[iSetting].data.binary.cbData;
				break;
			}
		}
		
		if (dataType == XUSER_DATA_TYPE_UNICODE && fileSize < sizeof(wchar_t)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Invalid Setting (0x%08x) UNICODE fileSize (%u) must be greater than 2.", __func__, pSettings[iSetting].dwSettingId, fileSize);
			result = ERROR_FILE_INVALID;
			break;
		}
		if (dataType == XUSER_DATA_TYPE_UNICODE && fileSize % 2) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Invalid Setting (0x%08x) UNICODE/wchar_t fileSize is not multiple of 2 (%u).", __func__, pSettings[iSetting].dwSettingId, fileSize);
			result = ERROR_FILE_INVALID;
			break;
		}
		
		FILE *fp;
		errno_t errorFileOpen = _wfopen_s(&fp, filePathSetting, L"wb");
		if (errorFileOpen) {
			XLLN_DEBUG_LOG_ECODE(errorFileOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s fopen(\"%ls\", \"wb\") error:", __func__, filePathSetting);
			result = errorFileOpen;
			free(filePathSetting);
			break;
		}
		free(filePathSetting);
		
		if (dataType == XUSER_DATA_TYPE_UNICODE && (dataLocation[fileSize - 2] != 0 || dataLocation[fileSize - 1] != 0)) {
			dataLocation[fileSize - 2] = dataLocation[fileSize - 1] = 0;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s Invalid Setting (0x%08x) UNICODE/wchar_t text was not null terminated. Truncating data.", __func__, pSettings[iSetting].dwSettingId);
		}
		
		fwrite(dataLocation, sizeof(uint8_t), fileSize, fp);
		
		fclose(fp);
	}
	
	free(pathUser);
	free(pathTitle);
	
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

// #5339
DWORD WINAPI XUserReadProfileSettingsByXuid(
	DWORD dwTitleId,
	DWORD dwUserIndexRequester,
	DWORD read_for_num_of_xuids,
	const XUID *read_for_xuids,
	DWORD dwNumSettingIds,
	const uint32_t *pdwSettingIds,
	uint32_t *pcbResults,
	XUSER_READ_PROFILE_SETTING_RESULT *pResults,
	XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwUserIndexRequester >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndexRequester);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[dwUserIndexRequester]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndexRequester);
		return ERROR_NOT_LOGGED_ON;
	}
	if (dwNumSettingIds == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumSettingIds is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwNumSettingIds > 0x20) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumSettingIds (0x%08x) is greater than 0x20.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pdwSettingIds) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwSettingIds is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pcbResults) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbResults is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (*pcbResults && !pResults) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (*pcbResults && !pResults).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (read_for_num_of_xuids == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s read_for_num_of_xuids is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (read_for_num_of_xuids > 0x10) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s read_for_num_of_xuids (0x%08x) is greater than 0x10.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!read_for_xuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s read_for_xuids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	if (pResults && *pcbResults >= sizeof(XUSER_READ_PROFILE_SETTING_RESULT)) {
		pResults->dwSettingsLen = 0;
	}

	uint32_t result = GetProfileSettingsBufferSize(dwTitleId, read_for_num_of_xuids, dwNumSettingIds, pdwSettingIds, pcbResults, pResults != 0);
	if (result) {
		if (!(result == ERROR_INSUFFICIENT_BUFFER && !pResults)) {
			XLLN_DEBUG_LOG_ECODE(result, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s GetProfileSettingsBufferSize(...) error:", __func__);
		}
		return result;
	}

	if (!xlln_file_config_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XLLN Config is not set so the profile settings directories cannot be determined.");
		return ERROR_FUNCTION_FAILED;
	}

	uint32_t bufferRequired = sizeof(XUSER_READ_PROFILE_SETTING_RESULT) + (read_for_num_of_xuids * dwNumSettingIds * sizeof(XUSER_PROFILE_SETTING));

	if (*pcbResults >= bufferRequired) {
		pResults->dwSettingsLen = dwNumSettingIds;
		pResults->pSettings = (XUSER_PROFILE_SETTING*)((uint8_t*)pResults + sizeof(XUSER_READ_PROFILE_SETTING_RESULT));

		wchar_t *pathConfig = PathFromFilename(xlln_file_config_path);
		wchar_t *pathDefaults = FormMallocString(L"%sprofile/defaults/", pathConfig);

		for (uint32_t iReadForXuid = 0; iReadForXuid < read_for_num_of_xuids; iReadForXuid++) {

			char *username = 0;
			uint32_t iLocalUser = 0;
			for (; iLocalUser < XLIVE_LOCAL_USER_COUNT; iLocalUser++) {
				if (xlive_users_info[iLocalUser]->UserSigninState == eXUserSigninState_NotSignedIn) {
					continue;
				}
				if (xlive_users_info[iLocalUser]->xuid == read_for_xuids[iReadForXuid]) {
					username = CloneString(xlive_users_info[iLocalUser]->szUserName);
					break;
				}
			}
			if (iLocalUser == XLIVE_LOCAL_USER_COUNT) {
				iLocalUser = XUSER_INDEX_NONE;
			}

			wchar_t *pathUser = 0;
			wchar_t *pathTitle = 0;

			if (username) {
				pathUser = FormMallocString(L"%sprofile/user/%hs/", pathConfig, username);
				pathTitle = FormMallocString(L"%sprofile/title/%08X/%hs/", pathConfig, xlive_title_id, username);
			}

			for (uint32_t iSettingId = 0; iSettingId < dwNumSettingIds; iSettingId++) {
				uint32_t settingId = pdwSettingIds[iSettingId];
				uint8_t dataType;
				uint16_t dataSize;
				bool isTitleSetting;
				GetXProfileSettingInfo(settingId, &dataType, &dataSize, &isTitleSetting, 0);

				XUSER_PROFILE_SETTING *pSetting = &pResults->pSettings[iSettingId];
				pSetting->dwSettingId = settingId;
				pSetting->source = XSOURCE_NO_VALUE;
				pSetting->data.type = XUSER_DATA_TYPE_NULL;
				pSetting->user.dwUserIndex = iLocalUser;
				pSetting->user.xuid = read_for_xuids[iReadForXuid];

				uint32_t requiredReadSizeMin = dataSize;
				switch (dataType) {
				case XUSER_DATA_TYPE_INT32:
					requiredReadSizeMin = sizeof(LONG);
					break;
				case XUSER_DATA_TYPE_INT64:
					requiredReadSizeMin = sizeof(LONGLONG);
					break;
				case XUSER_DATA_TYPE_DOUBLE:
					requiredReadSizeMin = sizeof(double);
					break;
				case XUSER_DATA_TYPE_FLOAT:
					requiredReadSizeMin = sizeof(FLOAT);
					break;
				case XUSER_DATA_TYPE_DATETIME:
					requiredReadSizeMin = sizeof(FILETIME);
					break;
				case XUSER_DATA_TYPE_UNICODE:
					requiredReadSizeMin = sizeof(wchar_t);
					break;
				}

				for (uint8_t iPath = isTitleSetting ? 0 : 1; iPath < 3; iPath++) {
					wchar_t *filePathSetting;
					switch (iPath) {
					case 0:
						if (pathTitle) {
							filePathSetting = FormMallocString(fileNameSetting, pathTitle, settingId);
							break;
						}
						iPath++;
					case 1:
						if (pathUser) {
							filePathSetting = FormMallocString(fileNameSetting, pathUser, settingId);
							break;
						}
						iPath++;
					default:
						filePathSetting = FormMallocString(fileNameSetting, pathDefaults, settingId);
						break;
					}

					FILE *fp;
					errno_t errorFileOpen = _wfopen_s(&fp, filePathSetting, L"rb");
					if (errorFileOpen) {
						result = errorFileOpen;
						XLLN_DEBUG_LOG_ECODE(errorFileOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s fopen(\"%ls\", \"rb\") error:", __func__, filePathSetting);
					}

					free(filePathSetting);

					if (!errorFileOpen) {
						fseek(fp, 0, SEEK_END);
						uint32_t fileSize = ftell(fp);

						uint8_t *dataLocation = 0;
						switch (dataType) {
						case XUSER_DATA_TYPE_INT32:
							dataLocation = (uint8_t*)&pSetting->data.nData;
							break;
						case XUSER_DATA_TYPE_INT64:
							dataLocation = (uint8_t*)&pSetting->data.i64Data;
							break;
						case XUSER_DATA_TYPE_DOUBLE:
							dataLocation = (uint8_t*)&pSetting->data.dblData;
							break;
						case XUSER_DATA_TYPE_FLOAT:
							dataLocation = (uint8_t*)&pSetting->data.fData;
							break;
						case XUSER_DATA_TYPE_DATETIME:
							dataLocation = (uint8_t*)&pSetting->data.ftData;
							break;
						case XUSER_DATA_TYPE_UNICODE:
						case XUSER_DATA_TYPE_BINARY:
							dataLocation = (uint8_t*)((size_t)pResults + (size_t)bufferRequired);
							break;
						}

						if (fileSize != dataSize && dataType != XUSER_DATA_TYPE_UNICODE && dataType != XUSER_DATA_TYPE_BINARY) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Setting file contents size (%u) does not match dataSize (%u). File: \"%ls\".", __func__, fileSize, dataSize, filePathSetting);
						}
						else if (dataType == XUSER_DATA_TYPE_UNICODE && fileSize < sizeof(wchar_t)) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Invalid Setting file (0x%08x) UNICODE fileSize (%u) must be greater than 2. File: \"%ls\".", __func__, settingId, fileSize, filePathSetting);
						}
						else if (dataType == XUSER_DATA_TYPE_UNICODE && fileSize % 2) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Invalid Setting file (0x%08x) UNICODE/wchar_t fileSize is not multiple of 2 (%u). File: \"%ls\".", __func__, settingId, fileSize, filePathSetting);
						}
						else if (fileSize > dataSize) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Setting file contents (size %u) is too big (need max size %u). File: \"%ls\".", __func__, fileSize, dataSize, filePathSetting);
						}
						else {
							if (dataLocation && *pcbResults >= bufferRequired) {
								pSetting->source = iPath == 2 ? XSOURCE_DEFAULT : XSOURCE_TITLE;
								pSetting->data.type = dataType;
								uint32_t dataSizeToRead = dataSize;
								if (fileSize < dataSize) { // For UNICODE and BINARY.
									dataSizeToRead = dataSize;
									bufferRequired += fileSize;
									if (dataType != XUSER_DATA_TYPE_UNICODE && dataType != XUSER_DATA_TYPE_BINARY) {
										__debugbreak();
									}
								}
								if (dataType == XUSER_DATA_TYPE_UNICODE) {
									pSetting->data.string.cbData = dataSizeToRead;
									pSetting->data.string.pwszData = (wchar_t*)dataLocation;
								}
								else if (dataType == XUSER_DATA_TYPE_BINARY) {
									pSetting->data.binary.cbData = dataSizeToRead;
									pSetting->data.binary.pbData = dataLocation;
								}
								fseek(fp, 0, SEEK_SET);
								fread(dataLocation, sizeof(uint8_t), dataSizeToRead, fp);
								// Make sure the string is null terminated.
								if (dataType == XUSER_DATA_TYPE_UNICODE && (dataLocation[dataSizeToRead - 2] != 0 || dataLocation[dataSizeToRead - 1] != 0)) {
									dataLocation[dataSizeToRead - 2] = dataLocation[dataSizeToRead - 1] = 0;
									XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s Invalid Setting UNICODE/wchar_t text was not null terminated. Truncating read data. File: \"%ls\".", __func__, settingId, filePathSetting);
								}
							}
						}

						fclose(fp);
						break;
					}
				}
			}

			if (pathUser) {
				free(pathUser);
			}
			if (pathTitle) {
				free(pathTitle);
			}
		}

		delete[] pathConfig;
		free(pathDefaults);
	}

	if (*pcbResults < bufferRequired) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_FATAL, "%s ERROR_INSUFFICIENT_BUFFER even though the already allocated result buffer is defined.", __func__);
		__debugbreak();
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

// #5331
DWORD WINAPI XUserReadProfileSettings(
	DWORD dwTitleId,
	DWORD dwUserIndex,
	DWORD dwNumSettingIds,
	const uint32_t *pdwSettingIds,
	uint32_t *pcbResults,
	XUSER_READ_PROFILE_SETTING_RESULT *pResults,
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
	XUID readForXuids[] = { xlive_users_info[dwUserIndex]->xuid };
	uint32_t result = XUserReadProfileSettingsByXuid(dwTitleId, dwUserIndex, 1, readForXuids, dwNumSettingIds, pdwSettingIds, pcbResults, pResults, pXOverlapped);
	return result;
}

// #5346
DWORD WINAPI XUserEstimateRankForRating(DWORD dwNumRequests, const XUSER_RANK_REQUEST *pRankRequests, DWORD cbResults, XUSER_ESTIMATE_RANK_RESULTS *pResults, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!dwNumRequests) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumRequests is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pRankRequests) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pRankRequests is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cbResults && !pResults) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pResults is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cbResults < (sizeof(XUSER_ESTIMATE_RANK_RESULTS) + (sizeof(DWORD) * dwNumRequests))) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (cbResults < (sizeof(XUSER_ESTIMATE_RANK_RESULTS) + (sizeof(DWORD) * dwNumRequests))) (cbResults < 0x%08x).", __func__, cbResults, sizeof(XUSER_ESTIMATE_RANK_RESULTS) + (sizeof(DWORD) * dwNumRequests));
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t result = ERROR_FUNCTION_FAILED;

	(*pResults).dwNumRanks = 0;

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

// #5377
DWORD WINAPI XUserFindUsers(XUID qwUserId, DWORD dwUsers, const FIND_USER_INFO *pUsers, DWORD cbResults, FIND_USERS_RESPONSE *pResults, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (!qwUserId) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s qwUserId is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwUsers == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwUsers is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwUsers > 0x64) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwUsers (0x%08x) is greater than 0x64.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pUsers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pUsers is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cbResults < (24 * dwUsers + 8)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (cbResults < (24 * dwUsers + 8)) (0x%08x < 0x%08x).", __func__, cbResults, 24 * dwUsers + 8);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pResults) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pResults is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	uint32_t result = ERROR_FUNCTION_FAILED;

	(*pResults).dwResults = 0;

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
