#include "windows.h"
#include "xlln-modules.hpp"
#include "../dllmain.hpp"
#include "xlln.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-hook.hpp"
#include <string>
#include <vector>

typedef struct {
	HINSTANCE hInstance;
	wchar_t *moduleName;
	DWORD lastError;
} XLLN_MODULE_INFO;

static HANDLE xlln_mutex_load_modules = 0;
static std::vector<XLLN_MODULE_INFO*> xlln_modules;

static void(*xlln_post_imports_hook)() = NULL;
static BYTE xlln_post_imports_hook_source_data[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
static DWORD xlln_post_imports_hook_source_address = NULL;

static void ModuleEntryPointCodeCaveReceive()
{
	DWORD OldProtection;
	DWORD temp;

	int numBytes = 5;

	VirtualProtect((LPVOID)xlln_post_imports_hook_source_address, numBytes, PAGE_EXECUTE_READWRITE, &OldProtection);
	memcpy((LPVOID)xlln_post_imports_hook_source_address, xlln_post_imports_hook_source_data, numBytes);
	VirtualProtect((LPVOID)xlln_post_imports_hook_source_address, numBytes, OldProtection, &temp);

	xlln_post_imports_hook();
}

static __declspec(naked) void ModuleEntryPointCodeCaveReceiveHelper(void)
{
	__asm
	{
		pushfd
		pushad

		// Modify return address to be -5 to make up for the call to this.
		sub[esp + 20h + 4h], 5

		// This will undo the code cave and execute the desired function.
		call ModuleEntryPointCodeCaveReceive

		popad
		popfd
		retn
	}
}

static HRESULT InjectModuleEntryPointHook(HMODULE hModule, void(post_imports_hook)())
{
	XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG, "Hooking Entity Point of Module: 0x%08x.", hModule);
	if (hModule == NULL || hModule == INVALID_HANDLE_VALUE) {
		XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL, "Invalid hModule handle.");
		return ERROR_INVALID_HANDLE;
	}

	IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)hModule;

	if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
		XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL, "Not DOS - This file is not a DOS application.");
		return ERROR_BAD_EXE_FORMAT;
	}

	IMAGE_NT_HEADERS* nt_headers = (IMAGE_NT_HEADERS*)((DWORD)hModule + dos_header->e_lfanew);

	if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
		XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL, "Not Valid PE - This file is not a valid NT Portable Executable.");
		return ERROR_BAD_EXE_FORMAT;
	}

	xlln_post_imports_hook_source_address = (DWORD)((DWORD)xlln_hmod_title + (DWORD)nt_headers->OptionalHeader.AddressOfEntryPoint);
	xlln_post_imports_hook = post_imports_hook;

	{
		DWORD offset = (PtrToUlong(ModuleEntryPointCodeCaveReceiveHelper) - xlln_post_imports_hook_source_address) - 5;

		// Call instruction.
		BYTE patch[5] = { 0xE8, 0x00, 0x00, 0x00, 0x00 };
		int numBytes = 5;
		memcpy(patch + 1, &offset, sizeof(DWORD));

		DWORD OldProtection;
		DWORD temp;

		VirtualProtect((LPVOID)xlln_post_imports_hook_source_address, numBytes, PAGE_EXECUTE_READWRITE, &OldProtection);
		memcpy((LPVOID)xlln_post_imports_hook_source_data, (LPVOID)xlln_post_imports_hook_source_address, numBytes);
		memcpy((LPVOID)xlln_post_imports_hook_source_address, patch, numBytes);
		VirtualProtect((LPVOID)xlln_post_imports_hook_source_address, numBytes, OldProtection, &temp);
	}

	XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG, "Hooked Entity Point of Module: 0x%08x.", hModule);
	return ERROR_SUCCESS;
}

static void InitPostImports()
{
	XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_TRACE | XLLN_LOG_LEVEL_DEBUG, "Hooked PE entity point invoked InitPostImports().");

	if (xlln_mutex_load_modules) {
		// Load all additional modules.
		WIN32_FIND_DATAW data;
		HANDLE hFind = FindFirstFileW(L"./xlln/modules/*.dll", &data);
		if (hFind != INVALID_HANDLE_VALUE) {
			do {
				wchar_t *xllnModuleFilePath = FormMallocString(L"./xlln/modules/%s", data.cFileName);
				XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG, "Loading XLLN-Module: \"%ls\".", xllnModuleFilePath);
				HINSTANCE hInstanceXllnModule = LoadLibraryW(xllnModuleFilePath);
				if (hInstanceXllnModule) {
					XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO, "XLLN-Module loaded: \"%ls\".", xllnModuleFilePath);
					XLLN_MODULE_INFO *xllnModuleInfo = (XLLN_MODULE_INFO*)malloc(sizeof(XLLN_MODULE_INFO));
					memset(xllnModuleInfo, 0, sizeof(XLLN_MODULE_INFO));
					xllnModuleInfo->hInstance = hInstanceXllnModule;
					xllnModuleInfo->moduleName = xllnModuleFilePath;
					xlln_modules.push_back(xllnModuleInfo);
				}
				else {
					XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_WARN, "XLLN-Module was not loaded: \"%ls\".", xllnModuleFilePath);
					free(xllnModuleFilePath);
				}
			} while (FindNextFileW(hFind, &data));
			FindClose(hFind);
		}

		typedef DWORD(WINAPI *tXllnModulePostInit)();
		for (unsigned int i = 0; i < xlln_modules.size(); i++) {
			if (!xlln_modules[i]->hInstance) {
				continue;
			}
			tXllnModulePostInit xllnModulePostInit = (tXllnModulePostInit)GetProcAddress(xlln_modules[i]->hInstance, (PCSTR)41101);
			if (xllnModulePostInit) {
				XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG, "Invoking XLLN-Module Post Init for: \"%ls\".", xlln_modules[i]->moduleName);
				xlln_modules[i]->lastError = xllnModulePostInit();
				if (xlln_modules[i]->lastError == ERROR_SUCCESS) {
					XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO, "XLLN-Module Post Init invoked for: \"%ls\".", xlln_modules[i]->moduleName);
				} else {
					XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR, "XLLN-Module Post Init invoked and returned error 0x%08x \"%ls\".", xlln_modules[i]->lastError, xlln_modules[i]->moduleName);
					FreeLibrary(xlln_modules[i]->hInstance);
					XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG, "Unloaded XLLN-Module: \"%ls\".", xlln_modules[i]->moduleName);
					xlln_modules[i]->hInstance = 0;
				}
			}
		}
	}

	XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG, "Returning from hooked PE entity point InitPostImports().");
}

HRESULT InitXllnModules()
{
	char *mutexName = FormMallocString("Global\\XllnLoadModules0x%x", GetCurrentProcessId());
	xlln_mutex_load_modules = CreateMutexA(0, TRUE, mutexName);
	DWORD lastErr = GetLastError();
	if (lastErr == ERROR_ALREADY_EXISTS || (xlln_mutex_load_modules && xlln_mutex_load_modules == INVALID_HANDLE_VALUE)) {
		XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR, "XLLN failed to create mutex for loading XLLN Modules. Mutex: \"%s\".", mutexName);
		if (xlln_mutex_load_modules && xlln_mutex_load_modules != INVALID_HANDLE_VALUE) {
			CloseHandle(xlln_mutex_load_modules);
		}
		xlln_mutex_load_modules = 0;
	}
	free(mutexName);

	// Emphasize that NOTHING else should be done after this point to cause this DLL not to load successfully.
	return InjectModuleEntryPointHook(xlln_hmod_title, InitPostImports);
}

HRESULT UninitXllnModules()
{
	// Free all additional modules.
	typedef DWORD(WINAPI *tXllnModulePreUninit)();
	for (unsigned int i = 0; i < xlln_modules.size(); i++) {
		if (!xlln_modules[i]->hInstance) {
			continue;
		}
		tXllnModulePreUninit xllnModulePreUninit = (tXllnModulePreUninit)GetProcAddress(xlln_modules[i]->hInstance, (PCSTR)41102);
		if (xllnModulePreUninit) {
			XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG, "Invoking XLLN-Module Pre Uninit for: \"%ls\".", xlln_modules[i]->moduleName);
			xlln_modules[i]->lastError = xllnModulePreUninit();
			if (xlln_modules[i]->lastError == ERROR_SUCCESS) {
				XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO, "XLLN-Module Pre Uninit invoked for: \"%ls\".", xlln_modules[i]->moduleName);
			}
			else {
				XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR, "XLLN-Module Pre Uninit invoked and returned error 0x%08x \"%ls\".", xlln_modules[i]->lastError, xlln_modules[i]->moduleName);
			}
		}
	}

	unsigned int i = xlln_modules.size();
	if (i) {
		// Iterate backward and free all modules (in case order is important with undoing hooks for example).
		do {
			i--;
			if (xlln_modules[i]->hInstance) {
				XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG, "Unloading XLLN-Module: \"%ls\".", xlln_modules[i]->moduleName);
				FreeLibrary(xlln_modules[i]->hInstance);
				XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO, "Unloaded XLLN-Module: \"%ls\".", xlln_modules[i]->moduleName);
			}
			free(xlln_modules[i]->moduleName);
			free(xlln_modules[i]);
		} while (i > 0);
		xlln_modules.clear();
	}

	if (xlln_mutex_load_modules) {
		XLLNDebugLogF(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG, "Closing load XLLN-Module mutex: 0x%08x.", xlln_mutex_load_modules);
		CloseHandle(xlln_mutex_load_modules);
		xlln_mutex_load_modules = 0;
	}

	return ERROR_SUCCESS;
}
