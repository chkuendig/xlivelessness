#include <winsock2.h>
#include "xdefs.hpp"
#include "live-over-lan.hpp"
#include "packet-handler.hpp"
#include "net-entity.hpp"
#include "xsocket.hpp"
#include "xlocator.hpp"
#include "xsession.hpp"
#include "../xlln/debug-text.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"
#include "xnet.hpp"
#include "xlive.hpp"
#include <thread>
#include <condition_variable>
#include <atomic>
#include <vector>

CRITICAL_SECTION xlln_critsec_liveoverlan_sessions;
static std::condition_variable liveoverlan_cond_empty;
static std::thread liveoverlan_empty_thread;
static std::atomic<bool> liveoverlan_empty_exit = TRUE;

CRITICAL_SECTION xlln_critsec_liveoverlan_broadcast;
static std::condition_variable liveoverlan_cond_broadcast;
static std::thread liveoverlan_thread;
static BOOL liveoverlan_running = FALSE;
static std::atomic<bool> liveoverlan_exit = TRUE;
static std::atomic<bool> liveoverlan_break_sleep = FALSE;

// Key: InstanceId.
std::map<uint32_t, LIVE_SESSION_REMOTE*> liveoverlan_remote_sessions_xlocator;
std::map<uint32_t, LIVE_SESSION_REMOTE*> liveoverlan_remote_sessions_xsession;

void LiveOverLanBroadcastLocalSessionUnadvertise(const XUID xuid)
{
	EnterCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	if (xlive_xlocator_local_session && xlive_xlocator_local_session->xuid == xuid) {
		LiveOverLanDestroyLiveSession(&xlive_xlocator_local_session);
	}
	LeaveCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	
	SOCKADDR_IN SendStruct;
	if (IsUsingBasePort(xlive_base_port)) {
		SendStruct.sin_port = htons(xlive_base_port);
	}
	else {
		SendStruct.sin_port = htons(xlive_port_online);
	}
	SendStruct.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	SendStruct.sin_family = AF_INET;
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_DEBUG
		, "%s broadcast local sessions unadvertise to core socket 0x%08x to port %hu."
		, __func__
		, xlive_xsocket_perpetual_core_socket
		, ntohs(SendStruct.sin_port)
	);
	
	const int packetSizeHeaderType = sizeof(XLLNNetPacketType::TYPE);
	const int packetSizeTypeLiveOverLanUnadvertise = sizeof(XLLNNetPacketType::LIVE_OVER_LAN_UNADVERTISE);
	const int packetSize = packetSizeHeaderType + packetSizeTypeLiveOverLanUnadvertise;
	
	uint8_t *packetBuffer = new uint8_t[packetSize];
	packetBuffer[0] = XLLNNetPacketType::tLIVE_OVER_LAN_UNADVERTISE;
	XLLNNetPacketType::LIVE_OVER_LAN_UNADVERTISE &liveOverLanUnadvertise = *(XLLNNetPacketType::LIVE_OVER_LAN_UNADVERTISE*)&packetBuffer[packetSizeHeaderType];
	liveOverLanUnadvertise.xuid = xuid;
	
	XllnSocketSendTo(
		xlive_xsocket_perpetual_core_socket
		, (char*)packetBuffer
		, packetSize
		, 0
		, (const SOCKADDR*)&SendStruct
		, sizeof(SendStruct)
	);
	
	delete[] packetBuffer;
}

void LiveOverLanBroadcastRemoteSessionUnadvertise(const uint32_t instance_id, const uint8_t session_type, const XUID xuid)
{
	EnterCriticalSection(&xlln_critsec_liveoverlan_sessions);
	
	if (session_type == XLLN_LIVEOVERLAN_SESSION_TYPE_XLOCATOR) {
		// Delete the old entry if there already is one.
		if (liveoverlan_remote_sessions_xlocator.count(instance_id)) {
			LIVE_SESSION_REMOTE *liveSessionRemoteOld = liveoverlan_remote_sessions_xlocator[instance_id];
			LiveOverLanDestroyLiveSession(&liveSessionRemoteOld->liveSession);
			delete liveSessionRemoteOld;
			liveoverlan_remote_sessions_xlocator.erase(instance_id);
		}
	}
	else {
		// Delete the old entry if there already is one.
		if (liveoverlan_remote_sessions_xsession.count(instance_id)) {
			LIVE_SESSION_REMOTE *liveSessionRemoteOld = liveoverlan_remote_sessions_xsession[instance_id];
			LiveOverLanDestroyLiveSession(&liveSessionRemoteOld->liveSession);
			delete liveSessionRemoteOld;
			liveoverlan_remote_sessions_xsession.erase(instance_id);
		}
	}
	
	LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
}

void LiveOverLanAddRemoteLiveSession(const uint32_t instance_id, const uint8_t session_type, LIVE_SESSION *live_session)
{
	EnterCriticalSection(&xlln_critsec_liveoverlan_sessions);
	
	if (session_type == XLLN_LIVEOVERLAN_SESSION_TYPE_XLOCATOR) {
		// Delete the old entry if there already is one.
		if (liveoverlan_remote_sessions_xlocator.count(instance_id)) {
			LIVE_SESSION_REMOTE *liveSessionRemoteOld = liveoverlan_remote_sessions_xlocator[instance_id];
			LiveOverLanDestroyLiveSession(&liveSessionRemoteOld->liveSession);
			delete liveSessionRemoteOld;
		}
	}
	else {
		// Delete the old entry if there already is one.
		if (liveoverlan_remote_sessions_xsession.count(instance_id)) {
			LIVE_SESSION_REMOTE *liveSessionRemoteOld = liveoverlan_remote_sessions_xsession[instance_id];
			LiveOverLanDestroyLiveSession(&liveSessionRemoteOld->liveSession);
			delete liveSessionRemoteOld;
		}
	}

	XNADDR xnaddr;
	uint32_t resultNetter = NetterEntityGetXnaddrByInstanceId(&xnaddr, 0, instance_id);
	if (resultNetter) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_ERROR
			, "%s NetterEntityGetXnaddrByInstanceId failed to find NetEntity 0x%08x with error 0x%08x."
			, __func__
			, instance_id
			, resultNetter
		);
	}

	LIVE_SESSION_REMOTE *liveSessionRemote = new LIVE_SESSION_REMOTE;
	liveSessionRemote->liveSession = live_session;
	liveSessionRemote->xnAddr = xnaddr;
	time_t ltime;
	time(&ltime);
	liveSessionRemote->timeOfLastContact = (unsigned long)ltime;
	if (session_type == XLLN_LIVEOVERLAN_SESSION_TYPE_XLOCATOR) {
		liveoverlan_remote_sessions_xlocator[instance_id] = liveSessionRemote;
	}
	else {
		liveoverlan_remote_sessions_xsession[instance_id] = liveSessionRemote;
	}
	
	LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
}

static void LiveOverLanBroadcastLocalSession()
{
	EnterCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	
	bool anyXSessionLocalSessions = false;
	for (auto const &entry : xlive_xsession_local_sessions) {
		if (entry.second->liveSession->sessionType == XLLN_LIVEOVERLAN_SESSION_TYPE_XSESSION && entry.second->liveSession->sessionFlags & XSESSION_CREATE_HOST) {
			anyXSessionLocalSessions = true;
			break;
		}
	}
	
	if (!xlive_xlocator_local_session && !anyXSessionLocalSessions) {
		LeaveCriticalSection(&xlln_critsec_liveoverlan_broadcast);
		return;
	}
	
	SOCKADDR_IN SendStruct;
	if (IsUsingBasePort(xlive_base_port)) {
		SendStruct.sin_port = htons(xlive_base_port);
	}
	else {
		SendStruct.sin_port = htons(xlive_port_online);
	}
	SendStruct.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	SendStruct.sin_family = AF_INET;
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_DEBUG
		, "%s broadcast local session(s) to socket 0x%08x to port %hu."
		, __func__
		, xlive_xsocket_perpetual_core_socket
		, ntohs(SendStruct.sin_port)
	);
	
	if (xlive_xlocator_local_session) {
		uint8_t *liveSessionSerialisedPacket;
		uint32_t liveSessionSerialisedPacketSize;
		if (LiveOverLanSerialiseLiveSessionIntoNetPacket(xlive_xlocator_local_session, &liveSessionSerialisedPacket, &liveSessionSerialisedPacketSize)) {
			XllnSocketSendTo(
				xlive_xsocket_perpetual_core_socket
				, (char*)liveSessionSerialisedPacket
				, liveSessionSerialisedPacketSize
				, 0
				, (const SOCKADDR*)&SendStruct
				, sizeof(SendStruct)
			);
			delete[] liveSessionSerialisedPacket;
		}
	}
	
	if (anyXSessionLocalSessions) {
		for (auto const &entry : xlive_xsession_local_sessions) {
			if (!(entry.second->liveSession->sessionType == XLLN_LIVEOVERLAN_SESSION_TYPE_XSESSION && entry.second->liveSession->sessionFlags & XSESSION_CREATE_HOST)) {
				continue;
			}
			
			uint8_t *liveSessionSerialisedPacket;
			uint32_t liveSessionSerialisedPacketSize;
			if (LiveOverLanSerialiseLiveSessionIntoNetPacket(entry.second->liveSession, &liveSessionSerialisedPacket, &liveSessionSerialisedPacketSize)) {
				XllnSocketSendTo(
					xlive_xsocket_perpetual_core_socket
					, (char*)liveSessionSerialisedPacket
					, liveSessionSerialisedPacketSize
					, 0
					, (const SOCKADDR*)&SendStruct
					, sizeof(SendStruct)
				);
				delete[] liveSessionSerialisedPacket;
			}
		}
	}

	LeaveCriticalSection(&xlln_critsec_liveoverlan_broadcast);
}

static VOID LiveOverLanBroadcast()
{
	std::mutex mutexPause;
	while (1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_INFO
			, "%s continuing."
			, __func__
		);
		LiveOverLanBroadcastLocalSession();

		std::unique_lock<std::mutex> lock(mutexPause);
		liveoverlan_cond_broadcast.wait_for(lock, std::chrono::seconds(10), []() { return liveoverlan_exit == TRUE || liveoverlan_break_sleep == TRUE; });
		if (liveoverlan_exit) {
			break;
		}
		liveoverlan_break_sleep = FALSE;
	}
}

void LiveOverLanDestroyLiveSession(LIVE_SESSION **live_session)
{
	for (uint32_t iProperty = 0; iProperty < (*live_session)->propertiesCount; iProperty++) {
		XUSER_PROPERTY &property = (*live_session)->pProperties[iProperty];
		if (property.value.type == XUSER_DATA_TYPE_UNICODE) {
			delete[] property.value.string.pwszData;
		}
		else if (property.value.type == XUSER_DATA_TYPE_BINARY && property.value.binary.cbData) {
			delete[] property.value.binary.pbData;
		}
	}
	delete[] (*live_session)->pProperties;
	delete[] (*live_session)->pContexts;
	delete *live_session;
	*live_session = 0;
}

bool LiveOverLanDeserialiseLiveSession(
	const uint8_t *live_session_buffer
	, const uint32_t live_session_buffer_size
	, LIVE_SESSION **result_live_session
)
{
	if (!result_live_session) {
		return false;
	}
	*result_live_session = 0;
	if (!live_session_buffer) {
		return false;
	}

	LIVE_SESSION &liveSessionSerialised = *(LIVE_SESSION*)live_session_buffer;
	const uint32_t liveSessionStaticDataSize = (uint32_t)&liveSessionSerialised.pContexts - (uint32_t)&liveSessionSerialised;

	// --- Verify the buffer and length to ensure the object passed in is valid and not corrupt or truncated. ---
	{
		uint32_t bufferSizeCheck = 0;

		bufferSizeCheck += liveSessionStaticDataSize;

		if (bufferSizeCheck > live_session_buffer_size) {
			return false;
		}

		bufferSizeCheck += (liveSessionSerialised.contextsCount * sizeof(*liveSessionSerialised.pContexts));

		if (bufferSizeCheck > live_session_buffer_size) {
			return false;
		}

		for (uint32_t iProperty = 0; iProperty < liveSessionSerialised.propertiesCount; iProperty++) {
			XUSER_PROPERTY_SERIALISED &propertySerialised = *(XUSER_PROPERTY_SERIALISED*)&live_session_buffer[bufferSizeCheck];

			bufferSizeCheck += sizeof(propertySerialised.propertyId);
			bufferSizeCheck += sizeof(propertySerialised.type);

			if (bufferSizeCheck > live_session_buffer_size) {
				return false;
			}

			switch (propertySerialised.type) {
				case XUSER_DATA_TYPE_CONTEXT:
				case XUSER_DATA_TYPE_INT32: {
					bufferSizeCheck += sizeof(propertySerialised.nData);
					break;
				}
				case XUSER_DATA_TYPE_INT64: {
					bufferSizeCheck += sizeof(propertySerialised.i64Data);
					break;
				}
				case XUSER_DATA_TYPE_DOUBLE: {
					bufferSizeCheck += sizeof(propertySerialised.dblData);
					break;
				}
				case XUSER_DATA_TYPE_FLOAT: {
					bufferSizeCheck += sizeof(propertySerialised.fData);
					break;
				}
				case XUSER_DATA_TYPE_DATETIME: {
					bufferSizeCheck += sizeof(propertySerialised.ftData);
					break;
				}
				case XUSER_DATA_TYPE_BINARY: {
					bufferSizeCheck += sizeof(propertySerialised.binary.cbData);
					if (bufferSizeCheck > live_session_buffer_size) {
						return false;
					}
					bufferSizeCheck += propertySerialised.binary.cbData;
					break;
				}
				case XUSER_DATA_TYPE_UNICODE: {
					bufferSizeCheck += sizeof(propertySerialised.string.cbData);
					if (bufferSizeCheck > live_session_buffer_size) {
						return false;
					}
					uint32_t dataSize = propertySerialised.string.cbData;
					if (dataSize < sizeof(wchar_t)) {
						return false;
					}
					else if (dataSize % 2) {
						return false;
					}
					bufferSizeCheck += dataSize;
					break;
				}
				case XUSER_DATA_TYPE_NULL: {
					break;
				}
				default: {
					return false;
				}
			}

			if (bufferSizeCheck > live_session_buffer_size) {
				return false;
			}
		}

		if (bufferSizeCheck != live_session_buffer_size) {
			return false;
		}
	}

	// --- Parse out all the contents into a new Live Session struct. ---

	LIVE_SESSION *liveSession = new LIVE_SESSION;

	memcpy(liveSession, live_session_buffer, liveSessionStaticDataSize);

	if (!liveSession->contextsCount) {
		liveSession->pContexts = 0;
	}
	else {
		liveSession->pContexts = new XUSER_CONTEXT[liveSession->contextsCount];
		XUSER_CONTEXT *pXUserContextsSerialised = (XUSER_CONTEXT*)&(live_session_buffer[liveSessionStaticDataSize]);
		for (uint32_t iContext = 0; iContext < liveSession->contextsCount; iContext++) {
			XUSER_CONTEXT &context = liveSession->pContexts[iContext];
			XUSER_CONTEXT &contextSerialised = pXUserContextsSerialised[iContext];
			context = contextSerialised;
		}
	}

	uint8_t *pXUserPropertiesBuffer = (uint8_t*)&(live_session_buffer[liveSessionStaticDataSize + (liveSession->contextsCount * sizeof(*liveSession->pContexts))]);

	if (!liveSession->propertiesCount) {
		liveSession->pProperties = 0;
	}
	else {
		liveSession->pProperties = new XUSER_PROPERTY[liveSession->propertiesCount];
		XUSER_CONTEXT *pXUserContextsSerialised = (XUSER_CONTEXT*)&(live_session_buffer[liveSessionStaticDataSize]);
		for (uint32_t iProperty = 0; iProperty < liveSession->propertiesCount; iProperty++) {
			XUSER_PROPERTY &property = liveSession->pProperties[iProperty];
			XUSER_PROPERTY_SERIALISED &propertySerialised = *(XUSER_PROPERTY_SERIALISED*)pXUserPropertiesBuffer;

			uint32_t xuserPropertySize = 0;

			xuserPropertySize += sizeof(propertySerialised.propertyId);
			property.dwPropertyId = propertySerialised.propertyId;

			xuserPropertySize += sizeof(propertySerialised.type);
			property.value.type = propertySerialised.type;

			switch (propertySerialised.type) {
				case XUSER_DATA_TYPE_CONTEXT:
				case XUSER_DATA_TYPE_INT32: {
					xuserPropertySize += sizeof(propertySerialised.nData);
					property.value.nData = propertySerialised.nData;
					break;
				}
				case XUSER_DATA_TYPE_INT64: {
					xuserPropertySize += sizeof(propertySerialised.i64Data);
					property.value.i64Data = propertySerialised.i64Data;
					break;
				}
				case XUSER_DATA_TYPE_DOUBLE: {
					xuserPropertySize += sizeof(propertySerialised.dblData);
					property.value.dblData = propertySerialised.dblData;
					break;
				}
				case XUSER_DATA_TYPE_FLOAT: {
					xuserPropertySize += sizeof(propertySerialised.fData);
					property.value.fData = propertySerialised.fData;
					break;
				}
				case XUSER_DATA_TYPE_DATETIME: {
					xuserPropertySize += sizeof(propertySerialised.ftData);
					property.value.ftData = propertySerialised.ftData;
					break;
				}
				case XUSER_DATA_TYPE_BINARY: {
					xuserPropertySize += sizeof(propertySerialised.binary.cbData);
					property.value.binary.cbData = propertySerialised.binary.cbData;
					if (property.value.binary.cbData == 0) {
						property.value.binary.pbData = 0;
						break;
					}
					xuserPropertySize += property.value.binary.cbData;
					property.value.binary.pbData = new uint8_t[property.value.binary.cbData];
					memcpy(property.value.binary.pbData, &propertySerialised.binary.pbData, property.value.binary.cbData);
					break;
				}
				case XUSER_DATA_TYPE_UNICODE: {
					xuserPropertySize += sizeof(propertySerialised.string.cbData);
					property.value.string.cbData = propertySerialised.string.cbData;
					xuserPropertySize += property.value.string.cbData;
					property.value.string.pwszData = new wchar_t[property.value.string.cbData / 2];
					memcpy(property.value.string.pwszData, &propertySerialised.string.pwszData, property.value.string.cbData);
					property.value.string.pwszData[(property.value.string.cbData / 2) - 1] = 0;
					break;
				}
			}

			pXUserPropertiesBuffer += xuserPropertySize;
		}
	}

	if (pXUserPropertiesBuffer != live_session_buffer + live_session_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_FATAL
			, "%s the end result of the pXUserPropertiesBuffer (0x%08x) should not be different from (0x%08x) the result buffer (0x%08x) + buffer size (0x%08x)."
			, __func__
			, pXUserPropertiesBuffer
			, live_session_buffer + live_session_buffer_size
			, live_session_buffer
			, live_session_buffer_size
		);
		__debugbreak();
		return false;
	}

	*result_live_session = liveSession;

	return true;
}

bool LiveOverLanSerialiseLiveSessionIntoNetPacket(
	LIVE_SESSION *live_session
	, uint8_t **result_buffer
	, uint32_t *result_buffer_size
)
{
	if (result_buffer_size) {
		*result_buffer_size = 0;
	}
	if (result_buffer) {
		*result_buffer = 0;
	}
	if (!live_session || !result_buffer || !result_buffer_size) {
		return false;
	}

	*result_buffer_size += sizeof(XLLNNetPacketType::TYPE);
	const uint32_t liveSessionStaticDataSize = (uint32_t)&live_session->pContexts - (uint32_t)live_session;
	*result_buffer_size += liveSessionStaticDataSize;
	*result_buffer_size += live_session->contextsCount * sizeof(*live_session->pContexts);
	{
		for (uint32_t iProperty = 0; iProperty < live_session->propertiesCount; iProperty++) {
			XUSER_PROPERTY &property = live_session->pProperties[iProperty];

			uint32_t xuserPropertySize = 0;
			xuserPropertySize += sizeof(XUSER_PROPERTY_SERIALISED::propertyId);
			xuserPropertySize += sizeof(XUSER_PROPERTY_SERIALISED::type);

			switch (property.value.type) {
				case XUSER_DATA_TYPE_CONTEXT:
				case XUSER_DATA_TYPE_INT32: {
					xuserPropertySize += sizeof(XUSER_PROPERTY_SERIALISED::nData);
					break;
				}
				case XUSER_DATA_TYPE_INT64: {
					xuserPropertySize += sizeof(XUSER_PROPERTY_SERIALISED::i64Data);
					break;
				}
				case XUSER_DATA_TYPE_DOUBLE: {
					xuserPropertySize += sizeof(XUSER_PROPERTY_SERIALISED::dblData);
					break;
				}
				case XUSER_DATA_TYPE_FLOAT: {
					xuserPropertySize += sizeof(XUSER_PROPERTY_SERIALISED::fData);
					break;
				}
				case XUSER_DATA_TYPE_DATETIME: {
					xuserPropertySize += sizeof(XUSER_PROPERTY_SERIALISED::ftData);
					break;
				}
				case XUSER_DATA_TYPE_BINARY: {
					xuserPropertySize += sizeof(XUSER_PROPERTY_SERIALISED::binary.cbData);
					xuserPropertySize += property.value.binary.cbData;
					break;
				}
				case XUSER_DATA_TYPE_UNICODE: {
					uint32_t dataSize = property.value.string.cbData;
					if (dataSize < sizeof(wchar_t)) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
							, "%s Property 0x%08x XUSER_DATA_TYPE_UNICODE (dataSize < sizeof(wchar_t)) (%u < %u)."
							, __func__
							, property.dwPropertyId
							, dataSize
							, sizeof(wchar_t)
						);
						dataSize = sizeof(wchar_t);
					}
					else if (dataSize % 2) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
							, "%s Property 0x%08x XUSER_DATA_TYPE_UNICODE dataSize (%u) is odd."
							, __func__
							, property.dwPropertyId
							, dataSize
						);
						dataSize += 1;
					}
					xuserPropertySize += sizeof(dataSize);
					xuserPropertySize += dataSize;
					break;
				}
			}

			*result_buffer_size += xuserPropertySize;
		}
	}

	*result_buffer = new uint8_t[*result_buffer_size];
	((uint8_t*)*result_buffer)[0] = XLLNNetPacketType::tLIVE_OVER_LAN_ADVERTISE;
	memcpy(&(((uint8_t*)*result_buffer)[sizeof(XLLNNetPacketType::TYPE)]), live_session, liveSessionStaticDataSize);
	XUSER_CONTEXT *pXUserContexts = (XUSER_CONTEXT*)&(((uint8_t*)*result_buffer)[sizeof(XLLNNetPacketType::TYPE) + liveSessionStaticDataSize]);
	for (uint32_t iContext = 0; iContext < live_session->contextsCount; iContext++) {
		XUSER_CONTEXT &context = live_session->pContexts[iContext];
		XUSER_CONTEXT &contextSerialised = pXUserContexts[iContext];
		contextSerialised = context;
	}

	uint8_t *pXUserPropertiesBuffer = (uint8_t*)&(((uint8_t*)*result_buffer)[sizeof(XLLNNetPacketType::TYPE) + liveSessionStaticDataSize + (live_session->contextsCount * sizeof(*live_session->pContexts))]);
	for (uint32_t iProperty = 0; iProperty < live_session->propertiesCount; iProperty++) {
		XUSER_PROPERTY &property = live_session->pProperties[iProperty];
		XUSER_PROPERTY_SERIALISED &propertySerialised = *(XUSER_PROPERTY_SERIALISED*)pXUserPropertiesBuffer;
		
		uint32_t xuserPropertySize = 0;

		xuserPropertySize += sizeof(propertySerialised.propertyId);
		propertySerialised.propertyId = property.dwPropertyId;

		xuserPropertySize += sizeof(propertySerialised.type);
		propertySerialised.type = property.value.type;

		switch (propertySerialised.type) {
			case XUSER_DATA_TYPE_CONTEXT:
			case XUSER_DATA_TYPE_INT32: {
				xuserPropertySize += sizeof(propertySerialised.nData);
				propertySerialised.nData = property.value.nData;
				break;
			}
			case XUSER_DATA_TYPE_INT64: {
				xuserPropertySize += sizeof(propertySerialised.i64Data);
				propertySerialised.i64Data = property.value.i64Data;
				break;
			}
			case XUSER_DATA_TYPE_DOUBLE: {
				xuserPropertySize += sizeof(propertySerialised.dblData);
				propertySerialised.dblData = property.value.dblData;
				break;
			}
			case XUSER_DATA_TYPE_FLOAT: {
				xuserPropertySize += sizeof(propertySerialised.fData);
				propertySerialised.fData = property.value.fData;
				break;
			}
			case XUSER_DATA_TYPE_DATETIME: {
				xuserPropertySize += sizeof(propertySerialised.ftData);
				propertySerialised.ftData = property.value.ftData;
				break;
			}
			case XUSER_DATA_TYPE_BINARY: {
				xuserPropertySize += sizeof(propertySerialised.binary.cbData);
				propertySerialised.binary.cbData = property.value.binary.cbData;
				if (propertySerialised.binary.cbData == 0) {
					break;
				}
				xuserPropertySize += property.value.binary.cbData;
				memcpy(&propertySerialised.binary.pbData, property.value.binary.pbData, propertySerialised.binary.cbData);
				break;
			}
			case XUSER_DATA_TYPE_UNICODE: {
				uint32_t dataSize = property.value.string.cbData;
				if (dataSize < sizeof(wchar_t)) {
					dataSize = sizeof(wchar_t);
				}
				else if (dataSize % 2) {
					dataSize += 1;
				}
				xuserPropertySize += sizeof(dataSize);
				propertySerialised.string.cbData = dataSize;
				xuserPropertySize += dataSize;
				memcpy(&propertySerialised.string.pwszData, property.value.string.pwszData, dataSize);
				((wchar_t*)&propertySerialised.string.pwszData)[(dataSize / 2) - 1] = 0;
				break;
			}
			case XUSER_DATA_TYPE_NULL: {
				break;
			}
			default: {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
					, "%s Property 0x%08x has unknown XUSER_DATA_TYPE (0x%02hhx). Changing to NULL."
					, __func__
					, propertySerialised.propertyId
					, propertySerialised.type
				);
				propertySerialised.type = XUSER_DATA_TYPE_NULL;
				break;
			}
		}

		pXUserPropertiesBuffer += xuserPropertySize;
	}

	if (pXUserPropertiesBuffer != *result_buffer + *result_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_FATAL
			, "%s the end result of the pXUserPropertiesBuffer (0x%08x) should not be different from (0x%08x) the result buffer (0x%08x) + buffer size (0x%08x)."
			, __func__
			, pXUserPropertiesBuffer
			, *result_buffer + *result_buffer_size
			, *result_buffer
			, *result_buffer_size
		);
		__debugbreak();
		return false;
	}
	
	return true;
}

VOID LiveOverLanStartBroadcast()
{
	EnterCriticalSection(&xlln_critsec_liveoverlan_broadcast);

	if (liveoverlan_running) {
		liveoverlan_break_sleep = TRUE;
		liveoverlan_cond_broadcast.notify_all();
	}
	else {
		liveoverlan_running = TRUE;
		liveoverlan_exit = FALSE;
		liveoverlan_thread = std::thread(LiveOverLanBroadcast);
	}

	LeaveCriticalSection(&xlln_critsec_liveoverlan_broadcast);
}
VOID LiveOverLanStopBroadcast()
{
	EnterCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	if (xlive_xlocator_local_session) {
		LiveOverLanDestroyLiveSession(&xlive_xlocator_local_session);
	}
	xlive_xlocator_local_session = 0;

	if (liveoverlan_running) {
		liveoverlan_running = FALSE;
		LeaveCriticalSection(&xlln_critsec_liveoverlan_broadcast);
		if (liveoverlan_exit == FALSE) {
			liveoverlan_exit = TRUE;
			liveoverlan_cond_broadcast.notify_all();
			liveoverlan_thread.join();
		}
	}
	else {
		LeaveCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	}
}

static VOID LiveOverLanEmpty()
{
	std::mutex mymutex;
	while (1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_INFO
			, "%s continuing. Removing any old entries."
			, __func__
		);
		
		{
			EnterCriticalSection(&xlln_critsec_liveoverlan_sessions);
			
			time_t ltime;
			time(&ltime);//seconds since epoch.
			DWORD timetoremove = ((DWORD)ltime) - 15;
			std::vector<uint32_t> removesessions;
			
			for (auto const &session : liveoverlan_remote_sessions_xlocator) {
				if (session.second->timeOfLastContact > timetoremove) {
					continue;
				}
				LiveOverLanDestroyLiveSession(&session.second->liveSession);
				delete session.second;
				removesessions.push_back(session.first);
			}
			for (auto const &session : removesessions) {
				liveoverlan_remote_sessions_xlocator.erase(session);
			}
			removesessions.clear();
			
			for (auto const &session : liveoverlan_remote_sessions_xsession) {
				if (session.second->timeOfLastContact > timetoremove) {
					continue;
				}
				LiveOverLanDestroyLiveSession(&session.second->liveSession);
				delete session.second;
				removesessions.push_back(session.first);
			}
			for (auto const &session : removesessions) {
				liveoverlan_remote_sessions_xsession.erase(session);
			}
			removesessions.clear();
			
			LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
		}
		
		std::unique_lock<std::mutex> lock(mymutex);
		liveoverlan_cond_empty.wait_for(lock, std::chrono::seconds(10), []() { return liveoverlan_empty_exit == TRUE; });
		if (liveoverlan_empty_exit) {
			break;
		}
	}
}
VOID LiveOverLanStartEmpty()
{
	if (liveoverlan_empty_exit == FALSE) {
		liveoverlan_empty_exit = TRUE;
		liveoverlan_cond_empty.notify_all();
		liveoverlan_empty_thread.join();
	}
	liveoverlan_empty_exit = FALSE;
	liveoverlan_empty_thread = std::thread(LiveOverLanEmpty);
}
VOID LiveOverLanStopEmpty()
{
	EnterCriticalSection(&xlln_critsec_liveoverlan_sessions);
	
	for (auto const &session : liveoverlan_remote_sessions_xlocator) {
		LiveOverLanDestroyLiveSession(&session.second->liveSession);
		delete session.second;
	}
	liveoverlan_remote_sessions_xlocator.clear();
	
	for (auto const &session : liveoverlan_remote_sessions_xsession) {
		LiveOverLanDestroyLiveSession(&session.second->liveSession);
		delete session.second;
	}
	liveoverlan_remote_sessions_xsession.clear();
	
	LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
	
	if (liveoverlan_empty_exit == FALSE) {
		liveoverlan_empty_exit = TRUE;
		liveoverlan_cond_empty.notify_all();
		liveoverlan_empty_thread.join();
	}
}

VOID LiveOverLanAbort()
{
	if (liveoverlan_empty_exit == FALSE) {
		liveoverlan_empty_exit = TRUE;
		liveoverlan_cond_empty.notify_all();
		liveoverlan_empty_thread.detach();
	}
	if (liveoverlan_exit == FALSE) {
		liveoverlan_exit = TRUE;
		liveoverlan_cond_broadcast.notify_all();
		liveoverlan_thread.detach();
	}
}
