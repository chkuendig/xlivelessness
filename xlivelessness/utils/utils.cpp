#include "windows.h"
#include "utils.hpp"
#include <string>

char* FormMallocString(const char *const format, ...)
{
	size_t currentLen = 1;
	size_t currentBufLen = sizeof(char) * currentLen;
	char *resultStr = (char*)malloc(currentBufLen);

	va_list args;
	while (1) {
		va_start(args, format);
		int needLen = vsnprintf(resultStr, currentLen, format, args);
		va_end(args);
		if (needLen < 0) {
			// string formatting error.
			free(resultStr);
			return 0;
		}
		if ((size_t)needLen >= currentLen) {
			currentLen = needLen + 1;
			currentBufLen = sizeof(char) * currentLen;
			resultStr = (char*)realloc(resultStr, currentBufLen);
		}
		else {
			break;
		}
	}
	return resultStr;
}

wchar_t* FormMallocString(const wchar_t *const format, ...)
{
	va_list args;
	va_start(args, format);
	int needLen = _vscwprintf_p(format, args);

	if (needLen < 0) {
		va_end(args);
		return 0;
	}

	size_t resultBufLen = sizeof(wchar_t) * (needLen + 1);
	wchar_t *resultStr = (wchar_t*)malloc(resultBufLen);
	needLen = _vsnwprintf(resultStr, resultBufLen, format, args);
	// I cannot for the life of me get this to work without it corrupting the heap. So much for being the safe version.
	//needLen = _vsnwprintf_s(resultStr, resultBufLen, _TRUNCATE, format, args);
	va_end(args);

	if (needLen < 0) {
		free(resultStr);
		return 0;
	}

	return resultStr;
}

// new'ly[] allocate a copy of this input string.
char* CloneString(const char *str)
{
	size_t strBufLen = strlen(str) + 1;
	char *result = new char[strBufLen];
	memcpy(result, str, strBufLen);
	return result;
}

// new'ly[] allocate a copy of this input string.
wchar_t* CloneString(const wchar_t *str)
{
	size_t strWBufLen = wcslen(str) + 1;
	wchar_t *result = new wchar_t[strWBufLen];
	memcpy(result, str, strWBufLen * sizeof(wchar_t));
	return result;
}

size_t TrimRemoveConsecutiveSpaces(char* text)
{
	size_t text_len = strlen(text);
	size_t text_pos = 0;
	for (size_t j = 0; j < text_len; j++) {
		if (text_pos == 0) {
			if (text[j] != ' ') {
				text[text_pos++] = text[j];
			}
			continue;
		}
		if (!(text[j] == ' ' && text[text_pos - 1] == ' ')) {
			text[text_pos++] = text[j];
		}
	}
	text[text_pos] = 0;
	if (text[text_pos - 1] == ' ') {
		text[--text_pos] = 0;
	}
	return text_pos;//new length
}

int FindLineStart(FILE* fp, int lineStrLen)
{
	int fp_offset_orig = ftell(fp);
	for (int i = lineStrLen; i < 255; i++) {
		if (fp_offset_orig - i < 0) {
			fseek(fp, fp_offset_orig, SEEK_SET);
			return 0;
		}
		fseek(fp, fp_offset_orig - i, SEEK_SET);
		int c = 0;
		if ((c = fgetc(fp)) == '\r' || c == '\n') {
			fseek(fp, fp_offset_orig, SEEK_SET);
			return fp_offset_orig - i + 1;
		}
	}
	fseek(fp, fp_offset_orig, SEEK_SET);
	return fp_offset_orig - lineStrLen;
}

// new'ly[] allocates fileLine.
bool GetFileLine(FILE* fp, char* &fileLine)
{
	fileLine = 0;
	int resultErr;

	fpos_t startPos;
	resultErr = fgetpos(fp, &startPos);
	if (resultErr) {
		return false;
	}

	int currentChar;
	while (currentChar = fgetc(fp)) {
		if (currentChar == EOF) {
			break;
		}
		else if (currentChar == '\r') {
			break;
		}
		else if (currentChar == '\n') {
			break;
		}
	}

	fpos_t endPos;
	resultErr = fgetpos(fp, &endPos);
	if (resultErr) {
		return false;
	}

	size_t lineBufLen = (size_t)(endPos - startPos);
	if (lineBufLen <= 0) {
		return false;
	}

	// find any extra line ending char.
	bool removeExtra = false;
	if (currentChar != EOF) {
		int nextChar = fgetc(fp);
		if (
			(nextChar == '\r' && currentChar == '\n')
			|| (nextChar == '\n' && currentChar == '\r')
			) {
			removeExtra = true;
		}
	}

	fsetpos(fp, &startPos);

	fileLine = new char[lineBufLen];

	if (!fread(fileLine, sizeof(char), lineBufLen, fp)) {
		if (removeExtra) {
			fgetc(fp);
		}
		delete[] fileLine;
		fileLine = 0;
		return false;
	}
	fileLine[lineBufLen - 1] = 0;
	if (removeExtra) {
		fgetc(fp);
	}
	return true;
}

bool GetNTStringLine(char* text, int lineNum, char* &line)
{
	int inc_line_num = 0;
	char* line_begin = text;

	while (inc_line_num++ < lineNum) {
		line_begin = strchr(line_begin, '\n');
		if (line_begin++ == 0) {
			return false;
		}
	}

	int line_len = 0;
	char* line_next = strchr(line_begin, '\n');
	if (line_next == 0) {
		line_len = strlen(line_begin);
	}
	else {
		if (*(line_next - 1) == '\r') {
			line_next--;
		}
		line_len = line_next - line_begin;
	}

	if (line_len <= 0) {
		line = 0;
		return true;
	}

	line = (char*)malloc(sizeof(char) * (line_len + 1));

	memcpy(line, line_begin, line_len);
	line[line_len] = 0;

	return true;
}

// If missing versioning parameters, major will always be on leftmost side.
uint8_t CmpVersions(const char *version_base, const char *version_alt)
{
	if (strcmp(version_base, version_alt) == 0)
		return 0b00000;//same

	int versions[2][4];
	for (int j = 0; j < 2; j++) {
		for (int i = 0; i < 4; i++) {
			versions[j][i] = 0;
		}
	}

	if (sscanf_s(version_base, "%d.%d.%d.%d", &versions[0][0], &versions[0][1], &versions[0][2], &versions[0][3]) <= 0) {
		return 0b10000;//invalid
	}

	if (sscanf_s(version_alt, "%d.%d.%d.%d", &versions[1][0], &versions[1][1], &versions[1][2], &versions[1][3]) <= 0) {
		return 0b10000;//invalid
	}

	for (int i = 0; i < 4; i++) {
		if (versions[0][i] == versions[1][i]) {
			continue;
		}
		else if (versions[0][i] > versions[1][i]) {//alt is old
			return 0b10000 | (0b1000 >> i);
		}
		else {//alt is new
			return 0b00000 | (0b1000 >> i);
		}
	}

	return 0b00000;//same
	//return 0b01000;//new major
	//return 0b00100;//new minor
	//return 0b00010;//new revision
	//return 0b00001;//new build
	//return 0b11000;//old major
	//return 0b10100;//old minor
	//return 0b10010;//old revision
	//return 0b10001;//old build
}

void ReadIniFile(
	void *fileConfig
	, bool configIsFILE
	, const char *header
	, const char *headerVersion
	, int(interpretSettingFunc)(const char *fileLine, const char *version, size_t lineNumber, void *interpretationContext)
	, void *interpretationContext
)
{
	char *headerReadStr = FormMallocString("[%s:%%[^]]]", header);
	bool foundFirstHeader = false;
	char versioning[31] = { "\0000" };
	char *version = &versioning[1];
	int lineNumber = 0;
	char* fileLine;
	while ((configIsFILE && GetFileLine((FILE*)fileConfig, fileLine)) || (!configIsFILE && GetNTStringLine((char*)fileConfig, lineNumber, fileLine))) {
		lineNumber++;
		if (fileLine) {
			size_t iLineSearch = 0;
			while (fileLine[iLineSearch] == '\t' || fileLine[iLineSearch] == ' ') {
				iLineSearch++;
			}
			if (fileLine[iLineSearch] == 0) {
				free(fileLine);
				continue;
			}
			if (fileLine[iLineSearch] == headerReadStr[0] && sscanf_s(&fileLine[iLineSearch], headerReadStr, version, 30)) {
				foundFirstHeader = true;
				// ("Found header on line %d asserting version %s requires version %s.", lineNumber, version, headerVersion);
				// Does not send this line to interpreter.
				if ((versioning[0] = CmpVersions(headerVersion, version)) == 0) {
					free(fileLine);
					continue;
				}
			}
			int rtnCode = interpretSettingFunc(fileLine, foundFirstHeader ? version : 0, lineNumber, interpretationContext);
			free(fileLine);
			if (rtnCode) {
				break;
			}
		}
	}
	free(headerReadStr);
}

void GetVKeyCodeString(int vkey, char* rtnString, int strLen)
{
	snprintf(rtnString, 5, "0x%x", vkey);
	char key_name[20];
	memset(key_name, 0, sizeof(key_name));
	if (vkey >= 0x70 && vkey <= 0x87) {
		int func_num = vkey - 0x70 + 1;
		snprintf(key_name, 20, "VK_F%d", func_num);
	}
	else if (vkey == 0x24) {
		snprintf(key_name, 20, "VK_Home");
	}
	if (strlen(key_name) > 0) {
		snprintf(rtnString + strlen(rtnString), strLen - 5, " - %s", key_name);
	}
}

void PadCStringWithChar(char* strToPad, int toFullLength, char c)
{
	for (int i = strlen(strToPad); i < toFullLength - 1; i++) {
		memset(strToPad + i, c, sizeof(char));
	}
	memset(strToPad + toFullLength - 1, 0, sizeof(char));
}

int GetWidePathFromFullWideFilename(wchar_t* filepath, wchar_t* rtnpath)
{
	wchar_t* offset = wcsrchr(filepath, L'\\');
	wchar_t* off2 = wcsrchr(filepath, L'/');
	offset = offset == NULL ? off2 : ((off2 != NULL && offset < off2) ? off2 : offset);
	if (offset == NULL) {
		return -1;
	}
	swprintf(rtnpath, offset - filepath + 2, filepath);
	return 0;
}

LONG GetDWORDRegKey(HKEY hKey, wchar_t* strValueName, DWORD* nValue)
{
	DWORD dwBufferSize(sizeof(DWORD));
	DWORD nResult(0);
	LONG nError = ::RegQueryValueExW(hKey,
		strValueName,
		0,
		NULL,
		reinterpret_cast<LPBYTE>(&nResult),
		&dwBufferSize);
	if (ERROR_SUCCESS == nError)
	{
		*nValue = nResult;
	}
	return nError;
}

char* custom_label_literal(char* label_escaped)
{
	int label_escaped_length = strlen(label_escaped);
	int length_shift = 0;
	char* label_literal = (char*)malloc(label_escaped_length + 1);

	for (int i = 0; i < label_escaped_length; i++) {
		if (label_escaped[i] == '\\' && label_escaped[i + 1] == '\\') {
			label_literal[i + length_shift] = '\\';
			length_shift--;
			i++;
		}
		else if (label_escaped[i] == '\\' && label_escaped[i + 1] == 'n') {
			label_literal[i + length_shift] = '\n';
			length_shift--;
			i++;
		}
		else if (label_escaped[i] == '\\' && label_escaped[i + 1] == 'r') {
			label_literal[i + length_shift] = '\r';
			length_shift--;
			i++;
		}
		else {
			label_literal[i + length_shift] = label_escaped[i];
		}
	}

	label_literal[label_escaped_length + length_shift] = 0;
	return label_literal;
}

char* custom_label_escape(char* label_literal)
{
	int label_literal_length = strlen(label_literal);
	int length_shift = 0;
	int label_escaped_length = label_literal_length + length_shift + 2;
	char* label_escaped = (char*)malloc(label_escaped_length);

	for (int i = 0; i < label_literal_length; i++) {
		if (label_escaped_length < i + length_shift + 2) {
			label_escaped = (char*)realloc(label_escaped, label_literal_length + length_shift + 2);
		}
		if (label_literal[i] == '\\') {
			label_escaped[i + length_shift] = '\\';
			length_shift++;
			label_escaped[i + length_shift] = '\\';
		}
		else if (label_literal[i] == '\n') {
			label_escaped[i + length_shift] = '\\';
			length_shift++;
			label_escaped[i + length_shift] = 'n';
		}
		else if (label_literal[i] == '\r') {
			label_escaped[i + length_shift] = '\\';
			length_shift++;
			label_escaped[i + length_shift] = 'r';
		}
		else {
			label_escaped[i + length_shift] = label_literal[i];
		}
	}

	label_escaped[label_literal_length + length_shift] = 0;
	return label_escaped;
}

bool FloatIsNaN(float &vagueFloat)
{
	DWORD* vague = (DWORD*)&vagueFloat;
	if ((*vague >= 0x7F800000 && *vague <= 0x7FFFFFFF) || (*vague >= 0xFF800000 && *vague <= 0xFFFFFFFF)) {
		return true;
	}
	return false;
}

int HostnameToIp(char* hostname, char* ip, DWORD buflen)
{
	/*struct hostent *he;
	struct in_addr **addr_list;
	int i;
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	if ((he = gethostbyname(hostname)) == NULL) {
		return WSAGetLastError();
	}

	addr_list = (struct in_addr **) he->h_addr_list;

	for (i = 0; addr_list[i] != NULL; i++)
	{
		//Return the first one;
		strncpy_s(ip, buflen, inet_ntoa(*addr_list[i]), 100);
		return 0;
	}*/

	return 1;
}

static bool rfc3986_allow(char i)
{
	//isalnum(i)//PoS crashes
	if ((i >= '0' && i <= '9') ||
		(i >= 'a' && i <= 'z') ||
		(i >= 'A' && i <= 'Z') ||
		i == '~' || i == '-' || i == '.' || i == '_')
		return true;
	return false;
}

char* encode_rfc3986(char* label_literal, int label_literal_length)
{
	if (label_literal_length < 0)
		label_literal_length = strlen(label_literal);
	int escaped_buflen = (label_literal_length * 3) + 1;
	char* label_escaped = (char*)malloc(escaped_buflen * sizeof(char));
	int escaped_buff_i = 0;

	for (int i = 0; i < label_literal_length; i++) {
		unsigned char uletter = label_literal[i];
		if (!rfc3986_allow(uletter)) {
			sprintf_s(label_escaped + escaped_buff_i, 4, "%%%02X", uletter);
			escaped_buff_i += 2;
		}
		else {
			label_escaped[escaped_buff_i] = label_literal[i];
		}
		escaped_buff_i++;
	}
	label_escaped[escaped_buff_i] = 0;
	return label_escaped;
}

bool StrnCaseInsensEqu(char* str1, char* str2, unsigned int chk_len)
{
	unsigned int chk_len2 = strlen(str1);
	if (chk_len2 < chk_len) {
		chk_len = chk_len2;
	}
	chk_len2 = strlen(str2);
	if (chk_len2 != chk_len) {
		return false;
	}
	const int case_diff = 'A' - 'a';
	for (unsigned int i = 0; i < chk_len; i++) {
		if (str1[i] != str2[i]) {
			int sa = str1[i];
			if (sa >= 'a' && sa <= 'z') {
				sa += case_diff;
			}
			else if (sa >= 'A' && sa <= 'Z') {
				sa -= case_diff;
			}
			if (sa == str2[i]) {
				continue;
			}
			return false;
		}
	}

	return true;
}

char* GetModuleFilePathA(HMODULE hModule)
{
	size_t titleFilenameBufLen = 100;
	char *titleFileName = (char*)malloc(sizeof(char) * titleFilenameBufLen);
	DWORD titleFilenameLen;
	DWORD lastErr;
	while (1) {
		titleFilenameLen = GetModuleFileNameA(hModule, titleFileName, titleFilenameBufLen);
		if ((lastErr = GetLastError()) == ERROR_INSUFFICIENT_BUFFER) {
			titleFileName = (char*)realloc(titleFileName, sizeof(char) * (titleFilenameBufLen += 100));
			continue;
		}
		break;
	}
	if (!titleFilenameLen) {
		free(titleFileName);
		return 0;
	}
	return titleFileName;
}

wchar_t* GetModuleFilePathW(HMODULE hModule)
{
	size_t titleFilenameBufLen = 100;
	wchar_t *titleFileName = (wchar_t*)malloc(sizeof(wchar_t) * titleFilenameBufLen);
	DWORD titleFilenameLen;
	DWORD lastErr;
	while (1) {
		titleFilenameLen = GetModuleFileNameW(hModule, titleFileName, titleFilenameBufLen);
		if ((lastErr = GetLastError()) == ERROR_INSUFFICIENT_BUFFER) {
			titleFileName = (wchar_t*)realloc(titleFileName, sizeof(wchar_t) * (titleFilenameBufLen += 100));
			continue;
		}
		break;
	}
	if (!titleFilenameLen) {
		free(titleFileName);
		return 0;
	}
	return titleFileName;
}

void wcstombs2(char* buffer, const wchar_t* text, size_t buf_len)
{
	if (buf_len == 0) {
		return;
	}
	size_t loop_len = wcslen(text) + 1;
	if (loop_len > buf_len) {
		loop_len = buf_len;
	}
	loop_len--;
	size_t buffer_i = 0;
	for (size_t i = 0; i < loop_len; i++) {
		if (text[i] >= 0 && text[i] <= 0xFF) {
			buffer[buffer_i++] = (char)text[i];
		}
	}
	buffer[buffer_i] = 0;
}

void ReplaceFilePathSensitiveChars(char *filename)
{
	size_t loopLen = strlen(filename);
	for (size_t i = 0; i < loopLen; i++) {
		if (
			filename[i] == '\\'
			|| filename[i] == '/'
			|| filename[i] == ':'
			|| filename[i] == '*'
			|| filename[i] == '?'
			|| filename[i] == '\"'
			|| filename[i] == '<'
			|| filename[i] == '>'
			|| filename[i] == '|'
			) {
			filename[i] = '_';
		}
	}
}

void ReplaceFilePathSensitiveChars(wchar_t *filename)
{
	size_t loopLen = wcslen(filename);
	for (size_t i = 0; i < loopLen; i++) {
		if (
			filename[i] == '\\'
			|| filename[i] == '/'
			|| filename[i] == ':'
			|| filename[i] == '*'
			|| filename[i] == '?'
			|| filename[i] == '\"'
			|| filename[i] == '<'
			|| filename[i] == '>'
			|| filename[i] == '|'
		) {
			filename[i] = '_';
		}
	}
}

// Assuming whitespace trimmed pathname.
// Can handle forward or back slash combined paths.
const char* LastPathItem(const char *pathname)
{
	return LastPathItem(pathname, EOF);
}

const char* LastPathItem(const char *pathname, size_t end_length)
{
	return LastPathItem((char*)pathname, end_length);
}

// Assuming whitespace trimmed pathname.
// Can handle forward or back slash combined paths.
const wchar_t* LastPathItem(const wchar_t *pathname)
{
	return LastPathItem(pathname, EOF);
}

const wchar_t* LastPathItem(const wchar_t *pathname, size_t end_length)
{
	return LastPathItem((wchar_t*)pathname, end_length);
}

char* LastPathItem(char *pathname, size_t end_length)
{
	if (!pathname) {
		return 0;
	}
	
	char *result = pathname;
	size_t pathnameLen = end_length != EOF ? end_length : strlen(pathname);
	
	// skip the last normal char and null sentinel
	// (since we don't want to break on a folder path ending with a slash).
	for (char *pathnameScan = &pathname[pathnameLen - 2]; pathnameScan >= pathname; pathnameScan--) {
		if (*pathnameScan == '/' || *pathnameScan == '\\') {
			result = &pathnameScan[1];
			break;
		}
	}
	
	return result;
}

wchar_t* LastPathItem(wchar_t *pathname, size_t end_length)
{
	if (!pathname) {
		return 0;
	}
	
	wchar_t *result = pathname;
	size_t pathnameLen = end_length != EOF ? end_length : wcslen(pathname);
	
	// skip the last normal char and null sentinel
	// (since we don't want to break on a folder path ending with a slash).
	for (wchar_t *pathnameScan = &pathname[pathnameLen - 2]; pathnameScan >= pathname; pathnameScan--) {
		if (*pathnameScan == '/' || *pathnameScan == '\\') {
			result = &pathnameScan[1];
			break;
		}
	}
	
	return result;
}

// Can handle forward or back slash combined paths.
const char* FilenameFromPathname(const char *pathname)
{
	if (!pathname) {
		return 0;
	}
	
	const char *result = pathname;
	size_t pathnameLen = strlen(pathname);
	
	// skip the null sentinel
	for (const char *pathnameScan = &pathname[pathnameLen - 1]; pathnameScan > pathname; pathnameScan--) {
		if (*pathnameScan == '/' || *pathnameScan == '\\') {
			result = &pathnameScan[1];
			break;
		}
	}
	
	return result;
}

// Can handle forward or back slash combined paths.
const wchar_t* FilenameFromPathname(const wchar_t *pathname)
{
	return FilenameFromPathname((wchar_t*)pathname);
}
// Can handle forward or back slash combined paths.
wchar_t* FilenameFromPathname(wchar_t *pathname)
{
	if (!pathname) {
		return 0;
	}
	
	wchar_t *result = pathname;
	size_t pathnameLen = wcslen(pathname);
	
	// skip the null sentinel
	for (wchar_t *pathnameScan = &pathname[pathnameLen - 1]; pathnameScan > pathname; pathnameScan--) {
		if (*pathnameScan == '/' || *pathnameScan == '\\') {
			result = &pathnameScan[1];
			break;
		}
	}
	
	return result;
}

// new'ly[] allocates a string with just the path not including the filename.
char* PathFromFilename(const char *filename_path)
{
	const char *filename = FilenameFromPathname(filename_path);
	if (!filename) {
		return 0;
	}
	
	size_t pathLen = ((size_t)filename - (size_t)filename_path) / sizeof(char);
	char *path = new char[pathLen + 1];
	memcpy(path, filename_path, pathLen * sizeof(char));
	path[pathLen] = 0;
	
	return path;
}

// new'ly[] allocates a string with just the path not including the filename.
wchar_t* PathFromFilename(const wchar_t *filename_path)
{
	const wchar_t *filename = FilenameFromPathname(filename_path);
	if (!filename) {
		return 0;
	}

	size_t pathLen = ((size_t)filename - (size_t)filename_path) / sizeof(wchar_t);
	wchar_t *path = new wchar_t[pathLen + 1];
	memcpy(path, filename_path, pathLen * sizeof(wchar_t));
	path[pathLen] = 0;

	return path;
}

// new'ly[] allocates a string with just the path not including the filename.
wchar_t* GetParentPath(const wchar_t *path)
{
	const wchar_t *lastItem = LastPathItem(path);
	if (!lastItem) {
		return 0;
	}

	size_t parentPathLen = ((size_t)lastItem - (size_t)path) / sizeof(wchar_t);
	wchar_t *parentPath = new wchar_t[parentPathLen + 1];
	memcpy(parentPath, path, parentPathLen * sizeof(wchar_t));
	parentPath[parentPathLen] = 0;

	return parentPath;
}

// Find the number of chars that are the same between the primary and secondary string (does not include end sentinel).
// If identical then { result == strlen(primary) }.
size_t StrSameChrFor(const char *primary, const char *secondary)
{
	if (!primary || !secondary) {
		return 0;
	}
	
	for (size_t i = 0; true; i++) {
		if (primary[i] != secondary[i]
			|| primary[i] == 0
			|| primary[i] == EOF
			|| secondary[i] == 0
			|| secondary[i] == EOF
		) {
			return i;
		}
	}
	
	return 0;
}

// Find the number of chars that are the same between the primary and secondary string (does not include end sentinel).
// If identical then { result == strlen(primary) }.
size_t StrSameChrFor(const wchar_t *primary, const wchar_t *secondary)
{
	if (!primary || !secondary) {
		return 0;
	}
	
	for (size_t i = 0; true; i++) {
		if (primary[i] != secondary[i]
			|| primary[i] == 0
			|| primary[i] == EOF
			|| secondary[i] == 0
			|| secondary[i] == EOF
		) {
			return i;
		}
	}
	
	return 0;
}

// Same as StrSameChrFor except it only checks for whole path items (whole matching directories and ending file).
size_t StrSamePathFor(const char *primary, const char *secondary)
{
	if (!primary || !secondary) {
		return 0;
	}
	
	size_t i = 0;
	
	// Search for first difference or end of string.
	while (1) {
		if (primary[i] != secondary[i]
			|| primary[i] == 0
			|| primary[i] == EOF
			|| secondary[i] == 0
			|| secondary[i] == EOF
		) {
			break;
		}
		i++;
	}
	
	if (i != 0 && (primary[i] != 0 || secondary[i] != 0)) {
		// Go back if this is not the same path item.
		while (1) {
			if (primary[i] == '/' || primary[i] == '\\') {
				i++;
				break;
			}
			if (i == 0) {
				break;
			}
			i--;
		}
	}
	
	return i;
}

// Same as StrSameChrFor except it only checks for whole path items (whole matching directories and ending file).
size_t StrSamePathFor(const wchar_t *primary, const wchar_t *secondary)
{
	if (!primary || !secondary) {
		return 0;
	}
	
	size_t i = 0;
	
	// Search for first difference or end of string.
	while (1) {
		if (primary[i] != secondary[i]
			|| primary[i] == 0
			|| primary[i] == EOF
			|| secondary[i] == 0
			|| secondary[i] == EOF
		) {
			break;
		}
		i++;
	}
	
	if (i != 0 && (primary[i] != 0 || secondary[i] != 0)) {
		// Go back if this is not the same path item.
		while (1) {
			if (primary[i] == '/' || primary[i] == '\\') {
				i++;
				break;
			}
			if (i == 0) {
				break;
			}
			i--;
		}
	}
	
	return i;
}

size_t LenOfFirstPathItem(const char *pathname)
{
	size_t result = 0;
	char chr;
	while (1) {
		chr = pathname[result];
		if (chr == 0) {
			break;
		}
		result++;
		if (chr == '/' || chr == '\\') {
			break;
		}
	}
	return result;
}

size_t LenOfFirstPathItem(const wchar_t *pathname)
{
	size_t result = 0;
	wchar_t chr;
	while (1) {
		chr = pathname[result];
		if (chr == 0) {
			break;
		}
		result++;
		if (chr == '/' || chr == '\\') {
			break;
		}
	}
	return result;
}

// new'ly[] allocates a string for the end item found before the end_position.
char* GetItemBeforeLastPos(const char *full_pathname, const char *&end_position)
{
	if (full_pathname == 0 || (end_position != 0 && end_position <= full_pathname)) {
		return 0;
	}
	if (end_position == 0) {
		end_position = full_pathname + strlen(full_pathname);
	}
	
	const char *start_position = LastPathItem(full_pathname, end_position - full_pathname);

	size_t resultLen = ((size_t)end_position - (size_t)start_position) / sizeof(char);
	char *result = new char[resultLen + 1];
	memcpy(result, start_position, resultLen * sizeof(char));
	result[resultLen] = 0;
	
	end_position = start_position;
	
	return result;
}

// new'ly[] allocates a string for the end item found before the end_position.
wchar_t* GetItemBeforeLastPos(const wchar_t *full_pathname, const wchar_t *&end_position)
{
	if (full_pathname == 0 || (end_position != 0 && end_position <= full_pathname)) {
		return 0;
	}
	if (end_position == 0) {
		end_position = full_pathname + wcslen(full_pathname);
	}
	
	const wchar_t *start_position = LastPathItem(full_pathname, end_position - full_pathname);
	
	size_t resultLen = ((size_t)end_position - (size_t)start_position) / sizeof(wchar_t);
	wchar_t *result = new wchar_t[resultLen + 1];
	memcpy(result, start_position, resultLen * sizeof(wchar_t));
	result[resultLen] = 0;
	
	end_position = start_position;
	
	return result;
}

bool EndsWith(const char *str, const char *suffix)
{
	if (!str || !suffix) {
		return false;
	}
	size_t lenstr = strlen(str);
	size_t lensuffix = strlen(suffix);
	if (lensuffix > lenstr) {
		return false;
	}
	return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

bool EndsWith(const wchar_t *str, const wchar_t *suffix)
{
	if (!str || !suffix) {
		return false;
	}
	size_t lenstr = wcslen(str);
	size_t lensuffix = wcslen(suffix);
	if (lensuffix > lenstr) {
		return false;
	}
	return wcsncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

bool EndsWithCaseInsensitive(const wchar_t *str, const wchar_t *suffix)
{
	if (!str || !suffix) {
		return false;
	}
	size_t lenstr = wcslen(str);
	size_t lensuffix = wcslen(suffix);
	if (lensuffix > lenstr) {
		return false;
	}
	return _wcsnicmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

uint32_t EnsureDirectoryExists(const wchar_t *path)
{
	int32_t resultMkdir = _wmkdir(path);
	uint32_t errorMkdir = GetLastError();
	if (resultMkdir == 0 || errorMkdir == ERROR_ALREADY_EXISTS) {
		return ERROR_SUCCESS;
	}
	if (errorMkdir == ERROR_PATH_NOT_FOUND) {
		wchar_t *parentPath = GetParentPath(path);
		uint32_t resultNested = EnsureDirectoryExists(parentPath);
		delete[] parentPath;
		if (resultNested == ERROR_SUCCESS) {
			// Try again now that the parent path exists.
			return EnsureDirectoryExists(path);
		}
		return resultNested;
	}
	return errorMkdir;
}

void CustomMemCpy(void *dst, void *src, rsize_t len, bool direction_ascending)
{
	if (direction_ascending) {
		for (rsize_t i = 0; i < len; i++) {
			((uint8_t*)dst)[i] = ((uint8_t*)src)[i];
		}
	}
	else {
		if (len > 0) {
			for (rsize_t i = len - 1; true; i--) {
				((uint8_t*)dst)[i] = ((uint8_t*)src)[i];
				if (i == 0) {
					break;
				}
			}
		}
	}
}
