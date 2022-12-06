#include <winsock2.h>
#include "xdefs.hpp"
#include "xwsa.hpp"
#include "xnet.hpp"
#include "xsocket.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/xlln.hpp"

// #1
INT WINAPI XWSAStartup(WORD wVersionRequested, WSADATA *lpWSAData)
{
	TRACE_FX();
	INT result = WSAStartup(wVersionRequested, lpWSAData);
	if (result != ERROR_SUCCESS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "WSAStartup %08x.", result);
		return result;
	}
	
	result = XNetStartup(0);
	if (result != ERROR_SUCCESS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XNetStartup %08x.", result);
		return result;
	}
	
	return ERROR_SUCCESS;
}

// #2
INT WINAPI XWSACleanup()
{
	TRACE_FX();
	INT result = WSACleanup();
	if (result != ERROR_SUCCESS) {
		INT errorWSACleanup = WSAGetLastError();
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s SOCKET_ERROR %08x.", __func__, errorWSACleanup);
		WSASetLastError(errorWSACleanup);
		return SOCKET_ERROR;
	}
	return ERROR_SUCCESS;
}

// #16
BOOL WINAPI XWSAGetOverlappedResult(SOCKET perpetual_socket, WSAOVERLAPPED *lpOverlapped, DWORD *lpcbTransfer, BOOL fWait, DWORD *lpdwFlags)
{
	TRACE_FX();
	
	while (1) {
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		BOOL resultGetOverlappedResult = WSAGetOverlappedResult(transitorySocket, lpOverlapped, lpcbTransfer, fWait, lpdwFlags);
		int errorGetOverlappedResult = WSAGetLastError();
		
		if (!resultGetOverlappedResult && XSocketPerpetualSocketChangedError(errorGetOverlappedResult, perpetual_socket, transitorySocket)) {
			continue;
		}
		
		if (!resultGetOverlappedResult) {
			XLLN_DEBUG_LOG_ECODE(errorGetOverlappedResult, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s GetOverlappedResult error on socket (P,T) (0x%08x,0x%08x)."
				, __func__
				, perpetual_socket
				, transitorySocket
			);
		}
		
		WSASetLastError(errorGetOverlappedResult);
		return resultGetOverlappedResult;
	}
}

// #17
int WINAPI XWSACancelOverlappedIO(SOCKET perpetual_socket)
{
	TRACE_FX();
	if (perpetual_socket == INVALID_SOCKET) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s perpetual_socket is INVALID_SOCKET.", __func__);
		WSASetLastError(WSAENOTSOCK);
		return SOCKET_ERROR;
	}
	SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
	if (transitorySocket == INVALID_SOCKET) {
		WSASetLastError(WSAENOTSOCK);
		return SOCKET_ERROR;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	// TODO keep track of the (only one should exist at a time per socket or one per send and recv = 2? per socket) overlapped event from recv or send and lookup here to use in the cancel call.
	//return WSACancelAsyncRequest(hEvent);
	WSASetLastError(WSASYSCALLFAILURE);
	return SOCKET_ERROR;
}

// #19
int WINAPI XWSARecv(
	SOCKET perpetual_socket
	, WSABUF *lpBuffers
	, DWORD dwBufferCount
	, DWORD *lpNumberOfBytesRecvd
	, DWORD *lpFlags
	, WSAOVERLAPPED *lpOverlapped
	, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
)
{
	TRACE_FX();
	if (*lpFlags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s lpFlags value (0x%08x) must be 0 on Perpetual Socket 0x%08x."
			, __func__
			, *lpFlags
			, perpetual_socket
		);
		
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	if (dwBufferCount != 1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s dwBufferCount value (%u) must be 1 on Perpetual Socket 0x%08x."
			, __func__
			, dwBufferCount
			, perpetual_socket
		);
		
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	if (lpCompletionRoutine) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s lpCompletionRoutine must be NULL on Perpetual Socket 0x%08x."
			, __func__
			, perpetual_socket
		);
		
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	
	while (1) {
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		INT resultRecv = WSARecv(transitorySocket, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);
		int errorRecv = WSAGetLastError();
		
		if (resultRecv == SOCKET_ERROR && errorRecv != WSA_IO_PENDING && XSocketPerpetualSocketChangedError(errorRecv, perpetual_socket, transitorySocket)) {
			continue;
		}
		
		if (resultRecv != ERROR_SUCCESS && errorRecv != WSA_IO_PENDING) {
			XLLN_DEBUG_LOG_ECODE(errorRecv, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s recv error on socket (P,T) (0x%08x,0x%08x)."
				, __func__
				, perpetual_socket
				, transitorySocket
			);
		}
		
		WSASetLastError(errorRecv);
		return resultRecv;
	}
}

// #21
int WINAPI XWSARecvFrom(
	SOCKET perpetual_socket
	, WSABUF *lpBuffers
	, DWORD dwBufferCount
	, DWORD *lpNumberOfBytesRecvd
	, DWORD *lpFlags
	, struct sockaddr FAR *lpFrom
	, INT *lpFromlen
	, WSAOVERLAPPED *lpOverlapped
	, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
)
{
	TRACE_FX();
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return WSARecvFrom(perpetual_socket, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpFrom, lpFromlen, lpOverlapped, lpCompletionRoutine);
}

// #23
int WINAPI XWSASend(
	SOCKET perpetual_socket
	, WSABUF *lpBuffers
	, DWORD dwBufferCount
	, DWORD *lpNumberOfBytesSent
	, DWORD dwFlags
	, WSAOVERLAPPED *lpOverlapped
	, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
)
{
	TRACE_FX();
	if (dwFlags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s dwFlags value (0x%08x) must be 0 on Perpetual Socket 0x%08x."
			, __func__
			, dwFlags
			, perpetual_socket
		);
		
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	if (lpCompletionRoutine) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s lpCompletionRoutine must be NULL on Perpetual Socket 0x%08x."
			, __func__
			, perpetual_socket
		);
		
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	
	while (1) {
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		INT resultSend = WSASend(perpetual_socket, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
		int errorSend = WSAGetLastError();
		
		if (resultSend == SOCKET_ERROR && errorSend != WSA_IO_PENDING && XSocketPerpetualSocketChangedError(errorSend, perpetual_socket, transitorySocket)) {
			continue;
		}
		
		if (resultSend == SOCKET_ERROR && errorSend != WSA_IO_PENDING) {
			XLLN_DEBUG_LOG_ECODE(errorSend, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s send error on socket (P,T) (0x%08x,0x%08x)."
				, __func__
				, perpetual_socket
				, transitorySocket
			);
		}
		
		WSASetLastError(errorSend);
		return resultSend;
	}
}

// #25
int WINAPI XWSASendTo(
	SOCKET perpetual_socket
	, WSABUF *lpBuffers
	, DWORD dwBufferCount
	, DWORD *lpNumberOfBytesSent
	, DWORD dwFlags
	, const struct sockaddr FAR *lpTo
	, int iToLen
	, WSAOVERLAPPED *lpOverlapped
	, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
)
{
	TRACE_FX();
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return WSASendTo(perpetual_socket, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iToLen, lpOverlapped, lpCompletionRoutine);
}

// #28
VOID WINAPI XWSASetLastError(int iError)
{
	TRACE_FX();
	WSASetLastError(iError);
}

// #29
WSAEVENT WINAPI XWSACreateEvent()
{
	TRACE_FX();
	return WSACreateEvent();
	//return CreateEvent(NULL, TRUE, FALSE, NULL);
}

// #30
BOOL WINAPI XWSACloseEvent(WSAEVENT hEvent)
{
	TRACE_FX();
	return WSACloseEvent(hEvent);
}

// #31
BOOL WINAPI XWSASetEvent(WSAEVENT hEvent)
{
	TRACE_FX();
	return WSASetEvent(hEvent);
}

// #32
BOOL WINAPI XWSAResetEvent(WSAEVENT hEvent)
{
	TRACE_FX();
	return WSAResetEvent(hEvent);
}

// #33
DWORD WINAPI XWSAWaitForMultipleEvents(DWORD cEvents, const WSAEVENT FAR *lphEvents, BOOL fWaitAll, DWORD dwTimeout, BOOL fAlertable)
{
	TRACE_FX();
	return WSAWaitForMultipleEvents(cEvents, lphEvents, fWaitAll, dwTimeout, fAlertable);
}

// #34
INT WINAPI XWSAFDIsSet(SOCKET perpetual_socket, fd_set *set)
{
	TRACE_FX();
	
	while (1) {
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		INT resultFDIsSet = __WSAFDIsSet(transitorySocket, set);
		int errorFDIsSet = WSAGetLastError();
		
		if (resultFDIsSet == 0 && XSocketPerpetualSocketChangedError(errorFDIsSet, perpetual_socket, transitorySocket)) {
			continue;
		}
		
		if (resultFDIsSet == 0) {
			XLLN_DEBUG_LOG_ECODE(errorFDIsSet, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s FDIsSet error on socket (P,T) (0x%08x,0x%08x)."
				, __func__
				, perpetual_socket
				, transitorySocket
			);
		}
		
		WSASetLastError(errorFDIsSet);
		return resultFDIsSet;
	}
}

// #35
int WINAPI XWSAEventSelect(SOCKET perpetual_socket, WSAEVENT hEventObject, long lNetworkEvents)
{
	TRACE_FX();
	
	while (1) {
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		INT resultEventSelect = WSAEventSelect(transitorySocket, hEventObject, lNetworkEvents);
		int errorEventSelect = WSAGetLastError();
		
		if (resultEventSelect && XSocketPerpetualSocketChangedError(errorEventSelect, perpetual_socket, transitorySocket)) {
			continue;
		}
		
		if (resultEventSelect) {
			XLLN_DEBUG_LOG_ECODE(errorEventSelect, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s EventSelect error on socket (P,T) (0x%08x,0x%08x)."
				, __func__
				, perpetual_socket
				, transitorySocket
			);
		}
		
		WSASetLastError(errorEventSelect);
		return resultEventSelect;
	}
}
