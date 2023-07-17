#include <winsock2.h>
#include "xdefs.hpp"
#include "xcontent.hpp"
#include "xlive.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/xlln.hpp"

CRITICAL_SECTION xlive_critsec_xcontent;
// Key: enumerator handle (id).
// Value: Vector of ? that have already been returned for that enumerator.
std::map<HANDLE, DWORD> xlive_xcontent_enumerators;

// #5350
HRESULT WINAPI XLiveContentCreateAccessHandle(DWORD dwUserIndex, XLIVE_CONTENT_INFO *pContentInfo, DWORD dwLicenseInfoVersion, XLIVE_PROTECTED_BUFFER *xebBuffer, ULONG ulOffset, HANDLE *phContentAccess, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (!pContentInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pContentInfo is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (dwLicenseInfoVersion != XLIVE_LICENSE_INFO_VERSION) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwLicenseInfoVersion != XLIVE_LICENSE_INFO_VERSION) (%u != %u).", __func__, dwLicenseInfoVersion, XLIVE_LICENSE_INFO_VERSION);
		return E_INVALIDARG;
	}
	if (!xebBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xebBuffer is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!phContentAccess) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phContentAccess is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (pXOverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXOverlapped is not NULL.", __func__);
		return E_NOTIMPL;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	*phContentAccess = 0;
	return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	return S_OK;
}

// #5351
HRESULT WINAPI XLiveContentInstallPackage(XLIVE_CONTENT_INFO *pContentInfo, WCHAR *pszCabFilePath, XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS *pInstallCallbackParams)
{
	TRACE_FX();
	if (!pContentInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pContentInfo is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (pContentInfo->dwContentAPIVersion != 1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pContentInfo->dwContentAPIVersion != 1) (%u != 1).", __func__, pContentInfo->dwContentAPIVersion);
		return E_INVALIDARG;
	}
	if (pContentInfo->dwContentType != XCONTENTTYPE_MARKETPLACE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pContentInfo->dwContentType != XCONTENTTYPE_MARKETPLACE) (%u != %u).", __func__, pContentInfo->dwContentType, XCONTENTTYPE_MARKETPLACE);
		return E_INVALIDARG;
	}
	if (!pszCabFilePath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pszCabFilePath is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (pInstallCallbackParams) {
		if (pInstallCallbackParams->cbSize != sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V1) && pInstallCallbackParams->cbSize != sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V2)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pInstallCallbackParams->cbSize (%u) is not sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_(V1/V2)) (12/16).", __func__, pInstallCallbackParams->cbSize);
			return E_INVALIDARG;
		}
		if (!pInstallCallbackParams->pInstallCallback && pInstallCallbackParams->cbSize == sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V1)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pInstallCallbackParams->pInstallCallback is NULL.", __func__);
			return E_INVALIDARG;
		}
		if (!pInstallCallbackParams->pInstallCallback && !pInstallCallbackParams->pInstallCallbackEx && pInstallCallbackParams->cbSize == sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V2)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pInstallCallbackParams->pInstallCallback and pInstallCallbackParams->pInstallCallbackEx is NULL.", __func__);
			return E_INVALIDARG;
		}
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return E_UNEXPECTED;
	return E_ACCESSDENIED;
	return S_OK;
}

// #5352
HRESULT WINAPI XLiveContentUninstall(XLIVE_CONTENT_INFO *pContentInfo, XUID *pxuidFor, XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS *pInstallCallbackParams)
{
	TRACE_FX();
	if (!pContentInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pContentInfo is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (pContentInfo->dwContentAPIVersion != 1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pContentInfo->dwContentAPIVersion != 1) (%u != 1).", __func__, pContentInfo->dwContentAPIVersion);
		return E_INVALIDARG;
	}
	if (pContentInfo->dwContentType != XCONTENTTYPE_MARKETPLACE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pContentInfo->dwContentType != XCONTENTTYPE_MARKETPLACE) (%u != %u).", __func__, pContentInfo->dwContentType, XCONTENTTYPE_MARKETPLACE);
		return E_INVALIDARG;
	}
	if (pxuidFor && !*pxuidFor) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *pxuidFor is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (pxuidFor && !XUID_LIVE_ENABLED(*pxuidFor)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxuidFor is not a LIVE account.", __func__);
		return E_INVALIDARG;
	}
	if (pInstallCallbackParams) {
		if (pInstallCallbackParams->cbSize != sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V1) && pInstallCallbackParams->cbSize != sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V2)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pInstallCallbackParams->cbSize (%u) is not sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_(V1/V2)) (12/16).", __func__, pInstallCallbackParams->cbSize);
			return E_INVALIDARG;
		}
		if (!pInstallCallbackParams->pInstallCallback && pInstallCallbackParams->cbSize == sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V1)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pInstallCallbackParams->pInstallCallback is NULL.", __func__);
			return E_INVALIDARG;
		}
		if (!pInstallCallbackParams->pInstallCallback && !pInstallCallbackParams->pInstallCallbackEx && pInstallCallbackParams->cbSize == sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V2)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pInstallCallbackParams->pInstallCallback and pInstallCallbackParams->pInstallCallbackEx is NULL.", __func__);
			return E_INVALIDARG;
		}
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return E_UNEXPECTED;
	return E_ACCESSDENIED;
	return HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
	return S_OK;
}

// #5354
HRESULT WINAPI XLiveContentVerifyInstalledPackage(XLIVE_CONTENT_INFO *pContentInfo, XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS *pInstallCallbackParams)
{
	TRACE_FX();
	if (!pContentInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pContentInfo is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (pContentInfo->dwContentAPIVersion != 1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pContentInfo->dwContentAPIVersion != 1) (%u != 1).", __func__, pContentInfo->dwContentAPIVersion);
		return E_INVALIDARG;
	}
	if (pContentInfo->dwContentType != XCONTENTTYPE_MARKETPLACE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pContentInfo->dwContentType != XCONTENTTYPE_MARKETPLACE) (%u != %u).", __func__, pContentInfo->dwContentType, XCONTENTTYPE_MARKETPLACE);
		return E_INVALIDARG;
	}
	if (pInstallCallbackParams) {
		if (pInstallCallbackParams->cbSize != sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V1) && pInstallCallbackParams->cbSize != sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V2)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pInstallCallbackParams->cbSize (%u) is not sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_(V1/V2)) (12/16).", __func__, pInstallCallbackParams->cbSize);
			return E_INVALIDARG;
		}
		if (!pInstallCallbackParams->pInstallCallback && pInstallCallbackParams->cbSize == sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V1)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pInstallCallbackParams->pInstallCallback is NULL.", __func__);
			return E_INVALIDARG;
		}
		if (!pInstallCallbackParams->pInstallCallback && !pInstallCallbackParams->pInstallCallbackEx && pInstallCallbackParams->cbSize == sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V2)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pInstallCallbackParams->pInstallCallback and pInstallCallbackParams->pInstallCallbackEx is NULL.", __func__);
			return E_INVALIDARG;
		}
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return E_ABORT;
	return E_UNEXPECTED;
	return S_OK;
}

// #5355
HRESULT WINAPI XLiveContentGetPath(DWORD dwUserIndex, XLIVE_CONTENT_INFO *pContentInfo, WCHAR *pszPath, DWORD *pcchPath)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (!pContentInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pContentInfo is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (pContentInfo->dwContentAPIVersion != 1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pContentInfo->dwContentAPIVersion != 1) (%u != 1).", __func__, pContentInfo->dwContentAPIVersion);
		return E_INVALIDARG;
	}
	if (!pcchPath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcchPath is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!*pcchPath && pszPath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pszPath is not NULL when *pcchPath is 0.", __func__);
		return E_INVALIDARG;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return E_UNEXPECTED;
	// i think path needs to end with / or backslash when this is implemented. Also note the different macros for potentially different paths XCONTENTTYPE_.
	return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
	return S_OK;
}

// #5356
HRESULT WINAPI XLiveContentGetDisplayName(DWORD dwUserIndex, XLIVE_CONTENT_INFO *pContentInfo, WCHAR *pszDisplayName, DWORD *pcchDisplayName)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (!pContentInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pContentInfo is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (pContentInfo->dwContentAPIVersion != 1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pContentInfo->dwContentAPIVersion != 1) (%u != 1).", __func__, pContentInfo->dwContentAPIVersion);
		return E_INVALIDARG;
	}
	if (!pcchDisplayName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcchDisplayName is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!*pcchDisplayName && pszDisplayName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pszDisplayName is not NULL when *pcchDisplayName is 0.", __func__);
		return E_INVALIDARG;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return E_UNEXPECTED;
	return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
	return S_OK;
}

// #5357
HRESULT WINAPI XLiveContentGetThumbnail(DWORD dwUserIndex, XLIVE_CONTENT_INFO *pContentInfo, BYTE *pbThumbnail, DWORD *pcbThumbnail)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (!pContentInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pContentInfo is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (pContentInfo->dwContentAPIVersion != 1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pContentInfo->dwContentAPIVersion != 1) (%u != 1).", __func__, pContentInfo->dwContentAPIVersion);
		return E_INVALIDARG;
	}
	if (!pcbThumbnail) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbThumbnail is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!*pcbThumbnail && pbThumbnail) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pbThumbnail is not NULL when *pcbThumbnail is 0.", __func__);
		return E_INVALIDARG;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return E_UNEXPECTED;
	return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
	return S_OK;
}

// #5358
HRESULT WINAPI XLiveContentInstallLicense(XLIVE_CONTENT_INFO *pContentInfo, WCHAR *szLicenseFilePath, XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS *pInstallCallbackParams)
{
	TRACE_FX();
	if (!pContentInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pContentInfo is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (pContentInfo->dwContentAPIVersion != 1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pContentInfo->dwContentAPIVersion != 1) (%u != 1).", __func__, pContentInfo->dwContentAPIVersion);
		return E_INVALIDARG;
	}
	if (pContentInfo->dwContentType != XCONTENTTYPE_MARKETPLACE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pContentInfo->dwContentType != XCONTENTTYPE_MARKETPLACE) (%u != %u).", __func__, pContentInfo->dwContentType, XCONTENTTYPE_MARKETPLACE);
		return E_INVALIDARG;
	}
	if (!szLicenseFilePath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s szLicenseFilePath is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (pInstallCallbackParams) {
		if (pInstallCallbackParams->cbSize != sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V1) && pInstallCallbackParams->cbSize != sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V2)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pInstallCallbackParams->cbSize (%u) is not sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_(V1/V2)) (12/16).", __func__, pInstallCallbackParams->cbSize);
			return E_INVALIDARG;
		}
		if (!pInstallCallbackParams->pInstallCallback && pInstallCallbackParams->cbSize == sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V1)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pInstallCallbackParams->pInstallCallback is NULL.", __func__);
			return E_INVALIDARG;
		}
		if (!pInstallCallbackParams->pInstallCallback && !pInstallCallbackParams->pInstallCallbackEx && pInstallCallbackParams->cbSize == sizeof(XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS_V2)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pInstallCallbackParams->pInstallCallback and pInstallCallbackParams->pInstallCallbackEx is NULL.", __func__);
			return E_INVALIDARG;
		}
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return E_UNEXPECTED;
	return E_ACCESSDENIED;
	return HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
	return S_OK;
}

// #5360
DWORD WINAPI XLiveContentCreateEnumerator(DWORD cItems, XLIVE_CONTENT_RETRIEVAL_INFO *pContentRetrievalInfo, DWORD *pcbBuffer, HANDLE *phContent)
{
	TRACE_FX();
	if (!cItems) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cItems is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cItems > 100) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cItems (%u) is greater than 100.", __func__, cItems);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pcbBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcbBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!phContent) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phContent is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pContentRetrievalInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pContentRetrievalInfo is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (pContentRetrievalInfo->dwContentAPIVersion != 1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pContentRetrievalInfo->dwContentAPIVersion (%u) is not 1.", __func__, pContentRetrievalInfo->dwContentAPIVersion);
		return ERROR_INVALID_PARAMETER;
	}
	if (pContentRetrievalInfo->dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, pContentRetrievalInfo->dwUserIndex);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_users_info[pContentRetrievalInfo->dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, pContentRetrievalInfo->dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if ((pContentRetrievalInfo->dwRetrievalMask & XLIVE_CONTENT_FLAG_RETRIEVE_FOR_ALL_USERS) && (pContentRetrievalInfo->dwRetrievalMask & XLIVE_CONTENT_FLAG_RETRIEVE_BY_XUID)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((pContentRetrievalInfo->dwRetrievalMask & XLIVE_CONTENT_FLAG_RETRIEVE_FOR_ALL_USERS) && (pContentRetrievalInfo->dwRetrievalMask & XLIVE_CONTENT_FLAG_RETRIEVE_BY_XUID)).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (pContentRetrievalInfo->dwContentType != XCONTENTTYPE_MARKETPLACE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pContentRetrievalInfo->dwContentType (%u) is not XCONTENTTYPE_MARKETPLACE.", __func__, pContentRetrievalInfo->dwContentType);
		return ERROR_INVALID_PARAMETER;
	}
	if (!(pContentRetrievalInfo->dwRetrievalMask & XLIVE_CONTENT_FLAG_RETRIEVE_FOR_ALL_CONTENT_TYPES)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s !(pContentRetrievalInfo->dwRetrievalMask & XLIVE_CONTENT_FLAG_RETRIEVE_FOR_ALL_CONTENT_TYPES).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!(pContentRetrievalInfo->dwRetrievalMask & (XLIVE_CONTENT_FLAG_RETRIEVE_FOR_ALL_USERS | XLIVE_CONTENT_FLAG_RETRIEVE_BY_XUID))) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s !(pContentRetrievalInfo->dwRetrievalMask & (XLIVE_CONTENT_FLAG_RETRIEVE_FOR_ALL_USERS | XLIVE_CONTENT_FLAG_RETRIEVE_BY_XUID)).", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	*pcbBuffer = cItems * sizeof(XCONTENT_DATA);
	*phContent = CreateMutex(NULL, NULL, NULL);
	EnterCriticalSection(&xlive_critsec_xcontent);
	xlive_xcontent_enumerators[*phContent];
	LeaveCriticalSection(&xlive_critsec_xcontent);

	return ERROR_SUCCESS;
}

// #5361
HRESULT WINAPI XLiveContentRetrieveOffersByDate(DWORD dwUserIndex, DWORD dwOfferInfoVersion, SYSTEMTIME *pstStartDate, XLIVE_OFFER_INFO *pOfferInfoArray, DWORD *pcOfferInfo, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (dwOfferInfoVersion > XLIVE_OFFER_INFO_VERSION) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwOfferInfoVersion > XLIVE_OFFER_INFO_VERSION) (%u > %u).", __func__, dwOfferInfoVersion, XLIVE_OFFER_INFO_VERSION);
		return E_INVALIDARG;
	}
	if (!pOfferInfoArray) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pOfferInfoArray is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!pcOfferInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcOfferInfo is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (*pcOfferInfo != XLIVE_MAX_RETURNED_OFFER_INFO) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (*pcOfferInfo != XLIVE_MAX_RETURNED_OFFER_INFO) (%u != %u).", __func__, *pcOfferInfo, XLIVE_MAX_RETURNED_OFFER_INFO);
		return E_INVALIDARG;
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

// #5362
BOOL WINAPI XLiveMarketplaceDoesContentIdMatch(const BYTE *pbContentId, const XLIVE_CONTENT_INFO *pContentData)
{
	TRACE_FX();
	if (!pbContentId) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pbContentId is NULL.", __func__);
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	if (!pContentData) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pContentData is NULL.", __func__);
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	bool result = memcmp(pbContentId, &(pContentData->abContentID), XLIVE_CONTENT_ID_SIZE) == 0;
	return result ? TRUE : FALSE;
}

// #5363
HRESULT WINAPI XLiveContentGetLicensePath(DWORD dwUserIndex, XLIVE_CONTENT_INFO *pContentInfo, WCHAR *pszLicensePath, DWORD *pcchPath)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (!pContentInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pContentInfo is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (pContentInfo->dwContentAPIVersion != 1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pContentInfo->dwContentAPIVersion != 1) (%u != 1).", __func__, pContentInfo->dwContentAPIVersion);
		return E_INVALIDARG;
	}
	if (!pcchPath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcchPath is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!*pcchPath && pszLicensePath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pszLicensePath is not NULL when *pcchPath is 0.", __func__);
		return E_INVALIDARG;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return E_UNEXPECTED;
	return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
	return S_OK;
}

// #5367
DWORD WINAPI XContentGetMarketplaceCounts(DWORD dwUserIndex, DWORD dwContentCategories, DWORD cbResults, XOFFERING_CONTENTAVAILABLE_RESULT *pResults, XOVERLAPPED *pXOverlapped)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (!cbResults) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cbResults is 0.", __func__);
		return E_INVALIDARG;
	}
	if (!pResults) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pResults is NULL.", __func__);
		return E_INVALIDARG;
	}

	pResults->dwNewOffers = 0;
	pResults->dwTotalOffers = 0;

	uint32_t result = S_OK;
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
