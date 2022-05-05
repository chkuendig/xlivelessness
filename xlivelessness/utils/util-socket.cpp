#include <winsock2.h>
#include "windows.h"
#include "util-socket.hpp"
#include "utils.hpp"
#include <WS2tcpip.h>

uint16_t GetSockAddrPort(const SOCKADDR_STORAGE *sockAddrStorage)
{
	uint16_t portHBO =
		sockAddrStorage->ss_family == AF_INET
		? ntohs((*(sockaddr_in*)sockAddrStorage).sin_port)
		: (
			sockAddrStorage->ss_family == AF_INET6
			? ntohs((*(sockaddr_in6*)sockAddrStorage).sin6_port)
			: 0
			);
	return portHBO;
}

bool SetSockAddrPort(SOCKADDR_STORAGE *sockAddrStorage, uint16_t portHBO)
{
	switch (sockAddrStorage->ss_family) {
	case AF_INET:
		(*(sockaddr_in*)sockAddrStorage).sin_port = htons(portHBO);
		break;
	case AF_INET6:
		(*(sockaddr_in6*)sockAddrStorage).sin6_port = htons(portHBO);
		break;
	default:
		return false;
	}
	return true;
}

/// Malloc'd result.
char* GetSockAddrInfo(const SOCKADDR_STORAGE *sockAddrStorage)
{
	// Maximum length of full domain name + sentinel.
	char namebuf[253 + 1];
	int error = getnameinfo((sockaddr*)sockAddrStorage, sizeof(SOCKADDR_STORAGE), namebuf, sizeof(namebuf), NULL, 0, NI_NUMERICHOST);
	if (!error) {
		uint16_t portHBO = GetSockAddrPort(sockAddrStorage);
		return portHBO
			? (
				sockAddrStorage->ss_family == AF_INET
				? FormMallocString("%s:%hu", namebuf, portHBO)
				: FormMallocString("[%s]:%hu", namebuf, portHBO)
			)
			: FormMallocString("%s", namebuf);
	}
	return 0;
}

bool SockAddrsMatch(const SOCKADDR_STORAGE *sockAddr1, const SOCKADDR_STORAGE *sockAddr2)
{
	if (sockAddr1->ss_family != sockAddr2->ss_family) {
		return false;
	}

	switch (sockAddr1->ss_family) {
	case AF_INET:
		if (
			((sockaddr_in*)sockAddr1)->sin_port != ((sockaddr_in*)sockAddr2)->sin_port
			|| ((sockaddr_in*)sockAddr1)->sin_addr.s_addr != ((sockaddr_in*)sockAddr2)->sin_addr.s_addr
			) {
			return false;
		}
		break;
	case AF_INET6:
		if (
			((sockaddr_in6*)sockAddr1)->sin6_port != ((sockaddr_in6*)sockAddr2)->sin6_port
			|| ((sockaddr_in6*)sockAddr1)->sin6_flowinfo != ((sockaddr_in6*)sockAddr2)->sin6_flowinfo
			|| ((sockaddr_in6*)sockAddr1)->sin6_scope_id != ((sockaddr_in6*)sockAddr2)->sin6_scope_id
			) {
			return false;
		}
		if (memcmp(((sockaddr_in6*)sockAddr1)->sin6_addr.s6_addr, ((sockaddr_in6*)sockAddr2)->sin6_addr.s6_addr, sizeof(IN6_ADDR)) != 0) {
			return false;
		}
		break;
	default:
		if (memcmp(sockAddr1, sockAddr2, sizeof(SOCKADDR_STORAGE)) != 0) {
			return false;
		}
		break;
	}

	return true;
}

DWORD IPHLPAPI_ConvertLengthToIpv4Mask(uint32_t mask_length, uint32_t *mask)
{
	if (!mask) {
		return ERROR_INVALID_PARAMETER;
	}
	if (mask_length > 32) {
		return ERROR_INVALID_PARAMETER;
	}
	
	*mask = (1 << (mask_length - 1)) | ((1 << (mask_length - 1)) - 1);
	
	return NO_ERROR;
}

int WS2_32_inet_pton(int af, const char *src, void *dst)
{
	struct sockaddr_storage ss;
	int size = sizeof(ss);
	char src_copy[INET6_ADDRSTRLEN+1];
	
	ZeroMemory(&ss, sizeof(ss));
	// Stupid non-const API.
	strncpy_s(src_copy, src, INET6_ADDRSTRLEN+1);
	src_copy[INET6_ADDRSTRLEN] = 0;
	
	if (WSAStringToAddressA(src_copy, af, NULL, (struct sockaddr *)&ss, &size) == 0) {
		switch(af) {
			case AF_INET:
				*(struct in_addr *)dst = ((struct sockaddr_in *)&ss)->sin_addr;
				return 1;
			case AF_INET6:
				*(struct in6_addr *)dst = ((struct sockaddr_in6 *)&ss)->sin6_addr;
				return 1;
		}
	}
	return 0;
}

const char* WS2_32_inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
	struct sockaddr_storage ss;
	unsigned long s = size;
	
	ZeroMemory(&ss, sizeof(ss));
	ss.ss_family = af;
	
	switch(af) {
		case AF_INET:
			((struct sockaddr_in *)&ss)->sin_addr = *(struct in_addr *)src;
			break;
		case AF_INET6:
			((struct sockaddr_in6 *)&ss)->sin6_addr = *(struct in6_addr *)src;
			break;
		default:
			return NULL;
	}
	
	// Cannot directly use &size because of strict aliasing rules.
	return (WSAAddressToStringA((struct sockaddr *)&ss, sizeof(ss), NULL, dst, &s) == 0) ? dst : NULL;
}
