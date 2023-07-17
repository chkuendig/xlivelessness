#include <winsock2.h>
#include <Windows.h>
#include "../xlive/xdefs.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/wnd-main.hpp"
#include "../xlive/xlive.hpp"
#include "../xlive/packet-handler.hpp"
#include "../xlive/live-over-lan.hpp"
#include "../xlive/xsocket.hpp"
#include "../xlive/xlocator.hpp"
#include "../xlive/xnet.hpp"
#include "../resource.h"
#include "./xlln-keep-alive.hpp"
#include <thread>
#include <condition_variable>
#include <atomic>

static std::condition_variable xlln_keep_alive_cond;
static std::thread xlln_keep_alive_thread;
static std::condition_variable xlln_core_socket_cond;
static std::thread xlln_core_socket_thread;
static std::atomic<bool> xlln_keep_alive_exit = TRUE;

static void ThreadKeepAlive()
{
	const size_t hubRequestPacketBufferSize = sizeof(XLLNNetPacketType::TYPE) + sizeof(XLLNNetPacketType::HUB_REQUEST_PACKET);
	uint8_t *hubRequestPacketBuffer = new uint8_t[hubRequestPacketBufferSize];
	hubRequestPacketBuffer[0] = XLLNNetPacketType::tHUB_REQUEST;
	XLLNNetPacketType::HUB_REQUEST_PACKET *hubRequest = (XLLNNetPacketType::HUB_REQUEST_PACKET*)&hubRequestPacketBuffer[sizeof(XLLNNetPacketType::TYPE)];
	hubRequest->xllnVersion = (DLL_VERSION_MAJOR << 24) + (DLL_VERSION_MINOR << 16) + (DLL_VERSION_REVISION << 8) + DLL_VERSION_BUILD;
	hubRequest->titleId = xlive_title_id;
	hubRequest->titleVersion = xlive_title_version;
	
	std::mutex mutexPause;
	while (1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_INFO
			, "%s continuing. Keeping any Hub connections alive."
			, __func__
		);
		
		__time64_t ltime;
		_time64(&ltime);//seconds since epoch.
		
		if (xlln_direct_ip_connect.timeoutAt && xlln_direct_ip_connect.timeoutAt < ltime) {
			XllnDirectIpConnectCancel();
			MessageBoxW(xlln_window_hwnd, L"Connection attempt timed out.", L"XLLN Direct IP Connect Error", MB_OK);
		}
		
		hubRequest->instanceId = ntohl(xlive_local_xnAddr.inaOnline.s_addr);
		hubRequest->portBaseHBO = xlive_base_port;
		
		if (hubRequest->instanceId) {
			EnterCriticalSection(&xlive_critsec_broadcast_addresses);
			
			__time64_t timeToSendKeepAlive = ltime - 30;
			for (auto const &broadcastEntity : xlive_broadcast_addresses) {
				if (broadcastEntity.entityType != XLLNBroadcastEntity::TYPE::tHUB_SERVER && broadcastEntity.entityType != XLLNBroadcastEntity::TYPE::tUNKNOWN) {
					continue;
				}
				if (broadcastEntity.lastComm > timeToSendKeepAlive) {
					continue;
				}
				
				SendToPerpetualSocket(
					xlive_xsocket_perpetual_core_socket
					, (char*)hubRequestPacketBuffer
					, (int)hubRequestPacketBufferSize
					, 0
					, (const sockaddr*)&broadcastEntity.sockaddr
					, sizeof(broadcastEntity.sockaddr)
				);
			}
			
			LeaveCriticalSection(&xlive_critsec_broadcast_addresses);
		}
		
		std::unique_lock<std::mutex> lock(mutexPause);
		xlln_keep_alive_cond.wait_for(lock, std::chrono::seconds(5), []() { return xlln_keep_alive_exit == TRUE; });
		if (xlln_keep_alive_exit) {
			break;
		}
	}
}

static void ThreadCoreSocket()
{
	std::mutex mutexPause;
	while (1) {
		bool shouldRecv = false;
		
		EnterCriticalSection(&xlive_critsec_sockets);
		
		if (xlive_xsocket_perpetual_core_socket != INVALID_SOCKET) {
			SOCKET transitorySocket = xlive_xsocket_perpetual_to_transitory_socket[xlive_xsocket_perpetual_core_socket];
			SOCKET_MAPPING_INFO *socketMappingInfo = xlive_socket_info[transitorySocket];
			if (socketMappingInfo->perpetualSocket == xlive_xsocket_perpetual_core_socket) {
				shouldRecv = true;
			}
		}
		
		LeaveCriticalSection(&xlive_critsec_sockets);
		
		if (shouldRecv) {
			// The function already captures XLLN packets so anything else we can just ignore anyway. All we need is a thread to trigger the read from the socket.
			const int dataBufferSize = 1024;
			char dataBuffer[dataBufferSize];
			SOCKADDR_STORAGE sockAddrExternal;
			int sockAddrExternalLen = sizeof(sockAddrExternal);
			while (1) {
				int resultRecvFromDataSize = XSocketRecvFrom(xlive_xsocket_perpetual_core_socket, dataBuffer, dataBufferSize, 0, (sockaddr*)&sockAddrExternal, &sockAddrExternalLen);
				if (resultRecvFromDataSize > 0) {
					// Empty the socket of queued packets.
					continue;
				}
				if (resultRecvFromDataSize == SOCKET_ERROR) {
					int resultErrorRecvFrom = WSAGetLastError();
					if (resultErrorRecvFrom == WSAEWOULDBLOCK) {
						// Normal error. No more data.
						break;
					}
				}
				// Perhaps log error here.
				break;
			}
		}
		
		std::unique_lock<std::mutex> lock(mutexPause);
		xlln_core_socket_cond.wait_for(lock, std::chrono::seconds(1), []() { return xlln_keep_alive_exit == TRUE; });
		if (xlln_keep_alive_exit) {
			break;
		}
	}
}

void XLLNKeepAliveStart()
{
	TRACE_FX();
	
	XLLNKeepAliveStop();
	xlln_keep_alive_exit = FALSE;
	xlln_keep_alive_thread = std::thread(ThreadKeepAlive);
	xlln_core_socket_thread = std::thread(ThreadCoreSocket);
}

void XLLNKeepAliveStop()
{
	TRACE_FX();
	
	if (xlln_keep_alive_exit == FALSE) {
		xlln_keep_alive_exit = TRUE;
		xlln_keep_alive_cond.notify_all();
		xlln_keep_alive_thread.join();
		xlln_core_socket_cond.notify_all();
		xlln_core_socket_thread.join();
	}
}

void XLLNKeepAliveAbort()
{
	TRACE_FX();
	
	if (xlln_keep_alive_exit == FALSE) {
		xlln_keep_alive_exit = TRUE;
		xlln_keep_alive_cond.notify_all();
		xlln_keep_alive_thread.detach();
		xlln_core_socket_cond.notify_all();
		xlln_core_socket_thread.detach();

		XLLNCloseCoreSocket();
	}
}
