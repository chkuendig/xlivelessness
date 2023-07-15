#include <winsock2.h>
#include "xdefs.hpp"
#include "xstorage.hpp"
#include "xlive.hpp"
#include "../xlln/debug-text.hpp"
#include "../utils/utils.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/xlln-config.hpp"

static uint32_t ToXStorageError(uint32_t error)
{
	uint32_t result = error;
	
	switch (result) {
		case ERROR_ACCESS_DENIED:
			result = XONLINE_E_STORAGE_ACCESS_DENIED;
			break;
		case ERROR_FILE_TOO_LARGE:
			result = XONLINE_E_STORAGE_FILE_IS_TOO_BIG;
			break;
		case ERROR_INSUFFICIENT_BUFFER:
			result = XONLINE_E_STORAGE_FILE_IS_TOO_BIG;
			break;
		case ERROR_FILE_NOT_FOUND:
			result = XONLINE_E_STORAGE_FILE_NOT_FOUND;
			break;
		case ERROR_FILE_EXISTS:
			result = XONLINE_E_STORAGE_FILE_ALREADY_EXISTS;
			break;
	}
	return result;
}

// #5304
DWORD WINAPI XStorageUploadFromMemoryGetProgress(XOVERLAPPED *pXOverlapped, DWORD *pdwPercentComplete, ULONGLONG *pqwNumerator, ULONGLONG *pqwDenominator)
{
	TRACE_FX();
	if (!pXOverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXOverlapped is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pqwNumerator != !pqwDenominator) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pqwNumerator and pqwDenominator must both be defined or undefined.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (pdwPercentComplete) {
		*pdwPercentComplete = 100;
	}
	if (pqwNumerator) {
		*pqwNumerator = 100;
	}
	if (pqwDenominator) {
		*pqwDenominator = 100;
	}
	
	return ERROR_SUCCESS;
}

// #5305
DWORD WINAPI XStorageUploadFromMemory(DWORD dwUserIndex, const WCHAR *wszServerPath, DWORD dwBufferSize, const BYTE *pbBuffer, XOVERLAPPED *pXOverlapped)
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
	if (!wszServerPath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s wszServerPath is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwBufferSize == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwBufferSize is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pbBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pbBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (!xlln_file_config_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XLLN Config is not set so the storage directory cannot be determined.");
		return XONLINE_E_STORAGE_CANNOT_FIND_PATH;
	}
	
	uint32_t result = ERROR_SUCCESS;
	
	wchar_t *configPath = PathFromFilename(xlln_file_config_path);
	wchar_t *storageFilePath = FormMallocString(L"%s%s", configPath, wszServerPath);
	delete[] configPath;
	wchar_t *storagePath = PathFromFilename(storageFilePath);
	uint32_t errorMkdir = EnsureDirectoryExists(storagePath);
	if (errorMkdir) {
		XLLN_DEBUG_LOG_ECODE(errorMkdir, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s EnsureDirectoryExists(...) error on path \"%ls\".", __func__, storagePath);
		result = XONLINE_E_STORAGE_INVALID_STORAGE_PATH;
	}
	delete[] storagePath;
	
	if (!errorMkdir) {
		FILE *fp;
		errno_t errorFileOpen = _wfopen_s(&fp, storageFilePath, L"wb");
		if (errorFileOpen) {
			result = errorFileOpen;
			XLLN_DEBUG_LOG_ECODE(errorFileOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s fopen(\"%ls\", \"wb\") error:", __func__, storageFilePath);
		}
		else {
			fwrite(pbBuffer, 1, dwBufferSize, fp);
			fclose(fp);
		}
	}
	delete[] storageFilePath;
	
	result = ToXStorageError(result);
	
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

// #5306
DWORD WINAPI XStorageEnumerate(
	DWORD dwUserIndex,
	const WCHAR *wszServerPath,
	DWORD dwStartingIndex,
	DWORD dwMaxResultsToReturn,
	DWORD cbResults,
	XSTORAGE_ENUMERATE_RESULTS *pResults,
	XOVERLAPPED *pXOverlapped
)
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
	if (!wszServerPath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s wszServerPath is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwMaxResultsToReturn > MAX_STORAGE_RESULTS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwMaxResultsToReturn > MAX_STORAGE_RESULTS) (%u >= %u).", __func__, dwMaxResultsToReturn, MAX_STORAGE_RESULTS);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwStartingIndex >= MAX_STORAGE_RESULTS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwStartingIndex >= MAX_STORAGE_RESULTS) (%u >= %u).", __func__, dwStartingIndex, MAX_STORAGE_RESULTS);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwStartingIndex + dwMaxResultsToReturn < dwStartingIndex) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwStartingIndex + dwMaxResultsToReturn < dwStartingIndex) (%u + %u >= %u).", __func__, dwStartingIndex, dwMaxResultsToReturn, dwStartingIndex);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwStartingIndex + dwMaxResultsToReturn > MAX_STORAGE_RESULTS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (dwStartingIndex + dwMaxResultsToReturn > MAX_STORAGE_RESULTS) (%u + %u > %u).", __func__, dwStartingIndex, dwMaxResultsToReturn, MAX_STORAGE_RESULTS);
		return ERROR_INVALID_PARAMETER;
	}
	if (cbResults < sizeof(XSTORAGE_ENUMERATE_RESULTS) + (dwMaxResultsToReturn * (sizeof(XSTORAGE_FILE_INFO) + (XONLINE_MAX_PATHNAME_LENGTH * sizeof(WCHAR))))) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s (cbResults < sizeof(XSTORAGE_ENUMERATE_RESULTS) + (dwMaxResultsToReturn * (sizeof(XSTORAGE_FILE_INFO) + (XONLINE_MAX_PATHNAME_LENGTH * sizeof(WCHAR))))) (%u < %u)."
			, __func__
			, cbResults
			, sizeof(XSTORAGE_ENUMERATE_RESULTS) + (dwMaxResultsToReturn * (sizeof(XSTORAGE_FILE_INFO) + (XONLINE_MAX_PATHNAME_LENGTH * sizeof(WCHAR))))
		);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pResults) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pResults is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	
	pResults->dwNumItemsReturned = 0;
	pResults->dwTotalNumItems = 0;
	
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

// #5307
DWORD WINAPI XStorageDownloadToMemoryGetProgress(XOVERLAPPED *pXOverlapped, DWORD *pdwPercentComplete, ULONGLONG *pqwNumerator, ULONGLONG *pqwDenominator)
{
	TRACE_FX();
	if (!pXOverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pXOverlapped is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pqwNumerator != !pqwDenominator) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pqwNumerator and pqwDenominator must both be defined or undefined.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (pdwPercentComplete) {
		*pdwPercentComplete = 100;
	}
	if (pqwNumerator) {
		*pqwNumerator = 100;
	}
	if (pqwDenominator) {
		*pqwDenominator = 100;
	}
	
	return ERROR_SUCCESS;
}

// #5308
DWORD WINAPI XStorageDelete(DWORD dwUserIndex, const WCHAR *wszServerPath, XOVERLAPPED *pXOverlapped)
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
	if (!wszServerPath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s wszServerPath is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (!xlln_file_config_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XLLN Config is not set so the storage directory cannot be determined.");
		return XONLINE_E_STORAGE_CANNOT_FIND_PATH;
	}
	
	uint32_t result = ERROR_SUCCESS;
	
	wchar_t *configPath = PathFromFilename(xlln_file_config_path);
	wchar_t *storageFilePath = FormMallocString(L"%s%s", configPath, wszServerPath);
	delete[] configPath;
	wchar_t *storagePath = PathFromFilename(storageFilePath);
	uint32_t errorMkdir = EnsureDirectoryExists(storagePath);
	if (errorMkdir) {
		XLLN_DEBUG_LOG_ECODE(errorMkdir, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s EnsureDirectoryExists(...) error on path \"%ls\".", __func__, storagePath);
		result = XONLINE_E_STORAGE_INVALID_STORAGE_PATH;
	}
	delete[] storagePath;
	
	if (!errorMkdir) {
		wchar_t *storageFilePathExt = FormMallocString(L"\\\\?\\%s", storageFilePath);
		BOOL resultDelete = DeleteFileW(storageFilePathExt);
		if (!resultDelete) {
			result = GetLastError();
			XLLN_DEBUG_LOG_ECODE(result, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s DeleteFileW(\"%ls\") error:", __func__, storageFilePathExt);
		}
		delete[] storageFilePathExt;
	}
	
	delete[] storageFilePath;
	
	result = ToXStorageError(result);
	
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

// #5344
DWORD WINAPI XStorageBuildServerPath(
	DWORD dwUserIndex,
	XSTORAGE_FACILITY storageFacility,
	CONST void *pvStorageFacilityInfo,
	DWORD dwStorageFacilityInfoSize,
	LPCWSTR *lpwszItemName,
	WCHAR *pwszServerPath,
	DWORD *pdwServerPathLength)
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
	if (!pvStorageFacilityInfo && dwStorageFacilityInfoSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!pvStorageFacilityInfo && dwStorageFacilityInfoSize).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!lpwszItemName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s lpwszItemName is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!*lpwszItemName) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *lpwszItemName is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pwszServerPath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pwszServerPath is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pdwServerPathLength) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwServerPathLength is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (storageFacility != XSTORAGE_FACILITY_GAME_CLIP && storageFacility != XSTORAGE_FACILITY_PER_TITLE && storageFacility != XSTORAGE_FACILITY_PER_USER_TITLE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s storageFacility (%u) enum is unknown.", __func__, storageFacility);
		return ERROR_INVALID_PARAMETER;
	}
	if (storageFacility == XSTORAGE_FACILITY_GAME_CLIP && !pvStorageFacilityInfo) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (storageFacility == XSTORAGE_FACILITY_GAME_CLIP && !pvStorageFacilityInfo).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (pvStorageFacilityInfo && dwStorageFacilityInfoSize < sizeof(XSTORAGE_FACILITY_INFO_GAME_CLIP)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwStorageFacilityInfoSize (%u) must be at least %u when pvStorageFacilityInfo is not NULL.", __func__, dwStorageFacilityInfoSize, sizeof(XSTORAGE_FACILITY_INFO_GAME_CLIP));
		return ERROR_INVALID_PARAMETER;
	}
	
	char *username = CloneString(xlive_users_info[dwUserIndex]->szUserName);
	ReplaceFilePathSensitiveChars(username);
	wchar_t *itemName = CloneString((wchar_t*)lpwszItemName);
	if (wcschr(itemName, L'*') != 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "(RESEARCH) %s itemName contains wildcard *.", __func__);
	}
	ReplaceFilePathSensitiveChars(itemName); // '*' is a wildcard to allow multiple item enumeration?
	wchar_t *storageFilePath = FormMallocString(L"storage/%hs/%s", username, itemName);
	delete[] username;
	delete[] itemName;
	size_t storageFilePathBufSize = (wcslen(storageFilePath) + 1) * sizeof(wchar_t);
	
	if (storageFilePathBufSize > *pdwServerPathLength) {
		free(storageFilePath);
		return ERROR_INSUFFICIENT_BUFFER;
	}
	
	memcpy(pwszServerPath, storageFilePath, storageFilePathBufSize);
	*pdwServerPathLength = storageFilePathBufSize;
	
	free(storageFilePath);
	return ERROR_SUCCESS;
}

// #5309
DWORD WINAPI XStorageBuildServerPathByXuid(
	XUID xuidUser,
	XSTORAGE_FACILITY storageFacility,
	CONST void *pvStorageFacilityInfo,
	DWORD dwStorageFacilityInfoSize,
	LPCWSTR *lpwszItemName,
	WCHAR *pwszServerPath,
	DWORD *pdwServerPathLength)
{
	TRACE_FX();
	size_t iUser = 0;
	for (; iUser < XLIVE_LOCAL_USER_COUNT; iUser++) {
		if (xlive_users_info[iUser]->xuid == xuidUser) {
			break;
		}
	}
	if (iUser >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Local XUID 0x%016x does not exist.", __func__, xuidUser);
		return ERROR_NO_SUCH_USER;
	}
	return XStorageBuildServerPath(iUser, storageFacility, pvStorageFacilityInfo, dwStorageFacilityInfoSize, lpwszItemName, pwszServerPath, pdwServerPathLength);
}

// #5345
DWORD WINAPI XStorageDownloadToMemory(
	DWORD dwUserIndex,
	const WCHAR *wszServerPath,
	DWORD dwBufferSize,
	const BYTE *pbBuffer,
	DWORD cbResults,
	XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS *pResults,
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
	if (!wszServerPath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s wszServerPath is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!*wszServerPath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *wszServerPath is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (dwBufferSize == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwBufferSize is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!pbBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pbBuffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (cbResults != sizeof(XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (cbResults != sizeof(XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS)) (%u != %u).", __func__, cbResults, sizeof(XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS));
		return ERROR_INVALID_PARAMETER;
	}
	if (!pResults) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pResults is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (!xlln_file_config_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XLLN Config is not set so the storage directory cannot be determined.");
		return XONLINE_E_STORAGE_CANNOT_FIND_PATH;
	}
	
	uint32_t result = ERROR_SUCCESS;
	
	wchar_t *configPath = PathFromFilename(xlln_file_config_path);
	wchar_t *storageFilePath = FormMallocString(L"%s%s", configPath, wszServerPath);
	delete[] configPath;
	wchar_t *storagePath = PathFromFilename(storageFilePath);
	uint32_t errorMkdir = EnsureDirectoryExists(storagePath);
	if (errorMkdir) {
		XLLN_DEBUG_LOG_ECODE(errorMkdir, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s EnsureDirectoryExists(...) error on path \"%ls\".", __func__, storagePath);
		result = XONLINE_E_STORAGE_INVALID_STORAGE_PATH;
	}
	delete[] storagePath;
	
	if (!errorMkdir) {
		FILE *fp;
		errno_t errorFileOpen = _wfopen_s(&fp, storageFilePath, L"rb");
		if (errorFileOpen) {
			result = errorFileOpen;
			XLLN_DEBUG_LOG_ECODE(errorFileOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s fopen(\"%ls\", \"rb\") error:", __func__, storageFilePath);
		}
		else {
			fseek(fp, 0, SEEK_END);
			int32_t fileSize = ftell(fp);
			
			pResults->dwBytesTotal = fileSize;
			pResults->xuidOwner = xlive_users_info[dwUserIndex]->xuid;
			
			if (fileSize < 0 || (uint32_t)fileSize > dwBufferSize) {
				result = ERROR_INSUFFICIENT_BUFFER;
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (fileSize > dwBufferSize) (%u > %u).", __func__, fileSize, dwBufferSize);
				fclose(fp);
			}
			else {
				fseek(fp, 0, SEEK_SET);
				fread((void *)pbBuffer, 1, fileSize, fp);
				fclose(fp);
				
				HANDLE hFile = CreateFileW(storageFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hFile == INVALID_HANDLE_VALUE) {
					result = GetLastError();
					XLLN_DEBUG_LOG_ECODE(result, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s CreateFileW(\"%ls\", GENERIC_READ, ...) error:", __func__, storageFilePath);
					GetSystemTimeAsFileTime(&pResults->ftCreated);
				}
				else {
					GetFileTime(hFile, &pResults->ftCreated, NULL, NULL);
					CloseHandle(hFile);
				}
			}
		}
	}
	delete[] storageFilePath;
	
	result = ToXStorageError(result);
	
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
