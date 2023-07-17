#include <winsock2.h>
#include "xdefs.hpp"
#include "xmarketplace.hpp"
#include "xlive.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/xlln.hpp"

CRITICAL_SECTION xlive_critsec_xmarketplace;
// Key: enumerator handle (id).
// Value: Vector of ? that have already been returned for that enumerator.
std::map<HANDLE, DWORD> xlive_xmarketplace_enumerators;

// #5370
DWORD WINAPI XMarketplaceConsumeAssets(DWORD dwUserIndex, DWORD cAssets, const XMARKETPLACE_ASSET *pAssets, XOVERLAPPED *pXOverlapped)
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
	if (!cAssets) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cAssets is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pAssets) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pAssets is NULL.", __func__);
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

// #5371
DWORD WINAPI XMarketplaceCreateAssetEnumerator(DWORD dwUserIndex, DWORD cItem, DWORD *pcbBuffer, HANDLE *phEnum)
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
	if (!cItem) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cItem is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cItem > 0x64) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cItem (%u) is greater than 0x64.", __func__, cItem);
		return ERROR_INVALID_PARAMETER;
	}
	if (!phEnum) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phEnum is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	if (pcbBuffer) {
		// i think 2524 is that struct including the maximum size of each of the pointers in it.
		//*pcbBuffer = 2524 * cItem;
		*pcbBuffer = (sizeof(XMARKETPLACE_CONTENTOFFER_INFO*) + sizeof(XMARKETPLACE_CONTENTOFFER_INFO) + 2416) * cItem;
	}

	*phEnum = CreateMutex(NULL, NULL, NULL);
	EnterCriticalSection(&xlive_critsec_xmarketplace);
	xlive_xmarketplace_enumerators[*phEnum];
	LeaveCriticalSection(&xlive_critsec_xmarketplace);

	return ERROR_SUCCESS;
}

// #5372
DWORD WINAPI XMarketplaceCreateOfferEnumerator(DWORD dwUserIndex, DWORD dwOfferType, DWORD dwContentCategories, DWORD cItem, DWORD *pcbBuffer, HANDLE *phEnum)
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
	if (!dwOfferType) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwOfferType is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!cItem) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cItem is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cItem > 0x64) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cItem (%u) is greater than 0x64.", __func__, cItem);
		return ERROR_INVALID_PARAMETER;
	}
	if (!phEnum) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phEnum is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	if (pcbBuffer) {
		// i think 2524 is that struct including the maximum size of each of the pointers in it.
		//*pcbBuffer = 2524 * cItem;
		*pcbBuffer = (sizeof(XMARKETPLACE_CONTENTOFFER_INFO*) + sizeof(XMARKETPLACE_CONTENTOFFER_INFO) + 2416) * cItem;
	}

	*phEnum = CreateMutex(NULL, NULL, NULL);
	EnterCriticalSection(&xlive_critsec_xmarketplace);
	xlive_xmarketplace_enumerators[*phEnum];
	LeaveCriticalSection(&xlive_critsec_xmarketplace);

	return ERROR_SUCCESS;
}

// #5374
DWORD WINAPI XMarketplaceGetDownloadStatus(DWORD dwUserIndex, ULONGLONG qwOfferID, DWORD *pdwResult)
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
	if (!qwOfferID) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s qwOfferID is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);

	return ERROR_NOT_FOUND;
	return ERROR_IO_PENDING;
	return ERROR_DISK_FULL;
}

// #5375
VOID WINAPI XMarketplaceGetImageUrl(DWORD dwTitleID, ULONGLONG qwOfferID, DWORD cchStringBuffer, WCHAR *pwszStringBuffer)
{
	TRACE_FX();
	if (cchStringBuffer < XMARKETPLACE_IMAGE_URL_MINIMUM_WCHARCOUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (cchStringBuffer < XMARKETPLACE_IMAGE_URL_MINIMUM_WCHARCOUNT) (0x%08x < 0x%08x).", __func__, cchStringBuffer, XMARKETPLACE_IMAGE_URL_MINIMUM_WCHARCOUNT);
		return;
	}
	if (!pwszStringBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pwszStringBuffer is NULL.", __func__);
		return;
	}
	swprintf(pwszStringBuffer, cchStringBuffer, L"//global/t:%08x/marketplace/0/%016I64x", dwTitleID, qwOfferID);
}

// #5376
DWORD WINAPI XMarketplaceCreateOfferEnumeratorByOffering(DWORD dwUserIndex, DWORD cItem, const ULONGLONG *pqwNumOffersIds, WORD cOfferIDs, DWORD *pcbBuffer, HANDLE *phEnum)
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
	if (!cItem) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cItem is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cItem > 0x64) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cItem (%u) is greater than 0x64.", __func__, cItem);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pqwNumOffersIds) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pqwNumOffersIds is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!cOfferIDs) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cOfferIDs is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cOfferIDs > 20) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cOfferIDs (%u) is greater than 20.", __func__, cItem);
		return ERROR_INVALID_PARAMETER;
	}
	if (!phEnum) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phEnum is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	if (pcbBuffer) {
		// i think 2524 is that struct including the maximum size of each of the pointers in it.
		//*pcbBuffer = 2524 * cItem;
		*pcbBuffer = (sizeof(XMARKETPLACE_CONTENTOFFER_INFO*) + sizeof(XMARKETPLACE_CONTENTOFFER_INFO) + 2416) * cItem;
	}

	*phEnum = CreateMutex(NULL, NULL, NULL);
	EnterCriticalSection(&xlive_critsec_xmarketplace);
	xlive_xmarketplace_enumerators[*phEnum];
	LeaveCriticalSection(&xlive_critsec_xmarketplace);

	return ERROR_SUCCESS;
}
