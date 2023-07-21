#include <winsock2.h>
#include "xdefs.hpp"
#include "xnet.hpp"
#include "xlive.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/debug-text.hpp"
#include "xsocket.hpp"
#include "xnetqos.hpp"
#include "net-entity.hpp"
#include <WS2tcpip.h>
#include <iptypes.h>

BOOL xlive_net_initialized = FALSE;

XNetStartupParams xlive_net_startup_params;

XNADDR xlive_local_xnAddr;

CRITICAL_SECTION xlive_critsec_xnet_session_keys;

// Key: sessionId / xnkid.
static std::map<uint64_t, XNKEY*> xlive_xnet_session_keys;

static void ResetXNetStartupParams()
{
	xlive_net_startup_params.cfgSizeOfStruct = sizeof(XNetStartupParams);
	xlive_net_startup_params.cfgFlags = 0;
	xlive_net_startup_params.cfgSockMaxDgramSockets = 8;
	xlive_net_startup_params.cfgSockMaxStreamSockets = 32;
	xlive_net_startup_params.cfgSockDefaultRecvBufsizeInK = 16;
	xlive_net_startup_params.cfgSockDefaultSendBufsizeInK = 16;
	xlive_net_startup_params.cfgKeyRegMax = 8;
	xlive_net_startup_params.cfgSecRegMax = 32;
	xlive_net_startup_params.cfgQosDataLimitDiv4 = 64;
	xlive_net_startup_params.cfgQosProbeTimeoutInSeconds = 2;
	xlive_net_startup_params.cfgQosProbeRetries = 3;
	xlive_net_startup_params.cfgQosSrvMaxSimultaneousResponses = 8;
	xlive_net_startup_params.cfgQosPairWaitTimeInSeconds = 2;
}

static void UnregisterSecureAddr(const IN_ADDR ina)
{
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
}


// #51
INT WINAPI XNetStartup(const XNetStartupParams *pxnsp)
{
	TRACE_FX();
	
	ResetXNetStartupParams();
	
	if (pxnsp && pxnsp->cfgSizeOfStruct != sizeof(XNetStartupParams)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (pxnsp->cfgSizeOfStruct != sizeof(XNetStartupParams)) (%u != %u).", __func__, pxnsp->cfgSizeOfStruct, sizeof(XNetStartupParams));
		return ERROR_INVALID_PARAMETER;
	}
	if (xlive_netsocket_abort) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive NetSocket is disabled.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	
	if (pxnsp) {
		if (pxnsp->cfgFlags) {
			xlive_net_startup_params.cfgFlags = pxnsp->cfgFlags;
		}
		if (pxnsp->cfgSockMaxDgramSockets) {
			xlive_net_startup_params.cfgSockMaxDgramSockets = pxnsp->cfgSockMaxDgramSockets;
		}
		if (pxnsp->cfgSockMaxStreamSockets) {
			xlive_net_startup_params.cfgSockMaxStreamSockets = pxnsp->cfgSockMaxStreamSockets;
		}
		if (pxnsp->cfgSockDefaultRecvBufsizeInK) {
			xlive_net_startup_params.cfgSockDefaultRecvBufsizeInK = pxnsp->cfgSockDefaultRecvBufsizeInK;
		}
		if (pxnsp->cfgSockDefaultSendBufsizeInK) {
			xlive_net_startup_params.cfgSockDefaultSendBufsizeInK = pxnsp->cfgSockDefaultSendBufsizeInK;
		}
		if (pxnsp->cfgKeyRegMax) {
			xlive_net_startup_params.cfgKeyRegMax = pxnsp->cfgKeyRegMax;
		}
		if (pxnsp->cfgSecRegMax) {
			xlive_net_startup_params.cfgSecRegMax = pxnsp->cfgSecRegMax;
		}
		if (pxnsp->cfgQosDataLimitDiv4) {
			xlive_net_startup_params.cfgQosDataLimitDiv4 = pxnsp->cfgQosDataLimitDiv4;
		}
		if (pxnsp->cfgQosProbeTimeoutInSeconds) {
			xlive_net_startup_params.cfgQosProbeTimeoutInSeconds = pxnsp->cfgQosProbeTimeoutInSeconds;
		}
		if (pxnsp->cfgQosProbeRetries) {
			xlive_net_startup_params.cfgQosProbeRetries = pxnsp->cfgQosProbeRetries;
		}
		if (pxnsp->cfgQosSrvMaxSimultaneousResponses) {
			xlive_net_startup_params.cfgQosSrvMaxSimultaneousResponses = pxnsp->cfgQosSrvMaxSimultaneousResponses;
		}
		if (pxnsp->cfgQosPairWaitTimeInSeconds) {
			xlive_net_startup_params.cfgQosPairWaitTimeInSeconds = pxnsp->cfgQosPairWaitTimeInSeconds;
		}
	}
	
	xlive_net_initialized = TRUE;
	
	return ERROR_SUCCESS;
}

// #52
INT WINAPI XNetCleanup()
{
	TRACE_FX();
	
	xlive_net_initialized = FALSE;
	ResetXNetStartupParams();
	
	if (!xlive_online_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive Online is not initialised.", __func__);
		return WSANOTINITIALISED;
	}
	
	return ERROR_SUCCESS;
}

// #53
INT WINAPI XNetRandom(BYTE *pb, UINT cb)
{
	TRACE_FX();
	if (!pb) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pb is NULL.", __func__);
		return WSAEFAULT;
	}

	for (DWORD i = 0; i < cb; i++) {
		pb[i] = static_cast<BYTE>(rand());
	}

	return S_OK;
}

// #54
INT WINAPI XNetCreateKey(XNKID *pxnkid, XNKEY *pxnkey)
{
	TRACE_FX();
	if (!pxnkid) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnkid is NULL.", __func__);
		return WSAEFAULT;
	}
	if (!pxnkey) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnkey is NULL.", __func__);
		return WSAEFAULT;
	}

	memset(pxnkid, 0x8B, sizeof(XNKID));
	//memset(pxnkid, 0xAB, sizeof(XNKID));
	memset(pxnkey, 0XAA, sizeof(XNKEY));

	pxnkid->ab[0] &= ~XNET_XNKID_MASK;
	pxnkid->ab[0] |= XNET_XNKID_SYSTEM_LINK_XPLAT;
	
	return S_OK;
}

// #55
INT WINAPI XNetRegisterKey(const XNKID *pxnkid, const XNKEY *pxnkey)
{
	TRACE_FX();
	if (!xlive_net_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive Net is not initialised.", __func__);
		return E_UNEXPECTED;
	}
	if (!pxnkid) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnkid is NULL.", __func__);
		return WSAEFAULT;
	}
	if (!pxnkey) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnkey is NULL.", __func__);
		return WSAEFAULT;
	}
	if (!XNetXnKidIsOnlinePeer(pxnkid) && !XNetXnKidIsOnlineTitleServer(pxnkid) && !XNetXnKidIsSystemLinkXPlat(pxnkid)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XNetXnKidFlag(pxnkid) (0x%02x) is not XNET_XNKID_ONLINE_PEER, XNET_XNKID_ONLINE_TITLESERVER or XNET_XNKID_SYSTEM_LINK_XPLAT.", __func__, XNetXnKidFlag(pxnkid));
		return E_INVALIDARG;
	}
	
	const uint64_t &sessionId = *(uint64_t*)pxnkid;
	
	{
		EnterCriticalSection(&xlive_critsec_xnet_session_keys);
		
		if (xlive_xnet_session_keys.count(sessionId)) {
			LeaveCriticalSection(&xlive_critsec_xnet_session_keys);
			XLLN_DEBUG_LOG(
				XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnkid (0x%016I64x) is already registered."
				, __func__
				, sessionId
			);
			return ERROR_ALREADY_REGISTERED;
		}

		*(xlive_xnet_session_keys[sessionId] = new XNKEY) = *pxnkey;
		
		LeaveCriticalSection(&xlive_critsec_xnet_session_keys);
	}
	
	return S_OK;
}

// #56
INT WINAPI XNetUnregisterKey(const XNKID* pxnkid)
{
	TRACE_FX();
	if (!xlive_net_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive Net is not initialised.", __func__);
		return E_UNEXPECTED;
	}
	if (!pxnkid) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnkid is NULL.", __func__);
		return WSAEFAULT;
	}
	
	const uint64_t &sessionId = *(uint64_t*)pxnkid;
	
	{
		EnterCriticalSection(&xlive_critsec_qos_listeners);
		EnterCriticalSection(&xlive_critsec_xnet_session_keys);
		
		if (!xlive_xnet_session_keys.count(sessionId)) {
			EnterCriticalSection(&xlive_critsec_qos_listeners);
			LeaveCriticalSection(&xlive_critsec_xnet_session_keys);
			XLLN_DEBUG_LOG(
				XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnkid (0x%016I64x) does not exist."
				, __func__
				, sessionId
			);
			return ERROR_NOT_FOUND;
		}
		
		XNKEY *xnkey = xlive_xnet_session_keys[sessionId];
		xlive_qos_listeners.erase(sessionId);
		xlive_xnet_session_keys.erase(sessionId);
		delete xnkey;
		
		LeaveCriticalSection(&xlive_critsec_xnet_session_keys);
		LeaveCriticalSection(&xlive_critsec_qos_listeners);
	}
	
	return S_OK;
}

// #57
INT WINAPI XNetXnAddrToInAddr(XNADDR *pxna, XNKID *pnkid, IN_ADDR *pina)
{
	TRACE_FX();

	uint32_t instanceId = ntohl(pxna->inaOnline.s_addr);
	uint16_t portBaseHBO = ntohs(pxna->wPortOnline);
	uint32_t ipv4HBO = ntohl(pxna->ina.s_addr);

	uint32_t resultNetter = NetterEntityEnsureExists(instanceId, portBaseHBO);
	if (resultNetter) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_ERROR
			, "%s NetterEntityEnsureExists failed to create NetEntity 0x%08x:%hu with error 0x%08x."
			, __func__
			, instanceId
			, portBaseHBO
			, resultNetter
		);
		//*pina = 0;
		return E_UNEXPECTED;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "%s NetterEntityEnsureExists created 0x%08x:%hu."
		, __func__
		, instanceId
		, portBaseHBO
	);

	pina->s_addr = htonl(instanceId);

	return S_OK;
}

// #58
INT WINAPI XNetServerToInAddr(const IN_ADDR ina, DWORD dwServiceId, IN_ADDR *pina)
{
	TRACE_FX();
	if (!pina) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pina is NULL.", __func__);
		return WSAEFAULT;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	// actually look up the xlln instance.
	pina->s_addr = 0;
	return WSAEHOSTUNREACH;
	return ERROR_SUCCESS;
}

// #59
INT WINAPI XNetTsAddrToInAddr(const TSADDR *ptsa, DWORD dwServiceId, const XNKID *pxnkid, IN_ADDR *pina)
{
	TRACE_FX();
	if (!pina) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pina is NULL.", __func__);
		return WSAEFAULT;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	// actually look up the xlln instance / title server.
	pina->s_addr = 0;
	return WSAEHOSTUNREACH;
	return ERROR_SUCCESS;
}

// #60
INT WINAPI XNetInAddrToXnAddr(const IN_ADDR ina, XNADDR *pxna, XNKID *pxnkid)
{
	TRACE_FX();

	uint32_t instanceId = ntohl(ina.s_addr);

	uint32_t resultNetter = NetterEntityGetXnaddrByInstanceId(pxna, pxnkid, instanceId);
	if (resultNetter) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_ERROR
			, "XNetInAddrToXnAddr NetterEntityGetXnaddrByInstanceId failed to find NetEntity 0x%08x with error 0x%08x."
			, instanceId
			, resultNetter
		);
		return E_FAIL;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "XNetInAddrToXnAddr NetterEntityGetXnaddrByInstanceId found 0x%08x."
		, instanceId
	);

	return S_OK;
}

// #61
INT WINAPI XNetInAddrToServer(const IN_ADDR ina, IN_ADDR *pina)
{
	TRACE_FX();
	if (!pina) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pina is NULL.", __func__);
		return WSAEFAULT;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	// actually look up the xlln instance.
	pina->s_addr = 0;
	return WSAEHOSTUNREACH;
	return ERROR_SUCCESS;
}

// #62
INT WINAPI XNetInAddrToString(const IN_ADDR ina, char *pchBuf, INT cchBuf)
{
	TRACE_FX();
	if (!pchBuf) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pchBuf is NULL.", __func__);
		return WSAEFAULT;
	}
	if (!cchBuf) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cchBuf is 0.", __func__);
		return WSAEINVAL;
	}
	if (cchBuf < 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s cchBuf is negative.", __func__);
		return WSAEINVAL;
	}

	pchBuf[0] = 0;

	SOCKADDR_IN sockAddr;
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = 0;
	sockAddr.sin_addr = ina;

	// Maximum length of full domain name + sentinel.
	char namebuf[253 + 1];
	int errorGetNameInfo = getnameinfo((const sockaddr*)&sockAddr, sizeof(SOCKADDR_IN), namebuf, sizeof(namebuf), NULL, 0, NI_NUMERICHOST);
	if (errorGetNameInfo) {
		XLLN_DEBUG_LOG_ECODE(errorGetNameInfo, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s getnameinfo error:", __func__);
		return errorGetNameInfo;
	}
	size_t nameLen = strnlen(namebuf, sizeof(namebuf));
	if ((int32_t)nameLen >= cchBuf) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (nameLen >= cchBuf) (%zu >= %d).", __func__, nameLen, cchBuf);
		return WSAEMSGSIZE;
	}
	memcpy(pchBuf, namebuf, nameLen);
	pchBuf[nameLen] = 0;

	return ERROR_SUCCESS;
}

// #63
INT WINAPI XNetUnregisterInAddr(const IN_ADDR ina)
{
	TRACE_FX();
	UnregisterSecureAddr(ina);
	return S_OK;
}

// #64
INT WINAPI XNetXnAddrToMachineId(const XNADDR *pxnaddr, ULONGLONG *pqwMachineId)
{
	TRACE_FX();
	if (!pxnaddr) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnaddr is NULL.", __func__);
		return E_POINTER;
	}
	if (!pqwMachineId) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pqwMachineId is NULL.", __func__);
		return E_POINTER;
	}
	//TODO  !CIpAddr::IsValidUnicast(pxnaddr->ina.s_addr) || !CIpAddr::IsValidUnicast(pxnaddr->inaOnline.s_addr) || 
	if (pxnaddr->ina.s_addr == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnaddr->ina.s_addr is 0.", __func__);
		*pqwMachineId = 0;
		return E_INVALIDARG;
	}
	if (pxnaddr->inaOnline.s_addr == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnaddr->inaOnline.s_addr is 0.", __func__);
		*pqwMachineId = 0;
		return E_INVALIDARG;
	}
	if (pxnaddr->wPortOnline == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnaddr->wPortOnline is 0.", __func__);
		*pqwMachineId = 0;
		return E_INVALIDARG;
	}

	*pqwMachineId = *(ULONGLONG*)&pxnaddr->abOnline[8];
	return S_OK;
}

// #65
INT WINAPI XNetConnect(const IN_ADDR ina)
{
	TRACE_FX();
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return S_OK;
}

// #66
INT WINAPI XNetGetConnectStatus(const IN_ADDR ina)
{
	TRACE_FX();
	if (!xlive_net_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive Net is not initialised.", __func__);
		return XNET_CONNECT_STATUS_PENDING;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return XNET_CONNECT_STATUS_CONNECTED;
}

// #67
INT WINAPI XNetDnsLookup(const char *pszHost, WSAEVENT hEvent, XNDNS **ppxndns)
{
	TRACE_FX();
	if (!ppxndns) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ppxndns is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	// Improper Implementation.
	struct hostent *he = gethostbyname(pszHost);
	INT result = WSAGetLastError();

	if (he != NULL) {
		struct in_addr **addr_list = (struct in_addr **)he->h_addr_list;
		int i = 0;
		for (; addr_list[i] != NULL && i < 8; i++) {
			memcpy_s((&(*ppxndns)->aina)[i], sizeof(in_addr), addr_list[i], sizeof(in_addr));
		}
		(*ppxndns)->cina = i;
	}

	return result;
}

// #68
INT WINAPI XNetDnsRelease(XNDNS *pxndns)
{
	TRACE_FX();
	INT result = ERROR_SUCCESS;
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return result;
}

// #73
DWORD WINAPI XNetGetTitleXnAddr(XNADDR *pAddr)
{
	TRACE_FX();
	if (!xlive_net_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive Net is not initialised.", __func__);
		return XNET_GET_XNADDR_PENDING;
	}
	if (!pAddr) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pAddr is NULL.", __func__);
		return XNET_GET_XNADDR_PENDING;
	}
	*pAddr = xlive_local_xnAddr; // same as memcpy.
	return XNET_GET_XNADDR_STATIC | XNET_GET_XNADDR_ETHERNET | XNET_GET_XNADDR_ONLINE;
}

// #74
DWORD WINAPI XNetGetDebugXnAddr(XNADDR *pAddr)
{
	TRACE_FX();
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO verify that this works.", __func__);
	return XNetGetTitleXnAddr(pAddr);
}

// #75
DWORD WINAPI XNetGetEthernetLinkStatus()
{
	TRACE_FX();
	if (!xlive_net_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive Net is not initialised.", __func__);
		return XNET_ETHERNET_LINK_INACTIVE;
	}
	return XNET_ETHERNET_LINK_ACTIVE | XNET_ETHERNET_LINK_100MBPS | XNET_ETHERNET_LINK_FULL_DUPLEX;
}

//TODO XNetGetBroadcastVersionStatus
static DWORD xlive_broadcast_incompatible_version = 0;
//xlive_broadcast_incompatible_version = XNET_BROADCAST_VERSION_OLDER;
//xlive_broadcast_incompatible_version = XNET_BROADCAST_VERSION_NEWER;

// #76
DWORD WINAPI XNetGetBroadcastVersionStatus(BOOL fReset)
{
	TRACE_FX();
	DWORD result = xlive_broadcast_incompatible_version;
	if (fReset) {
		xlive_broadcast_incompatible_version = 0;
	}
	return result;
}

// #78
INT WINAPI XNetGetOpt(DWORD dwOptId, BYTE *pbValue, DWORD *pdwValueSize)
{
	TRACE_FX();
	if (!pbValue) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pbValue is NULL.", __func__);
		return WSAEFAULT;
	}
	if (!pdwValueSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwValueSize is NULL.", __func__);
		return WSAEFAULT;
	}
	if (!*pdwValueSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *pdwValueSize is 0.", __func__);
		return WSAEINVAL;
	}

	uint32_t requiredSize = 0;
	switch (dwOptId) {
		default: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s unknown dwOptId (0x%08x).", __func__, dwOptId);
			return WSAEINVAL;
		}
		case XNET_OPTID_STARTUP_PARAMS: {
			requiredSize = sizeof(XNetStartupParams);
			break;
		}
		case XNET_OPTID_NIC_XMIT_BYTES:
		case XNET_OPTID_NIC_RECV_BYTES:
		case XNET_OPTID_CALLER_XMIT_BYTES:
		case XNET_OPTID_CALLER_RECV_BYTES: {
			requiredSize = sizeof(ULONGLONG);
			break;
		}
		case XNET_OPTID_NIC_XMIT_FRAMES:
		case XNET_OPTID_NIC_RECV_FRAMES:
		case XNET_OPTID_CALLER_XMIT_FRAMES:
		case XNET_OPTID_CALLER_RECV_FRAMES: {
			requiredSize = sizeof(DWORD);
			break;
		}
	}

	if (requiredSize > *pdwValueSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (requiredSize > *pdwValueSize) (%u > %u).", __func__, requiredSize, *pdwValueSize);
		*pdwValueSize = requiredSize;
		return WSAEMSGSIZE;
	}

	*pdwValueSize = requiredSize;

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);

	switch (dwOptId) {
		case XNET_OPTID_STARTUP_PARAMS: {
			memset(pbValue, 0, sizeof(XNetStartupParams));
			break;
		}
		case XNET_OPTID_NIC_XMIT_BYTES: {
			*(ULONGLONG*)pbValue = 0;
			break;
		}
		case XNET_OPTID_NIC_XMIT_FRAMES: {
			*(DWORD*)pbValue = 0;
			break;
		}
		case XNET_OPTID_NIC_RECV_BYTES: {
			*(ULONGLONG*)pbValue = 0;
			break;
		}
		case XNET_OPTID_NIC_RECV_FRAMES: {
			*(DWORD*)pbValue = 0;
			break;
		}
		case XNET_OPTID_CALLER_XMIT_BYTES: {
			*(ULONGLONG*)pbValue = 0;
			break;
		}
		case XNET_OPTID_CALLER_XMIT_FRAMES: {
			*(DWORD*)pbValue = 0;
			break;
		}
		case XNET_OPTID_CALLER_RECV_BYTES: {
			*(ULONGLONG*)pbValue = 0;
			break;
		}
		case XNET_OPTID_CALLER_RECV_FRAMES: {
			*(DWORD*)pbValue = 0;
			break;
		}
	}

	return ERROR_SUCCESS;
}

// #79
INT WINAPI XNetSetOpt(DWORD dwOptId, BYTE *pbValue, DWORD *pdwValueSize)
{
	TRACE_FX();
	if (!pbValue) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pbValue is NULL.", __func__);
		return WSAEFAULT;
	}
	if (!pdwValueSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwValueSize is NULL.", __func__);
		return WSAEFAULT;
	}
	if (!*pdwValueSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *pdwValueSize is 0.", __func__);
		return WSAEINVAL;
	}

	switch (dwOptId) {
		default: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s unknown dwOptId (0x%08x).", __func__, dwOptId);
			return WSAEINVAL;
		}
		case XNET_OPTID_STARTUP_PARAMS: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XNET_OPTID_STARTUP_PARAMS cannot be set.", __func__);
			return WSAEINVAL;
		}
		case XNET_OPTID_NIC_XMIT_BYTES: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XNET_OPTID_NIC_XMIT_BYTES cannot be set.", __func__);
			return WSAEINVAL;
		}
		case XNET_OPTID_NIC_XMIT_FRAMES: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XNET_OPTID_NIC_XMIT_FRAMES cannot be set.", __func__);
			return WSAEINVAL;
		}
		case XNET_OPTID_NIC_RECV_BYTES: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XNET_OPTID_NIC_RECV_BYTES cannot be set.", __func__);
			return WSAEINVAL;
		}
		case XNET_OPTID_NIC_RECV_FRAMES: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XNET_OPTID_NIC_RECV_FRAMES cannot be set.", __func__);
			return WSAEINVAL;
		}
		case XNET_OPTID_CALLER_XMIT_BYTES: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XNET_OPTID_CALLER_XMIT_BYTES cannot be set.", __func__);
			return WSAEINVAL;
		}
		case XNET_OPTID_CALLER_XMIT_FRAMES: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XNET_OPTID_CALLER_XMIT_FRAMES cannot be set.", __func__);
			return WSAEINVAL;
		}
		case XNET_OPTID_CALLER_RECV_BYTES: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XNET_OPTID_CALLER_RECV_BYTES cannot be set.", __func__);
			return WSAEINVAL;
		}
		case XNET_OPTID_CALLER_RECV_FRAMES: {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XNET_OPTID_CALLER_RECV_FRAMES cannot be set.", __func__);
			return WSAEINVAL;
		}
	}

	return ERROR_SUCCESS;
}

// #80
// FIXME this is my guess at the function signature.
INT WINAPI XNetStartupEx(const XNetStartupParams *pxnsp, BOOL a2)
{
	TRACE_FX();
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	INT result = XNetStartup(pxnsp);
	return result;
}

// #81
INT WINAPI XNetReplaceKey(const XNKID *pxnkidUnregister, const XNKID *pxnkidReplace)
{
	TRACE_FX();
	if (!pxnkidUnregister) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnkidUnregister is NULL.", __func__);
		return WSAEFAULT;
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return ERROR_SUCCESS;
}

// #82
INT WINAPI XNetGetXnAddrPlatform(const XNADDR *pxnaddr, DWORD *pdwPlatform)
{
	TRACE_FX();
	if (!pdwPlatform) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pdwPlatform is NULL.", __func__);
		return WSAEFAULT;
	}
	*pdwPlatform = XNET_XNADDR_PLATFORM_XBOX1;
	if (*(uint32_t*)&pxnaddr->abEnet[0] == 0 && *(uint16_t*)&pxnaddr->abEnet[4] == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pxnaddr->abEnet is all 0.", __func__);
		return WSAEINVAL;
	}
	*pdwPlatform = XNET_XNADDR_PLATFORM_WINPC;
	return ERROR_SUCCESS;
}

// #83
// system_link_port_NBO - network byte (big-endian) order
INT WINAPI XNetGetSystemLinkPort(uint16_t *system_link_port_NBO)
{
	TRACE_FX();
	if (!system_link_port_NBO) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s system_link_port_NBO is NULL.", __func__);
		return E_POINTER;
	}
	// TODO XNetGetSystemLinkPort XEX_PRIVILEGE_CROSSPLATFORM_SYSTEM_LINK.
	
	*system_link_port_NBO = htons(xlive_port_system_link);
	
	return S_OK;
	return WSAEACCES;
}

// #84
// system_link_port_NBO - network byte (big-endian) order
INT WINAPI XNetSetSystemLinkPort(uint16_t system_link_port_NBO)
{
	TRACE_FX();
	// network byte (big-endian) to little-endian host.
	uint16_t portHBO = ntohs(system_link_port_NBO);
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	
	xlive_port_system_link = portHBO;
	
	return ERROR_SUCCESS;
}

// #5023
HRESULT WINAPI XNetGetCurrentAdapter(CHAR *pszAdapter, ULONG *pcchBuffer)
{
	TRACE_FX();
	if (!pcchBuffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pcchBuffer is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!xlive_net_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive Net is not initialised.", __func__);
		*pcchBuffer = 0;
		return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	}

	if (pszAdapter) {
		pszAdapter[0] = 0;
	}

	{
		EnterCriticalSection(&xlive_critsec_network_adapter);

		if (!xlive_network_adapter || !xlive_network_adapter->name) {
			LeaveCriticalSection(&xlive_critsec_network_adapter);
			*pcchBuffer = 0;
			return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
		}

		uint32_t nameLen = strnlen(xlive_network_adapter->name, MAX_ADAPTER_NAME_LENGTH + 4);
		if (nameLen >= *pcchBuffer) {
			*pcchBuffer = nameLen + 1;
		}
		else if (pszAdapter) {
			memcpy(pszAdapter, xlive_network_adapter->name, nameLen);
			pszAdapter[nameLen] = 0;
		}

		LeaveCriticalSection(&xlive_critsec_network_adapter);
	}

	return S_OK;
}

// #5296
// assuming online_port_NBO - network byte (big-endian) order
HRESULT WINAPI XLiveGetLocalOnlinePort(uint16_t *online_port_NBO)
{
	TRACE_FX();
	if (!online_port_NBO) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s online_port_NBO is NULL.", __func__);
		return E_INVALIDARG;
	}
	
	if (IsUsingBasePort(xlive_base_port)) {
		*online_port_NBO = htons(xlive_base_port);
	}
	else {
		*online_port_NBO = htons(xlive_port_online);
	}
	
	return S_OK;
}

BOOL InitXNet()
{
	ResetXNetStartupParams();
	
	return TRUE;
}

BOOL UninitXNet()
{
	return TRUE;
}
