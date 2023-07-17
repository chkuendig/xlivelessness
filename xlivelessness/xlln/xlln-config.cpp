#include <winsock2.h>
#include "../xlive/xdefs.hpp"
#include "xlln-config.hpp"
#include "debug-text.hpp"
#include "../utils/utils.hpp"
#include "../xlln/xlln.hpp"
#include "../resource.h"
#include "../xlive/xrender.hpp"
#include "../xlive/xsocket.hpp"
#include "../xlive/xlive.hpp"
#include "../xlive/voice.hpp"
#include <stdio.h>
#include <vector>
// Link with iphlpapi.lib
#include <iphlpapi.h>

wchar_t *xlln_file_config_path = 0;

static const char XllnConfigHeader[] = "XLLN-Config-Version";
static const char XllnConfigVersion[] = DLL_VERSION_STR;

typedef struct {
	bool keepInvalidLines = false;
	bool saveValuesRead = true;
	bool readConfigFromOtherInstance = false;
	std::vector<char*> badConfigEntries;
	std::vector<char*> otherConfigHeaderEntries;
	std::vector<char*> readSettings;
} INTERPRET_CONFIG_CONTEXT;

static int interpretConfigSetting(const char *fileLine, const char *version, size_t lineNumber, void *interpretationContext)
{
	if (!version) {
		return 0;
	}
	INTERPRET_CONFIG_CONTEXT &configContext = *(INTERPRET_CONFIG_CONTEXT*)interpretationContext;

	bool unrecognised = false;
	bool duplicated = false;
	bool incorrect = false;
	size_t fileLineLen = strlen(fileLine);

	if (fileLine[0] == '#' || fileLine[0] == ';' || fileLineLen <= 2) {
		unrecognised = true;
	}
	// When the version of the header has changed.
	else if (version[-1] != 0) {
		unrecognised = true;
	}
	else {
		const char *equals = strchr(fileLine, '=');

		if (!equals || equals == fileLine) {
			unrecognised = true;
		}
		else {
			size_t iValueSearch = 0;
			const char *value = equals + 1;
			while (*value == '\t' || *value == ' ') {
				value++;
			}

			const char *whitespace = equals;
			while (&whitespace[-1] != fileLine && (whitespace[-1] == '\t' || whitespace[-1] == ' ')) {
				whitespace--;
			}
			size_t settingStrLen = whitespace - fileLine;

			char *settingName = new char[settingStrLen + 1];
			memcpy(settingName, fileLine, settingStrLen);
			settingName[settingStrLen] = 0;

			for (char *&readSettingName : configContext.readSettings) {
				if (_stricmp(readSettingName, settingName) == 0) {
					duplicated = true;
					break;
				}
			}

#define SettingNameMatches(x) _stricmp(x, settingName) == 0

			if (!unrecognised && !duplicated && !incorrect) {

				if (SettingNameMatches("xlln_debug_log_blacklist")) {
					if (configContext.saveValuesRead) {
						char *blacklist = CloneString(value);
						char *nextValue = blacklist;
						char *comma;

						do {
							comma = strchr(nextValue, ',');
							if (comma) {
								*comma = 0;
							}

							addDebugTextBlacklist((const char*)nextValue);

							nextValue = comma + 1;
						} while (comma);
						free(blacklist);
					}
				}
				else if (SettingNameMatches("xlln_debuglog_level")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "0x%x", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							xlln_debuglog_level = tempuint32;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_hotkey_id_guide")) {
					uint16_t tempuint16;
					if (sscanf_s(value, "0x%hx", &tempuint16) == 1) {
						if (configContext.saveValuesRead) {
							xlive_hotkey_id_guide = tempuint16;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_fps_limit")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							SetFPSLimit(tempuint32);
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_base_port")) {
					uint16_t tempuint16;
					if (sscanf_s(value, "%hu", &tempuint16) == 1) {
						if (configContext.saveValuesRead) {
							if (configContext.readConfigFromOtherInstance) {
								xlive_base_port = xlive_base_port_broadcast_spacing_start + ((xlln_local_instance_index - 1) * xlive_base_port_broadcast_spacing_increment);
							}
							else {
								if (tempuint16 == 0) {
									tempuint16 = 0xFFFF;
								}
								xlive_base_port = tempuint16;
							}
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_base_port_broadcast_spacing")) {
					uint16_t tempuint16Start, tempuint16Increment, tempuint16End;
					if (
						sscanf_s(value, "%hu,%hu,%hu", &tempuint16Start, &tempuint16Increment, &tempuint16End) == 3
						&& tempuint16Start != 0 && tempuint16Start != 0xFFFF
						&& tempuint16Increment != 0 && tempuint16Increment != 0xFFFF
						&& tempuint16End != 0 && tempuint16End != 0xFFFF
					) {
						uint16_t portRange = tempuint16Start;
						uint16_t portRangePrev;
						while (1) {
							portRangePrev = portRange;
							portRange += tempuint16Increment;
							if (portRange < portRangePrev) {
								// Overflow.
								portRange = 0;
								break;
							}
							if (portRange > tempuint16End) {
								// misaligned and passed the end.
								portRange = 0;
								break;
							}
							if (portRange == tempuint16End) {
								break;
							}
						}
						
						if (portRange) {
							if (configContext.saveValuesRead) {
								xlive_base_port_broadcast_spacing_start = tempuint16Start;
								xlive_base_port_broadcast_spacing_increment = tempuint16Increment;
								xlive_base_port_broadcast_spacing_end = tempuint16End;
								
								if (configContext.readConfigFromOtherInstance) {
									xlive_base_port = xlive_base_port_broadcast_spacing_start + ((xlln_local_instance_index - 1) * xlive_base_port_broadcast_spacing_increment);
								}
							}
						}
						else {
							incorrect = true;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_net_disable")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead && !xlive_netsocket_abort) {
							xlive_netsocket_abort = tempuint32 > 0;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_xhv_engine_enabled")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							xlive_xhv_engine_enabled = tempuint32 > 0;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_network_adapter")) {
					size_t adapterNameLen = strlen(value);
					if (adapterNameLen == 0) {
						if (configContext.saveValuesRead) {
							if (xlive_config_preferred_network_adapter_name) {
								delete[] xlive_config_preferred_network_adapter_name;
								xlive_config_preferred_network_adapter_name = NULL;
							}
						}
					}
					else if (adapterNameLen > 0 && adapterNameLen <= MAX_ADAPTER_NAME_LENGTH) {
						if (configContext.saveValuesRead) {
							if (xlive_config_preferred_network_adapter_name) {
								delete[] xlive_config_preferred_network_adapter_name;
							}
							xlive_config_preferred_network_adapter_name = CloneString(value);
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_ignore_title_network_adapter")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							xlive_ignore_title_network_adapter = tempuint32 > 0;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_broadcast_address")) {
					if (configContext.saveValuesRead) {
						// TODO ParseBroadcastAddrInput(temp);
						// do this later when config saving can be done dynamically (also need to update UI).
						if (broadcastAddrInput) {
							delete[] broadcastAddrInput;
						}
						broadcastAddrInput = CloneString(value);
					}
				}
				else if (SettingNameMatches("xlln_direct_ip_connect_password")) {
					if (configContext.saveValuesRead) {
						if (xlln_direct_ip_connect_password) {
							delete[] xlln_direct_ip_connect_password;
						}
						xlln_direct_ip_connect_password = CloneString(value);
					}
				}
				else if (SettingNameMatches("xlln_direct_ip_connect_ip_port")) {
					if (configContext.saveValuesRead) {
						if (xlln_direct_ip_connect_ip_port) {
							delete[] xlln_direct_ip_connect_ip_port;
						}
						xlln_direct_ip_connect_ip_port = CloneString(value);
					}
				}
				else if (SettingNameMatches("xlive_auto_login_on_xliveinitialize")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							xlive_auto_login_on_xliveinitialize = tempuint32 > 0;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (_strnicmp("xlive_username_p", settingName, 16) == 0) {
					uint32_t iUser;
					if (sscanf_s(&settingName[16], "%u", &iUser) == 1 || iUser == 0 || iUser > XLIVE_LOCAL_USER_COUNT) {
						iUser--;
						char *username = CloneString(value);
						if (TrimRemoveConsecutiveSpaces(username) <= XUSER_MAX_NAME_LENGTH) {
							if (configContext.saveValuesRead && xlive_users_info[iUser]) {
								memcpy(xlive_users_username[iUser], username, XUSER_NAME_SIZE);
								xlive_users_username[iUser][XUSER_NAME_SIZE - 1] = 0;
							}
						}
						else {
							incorrect = true;
						}
						delete[] username;
					}
					else {
						unrecognised = true;
					}
				}
				else if (_strnicmp("xlive_user_live_enabled_p", settingName, 25) == 0) {
					uint32_t iUser;
					if (sscanf_s(&settingName[25], "%u", &iUser) == 1 || iUser == 0 || iUser > XLIVE_LOCAL_USER_COUNT) {
						iUser--;
						uint32_t tempuint32;
						if (sscanf_s(value, "%u", &tempuint32) == 1) {
							if (configContext.saveValuesRead) {
								xlive_users_live_enabled[iUser] = tempuint32 > 0;
							}
						}
						else {
							incorrect = true;
						}
					}
					else {
						unrecognised = true;
					}
				}
				else if (_strnicmp("xlive_user_auto_login_p", settingName, 23) == 0) {
					uint32_t iUser;
					if (sscanf_s(&settingName[23], "%u", &iUser) == 1 || iUser == 0 || iUser > XLIVE_LOCAL_USER_COUNT) {
						iUser--;
						uint32_t tempuint32;
						if (sscanf_s(value, "%u", &tempuint32) == 1) {
							if (configContext.saveValuesRead) {
								xlive_users_auto_login[iUser] = tempuint32 > 0;
							}
						}
						else {
							incorrect = true;
						}
					}
					else {
						unrecognised = true;
					}
				}
				else {
					unrecognised = true;
				}

				if (!unrecognised && !duplicated && !incorrect) {
					configContext.readSettings.push_back(CloneString(settingName));
				}
			}

			delete[] settingName;
		}
	}

	if (unrecognised || duplicated || incorrect) {

		if (configContext.keepInvalidLines) {
			// When the version of the header has changed.
			if (version[-1] != 0) {
				configContext.otherConfigHeaderEntries.push_back(CloneString(fileLine));
			}
			else {
				configContext.badConfigEntries.push_back(CloneString(fileLine));
			}
		}
	}
	return 0;
}

static uint32_t SaveXllnConfig(const wchar_t *file_config_path, INTERPRET_CONFIG_CONTEXT *configContext)
{
	FILE* fileConfig = 0;
	
	uint32_t err = _wfopen_s(&fileConfig, file_config_path, L"wb");
	if (err) {
		return err;
	}

#define WriteText(text) fputs(text, fileConfig)
#define WriteTextF(format, ...) fprintf_s(fileConfig, format, __VA_ARGS__)

	WriteText("#--- XLiveLessNess Configuration File ---");
	WriteText("\n");
	
	WriteText("\n# xlln_debug_log_blacklist:");
	WriteText("\n# A comma separated list of trace function names to ignore in the logging to reduce spam.");
	WriteText("\n");
	
	WriteText("\n# xlln_debuglog_level:");
	WriteText("\n# Saves the log level from last use (stored in Hexadecimal).");
	WriteText("\n# DEFAULT value: 0x873F.");
	WriteText("\n# Binary to Hexadecimal:");
	WriteText("\n# 0b1000011100111111 == 0x873F.");
	WriteText("\n# 0bO0000MXG00FEWIDT - Bit Mask Legend.");
	WriteText("\n# Context Options:");
	WriteText("\n# - O - Other - Logs related to functionality from other areas of the application.");
	WriteText("\n# - M - XLLN-Module - Logs related to XLLN-Module functionality.");
	WriteText("\n# - X - XLiveLessNess - Logs related to XLiveLessNess functionality.");
	WriteText("\n# - G - XLive - Logs related to XLive(GFWL) functionality.");
	WriteText("\n# Log Level Options:");
	WriteText("\n# - F - Fatal - Errors that will terminate the application.");
	WriteText("\n# - E - Error - Any error which is fatal to the operation, but not the service or application (can't open a required file, missing data, etc.).");
	WriteText("\n# - W - Warning - Anything that can potentially cause application oddities, but is being handled adequately.");
	WriteText("\n# - I - Info - Generally useful information to log (service start/stop, configuration assumptions, etc).");
	WriteText("\n# - D - Debug - Function, variable and operation logging.");
	WriteText("\n# - T - Trace - Function call tracing.");
	WriteText("\n");
	
	WriteText("\n# xlive_hotkey_id_guide:");
	WriteText("\n# Valid values: 0x0 to 0xFFFF.");
	WriteText("\n#   0x0 - Turns off the hotkey.");
	WriteText("\n#   0x24 - (DEFAULT) - VK_HOME");
	WriteText("\n# This hotkey will open the Guide menu when pressed on (usually) the active Title screen/window.");
	WriteText("\n# If the Title does not use Direct3D (such as in dedicated servers) then the Guide hotkey will not work.");
	WriteText("\n# The following link documents the keyboard Virtual-Key (VK) codes available for use:");
	WriteText("\n# https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes");
	WriteText("\n");
	
	WriteText("\n# xlive_fps_limit:");
	WriteText("\n# Valid values: 0 to (2^32 - 1).");
	WriteText("\n#   0 - Disables the built in limiter.");
	WriteText("\n#   60 - (DEFAULT)");
	WriteText("\n# The frequency (times per second) that XLiveRender is permitted to execute.");
	WriteText("\n# XLiveRender is typically used in Title render loops so this value can be considered as an FPS limit.");
	WriteText("\n");
	
	WriteText("\n# xlive_base_port:");
	WriteText("\n# Valid values: 0 to 65535.");
	WriteText("\n#   0, 65535 - (DEFAULT) Disables the base port override mechanism.");
	WriteText("\n#   2000 - (RECOMMENDED) Use this if enabling.");
	WriteText("\n# The port to base all other ports off of assuming their port numbers are divisable by 100 with the remainder being the offset from the base port. Note that this value is the base port, so a base of 65534 with an offset of anything greater than +1 is not going to be a valid port number (overflows back to 0 which is bad).");
	WriteText("\n# The purpose of this base port is to allow multiple instances of the Title to run where each instance would be spaced from each other by this base port. If the Title doesn't have port mappings compatible with this concept and/or prevents the user from opening multiple instances of the game, an XLLN-Module could be made to hack it to be compatible.");
	WriteText("\n# When this feature is disabled the ports will not be mapped differently from what the Title originally requested.");
	WriteText("\n# For example:");
	WriteText("\n#   - if the Title attempts to use ports 1001 and 1201 then both offsets would be +1 from the base (say the base is 2000 which makes it 2001 in both cases) which also becomes a problem and is bad.");
	WriteText("\n#   - if the the base port is 65534 and the Title attempts to use the port 1002 (offset +2), then it will be mapped to 65536 which isn't a valid 16-bit integer (overflow) and is actually port 0. Port 0 is not a valid port which is an indicator for any port available and the operating system will select one at random which is also bad news.");
	WriteText("\n#   - if the the base port is 65534 and the Title attempts to use the port 1003 (offset +3), then it will be mapped to 65537 which is actually port 1.");
	WriteText("\n");
	
	WriteText("\n# xlive_base_port_broadcast_spacing:");
	WriteText("\n# Valid values: 1 to 65534, 1 to 65534, 1 to 65534.");
	WriteText("\n#   2000,100,2400 - (DEFAULT) (RECOMMENDED).");
	WriteText("\n# The 1st number is the port to start at (inclusive).");
	WriteText("\n# The 2nd number is the number to increment by.");
	WriteText("\n# The 3rd number is the port to end at (inclusive).");
	WriteText("\n# The port number series created by this range is used when broadcasting packets to local networks. This is important when you or someone else locally is running multiple instances of a Title and needs to be notified that your or their host / game lobby is available to join.");
	WriteText("\n# The default generates the following series: 2000, 2100, 2200, 2300, 2400.");
	WriteText("\n# Therefore by default only 5 instances are supported (unless an XLLN-Hub server is used). This range could be increased but would result in greater network data usage / spam and for the normal user 5 should be plenty.");
	WriteText("\n# Additionally, the start and increment values are used as the new xlive_base_port for new multi-instances when they do not have an existing config file (the end value does not stop this incrementing).");
	WriteText("\n");
	
	WriteText("\n# xlive_net_disable:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - (DEFAULT) All network functionality related to xlive functions is enabled.");
	WriteText("\n#   1 - All network functionality related to xlive functions is disabled.");
	WriteText("\n# Allows turning off xlive network functionality.");
	WriteText("\n");
	
	WriteText("\n# xlive_xhv_engine_enabled:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - (DEFAULT) The XHV Engine will not run.");
	WriteText("\n#   1 - The XHV Engine is enabled for use.");
	WriteText("\n# Allows activating the XHV Engine which is used for voice chat.");
	WriteText("\n");
	
	WriteText("\n# xlive_network_adapter:");
	WriteText("\n# Example value: {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}");
	WriteText("\n# The network adapter to use if it is available. If it is not available or this setting is blank then it will be auto-selected.");
	WriteText("\n");
	
	WriteText("\n# xlive_ignore_title_network_adapter:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - (DEFAULT) Factors in the Title's preference for available network adapters.");
	WriteText("\n#   1 - Ignores the setting from the Title.");
	WriteText("\n# On startup in XLiveInitialize(...) the title specifies its preferred network adapter to use (if any). This setting can ignore its preference.");
	WriteText("\n");
	
	WriteText("\n# xlive_broadcast_address:");
	WriteText("\n# A comma separated list of network addresses to send broadcast packets to.");
	WriteText("\n# All addresses are comma, separated. Do not use spaces at all.");
	WriteText("\n# The IP and corresponding Port are (and must be to be considered valid) separated with a colon ':'. Since IPv6 uses colons in its format, IPv6 addresses need to be enclosed in square brackets '[' and ']'.");
	WriteText("\n# If an address is specified with port number 0, it will use the port that the Title tried to broadcast to instead of overwriting that part also.");
	WriteText("\n# So an example of a comma separated list of a domain name, IPv4 (local address broadcast) and an IPv6 address is as follows:");
	WriteText("\n# glitchyscripts.com:1100,192.168.0.255:1100,[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:1100");
	WriteText("\n");
	
	WriteText("\n# xlln_direct_ip_connect_password:");
	WriteText("\n# The last used password used to host or connect with via Direct IP.");
	WriteText("\n");
	
	WriteText("\n# xlln_direct_ip_connect_ip_port:");
	WriteText("\n# The saved IP address and Port of a lobby to connect to.");
	WriteText("\n# The IP and Port are separated with a colon ':'. Since IPv6 uses colons in its format, IPv6 addresses need to be enclosed in square brackets '[' and ']'.");
	WriteText("\n");
	
	WriteText("\n# xlive_auto_login_on_xliveinitialize:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - (DEFAULT) No action.");
	WriteText("\n#   1 - Any users set with xlive_user_auto_login_p? will always be logged in automatically, otherwise if none set then the XLLN window will open.");
	WriteText("\n# This property triggers when the TITLE calls XLiveInitialize/Ex(...) which is generally very early in any TITLE initialisation sequence.");
	WriteText("\n");
	
	WriteText("\n# xlive_username_p1...:");
	WriteText("\n# Max username length is 15 characters.");
	WriteText("\n# The username for each local profile to use.");
	WriteText("\n# This option does not apply when logging in from XLLN-Modules via XLLNLogin(...).");
	WriteText("\n");
	
	WriteText("\n# xlive_user_live_enabled_p1...:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - The user will be signed in to a local offline account.");
	WriteText("\n#   1 - The user will be signed in to an online LIVE account.");
	WriteText("\n# This option does not apply when logging in from XLLN-Modules via XLLNLogin(...).");
	WriteText("\n");
	
	WriteText("\n# xlive_user_auto_login_p1...:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - Do not sign in automatically.");
	WriteText("\n#   1 - Auto sign in.");
	WriteText("\n# Whether the user will be automatically signed in when the Title calls XShowSigninUI(...) and there are no other users already logged in.");
	WriteText("\n# This option does not apply when logging in from XLLN-Modules via XLLNLogin(...).");
	WriteText("\n");
	
	WriteText("\n");
	WriteTextF("\n[%s:%s]", XllnConfigHeader, XllnConfigVersion);
	WriteText("\nxlln_debug_log_blacklist = ");
	EnterCriticalSection(&xlln_critsec_debug_log);
	for (int i = 0; i < blacklist_len; i++) {
		if (i != 0) {
			WriteText(",");
		}
		WriteText(blacklist[i]);
	}
	LeaveCriticalSection(&xlln_critsec_debug_log);
	WriteTextF("\nxlln_debuglog_level = 0x%08x", xlln_debuglog_level);
	WriteTextF("\nxlive_hotkey_id_guide = 0x%04x", (uint16_t)xlive_hotkey_id_guide);
	WriteTextF("\nxlive_fps_limit = %u", (uint32_t)xlive_fps_limit);
	WriteTextF("\nxlive_base_port = %hu", xlive_base_port == 0xFFFF ? 0 : xlive_base_port);
	WriteTextF("\nxlive_base_port_broadcast_spacing = %hu,%hu,%hu", xlive_base_port_broadcast_spacing_start, xlive_base_port_broadcast_spacing_increment, xlive_base_port_broadcast_spacing_end);
	WriteTextF("\nxlive_net_disable = %u", xlive_netsocket_abort ? 1 : 0);
	WriteTextF("\nxlive_xhv_engine_enabled = %u", xlive_xhv_engine_enabled ? 1 : 0);
	WriteTextF("\nxlive_network_adapter = %s", xlive_config_preferred_network_adapter_name ? xlive_config_preferred_network_adapter_name : "");
	WriteTextF("\nxlive_ignore_title_network_adapter = %u", xlive_ignore_title_network_adapter ? 1 : 0);
	WriteTextF("\nxlive_broadcast_address = %s", broadcastAddrInput);
	WriteTextF("\nxlln_direct_ip_connect_password = %s", xlln_direct_ip_connect_password);
	WriteTextF("\nxlln_direct_ip_connect_ip_port = %s", xlln_direct_ip_connect_ip_port);
	WriteTextF("\nxlive_auto_login_on_xliveinitialize = %u", xlive_auto_login_on_xliveinitialize ? 1 : 0);
	for (uint32_t iUser = 0; iUser < XLIVE_LOCAL_USER_COUNT; iUser++) {
		WriteTextF("\nxlive_username_p%u = %s", iUser + 1, xlive_users_username[iUser]);
		WriteTextF("\nxlive_user_live_enabled_p%u = %u", iUser + 1, xlive_users_live_enabled[iUser] ? 1 : 0);
		WriteTextF("\nxlive_user_auto_login_p%u = %u", iUser + 1, xlive_users_auto_login[iUser] ? 1 : 0);
	}
	WriteText("\n\n");
	
	if (configContext) {
		for (char *&badEntry : configContext->badConfigEntries) {
			WriteText(badEntry);
			WriteText("\n");
		}
		if (configContext->badConfigEntries.size()) {
			WriteText("\n\n");
		}
		for (char *&otherEntry : configContext->otherConfigHeaderEntries) {
			WriteText(otherEntry);
			WriteText("\n");
		}
		if (configContext->otherConfigHeaderEntries.size()) {
			WriteText("\n");
		}
	}
	
	fclose(fileConfig);
	
	return ERROR_SUCCESS;
}

static uint32_t XllnConfig(bool save_config)
{
	// Always read the/a config file before saving.
	uint32_t result = ERROR_FUNCTION_FAILED;
	errno_t errorFopen = ERROR_SUCCESS;
	FILE* fileConfig = 0;
	
	// Use exec argument path if provided.
	if (xlln_file_config_path) {
		errorFopen = _wfopen_s(&fileConfig, xlln_file_config_path, L"rb");
		if (!fileConfig) {
			if (errorFopen == ENOENT) {
				result = ERROR_FILE_NOT_FOUND;
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "Config file not found: \"%ls\".", xlln_file_config_path);
			}
			else {
				XLLN_DEBUG_LOG_ECODE(errorFopen, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "Config file \"%ls\" read error:", xlln_file_config_path);
				return ERROR_FUNCTION_FAILED;
			}
		}
	}
	
	wchar_t *configAutoPath = 0;
	wchar_t *configAutoFallbackPathAppdata = 0;
	wchar_t *configAutoFallbackPathWorkingDir = 0;
	bool saveConfigFileToWorkingDirectoryOverride = false;
	bool readConfigFromOtherInstance = false;
	// Do not use auto paths if an exec arg path was provided.
	if (!fileConfig && !xlln_file_config_path) {
		wchar_t* appdataPath = 0;
		errno_t errorEnvVar = _wdupenv_s(&appdataPath, NULL, L"LOCALAPPDATA");
		if (errorEnvVar) {
			XLLN_DEBUG_LOG_ECODE(errorEnvVar, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "%s %%LOCALAPPDATA%% path unable to be resolved with error:", __func__);
		}
		
		for (uint32_t searchInstanceIndex = xlln_local_instance_index; searchInstanceIndex != 0; searchInstanceIndex--) {
			if (searchInstanceIndex != xlln_local_instance_index) {
				readConfigFromOtherInstance = true;
			}
			bool appdataDirectory = false;
			do {
				if (configAutoPath) {
					free(configAutoPath);
				}
				if (appdataDirectory) {
					configAutoPath = FormMallocString(L"%s/XLiveLessNess/xlln-config-%u.ini", appdataPath, searchInstanceIndex);
					if (!configAutoFallbackPathAppdata) {
						configAutoFallbackPathAppdata = CloneString(configAutoPath);
					}
				}
				else {
					configAutoPath = FormMallocString(L"./XLiveLessNess/xlln-config-%u.ini", searchInstanceIndex);
					if (!configAutoFallbackPathWorkingDir) {
						configAutoFallbackPathWorkingDir = CloneString(configAutoPath);
					}
				}
				errorFopen = _wfopen_s(&fileConfig, configAutoPath, L"rb");
				if (fileConfig) {
					fseek(fileConfig, (long)0, SEEK_END);
					uint32_t fileSize = ftell(fileConfig);
					fseek(fileConfig, (long)0, SEEK_SET);
					fileSize -= ftell(fileConfig);
					
					if (fileSize == 0) {
						fclose(fileConfig);
						fileConfig = 0;
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "Auto config file is empty: \"%ls\".", configAutoPath);
						if (!appdataDirectory && searchInstanceIndex == xlln_local_instance_index) {
							saveConfigFileToWorkingDirectoryOverride = true;
						}
					}
					else {
						searchInstanceIndex = 0;
						break;
					}
				}
				else {
					if (errorFopen == ENOENT) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "Auto config file not found: \"%ls\".", configAutoPath);
					}
					else {
						XLLN_DEBUG_LOG_ECODE(errorFopen, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "Auto config file \"%ls\" read error:", configAutoPath);
					}
				}
			} while (!errorEnvVar && (appdataDirectory = !appdataDirectory));
			if (searchInstanceIndex == 0) {
				break;
			}
		}
		
		if (appdataPath) {
			free(appdataPath);
		}
	}
	
	INTERPRET_CONFIG_CONTEXT *interpretationContext = 0;
	if (fileConfig) {
		interpretationContext = new INTERPRET_CONFIG_CONTEXT;
		interpretationContext->keepInvalidLines = true;
		interpretationContext->saveValuesRead = !save_config;
		interpretationContext->readConfigFromOtherInstance = readConfigFromOtherInstance;
		
		ReadIniFile(fileConfig, true, XllnConfigHeader, XllnConfigVersion, interpretConfigSetting, (void*)interpretationContext);
		
		for (char *&readSettingName : interpretationContext->readSettings) {
			delete[] readSettingName;
		}
		interpretationContext->readSettings.clear();
		
		fclose(fileConfig);
		fileConfig = 0;
	}
	
	if (configAutoPath) {
		free(configAutoPath);
		configAutoPath = 0;
	}
	
	for (uint8_t i = 0; i < 2; i++) {
		wchar_t *saveToConfigFilePath = 0;
		if (xlln_file_config_path) {
			saveToConfigFilePath = xlln_file_config_path;
			i = 2;
		}
		else if (saveConfigFileToWorkingDirectoryOverride) {
			saveToConfigFilePath = configAutoFallbackPathWorkingDir;
			i = 2;
		}
		else if (i == 0) {
			saveToConfigFilePath = configAutoFallbackPathAppdata;
		}
		else if (i == 1) {
			saveToConfigFilePath = configAutoFallbackPathWorkingDir;
		}
		
		if (saveToConfigFilePath) {
			wchar_t *saveToConfigPath = PathFromFilename(saveToConfigFilePath);
			uint32_t errorMkdir = EnsureDirectoryExists(saveToConfigPath);
			delete[] saveToConfigPath;
			if (errorMkdir) {
				result = ERROR_DIRECTORY;
				XLLN_DEBUG_LOG_ECODE(errorMkdir, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "%s EnsureDirectoryExists(...) error on path \"%ls\".", __func__, saveToConfigFilePath);
				break;
			}
			else {
				uint32_t errorSaveConfig = SaveXllnConfig(saveToConfigFilePath, interpretationContext);
				if (errorSaveConfig) {
					result = errorSaveConfig;
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "SaveXllnConfig(...) error: %u, \"%ls\".", errorSaveConfig, saveToConfigFilePath);
				}
				else {
					result = ERROR_SUCCESS;
					if (!xlln_file_config_path) {
						xlln_file_config_path = CloneString(saveToConfigFilePath);
					}
				}
				break;
			}
		}
	}
	
	if (configAutoFallbackPathAppdata) {
		delete[] configAutoFallbackPathAppdata;
		configAutoFallbackPathAppdata = 0;
	}
	if (configAutoFallbackPathWorkingDir) {
		delete[] configAutoFallbackPathWorkingDir;
		configAutoFallbackPathWorkingDir = 0;
	}
	
	if (interpretationContext) {
		for (char *&badEntry : interpretationContext->badConfigEntries) {
			delete[] badEntry;
		}
		interpretationContext->badConfigEntries.clear();
		for (char *&otherEntry : interpretationContext->otherConfigHeaderEntries) {
			delete[] otherEntry;
		}
		interpretationContext->otherConfigHeaderEntries.clear();
	}
	
	return result;
}

uint32_t InitXllnConfig()
{
	XllnConfig(false);
	return ERROR_SUCCESS;
}

uint32_t UninitXllnConfig()
{
	XllnConfig(true);
	return ERROR_SUCCESS;
}
