#pragma once

#define XNOTIFYUI_POS_CENTER		0
#define XNOTIFYUI_POS_TOPCENTER		0b0001
#define XNOTIFYUI_POS_BOTTOMCENTER	0b0010
#define XNOTIFYUI_POS_CENTERLEFT	0b0100
#define XNOTIFYUI_POS_CENTERRIGHT	0b1000
#define XNOTIFYUI_POS_TOPLEFT		(XNOTIFYUI_POS_TOPCENTER | XNOTIFYUI_POS_CENTERLEFT)
#define XNOTIFYUI_POS_TOPRIGHT		(XNOTIFYUI_POS_TOPCENTER | XNOTIFYUI_POS_CENTERRIGHT)
#define XNOTIFYUI_POS_BOTTOMLEFT	(XNOTIFYUI_POS_BOTTOMCENTER | XNOTIFYUI_POS_CENTERLEFT)
#define XNOTIFYUI_POS_BOTTOMRIGHT	(XNOTIFYUI_POS_BOTTOMCENTER | XNOTIFYUI_POS_CENTERRIGHT)
#define XNOTIFYUI_POS_MASK			0b1111

extern CRITICAL_SECTION xlive_critsec_xnotify;
extern bool xlive_notify_system_ui_open;

void XLiveNotifyAddEvent(uint32_t notification_id, uint32_t notification_value);
bool XLiveNotifyDeleteListener(HANDLE notification_listener);
bool InitXNotify();
bool UninitXNotify();
