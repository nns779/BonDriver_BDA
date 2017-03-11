//------------------------------------------------------------------------------
// File: BonTuner.h
//   Define CBonTuner class
//------------------------------------------------------------------------------
#pragma once

#include <Windows.h>
#include <stdio.h>

#include <list>
#include <vector>
#include <map>
#include <deque>

#include "IBonDriver2.h"
#include "IBdaSpecials2.h"

#include "DSFilterEnum.h"

#include "LockChannel.h"

#include <iostream>
#include <dshow.h>

#include <tuner.h>

#include "common.h"

// transform()
#include <algorithm>

class CTsWriter;

// CBonTuner class
////////////////////////////////
class CBonTuner : public IBonDriver2
{
public:
	////////////////////////////////////////
	// �R���X�g���N�^ & �f�X�g���N�^
	////////////////////////////////////////
	CBonTuner();
	virtual ~CBonTuner();

	////////////////////////////////////////
	// IBonDriver �����o�֐�
	////////////////////////////////////////
	const BOOL OpenTuner(void);
	void CloseTuner(void);

	const BOOL SetChannel(const BYTE byCh);
	const float GetSignalLevel(void);

	const DWORD WaitTsStream(const DWORD dwTimeOut = 0);
	const DWORD GetReadyCount(void);

	const BOOL GetTsStream(BYTE *pDst, DWORD *pdwSize, DWORD *pdwRemain);
	const BOOL GetTsStream(BYTE **ppDst, DWORD *pdwSize, DWORD *pdwRemain);

	void PurgeTsStream(void);

	////////////////////////////////////////
	// IBonDriver2 �����o�֐�
	////////////////////////////////////////
	LPCTSTR GetTunerName(void);

	const BOOL IsTunerOpening(void);

	LPCTSTR EnumTuningSpace(const DWORD dwSpace);
	LPCTSTR EnumChannelName(const DWORD dwSpace, const DWORD dwChannel);

	const BOOL SetChannel(const DWORD dwSpace, const DWORD dwChannel);

	const DWORD GetCurSpace(void);
	const DWORD GetCurChannel(void);

	void Release(void);
	
	////////////////////////////////////////
	// �ÓI�����o�ϐ�
	////////////////////////////////////////

	// Dll�̃��W���[���n���h��
	static HMODULE st_hModule;

	// �쐬���ꂽCBontuner�C���X�^���X�̈ꗗ
	static std::list<CBonTuner*> st_InstanceList;

	// st_InstanceList����p
	static CRITICAL_SECTION st_LockInstanceList;

protected:
	////////////////////////////////////////
	// ���������o�֐�
	////////////////////////////////////////

	// COM������p�X���b�h
	static DWORD WINAPI COMProcThread(LPVOID lpParameter);

	// Decode������p�X���b�h
	static DWORD WINAPI DecodeProcThread(LPVOID lpParameter);

	// TsWriter �R�[���o�b�N�֐�
	static int CALLBACK RecvProc(void* pParam, BYTE* pbData, DWORD dwSize);

	// �f�[�^��M�X�^�[�g�E��~
	void StartRecv(void);
	void StopRecv(void);

	// ini �t�@�C���Ǎ�
	void ReadIniFile(void);

	// �M����Ԃ��擾
	void GetSignalState(int* pnStrength, int* pnQuality, int* pnLock);

	// �`�����l���ؑ�
	BOOL LockChannel(const TuningParam *pTuningParam, BOOL bLockTwice);

	// �`���[�i�ŗLDll�̃��[�h
	HRESULT CheckAndInitTunerDependDll(std::wstring tunerGUID, std::wstring tunerFriendlyName);

	// �`���[�i�ŗLDll�ł̃L���v�`���f�o�C�X�m�F
	HRESULT CheckCapture(std::wstring tunerGUID, std::wstring tunerFriendlyName, std::wstring captureGUID, std::wstring captureFriendlyName);
		
	// �`���[�i�ŗL�֐��̃��[�h
	void LoadTunerDependCode(void);

	// �`���[�i�ŗL�֐���Dll�̉��
	void ReleaseTunerDependCode(void);

	// GraphBuilder
	HRESULT InitializeGraphBuilder(void);
	void CleanupGraph(void);
	HRESULT RunGraph(void);
	void StopGraph(void);

	// TuningSpace
	HRESULT CreateTuningSpace(void);
	void UnloadTuningSpace(void);
	HRESULT InitTuningSpace(void);

	// NetworkProvider
	HRESULT LoadNetworkProvider(void);
	void UnloadNetworkProvider(void);

	// �`���[�i�E�L���v�`���f�o�C�X�̓Ǎ��݃��X�g�擾
	HRESULT InitDSFilterEnum(void);

	// �`���[�i�E�L���v�`���f�o�C�X���܂߂Ă��ׂẴt�B���^�O���t�����[�h����Run�����݂�
	HRESULT LoadAndConnectDevice(void);

	// TunerDevice
	void UnloadTunerDevice(void);
	
	// CaptureDevice
	void UnloadCaptureDevice(void);
	
	// TsWriter
	HRESULT LoadAndConnectTsWriter(void);
	void UnloadTsWriter(void);

	// Demultiplexer
	HRESULT LoadAndConnectDemux(void);
	void UnloadDemux(void);
	
	// TIF (Transport Information Filter)
	HRESULT LoadAndConnectTif(void);
	void UnloadTif(void);

	// TsWriter/Demultiplexer/TIF��Load&Connect��Run����
	HRESULT LoadAndConnectMiscFilters(void);

	// �`���[�i�M����Ԏ擾�p�C���^�[�t�F�[�X
	HRESULT LoadTunerSignalStatistics(void);
	void UnloadTunerSignalStatistics(void);

	// Pin �̐ڑ�
	HRESULT Connect(const WCHAR* pszName, IBaseFilter* pFrom, IBaseFilter* pTo);

	// �S�Ă� Pin ��ؒf����
	void DisconnectAll(IBaseFilter* pFilter);

	// CCOM������p�X���b�h����Ăяo�����֐�
	const BOOL _OpenTuner(void);
	void _CloseTuner(void);
	const BOOL _SetChannel(const DWORD dwSpace, const DWORD dwChannel);
	const float _GetSignalLevel(void);
	const BOOL _IsTunerOpening(void);
	const DWORD _GetCurSpace(void);
	const DWORD _GetCurChannel(void);


protected:
	////////////////////////////////////////
	// �����o�ϐ�
	////////////////////////////////////////

	////////////////////////////////////////
	// COM������p�X���b�h�p
	////////////////////////////////////////

	enum enumCOMRequest {
		eCOMReqOpenTuner = 1,
		eCOMReqCloseTuner,
		eCOMReqSetChannel,
		eCOMReqGetSignalLevel,
		eCOMReqIsTunerOpening,
		eCOMReqGetCurSpace,
		eCOMReqGetCurChannel,
	};

	struct COMReqParamSetChannel {
		DWORD dwSpace;
		DWORD dwChannel;
	};

	union COMReqParm {
		COMReqParamSetChannel SetChannel;
	};

	union COMReqRetVal {
		BOOL OpenTuner;
		BOOL SetChannel;
		float GetSignalLevel;
		BOOL IsTunerOpening;
		DWORD GetCurSpace;
		DWORD GetCurChannel;
	};

	struct COMProc {
		HANDLE hThread;					// �X���b�h�n���h��
		HANDLE hReqEvent;				// COMProc�X���b�h�ւ̃R�}���h���s�v��
		HANDLE hEndEvent;				// COMProc�X���b�h����̃R�}���h�����ʒm
		CRITICAL_SECTION csLock;		// �r���p
		enumCOMRequest nRequest;		// ���N�G�X�g
		COMReqParm uParam;				// �p�����[�^
		COMReqRetVal uRetVal;			// �߂�l
		DWORD dwTick;					// ���݂�TickCount
		DWORD dwTickLastCheck;			// �Ō�Ɉُ�Ď��̊m�F���s����TickCount
		DWORD dwTickSignalLockErr;		// SignalLock�ُ̈픭��TickCount
		DWORD dwTickBitRateErr;			// BitRate�ُ̈픭��TckCount
		BOOL bSignalLockErr;			// SignalLock�ُ̈픭����Flag
		BOOL bBitRateErr;				// BitRate�ُ̈픭����Flag
		BOOL bDoReLockChannel;			// �`�����l�����b�N�Ď��s��
		BOOL bDoReOpenTuner;			// �`���[�i�[�ăI�[�v����
		unsigned int nReLockFailCount;	// Re-LockChannel���s��
		DWORD dwReOpenSpace;			// �`���[�i�[�ăI�[�v�����̃J�����g�`���[�j���O�X�y�[�X�ԍ��ޔ�
		DWORD dwReOpenChannel;			// �`���[�i�[�ăI�[�v�����̃J�����g�`�����l���ԍ��ޔ�
		HANDLE hTerminateRequest;		// �X���b�h�I���v��
		
		COMProc(void)
			: hThread(NULL),
			  hReqEvent(NULL),
			  hEndEvent(NULL),
			  hTerminateRequest(NULL),
			  dwTick(0),
			  dwTickLastCheck(0),
			  dwTickSignalLockErr(0),
			  dwTickBitRateErr(0),
			  bSignalLockErr(FALSE),
			  bBitRateErr(FALSE),
			  bDoReLockChannel(FALSE),
			  bDoReOpenTuner(FALSE),
			  dwReOpenSpace(CBonTuner::SPACE_INVALID),
			  dwReOpenChannel(CBonTuner::CHANNEL_INVALID),
			  nReLockFailCount(0)
		{
			hReqEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
			hEndEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
			hTerminateRequest = ::CreateEvent(NULL, FALSE, FALSE, NULL);
			::InitializeCriticalSection(&csLock);
		};
		
		~COMProc(void)
		{
			::CloseHandle(hReqEvent);
			hReqEvent = NULL;
			::CloseHandle(hEndEvent);
			hEndEvent = NULL;
			::CloseHandle(hTerminateRequest);
			hTerminateRequest = NULL;
			::DeleteCriticalSection(&csLock);
		};
		
		inline BOOL CheckTick(void)
		{
			dwTick = ::GetTickCount();
			if (dwTick - dwTickLastCheck > 1000) {
				dwTickLastCheck = dwTick;
				return TRUE;
			}
			return FALSE;
		}
		
		inline void ResetWatchDog(void)
		{
			bSignalLockErr = FALSE;
			bBitRateErr = FALSE;
		}

		inline BOOL CheckSignalLockErr(BOOL state, DWORD threshold)
		{
			if (state) {
				//����
				bSignalLockErr = FALSE;
			} else {
				// �ُ�
				if (!bSignalLockErr) {
					// ���񔭐�
					bSignalLockErr = TRUE;
					dwTickSignalLockErr = dwTick;
				}
				else {
					// �O��ȑO�ɔ������Ă���
					if ((dwTick - dwTickSignalLockErr) > threshold) {
						// �ݒ莞�Ԉȏ�o�߂��Ă���
						ResetWatchDog();
						return TRUE;
					}
				}
			}
			return FALSE;
		}
		
		inline BOOL CheckBitRateErr(BOOL state, DWORD threshold)
		{
			if (state) {
				//����
				bSignalLockErr = FALSE;
			}
			else {
				// �ُ�
				if (!bBitRateErr) {
					// ���񔭐�
					bBitRateErr = TRUE;
					dwTickBitRateErr = dwTick;
				}
				else {
					// �O��ȑO�ɔ������Ă���
					if ((dwTick - dwTickBitRateErr) > threshold) {
						// �ݒ莞�Ԉȏ�o�߂��Ă���
						ResetWatchDog();
						return TRUE;
					}
				}
			}
			return FALSE;
		}

		inline void SetReLockChannel(void)
		{
			bDoReLockChannel = TRUE;
			nReLockFailCount = 0;
		}
		
		inline void ResetReLockChannel(void)
		{
			bDoReLockChannel = FALSE;
			nReLockFailCount = 0;
		}
		
		inline BOOL CheckReLockFailCount(unsigned int threshold)
		{
			return (++nReLockFailCount >= threshold);
		}
		
		inline void SetReOpenTuner(DWORD space, DWORD channel)
		{
			bDoReOpenTuner = TRUE;
			dwReOpenSpace = space;
			dwReOpenChannel = channel;
		}
		
		inline void ClearReOpenChannel(void)
		{
			dwReOpenSpace = CBonTuner::SPACE_INVALID;
			dwReOpenChannel = CBonTuner::CHANNEL_INVALID;
		}
		
		inline BOOL CheckReOpenChannel(void)
		{
			return (dwReOpenSpace != CBonTuner::SPACE_INVALID && dwReOpenChannel != CBonTuner::CHANNEL_INVALID);
		}

		inline void ResetReOpenTuner(void)
		{
			bDoReOpenTuner = FALSE;
			ClearReOpenChannel();
		}
	};
	COMProc m_aCOMProc;

	////////////////////////////////////////
	// Decode������p�X���b�h�p
	////////////////////////////////////////

	struct DecodeProc {
		HANDLE hThread;					// �X���b�h�n���h��
		LONG lTerminateFlag;
		DecodeProc(void)
			: hThread(NULL),
			lTerminateFlag(0)
		{
		};
		~DecodeProc(void)
		{
			lTerminateFlag = 0;
		};
	};
	DecodeProc m_aDecodeProc;

	////////////////////////////////////////
	// �`���[�i�p�����[�^�֌W
	////////////////////////////////////////

	// INI�t�@�C���Ŏw��ł���GUID/FriendlyName�ő吔
	static const unsigned int MAX_GUID = 100;

	// �`���[�i�E�L���v�`�������Ɏg�p����GUID�������FriendlyName������̑g����
	struct TunerSearchData {
		std::wstring TunerGUID;
		std::wstring TunerFriendlyName;
		std::wstring CaptureGUID;
		std::wstring CaptureFriendlyName;
		TunerSearchData(void)
		{
		};
		TunerSearchData(WCHAR* tunerGuid, WCHAR* tunerFriendlyName, WCHAR* captureGuid, WCHAR* captureFriendlyName)
			: TunerGUID(tunerGuid),
			TunerFriendlyName(tunerFriendlyName),
			CaptureGUID(captureGuid),
			CaptureFriendlyName(captureFriendlyName)
		{
			std::transform(TunerGUID.begin(), TunerGUID.end(), TunerGUID.begin(), towlower);
			std::transform(CaptureGUID.begin(), CaptureGUID.end(), CaptureGUID.begin(), towlower);
		};
	};

	// INI �t�@�C���Ŏw�肷��`���[�i�p�����[�^
	struct TunerParam {
		std::map<unsigned int, TunerSearchData*> Tuner;
												// Tuner��Capture��GUID/FriendlyName�w��
		BOOL bNotExistCaptureDevice;			// Tuner�f�o�C�X�݂̂�Capture�f�o�C�X�����݂��Ȃ��ꍇTRUE
		BOOL bCheckDeviceInstancePath;			// Tuner��Capture�̃f�o�C�X�C���X�^���X�p�X����v���Ă��邩�̊m�F���s�����ǂ���
#ifdef UNICODE
		std::wstring sTunerName;						// GetTunerName�ŕԂ����O
#else
		string sTunerName;						// GetTunerName�ŕԂ����O
#endif
		std::wstring sDLLBaseName;					// �ŗLDLL
		TunerParam(void)
			: bNotExistCaptureDevice(TRUE),
			  bCheckDeviceInstancePath(TRUE)
		{
		};
		~TunerParam(void)
		{
			for (std::map<unsigned int, TunerSearchData*>::iterator it = Tuner.begin(); it != Tuner.end(); it++) {
				SAFE_DELETE(it->second);
			}
			Tuner.clear();
		};
	};
	TunerParam m_aTunerParam;

	// Tone�M���ؑ֎���Wait����
	unsigned int m_nToneWait;

	// CH�ؑ֌��Lock�m�F����
	unsigned int m_nLockWait;

	// CH�ؑ֌��Lock�m�FDelay����
	unsigned int m_nLockWaitDelay;

	// CH�ؑ֌��Lock�m�FRetry��
	unsigned int m_nLockWaitRetry;

	// CH�ؑ֓���������I��2�x�s�����ǂ���
	BOOL m_bLockTwice;

	// CH�ؑ֓���������I��2�x�s���ꍇ��Delay����(msec)
	unsigned int m_nLockTwiceDelay;

	// SignalLocked�̊Ď�����(msec) 0�ŊĎ����Ȃ�
	unsigned int m_nWatchDogSignalLocked;

	// BitRate�̊Ď�����(msec) 0�ŊĎ����Ȃ�
	unsigned int m_nWatchDogBitRate;

	// �ُ팟�m���A�`���[�i�̍ăI�[�v�������݂�܂ł�CH�ؑ֓��쎎�s��
	unsigned int m_nReOpenWhenGiveUpReLock;

	// �`���[�i�̍ăI�[�v�������݂�ꍇ�ɕʂ̃`���[�i��D�悵�Č������邩�ǂ���
	BOOL m_bTryAnotherTuner;

	// CH�ؑւɎ��s�����ꍇ�ɁA�ُ팟�m�����l�o�b�N�O�����h��CH�ؑ֓�����s�����ǂ���
	BOOL m_bBackgroundChannelLock;

	// SignalLevel �Z�o���@
	//   0 .. IBDA_SignalStatistics::get_SignalStrength�Ŏ擾�����l �� StrengthCoefficient�Ŏw�肵�����l �{ StrengthBias�Ŏw�肵�����l
	//   1 .. IBDA_SignalStatistics::get_SignalQuality�Ŏ擾�����l �� QualityCoefficient�Ŏw�肵�����l �{ QualityBias�Ŏw�肵�����l
	//   2 .. (IBDA_SignalStatistics::get_SignalStrength �� StrengthCoefficient �{ StrengthBias) �~ (IBDA_SignalStatistics::get_SignalQuality �� QualityCoefficient �{ QualityBias)
	//   3 .. (IBDA_SignalStatistics::get_SignalStrength �� StrengthCoefficient �{ StrengthBias) �{ (IBDA_SignalStatistics::get_SignalQuality �� QualityCoefficient �{ QualityBias)
	//  10 .. ITuner::get_SignalStrength�Ŏ擾����Strength�l �� StrengthCoefficient�Ŏw�肵�����l �{ StrengthBias�Ŏw�肵�����l
	//  11 .. ITuner::get_SignalStrength�Ŏ擾����Quality�l �� QualityCoefficient�Ŏw�肵�����l �{ QualityBias�Ŏw�肵�����l
	//  12 .. (ITuner::get_SignalStrength��Strength�l �� StrengthCoefficient �{ StrengthBias) �~ (ITuner::get_SignalStrength��Quality�l �� QualityCoefficient �{ QualityBias)
	//  13 .. (ITuner::get_SignalStrength��Strength�l �� StrengthCoefficient �{ StrengthBias) �{ (ITuner::get_SignalStrength��Quality�l �� QualityCoefficient �{ QualityBias)
	// 100 .. �r�b�g���[�g�l(Mibps)
	unsigned int m_nSignalLevelCalcType;
	BOOL m_bSignalLevelGetTypeSS;		// SignalLevel �Z�o�� IBDA_SignalStatistics ���g�p����
	BOOL m_bSignalLevelGetTypeTuner;	// SignalLevel �Z�o�� ITuner ���g�p����
	BOOL m_bSignalLevelGetTypeBR;		// SignalLevel �Z�o�� �r�b�g���[�g�l���g�p����
	BOOL m_bSignalLevelNeedStrength;	// SignalLevel �Z�o�� SignalStrength �l���g�p����
	BOOL m_bSignalLevelNeedQuality;		// SignalLevel �Z�o�� SignalQuality �l���g�p����
	BOOL m_bSignalLevelCalcTypeMul;		// SignalLevel �Z�o�� SignalStrength �� SignalQuality �̊|���Z���g�p����
	BOOL m_bSignalLevelCalcTypeAdd;		// SignalLevel �Z�o�� SignalStrength �� SignalQuality �̑����Z���g�p����

	// Strength �l�␳�W��
	float m_fStrengthCoefficient;

	// Quality �l�␳�W��
	float m_fQualityCoefficient;

	// Strength �l�␳�o�C�A�X
	float m_fStrengthBias;

	// Quality �l�␳�o�C�A�X
	float m_fQualityBias;

	// �`���[�j���O��Ԃ̔��f���@
	// 0 .. ��Ƀ`���[�j���O�ɐ������Ă����ԂƂ��Ĕ��f����
	// 1 .. IBDA_SignalStatistics::get_SignalLocked�Ŏ擾�����l�Ŕ��f����
	// 2 .. ITuner::get_SignalStrength�Ŏ擾�����l�Ŕ��f����
	unsigned int m_nSignalLockedJudgeType;
	BOOL m_bSignalLockedJudgeTypeSS;	// �`���[�j���O��Ԃ̔��f�� IBDA_SignalStatistics ���g�p����
	BOOL m_bSignalLockedJudgeTypeTuner;	// �`���[�j���O��Ԃ̔��f�� ITuner ���g�p����

	////////////////////////////////////////
	// BonDriver �p�����[�^�֌W
	////////////////////////////////////////

	// �o�b�t�@1������̃T�C�Y
	DWORD m_dwBuffSize;

	// �ő�o�b�t�@��
	DWORD m_dwMaxBuffCount;

	// �ŏ��o�b�t�@��
	DWORD m_dwMinBuffCount;

	// m_hOnDecodeEvent���Z�b�g����f�[�^�o�b�t�@��
	unsigned int m_nWaitTsCount;

	// WaitTsStream�ōŒ���ҋ@���鎞��
	unsigned int m_nWaitTsSleep;

	// SetChannel()�Ń`�����l�����b�N�Ɏ��s�����ꍇ�ł�FALSE��Ԃ��Ȃ��悤�ɂ��邩�ǂ���
	BOOL m_bAlwaysAnswerLocked;

	////////////////////////////////////////
	// �`�����l���p�����[�^
	////////////////////////////////////////

	// �`�����l���f�[�^
	struct ChData {
#ifdef UNICODE
		std::wstring sServiceName;
#else
		string sServiceName;
#endif
		unsigned int Satellite;			// �q����M�ݒ�ԍ�
		unsigned int Polarisation;		// �Δg��ޔԍ� (0 .. ���w��, 1 .. H, 2 .. V, 3 .. L, 4 .. R)
		unsigned int ModulationType;	// �ϒ������ݒ�ԍ�
		long Frequency;					// ���g��(KHz)
		union {
			long SID;					// �T�[�r�XID
			long PhysicalChannel;		// ATSC / Digital Cable�p
		};
		union {
			long TSID;					// �g�����X�|�[�g�X�g���[��ID
			long Channel;				// ATSC / Digital Cable�p
		};
		union {
			long ONID;					// �I���W�i���l�b�g���[�NID
			long MinorChannel;			// ATSC / Digital Cable�p
		};
		long MajorChannel;				// Digital Cable�p
		long SourceID;					// Digital Cable�p
		BOOL LockTwiceTarget;			// CH�ؑ֓���������I��2�x�s���Ώ�
		ChData(void)
			: Satellite(0),
			  Polarisation(0),
			  ModulationType(0),
			  Frequency(0),
			  SID(-1),
			  TSID(-1),
			  ONID(-1),
			  MajorChannel(-1),
			  SourceID(-1),
			  LockTwiceTarget(FALSE)
		{
		};
	};

	// �`���[�j���O��ԃf�[�^
	struct TuningSpaceData {
#ifdef UNICODE
		std::wstring sTuningSpaceName;		// EnumTuningSpace�ŕԂ�Tuning Space��
#else
		string sTuningSpaceName;		// EnumTuningSpace�ŕԂ�Tuning Space��
#endif
		std::map<unsigned int, ChData*> Channels;		// �`�����l���ԍ��ƃ`�����l���f�[�^
		DWORD dwNumChannel;				// �`�����l����
		TuningSpaceData(void)
			: dwNumChannel(0)
		{
		};
		~TuningSpaceData(void)
		{
			for (std::map<unsigned int, ChData*>::iterator it = Channels.begin(); it != Channels.end(); it++) {
				SAFE_DELETE(it->second);
			}
			Channels.clear();
		};
	};

	// �`���[�j���O�X�y�[�X�ꗗ
	struct TuningData {
		std::map<unsigned int, TuningSpaceData*> Spaces;	// �`���[�j���O�X�y�[�X�ԍ��ƃf�[�^
		DWORD dwNumSpace;					// �`���[�j���O�X�y�[�X��
		TuningData(void)
			: dwNumSpace(0)
		{
		};
		~TuningData(void)
		{
			for (std::map<unsigned int, TuningSpaceData*>::iterator it = Spaces.begin(); it != Spaces.end(); it++) {
				SAFE_DELETE(it->second);
			}
			Spaces.clear();
		};
	};
	TuningData m_TuningData;

	// ini�t�@�C������CH�ݒ��Ǎ��ލۂ�
	// �g�p����Ă��Ȃ�CH�ԍ��������Ă��O�l�����m�ۂ��Ă������ǂ���
	// FALSE .. �g�p����ĂȂ��ԍ����������ꍇ�O�l���A��������
	// TRUE .. �g�p����Ă��Ȃ��ԍ������̂܂܋�CH�Ƃ��Ċm�ۂ��Ă���
	BOOL m_bReserveUnusedCh;

	////////////////////////////////////////
	// �q����M�p�����[�^
	////////////////////////////////////////

	// ini�t�@�C���Ŏ�t����Δg��ސ�
	static const unsigned int POLARISATION_SIZE = 5;

	// CBonTuner�Ŏg�p����Δg��ޔԍ���Polarisation�^��Mapping
	static const Polarisation PolarisationMapping[POLARISATION_SIZE];

	// �Δg��ޖ���ini�t�@�C���ł̋L��
	static const WCHAR PolarisationChar[POLARISATION_SIZE];

	// ini�t�@�C���Őݒ�ł���ő�q���� + 1
	static const unsigned int MAX_SATELLITE = 5;

	// �q����M�ݒ�f�[�^
	struct Satellite {
		AntennaParam Polarisation[POLARISATION_SIZE];	// �Δg��ޖ��̃A���e�i�ݒ�
	};
	Satellite m_aSatellite[MAX_SATELLITE];

	// �`�����l�����̎��������Ɏg�p����q���̖���
	std::wstring m_sSatelliteName[MAX_SATELLITE];

	////////////////////////////////////////
	// �ϒ������p�����[�^
	////////////////////////////////////////

	// ini�t�@�C���Őݒ�ł���ő�ϒ�������
	static const unsigned int MAX_MODULATION = 4;

	// �ϒ������ݒ�f�[�^
	ModulationMethod m_aModulationType[MAX_MODULATION];

	// �`�����l�����̎��������Ɏg�p����ϒ������̖���
	std::wstring m_sModulationName[MAX_MODULATION];

	////////////////////////////////////////
	// BonDriver �֘A
	////////////////////////////////////////

	// ini�t�@�C����Path
	WCHAR m_szIniFilePath[_MAX_PATH + 1];

	// TS�o�b�t�@����p
	CRITICAL_SECTION m_csTSBuff;

	// Decode������TS�o�b�t�@����p
	CRITICAL_SECTION m_csDecodedTSBuff;

	// ��M�C�x���g
	LONG m_lStreamFlag;

	// �f�R�[�h�C�x���g
	HANDLE m_hOnDecodeEvent;

	// ��MTS�f�[�^�o�b�t�@
	struct TS_DATA {
		BYTE* pbyBuff;
		DWORD dwSize;
		DWORD dwBuffSize;
		bool bAlloc;
		TS_DATA(void)
			: pbyBuff(NULL),
			  dwSize(0),
			  dwBuffSize(0),
			  bAlloc(false)
		{
		};
		TS_DATA(BYTE* buff, DWORD size)
			: dwSize(0)
		{
			if (buff) {
				pbyBuff = buff;
				bAlloc = false;
			}
			else {
				pbyBuff = new BYTE[size];
				bAlloc = true;
			}
			dwBuffSize = size;
		};
		~TS_DATA(void) {
			if (bAlloc) {
				SAFE_DELETE_ARRAY(pbyBuff);
			}
		};
		void Expand(BYTE* buff, DWORD size)
		{
			if (bAlloc) {
				SAFE_DELETE_ARRAY(pbyBuff);
			}
			if (buff) {
				pbyBuff = buff;
				bAlloc = false;
			}
			else {
				pbyBuff = new BYTE[size];
				bAlloc = true;
			}
			dwSize = 0;
			dwBuffSize = size;
		}
		DWORD Put(BYTE* data, DWORD size)
		{
			DWORD copysize;
			copysize = (dwBuffSize - dwSize > size) ? size : dwBuffSize - dwSize;
			memcpy(pbyBuff + dwSize, data, copysize);
			dwSize += copysize;
			return copysize;
		};
	};

	class TS_BUFF {
	private:
		std::deque<TS_DATA *> TsBuff;
		std::deque<TS_DATA *> FreeBuff;
		TS_DATA *BufferingItem;
		DWORD BuffSize;
		DWORD Count;
		DWORD MaxCount;
		DWORD MinCount;
		CRITICAL_SECTION cs;
	public:
		TS_BUFF(void);
		~TS_BUFF(void);
		void SetSize(DWORD dwBuffSize, DWORD dwMaxCount, DWORD dwMinCount);
		void Purge();
		BOOL AddData(BYTE *pbyData, DWORD dwSize);
		TS_DATA * Get(void);
		void Free(TS_DATA *ts);
	};

	// ��MTS�f�[�^�o�b�t�@
	TS_BUFF m_TsBuff;

	// Decode�����̏I�����TS�f�[�^�o�b�t�@
	std::deque<TS_DATA *> m_DecodedTsBuff;

	// GetTsStream�ŎQ�Ƃ����o�b�t�@
	TS_DATA* m_LastBuff;

	// �f�[�^��M��
	BOOL m_bRecvStarted;

	// �r�b�g���[�g�v�Z�p
	class BitRate {
	private:
		DWORD Rate1sec;					// 1�b�Ԃ̃��[�g���Z�p (bytes/sec)
		DWORD RateLast[5];				// ����5�b�Ԃ̃��[�g (bytes/sec)
		DWORD DataCount;				// ����5�b�Ԃ̃f�[�^�� (0�`5)
		float Rate;						// ���σr�b�g���[�g (Mibps)
		DWORD LastTick;					// �O���TickCount�l
		CRITICAL_SECTION csRate1Sec;	// nRate1sec �r���p
		CRITICAL_SECTION csRateLast;	// nRateLast �r���p

	public:
		BitRate(void)
			: Rate1sec(0),
			  RateLast(),
			  DataCount(0),
			  Rate(0.0F)
		{
			::InitializeCriticalSection(&csRate1Sec);
			::InitializeCriticalSection(&csRateLast);
			LastTick = ::GetTickCount();
		};

		~BitRate(void)
		{
			::DeleteCriticalSection(&csRateLast);
			::DeleteCriticalSection(&csRate1Sec);
		}

		inline void AddRate(DWORD Count)
		{
			::EnterCriticalSection(&csRate1Sec);
			Rate1sec += Count;
			::LeaveCriticalSection(&csRate1Sec);
		}

		inline DWORD CheckRate(void)
		{
			DWORD total = 0;
			DWORD Tick = ::GetTickCount();
			if (Tick - LastTick > 1000) {
				::EnterCriticalSection(&csRateLast);
				for (unsigned int i = (sizeof(RateLast) / sizeof(RateLast[0])) - 1; i > 0; i--) {
					RateLast[i] = RateLast[i - 1];
					total += RateLast[i];
				}
				::EnterCriticalSection(&csRate1Sec);
				RateLast[0] = Rate1sec;
				Rate1sec = 0;
				::LeaveCriticalSection(&csRate1Sec);
				total += RateLast[0];
				if (DataCount < 5)
					DataCount++;
				if (DataCount)
					Rate = ((float)total / (float)DataCount) / 131072.0F;
				LastTick = Tick;
				::LeaveCriticalSection(&csRateLast);
			}
			DWORD remain = 1000 - (Tick - LastTick);
			return (remain > 1000) ? 1000 : remain;
		}

		inline void Clear(void)
		{
			::EnterCriticalSection(&csRateLast);
			::EnterCriticalSection(&csRate1Sec);
			Rate1sec = 0;
			for (unsigned int i = 0; i < sizeof(RateLast) / sizeof(RateLast[0]); i++) {
				RateLast[i] = 0;
			}
			DataCount = 0;
			Rate = 0.0F;
			LastTick = ::GetTickCount();
			::LeaveCriticalSection(&csRate1Sec);
			::LeaveCriticalSection(&csRateLast);
		}

		inline float GetRate(void)
		{
			return Rate;
		}
	};
	BitRate m_BitRate;

	////////////////////////////////////////
	// �`���[�i�֘A
	////////////////////////////////////////

	// �`���[�i�f�o�C�X�r�������p
	HANDLE m_hSemaphore;

	// Graph
	ITuningSpace *m_pITuningSpace;
	ITuner *m_pITuner;
	IBaseFilter *m_pNetworkProvider;
	IBaseFilter *m_pTunerDevice;
	IBaseFilter *m_pCaptureDevice;
	IBaseFilter *m_pTsWriter;
	IBaseFilter *m_pDemux;
	IBaseFilter *m_pTif;
	IGraphBuilder *m_pIGraphBuilder;
	IMediaControl *m_pIMediaControl;
	CTsWriter *m_pCTsWriter;

	// �`���[�i�M����Ԏ擾�p�C���^�[�t�F�[�X
	IBDA_SignalStatistics *m_pIBDA_SignalStatistics;

	// DS�t�B���^�[�� CDSFilterEnum
	CDSFilterEnum *m_pDSFilterEnumTuner;
	CDSFilterEnum *m_pDSFilterEnumCapture;

	// DS�t�B���^�[�̏��
	struct DSListData {
		std::wstring GUID;
		std::wstring FriendlyName;
		ULONG Order;
		DSListData(std::wstring _GUID, std::wstring _FriendlyName, ULONG _Order)
			: GUID(_GUID),
			FriendlyName(_FriendlyName),
			Order(_Order)
		{
		};
	};

	// ���[�h���ׂ��`���[�i�E�L���v�`���̃��X�g
	struct TunerCaptureList {
		DSListData Tuner;
		std::vector<DSListData> CaptureList;
		TunerCaptureList(std::wstring TunerGUID, std::wstring TunerFriendlyName, ULONG TunerOrder)
			: Tuner(TunerGUID, TunerFriendlyName, TunerOrder)
		{
		};
		TunerCaptureList(DSListData _Tuner)
			: Tuner(_Tuner)
		{
		};
	};
	std::list<TunerCaptureList> m_UsableTunerCaptureList;

	// �`���[�i�[�̎g�p����TuningSpace�̎��
	enum enumTunerType {
		eTunerTypeDVBS = 1,				// DBV-S/DVB-S2
		eTunerTypeDVBT = 2,				// DVB-T
		eTunerTypeDVBC = 3,				// DVB-C
		eTunerTypeDVBT2 = 4,			// DVB-T2
		eTunerTypeISDBS = 11,			// ISDB-S
		eTunerTypeISDBT = 12,			// ISDB-T
		eTunerTypeATSC_Antenna = 21,	// ATSC
		eTunerTypeATSC_Cable = 22,		// ATSC Cable
		eTunerTypeDigitalCable = 23,	// Digital Cable
	};
	enumTunerType m_nDVBSystemType;

	// �`���[�i�[�Ɏg�p����NetworkProvider 
	enum enumNetworkProvider {
		eNetworkProviderAuto = 0,		// ����
		eNetworkProviderGeneric = 1,	// Microsoft Network Provider
		eNetworkProviderDVBS = 2,		// Microsoft DVB-S Network Provider
		eNetworkProviderDVBT = 3,		// Microsoft DVB-T Network Provider
		eNetworkProviderDVBC = 4,		// Microsoft DVB-C Network Provider
		eNetworkProviderATSC = 5,		// Microsoft ATSC Network Provider
	};
	enumNetworkProvider m_nNetworkProvider;

	// �q����M�p�����[�^/�ϒ������p�����[�^�̃f�t�H���g�l 1 .. SPHD, 2 .. BS/CS110, 3 .. UHF/CATV
	DWORD m_nDefaultNetwork;

	// Tuner is opened
	BOOL m_bOpened;

	// SetChannel()�����݂��`���[�j���O�X�y�[�X�ԍ�
	DWORD m_dwTargetSpace;

	// �J�����g�`���[�j���O�X�y�[�X�ԍ�
	DWORD m_dwCurSpace;

	// �`���[�j���O�X�y�[�X�ԍ��s��
	static const DWORD SPACE_INVALID = 0xFFFFFFFF;

	// SetChannel()�����݂��`�����l���ԍ�
	DWORD m_dwTargetChannel;

	// �J�����g�`�����l���ԍ�
	DWORD m_dwCurChannel;

	// �`�����l���ԍ��s��
	static const DWORD CHANNEL_INVALID = 0xFFFFFFFF;

	// ���݂̃g�[���ؑ֏��
	long m_nCurTone; // current tone signal state

	// �g�[���ؑ֏�ԕs��
	static const long TONE_UNKNOWN = -1;

	// �Ō��LockChannel���s�������̃`���[�j���O�p�����[�^
	TuningParam m_LastTuningParam;

	// TunerSpecial DLL module handle
	HMODULE m_hModuleTunerSpecials;

	// �`���[�i�ŗL�֐� IBdaSpecials
	IBdaSpecials *m_pIBdaSpecials;
	IBdaSpecials2a1 *m_pIBdaSpecials2;

	// �`���[�i�ŗL�̊֐����K�v���ǂ������������ʂ���DB
	// GUID ���L�[�� DLL ���𓾂�
	struct TUNER_SPECIAL_DLL {
		std::wstring sTunerGUID;
		std::wstring sDLLBaseName;
	};
	static const TUNER_SPECIAL_DLL aTunerSpecialData[];

	// �`�����l������������ inline �֐�
	inline int MakeChannelName(WCHAR* pszName, size_t size, CBonTuner::ChData* pChData)
	{
		long m = pChData->Frequency / 1000;
		long k = pChData->Frequency % 1000;
		if (k == 0)
			return ::swprintf_s(pszName, size, L"%s/%05ld%c/%s", m_sSatelliteName[pChData->Satellite].c_str(), m, PolarisationChar[pChData->Polarisation], m_sModulationName[pChData->ModulationType].c_str());
		else {
			return ::swprintf_s(pszName, size, L"%s/%05ld.%03ld%c/%s", m_sSatelliteName[pChData->Satellite].c_str(), m, k, PolarisationChar[pChData->Polarisation], m_sModulationName[pChData->ModulationType].c_str());

		}
	}
};

