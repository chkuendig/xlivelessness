#include <winsock2.h>
#include "voice.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/xlln.hpp"
#include <dsound.h>

bool xlive_xhv_engine_enabled = true;
// There are two different versions.
// See XHV_VOICECHAT_MODE_PACKET_SIZE_NEW and XHV_VOICECHAT_MODE_PACKET_SIZE_OLD.
// TODO figure out when to use which version.
bool xlive_xhv_engine_version_new = false;

// #5008
INT WINAPI XHVCreateEngine(XHV_INIT_PARAMS *pParams, HANDLE *phWorkerThread, IXHVEngine **ppEngine)
{
	TRACE_FX();
	if (!pParams) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pParams is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!ppEngine) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ppEngine is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (phWorkerThread) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s phWorkerThread is not officially supported.", __func__);
		return E_INVALIDARG;
	}
	if (!pParams->dwMaxLocalTalkers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pParams->dwMaxLocalTalkers is 0.", __func__);
		return E_INVALIDARG;
	}
	if (pParams->dwMaxLocalTalkers > XHV_MAX_LOCAL_TALKERS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pParams->dwMaxLocalTalkers (%u) is greater than %u.", __func__, pParams->dwMaxLocalTalkers, XHV_MAX_LOCAL_TALKERS);
		return E_INVALIDARG;
	}
	if (pParams->dwMaxRemoteTalkers > XHV_MAX_REMOTE_TALKERS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pParams->dwMaxRemoteTalkers (%u) is greater than %u.", __func__, pParams->dwMaxRemoteTalkers, XHV_MAX_REMOTE_TALKERS);
		return E_INVALIDARG;
	}
	if (pParams->dwNumLocalTalkerEnabledModes > XHV_MAX_PROCESSING_MODES) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pParams->dwNumLocalTalkerEnabledModes (%u) is greater than %u.", __func__, pParams->dwNumLocalTalkerEnabledModes, XHV_MAX_PROCESSING_MODES);
		return E_INVALIDARG;
	}
	if (pParams->dwNumRemoteTalkerEnabledModes > XHV_MAX_PROCESSING_MODES) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pParams->dwNumRemoteTalkerEnabledModes (%u) is greater than %u.", __func__, pParams->dwNumRemoteTalkerEnabledModes, XHV_MAX_PROCESSING_MODES);
		return E_INVALIDARG;
	}
	if (!pParams->dwNumLocalTalkerEnabledModes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pParams->dwNumLocalTalkerEnabledModes is 0.", __func__);
		return E_INVALIDARG;
	}
	if (!pParams->dwMaxRemoteTalkers && pParams->dwNumRemoteTalkerEnabledModes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!pParams->dwMaxRemoteTalkers && pParams->dwNumRemoteTalkerEnabledModes).", __func__);
		return E_INVALIDARG;
	}
	if (pParams->dwMaxRemoteTalkers && !pParams->dwNumRemoteTalkerEnabledModes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pParams->dwMaxRemoteTalkers && !pParams->dwNumRemoteTalkerEnabledModes).", __func__);
		return E_INVALIDARG;
	}
	if (pParams->dwNumLocalTalkerEnabledModes > 0 && !pParams->localTalkerEnabledModes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pParams->dwNumLocalTalkerEnabledModes > 0 && !pParams->localTalkerEnabledModes).", __func__);
		return E_INVALIDARG;
	}
	if (pParams->dwNumRemoteTalkerEnabledModes > 0 && !pParams->remoteTalkerEnabledModes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pParams->dwNumRemoteTalkerEnabledModes > 0 && !pParams->remoteTalkerEnabledModes).", __func__);
		return E_INVALIDARG;
	}
	for (uint32_t iProcessingMode = 0; iProcessingMode < pParams->dwNumLocalTalkerEnabledModes; iProcessingMode++) {
		if (pParams->localTalkerEnabledModes[iProcessingMode] != XHV_LOOPBACK_MODE && pParams->localTalkerEnabledModes[iProcessingMode] != XHV_VOICECHAT_MODE) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Invalid pParams->localTalkerEnabledModes item specified (0x%08x) Must be XHV_LOOPBACK_MODE and/or XHV_VOICECHAT_MODE.", __func__, pParams->localTalkerEnabledModes[iProcessingMode]);
			return E_INVALIDARG;
		}
	}
	for (uint32_t iProcessingMode = 0; iProcessingMode < pParams->dwNumRemoteTalkerEnabledModes; iProcessingMode++) {
		if (pParams->remoteTalkerEnabledModes[iProcessingMode] != XHV_LOOPBACK_MODE && pParams->remoteTalkerEnabledModes[iProcessingMode] != XHV_VOICECHAT_MODE) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Invalid pParams->remoteTalkerEnabledModes item specified (0x%08x) Must be XHV_LOOPBACK_MODE and/or XHV_VOICECHAT_MODE.", __func__, pParams->remoteTalkerEnabledModes[iProcessingMode]);
			return E_INVALIDARG;
		}
	}
	if (pParams->bCustomVADProvided && !pParams->pfnMicrophoneRawDataReady) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pParams->bCustomVADProvided yet pParams->pfnMicrophoneRawDataReady is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!pParams->hwndFocus) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pParams->hwndFocus is NULL.", __func__);
		return E_INVALIDARG;
	}
	
	if (!xlive_xhv_engine_enabled) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "%s XHV Engines are disabled.", __func__);
		return DSERR_NODRIVER;
	}
	
	XHVEngine *xhvEngine = new XHVEngine(pParams);
	
	if (pParams->bCustomVADProvided) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "%s A custom voice activation detection (VAD) callback has been provided.", __func__);
	}
	
	*ppEngine = xhvEngine;
	
	return S_OK;
}

// #5314
INT WINAPI XUserMuteListQuery(DWORD dwUserIndex, XUID XuidRemoteTalker, BOOL *pfOnMuteList)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return ERROR_INVALID_PARAMETER;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState != eXUserSigninState_SignedInToLive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in to LIVE.", __func__, dwUserIndex);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!pfOnMuteList) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pfOnMuteList is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!XUID_LIVE_ENABLED(XuidRemoteTalker)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XuidRemoteTalker (0x%016I64x) is not a LIVE enabled account.", __func__, XuidRemoteTalker);
		return ERROR_INVALID_PARAMETER;
	}
	
	*pfOnMuteList = FALSE;
	
	return S_OK;
}

__stdcall XHVEngine::XHVEngine(XHV_INIT_PARAMS *init_params)
{
	TRACE_FX();
	
	InitializeCriticalSection(&this->xhv_lock);
	
	xhv_reference_count = 1;
	
	this->registered_users_max_local = init_params->dwMaxLocalTalkers;
	this->registered_users_max_remote = init_params->dwMaxRemoteTalkers;
	this->relax_privileges = !!(init_params->bRelaxPrivileges);
	this->hwnd_title_focus = init_params->hwndFocus;
	this->pfnMicrophoneRawDataReady = init_params->bCustomVADProvided ? init_params->pfnMicrophoneRawDataReady : 0;
	
	for (uint32_t iProcessingMode = 0; iProcessingMode < init_params->dwNumLocalTalkerEnabledModes; iProcessingMode++) {
		this->processing_modes_enabled_local.insert(init_params->localTalkerEnabledModes[iProcessingMode]);
	}
	for (uint32_t iProcessingMode = 0; iProcessingMode < init_params->dwNumLocalTalkerEnabledModes; iProcessingMode++) {
		this->processing_modes_enabled_remote.insert(init_params->remoteTalkerEnabledModes[iProcessingMode]);
	}
}

__stdcall XHVEngine::~XHVEngine()
{
	TRACE_FX();
	
	DeleteCriticalSection(&this->xhv_lock);
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "XHVEngine (0x%08x) destroyed.", this);
}

LONG __stdcall XHVEngine::AddRef()
{
	TRACE_FX();
	LONG result = 0;
	
	result = InterlockedIncrement(&this->xhv_reference_count);
	
	return result;
}

LONG __stdcall XHVEngine::Release()
{
	TRACE_FX();
	LONG result = 0;
	
	result = InterlockedDecrement(&this->xhv_reference_count);
	
	if (!result) {
		delete this;
	}
	return result;
}

HRESULT __stdcall XHVEngine::Lock(XHV_LOCK_TYPE lockType)
{
	TRACE_FX();
	
	switch (lockType) {
		case XHV_LOCK_TYPE_TRYLOCK: {
			//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s Attempting XHV_LOCK_TYPE_TRYLOCK on XHVEngine (0x%08x).", __func__, this);
			if (!TryEnterCriticalSection(&this->xhv_lock)) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_INFO, "%s XHV_LOCK_TYPE_TRYLOCK failed. XHVEngine (0x%08x) already locked.", __func__, this);
				return E_ABORT;
			}
			//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s XHV_LOCK_TYPE_TRYLOCK success on XHVEngine (0x%08x).", __func__, this);
			return S_OK;
		}
		case XHV_LOCK_TYPE_LOCK: {
			//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s Attempting XHV_LOCK_TYPE_LOCK on XHVEngine (0x%08x).", __func__, this);
			EnterCriticalSection(&this->xhv_lock);
			//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s XHV_LOCK_TYPE_LOCK success on XHVEngine (0x%08x).", __func__, this);
			return S_OK;
		}
		case XHV_LOCK_TYPE_UNLOCK: {
			//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s Attempting XHV_LOCK_TYPE_UNLOCK on XHVEngine (0x%08x).", __func__, this);
			LeaveCriticalSection(&this->xhv_lock);
			//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s XHV_LOCK_TYPE_UNLOCK success on XHVEngine (0x%08x).", __func__, this);
			return S_OK;
		}
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s lockType (0x%08x) is invalid.", __func__, lockType);
	return E_INVALIDARG;
}

HRESULT __stdcall XHVEngine::StartStopLocalProcessingModes(bool start, DWORD dwUserIndex, CONST XHV_PROCESSING_MODE *processingModes, DWORD dwNumProcessingModes)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return XONLINE_E_NO_USER;
	}
	if (!processingModes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s processingModes is NULL.", __func__);
		return E_INVALIDARG;
	}
	
	for (uint32_t iProcessingMode = 0; iProcessingMode < dwNumProcessingModes; iProcessingMode++) {
		if (processingModes[iProcessingMode] != XHV_LOOPBACK_MODE && processingModes[iProcessingMode] != XHV_VOICECHAT_MODE) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Invalid processing mode specified (0x%08x) Must be XHV_LOOPBACK_MODE and/or XHV_VOICECHAT_MODE.", __func__, processingModes[iProcessingMode]);
			return E_INVALIDARG;
		}
	}
	
	if (dwNumProcessingModes > XHV_MAX_PROCESSING_MODES) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwNumProcessingModes (0x%08x) is greater than XHV_MAX_PROCESSING_MODES (0x%08x).", __func__, dwNumProcessingModes, XHV_MAX_PROCESSING_MODES);
		return E_INVALIDARG;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		auto const registeredUserLocal = this->registered_users_local.find(dwUserIndex);
		if (registeredUserLocal == this->registered_users_local.end()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x is not registered.", __func__, dwUserIndex);
			result = E_UNEXPECTED;
		}
		else {
			bool error = false;
			for (uint32_t iProcessingMode = 0; iProcessingMode < dwNumProcessingModes; iProcessingMode++) {
				if (start && !this->processing_modes_enabled_local.count(processingModes[iProcessingMode])) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Local enabled processing modes does not include 0x%08x.", __func__, processingModes[iProcessingMode]);
					result = E_UNEXPECTED;
					error = true;
				}
				if (((*registeredUserLocal).second->processing_modes.count(processingModes[iProcessingMode]) > 0) == start) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Local processing mode 0x%08x has already %s for user 0x%08x.", __func__, processingModes[iProcessingMode], start ? "started" : "stopped", dwUserIndex);
					result = E_UNEXPECTED;
					error = true;
				}
			}
			if (!error) {
				for (uint32_t iProcessingMode = 0; iProcessingMode < dwNumProcessingModes; iProcessingMode++) {
					if (start) {
						(*registeredUserLocal).second->processing_modes.insert(processingModes[iProcessingMode]);
					}
					else {
						(*registeredUserLocal).second->processing_modes.erase(processingModes[iProcessingMode]);
					}
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s Local processing mode has 0x%08x been %s for user 0x%08x.", __func__, processingModes[iProcessingMode], start ? "started" : "stopped", dwUserIndex);
				}
				result = S_OK;
			}
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT __stdcall XHVEngine::StartLocalProcessingModes(DWORD dwUserIndex, CONST XHV_PROCESSING_MODE *processingModes, DWORD dwNumProcessingModes)
{
	TRACE_FX();
	HRESULT result = this->StartStopLocalProcessingModes(true, dwUserIndex, processingModes, dwNumProcessingModes);
	return result;
}

HRESULT __stdcall XHVEngine::StopLocalProcessingModes(DWORD dwUserIndex, CONST XHV_PROCESSING_MODE *processingModes, DWORD dwNumProcessingModes)
{
	TRACE_FX();
	HRESULT result = this->StartStopLocalProcessingModes(false, dwUserIndex, processingModes, dwNumProcessingModes);
	return result;
}

HRESULT __stdcall XHVEngine::StartStopRemoteProcessingModes(bool start, XUID xuidRemoteTalker, CONST XHV_PROCESSING_MODE *processingModes, DWORD dwNumProcessingModes)
{
	TRACE_FX();
	if (!xuidRemoteTalker) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuidRemoteTalker is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!processingModes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s processingModes is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (dwNumProcessingModes != 1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s There must be only 1 processingModes specified, not 0x%08x.", __func__, dwNumProcessingModes);
		return E_INVALIDARG;
	}
	if (processingModes[0] != XHV_VOICECHAT_MODE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s The only processing mode allowed is XHV_VOICECHAT_MODE.", __func__);
		return E_INVALIDARG;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		auto const registeredUserRemote = this->registered_users_remote.find(xuidRemoteTalker);
		if (registeredUserRemote == this->registered_users_remote.end()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuidRemoteTalker 0x%016I64x is not registered.", __func__, xuidRemoteTalker);
			result = E_UNEXPECTED;
		}
		else {
			bool error = false;
			for (uint32_t iProcessingMode = 0; iProcessingMode < dwNumProcessingModes; iProcessingMode++) {
				if (start && !this->processing_modes_enabled_local.count(processingModes[iProcessingMode])) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Local enabled processing modes does not include 0x%08x.", __func__, processingModes[iProcessingMode]);
					result = E_UNEXPECTED;
					error = true;
				}
				if (((*registeredUserRemote).second->processing_modes.count(processingModes[iProcessingMode]) > 0) == start) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Local processing mode 0x%08x has already %s for xuidRemoteTalker 0x%016I64x.", __func__, processingModes[iProcessingMode], start ? "started" : "stopped", xuidRemoteTalker);
					result = E_UNEXPECTED;
					error = true;
				}
			}
			if (!error) {
				for (uint32_t iProcessingMode = 0; iProcessingMode < dwNumProcessingModes; iProcessingMode++) {
					if (start) {
						(*registeredUserRemote).second->processing_modes.insert(processingModes[iProcessingMode]);
					}
					else {
						(*registeredUserRemote).second->processing_modes.erase(processingModes[iProcessingMode]);
					}
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s Local processing mode has 0x%08x been %s for xuidRemoteTalker 0x%016I64x.", __func__, processingModes[iProcessingMode], start ? "started" : "stopped", xuidRemoteTalker);
				}
				result = S_OK;
			}
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT __stdcall XHVEngine::StartRemoteProcessingModes(XUID xuidRemoteTalker, CONST XHV_PROCESSING_MODE *processingModes, DWORD dwNumProcessingModes)
{
	TRACE_FX();
	HRESULT result = this->StartStopRemoteProcessingModes(true, xuidRemoteTalker, processingModes, dwNumProcessingModes);
	return result;
}

HRESULT __stdcall XHVEngine::StopRemoteProcessingModes(XUID xuidRemoteTalker, CONST XHV_PROCESSING_MODE *processingModes, DWORD dwNumProcessingModes)
{
	TRACE_FX();
	HRESULT result = this->StartStopRemoteProcessingModes(false, xuidRemoteTalker, processingModes, dwNumProcessingModes);
	return result;
}

HRESULT __stdcall XHVEngine::SetMaxDecodePackets(DWORD dwMaxDecodePackets)
{
	TRACE_FX();
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s is not implemented in GFWL.", __func__);
	return S_OK;
}

HRESULT __stdcall XHVEngine::RegisterLocalTalker(DWORD dwUserIndex)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return XONLINE_E_NO_USER;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		if (this->registered_users_local.count(dwUserIndex)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x is already registered.", __func__, dwUserIndex);
			result = E_UNEXPECTED;
		}
		else if (this->registered_users_local.size() + 1 > XHV_MAX_LOCAL_TALKERS) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XHV_MAX_LOCAL_TALKERS (0x%08x) limit reached.", __func__, XHV_MAX_LOCAL_TALKERS);
			result = E_FAIL;
		}
		else if (this->registered_users_local.size() + 1 > this->registered_users_max_local) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s this->registered_users_max_local (0x%08x) limit reached.", __func__, this->registered_users_max_local);
			result = E_FAIL;
		}
		else {
			
			VOICE_REGISTERED_USER_LOCAL *registered_data_local = new VOICE_REGISTERED_USER_LOCAL;
			registered_users_local[dwUserIndex] = registered_data_local;
			
			registered_data_local->is_headset_present = true;
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s User 0x%08x has been registered.", __func__, dwUserIndex);
			result = S_OK;
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT __stdcall XHVEngine::UnregisterLocalTalker(DWORD dwUserIndex)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		if (!this->registered_users_local.count(dwUserIndex)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x is not registered.", __func__, dwUserIndex);
			result = E_UNEXPECTED;
		}
		else {
			delete this->registered_users_local[dwUserIndex];
			this->registered_users_local.erase(dwUserIndex);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s User 0x%08x has been unregistered.", __func__, dwUserIndex);
			result = S_OK;
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT __stdcall XHVEngine::RegisterRemoteTalker(XUID xuidRemoteTalker, XAUDIOVOICEFXCHAIN *pfxRemoteTalkerFX, XAUDIOVOICEFXCHAIN *pfxTalkerPairFX, XAUDIOSUBMIXVOICE *pOutputVoice)
{
	TRACE_FX();
	if (!xuidRemoteTalker) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuidRemoteTalker is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (pfxRemoteTalkerFX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pfxRemoteTalkerFX must be NULL.", __func__);
		return E_INVALIDARG;
	}
	if (pfxTalkerPairFX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pfxTalkerPairFX must be NULL.", __func__);
		return E_INVALIDARG;
	}
	if (pOutputVoice) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pOutputVoice must be NULL.", __func__);
		return E_INVALIDARG;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		if (this->registered_users_remote.count(xuidRemoteTalker)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuidRemoteTalker 0x%016I64x is already registered.", __func__, xuidRemoteTalker);
			result = E_UNEXPECTED;
		}
		else if (this->registered_users_remote.size() + 1 > XHV_MAX_REMOTE_TALKERS) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XHV_MAX_REMOTE_TALKERS (0x%08x) limit reached.", __func__, XHV_MAX_REMOTE_TALKERS);
			result = E_FAIL;
		}
		else if (this->registered_users_remote.size() + 1 > this->registered_users_max_remote) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s this->registered_users_max_remote (0x%08x) limit reached.", __func__, this->registered_users_max_remote);
			result = E_FAIL;
		}
		else {
			registered_users_remote[xuidRemoteTalker] = new VOICE_REGISTERED_USER_REMOTE;
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s xuidRemoteTalker 0x%016I64x has been registered.", __func__, xuidRemoteTalker);
			result = S_OK;
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT __stdcall XHVEngine::UnregisterRemoteTalker(XUID xuidRemoteTalker)
{
	TRACE_FX();
	if (!xuidRemoteTalker) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuidRemoteTalker is NULL.", __func__);
		return E_INVALIDARG;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		if (!this->registered_users_remote.count(xuidRemoteTalker)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuidRemoteTalker 0x%016I64x is not registered.", __func__, xuidRemoteTalker);
			result = E_UNEXPECTED;
		}
		else {
			delete this->registered_users_remote[xuidRemoteTalker];
			this->registered_users_remote.erase(xuidRemoteTalker);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s xuidRemoteTalker 0x%016I64x has been unregistered.", __func__, xuidRemoteTalker);
			result = S_OK;
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT __stdcall XHVEngine::GetRemoteTalkers(DWORD *pdwRemoteTalkersCount, XUID *pxuidRemoteTalkers)
{
	TRACE_FX();
	if (!pdwRemoteTalkersCount) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwRemoteTalkersCount is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!pxuidRemoteTalkers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxuidRemoteTalkers is NULL.", __func__);
		return E_INVALIDARG;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		uint32_t numberOfRemoteTalkers = 0;
		
		for (auto const &registeredUserRemote : this->registered_users_remote) {
			pxuidRemoteTalkers[numberOfRemoteTalkers++] = registeredUserRemote.first;
		}
		
		if (numberOfRemoteTalkers > this->registered_users_max_remote) {
			__debugbreak();
		}
		
		result = S_OK;
		
		*pdwRemoteTalkersCount = numberOfRemoteTalkers;
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

BOOL __stdcall XHVEngine::IsHeadsetPresent(DWORD dwUserIndex)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return FALSE;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return FALSE;
	}
	
	BOOL result = FALSE;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		auto const registeredUserLocal = this->registered_users_local.find(dwUserIndex);
		if (registeredUserLocal != this->registered_users_local.end()) {
			if ((*registeredUserLocal).second->is_headset_present) {
				result = TRUE;
			}
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

BOOL __stdcall XHVEngine::IsLocalTalking(DWORD dwUserIndex)
{
	TRACE_FX();
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return FALSE;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return FALSE;
	}
	
	BOOL result = FALSE;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		auto const registeredUserLocal = this->registered_users_local.find(dwUserIndex);
		if (registeredUserLocal == this->registered_users_local.end()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x is not registered.", __func__, dwUserIndex);
		}
		else {
			if ((*registeredUserLocal).second->is_talking) {
				result = TRUE;
			}
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

BOOL __stdcall XHVEngine::isRemoteTalking(XUID xuidRemoteTalker)
{
	TRACE_FX();
	if (!xuidRemoteTalker) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuidRemoteTalker is NULL.", __func__);
		return E_INVALIDARG;
	}
	
	BOOL result = FALSE;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		auto const registeredUserRemote = this->registered_users_remote.find(xuidRemoteTalker);
		if (registeredUserRemote == this->registered_users_remote.end()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuidRemoteTalker 0x%016I64x is not registered.", __func__, xuidRemoteTalker);
		}
		else {
			if ((*registeredUserRemote).second->is_talking) {
				result = TRUE;
			}
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

DWORD __stdcall XHVEngine::GetDataReadyFlags()
{
	TRACE_FX();
	
	DWORD result = 0;
	
	// GFWL assembly function aquires the lock yet definition comments suggests locking is not done.
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		for (uint32_t iLocalTalker = 0; iLocalTalker < XLIVE_LOCAL_USER_COUNT; iLocalTalker++) {
			auto const registeredUserLocal = this->registered_users_local.find(iLocalTalker);
			if (registeredUserLocal != this->registered_users_local.end()) {
				if ((*registeredUserLocal).second->is_talking) {
					result |= (1 << iLocalTalker);
				}
			}
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT __stdcall XHVEngine::GetLocalChatData(DWORD dwUserIndex, BYTE *pbData, DWORD *pdwSize, DWORD *pdwPackets)
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
	if (!pbData) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pbData is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!pdwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwSize is NULL.", __func__);
		return E_INVALIDARG;
	}
	uint32_t numOfPacketsCanFit = 0;
	if (xlive_xhv_engine_version_new) {
		if (*pdwSize < XHV_VOICECHAT_MODE_PACKET_SIZE_NEW) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s *pdwSize (0x%08x) is not big enough to hold at least one XHV_VOICECHAT_MODE_PACKET_SIZE (0x%08x).", __func__, *pdwSize, XHV_VOICECHAT_MODE_PACKET_SIZE_NEW);
			return E_OUTOFMEMORY;
		}
		numOfPacketsCanFit = *pdwSize / XHV_VOICECHAT_MODE_PACKET_SIZE_NEW;
		if (*pdwSize > (XHV_MAX_VOICECHAT_PACKETS * XHV_VOICECHAT_MODE_PACKET_SIZE_NEW)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s *pdwSize (0x%08x) is bigger than (XHV_MAX_VOICECHAT_PACKETS * XHV_VOICECHAT_MODE_PACKET_SIZE) (0x%08x).", __func__, *pdwSize, XHV_MAX_VOICECHAT_PACKETS * XHV_VOICECHAT_MODE_PACKET_SIZE_NEW);
		}
	}
	else {
		if (*pdwSize < XHV_VOICECHAT_MODE_PACKET_SIZE_OLD) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s *pdwSize (0x%08x) is not big enough to hold at least one XHV_VOICECHAT_MODE_PACKET_SIZE (0x%08x).", __func__, *pdwSize, XHV_VOICECHAT_MODE_PACKET_SIZE_OLD);
			return E_OUTOFMEMORY;
		}
		numOfPacketsCanFit = *pdwSize / XHV_VOICECHAT_MODE_PACKET_SIZE_OLD;
		if (*pdwSize > (XHV_MAX_VOICECHAT_PACKETS * XHV_VOICECHAT_MODE_PACKET_SIZE_OLD)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s *pdwSize (0x%08x) is bigger than (XHV_MAX_VOICECHAT_PACKETS * XHV_VOICECHAT_MODE_PACKET_SIZE) (0x%08x).", __func__, *pdwSize, XHV_MAX_VOICECHAT_PACKETS * XHV_VOICECHAT_MODE_PACKET_SIZE_OLD);
		}
	}
	
	memset(pbData, 0, *pdwSize);
	
	*pdwSize = 0;
	
	if (pdwPackets) {
		*pdwPackets = 0;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		auto const registeredUserLocal = this->registered_users_local.find(dwUserIndex);
		if (registeredUserLocal == this->registered_users_local.end()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x is not registered.", __func__, dwUserIndex);
			result = E_UNEXPECTED;
		}
		else if (!(*registeredUserLocal).second->processing_modes.size()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Local processing mode not started.", __func__);
			result = E_UNEXPECTED;
		}
		else {
			(*registeredUserLocal).second;
			
			// TODO
			
			result = E_PENDING;
			//result = S_OK;
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT __stdcall XHVEngine::SetPlaybackPriority(XUID xuidRemoteTalker, DWORD dwUserIndex, XHV_PLAYBACK_PRIORITY playbackPriority)
{
	TRACE_FX();
	if (!xuidRemoteTalker) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuidRemoteTalker is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (xlive_users_info[dwUserIndex]->UserSigninState == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, dwUserIndex);
		return E_INVALIDARG;
	}
	if (playbackPriority > XHV_PLAYBACK_PRIORITY_MIN && playbackPriority != XHV_PLAYBACK_PRIORITY_NEVER) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s playbackPriority must be between XHV_PLAYBACK_PRIORITY_MAX (0x%08x) and XHV_PLAYBACK_PRIORITY_MIN (0x%08x) or set to XHV_PLAYBACK_PRIORITY_NEVER (0x%08x).", __func__, XHV_PLAYBACK_PRIORITY_MAX, XHV_PLAYBACK_PRIORITY_MIN, XHV_PLAYBACK_PRIORITY_NEVER);
		return E_INVALIDARG;
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		auto const registeredUserRemote = this->registered_users_remote.find(xuidRemoteTalker);
		if (registeredUserRemote == this->registered_users_remote.end()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuidRemoteTalker 0x%016I64x is not registered.", __func__, xuidRemoteTalker);
			result = E_UNEXPECTED;
		}
		else {
			auto const registeredUserLocal = this->registered_users_local.find(dwUserIndex);
			if (registeredUserLocal == this->registered_users_local.end()) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x is not registered.", __func__, dwUserIndex);
				result = E_UNEXPECTED;
			}
			else {
				(*registeredUserRemote).second;
				(*registeredUserLocal).second;
				
				// TODO
				playbackPriority;
				result = S_OK;
			}
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}

HRESULT __stdcall XHVEngine::SubmitIncomingChatData(XUID xuidRemoteTalker, CONST BYTE *pbData, DWORD *pdwSize)
{
	TRACE_FX();
	if (!xuidRemoteTalker) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuidRemoteTalker is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!pbData) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pbData is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!pdwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwSize is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (xlive_xhv_engine_version_new) {
		if (*pdwSize % XHV_VOICECHAT_MODE_PACKET_SIZE_NEW) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *pdwSize (0x%08x) is not a multiple of XHV_VOICECHAT_MODE_PACKET_SIZE (0x%08x).", __func__, *pdwSize, XHV_VOICECHAT_MODE_PACKET_SIZE_NEW);
			return E_INVALIDARG;
		}
	}
	else {
		if (*pdwSize % XHV_VOICECHAT_MODE_PACKET_SIZE_OLD) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *pdwSize (0x%08x) is not a multiple of XHV_VOICECHAT_MODE_PACKET_SIZE (0x%08x).", __func__, *pdwSize, XHV_VOICECHAT_MODE_PACKET_SIZE_OLD);
			return E_INVALIDARG;
		}
	}
	
	HRESULT result = E_UNEXPECTED;
	
	{
		this->Lock(XHV_LOCK_TYPE_LOCK);
		
		auto const registeredUserRemote = this->registered_users_remote.find(xuidRemoteTalker);
		if (registeredUserRemote == this->registered_users_remote.end()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuidRemoteTalker 0x%016I64x is not registered.", __func__, xuidRemoteTalker);
			result = E_UNEXPECTED;
		}
		else {
			(*registeredUserRemote).second;
			// TODO
			result = S_OK;
		}
		
		this->Lock(XHV_LOCK_TYPE_UNLOCK);
	}
	
	return result;
}
