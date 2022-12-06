#pragma once

extern BOOL xlive_net_initialized;
extern XNetStartupParams xlive_net_startup_params;
extern XNADDR xlive_local_xnAddr;
extern CRITICAL_SECTION xlive_critsec_xnet_session_keys;

BOOL InitXNet();
BOOL UninitXNet();
INT WINAPI XNetStartup(const XNetStartupParams *pxnsp);
INT WINAPI XNetCreateKey(XNKID *pxnkid, XNKEY *pxnkey);
