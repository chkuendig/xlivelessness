#include <winsock2.h>
#include "xdefs.hpp"
#include "xwsa.hpp"
#include "xnet.hpp"
#include "xsocket.hpp"
#include "net-entity.hpp"
#include "packet-handler.hpp"
#include "../utils/util-socket.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/xlln.hpp"

typedef struct {
	uint8_t *dataBuffer = 0;
	uint32_t dataBufferSize = 0;
	uint32_t dataSize = 0;
} XWSA_BUFFER;

typedef struct WSA_OVERLAPPED_SOCKET_INFO_ {
	WSA_OVERLAPPED_SOCKET_INFO_() {
		InitializeCriticalSection(&mutexInProgress);
		memset(&sockAddrExternal, 0, sizeof(sockAddrExternal));
		memset(&SEND_RECV, 0, sizeof(SEND_RECV));
	}
	~WSA_OVERLAPPED_SOCKET_INFO_() {
		DeleteCriticalSection(&mutexInProgress);
		
		if (isRecv) {
			if (SEND_RECV.RECV.dataBufferInternal) {
				delete[] SEND_RECV.RECV.dataBufferInternal;
				SEND_RECV.RECV.dataBufferInternal = 0;
			}
			SEND_RECV.RECV.dataBufferInternalSize = 0;
		}
		else {
			for (uint32_t iBuf = 0; iBuf < SEND_RECV.SEND.dataBuffersInternalCount; iBuf++) {
				delete[] SEND_RECV.SEND.dataBuffersInternal[iBuf].dataBuffer;
				SEND_RECV.SEND.dataBuffersInternal[iBuf].dataBufferSize = 0;
			}
			delete[] SEND_RECV.SEND.dataBuffersInternal;
			SEND_RECV.SEND.dataBuffersInternalCount = 0;
		}
	}

	SOCKET perpetualSocket = INVALID_SOCKET;
	// If set, dataBufferXlive and wsaOverlapped may no longer be valid memory.
	bool hasCompleted = false;
	
	CRITICAL_SECTION mutexInProgress;
	
	// Note that this can potentially be null.
	WSAOVERLAPPED *wsaOverlapped = 0;
	
	union {
		struct {
			uint8_t *dataBufferInternal;
			uint32_t dataBufferInternalSize;
			uint8_t *dataBufferXlive;
			uint32_t dataBufferXliveSize;
			sockaddr *sockAddrXlive;
			int *sockAddrXliveSize;
		} RECV;
		struct {
			XWSA_BUFFER *dataBuffersInternal;
			uint32_t dataBuffersInternalCount;
			uint32_t dataBuffersCount;
		} SEND;
	} SEND_RECV;
	bool isRecv;
	
	SOCKADDR_STORAGE sockAddrExternal;
	// Set when the buffer has already been processed in a previous call to XWSAGetOverlappedResult.
	uint32_t dataBufferSizeResult = 0;
	uint32_t dataTransferred = 0;
	uint32_t socketFlagReturned = 0;
	
} WSA_OVERLAPPED_SOCKET_INFO;

CRITICAL_SECTION xlive_critsec_overlapped_sockets;
// Key: perpetualSocket.
static std::map<SOCKET, WSA_OVERLAPPED_SOCKET_INFO*> xlive_overlapped_sockets_recv;
// Key: perpetualSocket.
static std::map<SOCKET, WSA_OVERLAPPED_SOCKET_INFO*> xlive_overlapped_sockets_send;

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

static BOOL WINAPI XWSAGetOverlappedResultInternal(SOCKET perpetual_socket, WSAOVERLAPPED *lpOverlapped, DWORD *lpcbTransfer, BOOL fWait, DWORD *lpdwFlags)
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
		
		if (!resultGetOverlappedResult && !fWait && errorGetOverlappedResult != WSA_IO_INCOMPLETE) {
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

// #16
BOOL WINAPI XWSAGetOverlappedResult(SOCKET perpetual_socket, WSAOVERLAPPED *lpOverlapped, DWORD *lpcbTransfer, BOOL fWait, DWORD *lpdwFlags)
{
	TRACE_FX();
	
	bool foundSocket = false;
	bool foundOverlapped = false;
	BOOL resultGetOverlappedResult = FALSE;
	int errorGetOverlappedResult = WSAEINVAL;
	
	{
		EnterCriticalSection(&xlive_critsec_overlapped_sockets);
		
		bool reloop;
		bool isRecv = false;
		do {
			reloop = false;
			auto xlive_overlapped_sockets = (isRecv ? xlive_overlapped_sockets_recv : xlive_overlapped_sockets_send);
			if (xlive_overlapped_sockets.count(perpetual_socket)) {
				foundSocket = true;
				WSA_OVERLAPPED_SOCKET_INFO *wsaOverlappedInfo = xlive_overlapped_sockets[perpetual_socket];
				
				if (wsaOverlappedInfo->wsaOverlapped == lpOverlapped) {
					foundOverlapped = true;
					
					if (wsaOverlappedInfo->hasCompleted) {
						*lpcbTransfer = wsaOverlappedInfo->dataTransferred;
						*lpdwFlags = wsaOverlappedInfo->socketFlagReturned;
						
						if (isRecv) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
								, "%s Perpetual Socket 0x%08x data recv already read completed size 0x%x data for title size 0x%x."
								, __func__
								, perpetual_socket
								, wsaOverlappedInfo->dataBufferSizeResult
								, wsaOverlappedInfo->dataTransferred
							);
						}
						else {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
								, "%s Perpetual Socket 0x%08x data send already read completed data to send size 0x%x sent wrapped size 0x%x."
								, __func__
								, perpetual_socket
								, wsaOverlappedInfo->dataTransferred
								, wsaOverlappedInfo->dataBufferSizeResult
							);
						}
						
						LeaveCriticalSection(&xlive_critsec_overlapped_sockets);
					}
					else {
						
						{
							BOOL resultInProgress = TryEnterCriticalSection(&wsaOverlappedInfo->mutexInProgress);
							if (resultInProgress == FALSE) {
								XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
									, "%s Perpetual Socket 0x%08x failed to enter mutexInProgress critsec."
									, __func__
									, perpetual_socket
								);
								errorGetOverlappedResult = WSAEALREADY;
								resultGetOverlappedResult = FALSE;
								
								LeaveCriticalSection(&xlive_critsec_overlapped_sockets);
								break;
							}
						}
						
						LeaveCriticalSection(&xlive_critsec_overlapped_sockets);
						
						resultGetOverlappedResult = XWSAGetOverlappedResultInternal(
							perpetual_socket
							, lpOverlapped
							, (DWORD*)&wsaOverlappedInfo->dataBufferSizeResult
							, fWait
							, (DWORD*)&wsaOverlappedInfo->socketFlagReturned
						);
						errorGetOverlappedResult = WSAGetLastError();
						
						if (isRecv) {
							if (resultGetOverlappedResult) {
								INT resultDataRecvSizeFromHelper = XSocketRecvFromHelper(
									(int)wsaOverlappedInfo->dataBufferSizeResult
									, perpetual_socket
									, (char*)wsaOverlappedInfo->SEND_RECV.RECV.dataBufferInternal
									, wsaOverlappedInfo->SEND_RECV.RECV.dataBufferInternalSize
									, wsaOverlappedInfo->socketFlagReturned
									, &wsaOverlappedInfo->sockAddrExternal
									, sizeof(wsaOverlappedInfo->sockAddrExternal)
									, wsaOverlappedInfo->SEND_RECV.RECV.sockAddrXlive
									, wsaOverlappedInfo->SEND_RECV.RECV.sockAddrXliveSize
								);
								if (resultDataRecvSizeFromHelper < 0) {
									resultDataRecvSizeFromHelper = 0;
								}
								
								wsaOverlappedInfo->dataTransferred = resultDataRecvSizeFromHelper;
								
								*lpcbTransfer = wsaOverlappedInfo->dataTransferred;
								*lpdwFlags = wsaOverlappedInfo->socketFlagReturned;
								
								if (wsaOverlappedInfo->dataTransferred > wsaOverlappedInfo->SEND_RECV.RECV.dataBufferXliveSize) {
									memcpy(wsaOverlappedInfo->SEND_RECV.RECV.dataBufferXlive, wsaOverlappedInfo->SEND_RECV.RECV.dataBufferInternal, wsaOverlappedInfo->SEND_RECV.RECV.dataBufferXliveSize);
									
									wsaOverlappedInfo->hasCompleted = true;
									errorGetOverlappedResult = WSAEMSGSIZE;
									resultGetOverlappedResult = FALSE;
									
									XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
										, "%s Perpetual Socket 0x%08x data recv size (0x%08x) data for title size 0x%x too big for title buffer size 0x%x."
										, __func__
										, perpetual_socket
										, wsaOverlappedInfo->dataBufferSizeResult
										, wsaOverlappedInfo->dataTransferred
										, wsaOverlappedInfo->SEND_RECV.RECV.dataBufferXliveSize
									);
								}
								else {
									memcpy(wsaOverlappedInfo->SEND_RECV.RECV.dataBufferXlive, wsaOverlappedInfo->SEND_RECV.RECV.dataBufferInternal, wsaOverlappedInfo->dataTransferred);
									
									wsaOverlappedInfo->hasCompleted = true;
									
									XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
										, "%s Perpetual Socket 0x%08x data recv size 0x%x data for title size 0x%x."
										, __func__
										, perpetual_socket
										, wsaOverlappedInfo->dataBufferSizeResult
										, wsaOverlappedInfo->dataTransferred
									);
								}
							}
							// TODO if cancelled by rebinding sockets.
							//else if (!resultGetOverlappedResult && errorGetOverlappedResult == ERROR_OPERATION_ABORTED && wsaOverlappedInfo->hasCompleted) {
							//}
							else if (!resultGetOverlappedResult && errorGetOverlappedResult != WSA_IO_INCOMPLETE) {
								wsaOverlappedInfo->hasCompleted = true;
								
								XLLN_DEBUG_LOG_ECODE(errorGetOverlappedResult, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
									, "%s Perpetual Socket 0x%08x recv failed."
									, __func__
									, perpetual_socket
								);
							}
						}
						else {
							if (resultGetOverlappedResult) {
								*lpcbTransfer = wsaOverlappedInfo->dataTransferred;
								*lpdwFlags = wsaOverlappedInfo->socketFlagReturned;
								
								XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
									, "%s Perpetual Socket 0x%08x data to send size 0x%x sent wrapped size 0x%x."
									, __func__
									, perpetual_socket
									, wsaOverlappedInfo->dataTransferred
									, wsaOverlappedInfo->dataBufferSizeResult
								);
								
								wsaOverlappedInfo->hasCompleted = true;
							}
							else if (!resultGetOverlappedResult && errorGetOverlappedResult != WSA_IO_INCOMPLETE) {
								wsaOverlappedInfo->hasCompleted = true;
								
								XLLN_DEBUG_LOG_ECODE(errorGetOverlappedResult, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
									, "%s Perpetual Socket 0x%08x send failed."
									, __func__
									, perpetual_socket
								);
							}
						}
						
						LeaveCriticalSection(&wsaOverlappedInfo->mutexInProgress);
					}
					
					break;
				}
			}
		} while (reloop || (isRecv = !isRecv));
		
		if (!foundOverlapped) {
			LeaveCriticalSection(&xlive_critsec_overlapped_sockets);
		}
	}
	
	if (!foundSocket) {
		WSASetLastError(WSAENOTSOCK);
		return FALSE;
	}
	if (!foundOverlapped) {
		WSASetLastError(WSAECANCELLED);
		return FALSE;
	}
	
	WSASetLastError(errorGetOverlappedResult);
	return resultGetOverlappedResult;
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
	
	bool foundSocket = false;
	int errorCancelOverlapped = WSASYSCALLFAILURE;
	int resultCancelOverlapped = SOCKET_ERROR;
	
	{
		EnterCriticalSection(&xlive_critsec_overlapped_sockets);
		
		bool isRecv = false;
		do {
			auto xlive_overlapped_sockets = (isRecv ? xlive_overlapped_sockets_recv : xlive_overlapped_sockets_send);
			if (xlive_overlapped_sockets.count(perpetual_socket)) {
				foundSocket = true;
				WSA_OVERLAPPED_SOCKET_INFO *wsaOverlappedInfo = xlive_overlapped_sockets[perpetual_socket];
				
				if (wsaOverlappedInfo->wsaOverlapped && wsaOverlappedInfo->wsaOverlapped->hEvent && wsaOverlappedInfo->wsaOverlapped->hEvent != INVALID_HANDLE_VALUE) {
					SetEvent(wsaOverlappedInfo->wsaOverlapped->hEvent);
				}
				
				wsaOverlappedInfo->hasCompleted = true;
				
				// Rebind this socket so the kernel cancels / returns from any blocking calls.
				//XSocketRebindSockets_(&perpetual_socket, 1);
				
				BOOL resultCancelIO = CancelIoEx((HANDLE)transitorySocket, 0);
				int errorCancelIo = GetLastError();
				
				{
					EnterCriticalSection(&wsaOverlappedInfo->mutexInProgress);
					LeaveCriticalSection(&wsaOverlappedInfo->mutexInProgress);
				}
				
				errorCancelOverlapped = ERROR_SUCCESS;
				resultCancelOverlapped = ERROR_SUCCESS;
				
				break;
			}
		} while (isRecv = !isRecv);
		
		LeaveCriticalSection(&xlive_critsec_overlapped_sockets);
	}
	
	if (!foundSocket) {
		errorCancelOverlapped = WSAENOTSOCK;
		resultCancelOverlapped = SOCKET_ERROR;
	}
	
	WSASetLastError(errorCancelOverlapped);
	return resultCancelOverlapped;
}

void XWSADeleteOverlappedSocket_(SOCKET perpetual_socket, SOCKET transitory_socket)
{
	EnterCriticalSection(&xlive_critsec_overlapped_sockets);
	
	bool isRecv = false;
	do {
		auto xlive_overlapped_sockets = (isRecv ? xlive_overlapped_sockets_recv : xlive_overlapped_sockets_send);
		if (xlive_overlapped_sockets.count(perpetual_socket)) {
			WSA_OVERLAPPED_SOCKET_INFO *wsaOverlappedInfo = xlive_overlapped_sockets[perpetual_socket];
			
			if (wsaOverlappedInfo->wsaOverlapped && wsaOverlappedInfo->wsaOverlapped->hEvent && wsaOverlappedInfo->wsaOverlapped->hEvent != INVALID_HANDLE_VALUE) {
				SetEvent(wsaOverlappedInfo->wsaOverlapped->hEvent);
			}
			
			wsaOverlappedInfo->hasCompleted = true;
			
			BOOL resultCancelIO = CancelIoEx((HANDLE)transitory_socket, 0);
			int errorCancelIo = GetLastError();
			
			{
				EnterCriticalSection(&wsaOverlappedInfo->mutexInProgress);
				LeaveCriticalSection(&wsaOverlappedInfo->mutexInProgress);
			}
			
			xlive_overlapped_sockets.erase(perpetual_socket);
			delete wsaOverlappedInfo;
		}
	} while (isRecv = !isRecv);
	
	LeaveCriticalSection(&xlive_critsec_overlapped_sockets);
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
	if (!lpFlags || *lpFlags) {
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
	if (!lpFlags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s lpFlags must not be NULL on Perpetual Socket 0x%08x."
			, __func__
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
	
	WSA_OVERLAPPED_SOCKET_INFO *wsaOverlappedInfo = 0;
	{
		EnterCriticalSection(&xlive_critsec_overlapped_sockets);
		
		if (xlive_overlapped_sockets_recv.count(perpetual_socket)) {
			wsaOverlappedInfo = xlive_overlapped_sockets_recv[perpetual_socket];
		}
		else {
			wsaOverlappedInfo = new WSA_OVERLAPPED_SOCKET_INFO;
			wsaOverlappedInfo->perpetualSocket = perpetual_socket;
			wsaOverlappedInfo->isRecv = true;
			xlive_overlapped_sockets_recv[perpetual_socket] = wsaOverlappedInfo;
			
			wsaOverlappedInfo->SEND_RECV.RECV.dataBufferInternalSize = 1024;
			wsaOverlappedInfo->SEND_RECV.RECV.dataBufferInternal = new uint8_t[wsaOverlappedInfo->SEND_RECV.RECV.dataBufferInternalSize];
			wsaOverlappedInfo->hasCompleted = true;
		}
		
		if (!wsaOverlappedInfo->hasCompleted) {
			LeaveCriticalSection(&xlive_critsec_overlapped_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s an existing Overlapped Recv operation is already in progress on Perpetual Socket 0x%08x."
				, __func__
				, perpetual_socket
			);
			
			WSASetLastError(WSAEINPROGRESS);
			return SOCKET_ERROR;
		}
		
		wsaOverlappedInfo->hasCompleted = false;
		wsaOverlappedInfo->wsaOverlapped = lpOverlapped;
		wsaOverlappedInfo->dataBufferSizeResult = 0;
		wsaOverlappedInfo->dataTransferred = 0;
		wsaOverlappedInfo->socketFlagReturned = 0;
		wsaOverlappedInfo->SEND_RECV.RECV.dataBufferXliveSize = lpBuffers[0].len;
		wsaOverlappedInfo->SEND_RECV.RECV.dataBufferXlive = (uint8_t*)lpBuffers[0].buf;
		wsaOverlappedInfo->SEND_RECV.RECV.sockAddrXlive = lpFrom;
		wsaOverlappedInfo->SEND_RECV.RECV.sockAddrXliveSize = lpFromlen;
		
		LeaveCriticalSection(&xlive_critsec_overlapped_sockets);
	}
	
	if (wsaOverlappedInfo->SEND_RECV.RECV.dataBufferInternalSize < wsaOverlappedInfo->SEND_RECV.RECV.dataBufferXliveSize) {
		delete[] wsaOverlappedInfo->SEND_RECV.RECV.dataBufferInternal;
		wsaOverlappedInfo->SEND_RECV.RECV.dataBufferInternalSize = wsaOverlappedInfo->SEND_RECV.RECV.dataBufferXliveSize;
		wsaOverlappedInfo->SEND_RECV.RECV.dataBufferInternal = new uint8_t[wsaOverlappedInfo->SEND_RECV.RECV.dataBufferInternalSize];
	}
	
	while (1) {
		bool connectionlessSocket = false;
		WSABUF bufferInternal;
		bufferInternal.buf = (CHAR*)wsaOverlappedInfo->SEND_RECV.RECV.dataBufferInternal;
		bufferInternal.len = wsaOverlappedInfo->SEND_RECV.RECV.dataBufferInternalSize;
		
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket, &connectionlessSocket);
		if (transitorySocket == INVALID_SOCKET) {
			wsaOverlappedInfo->hasCompleted = true;
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		int sockAddrExternalSize = sizeof(wsaOverlappedInfo->sockAddrExternal);
		INT resultRecvFrom = WSARecvFrom(
			transitorySocket
			, connectionlessSocket ? &bufferInternal : lpBuffers
			, dwBufferCount
			, (DWORD*)&(wsaOverlappedInfo->dataBufferSizeResult)
			, lpFlags
			, connectionlessSocket ? (sockaddr*)&wsaOverlappedInfo->sockAddrExternal : lpFrom
			, connectionlessSocket ? &sockAddrExternalSize : lpFromlen
			, lpOverlapped
			, lpCompletionRoutine
		);
		int errorRecvFrom = WSAGetLastError();
		
		if (resultRecvFrom == SOCKET_ERROR && errorRecvFrom != WSA_IO_PENDING && XSocketPerpetualSocketChangedError(errorRecvFrom, perpetual_socket, transitorySocket)) {
			continue;
		}
		
		if (resultRecvFrom != ERROR_SUCCESS && errorRecvFrom != WSA_IO_PENDING) {
			XLLN_DEBUG_LOG_ECODE(errorRecvFrom, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s recvfrom error on socket (P,T) (0x%08x,0x%08x)."
				, __func__
				, perpetual_socket
				, transitorySocket
			);
			
			wsaOverlappedInfo->hasCompleted = true;
			WSASetLastError(errorRecvFrom);
			return resultRecvFrom;
		}
		
		if (resultRecvFrom == ERROR_SUCCESS && wsaOverlappedInfo->dataBufferSizeResult == 0) {
			if (lpNumberOfBytesRecvd) {
				*lpNumberOfBytesRecvd = wsaOverlappedInfo->dataTransferred;
			}
			
			wsaOverlappedInfo->hasCompleted = true;
			WSASetLastError(errorRecvFrom);
			return resultRecvFrom;
		}
		
		if (!connectionlessSocket) {
			
			if (resultRecvFrom == ERROR_SUCCESS) {
				if (lpNumberOfBytesRecvd) {
					*lpNumberOfBytesRecvd = wsaOverlappedInfo->dataBufferSizeResult;
				}
				
				wsaOverlappedInfo->hasCompleted = true;
			}
			
			WSASetLastError(errorRecvFrom);
			return resultRecvFrom;
		}
		
		if (resultRecvFrom == ERROR_SUCCESS) {
			INT resultDataRecvSizeFromHelper = XSocketRecvFromHelper(
				(int)wsaOverlappedInfo->dataBufferSizeResult
				, perpetual_socket
				, (char*)wsaOverlappedInfo->SEND_RECV.RECV.dataBufferInternal
				, wsaOverlappedInfo->SEND_RECV.RECV.dataBufferInternalSize
				, *lpFlags
				, &wsaOverlappedInfo->sockAddrExternal
				, sockAddrExternalSize
				, lpFrom
				, lpFromlen
			);
			if (resultDataRecvSizeFromHelper <= 0) {
				continue;
			}
			
			wsaOverlappedInfo->dataTransferred = resultDataRecvSizeFromHelper;
			
			if (lpNumberOfBytesRecvd) {
				*lpNumberOfBytesRecvd = wsaOverlappedInfo->dataTransferred;
			}
			
			if (wsaOverlappedInfo->dataTransferred > wsaOverlappedInfo->SEND_RECV.RECV.dataBufferXliveSize) {
				memcpy(wsaOverlappedInfo->SEND_RECV.RECV.dataBufferXlive, wsaOverlappedInfo->SEND_RECV.RECV.dataBufferInternal, wsaOverlappedInfo->SEND_RECV.RECV.dataBufferXliveSize);
				
				wsaOverlappedInfo->hasCompleted = true;
				WSASetLastError(WSAEMSGSIZE);
				return SOCKET_ERROR;
			}
			
			memcpy(wsaOverlappedInfo->SEND_RECV.RECV.dataBufferXlive, wsaOverlappedInfo->SEND_RECV.RECV.dataBufferInternal, wsaOverlappedInfo->dataTransferred);
			
			wsaOverlappedInfo->hasCompleted = true;
		}
		
		WSASetLastError(errorRecvFrom);
		return resultRecvFrom;
	}
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
		
		INT resultSend = WSASend(
			transitorySocket
			, lpBuffers
			, dwBufferCount
			, lpNumberOfBytesSent
			, dwFlags
			, lpOverlapped
			, lpCompletionRoutine
		);
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
	// each buffer is essentially its own message afaik. so wrap each one.
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
	
	WSA_OVERLAPPED_SOCKET_INFO *wsaOverlappedInfo = 0;
	{
		EnterCriticalSection(&xlive_critsec_overlapped_sockets);
		
		if (xlive_overlapped_sockets_send.count(perpetual_socket)) {
			wsaOverlappedInfo = xlive_overlapped_sockets_send[perpetual_socket];
		}
		else {
			wsaOverlappedInfo = new WSA_OVERLAPPED_SOCKET_INFO;
			wsaOverlappedInfo->perpetualSocket = perpetual_socket;
			wsaOverlappedInfo->isRecv = false;
			xlive_overlapped_sockets_send[perpetual_socket] = wsaOverlappedInfo;
			
			wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternalCount = 1;
			wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal = new XWSA_BUFFER[wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternalCount];
			for (uint32_t iBuf = 0; iBuf < wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternalCount; iBuf++) {
				wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataBufferSize = 1024;
				wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataBuffer = new uint8_t[wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataBufferSize];
			}
			wsaOverlappedInfo->hasCompleted = true;
		}
		
		if (!wsaOverlappedInfo->hasCompleted) {
			LeaveCriticalSection(&xlive_critsec_overlapped_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s an existing Overlapped Send operation is already in progress on Perpetual Socket 0x%08x."
				, __func__
				, perpetual_socket
			);
			
			WSASetLastError(WSAEINPROGRESS);
			return SOCKET_ERROR;
		}
		
		wsaOverlappedInfo->hasCompleted = false;
		wsaOverlappedInfo->wsaOverlapped = lpOverlapped;
		wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersCount = dwBufferCount;
		wsaOverlappedInfo->dataBufferSizeResult = 0;
		wsaOverlappedInfo->dataTransferred = 0;
		wsaOverlappedInfo->socketFlagReturned = 0;
		
		LeaveCriticalSection(&xlive_critsec_overlapped_sockets);
	}
	
	for (uint32_t iBuf = 0; iBuf < wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersCount; iBuf++) {
		wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataSize = 0;
	}
	
	wsaOverlappedInfo->dataTransferred = 0;
	for (uint32_t iBuf = 0; iBuf < wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersCount; iBuf++) {
		wsaOverlappedInfo->dataTransferred += lpBuffers[iBuf].len;
	}
	
	while (1) {
		bool connectionlessSocket = false;
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket, &connectionlessSocket);
		if (transitorySocket == INVALID_SOCKET) {
			wsaOverlappedInfo->hasCompleted = true;
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		if (connectionlessSocket && lpTo && iToLen) {
			break;
		}
		
		INT resultSendTo = WSASendTo(
			transitorySocket
			, lpBuffers
			, dwBufferCount
			, (DWORD*)&(wsaOverlappedInfo->dataBufferSizeResult)
			, dwFlags
			, lpTo
			, iToLen
			, lpOverlapped
			, lpCompletionRoutine
		);
		int errorSendTo = WSAGetLastError();
		
		if (resultSendTo == SOCKET_ERROR && errorSendTo != WSA_IO_PENDING && XSocketPerpetualSocketChangedError(errorSendTo, perpetual_socket, transitorySocket)) {
			continue;
		}
		
		if (resultSendTo != ERROR_SUCCESS && errorSendTo != WSA_IO_PENDING) {
			XLLN_DEBUG_LOG_ECODE(errorSendTo, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s sendto error on socket (P,T) (0x%08x,0x%08x)."
				, __func__
				, perpetual_socket
				, transitorySocket
			);
			
			wsaOverlappedInfo->hasCompleted = true;
			WSASetLastError(errorSendTo);
			return resultSendTo;
		}
		
		if (resultSendTo == ERROR_SUCCESS) {
			wsaOverlappedInfo->hasCompleted = true;
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s socket (P,T) (0x%08x,0x%08x) data to send size 0x%x sent wrapped size 0x%x."
				, __func__
				, perpetual_socket
				, transitorySocket
				, wsaOverlappedInfo->dataTransferred
				, wsaOverlappedInfo->dataBufferSizeResult
			);
		}
		
		WSASetLastError(errorSendTo);
		return resultSendTo;
	}
	
	// Connectionless socket with destination from here on. Packet(s) must be wrapped.
	
	if (wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternalCount < wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersCount) {
		XWSA_BUFFER *dataBuffersInternalOld = wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal;
		wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal = new XWSA_BUFFER[wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersCount];
		
		for (uint32_t iBuf = 0; iBuf < wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersCount; iBuf++) {
			if (iBuf < wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternalCount) {
				wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataBufferSize = dataBuffersInternalOld[iBuf].dataBufferSize;
				wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataBuffer = dataBuffersInternalOld[iBuf].dataBuffer;
			}
			else {
				wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataBufferSize = 1024;
				wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataBuffer = new uint8_t[wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataBufferSize];
			}
		}
		
		wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternalCount = wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersCount;
		delete[] dataBuffersInternalOld;
	}
	
	const int packetSizeHeaderType = sizeof(XLLNNetPacketType::TYPE);
	
	SOCKADDR_STORAGE sockAddrExternal;
	int sockAddrExternalLen = sizeof(sockAddrExternal);
	memcpy(&sockAddrExternal, lpTo, sockAddrExternalLen < iToLen ? sockAddrExternalLen : iToLen);
	
	const uint32_t ipv4NBO = ((struct sockaddr_in*)&sockAddrExternal)->sin_addr.s_addr;
	// This address may (hopefully) be an instanceId.
	const uint32_t ipv4HBO = ntohl(ipv4NBO);
	const uint16_t portHBO = GetSockAddrPort(&sockAddrExternal);
	
	bool isBroadcast = (((SOCKADDR_STORAGE*)lpTo)->ss_family == AF_INET && (ipv4HBO == INADDR_BROADCAST || ipv4HBO == INADDR_ANY));
	
	for (uint32_t iBuf = 0; iBuf < wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersCount; iBuf++) {
		uint32_t bufferSizeRequired = packetSizeHeaderType + lpBuffers[iBuf].len;
		if (bufferSizeRequired > wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataBufferSize) {
			delete[] wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataBuffer;
			wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataBufferSize = bufferSizeRequired;
			wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataBuffer = new uint8_t[wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataBufferSize];
		}
		
		uint8_t *newPacketBufferTitleData = wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataBuffer;
		int newPacketBufferTitleDataAlreadyProcessedOffset = 0;
		
		XLLNNetPacketType::TYPE &newPacketTitleDataType = *(XLLNNetPacketType::TYPE*)&newPacketBufferTitleData[newPacketBufferTitleDataAlreadyProcessedOffset];
		newPacketBufferTitleDataAlreadyProcessedOffset += packetSizeHeaderType;
		newPacketTitleDataType = isBroadcast ? XLLNNetPacketType::tTITLE_BROADCAST_PACKET : XLLNNetPacketType::tTITLE_PACKET;
		
		memcpy(&newPacketBufferTitleData[newPacketBufferTitleDataAlreadyProcessedOffset], lpBuffers[iBuf].buf, lpBuffers[iBuf].len);
		newPacketBufferTitleDataAlreadyProcessedOffset += lpBuffers[iBuf].len;
		
		wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataSize = newPacketBufferTitleDataAlreadyProcessedOffset;
	}
	
	if (isBroadcast) {
		for (uint32_t iBuf = 0; iBuf < wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersCount; iBuf++) {
			INT resultSend = SOCKET_ERROR;
			int errorSend = WSAEINVAL;
			if (
				XllnSocketBroadcastTo(
					&resultSend
					, perpetual_socket
					, (char*)wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataBuffer
					, wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataSize
					, dwFlags
					, lpTo
					, iToLen
				)
			) {
				if (resultSend != ERROR_SUCCESS) {
					errorSend = WSAGetLastError();
					
					XLLN_DEBUG_LOG_ECODE(errorSend, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s XllnSocketBroadcastTo error on Perpetual Socket 0x%08x with buffer index 0x%x of data size 0x%x."
						, __func__
						, perpetual_socket
						, iBuf
						, wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataSize
					);
				}
			}
			else {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Perpetual Socket 0x%08x unexpected failed to broadcast buffer index 0x%x of data size 0x%x."
					, __func__
					, perpetual_socket
					, iBuf
					, wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataSize
				);
			}
		}
		
		if (wsaOverlappedInfo->wsaOverlapped) {
			wsaOverlappedInfo->wsaOverlapped->Internal = ERROR_SUCCESS;
			wsaOverlappedInfo->wsaOverlapped->InternalHigh = ERROR_SUCCESS;
			wsaOverlappedInfo->wsaOverlapped->Offset = ERROR_SUCCESS;
			wsaOverlappedInfo->wsaOverlapped->OffsetHigh = ERROR_SUCCESS;
			wsaOverlappedInfo->hasCompleted = true;
			if (wsaOverlappedInfo->wsaOverlapped->hEvent && wsaOverlappedInfo->wsaOverlapped->hEvent != INVALID_HANDLE_VALUE) {
				SetEvent(wsaOverlappedInfo->wsaOverlapped->hEvent);
			}
			WSASetLastError(WSA_IO_PENDING);
			return SOCKET_ERROR;
		}
		
		*lpNumberOfBytesSent = wsaOverlappedInfo->dataTransferred;
		
		wsaOverlappedInfo->hasCompleted = true;
		WSASetLastError(ERROR_SUCCESS);
		return ERROR_SUCCESS;
	}
	
	uint32_t resultNetter = NetterEntityGetAddrByInstanceIdPort(&sockAddrExternal, ipv4HBO, portHBO);
	if (resultNetter) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s Perpetual Socket (0x%08x) NetterEntityGetAddrByInstanceIdPort failed to find address 0x%08x:%hu with error 0x%08x."
			, __func__
			, perpetual_socket
			, ipv4HBO
			, portHBO
			, resultNetter
		);
		
		if (resultNetter == ERROR_PORT_NOT_SET) {
			if (sockAddrExternal.ss_family == AF_INET || sockAddrExternal.ss_family == AF_INET6) {
				SendUnknownUserPacket(
					perpetual_socket
					, 0
					, 0
					, XLLNNetPacketType::tUNKNOWN
					, true
					, &sockAddrExternal
					, false
					, 0
					, ipv4HBO
				);
			}
		}
		
		if (wsaOverlappedInfo->wsaOverlapped) {
			wsaOverlappedInfo->wsaOverlapped->Internal = ERROR_SUCCESS;
			wsaOverlappedInfo->wsaOverlapped->InternalHigh = ERROR_SUCCESS;
			wsaOverlappedInfo->wsaOverlapped->Offset = ERROR_SUCCESS;
			wsaOverlappedInfo->wsaOverlapped->OffsetHigh = ERROR_SUCCESS;
			wsaOverlappedInfo->hasCompleted = true;
			if (wsaOverlappedInfo->wsaOverlapped->hEvent && wsaOverlappedInfo->wsaOverlapped->hEvent != INVALID_HANDLE_VALUE) {
				SetEvent(wsaOverlappedInfo->wsaOverlapped->hEvent);
			}
			WSASetLastError(WSA_IO_PENDING);
			return SOCKET_ERROR;
		}
		
		*lpNumberOfBytesSent = wsaOverlappedInfo->dataTransferred;
		
		wsaOverlappedInfo->hasCompleted = true;
		WSASetLastError(ERROR_SUCCESS);
		return ERROR_SUCCESS;
	}
	
	{
		char *sockAddrInfo = GET_SOCKADDR_INFO(&sockAddrExternal);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "%s Perpetual Socket (0x%08x) NetterEntityGetAddrByInstanceIdPort found address/instanceId 0x%08x:%hu as %s."
			, __func__
			, perpetual_socket
			, ipv4HBO
			, portHBO
			, sockAddrInfo ? sockAddrInfo : ""
		);
		if (sockAddrInfo) {
			free(sockAddrInfo);
		}
	}
	
	while (1) {
		SOCKET transitorySocket = XSocketGetTransitorySocket(perpetual_socket);
		if (transitorySocket == INVALID_SOCKET) {
			wsaOverlappedInfo->hasCompleted = true;
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		WSABUF *wsaBufTemp = new WSABUF[wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersCount];
		for (uint32_t iBuf = 0; iBuf < wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersCount; iBuf++) {
			wsaBufTemp[iBuf].buf = (char*)wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataBuffer;
			wsaBufTemp[iBuf].len = wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersInternal[iBuf].dataSize;
		}
		
		INT resultSendTo = WSASendTo(
			transitorySocket
			, wsaBufTemp
			, wsaOverlappedInfo->SEND_RECV.SEND.dataBuffersCount
			, (DWORD*)&(wsaOverlappedInfo->dataBufferSizeResult)
			, dwFlags
			, (const sockaddr*)&sockAddrExternal
			, sockAddrExternalLen
			, lpOverlapped
			, lpCompletionRoutine
		);
		int errorSendTo = WSAGetLastError();
		
		delete[] wsaBufTemp;
		
		if (resultSendTo == SOCKET_ERROR && errorSendTo != WSA_IO_PENDING && XSocketPerpetualSocketChangedError(errorSendTo, perpetual_socket, transitorySocket)) {
			continue;
		}
		
		if (resultSendTo != ERROR_SUCCESS && errorSendTo != WSA_IO_PENDING) {
			XLLN_DEBUG_LOG_ECODE(errorSendTo, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s sendto error on socket (P,T) (0x%08x,0x%08x)."
				, __func__
				, perpetual_socket
				, transitorySocket
			);
			
			wsaOverlappedInfo->hasCompleted = true;
			WSASetLastError(errorSendTo);
			return resultSendTo;
		}
		
		if (resultSendTo == ERROR_SUCCESS) {
			wsaOverlappedInfo->hasCompleted = true;
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s socket (P,T) (0x%08x,0x%08x) data to send size 0x%x sent wrapped size 0x%x."
				, __func__
				, perpetual_socket
				, transitorySocket
				, wsaOverlappedInfo->dataTransferred
				, wsaOverlappedInfo->dataBufferSizeResult
			);
		}
		
		*lpNumberOfBytesSent = wsaOverlappedInfo->dataTransferred;
		
		WSASetLastError(errorSendTo);
		return resultSendTo;
	}
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
