#pragma once
#include "xdefs.hpp"
#include "xlive.hpp"
#include <map>
#include <set>

//
// The XHV worker thread will initially run on this processor
//

#define XHV_WORKER_THREAD_PROCESSOR             (1)
#define XHV_STACK_SIZE                          (0x10000)   // 64KB

#define XHV_MAX_REMOTE_TALKERS                  (30)
//#define XHV_MAX_LOCAL_TALKERS                   (1) // GFWL
//#define XHV_MAX_LOCAL_TALKERS                   (4) // XBOX
#define XHV_MAX_LOCAL_TALKERS                   (XLIVE_LOCAL_USER_COUNT)

//
// At the moment, you may only have up to this many enabled processing modes
// for local and remote talkers (separately).
//

#define XHV_MAX_PROCESSING_MODES                (2)

//
// When setting playback priorities, talker pairs between
// XHV_PLAYBACK_PRIORITY_MAX and XHV_PLAYBACK_PRIORITY_MIN, inclusive, will be
// heard, while any other value will result in muting that pair.
//

#define XHV_PLAYBACK_PRIORITY_MAX               (0)
#define XHV_PLAYBACK_PRIORITY_MIN               (0xFFFF)
#define XHV_PLAYBACK_PRIORITY_NEVER             (0xFFFFFFFF)

//
// Each packet reported by voice chat mode is the following size (including the
// XHV_CODEC_HEADER)
//

//#define XHV_VOICECHAT_MODE_PACKET_SIZE          ?
#define XHV_VOICECHAT_MODE_PACKET_SIZE_OLD      (10)
#define XHV_VOICECHAT_MODE_PACKET_SIZE_NEW      (42)

//
// When supplying a buffer to GetLocalChatData, you won't have to supply a
// buffer any larger than the following number of packets (or
// XHV_MAX_VOICECHAT_PACKETS * XHV_VOICECHAT_MODE_PACKET_SIZE bytes)
//

#define XHV_MAX_VOICECHAT_PACKETS               (10)

//
// XHV's final output submix voice consists of one channel per local talker.
// Each channel contains the mixed voice for its user index.  The data can be
// mapped to speakers or headsets.
//

#define XHV_XAUDIO_OUTPUT_CHANNEL_COUNT         (XHV_MAX_LOCAL_TALKERS)

//
// XHV uses three stages of submix voices.  Hence, XAudio's maximum stage count
// must be set to at least this value.
//

#define XHV_XAUDIO_SUBMIX_STAGE_COUNT           (3)

//
// XHV requires the use of a custom XAudio effect.  This effect has the
// following Effect ID.  Title effects should not use this ID.
//

#define XHV_XAUDIO_OUTPUT_EFFECTID              (XAUDIOFXID_STATICMAX)

//
// The number of effects in any chain passed into XHV is limited to this many.
//

#define XHV_MAX_FXCHAIN_EFFECTS                 (1)

//
// The microphone callback is given PCM data in this format.  Note that XAudio
// effect chains will process the data in the native XAudio format (see XAudio
// for details).
//

#define XHV_PCM_BYTES_PER_SAMPLE                (2)
#define XHV_PCM_SAMPLE_RATE                     (16000)

//
// Data Ready Flags.  These flags are set when there is local data waiting to be
// consumed (e.g. through GetLocalChatData).  GetLocalDataFlags() allows you to
// get the current state of these flags without entering XHV's critical section.
// Each mask is 4 bits, one for each local talker.  The least significant bit in
// each section indicates data is available for user index 0, while the most
// significant bit indicates user index 3.
//

#define XHV_VOICECHAT_DATA_READY_MASK           (0xF)
#define XHV_VOICECHAT_DATA_READY_OFFSET         (0)

//
// Typedefs, Enums and Structures
//

//typedef CONST LPVOID                            XHV_PROCESSING_MODE, *PXHV_PROCESSING_MODE;
typedef DWORD                                   XHV_PROCESSING_MODE, *PXHV_PROCESSING_MODE;

typedef DWORD                                   XHV_PLAYBACK_PRIORITY;
typedef VOID(*PFNMICRAWDATAREADY)(
	IN  DWORD                                   dwUserIndex,
	IN  PVOID                                   pvData,
	IN  DWORD                                   dwSize,
	IN  PBOOL                                   pVoiceDetected
);

typedef DWORD                                   XHV_LOCK_TYPE;

#define XHV_LOCK_TYPE_LOCK                      0
#define XHV_LOCK_TYPE_TRYLOCK                   1
#define XHV_LOCK_TYPE_UNLOCK                    2
#define XHV_LOCK_TYPE_COUNT                     3

typedef VOID *XAUDIOVOICEFXCHAIN;
typedef VOID *XAUDIOSUBMIXVOICE;

//
// Supported processing modes
//

//extern  CONST LPVOID                            _xhv_loopback_mode;
//extern  CONST LPVOID                            _xhv_voicechat_mode;

//#define XHV_LOOPBACK_MODE                       _xhv_loopback_mode
//#define XHV_VOICECHAT_MODE                      _xhv_voicechat_mode

#define XHV_LOOPBACK_MODE                       1
#define XHV_VOICECHAT_MODE                      2

//
// You must specify the following initialization parameters at creation time
//

typedef struct XHV_INIT_PARAMS
{
	DWORD                                       dwMaxRemoteTalkers;
	DWORD                                       dwMaxLocalTalkers;
	PXHV_PROCESSING_MODE                        localTalkerEnabledModes;
	DWORD                                       dwNumLocalTalkerEnabledModes;
	PXHV_PROCESSING_MODE                        remoteTalkerEnabledModes;
	DWORD                                       dwNumRemoteTalkerEnabledModes;
	BOOL                                        bCustomVADProvided;
	BOOL                                        bRelaxPrivileges;
	PFNMICRAWDATAREADY                          pfnMicrophoneRawDataReady;
	// These existed for the xbox version.
	//LPCXAUDIOVOICEFXCHAIN                       pfxDefaultRemoteTalkerFX;
	//LPCXAUDIOVOICEFXCHAIN                       pfxDefaultTalkerPairFX;
	//LPCXAUDIOVOICEFXCHAIN                       pfxOutputFX;
	HWND                                        hwndFocus;
} XHV_INIT_PARAMS, *PXHV_INIT_PARAMS;

//
// This header appears at the beginning of each blob of data reported by voice
// chat mode
//

#pragma pack(push, 1)
typedef struct XHV_CODEC_HEADER
{
    WORD                                        bMsgNo :  4;
    WORD                                        wSeqNo : 11;
    WORD                                        bFriendsOnly : 1;
} XHV_CODEC_HEADER, *PXHV_CODEC_HEADER;
#pragma pack (pop)

//
//  IXHVEngine interface
//

#pragma pack(push, 4)

class IXHVEngine
{
	public:
		virtual LONG __stdcall AddRef() = 0;
		virtual LONG __stdcall Release() = 0;
		virtual HRESULT __stdcall Lock(XHV_LOCK_TYPE lockType) = 0;
		virtual HRESULT __stdcall StartLocalProcessingModes(DWORD dwUserIndex, CONST XHV_PROCESSING_MODE *processingModes, DWORD dwNumProcessingModes) = 0;
		virtual HRESULT __stdcall StopLocalProcessingModes(DWORD dwUserIndex, CONST XHV_PROCESSING_MODE *processingModes, DWORD dwNumProcessingModes) = 0;
		virtual HRESULT __stdcall StartRemoteProcessingModes(XUID xuidRemoteTalker, CONST XHV_PROCESSING_MODE *processingModes, DWORD dwNumProcessingModes) = 0;
		virtual HRESULT __stdcall StopRemoteProcessingModes(XUID xuidRemoteTalker, CONST XHV_PROCESSING_MODE *processingModes, DWORD dwNumProcessingModes) = 0;
		virtual HRESULT __stdcall SetMaxDecodePackets(DWORD dwMaxDecodePackets) = 0;
		virtual HRESULT __stdcall RegisterLocalTalker(DWORD dwUserIndex) = 0;
		virtual HRESULT __stdcall UnregisterLocalTalker(DWORD dwUserIndex) = 0;
		virtual HRESULT __stdcall RegisterRemoteTalker(XUID xuidRemoteTalker, XAUDIOVOICEFXCHAIN *pfxRemoteTalkerFX, XAUDIOVOICEFXCHAIN *pfxTalkerPairFX, XAUDIOSUBMIXVOICE *pOutputVoice) = 0;
		virtual HRESULT __stdcall UnregisterRemoteTalker(XUID xuidRemoteTalker) = 0;
		virtual HRESULT __stdcall GetRemoteTalkers(DWORD *pdwRemoteTalkersCount, XUID *pxuidRemoteTalkers) = 0;
		virtual BOOL __stdcall IsHeadsetPresent(DWORD dwUserIndex) = 0;
		virtual BOOL __stdcall IsLocalTalking(DWORD dwUserIndex) = 0;
		virtual BOOL __stdcall isRemoteTalking(XUID xuidRemoteTalker) = 0;
		virtual DWORD __stdcall GetDataReadyFlags() = 0;
		virtual HRESULT __stdcall GetLocalChatData(DWORD dwUserIndex, BYTE *pbData, DWORD *pdwSize, DWORD *pdwPackets) = 0;
		virtual HRESULT __stdcall SetPlaybackPriority(XUID xuidRemoteTalker, DWORD dwUserIndex, XHV_PLAYBACK_PRIORITY playbackPriority) = 0;
		virtual HRESULT __stdcall SubmitIncomingChatData(XUID xuidRemoteTalker, CONST BYTE *pbData, DWORD *pdwSize) = 0;
};

typedef IXHVEngine                          *LPIXHVENGINE, *PIXHVENGINE;

typedef struct {
	std::set<XHV_PROCESSING_MODE> processing_modes;
	bool is_headset_present = false;
	bool is_talking = false;
} VOICE_REGISTERED_USER_LOCAL;

typedef struct {
	std::set<XHV_PROCESSING_MODE> processing_modes;
	bool is_talking = false;
} VOICE_REGISTERED_USER_REMOTE;

class XHVEngine : public IXHVEngine
{
	public:
		__stdcall XHVEngine(XHV_INIT_PARAMS *init_params);
		__stdcall ~XHVEngine();
		
		virtual LONG __stdcall AddRef();
		virtual LONG __stdcall Release();
		virtual HRESULT __stdcall Lock(XHV_LOCK_TYPE lockType);
		virtual HRESULT __stdcall StartLocalProcessingModes(DWORD dwUserIndex, CONST XHV_PROCESSING_MODE *processingModes, DWORD dwNumProcessingModes);
		virtual HRESULT __stdcall StopLocalProcessingModes(DWORD dwUserIndex, CONST XHV_PROCESSING_MODE *processingModes, DWORD dwNumProcessingModes);
		virtual HRESULT __stdcall StartRemoteProcessingModes(XUID xuidRemoteTalker, CONST XHV_PROCESSING_MODE *processingModes, DWORD dwNumProcessingModes);
		virtual HRESULT __stdcall StopRemoteProcessingModes(XUID xuidRemoteTalker, CONST XHV_PROCESSING_MODE *processingModes, DWORD dwNumProcessingModes);
		virtual HRESULT __stdcall SetMaxDecodePackets(DWORD dwMaxDecodePackets);
		virtual HRESULT __stdcall RegisterLocalTalker(DWORD dwUserIndex);
		virtual HRESULT __stdcall UnregisterLocalTalker(DWORD dwUserIndex);
		virtual HRESULT __stdcall RegisterRemoteTalker(XUID xuidRemoteTalker, XAUDIOVOICEFXCHAIN *pfxRemoteTalkerFX, XAUDIOVOICEFXCHAIN *pfxTalkerPairFX, XAUDIOSUBMIXVOICE *pOutputVoice);
		virtual HRESULT __stdcall UnregisterRemoteTalker(XUID xuidRemoteTalker);
		virtual HRESULT __stdcall GetRemoteTalkers(DWORD *pdwRemoteTalkersCount, XUID *pxuidRemoteTalkers);
		virtual BOOL __stdcall IsHeadsetPresent(DWORD dwUserIndex);
		virtual BOOL __stdcall IsLocalTalking(DWORD dwUserIndex);
		virtual BOOL __stdcall isRemoteTalking(XUID xuidRemoteTalker);
		virtual DWORD __stdcall GetDataReadyFlags();
		virtual HRESULT __stdcall GetLocalChatData(DWORD dwUserIndex, BYTE *pbData, DWORD *pdwSize, DWORD *pdwPackets);
		virtual HRESULT __stdcall SetPlaybackPriority(XUID xuidRemoteTalker, DWORD dwUserIndex, XHV_PLAYBACK_PRIORITY playbackPriority);
		virtual HRESULT __stdcall SubmitIncomingChatData(XUID xuidRemoteTalker, CONST BYTE *pbData, DWORD *pdwSize);
		
	private:
		CRITICAL_SECTION xhv_lock;
		LONG xhv_reference_count;
		
		uint32_t registered_users_max_local;
		uint32_t registered_users_max_remote;
		bool relax_privileges;
		PFNMICRAWDATAREADY pfnMicrophoneRawDataReady;
		HWND hwnd_title_focus;
		std::set<XHV_PROCESSING_MODE> processing_modes_enabled_local;
		std::set<XHV_PROCESSING_MODE> processing_modes_enabled_remote;
		
		// Key: user_index.
		std::map<uint32_t, VOICE_REGISTERED_USER_LOCAL*> registered_users_local;
		// Key: xuid_remote_user.
		std::map<uint64_t, VOICE_REGISTERED_USER_REMOTE*> registered_users_remote;
		
		HRESULT __stdcall StartStopLocalProcessingModes(bool start, DWORD dwUserIndex, CONST XHV_PROCESSING_MODE *processingModes, DWORD dwNumProcessingModes);
		HRESULT __stdcall StartStopRemoteProcessingModes(bool start, XUID xuidRemoteTalker, CONST XHV_PROCESSING_MODE *processingModes, DWORD dwNumProcessingModes);
};

#pragma pack (pop)

extern bool xlive_xhv_engine_enabled;
extern bool xlive_xhv_engine_version_new;
