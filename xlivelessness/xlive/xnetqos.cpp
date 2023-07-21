#include <winsock2.h>
#include "xdefs.hpp"
#include "xnetqos.hpp"
#include "xsocket.hpp"
#include "xnet.hpp"
#include "packet-handler.hpp"
#include "net-entity.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/xlln.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"
#include "live-over-lan.hpp"
#include <vector>
#include <thread>
#include <condition_variable>
#include <atomic>

struct QOS_TRANSIT_INFO {
	uint32_t probesSent = 0;
	std::chrono::steady_clock::time_point timeLastCommSent = std::chrono::high_resolution_clock::now();
};

struct QOS_PENDING_LOOKUP {
	uint32_t qosLookupId = 0;
	XNQOS *xnQos = 0;
	QOS_TRANSIT_INFO *qosTransitInfo = 0;
	WSAEVENT hEvent = INVALID_HANDLE_VALUE;
	std::chrono::steady_clock::time_point timeCreated = std::chrono::high_resolution_clock::now();
	uint32_t countProbes = 0;
	uint32_t maxBitsPerSec = QOS_LOOKUP_BITS_PER_SEC_DEFAULT;
	uint32_t countXn = 0;
	XNADDR *xnAddrs = 0;
	XNKID *xnKids = 0;
	XNKEY *xnKeys = 0;
	uint32_t countIn = 0;
	IN_ADDR *inAddrs = 0;
	uint32_t *inServiceIds = 0;
};

CRITICAL_SECTION xlive_critsec_qos_listeners;
// Key: sessionId / xnkid.
std::map<uint64_t, QOS_LISTENER_INFO*> xlive_qos_listeners;

CRITICAL_SECTION xlive_critsec_qos_lookups;
// Key: qosLookupId.
static std::map<uint32_t, QOS_PENDING_LOOKUP*> xlive_qos_lookups;

static std::condition_variable xlive_thread_cond_qos;
static std::thread xlive_thread_qos;
static std::atomic<bool> xlive_thread_exit_qos = TRUE;

// #69
// Calling XNetUnregisterKey with the session Id / xnkid will release this QoS listener automatically.
// Therefore the session Id needs to be registered via XNetRegisterKey before use here.
INT WINAPI XNetQosListen(XNKID *pxnkid, uint8_t *data, uint32_t data_size, uint32_t dwBitsPerSec, uint32_t dwFlags)
{
	TRACE_FX();
	if (!dwFlags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s dwFlags is 0.", __func__);
		return WSAEINVAL;
	}
	if (dwFlags & ~(XNET_QOS_LISTEN_ENABLE | XNET_QOS_LISTEN_DISABLE | XNET_QOS_LISTEN_SET_DATA | XNET_QOS_LISTEN_SET_BITSPERSEC | XNET_QOS_LISTEN_RELEASE)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Unknown dwFlags (0x%08x).", __func__, dwFlags);
		return WSAEINVAL;
	}
	if ((dwFlags & XNET_QOS_LISTEN_ENABLE) && (dwFlags & XNET_QOS_LISTEN_DISABLE)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Invalid dwFlags: cannot enable and disable.", __func__);
		return WSAEINVAL;
	}
	if ((dwFlags & XNET_QOS_LISTEN_RELEASE) && (dwFlags ^ XNET_QOS_LISTEN_RELEASE)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Invalid dwFlags: cannot release and change properties.", __func__);
		return WSAEINVAL;
	}
	if ((dwFlags & XNET_QOS_LISTEN_SET_DATA) && data_size > 0 && !data) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s data is NULL when told to set data of size (%u).", __func__, data_size);
		return WSAEFAULT;
	}
	if (!(dwFlags & XNET_QOS_LISTEN_SET_DATA) && (data_size > 0 || data)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s data_size and/or data is set when XNET_QOS_LISTEN_SET_DATA flag not set.", __func__);
		return WSAEINVAL;
	}
	if (data_size > 0 && data_size > ((uint32_t)xlive_net_startup_params.cfgQosDataLimitDiv4) * 4) {
		XLLN_DEBUG_LOG(
			XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s data_size bigger than (cfgQosDataLimitDiv4 * 4) (0x%08x > 0x%08x)."
			, __func__
			, data_size
			, ((uint32_t)xlive_net_startup_params.cfgQosDataLimitDiv4) * 4
		);
		return WSAEMSGSIZE;
	}
	if (dwBitsPerSec == 0) {
		dwBitsPerSec = QOS_LISTEN_BITS_PER_SEC_DEFAULT;
	}
	else if (dwBitsPerSec < 4000) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
			, "%s dwBitsPerSec (%u) < 4000 and > 0. Corrected to 4000."
			, __func__
			, dwBitsPerSec
		);
		dwBitsPerSec = 4000;
	}
	else if (dwBitsPerSec > 100000000) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
			, "%s dwBitsPerSec (%u) > 100000000. Corrected to 100000000."
			, __func__
			, dwBitsPerSec
		);
		dwBitsPerSec = 100000000;
	}
	
	EnterCriticalSection(&xlive_critsec_qos_listeners);
	QOS_LISTENER_INFO *qosListenerInfo = 0;
	
	if (dwFlags & XNET_QOS_LISTEN_RELEASE) {
		if (xlive_qos_listeners.count(*(uint64_t*)pxnkid)) {
			qosListenerInfo = xlive_qos_listeners[*(uint64_t*)pxnkid];
			xlive_qos_listeners.erase(*(uint64_t*)pxnkid);
			
			if (qosListenerInfo->pData) {
				delete[] qosListenerInfo->pData;
				qosListenerInfo->pData = 0;
			}
			delete qosListenerInfo;
			qosListenerInfo = 0;
		}
	}
	else {
		if (xlive_qos_listeners.count(*(uint64_t*)pxnkid)) {
			qosListenerInfo = xlive_qos_listeners[*(uint64_t*)pxnkid];
		}
		else {
			qosListenerInfo = new QOS_LISTENER_INFO;
			qosListenerInfo->sessionId = *(uint64_t*)pxnkid;
			xlive_qos_listeners[*(uint64_t*)pxnkid] = qosListenerInfo;
		}
		
		if (dwFlags & XNET_QOS_LISTEN_SET_DATA) {
			qosListenerInfo->dataSize = data_size;
			if (qosListenerInfo->pData) {
				delete[] qosListenerInfo->pData;
				qosListenerInfo->pData = 0;
			}
			if (qosListenerInfo->dataSize) {
				qosListenerInfo->pData = new uint8_t[qosListenerInfo->dataSize];
				memcpy(qosListenerInfo->pData, data, qosListenerInfo->dataSize);
			}
		}
		
		if (dwFlags & XNET_QOS_LISTEN_SET_BITSPERSEC) {
			qosListenerInfo->maxBitsPerSec = dwBitsPerSec;
		}
		
		if (dwFlags & XNET_QOS_LISTEN_ENABLE) {
			qosListenerInfo->active = true;
		}
		if (dwFlags & XNET_QOS_LISTEN_DISABLE) {
			qosListenerInfo->active = false;
		}
	}
	
	LeaveCriticalSection(&xlive_critsec_qos_listeners);
	
	return S_OK;
}

// #70
DWORD WINAPI XNetQosLookup(UINT cxna, XNADDR * apxna[], XNKID * apxnkid[], XNKEY * apxnkey[], UINT cina, IN_ADDR aina[], DWORD adwServiceId[], UINT cProbes, DWORD dwBitsPerSec, DWORD dwFlags, WSAEVENT hEvent, XNQOS** ppxnqos)
{
	TRACE_FX();
	if (!ppxnqos) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ppxnqos is NULL.", __func__);
		return WSAEFAULT;
	}
	*ppxnqos = 0;
	if (dwFlags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s No dwFlags (0x%08x) are officially supported.", __func__, dwFlags);
		return WSAEINVAL;
	}
	if (!cxna && !cina) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cxna and cina are 0.", __func__);
		return WSAEINVAL;
	}
	if (cxna) {
		if (!apxna) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s apxna is NULL.", __func__);
			return WSAEFAULT;
		}
		if (!apxnkid) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s apxnkid is NULL.", __func__);
			return WSAEFAULT;
		}
		if (!apxnkey) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s apxnkey is NULL.", __func__);
			return WSAEFAULT;
		}
	}
	if (cina) {
		if (!aina) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s aina is NULL.", __func__);
			return WSAEFAULT;
		}
		if (!adwServiceId) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s adwServiceId is NULL.", __func__);
			return WSAEFAULT;
		}
		
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s FIXME need to see how results are populated.", __func__);
	}
	if (dwBitsPerSec == 0) {
		dwBitsPerSec = QOS_LOOKUP_BITS_PER_SEC_DEFAULT;
	}
	else if (dwBitsPerSec < 4000) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
			, "%s dwBitsPerSec (%u) < 4000 and > 0. Corrected to 4000."
			, __func__
			, dwBitsPerSec
		);
		dwBitsPerSec = 4000;
	}
	else if (dwBitsPerSec > 100000000) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
			, "%s dwBitsPerSec (%u) > 100000000. Corrected to 100000000."
			, __func__
			, dwBitsPerSec
		);
		dwBitsPerSec = 100000000;
	}
	
	QOS_PENDING_LOOKUP *qosPendingLookup = new QOS_PENDING_LOOKUP;
	qosPendingLookup->countProbes = cProbes;
	qosPendingLookup->maxBitsPerSec = dwBitsPerSec;
	
	if (hEvent != INVALID_HANDLE_VALUE && hEvent) {
		qosPendingLookup->hEvent = hEvent;
	}
	
	qosPendingLookup->countXn = cxna;
	if (qosPendingLookup->countXn) {
		qosPendingLookup->xnAddrs = new XNADDR[qosPendingLookup->countXn];
		memcpy(qosPendingLookup->xnAddrs, *apxna, qosPendingLookup->countXn * sizeof(*qosPendingLookup->xnAddrs));
		qosPendingLookup->xnKids = new XNKID[qosPendingLookup->countXn];
		memcpy(qosPendingLookup->xnKids, *apxnkid, qosPendingLookup->countXn * sizeof(*qosPendingLookup->xnKids));
		qosPendingLookup->xnKeys = new XNKEY[qosPendingLookup->countXn];
		memcpy(qosPendingLookup->xnKeys, *apxnkey, qosPendingLookup->countXn * sizeof(*qosPendingLookup->xnKeys));
	}
	
	qosPendingLookup->countIn = cina;
	if (qosPendingLookup->countIn) {
		qosPendingLookup->inAddrs = new IN_ADDR[qosPendingLookup->countIn];
		memcpy(qosPendingLookup->inAddrs, aina, qosPendingLookup->countIn * sizeof(*qosPendingLookup->inAddrs));
		qosPendingLookup->inServiceIds = new uint32_t[qosPendingLookup->countIn];
		memcpy(qosPendingLookup->inServiceIds, adwServiceId, qosPendingLookup->countIn * sizeof(*qosPendingLookup->inServiceIds));
	}
	
	uint32_t xnQoSSize = (uint32_t)&(*ppxnqos)->axnqosinfo - (uint32_t)*ppxnqos;
	xnQoSSize += qosPendingLookup->countXn * sizeof(*qosPendingLookup->xnQos->axnqosinfo);
	xnQoSSize += qosPendingLookup->countIn * sizeof(*qosPendingLookup->xnQos->axnqosinfo);
	qosPendingLookup->xnQos = (XNQOS*)new uint8_t[xnQoSSize];
	memset(qosPendingLookup->xnQos, 0, xnQoSSize);
	
	qosPendingLookup->xnQos->cxnqos = qosPendingLookup->countXn + qosPendingLookup->countIn;
	qosPendingLookup->xnQos->cxnqosPending = qosPendingLookup->xnQos->cxnqos;
	
	qosPendingLookup->qosTransitInfo = new QOS_TRANSIT_INFO[qosPendingLookup->xnQos->cxnqosPending];
	
	*ppxnqos = qosPendingLookup->xnQos;
	
	{
		EnterCriticalSection(&xlive_critsec_qos_lookups);
		
		// Get a unique ID.
		do {
			qosPendingLookup->qosLookupId = rand();
		}
		while (xlive_qos_lookups.count(qosPendingLookup->qosLookupId));
		
		xlive_qos_lookups[qosPendingLookup->qosLookupId] = qosPendingLookup;
		
		LeaveCriticalSection(&xlive_critsec_qos_lookups);
	}
	
	return S_OK;
}

// #71
INT WINAPI XNetQosServiceLookup(DWORD dwFlags, WSAEVENT hEvent, XNQOS **ppxnqos)
{
	TRACE_FX();
	if (dwFlags & ~(XNET_QOS_SERVICE_LOOKUP_RESERVED)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s No dwFlags (0x%08x) are officially supported.", __func__, dwFlags);
		return WSAEINVAL;
	}
	if (!ppxnqos) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ppxnqos is NULL.", __func__);
		return WSAEINVAL;
	}
	
	
	QOS_PENDING_LOOKUP *qosPendingLookup = new QOS_PENDING_LOOKUP;
	qosPendingLookup->countProbes = 0;
	qosPendingLookup->maxBitsPerSec = QOS_LOOKUP_BITS_PER_SEC_DEFAULT;
	
	if (hEvent != INVALID_HANDLE_VALUE && hEvent) {
		qosPendingLookup->hEvent = hEvent;
	}
	
	qosPendingLookup->countIn = 1;
	qosPendingLookup->inAddrs = new IN_ADDR[qosPendingLookup->countIn];
	qosPendingLookup->inAddrs[0].s_addr = 0;
	qosPendingLookup->inServiceIds = new uint32_t[qosPendingLookup->countIn];
	qosPendingLookup->inServiceIds[0] = 0;
	
	
	uint32_t xnQoSSize = (uint32_t)&(*ppxnqos)->axnqosinfo - (uint32_t)*ppxnqos;
	xnQoSSize += qosPendingLookup->countXn * sizeof(*qosPendingLookup->xnQos->axnqosinfo);
	xnQoSSize += qosPendingLookup->countIn * sizeof(*qosPendingLookup->xnQos->axnqosinfo);
	qosPendingLookup->xnQos = (XNQOS*)new uint8_t[xnQoSSize];
	memset(qosPendingLookup->xnQos, 0, xnQoSSize);
	
	qosPendingLookup->xnQos->cxnqos = 0;
	qosPendingLookup->xnQos->cxnqosPending = qosPendingLookup->countXn + qosPendingLookup->countIn;
	
	qosPendingLookup->qosTransitInfo = new QOS_TRANSIT_INFO[qosPendingLookup->xnQos->cxnqosPending];
	
	*ppxnqos = qosPendingLookup->xnQos;
	
	// Remove this and actually QoS lookup on Hub server(s?).
	
	qosPendingLookup->xnQos->cxnqos = 1;
	qosPendingLookup->xnQos->cxnqosPending = 0;
	
	{
		size_t iXnQoS = 0;
		qosPendingLookup->xnQos->axnqosinfo[iXnQoS].bFlags = XNET_XNQOSINFO_TARGET_CONTACTED | XNET_XNQOSINFO_COMPLETE;
		qosPendingLookup->xnQos->axnqosinfo[iXnQoS].cProbesXmit = 1;
		qosPendingLookup->xnQos->axnqosinfo[iXnQoS].cProbesRecv = 1;
		qosPendingLookup->xnQos->axnqosinfo[iXnQoS].wRttMinInMsecs = 50;
		qosPendingLookup->xnQos->axnqosinfo[iXnQoS].wRttMedInMsecs = 60;
		qosPendingLookup->xnQos->axnqosinfo[iXnQoS].dwUpBitsPerSec = QOS_LOOKUP_BITS_PER_SEC_DEFAULT;
		qosPendingLookup->xnQos->axnqosinfo[iXnQoS].dwDnBitsPerSec = QOS_LOOKUP_BITS_PER_SEC_DEFAULT;
		// This probably does not need setting.
		qosPendingLookup->xnQos->axnqosinfo[iXnQoS].cbData = 0;
		qosPendingLookup->xnQos->axnqosinfo[iXnQoS].pbData = 0;
	}
	
	{
		EnterCriticalSection(&xlive_critsec_qos_lookups);
		
		// Get a unique ID.
		do {
			qosPendingLookup->qosLookupId = rand();
		}
		while (xlive_qos_lookups.count(qosPendingLookup->qosLookupId));
		
		xlive_qos_lookups[qosPendingLookup->qosLookupId] = qosPendingLookup;
		
		LeaveCriticalSection(&xlive_critsec_qos_lookups);
	}
	
	return S_OK;
}

// #72
INT WINAPI XNetQosRelease(XNQOS* pxnqos)
{
	TRACE_FX();
	if (!pxnqos) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnqos is NULL.", __func__);
		return WSAEFAULT;
	}
	
	{
		QOS_PENDING_LOOKUP *qosPendingLookup = 0;
		
		EnterCriticalSection(&xlive_critsec_qos_lookups);
		for (auto itrQosPendingLookup = xlive_qos_lookups.begin(); itrQosPendingLookup != xlive_qos_lookups.end(); itrQosPendingLookup++) {
			if (itrQosPendingLookup->second->xnQos == pxnqos) {
				qosPendingLookup = itrQosPendingLookup->second;
				break;
			}
		}
		if (qosPendingLookup) {
			xlive_qos_lookups.erase(qosPendingLookup->qosLookupId);
		}
		LeaveCriticalSection(&xlive_critsec_qos_lookups);
		
		if (qosPendingLookup) {
			if (qosPendingLookup->qosTransitInfo) {
				delete[] qosPendingLookup->qosTransitInfo;
			}
			if (qosPendingLookup->xnAddrs) {
				delete[] qosPendingLookup->xnAddrs;
			}
			if (qosPendingLookup->xnKids) {
				delete[] qosPendingLookup->xnKids;
			}
			if (qosPendingLookup->xnKeys) {
				delete[] qosPendingLookup->xnKeys;
			}
			if (qosPendingLookup->inAddrs) {
				delete[] qosPendingLookup->inAddrs;
			}
			if (qosPendingLookup->inServiceIds) {
				delete[] qosPendingLookup->inServiceIds;
			}
			// qosPendingLookup->xnQos does not need deleting as it is deleted below.
			delete qosPendingLookup;
		}
	}
	
	for (size_t iXnQoS = 0; iXnQoS < pxnqos->cxnqos; iXnQoS++) {
		if (pxnqos->axnqosinfo[iXnQoS].cbData && !pxnqos->axnqosinfo[iXnQoS].pbData) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnqos (0x%08x) pxnqos->axnqosinfo[%u].cbData is not 0 while pbData is NULL.", __func__, pxnqos, iXnQoS);
		}
		if (pxnqos->axnqosinfo[iXnQoS].pbData && !pxnqos->axnqosinfo[iXnQoS].cbData) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnqos (0x%08x) pxnqos->axnqosinfo[%u].pbData is not null while cbData is 0.", __func__, pxnqos, iXnQoS);
		}
		if (pxnqos->axnqosinfo[iXnQoS].pbData) {
			delete[] pxnqos->axnqosinfo[iXnQoS].pbData;
		}
	}
	
	delete[] pxnqos;
	
	return S_OK;
}

// #77
INT WINAPI XNetQosGetListenStats(XNKID *pxnKid, XNQOSLISTENSTATS *pQosListenStats)
{
	TRACE_FX();
	if (!pxnKid) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnKid is NULL.", __func__);
		return WSAEINVAL;
	}
	if (!pQosListenStats) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pQosListenStats is NULL.", __func__);
		return WSAEINVAL;
	}
	if (pQosListenStats->dwSizeOfStruct != sizeof(XNQOSLISTENSTATS)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pQosListenStats->dwSizeOfStruct != sizeof(XNQOSLISTENSTATS)) (0x%08x != 0x%08x).", __func__, pQosListenStats->dwSizeOfStruct, sizeof(XNQOSLISTENSTATS));
		return WSAEINVAL;
	}
	
	pQosListenStats->dwNumDataRequestsReceived = 0;
	pQosListenStats->dwNumProbesReceived = 0;
	pQosListenStats->dwNumSlotsFullDiscards = 0;
	pQosListenStats->dwNumDataRepliesSent = 0;
	pQosListenStats->dwNumDataReplyBytesSent = 0;
	pQosListenStats->dwNumProbeRepliesSent = 0;
	
	EnterCriticalSection(&xlive_critsec_qos_listeners);
	
	if (!xlive_qos_listeners.count(*(uint64_t*)pxnKid)) {
		LeaveCriticalSection(&xlive_critsec_qos_listeners);
		return WSAEINVAL;
	}
	
	QOS_LISTENER_INFO *qosListenerInfo = xlive_qos_listeners[*(uint64_t*)pxnKid];
	// TODO
	
	LeaveCriticalSection(&xlive_critsec_qos_listeners);
	
	return S_OK;
}

static void ThreadXLiveQoS()
{
	const int packetSizeHeaderType = sizeof(XLLNNetPacketType::TYPE);
	const int packetSizeTypeQosRequest = sizeof(XLLNNetPacketType::QOS_REQUEST);
	const int packetSize = packetSizeHeaderType + packetSizeTypeQosRequest;
	
	uint8_t *packetBuffer = new uint8_t[packetSize];
	packetBuffer[0] = XLLNNetPacketType::tQOS_REQUEST;
	XLLNNetPacketType::QOS_REQUEST &packetQosRequest = *(XLLNNetPacketType::QOS_REQUEST*)&packetBuffer[packetSizeHeaderType];
	
	SOCKADDR_STORAGE sockAddrExternal;
	const int sockAddrExternalLen = sizeof(sockAddrExternal);
	
	std::mutex mutexPause;
	while (1) {
		sockAddrExternal.ss_family = AF_INET;
		(*(sockaddr_in*)&sockAddrExternal).sin_addr.s_addr = htonl(INADDR_BROADCAST);
		if (IsUsingBasePort(xlive_base_port)) {
			(*(sockaddr_in*)&sockAddrExternal).sin_port = htons(xlive_base_port);
		}
		else {
			(*(sockaddr_in*)&sockAddrExternal).sin_port = htons(xlive_port_online);
		}
		
		packetQosRequest.instanceId = ntohl(xlive_local_xnAddr.inaOnline.s_addr);
		
		{
			EnterCriticalSection(&xlive_critsec_qos_lookups);
			for (auto itrQosPendingLookup = xlive_qos_lookups.begin(); itrQosPendingLookup != xlive_qos_lookups.end(); itrQosPendingLookup++) {
				QOS_PENDING_LOOKUP *qosPendingLookup = itrQosPendingLookup->second;
				if (qosPendingLookup->xnQos->cxnqosPending == 0) {
					continue;
				}
				
				for (size_t iXnQoS = 0; iXnQoS < qosPendingLookup->xnQos->cxnqos; iXnQoS++) {
					if (qosPendingLookup->xnQos->axnqosinfo[iXnQoS].bFlags & (XNET_XNQOSINFO_TARGET_DISABLED | XNET_XNQOSINFO_COMPLETE)) {
						continue;
					}
					
					// 7 seconds before probe timeout.
					if (qosPendingLookup->qosTransitInfo[iXnQoS].probesSent && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - qosPendingLookup->qosTransitInfo[iXnQoS].timeLastCommSent).count() > 7000) {
						continue;
					}
					
					// TODO currently finishing on the first response.
					// if (qosPendingLookup->qosTransitInfo[iXnQoS].probesSent >= qosPendingLookup->countProbes) {
					if (qosPendingLookup->qosTransitInfo[iXnQoS].probesSent >= qosPendingLookup->countProbes || qosPendingLookup->xnQos->axnqosinfo[iXnQoS].cProbesRecv) {
						// remove all these hardcoded completion values.
						qosPendingLookup->qosTransitInfo[iXnQoS].probesSent = qosPendingLookup->countProbes;
						qosPendingLookup->xnQos->axnqosinfo[iXnQoS].cProbesXmit = qosPendingLookup->countProbes;
						qosPendingLookup->xnQos->axnqosinfo[iXnQoS].cProbesRecv = qosPendingLookup->countProbes;
						
						qosPendingLookup->xnQos->axnqosinfo[iXnQoS].bFlags |= XNET_XNQOSINFO_COMPLETE;
						qosPendingLookup->xnQos->axnqosinfo[iXnQoS].bFlags &= ~(uint8_t)(XNET_XNQOSINFO_PARTIAL_COMPLETE);
						
						qosPendingLookup->xnQos->cxnqosPending--;
						
						if (qosPendingLookup->hEvent) {
							SetEvent(qosPendingLookup->hEvent);
						}
						
						continue;
					}
					
					qosPendingLookup->qosTransitInfo[iXnQoS].probesSent++;
					qosPendingLookup->xnQos->axnqosinfo[iXnQoS].cProbesXmit = qosPendingLookup->qosTransitInfo[iXnQoS].probesSent;
					
					packetQosRequest.qosLookupId = qosPendingLookup->qosLookupId;
					packetQosRequest.probeId = qosPendingLookup->qosTransitInfo[iXnQoS].probesSent;
					
					if (iXnQoS < qosPendingLookup->countXn) {
						// TODO use ina instead?
						const uint32_t ipv4NBO = qosPendingLookup->xnAddrs[iXnQoS].inaOnline.s_addr;
						const uint16_t portNBO = qosPendingLookup->xnAddrs[iXnQoS].wPortOnline;
						// This address may (hopefully) be an instanceId.
						const uint32_t ipv4HBO = ntohl(ipv4NBO);
						uint16_t portHBO = ntohs(portNBO);
						
						if (!IsUsingBasePort(portHBO)) {
							portHBO = xlive_port_online;
						}
						
						sockAddrExternal.ss_family = AF_INET;
						(*(sockaddr_in*)&sockAddrExternal).sin_addr.s_addr = ipv4NBO;
						(*(sockaddr_in*)&sockAddrExternal).sin_port = htons(portHBO);
						
						packetQosRequest.sessionId = *(uint64_t*)(&qosPendingLookup->xnKids[iXnQoS]);
					}
					else {
						qosPendingLookup->inAddrs[iXnQoS - qosPendingLookup->countXn];
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
							, "%s QoS unhandled inaddr."
							, __func__
						);
						continue;
					}
					
					qosPendingLookup->qosTransitInfo[iXnQoS].timeLastCommSent = std::chrono::high_resolution_clock::now();
					
					XllnSocketSendTo(
						xlive_xsocket_perpetual_core_socket
						, (char*)packetBuffer
						, packetSize
						, 0
						, (const sockaddr*)&sockAddrExternal
						, sockAddrExternalLen
					);
				}
				
			}
			LeaveCriticalSection(&xlive_critsec_qos_lookups);
		}
		
		std::unique_lock<std::mutex> lock(mutexPause);
		xlive_thread_cond_qos.wait_for(lock, std::chrono::seconds(1), []() { return xlive_thread_exit_qos == TRUE; });
		if (xlive_thread_exit_qos) {
			break;
		}
	}
	
	delete[] packetBuffer;
}

void XLiveQosReceiveRequest(XLLNNetPacketType::QOS_REQUEST *packetQosRequest, SOCKET perpetual_socket, const SOCKADDR_STORAGE *sockAddrExternal, const int sockAddrExternalLen)
{
	EnterCriticalSection(&xlive_critsec_qos_listeners);
	
	if (!xlive_qos_listeners.count(packetQosRequest->sessionId)) {
		LeaveCriticalSection(&xlive_critsec_qos_listeners);
		return;
	}
	
	{
		char *sockAddrInfo = GET_SOCKADDR_INFO(sockAddrExternal);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
			, "%s QOS_REQUEST from %s sessionId 0x%016I64x."
			, __func__
			, sockAddrInfo ? sockAddrInfo : "?"
			, packetQosRequest->sessionId
		);
		if (sockAddrInfo) {
			free(sockAddrInfo);
		}
	}
	
	QOS_LISTENER_INFO *qosListenerInfo = xlive_qos_listeners[packetQosRequest->sessionId];
	
	{
		const int packetSizeHeaderType = sizeof(XLLNNetPacketType::TYPE);
		const int packetSizeTypeQosResponse = sizeof(XLLNNetPacketType::QOS_RESPONSE);
		const int packetSize = packetSizeHeaderType + packetSizeTypeQosResponse + qosListenerInfo->dataSize;
		
		uint8_t *packetBuffer = new uint8_t[packetSize];
		packetBuffer[0] = XLLNNetPacketType::tQOS_RESPONSE;
		XLLNNetPacketType::QOS_RESPONSE &packetQosResponse = *(XLLNNetPacketType::QOS_RESPONSE*)&packetBuffer[packetSizeHeaderType];
		
		packetQosResponse.qosLookupId = packetQosRequest->qosLookupId;
		packetQosResponse.probeId = packetQosRequest->probeId;
		packetQosResponse.instanceId = ntohl(xlive_local_xnAddr.inaOnline.s_addr);
		packetQosResponse.sessionId = packetQosRequest->sessionId;
		packetQosResponse.enabled = qosListenerInfo->active ? 1 : 0;
		packetQosResponse.sizeData = qosListenerInfo->dataSize;
		
		if (qosListenerInfo->dataSize) {
			memcpy(&packetBuffer[packetSizeHeaderType + packetSizeTypeQosResponse], qosListenerInfo->pData, qosListenerInfo->dataSize);
		}
		
		SendToPerpetualSocket(
			perpetual_socket
			, (char*)packetBuffer
			, packetSize
			, 0
			, (const sockaddr*)sockAddrExternal
			, sockAddrExternalLen
		);
		
		delete[] packetBuffer;
	}
	
	LeaveCriticalSection(&xlive_critsec_qos_listeners);
}

void XLiveQosReceiveResponse(XLLNNetPacketType::QOS_RESPONSE *packetQosResponse)
{
	const int packetSizeTypeQosResponse = sizeof(XLLNNetPacketType::QOS_RESPONSE);
	
	std::chrono::steady_clock::time_point responseTime = std::chrono::high_resolution_clock::now();
	
	EnterCriticalSection(&xlive_critsec_qos_lookups);
	
	if (!xlive_qos_lookups.count(packetQosResponse->qosLookupId)) {
		LeaveCriticalSection(&xlive_critsec_qos_lookups);
		return;
	}
	
	QOS_PENDING_LOOKUP *qosPendingLookup = xlive_qos_lookups[packetQosResponse->qosLookupId];
	
	uint32_t iSessionId;
	for (iSessionId = 0; iSessionId < qosPendingLookup->countXn; iSessionId++) {
		if (*(uint64_t*)(&qosPendingLookup->xnKids[iSessionId]) == packetQosResponse->sessionId) {
			break;
		}
	}
	if (iSessionId >= qosPendingLookup->countXn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s QoS unhandled inaddr."
			, __func__
		);
		LeaveCriticalSection(&xlive_critsec_qos_lookups);
		return;
	}
	
	if (packetQosResponse->probeId != qosPendingLookup->qosTransitInfo[iSessionId].probesSent) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
			, "%s QoS old probe."
			, __func__
		);
		LeaveCriticalSection(&xlive_critsec_qos_lookups);
		return;
	}
	
	qosPendingLookup->xnQos->axnqosinfo[iSessionId].cProbesRecv++;
	qosPendingLookup->xnQos->axnqosinfo[iSessionId].bFlags |= XNET_XNQOSINFO_TARGET_CONTACTED;
	
	// TODO make a list of result pings to properly calculate these ping values. bandwidth idk.
	uint64_t milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(responseTime - qosPendingLookup->qosTransitInfo[iSessionId].timeLastCommSent).count();
	qosPendingLookup->xnQos->axnqosinfo[iSessionId].wRttMinInMsecs = milliseconds > 0xFFFFFFFF ? 0xFFFFFFFF : (uint32_t)milliseconds;
	qosPendingLookup->xnQos->axnqosinfo[iSessionId].wRttMedInMsecs = qosPendingLookup->xnQos->axnqosinfo[iSessionId].wRttMinInMsecs;
	qosPendingLookup->xnQos->axnqosinfo[iSessionId].dwUpBitsPerSec = QOS_LISTEN_BITS_PER_SEC_DEFAULT;
	qosPendingLookup->xnQos->axnqosinfo[iSessionId].dwDnBitsPerSec = qosPendingLookup->maxBitsPerSec;
	
	if (packetQosResponse->enabled) {
		qosPendingLookup->xnQos->axnqosinfo[iSessionId].bFlags &= ~(uint8_t)(XNET_XNQOSINFO_TARGET_DISABLED);
	}
	else {
		qosPendingLookup->xnQos->axnqosinfo[iSessionId].bFlags |= XNET_XNQOSINFO_TARGET_DISABLED;
	}
	
	if (packetQosResponse->sizeData) {
		qosPendingLookup->xnQos->axnqosinfo[iSessionId].bFlags |= XNET_XNQOSINFO_DATA_RECEIVED;
		qosPendingLookup->xnQos->axnqosinfo[iSessionId].cbData = packetQosResponse->sizeData;
		qosPendingLookup->xnQos->axnqosinfo[iSessionId].pbData = new uint8_t[qosPendingLookup->xnQos->axnqosinfo[iSessionId].cbData];
		uint8_t *titleData = (uint8_t*)&(((uint8_t*)packetQosResponse)[packetSizeTypeQosResponse]);
		memcpy(qosPendingLookup->xnQos->axnqosinfo[iSessionId].pbData, titleData, qosPendingLookup->xnQos->axnqosinfo[iSessionId].cbData);
	}
	
	LeaveCriticalSection(&xlive_critsec_qos_lookups);
}

void XLiveThreadQosStart()
{
	XLiveThreadQosStop();
	xlive_thread_exit_qos = FALSE;
	xlive_thread_qos = std::thread(ThreadXLiveQoS);
}

void XLiveThreadQosStop()
{
	if (xlive_thread_exit_qos == FALSE) {
		xlive_thread_exit_qos = TRUE;
		xlive_thread_cond_qos.notify_all();
		xlive_thread_qos.join();
	}
}

void XLiveThreadQosAbort()
{
	if (xlive_thread_exit_qos == FALSE) {
		xlive_thread_exit_qos = TRUE;
		xlive_thread_cond_qos.notify_all();
		xlive_thread_qos.detach();
	}
}
