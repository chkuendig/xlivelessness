#include "xdefs.hpp"
#include "xrender.hpp"
#include "../xlln/debug-text.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/wnd-main.hpp"
#include "xnotify.hpp"
#include <chrono>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <d3d11.h>
#include <d3d10.h>
#include <d3d9.h>

BOOL Initialised_XRender = FALSE;

CRITICAL_SECTION xlive_critsec_fps_limit;
std::atomic<DWORD> xlive_fps_limit = 60;

static std::chrono::system_clock::time_point nextFrame;
static std::chrono::system_clock::duration desiredRenderTime;

static IDirect3DDevice9 *device_d3d9 = 0;
static IDirect3DDevice9Ex *device_d3d9ex = 0;
static ID3D10Device *device_d3d10 = 0;
static ID3D11Device *device_d3d11 = 0;

static D3DPRESENT_PARAMETERS *presentation_parameters_d3d9 = 0;
static DXGI_SWAP_CHAIN_DESC *presentation_parameters_d3d10_d3d11 = 0;

static CRITICAL_SECTION xlive_critsec_hotkeys;
static std::condition_variable notify_cond_thread_hotkeys;
static std::thread thread_hotkeys;
static std::atomic<bool> exit_thread_hotkeys = TRUE;
static uint32_t thread_id_gui = 0;

CRITICAL_SECTION xlln_critsec_guide_ui_handlers;
std::vector<GUIDE_UI_HANDLER_INFO> xlln_guide_ui_handlers;
uint16_t xlive_hotkey_id_guide = VK_HOME;

static void hotkeyFuncGuide()
{
	EnterCriticalSection(&xlln_critsec_guide_ui_handlers);
	bool consumed = false;
	for (auto itrGuideUiHandlerInfo = xlln_guide_ui_handlers.begin(); itrGuideUiHandlerInfo != xlln_guide_ui_handlers.end(); itrGuideUiHandlerInfo++) {
		consumed = itrGuideUiHandlerInfo->guideUiHandler();
		if (consumed) {
			break;
		}
	}
	LeaveCriticalSection(&xlln_critsec_guide_ui_handlers);
	if (!consumed) {
		ShowXLLN(XLLN_SHOW_HOME, thread_id_gui);
	}
}

static const int hotkeyLen = 1;
static int hotkeyListenLen = 1;
static uint16_t* hotkeyId[hotkeyLen] = { &xlive_hotkey_id_guide };
static bool hotkeyPressed[hotkeyLen] = { false };
static void(*hotkeyFunc[hotkeyLen])(void) = { hotkeyFuncGuide };

static void ThreadHotkeys()
{
	std::mutex mutexPause;
	while (1) {
		HWND hWnd = 0;
		
		EnterCriticalSection(&xlive_critsec_hotkeys);
		
		if (presentation_parameters_d3d9 && presentation_parameters_d3d9->hDeviceWindow) {
			hWnd = presentation_parameters_d3d9->hDeviceWindow;
		}
		if (presentation_parameters_d3d10_d3d11 && presentation_parameters_d3d10_d3d11->OutputWindow) {
			hWnd = presentation_parameters_d3d10_d3d11->OutputWindow;
		}
		
		if (GetFocus() == hWnd || GetForegroundWindow() == hWnd) {
			if (xlive_notify_system_ui_open) {
				XLiveNotifyAddEvent(XN_SYS_UI, 0);
			}
			
			for (int i = 0; i < hotkeyListenLen; i++) {
				if (!*hotkeyId[i]) {
					continue;
				}
				//& 0x8000 is pressed
				//& 0x1 Key just transitioned from released to pressed.
				if (GetAsyncKeyState(*hotkeyId[i]) & 0x8000) {
					hotkeyPressed[i] = true;
				}
				else if (!(GetAsyncKeyState(*hotkeyId[i]) & 0x8000) && hotkeyPressed[i]) {
					hotkeyPressed[i] = false;
					hotkeyFunc[i]();
				}
			}
		}

		LeaveCriticalSection(&xlive_critsec_hotkeys);

		std::unique_lock<std::mutex> lock(mutexPause);
		notify_cond_thread_hotkeys.wait_for(lock, std::chrono::milliseconds(hWnd ? 10 : 1000), []() { return exit_thread_hotkeys == TRUE; });
		if (exit_thread_hotkeys) {
			break;
		}
	}
}

static void StartThreadHotkeys()
{
	StopThreadHotkeys();
	exit_thread_hotkeys = FALSE;
	thread_hotkeys = std::thread(ThreadHotkeys);
}

void StopThreadHotkeys()
{
	if (exit_thread_hotkeys == FALSE) {
		exit_thread_hotkeys = TRUE;
		notify_cond_thread_hotkeys.notify_all();
		thread_hotkeys.join();
	}
}

static void ReleaseD3DDevice()
{
	if (device_d3d9) {
		device_d3d9->Release();
		device_d3d9 = 0;
	}
	if (device_d3d9ex) {
		device_d3d9ex->Release();
		device_d3d9ex = 0;
	}
	if (device_d3d10) {
		device_d3d10->Release();
		device_d3d10 = 0;
	}
	if (device_d3d11) {
		device_d3d11->Release();
		device_d3d11 = 0;
	}
}

static void DeleteD3DPresentationParameters()
{
	if (presentation_parameters_d3d9) {
		delete presentation_parameters_d3d9;
		presentation_parameters_d3d9 = 0;
	}
	if (presentation_parameters_d3d10_d3d11) {
		delete presentation_parameters_d3d10_d3d11;
		presentation_parameters_d3d10_d3d11 = 0;
	}
}

static HRESULT UpdatePresentationParameters(void *pD3DPP)
{
	if (!pD3DPP) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pD3DPP is NULL.", __func__);
		return E_POINTER;
	}
	if (!device_d3d9 && !device_d3d9ex && !device_d3d10 && !device_d3d11) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s D3D Device not initialised.", __func__);
		return E_INVALIDARG;
	}
	if (((device_d3d9 || device_d3d9ex) && !((D3DPRESENT_PARAMETERS*)pD3DPP)->hDeviceWindow)
		|| ((device_d3d10 || device_d3d11) && !((DXGI_SWAP_CHAIN_DESC*)pD3DPP)->OutputWindow)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s D3D Device Window is NULL.", __func__);
		return E_INVALIDARG;
	}

	DeleteD3DPresentationParameters();

	if (device_d3d9 || device_d3d9ex) {
		presentation_parameters_d3d9 = new D3DPRESENT_PARAMETERS;
		memcpy(presentation_parameters_d3d9, pD3DPP, sizeof(D3DPRESENT_PARAMETERS));
	}
	else {
		presentation_parameters_d3d10_d3d11 = new DXGI_SWAP_CHAIN_DESC;
		memcpy(presentation_parameters_d3d10_d3d11, pD3DPP, sizeof(DXGI_SWAP_CHAIN_DESC));
	}

	return S_OK;
}

HRESULT D3DOnCreateDevice(IUnknown *pD3D, void *pD3DPP)
{
	if (!Initialised_XRender) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XRender is not initialised.", __func__);
		return E_UNEXPECTED;
	}
	if (!pD3D) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pD3D is NULL.", __func__);
		return E_POINTER;
	}
	if (!pD3DPP) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pD3DPP is NULL.", __func__);
		return E_POINTER;
	}

	EnterCriticalSection(&xlive_critsec_hotkeys);

	thread_id_gui = GetCurrentThreadId();

	DeleteD3DPresentationParameters();
	ReleaseD3DDevice();

	HRESULT resultQueryInterface = pD3D->QueryInterface(IID_IDirect3DDevice9, (void**)&device_d3d9);
	if (FAILED(resultQueryInterface)) {
		resultQueryInterface = pD3D->QueryInterface(IID_IDirect3DDevice9Ex, (void**)&device_d3d9ex);
		if (FAILED(resultQueryInterface)) {
			resultQueryInterface = pD3D->QueryInterface(IID_ID3D10Device, (void**)&device_d3d10);
			if (FAILED(resultQueryInterface)) {
				resultQueryInterface = pD3D->QueryInterface(IID_ID3D11Device, (void**)&device_d3d11);
				if (FAILED(resultQueryInterface)) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pPii->pD3D is not a valid D3D 9, 9Ex, 10 or 11 interface.", __func__);
					return E_INVALIDARG;
				}
				else {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "Direct3D 11 Device Initialised.");
				}
			}
			else {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "Direct3D 10 Device Initialised.");
			}
		}
		else {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "Direct3D 9 Ex Device Initialised.");
		}
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "Direct3D 9 Device Initialised.");
	}

	HRESULT errorPP = UpdatePresentationParameters(pD3DPP);
	if (FAILED(errorPP)) {
		ReleaseD3DDevice();
		LeaveCriticalSection(&xlive_critsec_hotkeys);
		return errorPP;
	}

	StartThreadHotkeys();

	LeaveCriticalSection(&xlive_critsec_hotkeys);

	return S_OK;
}

INT InitXRender()
{
	TRACE_FX();

	InitializeCriticalSection(&xlive_critsec_hotkeys);

	SetFPSLimit(xlive_fps_limit);

	Initialised_XRender = TRUE;
	return S_OK;
}

INT UninitXRender()
{
	TRACE_FX();
	if (!Initialised_XRender) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XRender is not initialised.", __func__);
		return E_UNEXPECTED;
	}

	StopThreadHotkeys();

	EnterCriticalSection(&xlive_critsec_hotkeys);
	DeleteD3DPresentationParameters();
	ReleaseD3DDevice();
	LeaveCriticalSection(&xlive_critsec_hotkeys);

	DeleteCriticalSection(&xlive_critsec_hotkeys);

	Initialised_XRender = FALSE;

	return S_OK;
}

DWORD SetFPSLimit(DWORD fps_limit)
{
	DWORD old_limit;
	EnterCriticalSection(&xlive_critsec_fps_limit);
	old_limit = xlive_fps_limit;
	xlive_fps_limit = fps_limit;
	nextFrame = std::chrono::system_clock::now();
	desiredRenderTime = std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::duration<double>(1.0 / (double)fps_limit));
	LeaveCriticalSection(&xlive_critsec_fps_limit);
	return old_limit;
}

static void frameTimeManagement()
{
	std::this_thread::sleep_until(nextFrame);
	do {
		nextFrame += desiredRenderTime;
	} while (std::chrono::system_clock::now() > nextFrame);
}

// #5002
INT WINAPI XLiveRender()
{
	TRACE_FX();
	if (!Initialised_XRender) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XRender is not initialised.", __func__);
		return E_UNEXPECTED;
	}

	if (xlive_fps_limit > 0) {
		frameTimeManagement();
	}
	return S_OK;
}

// #5005
HRESULT WINAPI XLiveOnCreateDevice(IUnknown *pD3D, VOID *pD3DPP)
{
	TRACE_FX();
	if (!Initialised_XRender) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XRender is not initialised.", __func__);
		return E_UNEXPECTED;
	}

	HRESULT result = D3DOnCreateDevice(pD3D, pD3DPP);

	return result;
}

// #5006
HRESULT WINAPI XLiveOnDestroyDevice()
{
	TRACE_FX();
	if (!Initialised_XRender) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XRender is not initialised.", __func__);
		return E_UNEXPECTED;
	}

	StopThreadHotkeys();
	EnterCriticalSection(&xlive_critsec_hotkeys);
	DeleteD3DPresentationParameters();
	ReleaseD3DDevice();
	LeaveCriticalSection(&xlive_critsec_hotkeys);

	return S_OK;
}

// #5007
HRESULT WINAPI XLiveOnResetDevice(void *pD3DPP)
{
	TRACE_FX();
	if (!Initialised_XRender) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XRender is not initialised.", __func__);
		return E_UNEXPECTED;
	}
	EnterCriticalSection(&xlive_critsec_hotkeys);
	HRESULT errorPP = UpdatePresentationParameters(pD3DPP);
	LeaveCriticalSection(&xlive_critsec_hotkeys);
	if (FAILED(errorPP)) {
		return errorPP;
	}

	return S_OK;
}
