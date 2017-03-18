// BonTuner.cpp: CBonTuner �N���X�̃C���v�������e�[�V����
//
//////////////////////////////////////////////////////////////////////

#include <Windows.h>
#include <stdio.h>

#include "BonTuner.h"

#include "common.h"

#include "tswriter.h"

#include <iostream>
#include <DShow.h>

// KSCATEGORY_...
#include <ks.h>
#pragma warning (push)
#pragma warning (disable: 4091)
#include <ksmedia.h>
#pragma warning (pop)
#include <bdatypes.h>
#include <bdamedia.h>

// bstr_t
#include <comdef.h>

#pragma comment(lib, "Strmiids.lib")
#pragma comment(lib, "ksproxy.lib")

#ifdef _DEBUG
#pragma comment(lib, "strmbasd.lib")
#else
#pragma comment(lib, "strmbase.lib")
#endif

#pragma comment(lib, "winmm.lib")

using namespace std;

FILE *g_fpLog = NULL;

//////////////////////////////////////////////////////////////////////
// �萔����`
//////////////////////////////////////////////////////////////////////

// TS Writer�̖���:AddFilter���ɖ��O��o�^���邾���Ȃ̂ŉ��ł��悢
static const WCHAR *FILTER_GRAPH_NAME_TSWRITER	= L"TS Writer";

// MPEG2 Demultiplexer�̖���:AddFilter���ɖ��O��o�^���邾���Ȃ̂ŉ��ł��悢
static const WCHAR *FILTER_GRAPH_NAME_DEMUX = L"MPEG2 Demultiplexer";

// MPEG2 TIF�̖���:CLSID�����ł͓���ł��Ȃ��̂ł��̖��O�ƈ�v������̂��g�p����
static const WCHAR *FILTER_GRAPH_NAME_TIF = L"BDA MPEG2 Transport Information Filter";

//////////////////////////////////////////////////////////////////////
// �ÓI�����o�ϐ�
//////////////////////////////////////////////////////////////////////

// Dll�̃��W���[���n���h��
HMODULE CBonTuner::st_hModule = NULL;

// �쐬���ꂽCBontuner�C���X�^���X�̈ꗗ
list<CBonTuner*> CBonTuner::st_InstanceList;

// st_InstanceList����p
CRITICAL_SECTION CBonTuner::st_LockInstanceList;

// CBonTuner�Ŏg�p����Δg��ޔԍ���Polarisation�^��Mapping
const Polarisation CBonTuner::PolarisationMapping[] = {
	BDA_POLARISATION_NOT_DEFINED,
	BDA_POLARISATION_LINEAR_H,
	BDA_POLARISATION_LINEAR_V,
	BDA_POLARISATION_CIRCULAR_L,
	BDA_POLARISATION_CIRCULAR_R
};

const WCHAR CBonTuner::PolarisationChar[] = {
	L'\0',
	L'H',
	L'V',
	L'L',
	L'R'
};

const CBonTuner::TUNER_SPECIAL_DLL CBonTuner::aTunerSpecialData [] = {
	// �����̓v���O���}����������Ȃ��Ǝv���̂ŁA�v���O��������GUID ���������ɐ��K�����Ȃ��̂ŁA
	// �ǉ�����ꍇ�́AGUID�͏������ŏ����Ă�������

	/* TBS6980A */
	{ L"{e9ead02c-8b8c-4d9b-97a2-2ec0324360b1}", L"TBS" },

	/* TBS6980B, Prof 8000 */
	{ L"{ed63ec0b-a040-4c59-bc9a-59b328a3f852}", L"TBS" },

	/* Prof 7300, 7301, TBS 8920 */ 
	{ L"{91b0cc87-9905-4d65-a0d1-5861c6f22cbf}", L"TBS" },	// 7301 �͌ŗL�֐��łȂ��Ă�OK������

	/* TBS 6920 */
	{ L"{ed63ec0b-a040-4c59-bc9a-59b328a3f852}", L"TBS" },

	/* Prof Prof 7500, Q-BOX II */ 
	{ L"{b45b50ff-2d09-4bf2-a87c-ee4a7ef00857}", L"TBS" },

	/* DVBWorld 2002, 2004, 2006 */
	{ L"{4c807f36-2db7-44ce-9582-e1344782cb85}", L"DVBWorld" },

	/* DVBWorld 210X, 2102X, 2104X */
	{ L"{5a714cad-60f9-4124-b922-8a0557b8840e}", L"DVBWorld" },

	/* DVBWorld 2005 */
	{ L"{ede18552-45e6-469f-93b5-27e94296de38}", L"DVBWorld" }, // 2005 �͌ŗL�֐��͕K�v�Ȃ�����

	{ L"", L"" }, // terminator
};

//////////////////////////////////////////////////////////////////////
// �C���X�^���X�������\�b�h
//////////////////////////////////////////////////////////////////////
#pragma warning(disable : 4273)
extern "C" __declspec(dllexport) IBonDriver * CreateBonDriver()
{
	return (IBonDriver *) new CBonTuner;
}
#pragma warning(default : 4273)

//////////////////////////////////////////////////////////////////////
// �\�z/����
//////////////////////////////////////////////////////////////////////
CBonTuner::CBonTuner()
	: m_nToneWait(100),
	m_nLockWait(2000),
	m_nLockWaitDelay(0),
	m_nLockWaitRetry(0),
	m_bLockTwice(FALSE),
	m_nLockTwiceDelay(100),
	m_nWatchDogSignalLocked(0),
	m_nWatchDogBitRate(0),
	m_nReOpenWhenGiveUpReLock(0),
	m_bTryAnotherTuner(FALSE),
	m_bBackgroundChannelLock(FALSE),
	m_nSignalLevelCalcType(0),
	m_bSignalLevelGetTypeSS(FALSE),
	m_bSignalLevelGetTypeTuner(FALSE),
	m_bSignalLevelGetTypeBR(FALSE),
	m_bSignalLevelNeedStrength(FALSE),
	m_bSignalLevelNeedQuality(FALSE),
	m_bSignalLevelCalcTypeMul(FALSE),
	m_bSignalLevelCalcTypeAdd(FALSE),
	m_fStrengthCoefficient(1),
	m_fQualityCoefficient(1),
	m_fStrengthBias(0),
	m_fQualityBias(0),
	m_nSignalLockedJudgeType(1),
	m_bSignalLockedJudgeTypeSS(FALSE),
	m_bSignalLockedJudgeTypeTuner(FALSE),
	m_dwBuffSize(188 * 1024),
	m_dwMaxBuffCount(512),
	m_dwMinBuffCount(32),
	m_nWaitTsCount(1),
	m_nWaitTsSleep(100),
	m_bAlwaysAnswerLocked(FALSE),
	m_bReserveUnusedCh(FALSE),
	m_szIniFilePath(L""),
	m_lStreamFlag(0),
	m_hOnDecodeEvent(NULL),
	m_LastBuff(NULL),
	m_bRecvStarted(FALSE),
	m_hSemaphore(NULL),
	m_pITuningSpace(NULL),
	m_pITuner(NULL),
	m_pNetworkProvider(NULL),
	m_pTunerDevice(NULL),
	m_pCaptureDevice(NULL),
	m_pTsWriter(NULL),
	m_pDemux(NULL),
	m_pTif(NULL),
	m_pIGraphBuilder(NULL),
	m_pIMediaControl(NULL), 
	m_pCTsWriter(NULL),
	m_pIBDA_SignalStatistics(NULL),
	m_pDSFilterEnumTuner(NULL),
	m_pDSFilterEnumCapture(NULL),
	m_nDVBSystemType(eTunerTypeDVBS),
	m_nNetworkProvider(eNetworkProviderAuto),
	m_nDefaultNetwork(1),
	m_bOpened(FALSE),
	m_dwTargetSpace(CBonTuner::SPACE_INVALID),
	m_dwCurSpace(CBonTuner::SPACE_INVALID),
	m_dwTargetChannel(CBonTuner::CHANNEL_INVALID),
	m_dwCurChannel(CBonTuner::CHANNEL_INVALID),
	m_nCurTone(CBonTuner::TONE_UNKNOWN),
	m_hModuleTunerSpecials(NULL),
	m_pIBdaSpecials(NULL),
	m_pIBdaSpecials2(NULL)
{
	// �C���X�^���X���X�g�Ɏ��g��o�^
	::EnterCriticalSection(&st_LockInstanceList);
	st_InstanceList.push_back(this);
	::LeaveCriticalSection(&st_LockInstanceList);

	setlocale(LC_CTYPE, "ja_JP.SJIS");

	::InitializeCriticalSection(&m_csTSBuff);
	::InitializeCriticalSection(&m_csDecodedTSBuff);

	ReadIniFile();

	m_TsBuff.SetSize(m_dwBuffSize, m_dwMaxBuffCount, m_dwMinBuffCount);

	// COM������p�X���b�h�N��
	m_aCOMProc.hThread = ::CreateThread(NULL, 0, CBonTuner::COMProcThread, this, 0, NULL);
}

CBonTuner::~CBonTuner()
{
	OutputDebug(L"~CBonTuner called.\n");
	CloseTuner();

	// COM������p�X���b�h�I��
	if (m_aCOMProc.hThread) {
		::SetEvent(m_aCOMProc.hTerminateRequest);
		::WaitForSingleObject(m_aCOMProc.hThread, INFINITE);
		::CloseHandle(m_aCOMProc.hThread);
		m_aCOMProc.hThread = NULL;
	}

	::DeleteCriticalSection(&m_csDecodedTSBuff);
	::DeleteCriticalSection(&m_csTSBuff);

	// �C���X�^���X���X�g���玩�g���폜
	::EnterCriticalSection(&st_LockInstanceList);
	st_InstanceList.remove(this);
	::LeaveCriticalSection(&st_LockInstanceList);
}

/////////////////////////////////////
//
// IBonDriver2 APIs
//
/////////////////////////////////////

const BOOL CBonTuner::OpenTuner(void)
{
	if (m_aCOMProc.hThread == NULL)
		return FALSE;

	DWORD dw;
	BOOL ret = FALSE;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqOpenTuner;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	if (dw == WAIT_OBJECT_0) {
		ret = m_aCOMProc.uRetVal.OpenTuner;
	}

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return ret;
}

const BOOL CBonTuner::_OpenTuner(void)
{
	if (m_bOpened)
		return TRUE;

	HRESULT hr;

	do {
		// �t�B���^�O���t�̍쐬
		if (FAILED(hr = InitializeGraphBuilder()))
			break;

		// �`���[�j���O�X�y�[�X�̓Ǎ�
		if (FAILED(hr = CreateTuningSpace()))
			break;

		// �l�b�g���[�N�v���o�C�_
		if (FAILED(hr = LoadNetworkProvider()))
			break;

		// �`���[�j���O�X�y�[�X������
		if (FAILED(hr = InitTuningSpace()))
			break;

		// ���[�h���ׂ��`���[�i�E�L���v�`���̃��X�g�쐬
		if (m_UsableTunerCaptureList.empty() && FAILED(hr = InitDSFilterEnum()))
			break;

		// �`���[�i�E�L���v�`���Ȍ�̍\�z�Ǝ��s
		if (FAILED(hr = LoadAndConnectDevice()))
			break;

		OutputDebug(L"Build graph Successfully.\n");

		if (m_bSignalLockedJudgeTypeSS || m_bSignalLevelGetTypeSS) {
			// �`���[�i�̐M����Ԏ擾�p�C���^�[�t�F�[�X�̎擾�i���s���Ă����s�j
			hr = LoadTunerSignalStatistics();
		}

		// TS��M�t���O������
		m_lStreamFlag = 0;

		// Decode�C�x���g�쐬
		m_hOnDecodeEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

		// Decode������p�X���b�h�N��
		m_aDecodeProc.hThread = ::CreateThread(NULL, 0, CBonTuner::DecodeProcThread, this, 0, NULL);

		// �R�[���o�b�N�֐��Z�b�g
		StartRecv();

		m_bOpened = TRUE;

		return TRUE;

	} while(0);

	// �����ɓ��B�����Ƃ������Ƃ͉��炩�̃G���[�Ŏ��s����
	_CloseTuner();

	return FALSE;
}

void CBonTuner::CloseTuner(void)
{
	if (m_aCOMProc.hThread == NULL)
		return;

	DWORD dw;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqCloseTuner;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return;
}

#pragma warning (push)
#pragma warning (disable: 4702)
void CBonTuner::_CloseTuner(void)
{
	m_bOpened = FALSE;

	// �O���t��~
	StopGraph();

	// �R�[���o�b�N�֐���~
	StopRecv();

	// Decode������p�X���b�h�I��
	if (m_aDecodeProc.hThread) {
		::InterlockedExchange(&m_aDecodeProc.lTerminateFlag, 1);
		::WaitForSingleObject(m_aDecodeProc.hThread, INFINITE);
		::CloseHandle(m_aDecodeProc.hThread);
		m_aDecodeProc.hThread = NULL;
	}

	// Decode�C�x���g�J��
	if (m_hOnDecodeEvent) {
		::CloseHandle(m_hOnDecodeEvent);
		m_hOnDecodeEvent = NULL;
	}

	// TS��M�t���O���
	m_lStreamFlag = 0;

	// �o�b�t�@���
	PurgeTsStream();

	// �`���[�i�̐M����Ԏ擾�p�C���^�[�t�F�[�X���
	UnloadTunerSignalStatistics();

	// �O���t���
	CleanupGraph();

	m_dwTargetSpace = m_dwCurSpace = CBonTuner::SPACE_INVALID;
	m_dwTargetChannel = m_dwCurChannel = CBonTuner::CHANNEL_INVALID;
	m_nCurTone = CBonTuner::TONE_UNKNOWN;

	if (m_hSemaphore) {
		try {
			::ReleaseSemaphore(m_hSemaphore, 1, NULL);
			::CloseHandle(m_hSemaphore);
			m_hSemaphore = NULL;
		} catch (...) {
			OutputDebug(L"Exception in ReleaseSemaphore.\n");
		}
	}

	return;
}
#pragma warning (pop)

const BOOL CBonTuner::SetChannel(const BYTE byCh)
{
	// IBonDriver (not IBonDriver2) �p�C���^�[�t�F�[�X; obsolete?
	return SetChannel(0UL, DWORD(byCh));
}

const float CBonTuner::GetSignalLevel(void)
{
	if (m_aCOMProc.hThread == NULL)
		return FALSE;

	DWORD dw;
	float ret = 0.0F;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqGetSignalLevel;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	if (dw == WAIT_OBJECT_0) {
		ret = m_aCOMProc.uRetVal.GetSignalLevel;
	}

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return ret;
}

const float CBonTuner::_GetSignalLevel(void)
{
	if (!m_bOpened)
		return -1.0F;

	HRESULT hr;
	float f = 0.0F;

	// �r�b�g���[�g��Ԃ��ꍇ
	if (m_bSignalLevelGetTypeBR) {
		return m_BitRate.GetRate();
	}

	// IBdaSpecials2�ŗL�֐�������Ίۓ���
	if (m_pIBdaSpecials2 && (hr = m_pIBdaSpecials2->GetSignalStrength(&f)) != E_NOINTERFACE) {
		return f;
	}

	//   get_SignalQuality �M���̕i�������� 1 �` 100 �̒l���擾����B
	//   get_SignalStrength �f�V�x���P�ʂ̐M���̋��x�������l���擾����B 
	int nStrength;
	int nQuality;
	int nLock;

	if (m_dwTargetChannel == CBonTuner::CHANNEL_INVALID)
		// SetChannel()����x���Ă΂�Ă��Ȃ��ꍇ��0��Ԃ�
		return 0;

	GetSignalState(&nStrength, &nQuality, &nLock);
	if (!nLock)
		// Lock�o���Ă��Ȃ��ꍇ��0��Ԃ�
		return 0;
	if (nStrength < 0 && m_bSignalLevelNeedStrength)
		// Strength��-1��Ԃ��ꍇ������
		return (float)nStrength;
	float s = 0.0F;
	float q = 0.0F;
	if (m_bSignalLevelNeedStrength)
		s = float(nStrength) / m_fStrengthCoefficient + m_fStrengthBias;
	if (m_bSignalLevelNeedQuality)
		q = float(nQuality) / m_fQualityCoefficient + m_fQualityBias;

	if (m_bSignalLevelCalcTypeMul)
		return s * q;
	return s + q;
}

const DWORD CBonTuner::WaitTsStream(const DWORD dwTimeOut)
{
	if( m_hOnDecodeEvent == NULL ){
		return WAIT_ABANDONED;
	}

	DWORD dwRet;
	if (m_nWaitTsSleep) {
		// WaitTsSleep ���w�肳��Ă���ꍇ
		dwRet = ::WaitForSingleObject(m_hOnDecodeEvent, 0);
		// �C�x���g���V�O�i����ԂłȂ���Ύw�莞�ԑҋ@����
		if (dwRet != WAIT_TIMEOUT)
			return dwRet;

		::Sleep(m_nWaitTsSleep);
	}

	// �C�x���g���V�O�i����ԂɂȂ�̂�҂�
	dwRet = ::WaitForSingleObject(m_hOnDecodeEvent, (dwTimeOut)? dwTimeOut : INFINITE);
	return dwRet;
}

const DWORD CBonTuner::GetReadyCount(void)
{
	DWORD dw;
	::EnterCriticalSection(&m_csDecodedTSBuff);
	dw = (DWORD)m_DecodedTsBuff.size();
	::LeaveCriticalSection(&m_csDecodedTSBuff);
	return dw;
}

const BOOL CBonTuner::GetTsStream(BYTE *pDst, DWORD *pdwSize, DWORD *pdwRemain)
{
	BYTE *pSrc = NULL;
	if (GetTsStream(&pSrc, pdwSize, pdwRemain)) {
		if (*pdwSize)
			memcpy(pDst, pSrc, *pdwSize);
		return TRUE;
	}
	return FALSE;
}

const BOOL CBonTuner::GetTsStream(BYTE **ppDst, DWORD *pdwSize, DWORD *pdwRemain)
{
	BOOL bRet = TRUE;

	if (m_LastBuff) {
		m_TsBuff.Free(m_LastBuff);
		m_LastBuff = NULL;
	}
	
	::EnterCriticalSection(&m_csDecodedTSBuff);

	if (!m_DecodedTsBuff.empty()) {
		m_LastBuff = m_DecodedTsBuff.front();
		m_DecodedTsBuff.pop_front();
	}

	if (m_LastBuff)
	{
		*pdwSize = m_LastBuff->dwSize;
		*ppDst = m_LastBuff->pbyBuff;
		*pdwRemain = (DWORD)m_DecodedTsBuff.size();
	}
	else {
		*pdwSize = 0;
		*ppDst = NULL;
		*pdwRemain = 0;
		bRet = FALSE;
	}

	::LeaveCriticalSection(&m_csDecodedTSBuff);

	return bRet;
}

void CBonTuner::PurgeTsStream(void)
{
	// �f�R�[�h��TS�o�b�t�@
	m_TsBuff.Free(m_LastBuff);
	m_LastBuff = NULL;

	::EnterCriticalSection(&m_csDecodedTSBuff);
	while (!m_DecodedTsBuff.empty())
	{
		TS_DATA *ts = m_DecodedTsBuff.front();
		m_DecodedTsBuff.pop_front();
		m_TsBuff.Free(ts);
	}
	::LeaveCriticalSection(&m_csDecodedTSBuff);

	// ��MTS�o�b�t�@
	m_TsBuff.Purge();

	// �r�b�g���[�g�v�Z�p�N���X
	m_BitRate.Clear();
}

LPCTSTR CBonTuner::GetTunerName(void)
{
	return m_aTunerParam.sTunerName.c_str();
}

const BOOL CBonTuner::IsTunerOpening(void)
{
	if (m_aCOMProc.hThread == NULL)
		return FALSE;

	DWORD dw;
	BOOL ret = FALSE;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqIsTunerOpening;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	if (dw == WAIT_OBJECT_0) {
		ret = m_aCOMProc.uRetVal.IsTunerOpening;
	}

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return ret;
}

const BOOL CBonTuner::_IsTunerOpening(void)
{
	return m_bOpened;
}

LPCTSTR CBonTuner::EnumTuningSpace(const DWORD dwSpace)
{
	if (dwSpace < m_TuningData.dwNumSpace) {
		map<unsigned int, TuningSpaceData*>::iterator it = m_TuningData.Spaces.find(dwSpace);
		if (it != m_TuningData.Spaces.end())
			return it->second->sTuningSpaceName.c_str();
		else
#ifdef UNICODE
			return _T("-");
#else
			return "-";
#endif
	}
	return NULL;
}

LPCTSTR CBonTuner::EnumChannelName(const DWORD dwSpace, const DWORD dwChannel)
{
	map<unsigned int, TuningSpaceData*>::iterator it = m_TuningData.Spaces.find(dwSpace);
	if (it != m_TuningData.Spaces.end()) {
		if (dwChannel < it->second->dwNumChannel) {
			map<unsigned int, ChData*>::iterator it2 = it->second->Channels.find(dwChannel);
			if (it2 != it->second->Channels.end())
				return it2->second->sServiceName.c_str();
			else
#ifdef UNICODE
				return _T("----");
#else
				return "----";
#endif
		}
	}
	return NULL;
}

const BOOL CBonTuner::SetChannel(const DWORD dwSpace, const DWORD dwChannel)
{
	if (m_aCOMProc.hThread == NULL)
		return FALSE;

	DWORD dw;
	BOOL ret = FALSE;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqSetChannel;
	m_aCOMProc.uParam.SetChannel.dwSpace = dwSpace;
	m_aCOMProc.uParam.SetChannel.dwChannel = dwChannel;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	if (dw == WAIT_OBJECT_0) {
		ret = m_aCOMProc.uRetVal.SetChannel;
	}

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return ret;
}

const BOOL CBonTuner::_SetChannel(const DWORD dwSpace, const DWORD dwChannel)
{
	HRESULT hr;

	OutputDebug(L"SetChannel(%d, %d)\n", dwSpace, dwChannel);

	m_dwTargetSpace = m_dwCurSpace = CBonTuner::SPACE_INVALID;
	m_dwTargetChannel = m_dwCurChannel = CBonTuner::CHANNEL_INVALID;

	map<unsigned int, TuningSpaceData*>::iterator it = m_TuningData.Spaces.find(dwSpace);
	if (it == m_TuningData.Spaces.end()) {
		OutputDebug(L"    Invalid channel space.\n");
		return FALSE;
	}

	if (dwChannel >= it->second->dwNumChannel) {
		OutputDebug(L"    Invalid channel number.\n");
		return FALSE;
	}

	map<unsigned int, ChData*>::iterator it2 = it->second->Channels.find(dwChannel);
	if (it2 == it->second->Channels.end()) {
		OutputDebug(L"    Reserved channel number.\n");
		return FALSE;
	}

	if (!m_bOpened) {
		OutputDebug(L"    Tuner not opened.\n");
		return FALSE;
	}

	m_bRecvStarted = FALSE;
	PurgeTsStream();
	ChData * Ch = it2->second;
	m_LastTuningParam.Frequency = Ch->Frequency;
	m_LastTuningParam.Polarisation = PolarisationMapping[Ch->Polarisation];
	m_LastTuningParam.Antenna = &m_aSatellite[Ch->Satellite].Polarisation[Ch->Polarisation];
	m_LastTuningParam.Modulation = &m_aModulationType[Ch->ModulationType];
	m_LastTuningParam.ONID = Ch->ONID;
	m_LastTuningParam.TSID = Ch->TSID;
	m_LastTuningParam.SID = Ch->SID;

	BOOL bRet = LockChannel(&m_LastTuningParam, m_bLockTwice && Ch->LockTwiceTarget);

	// IBdaSpecials�Œǉ��̏������K�v�Ȃ�s��
	if (m_pIBdaSpecials2)
		hr = m_pIBdaSpecials2->PostLockChannel(&m_LastTuningParam);

	::Sleep(100);
	PurgeTsStream();
	m_bRecvStarted = TRUE;

	// SetChannel()�����݂��`���[�j���O�X�y�[�X�ԍ��ƃ`�����l���ԍ�
	m_dwTargetSpace = dwSpace;
	m_dwTargetChannel = dwChannel;

	if (bRet) {
		OutputDebug(L"SetChannel success.\n");
		m_dwCurSpace = dwSpace;
		m_dwCurChannel = dwChannel;
		return TRUE;
	}
	// m_byCurTone = CBonTuner::TONE_UNKNOWN;

	OutputDebug(L"SetChannel failed.\n");
	if (m_bAlwaysAnswerLocked)
		return TRUE;
	return FALSE;
}

const DWORD CBonTuner::GetCurSpace(void)
{
	if (m_aCOMProc.hThread == NULL)
		return CBonTuner::SPACE_INVALID;

	DWORD dw;
	DWORD ret = CBonTuner::SPACE_INVALID;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqGetCurSpace;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	if (dw == WAIT_OBJECT_0) {
		ret = m_aCOMProc.uRetVal.GetCurSpace;
	}

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return ret;
}

const DWORD CBonTuner::_GetCurSpace(void)
{
	if (m_bAlwaysAnswerLocked)
		return m_dwTargetSpace;

	return m_dwCurSpace;
}

const DWORD CBonTuner::GetCurChannel(void)
{
	if (m_aCOMProc.hThread == NULL)
		return CBonTuner::CHANNEL_INVALID;

	DWORD dw;
	DWORD ret = CBonTuner::CHANNEL_INVALID;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqGetCurChannel;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	if (dw == WAIT_OBJECT_0) {
		ret = m_aCOMProc.uRetVal.GetCurChannel;
	}

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return ret;
}

const DWORD CBonTuner::_GetCurChannel(void)
{
	if (m_bAlwaysAnswerLocked)
		return m_dwTargetChannel;

	return m_dwCurChannel;
}

void CBonTuner::Release(void)
{
	OutputDebug(L"CBonTuner::Release called.\n");

	delete this;
}

DWORD WINAPI CBonTuner::COMProcThread(LPVOID lpParameter)
{
	BOOL terminate = FALSE;
	CBonTuner* pSys = (CBonTuner*) lpParameter;
	COMProc* pCOMProc = &pSys->m_aCOMProc;
	HRESULT hr;

	OutputDebug(L"COMProcThread: Thread created.\n");

	// COM������
	hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY);

	HANDLE h[2] = {
		pCOMProc->hTerminateRequest,
		pCOMProc->hReqEvent
	};

	while (!terminate) {
		DWORD ret = ::WaitForMultipleObjects(2, h, FALSE, 1000);
		switch (ret)
		{
		case WAIT_OBJECT_0:
			terminate = TRUE;
			break;

		case WAIT_OBJECT_0 + 1:
			switch (pCOMProc->nRequest)
			{

			case eCOMReqOpenTuner:
				pCOMProc->uRetVal.OpenTuner = pSys->_OpenTuner();
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqCloseTuner:
				// OpenTuner�ELockChannel�̍Ď��s���Ȃ璆�~
				pCOMProc->ResetReOpenTuner();
				pCOMProc->ResetReLockChannel();

				pSys->_CloseTuner();
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqSetChannel:
				// �ُ팟�m�Ď��^�C�}�[������
				pCOMProc->ResetWatchDog();

				// LockChannel�̍Ď��s���Ȃ璆�~
				pCOMProc->ResetReLockChannel();

				// OpenTuner�̍Ď��s���Ȃ�FALSE��Ԃ�
				if (pCOMProc->bDoReOpenTuner) {
					pCOMProc->ClearReOpenChannel();
					pCOMProc->uRetVal.SetChannel = FALSE;
				}
				else {
					pCOMProc->uRetVal.SetChannel = pSys->_SetChannel(pCOMProc->uParam.SetChannel.dwSpace, pCOMProc->uParam.SetChannel.dwChannel);
				}
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqGetSignalLevel:
				// OpenTuner�̍Ď��s���Ȃ�0��Ԃ�
				if (pCOMProc->bDoReOpenTuner) {
					pCOMProc->uRetVal.GetSignalLevel = 0.0F;
				}
				else {
					pCOMProc->uRetVal.GetSignalLevel = pSys->_GetSignalLevel();
				}
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqIsTunerOpening:
				// OpenTuner�̍Ď��s���Ȃ�TRUE��Ԃ�
				if (pCOMProc->bDoReOpenTuner) {
					pCOMProc->uRetVal.IsTunerOpening = TRUE;
				}
				else {
					pCOMProc->uRetVal.IsTunerOpening = pSys->_IsTunerOpening();
				}
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqGetCurSpace:
				// OpenTuner�̍Ď��s���Ȃ�ޔ�l��Ԃ�
				if (pCOMProc->bDoReOpenTuner) {
					pCOMProc->uRetVal.GetCurSpace = pCOMProc->dwReOpenSpace;
				}
				else {
					pCOMProc->uRetVal.GetCurSpace = pSys->_GetCurSpace();
				}
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqGetCurChannel:
				// OpenTuner�̍Ď��s���Ȃ�ޔ�l��Ԃ�
				if (pCOMProc->bDoReOpenTuner) {
					pCOMProc->uRetVal.GetCurChannel = pCOMProc->dwReOpenChannel;
				}
				else {
					pCOMProc->uRetVal.GetCurChannel = pSys->_GetCurChannel();
				}
				::SetEvent(pCOMProc->hEndEvent);
				break;

			default:
				break;
			}
			break;

		case WAIT_TIMEOUT:
			break;

		case WAIT_FAILED:
		default:
			DWORD err = ::GetLastError();
			OutputDebug(L"COMProcThread: Unknown error (ret=%d, LastError=0x%08x).\n", ret, err);
			terminate = TRUE;
			break;
		}

		if (terminate)
			break;

		// �ُ팟�m�����J�o���[
		// 1000ms������
		if (pCOMProc->CheckTick()) {

			// SetChannel()���s���̃o�b�N�O�����hCH�ؑ֊J�n
			if (pSys->m_bBackgroundChannelLock && pSys->m_dwCurChannel == CBonTuner::CHANNEL_INVALID && pSys->m_dwTargetChannel != CBonTuner::CHANNEL_INVALID) {
				OutputDebug(L"COMProcThread: Background retry.\n");
				pCOMProc->SetReLockChannel();
			}

			// �ُ팟�m
			if (!pCOMProc->bDoReLockChannel && !pCOMProc->bDoReOpenTuner && pSys->m_dwCurChannel != CBonTuner::CHANNEL_INVALID) {

				// SignalLock�̏�Ԋm�F
				if (pSys->m_nWatchDogSignalLocked != 0) {
					int lock = 0;
					pSys->GetSignalState(NULL, NULL, &lock);
					if (pCOMProc->CheckSignalLockErr(lock, pSys->m_nWatchDogSignalLocked * 1000)) {
						// �`�����l�����b�N�Ď��s
						OutputDebug(L"COMProcThread: WatchDogSignalLocked time is up.\n");
						pCOMProc->SetReLockChannel();
					}
				} // SignalLock�̏�Ԋm�F

				// BitRate�m�F
				if (pSys->m_nWatchDogBitRate != 0) {
					if (pCOMProc->CheckBitRateErr((pSys->m_BitRate.GetRate() > 0.0F), pSys->m_nWatchDogBitRate * 1000)) {
						// �`�����l�����b�N�Ď��s
						OutputDebug(L"COMProcThread: WatchDogBitRate time is up.\n");
						pCOMProc->SetReLockChannel();
					}
				} // BitRate�m�F
			} // �ُ팟�m

			// CH�ؑ֓��쎎�s���OpenTuner�Ď��s
			if (pCOMProc->bDoReOpenTuner) {
				// OpenTuner�Ď��s
				pSys->_CloseTuner();
				if (pSys->_OpenTuner() && (!pCOMProc->CheckReOpenChannel() || pSys->_SetChannel(pCOMProc->dwReOpenSpace, pCOMProc->dwReOpenChannel))) {
					// OpenTuner�ɐ������ASetChannnel�ɐ����������͕K�v�Ȃ�
					OutputDebug(L"COMProcThread: Re-OpenTuner SUCCESS.\n");
					pCOMProc->ResetReOpenTuner();
				}
				else {
					// ���s...���̂܂܎�����`�������W����
					OutputDebug(L"COMProcThread: Re-OpenTuner FAILED.\n");
				}
			} // CH�ؑ֓��쎎�s���OpenTuner�Ď��s

			// �ُ팟�m��`�����l�����b�N�Ď��s
			if (!pCOMProc->bDoReOpenTuner && pCOMProc->bDoReLockChannel) {
				// �`�����l�����b�N�Ď��s
				if (pSys->LockChannel(&pSys->m_LastTuningParam, FALSE)) {
					// LockChannel�ɐ�������
					OutputDebug(L"COMProcThread: Re-LockChannel SUCCESS.\n");
					pCOMProc->ResetReLockChannel();
				}
				else {
					// LockChannel���s
					OutputDebug(L"COMProcThread: Re-LockChannel FAILED.\n");
					if (pSys->m_nReOpenWhenGiveUpReLock != 0) {
						// CH�ؑ֓��쎎�s�񐔐ݒ�l��0�ȊO
						if (pCOMProc->CheckReLockFailCount(pSys->m_nReOpenWhenGiveUpReLock)) {
							// CH�ؑ֓��쎎�s�񐔂𒴂����̂�OpenTuner�Ď��s
							OutputDebug(L"COMProcThread: ReOpenWhenGiveUpReLock count is up.\n");
							pCOMProc->SetReOpenTuner(pSys->m_dwTargetSpace, pSys->m_dwTargetChannel);
							pCOMProc->ResetReLockChannel();
						}
					}
				}
			} // �ُ팟�m��`�����l�����b�N�Ď��s
		} // 1000ms������
	} // while (!terminate)

	// DS�t�B���^�[�񋓂ƃ`���[�i�E�L���v�`���̃��X�g���폜
	SAFE_DELETE(pSys->m_pDSFilterEnumTuner);
	SAFE_DELETE(pSys->m_pDSFilterEnumCapture);
	pSys->m_UsableTunerCaptureList.clear();

	::CoUninitialize();
	OutputDebug(L"COMProcThread: Thread terminated.\n");

	return 0;
}

DWORD WINAPI CBonTuner::DecodeProcThread(LPVOID lpParameter)
{
	CBonTuner* pSys = (CBonTuner*)lpParameter;
	IBdaSpecials2a1 **ppIBdaSpecials2 = &pSys->m_pIBdaSpecials2;

	OutputDebug(L"DecodeProcThread: Thread created.\n");

	// COM������
	::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY);

	// IBdaSpecials�ɂ��f�R�[�h�������K�v���ǂ���
	BOOL bNeedDecode = FALSE;
	if (*ppIBdaSpecials2) {
		(*ppIBdaSpecials2)->IsDecodingNeeded(&bNeedDecode);
	}
	OutputDebug(L"DecodeProcThread: Detected IBdaSpecials decoding=%d.\n", bNeedDecode);

	LONG *plTerminateFlag = &pSys->m_aDecodeProc.lTerminateFlag;
	const DWORD dwMaxBuffCount = pSys->m_dwMaxBuffCount;
	const unsigned int nWaitTsCount = pSys->m_nWaitTsCount;

	CRITICAL_SECTION *pcsDecodedTSBuff = &pSys->m_csDecodedTSBuff;
	LONG *plStreamFlag = &pSys->m_lStreamFlag;
	HANDLE *phOnDecodeEvent = &pSys->m_hOnDecodeEvent;
	TS_BUFF *pTsBuff = &pSys->m_TsBuff;
	std::deque<TS_DATA *> *pDecodedTsBuff = &pSys->m_DecodedTsBuff;

	BitRate *pBitRate = &pSys->m_BitRate;

	while (!*plTerminateFlag)
	{
		if (*plStreamFlag)
		{
			::InterlockedExchange(plStreamFlag, 0);

			// TS�o�b�t�@����̃f�[�^�擾
			while (TS_DATA *pBuff = pTsBuff->Get()) {
				// �K�v�Ȃ��IBdaSpecials�ɂ��f�R�[�h�������s��
				if (bNeedDecode) {
					(*ppIBdaSpecials2)->Decode(pBuff->pbyBuff, pBuff->dwSize);
				}

				::EnterCriticalSection(pcsDecodedTSBuff);

				size_t size = pDecodedTsBuff->size();

				// �I�[�o�[�t���[�̏ꍇ�͍폜
				while (size >= dwMaxBuffCount)
				{
					TS_DATA *ts = pDecodedTsBuff->back();
					pDecodedTsBuff->pop_back();
					pTsBuff->Free(ts);
					size--;
				}

				// �擾�����o�b�t�@���f�R�[�h�ς݃o�b�t�@�ɒǉ�
				pDecodedTsBuff->push_back(pBuff);

				// ��M�C�x���g�Z�b�g
				if ((size + 1) >= nWaitTsCount)
					::SetEvent(*phOnDecodeEvent);

				::LeaveCriticalSection(pcsDecodedTSBuff);
			}
		}
		else {
			// �x�e
			Sleep(pBitRate->CheckRate());
		}
	}

	::CoUninitialize();
	OutputDebug(L"DecodeProcThread: Thread terminated.\n");

	return 0;
}

int CALLBACK CBonTuner::RecvProc(void* pParam, BYTE* pbData, DWORD dwSize)
{
	CBonTuner* pSys = (CBonTuner*)pParam;

	pSys->m_BitRate.AddRate(dwSize);

	if (pSys->m_bRecvStarted) {
		if (pSys->m_TsBuff.AddData(pbData, dwSize)) {
			::InterlockedExchange(&pSys->m_lStreamFlag, 1);
		}
	}

	return 0;
}

void CBonTuner::StartRecv(void)
{
	if (m_pCTsWriter)
		m_pCTsWriter->SetCallBackRecv(RecvProc, this);
	m_bRecvStarted = TRUE;
}

void CBonTuner::StopRecv(void)
{
	if (m_pCTsWriter)
		m_pCTsWriter->SetCallBackRecv(NULL, this);
	m_bRecvStarted = FALSE;
}

void CBonTuner::ReadIniFile(void)
{
	// INI�t�@�C���̃t�@�C�����擾
	::GetModuleFileNameW(st_hModule, m_szIniFilePath, sizeof(m_szIniFilePath) / sizeof(m_szIniFilePath[0]));

	::wcscpy_s(m_szIniFilePath + ::wcslen(m_szIniFilePath) - 3, 4, L"ini");

	WCHAR buf[256];
	WCHAR buf2[256];
	WCHAR buf3[256];
	WCHAR buf4[256];
#ifndef UNICODE
	char charBuf[512];
#endif
	int val;
	wstring strBuf;

	// DebugLog���L�^���邩�ǂ���
	if (::GetPrivateProfileIntW(L"BONDRIVER", L"DebugLog", 0, m_szIniFilePath)) {
		WCHAR szDebugLogPath[_MAX_PATH + 1];
		::wcscpy_s(szDebugLogPath, _MAX_PATH + 1, m_szIniFilePath);
		::wcscpy_s(szDebugLogPath + ::wcslen(szDebugLogPath) - 3, 4, L"log");
		SetDebugLog(szDebugLogPath);
	}

	//
	// Tuner �Z�N�V����
	//

	// GUID0 - GUID99: Tuner�f�o�C�X��GUID ... �w�肳��Ȃ���Ό����������Ɏg�������Ӗ�����B
	// FriendlyName0 - FriendlyName99: Tuner�f�o�C�X��FriendlyName ... �w�肳��Ȃ���Ό����������Ɏg�������Ӗ�����B
	// CaptureGUID0 - CaptureGUID99: Capture�f�o�C�X��GUID ... �w�肳��Ȃ���ΐڑ��\�ȃf�o�C�X����������B
	// CaptureFriendlyName0 - CaptureFriendlyName99: Capture�f�o�C�X��FriendlyName ... �w�肳��Ȃ���ΐڑ��\�ȃf�o�C�X����������B
	for (unsigned int i = 0; i < MAX_GUID; i++) {
		WCHAR keyname[64];
		::swprintf_s(keyname, 64, L"GUID%d", i);
		::GetPrivateProfileStringW(L"TUNER", keyname, L"", buf, 256, m_szIniFilePath);
		::swprintf_s(keyname, 64, L"FriendlyName%d", i);
		::GetPrivateProfileStringW(L"TUNER", keyname, L"", buf2, 256, m_szIniFilePath);
		::swprintf_s(keyname, 64, L"CaptureGUID%d", i);
		::GetPrivateProfileStringW(L"TUNER", keyname, L"", buf3, 256, m_szIniFilePath);
		::swprintf_s(keyname, 64, L"CaptureFriendlyName%d", i);
		::GetPrivateProfileStringW(L"TUNER", keyname, L"", buf4, 256, m_szIniFilePath);
		if (buf[0] == L'\0' && buf2[0] == L'\0' && buf3[0] == L'\0' && buf4[0] == L'\0') {
			// �ǂ���w�肳��Ă��Ȃ�
			if (i == 0) {
				// �ԍ��Ȃ��̌^���œǍ���
				::GetPrivateProfileStringW(L"TUNER", L"GUID", L"", buf, 256, m_szIniFilePath);
				::GetPrivateProfileStringW(L"TUNER", L"FriendlyName", L"", buf2, 256, m_szIniFilePath);
				::GetPrivateProfileStringW(L"TUNER", L"CaptureGUID", L"", buf3, 256, m_szIniFilePath);
				::GetPrivateProfileStringW(L"TUNER", L"CaptureFriendlyName", L"", buf4, 256, m_szIniFilePath);
				// �ǂ���w�肳��Ă��Ȃ��ꍇ�ł��o�^
			} else
				break;
		}
		TunerSearchData *sdata = new TunerSearchData(buf, buf2, buf3, buf4);
		m_aTunerParam.Tuner.insert(pair<unsigned int, TunerSearchData*>(i, sdata));
	}

	// Tuner�f�o�C�X�݂̂�Capture�f�o�C�X�����݂��Ȃ�
	m_aTunerParam.bNotExistCaptureDevice = (BOOL)::GetPrivateProfileIntW(L"TUNER", L"NotExistCaptureDevice", 0, m_szIniFilePath);

	// Tuner��Capture�̃f�o�C�X�C���X�^���X�p�X����v���Ă��邩�̊m�F���s�����ǂ���
	m_aTunerParam.bCheckDeviceInstancePath = (BOOL)::GetPrivateProfileIntW(L"TUNER", L"CheckDeviceInstancePath", 1, m_szIniFilePath);

	// Tuner��: GetTunerName�ŕԂ��`���[�i�� ... �w�肳��Ȃ���΃f�t�H���g����
	//   �g����B���̏ꍇ�A�����`���[�i�𖼑O�ŋ�ʂ��鎖�͂ł��Ȃ�
	::GetPrivateProfileStringW(L"TUNER", L"Name", L"DVB-S2", buf, 256, m_szIniFilePath);
#ifdef UNICODE
	m_aTunerParam.sTunerName = buf;
#else
	::wcstombs_s(NULL, charBuf, 512, buf, _TRUNCATE);
	m_aTunerParam.sTunerName = charBuf;
#endif

	// �`���[�i�ŗL�֐����g�p���邩�ǂ����B
	//   �ȉ��� INI �t�@�C���Ŏw��\
	//     "" ... �g�p���Ȃ�; "AUTO" ... AUTO(default)
	//     "DLLName" ... �`���[�i�ŗL�֐��̓�����DLL���𒼐ڎw��
	::GetPrivateProfileStringW(L"TUNER", L"UseSpecial", L"AUTO", buf, 256, m_szIniFilePath);
	m_aTunerParam.sDLLBaseName = buf;

	// Tone�M���ؑ֎���Wait����
	m_nToneWait = ::GetPrivateProfileIntW(L"TUNER", L"ToneSignalWait", 100, m_szIniFilePath);

	// CH�ؑ֌��Lock�m�F����
	m_nLockWait = ::GetPrivateProfileIntW(L"TUNER", L"ChannelLockWait", 2000, m_szIniFilePath);

	// CH�ؑ֌��Lock�m�FDelay����
	m_nLockWaitDelay = ::GetPrivateProfileIntW(L"TUNER", L"ChannelLockWaitDelay", 0, m_szIniFilePath);

	// CH�ؑ֌��Lock�m�FRetry��
	m_nLockWaitRetry = ::GetPrivateProfileIntW(L"TUNER", L"ChannelLockWaitRetry", 0, m_szIniFilePath);

	// CH�ؑ֓���������I��2�x�s�����ǂ���
	m_bLockTwice = (BOOL)(::GetPrivateProfileIntW(L"TUNER", L"ChannelLockTwice", 0, m_szIniFilePath));

	// CH�ؑ֓���������I��2�x�s���ꍇ��Delay����
	m_nLockTwiceDelay = ::GetPrivateProfileIntW(L"TUNER", L"ChannelLockTwiceDelay", 100, m_szIniFilePath);

	// SignalLock�ُ̈팟�m����(�b)
	m_nWatchDogSignalLocked = ::GetPrivateProfileIntW(L"TUNER", L"WatchDogSignalLocked", 0, m_szIniFilePath);

	// BitRate�ُ̈팟�m����(�b)
	m_nWatchDogBitRate = ::GetPrivateProfileIntW(L"TUNER", L"WatchDogBitRate", 0, m_szIniFilePath);

	// �ُ팟�m���A�`���[�i�̍ăI�[�v�������݂�܂ł�CH�ؑ֓��쎎�s��
	m_nReOpenWhenGiveUpReLock = ::GetPrivateProfileIntW(L"TUNER", L"ReOpenWhenGiveUpReLock", 0, m_szIniFilePath);

	// �`���[�i�̍ăI�[�v�������݂�ꍇ�ɕʂ̃`���[�i��D�悵�Č������邩�ǂ���
	m_bTryAnotherTuner = (BOOL)(::GetPrivateProfileIntW(L"TUNER", L"TryAnotherTuner", 0, m_szIniFilePath));

	// CH�ؑւɎ��s�����ꍇ�ɁA�ُ팟�m�����l�o�b�N�O�����h��CH�ؑ֓�����s�����ǂ���
	m_bBackgroundChannelLock = (BOOL)(::GetPrivateProfileIntW(L"TUNER", L"BackgroundChannelLock", 0, m_szIniFilePath));

	// Tuning Space���i�݊��p�j
	::GetPrivateProfileStringW(L"TUNER", L"TuningSpaceName", L"�X�J�p�[", buf, 64, m_szIniFilePath);
	wstring sTempTuningSpaceName = buf;

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
	m_nSignalLevelCalcType = ::GetPrivateProfileIntW(L"TUNER", L"SignalLevelCalcType", 0, m_szIniFilePath);
	if (m_nSignalLevelCalcType >= 0 && m_nSignalLevelCalcType <= 9)
		m_bSignalLevelGetTypeSS = TRUE;
	if (m_nSignalLevelCalcType >= 10 && m_nSignalLevelCalcType <= 19)
		m_bSignalLevelGetTypeTuner = TRUE;
	if (m_nSignalLevelCalcType == 100)
		m_bSignalLevelGetTypeBR = TRUE;
	if (m_nSignalLevelCalcType == 0 || m_nSignalLevelCalcType == 2 || m_nSignalLevelCalcType == 3 ||
			m_nSignalLevelCalcType == 10 || m_nSignalLevelCalcType == 12 || m_nSignalLevelCalcType == 13)
		m_bSignalLevelNeedStrength = TRUE;
	if (m_nSignalLevelCalcType == 1 || m_nSignalLevelCalcType == 2 || m_nSignalLevelCalcType == 3 ||
			m_nSignalLevelCalcType == 11 || m_nSignalLevelCalcType == 12 || m_nSignalLevelCalcType == 13)
		m_bSignalLevelNeedQuality = TRUE;
	if (m_nSignalLevelCalcType == 2 || m_nSignalLevelCalcType == 12)
		m_bSignalLevelCalcTypeMul = TRUE;
	if (m_nSignalLevelCalcType == 3 || m_nSignalLevelCalcType == 13)
		m_bSignalLevelCalcTypeAdd = TRUE;

	// Strength �l�␳�W��
	::GetPrivateProfileStringW(L"TUNER", L"StrengthCoefficient", L"1.0", buf, 256, m_szIniFilePath);
	m_fStrengthCoefficient = (float)::_wtof(buf);
	if (m_fStrengthCoefficient == 0.0F)
		m_fStrengthCoefficient = 1.0F;

	// Quality �l�␳�W��
	::GetPrivateProfileStringW(L"TUNER", L"QualityCoefficient", L"1.0", buf, 256, m_szIniFilePath);
	m_fQualityCoefficient = (float)::_wtof(buf);
	if (m_fQualityCoefficient == 0.0F)
		m_fQualityCoefficient = 1.0F;

	// Strength �l�␳�o�C�A�X
	::GetPrivateProfileStringW(L"TUNER", L"StrengthBias", L"0.0", buf, 256, m_szIniFilePath);
	m_fStrengthBias = (float)::_wtof(buf);

	// Quality �l�␳�o�C�A�X
	::GetPrivateProfileStringW(L"TUNER", L"QualityBias", L"0.0", buf, 256, m_szIniFilePath);
	m_fQualityBias = (float)::_wtof(buf);

	// �`���[�j���O��Ԃ̔��f���@
	// 0 .. ��Ƀ`���[�j���O�ɐ������Ă����ԂƂ��Ĕ��f����
	// 1 .. IBDA_SignalStatistics::get_SignalLocked�Ŏ擾�����l�Ŕ��f����
	// 2 .. ITuner::get_SignalStrength�Ŏ擾�����l�Ŕ��f����
	m_nSignalLockedJudgeType = ::GetPrivateProfileIntW(L"TUNER", L"SignalLockedJudgeType", 1, m_szIniFilePath);
	if (m_nSignalLockedJudgeType == 1)
		m_bSignalLockedJudgeTypeSS = TRUE;
	if (m_nSignalLockedJudgeType == 2)
		m_bSignalLockedJudgeTypeTuner = TRUE;

	// �`���[�i�[�̎g�p����TuningSpace�̎��
	//    1 .. DVB-S/DVB-S2
	//    2 .. DVB-T
	//    3 .. DVB-C
	//    4 .. DVB-T2
	//   11 .. ISDB-S
	//   12 .. ISDB-T
	//   21 .. ATSC
	//   22 .. ATSC Cable
	//   23 .. Digital Cable
	m_nDVBSystemType = (enumTunerType)::GetPrivateProfileIntW(L"TUNER", L"DVBSystemType", 1, m_szIniFilePath);

	// �`���[�i�[�Ɏg�p����NetworkProvider
	//    0 .. ����
	//    1 .. Microsoft Network Provider
	//    2 .. Microsoft DVB-S Network Provider
	//    3 .. Microsoft DVB-T Network Provider
	//    4 .. Microsoft DVB-C Network Provider
	//    5 .. Microsoft ATSC Network Provider
	m_nNetworkProvider = (enumNetworkProvider)::GetPrivateProfileIntW(L"TUNER", L"NetworkProvider", 0, m_szIniFilePath);

	// �q����M�p�����[�^/�ϒ������p�����[�^�̃f�t�H���g�l
	//    1 .. SPHD
	//    2 .. BS/CS110
	//    3 .. UHF/CATV
	m_nDefaultNetwork = ::GetPrivateProfileIntW(L"TUNER", L"DefaultNetwork", 1, m_szIniFilePath);

	//
	// BonDriver �Z�N�V����
	//

	// �X�g���[���f�[�^�o�b�t�@1���̃T�C�Y
	// 188�~�ݒ萔(bytes)
	m_dwBuffSize = 188 * ::GetPrivateProfileIntW(L"BONDRIVER", L"BuffSize", 1024, m_szIniFilePath);

	// �X�g���[���f�[�^�o�b�t�@�̍ő��
	m_dwMaxBuffCount = ::GetPrivateProfileIntW(L"BONDRIVER", L"MaxBuffCount", 512, m_szIniFilePath);

	// �X�g���[���f�[�^�o�b�t�@�̍ŏ���
	// 0���w�肳�ꂽ�ꍇ��MaxBuffCount�Ɠ����l��p����
	m_dwMinBuffCount = ::GetPrivateProfileIntW(L"BONDRIVER", L"MinBuffCount", 0, m_szIniFilePath);

	// WaitTsStream���A�w�肳�ꂽ�����̃X�g���[���f�[�^�o�b�t�@�����܂�܂őҋ@����
	// �`���[�i��CPU���ׂ������Ƃ��͐��l��傫�ڂɂ���ƌ��ʂ�����ꍇ������
	m_nWaitTsCount = ::GetPrivateProfileIntW(L"BONDRIVER", L"WaitTsCount", 1, m_szIniFilePath);
	if (m_nWaitTsCount < 1)
		m_nWaitTsCount = 1;

	// WaitTsStream���X�g���[���f�[�^�o�b�t�@�����܂��Ă��Ȃ��ꍇ�ɍŒ���ҋ@���鎞��(msec)
	// �`���[�i��CPU���ׂ������Ƃ���100msec���x���w�肷��ƌ��ʂ�����ꍇ������
	m_nWaitTsSleep = ::GetPrivateProfileIntW(L"BONDRIVER", L"WaitTsSleep", 100, m_szIniFilePath);

	// SetChannel()�Ń`�����l�����b�N�Ɏ��s�����ꍇ�ł�FALSE��Ԃ��Ȃ��悤�ɂ��邩�ǂ���
	m_bAlwaysAnswerLocked = (BOOL)(::GetPrivateProfileIntW(L"BONDRIVER", L"AlwaysAnswerLocked", 0, m_szIniFilePath));

	//
	// Satellite �Z�N�V����
	//

	// �q���ʎ�M�p�����[�^

	// ���ݒ莞�p�iini�t�@�C������̓Ǎ��͍s��Ȃ��j
	m_sSatelliteName[0] = L"not set";						// �`�����l���������p�q������
	// ���̈ȊO�̓R���X�g���N�^�̃f�t�H���g�l�g�p

	// �f�t�H���g�l�ݒ�
	switch (m_nDefaultNetwork) {
	case 1:
		// SPHD
		// �q���ݒ�1�iJCSAT-3A�j
		m_sSatelliteName[1] = L"128.0E";						// �`�����l���������p�q������
		m_aSatellite[1].Polarisation[1].HighOscillator = m_aSatellite[1].Polarisation[1].LowOscillator = 11200000;
																// �����Δg��LNB���g��
		m_aSatellite[1].Polarisation[1].Tone = 0;				// �����Δg���g�[���M��
		m_aSatellite[1].Polarisation[2].HighOscillator = m_aSatellite[1].Polarisation[2].LowOscillator = 11200000;
																// �����Δg��LNB���g��
		m_aSatellite[1].Polarisation[2].Tone = 0;				// �����Δg���g�[���M��

		// �q���ݒ�2�iJCSAT-4B�j
		m_sSatelliteName[2] = L"124.0E";						// �`�����l���������p�q������
		m_aSatellite[2].Polarisation[1].HighOscillator = m_aSatellite[2].Polarisation[1].LowOscillator = 11200000;	
																// �����Δg��LNB���g��
		m_aSatellite[2].Polarisation[1].Tone = 1;				// �����Δg���g�[���M��
		m_aSatellite[2].Polarisation[2].HighOscillator = m_aSatellite[2].Polarisation[2].LowOscillator = 11200000;
																// �����Δg��LNB���g��
		m_aSatellite[2].Polarisation[2].Tone = 1;				// �����Δg���g�[���M��
		break;

	case 2:
		// BS/CS110
		// �q���ݒ�1
		m_sSatelliteName[1] = L"BS/CS110";						// �`�����l���������p�q������
		m_aSatellite[1].Polarisation[3].HighOscillator = m_aSatellite[1].Polarisation[3].LowOscillator = 10678000;
																// �����Δg��LNB���g��
		m_aSatellite[1].Polarisation[3].Tone = 0;				// �����Δg���g�[���M��
		m_aSatellite[1].Polarisation[4].HighOscillator = m_aSatellite[1].Polarisation[4].LowOscillator = 10678000;
																// �����Δg��LNB���g��
		m_aSatellite[1].Polarisation[4].Tone = 0;				// �����Δg���g�[���M��
		break;

	case 3:
		// UHF/CATV�͉q���ݒ�s�v
		break;
	}

	// �q���ݒ�1�`4�̐ݒ��Ǎ�
	for (unsigned int satellite = 1; satellite < MAX_SATELLITE; satellite++) {
		WCHAR keyname[64];
		::swprintf_s(keyname, 64, L"Satellite%01dName", satellite);
		::GetPrivateProfileStringW(L"SATELLITE", keyname, L"", buf, 64, m_szIniFilePath);
		if (buf[0] != L'\0') {
			m_sSatelliteName[satellite] = buf;
		}

		// �Δg���1�`4�̃A���e�i�ݒ��Ǎ�
		for (unsigned int polarisation = 1; polarisation < POLARISATION_SIZE; polarisation++) {
			// �ǔ����g�� (KHz)
			// �S�Δg���ʂł̐ݒ肪����Γǂݍ���
			::swprintf_s(keyname, 64, L"Satellite%01dOscillator", satellite);
			m_aSatellite[satellite].Polarisation[polarisation].LowOscillator = m_aSatellite[satellite].Polarisation[polarisation].HighOscillator
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].HighOscillator, m_szIniFilePath);
			::swprintf_s(keyname, 64, L"Satellite%01dHighOscillator", satellite);
			m_aSatellite[satellite].Polarisation[polarisation].HighOscillator
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].HighOscillator, m_szIniFilePath);
			::swprintf_s(keyname, 64, L"Satellite%01dLowOscillator", satellite);
			m_aSatellite[satellite].Polarisation[polarisation].LowOscillator
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].LowOscillator, m_szIniFilePath);
			// �ʐݒ肪����Ώ㏑���œǂݍ���
			::swprintf_s(keyname, 64, L"Satellite%01d%cOscillator", satellite, PolarisationChar[polarisation]);
			m_aSatellite[satellite].Polarisation[polarisation].LowOscillator = m_aSatellite[satellite].Polarisation[polarisation].HighOscillator
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].HighOscillator, m_szIniFilePath);
			::swprintf_s(keyname, 64, L"Satellite%01d%cHighOscillator", satellite, PolarisationChar[polarisation]);
			m_aSatellite[satellite].Polarisation[polarisation].HighOscillator
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].HighOscillator, m_szIniFilePath);
			::swprintf_s(keyname, 64, L"Satellite%01d%cLowOscillator", satellite, PolarisationChar[polarisation]);
			m_aSatellite[satellite].Polarisation[polarisation].LowOscillator
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].LowOscillator, m_szIniFilePath);

			// LNB�ؑ֎��g�� (KHz)
			// �S�Δg���ʂł̐ݒ肪����Γǂݍ���
			::swprintf_s(keyname, 64, L"Satellite%01dLNBSwitch", satellite);
			m_aSatellite[satellite].Polarisation[polarisation].LNBSwitch
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].LNBSwitch, m_szIniFilePath);
			// �ʐݒ肪����Ώ㏑���œǂݍ���
			::swprintf_s(keyname, 64, L"Satellite%01d%cLNBSwitch", satellite, PolarisationChar[polarisation]);
			m_aSatellite[satellite].Polarisation[polarisation].LNBSwitch
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].LNBSwitch, m_szIniFilePath);

			// �g�[���M�� (0 or 1)
			// �S�Δg���ʂł̐ݒ肪����Γǂݍ���
			::swprintf_s(keyname, 64, L"Satellite%01dToneSignal", satellite);
			m_aSatellite[satellite].Polarisation[polarisation].Tone
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].Tone, m_szIniFilePath);
			// �ʐݒ肪����Ώ㏑���œǂݍ���
			::swprintf_s(keyname, 64, L"Satellite%01d%cToneSignal", satellite, PolarisationChar[polarisation]);
			m_aSatellite[satellite].Polarisation[polarisation].Tone
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].Tone, m_szIniFilePath);

			// DiSEqC
			// �S�Δg���ʂł̐ݒ肪����Γǂݍ���
			::swprintf_s(keyname, 64, L"Satellite%01dDiSEqC", satellite);
			m_aSatellite[satellite].Polarisation[polarisation].DiSEqC
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].DiSEqC, m_szIniFilePath);
			// �ʐݒ肪����Ώ㏑���œǂݍ���
			::swprintf_s(keyname, 64, L"Satellite%01d%cDiSEqC", satellite, PolarisationChar[polarisation]);
			m_aSatellite[satellite].Polarisation[polarisation].DiSEqC
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].DiSEqC, m_szIniFilePath);
		}
	}

	//
	// Modulation �Z�N�V����
	//

	// �ϒ������ʃp�����[�^�i0�`3�̏��Ȃ̂Œ��Ӂj

	// �f�t�H���g�l�ݒ�
	switch (m_nDefaultNetwork) {
	case 1:
		//SPHD
		// �ϒ������ݒ�0�iDVB-S�j
		m_sModulationName[0] = L"DVB-S";							// �`�����l���������p�ϒ���������
		m_aModulationType[0].Modulation = BDA_MOD_NBC_QPSK;			// �ϒ��^�C�v
		m_aModulationType[0].InnerFEC = BDA_FEC_VITERBI;			// �����O���������^�C�v
		m_aModulationType[0].InnerFECRate = BDA_BCC_RATE_3_4;		// ����FEC���[�g
		m_aModulationType[0].OuterFEC = BDA_FEC_RS_204_188;			// �O���O���������^�C�v
		m_aModulationType[0].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// �O��FEC���[�g
		m_aModulationType[0].SymbolRate = 21096;					// �V���{�����[�g

		// �ϒ������ݒ�1�iDVB-S2�j
		m_sModulationName[1] = L"DVB-S2";							// �`�����l���������p�ϒ���������
		m_aModulationType[1].Modulation = BDA_MOD_NBC_8PSK;			// �ϒ��^�C�v
		m_aModulationType[1].InnerFEC = BDA_FEC_VITERBI;			// �����O���������^�C�v
		m_aModulationType[1].InnerFECRate = BDA_BCC_RATE_3_5;		// ����FEC���[�g
		m_aModulationType[1].OuterFEC = BDA_FEC_RS_204_188;			// �O���O���������^�C�v
		m_aModulationType[1].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// �O��FEC���[�g
		m_aModulationType[1].SymbolRate = 23303;					// �V���{�����[�g
		break;

	case 2:
		// BS/CS110
		// �ϒ������ݒ�0
		m_sModulationName[0] = L"ISDB-S";							// �`�����l���������p�ϒ���������
		m_aModulationType[0].Modulation = BDA_MOD_ISDB_S_TMCC;		// �ϒ��^�C�v
		m_aModulationType[0].InnerFEC = BDA_FEC_VITERBI;			// �����O���������^�C�v
		m_aModulationType[0].InnerFECRate = BDA_BCC_RATE_2_3;		// ����FEC���[�g
		m_aModulationType[0].OuterFEC = BDA_FEC_RS_204_188;			// �O���O���������^�C�v
		m_aModulationType[0].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// �O��FEC���[�g
		m_aModulationType[0].SymbolRate = 28860;					// �V���{�����[�g
		break;

	case 3:
		// UHF/CATV
		// �ϒ������ݒ�0
		m_sModulationName[0] = L"ISDB-T";							// �`�����l���������p�ϒ���������
		m_aModulationType[0].Modulation = BDA_MOD_ISDB_T_TMCC;		// �ϒ��^�C�v
		m_aModulationType[0].InnerFEC = BDA_FEC_VITERBI;			// �����O���������^�C�v
		m_aModulationType[0].InnerFECRate = BDA_BCC_RATE_3_4;		// ����FEC���[�g
		m_aModulationType[0].OuterFEC = BDA_FEC_RS_204_188;			// �O���O���������^�C�v
		m_aModulationType[0].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// �O��FEC���[�g
		m_aModulationType[0].SymbolRate = -1;						// �V���{�����[�g
		m_aModulationType[0].BandWidth = 6;							// �ш敝(MHz)
		break;
	}

	// �ϒ������ݒ�0�`3�̒l��Ǎ�
	for (unsigned int modulation = 0; modulation < MAX_MODULATION; modulation++) {
		WCHAR keyname[64];
		// �`�����l���������p�ϒ���������
		::swprintf_s(keyname, 64, L"ModulationType%01dName", modulation);
		::GetPrivateProfileStringW(L"MODULATION", keyname, L"", buf, 64, m_szIniFilePath);
		if (buf[0] != L'\0') {
			m_sModulationName[modulation] = buf;
		}

		// �ϒ��^�C�v
		::swprintf_s(keyname, 64, L"ModulationType%01dModulation", modulation);
		m_aModulationType[modulation].Modulation
			= (ModulationType)::GetPrivateProfileIntW(L"MODULATION", keyname, m_aModulationType[modulation].Modulation, m_szIniFilePath);

		// �����O���������^�C�v
		::swprintf_s(keyname, 64, L"ModulationType%01dInnerFEC", modulation);
		m_aModulationType[modulation].InnerFEC
			= (FECMethod)::GetPrivateProfileIntW(L"MODULATION", keyname, m_aModulationType[modulation].InnerFEC, m_szIniFilePath);

		// ����FEC���[�g
		::swprintf_s(keyname, 64, L"ModulationType%01dInnerFECRate", modulation);
		m_aModulationType[modulation].InnerFECRate
			= (BinaryConvolutionCodeRate)::GetPrivateProfileIntW(L"MODULATION", keyname, m_aModulationType[modulation].InnerFECRate, m_szIniFilePath);

		// �O���O���������^�C�v
		::swprintf_s(keyname, 64, L"ModulationType%01dOuterFEC", modulation);
		m_aModulationType[modulation].OuterFEC
			= (FECMethod)::GetPrivateProfileIntW(L"MODULATION", keyname, m_aModulationType[modulation].OuterFEC, m_szIniFilePath);

		// �O��FEC���[�g
		::swprintf_s(keyname, 64, L"ModulationType%01dOuterFECRate", modulation);
		m_aModulationType[modulation].OuterFECRate
			= (BinaryConvolutionCodeRate)::GetPrivateProfileIntW(L"MODULATION", keyname, m_aModulationType[modulation].OuterFECRate, m_szIniFilePath);

		// �V���{�����[�g
		::swprintf_s(keyname, 64, L"ModulationType%01dSymbolRate", modulation);
		m_aModulationType[modulation].SymbolRate
			= (long)::GetPrivateProfileIntW(L"MODULATION", keyname, m_aModulationType[modulation].SymbolRate, m_szIniFilePath);

		// �ш敝(MHz)
		::swprintf_s(keyname, 64, L"ModulationType%01dBandWidth", modulation);
		m_aModulationType[modulation].BandWidth
			= (long)::GetPrivateProfileIntW(L"MODULATION", keyname, m_aModulationType[modulation].BandWidth, m_szIniFilePath);
	}

	//
	// Channel �Z�N�V����
	//

	// ini�t�@�C������CH�ݒ��Ǎ��ލۂ�
	// �g�p����Ă��Ȃ�CH�ԍ��������Ă��O�l�����m�ۂ��Ă������ǂ���
	// 0 .. �g�p����ĂȂ��ԍ����������ꍇ�O�l���A��������
	// 1 .. �g�p����Ă��Ȃ��ԍ������̂܂܋�CH�Ƃ��Ċm�ۂ��Ă���
	m_bReserveUnusedCh = (BOOL)(::GetPrivateProfileIntW(L"CHANNEL", L"ReserveUnusedCh", 0, m_szIniFilePath));

	map<unsigned int, TuningSpaceData*>::iterator itSpace;
	map<unsigned int, ChData*>::iterator itCh;
	// �`���[�j���O���00�`99�̐ݒ��Ǎ�
	for (DWORD space = 0; space < 100; space++)	{
		DWORD result;
		WCHAR sectionname[64];

		::swprintf_s(sectionname, 64, L"TUNINGSPACE%02d", space);
		result = ::GetPrivateProfileSection(sectionname, buf, 256, m_szIniFilePath);
		if (result <= 0) {
			// TuningSpaceXX�̃Z�N�V���������݂��Ȃ��ꍇ
			if (space != 0)
				continue;
			// TuningSpace00�̎���Channel�Z�N�V����������
			::swprintf_s(sectionname, 64, L"CHANNEL");
		}

		// ���Ƀ`���[�j���O��ԃf�[�^�����݂���ꍇ�͂��̓��e������������
		// �����ꍇ�͋�̃`���[�j���O��Ԃ��쐬
		itSpace = m_TuningData.Spaces.find(space);
		if (itSpace == m_TuningData.Spaces.end()) {
			TuningSpaceData *tuningSpaceData = new TuningSpaceData();
			itSpace = m_TuningData.Spaces.insert(m_TuningData.Spaces.begin(), pair<unsigned int, TuningSpaceData*>(space, tuningSpaceData));
		}

		// Tuning Space��
		wstring temp;
		if (space == 0)
			temp = sTempTuningSpaceName;
		else
			temp = L"NoName";
		::GetPrivateProfileStringW(sectionname, L"TuningSpaceName", temp.c_str(), buf, 64, m_szIniFilePath);
		itSpace->second->sTuningSpaceName = buf;

		// UHF/CATV��CH�ݒ��������������
		::GetPrivateProfileStringW(sectionname, L"ChannelSettingsAuto", L"", buf, 256, m_szIniFilePath);
		temp = buf;
		if (temp == L"UHF") {
			for (unsigned int ch = 0; ch < 50; ch++) {
				itCh = itSpace->second->Channels.find(ch);
				if (itCh == itSpace->second->Channels.end()) {
					ChData *chData = new ChData();
					itCh = itSpace->second->Channels.insert(itSpace->second->Channels.begin(), pair<unsigned int, ChData*>(ch, chData));
				}

				itCh->second->Satellite = 0;
				itCh->second->Polarisation = 0;
				itCh->second->ModulationType = 0;
				itCh->second->Frequency = 473000 + 6000 * ch;
				::swprintf_s(buf, 256, L"%02dch", ch + 13);
#ifdef UNICODE
				itCh->second->sServiceName = buf;
#else
				::wcstombs_s(NULL, charBuf, 512, buf, _TRUNCATE);
				chData->sServiceName = charBuf;
#endif
			}
			itSpace->second->dwNumChannel = 50;
		}
		else if (temp == L"CATV") {
			for (unsigned int ch = 0; ch < 51; ch++) {
				itCh = itSpace->second->Channels.find(ch);
				if (itCh == itSpace->second->Channels.end()) {
					ChData *chData = new ChData();
					itCh = itSpace->second->Channels.insert(itSpace->second->Channels.begin(), pair<unsigned int, ChData*>(ch, chData));
				}

				itCh->second->Satellite = 0;
				itCh->second->Polarisation = 0;
				itCh->second->ModulationType = 0;
				long f;
				if (ch <= 22 - 13) {
					f = 111000 + 6000 * ch;
					if (ch == 22 - 13) {
						f += 2000;
					}
				}
				else {
					f = 225000 + 6000 * (ch - (23 - 13));
					if (ch >= 24 - 13 && ch <= 27 - 13) {
						f += 2000;
					}
				}
				itCh->second->Frequency = f;
				::swprintf_s(buf, 256, L"C%02dch", ch + 13);
#ifdef UNICODE
				itCh->second->sServiceName = buf;
#else
				::wcstombs_s(NULL, charBuf, 512, buf, _TRUNCATE);
				chData->sServiceName = charBuf;
#endif
			}
			itSpace->second->dwNumChannel = 51;
		}

		// CH�ݒ�
		//    �`�����l���ԍ� = �q���ԍ�,���g��,�Δg,�ϒ�����[,�`�����l����[,SID/MinorChannel[,TSID/Channel[,ONID/PhysicalChannel[,MajorChannel[,SourceID]]]]]]
		//    ��: CH001 = 1,12658,V,0
		//      �`�����l���ԍ� : CH000�`CH999�Ŏw��
		//      �q���ԍ�       : Sattelite�Z�N�V�����Őݒ肵���q���ԍ�(1�`4) �܂��� 0(���w�莞)
		//                       (�n�f�W�`���[�i�[����0���w�肵�Ă�������)
		//      ���g��         : ���g����MHz�Ŏw��
		//                       (�����_��t���邱�Ƃɂ��KHz�P�ʂł̎w�肪�\�ł�)
		//      �Δg           : 'V' = �����Δg 'H'=�����Δg 'L'=�����~�Δg 'R'=�E���~�Δg ' '=���w��
		//                       (�n�f�W�`���[�i�[���͖��w��)
		//      �ϒ�����       : Modulation�Z�N�V�����Őݒ肵���ϒ������ԍ�(0�`3)
		//      �`�����l����   : �`�����l������
		//                       (�ȗ������ꍇ�� 128.0E / 12658H / DVB - S �̂悤�Ȍ`���Ŏ�����������܂�)
		//      SID            : DVB / ISDB�̃T�[�r�XID
		//      TSID           : DVB / ISDB�̃g�����X�|�[�g�X�g���[��ID
		//      ONID           : DVB / ISDB�̃I���W�i���l�b�g���[�NID
		//      MinorChannel   : ATSC / Digital Cable��Minor Channel
		//      Channel        : ATSC / Digital Cable��Channel
		//      PhysicalChannel: ATSC / Digital Cable��Physical Channel
		//      MajorChannel   : Digital Cable��Major Channel
		//      SourceID       : Digital Cable��SourceID
		for (DWORD ch = 0; ch < 1000; ch++) {
			WCHAR keyname[64];

			::swprintf_s(keyname, 64, L"CH%03d", ch);
			result = ::GetPrivateProfileStringW(sectionname, keyname, L"", buf, 256, m_szIniFilePath);
			if (result <= 0)
				continue;

			// �ݒ�s���L����
			// ReserveUnusedCh���w�肳��Ă���ꍇ��CH�ԍ����㏑������
			DWORD chNum = m_bReserveUnusedCh ? ch : (DWORD)(itSpace->second->Channels.size());
			itCh = itSpace->second->Channels.find(chNum);
			if (itCh == itSpace->second->Channels.end()) {
				ChData *chData = new ChData();
				itCh = itSpace->second->Channels.insert(itSpace->second->Channels.begin(), pair<unsigned int, ChData*>(chNum, chData));
			}

			WCHAR szSatellite[256] = L"";
			WCHAR szFrequency[256] = L"";
			WCHAR szPolarisation[256] = L"";
			WCHAR szModulationType[256] = L"";
			WCHAR szServiceName[256] = L"";
			WCHAR szSID[256] = L"";
			WCHAR szTSID[256] = L"";
			WCHAR szONID[256] = L"";
			WCHAR szMajorChannel[256] = L"";
			WCHAR szSourceID[256] = L"";
			::swscanf_s(buf, L"%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,]", szSatellite, 256, szFrequency, 256,
				szPolarisation, 256, szModulationType, 256, szServiceName, 256, szSID, 256, szTSID, 256, szONID, 256, szMajorChannel, 256, szSourceID, 256);

			// �q���ԍ�
			val = _wtoi(szSatellite);
			if (val >= 0 && val < MAX_SATELLITE) {
				itCh->second->Satellite = val;
			}
			else
				OutputDebug(L"Format Error in readIniFile; Wrong Bird.\n");

			// ���g��
			WCHAR szMHz[256] = L"";
			WCHAR szKHz[256] = L"";
			::swscanf_s(szFrequency, L"%[^.].%[^.]", szMHz, 256, szKHz, 256);
			val = _wtoi(szMHz) * 1000 + _wtoi(szKHz);
			if ((val > 0) && (val <= 20000000)) {
				itCh->second->Frequency = val;
			}
			else
				OutputDebug(L"Format Error in readIniFile; Wrong Frequency.\n");

			// �Δg���
			if (szPolarisation[0] == L' ')
				szPolarisation[0] = L'\0';
			val = -1;
			for (unsigned int i = 0; i < POLARISATION_SIZE; i++) {
				if (szPolarisation[0] == PolarisationChar[i]) {
					val = i;
					break;
				}
			}
			if (val != -1) {
				itCh->second->Polarisation = val;
			}
			else
				OutputDebug(L"Format Error in readIniFile; Wrong Polarization.\n");

			// �ϒ�����
			val = _wtoi(szModulationType);
			if (val >= 0 && val < MAX_MODULATION) {
				itCh->second->ModulationType = val;
			}
			else
				OutputDebug(L"Format Error in readIniFile; Wrong Method.\n");

			// �`�����l����
			if (szServiceName[0] == 0)
				// ini�t�@�C���Ŏw�肵�����̂��Ȃ����128.0E/12658H/DVB-S �̂悤�Ȍ`���ō쐬����
				MakeChannelName(szServiceName, 256, itCh->second);

#ifdef UNICODE
			itCh->second->sServiceName = szServiceName;
#else
			::wcstombs_s(NULL, charBuf, 512, szServiceName, _TRUNCATE);
			chData->sServiceName = charBuf;
#endif

			// SID / PhysicalChannel
			if (szSID[0] != 0) {
				itCh->second->SID = wcstol(szSID, NULL, 0);
			}

			// TSID / Channel
			if (szTSID[0] != 0) {
				itCh->second->TSID = wcstol(szTSID, NULL, 0);
			}

			// ONID / MinorChannel
			if (szONID[0] != 0) {
				itCh->second->ONID = wcstol(szONID, NULL, 0);
			}

			// MajorChannel
			if (szMajorChannel[0] != 0) {
				itCh->second->MajorChannel = wcstol(szMajorChannel, NULL, 0);
			}

			// SourceID
			if (szSourceID[0] != 0) {
				itCh->second->SourceID = wcstol(szSourceID, NULL, 0);
			}
		}

		// CH�ԍ��̍ő�l + 1
		itCh = itSpace->second->Channels.end();
		if (itCh == itSpace->second->Channels.begin()) {
			itSpace->second->dwNumChannel = 0;
		}
		else {
			itCh--;
			itSpace->second->dwNumChannel = itCh->first + 1;
		}

		// CH�ؑ֓���������I��2�x�s���ꍇ�̑Ώ�CH
		if (m_bLockTwice) {
			unsigned int len = ::GetPrivateProfileStringW(sectionname, L"ChannelLockTwiceTarget", L"", buf, 256, m_szIniFilePath);
			if (len > 0) {
				WCHAR szToken[256];
				unsigned int nPos = 0;
				int nTokenLen;
				while (nPos < len) {
					// �J���}��؂�܂ł̕�������擾
					::swscanf_s(&buf[nPos], L"%[^,]%n", szToken, 256, &nTokenLen);
					if (nTokenLen) {
						// �����'-'��؂�̐��l�ɕ���
						DWORD begin = 0;
						DWORD end = itSpace->second->dwNumChannel - 1;
						WCHAR s1[256] = L"";
						WCHAR s2[256] = L"";
						WCHAR s3[256] = L"";
						int num = ::swscanf_s(szToken, L" %[0-9] %[-] %[0-9]", s1, 256, s2, 256, s3, 256);
						switch (num)
						{
						case 1:
							// "10"�̌`���i�P�Ǝw��j
							begin = end = _wtoi(s1);
							break;
						case 2:
							// "10-"�̌`��
							begin = _wtoi(s1);
							break;
						case 3:
							// "10-15"�̌`��
							begin = _wtoi(s1);
							end = _wtoi(s3);
							break;
						case 0:
							num = ::swscanf_s(szToken, L" %[-] %[0-9]", s2, 256, s3, 256);
							if (num == 2) {
								// "-10"�̌`��
								end = _wtoi(s3);
							}
							else {
								// ��͕s�\
								OutputDebug(L"Format Error in readIniFile; ChannelLockTwiceTarget.\n");
								continue;
							}
							break;
						}
						// �Ώ۔͈͂�CH��Flag��Set����
						for (DWORD ch = begin; ch <= end; ch++) {
							itCh = itSpace->second->Channels.find(ch);
							if (itCh != itSpace->second->Channels.end()) {
								itCh->second->LockTwiceTarget = TRUE;
							}
						}
					} // if (nTokenLen)
					nPos += nTokenLen + 1;
				} // while (nPos < len)
			} // if (len > 0) 
			else {
				// ChannelLockTwiceTarget�̎w�肪�����ꍇ�͂��ׂĂ�CH���Ώ�
				for (DWORD ch = 0; ch < itSpace->second->dwNumChannel - 1; ch++) {
					itCh = itSpace->second->Channels.find(ch);
					if (itCh != itSpace->second->Channels.end()) {
						itCh->second->LockTwiceTarget = TRUE;
					}
				}
			}
		} // if (m_bLockTwice)
	}

	// �`���[�j���O��Ԕԍ�0��T��
	itSpace = m_TuningData.Spaces.find(0);
	if (itSpace == m_TuningData.Spaces.end()) {
		// �����ɂ͗��Ȃ��͂������ǈꉞ
		// ���TuningSpaceData���`���[�j���O��Ԕԍ�0�ɑ}��
		TuningSpaceData *tuningSpaceData = new TuningSpaceData;
		itSpace = m_TuningData.Spaces.insert(m_TuningData.Spaces.begin(), pair<unsigned int, TuningSpaceData*>(0, tuningSpaceData));
	}

	if (!itSpace->second->Channels.size()) {
		// CH��`���������Ă��Ȃ�
		if (m_nDefaultNetwork == 1) {
			// SPHD�̏ꍇ�̂݉ߋ��̃o�[�W�����݊�����
			// 3��TP���f�t�H���g�ŃZ�b�g���Ă���
			ChData *chData;
			//   128.0E 12.658GHz V DVB-S *** 2015-10-10���݁ANIT�ɂ͑��݂��邯�ǒ�g��
			chData = new ChData();
			chData->Satellite = 1;
			chData->Polarisation = 2;
			chData->ModulationType = 0;
			chData->Frequency = 12658000;
			MakeChannelName(buf, 256, chData);
#ifdef UNICODE
			chData->sServiceName = buf;
#else
			::wcstombs_s(NULL, charBuf, 512, buf, _TRUNCATE);
			chData->sServiceName = charBuf;
#endif
			itSpace->second->Channels.insert(pair<unsigned int, ChData*>(0, chData));
			//   124.0E 12.613GHz H DVB-S2
			chData = new ChData();
			chData->Satellite = 2;
			chData->Polarisation = 1;
			chData->ModulationType = 1;
			chData->Frequency = 12613000;
			MakeChannelName(buf, 256, chData);
#ifdef UNICODE
			chData->sServiceName = buf;
#else
			::wcstombs_s(NULL, charBuf, 512, buf, _TRUNCATE);
			chData->sServiceName = charBuf;
#endif
			itSpace->second->Channels.insert(pair<unsigned int, ChData*>(1, chData));
			//   128.0E 12.733GHz H DVB-S2
			chData = new ChData();
			chData->Satellite = 1;
			chData->Polarisation = 1;
			chData->ModulationType = 1;
			chData->Frequency = 12733000;
			MakeChannelName(buf, 256, chData);
#ifdef UNICODE
			chData->sServiceName = buf;
#else
			::wcstombs_s(NULL, charBuf, 512, buf, _TRUNCATE);
			chData->sServiceName = charBuf;
#endif
			itSpace->second->Channels.insert(pair<unsigned int, ChData*>(2, chData));
			itSpace->second->dwNumChannel = 3;
		}
	}

	// �`���[�j���O��Ԃ̐�
	itSpace = m_TuningData.Spaces.end();
	if (itSpace == m_TuningData.Spaces.begin()) {
		// ���������ꉞ
		m_TuningData.dwNumSpace = 0;
	}
	else {
		itSpace--;
		m_TuningData.dwNumSpace = itSpace->first + 1;
	}
}

void CBonTuner::GetSignalState(int* pnStrength, int* pnQuality, int* pnLock)
{
	if (pnStrength) *pnStrength = 0;
	if (pnQuality) *pnQuality = 0;
	if (pnLock) *pnLock = 1;

	// �`���[�i�ŗL GetSignalState ������΁A�ۓ���
	HRESULT hr;
	if ((m_pIBdaSpecials) && (hr = m_pIBdaSpecials->GetSignalState(pnStrength, pnQuality, pnLock)) != E_NOINTERFACE) {
		// E_NOINTERFACE �łȂ���΁A�ŗL�֐����������Ƃ������Ȃ̂ŁA
		// ���̂܂܃��^�[��
		return;
	}

	if (m_pTunerDevice == NULL)
		return;

	long longVal;
	BYTE byteVal;

	if (m_pITuner) {
		if ((m_bSignalLevelGetTypeTuner && ((m_bSignalLevelNeedStrength && pnStrength) || (m_bSignalLevelNeedQuality && pnQuality))) ||
				(m_bSignalLockedJudgeTypeTuner && pnLock)) {
			longVal = 0;
			if (SUCCEEDED(hr = m_pITuner->get_SignalStrength(&longVal))) {
				int strength = (int)(longVal & 0xffff);
				int quality = (int)(longVal >> 16);
				if (m_bSignalLevelNeedStrength && pnStrength)
					*pnStrength = strength < 0 ? 0xffff - strength : strength;
				if (m_bSignalLevelNeedQuality && pnQuality)
					*pnQuality = min(max(quality, 0), 100);
				if (m_bSignalLockedJudgeTypeTuner && pnLock)
					*pnLock = strength > 0 ? 1 : 0;
			}
		}
	}

	if (m_pIBDA_SignalStatistics) {
		if (m_bSignalLevelGetTypeSS) {
			if (m_bSignalLevelNeedStrength && pnStrength) {
				longVal = 0;
				if (SUCCEEDED(hr = m_pIBDA_SignalStatistics->get_SignalStrength(&longVal)))
					*pnStrength = (int)(longVal & 0xffff);
			}

			if (m_bSignalLevelNeedQuality && pnQuality) {
				longVal = 0;
				if (SUCCEEDED(hr = m_pIBDA_SignalStatistics->get_SignalQuality(&longVal)))
					*pnQuality = (int)(min(max(longVal & 0xffff, 0), 100));
			}
		}

		if (m_bSignalLockedJudgeTypeSS && pnLock) {
			byteVal = 0;
			if (SUCCEEDED(hr = m_pIBDA_SignalStatistics->get_SignalLocked(&byteVal)))
				*pnLock = (int)byteVal;
		}
	}

	return;
}

BOOL CBonTuner::LockChannel(const TuningParam *pTuningParam, BOOL bLockTwice)
{
	HRESULT hr;

	// �`���[�i�ŗL LockChannel ������΁A�ۓ���
	if (m_pIBdaSpecials2 && (hr = m_pIBdaSpecials2->LockChannel(pTuningParam)) != E_NOINTERFACE) {
		// BonDriver_BDA����pDLL
		// E_NOINTERFACE �łȂ���΁A�ŗL�֐����������Ƃ������Ȃ̂ŁA
		// ���̒��őI�Ǐ������s�Ȃ��Ă���͂��B����Ă��̂܂܃��^�[��
		m_nCurTone = pTuningParam->Antenna->Tone;
		if (SUCCEEDED(hr) && bLockTwice) {
			OutputDebug(L"TwiceLock 1st[Special2] SUCCESS.\n");
			::Sleep(m_nLockTwiceDelay);
			hr = m_pIBdaSpecials2->LockChannel(pTuningParam);
		}
		if (SUCCEEDED(hr)) {
			OutputDebug(L"LockChannel[Special2] SUCCESS.\n");
			return TRUE;
		} else {
			OutputDebug(L"LockChannel[Special2] FAIL.\n");
			return FALSE;
		}
	}

	if (m_pIBdaSpecials && (hr = m_pIBdaSpecials->LockChannel(pTuningParam->Antenna->Tone ? 1 : 0, (pTuningParam->Polarisation == BDA_POLARISATION_LINEAR_H) ? TRUE : FALSE, pTuningParam->Frequency / 1000,
			(pTuningParam->Modulation->Modulation == BDA_MOD_NBC_8PSK || pTuningParam->Modulation->Modulation == BDA_MOD_8PSK) ? TRUE : FALSE)) != E_NOINTERFACE) {
		// BonDriver_BDA�I���W�i���݊�DLL
		// E_NOINTERFACE �łȂ���΁A�ŗL�֐����������Ƃ������Ȃ̂ŁA
		// ���̒��őI�Ǐ������s�Ȃ��Ă���͂��B����Ă��̂܂܃��^�[��
		m_nCurTone = pTuningParam->Antenna->Tone;
		if (SUCCEEDED(hr) && bLockTwice) {
			OutputDebug(L"TwiceLock 1st[Special] SUCCESS.\n");
			::Sleep(m_nLockTwiceDelay);
			hr = m_pIBdaSpecials->LockChannel(pTuningParam->Antenna->Tone ? 1 : 0, (pTuningParam->Polarisation == BDA_POLARISATION_LINEAR_H) ? TRUE : FALSE, pTuningParam->Frequency / 1000,
					(pTuningParam->Modulation->Modulation == BDA_MOD_NBC_8PSK || pTuningParam->Modulation->Modulation == BDA_MOD_8PSK) ? TRUE : FALSE);
		}
		if (SUCCEEDED(hr)) {
			OutputDebug(L"LockChannel[Special] SUCCESS.\n");
			return TRUE;
		}
		else {
			OutputDebug(L"LockChannel[Special] FAIL.\n");
			return FALSE;
		}
	}

	// �`���[�i�ŗL�g�[������֐�������΁A����������ŌĂяo��
	if (m_pIBdaSpecials2 && (hr = m_pIBdaSpecials2->Set22KHz(pTuningParam->Antenna->Tone)) != E_NOINTERFACE) {
		// BonDriver_BDA����pDLL
		if (SUCCEEDED(hr)) {
			OutputDebug(L"Set22KHz[Special2] successfully.\n");
			if (pTuningParam->Antenna->Tone != m_nCurTone) {
				m_nCurTone = pTuningParam->Antenna->Tone;
				::Sleep(m_nToneWait); // �q���֑ؑ҂�
			}
		}
		else {
			OutputDebug(L"Set22KHz[Special2] failed.\n");
			// BDA generic �ȕ��@�Ő؂�ւ�邩������Ȃ��̂ŁA���b�Z�[�W�����o���āA���̂܂ܑ��s
		}
	}
	else if (m_pIBdaSpecials && (hr = m_pIBdaSpecials->Set22KHz(pTuningParam->Antenna->Tone ? 1 : 0)) != E_NOINTERFACE) {
		// BonDriver_BDA�I���W�i���݊�DLL
		if (SUCCEEDED(hr)) {
			OutputDebug(L"Set22KHz[Special] successfully.\n");
			if (pTuningParam->Antenna->Tone != m_nCurTone) {
				m_nCurTone = pTuningParam->Antenna->Tone;
				::Sleep(m_nToneWait); // �q���֑ؑ҂�
			}
		}
		else {
			OutputDebug(L"Set22KHz[Special] failed.\n");
			// BDA generic �ȕ��@�Ő؂�ւ�邩������Ȃ��̂ŁA���b�Z�[�W�����o���āA���̂܂ܑ��s
		}
	}
	else {
		// �ŗL�֐����Ȃ������Ȃ̂ŁA��������
	}

	// IDVBSTuningSpace���L
	{
		CComQIPtr<IDVBSTuningSpace> pIDVBSTuningSpace(m_pITuningSpace);
		if (pIDVBSTuningSpace) {
			// LNB ���g����ݒ�
			if (pTuningParam->Antenna->HighOscillator != -1) {
				pIDVBSTuningSpace->put_HighOscillator(pTuningParam->Antenna->HighOscillator);
			}
			if (pTuningParam->Antenna->LowOscillator != -1) {
				pIDVBSTuningSpace->put_LowOscillator(pTuningParam->Antenna->LowOscillator);
			}

			// LNB�X�C�b�`�̎��g����ݒ�
			if (pTuningParam->Antenna->LNBSwitch != -1) {
				// LNBSwitch���g���̐ݒ肪����Ă���
				pIDVBSTuningSpace->put_LNBSwitch(pTuningParam->Antenna->LNBSwitch);
			}
			else {
				// 10GHz��ݒ肵�Ă�����High���ɁA20GHz��ݒ肵�Ă�����Low���ɐؑւ��͂�
				pIDVBSTuningSpace->put_LNBSwitch((pTuningParam->Antenna->Tone != 0) ? 10000000 : 20000000);
			}

			// �ʑ��ϒ��X�y�N�g�����]�̎��
			pIDVBSTuningSpace->put_SpectralInversion(BDA_SPECTRAL_INVERSION_AUTOMATIC);
		}
	}

	// ILocator�擾
	CComPtr<ILocator> pILocator;
	if (FAILED(hr = m_pITuningSpace->get_DefaultLocator(&pILocator)) || !pILocator) {
		OutputDebug(L"Fail to get ILocator.\n");
		return FALSE;
	}

	// RF �M���̎��g����ݒ�
	pILocator->put_CarrierFrequency(pTuningParam->Frequency);

	// �����O���������̃^�C�v��ݒ�
	pILocator->put_InnerFEC(pTuningParam->Modulation->InnerFEC);

	// ���� FEC ���[�g��ݒ�
	// �O�������������Ŏg���o�C�i�� �R���{���[�V�����̃R�[�h ���[�g DVB-S�� 3/4 S2�� 3/5
	pILocator->put_InnerFECRate(pTuningParam->Modulation->InnerFECRate);

	// �ϒ��^�C�v��ݒ�
	// DVB-S��QPSK�AS2�̏ꍇ�� 8PSK
	pILocator->put_Modulation(pTuningParam->Modulation->Modulation);

	// �O���O���������̃^�C�v��ݒ�
	//	���[�h-�\������ 204/188 (�O�� FEC), DVB-S2�ł�����
	pILocator->put_OuterFEC(pTuningParam->Modulation->OuterFEC);

	// �O�� FEC ���[�g��ݒ�
	pILocator->put_OuterFECRate(pTuningParam->Modulation->OuterFECRate);

	// QPSK �V���{�� ���[�g��ݒ�
	pILocator->put_SymbolRate(pTuningParam->Modulation->SymbolRate);

	// IDVBSLocator���L
	{
		CComQIPtr<IDVBSLocator> pIDVBSLocator(pILocator);
		if (pIDVBSLocator) {
			// �M���̕Δg��ݒ�
			pIDVBSLocator->put_SignalPolarisation(pTuningParam->Polarisation);
		}
	}

	// IDVBSLocator2���L
	{
		CComQIPtr<IDVBSLocator2> pIDVBSLocator2(pILocator);
		if (pIDVBSLocator2) {
			// DiSEqC��ݒ�
			if (pTuningParam->Antenna->DiSEqC >= BDA_LNB_SOURCE_A) {
				pIDVBSLocator2->put_DiseqLNBSource((LNB_Source)(pTuningParam->Antenna->DiSEqC));
			}
		}
	}

	// IDVBTLocator���L
	{
		CComQIPtr<IDVBTLocator> pIDVBTLocator(pILocator);
		if (pIDVBTLocator) {
			// ���g���̑ш敝 (MHz)��ݒ�
			if (pTuningParam->Modulation->BandWidth != -1) {
				pIDVBTLocator->put_Bandwidth(pTuningParam->Modulation->BandWidth);
			}
		}
	}

	// IATSCLocator���L
	{
		CComQIPtr<IATSCLocator> pIATSCLocator(pILocator);
		if (pIATSCLocator) {
			// ATSC PhysicalChannel
			if (pTuningParam->PhysicalChannel != -1) {
				pIATSCLocator->put_PhysicalChannel(pTuningParam->PhysicalChannel);
			}
		}
	}

	// ITuneRequest�쐬
	CComPtr<ITuneRequest> pITuneRequest;
	if (FAILED(hr = m_pITuningSpace->CreateTuneRequest(&pITuneRequest))) {
		OutputDebug(L"Fail to create ITuneRequest.\n");
		return FALSE;
	}

	// ITuneRequest��ILocator��ݒ�
	hr = pITuneRequest->put_Locator(pILocator);

	// IDVBTuneRequest���L
	{
		CComQIPtr<IDVBTuneRequest> pIDVBTuneRequest(pITuneRequest);
		if (pIDVBTuneRequest) {
			// DVB Triplet ID�̐ݒ�
			pIDVBTuneRequest->put_ONID(pTuningParam->ONID);
			pIDVBTuneRequest->put_TSID(pTuningParam->TSID);
			pIDVBTuneRequest->put_SID(pTuningParam->SID);
		}
	}

	// IChannelTuneRequest���L
	{
		CComQIPtr<IChannelTuneRequest> pIChannelTuneRequest(pITuneRequest);
		if (pIChannelTuneRequest) {
			// ATSC Channel
			pIChannelTuneRequest->put_Channel(pTuningParam->Channel);
		}
	}

	// IATSCChannelTuneRequest���L
	{
		CComQIPtr<IATSCChannelTuneRequest> pIATSCChannelTuneRequest(pITuneRequest);
		if (pIATSCChannelTuneRequest) {
			// ATSC MinorChannel
			pIATSCChannelTuneRequest->put_MinorChannel(pTuningParam->MinorChannel);
		}
	}

	// IDigitalCableTuneRequest���L
	{
		CComQIPtr<IDigitalCableTuneRequest> pIDigitalCableTuneRequest(pITuneRequest);
		if (pIDigitalCableTuneRequest) {
			// Digital Cable MinorChannel
			pIDigitalCableTuneRequest->put_MajorChannel(pTuningParam->MinorChannel);
			// Digital Cable SourceID
			pIDigitalCableTuneRequest->put_SourceID(pTuningParam->SourceID);
		}
	}

	if (m_pIBdaSpecials2) {
		// m_pIBdaSpecials��put_TuneRequest�̑O�ɉ��炩�̏������K�v�Ȃ�s��
		hr = m_pIBdaSpecials2->PreTuneRequest(pTuningParam, pITuneRequest);
	}

	if (pTuningParam->Antenna->Tone != m_nCurTone) {
		//�g�[���ؑւ���̏ꍇ�A��Ɉ�xTuneRequest���Ă���
		OutputDebug(L"Requesting pre tune.\n");
		if (FAILED(hr = m_pITuner->put_TuneRequest(pITuneRequest))) {
			OutputDebug(L"Fail to put pre tune request.\n");
			return FALSE;
		}
		OutputDebug(L"Pre tune request complete.\n");

		m_nCurTone = pTuningParam->Antenna->Tone;
		::Sleep(m_nToneWait); // �q���֑ؑ҂�
	}

	if (bLockTwice) {
		// TuneRequest�������I��2�x�s��
		OutputDebug(L"Requesting 1st twice tune.\n");
		if (FAILED(hr = m_pITuner->put_TuneRequest(pITuneRequest))) {
			OutputDebug(L"Fail to put 1st twice tune request.\n");
			return FALSE;
		}
		OutputDebug(L"1st Twice tune request complete.\n");
		::Sleep(m_nLockTwiceDelay);
	}

	unsigned int nRetryRemain = m_nLockWaitRetry;
	int nLock = 0;
	do {
		OutputDebug(L"Requesting tune.\n");
		if (FAILED(hr = m_pITuner->put_TuneRequest(pITuneRequest))) {
			OutputDebug(L"Fail to put tune request.\n");
			return FALSE;
		}
		OutputDebug(L"Tune request complete.\n");

		static const int LockRetryTime = 50;
		unsigned int nWaitRemain = m_nLockWait;
		::Sleep(m_nLockWaitDelay);
		GetSignalState(NULL, NULL, &nLock);
		while (!nLock && nWaitRemain) {
			DWORD dwSleepTime = (nWaitRemain > LockRetryTime) ? LockRetryTime : nWaitRemain;
			OutputDebug(L"Waiting lock status after %d msec.\n", nWaitRemain);
			::Sleep(dwSleepTime);
			nWaitRemain -= dwSleepTime;
			GetSignalState(NULL, NULL, &nLock);
		}
	} while (!nLock && nRetryRemain--);

	if (nLock != 0)
		OutputDebug(L"LockChannel success.\n");
	else
		OutputDebug(L"LockChannel failed.\n");

	return nLock != 0;
}

// �`���[�i�ŗLDll�̃��[�h
HRESULT CBonTuner::CheckAndInitTunerDependDll(wstring tunerGUID, wstring tunerFriendlyName)
{
	if (m_aTunerParam.sDLLBaseName == L"") {
		// �`���[�i�ŗL�֐����g��Ȃ��ꍇ
		return S_OK;
	}

	if (m_aTunerParam.sDLLBaseName == L"AUTO") {
		// INI �t�@�C���� "AUTO" �w��̏ꍇ
		BOOL found = FALSE;
		for (unsigned int i = 0; i < sizeof aTunerSpecialData / sizeof TUNER_SPECIAL_DLL; i++) {
			if ((aTunerSpecialData[i].sTunerGUID != L"") && (tunerGUID.find(aTunerSpecialData[i].sTunerGUID)) != wstring::npos) {
				// ���̎��̃`���[�i�ˑ��R�[�h���`���[�i�p�����[�^�ɕϐ��ɃZ�b�g����
				m_aTunerParam.sDLLBaseName = aTunerSpecialData[i].sDLLBaseName;
				break;
			}
		}
		if (!found) {
			// ������Ȃ������̂Ń`���[�i�ŗL�֐��͎g��Ȃ�
			return S_OK;
		}
	}

	// ������ DLL �����[�h����B
	WCHAR szPath[_MAX_PATH + 1] = L"";
	::GetModuleFileNameW(st_hModule, szPath, _MAX_PATH + 1);
	// �t���p�X�𕪉�
	WCHAR szDrive[_MAX_DRIVE];
	WCHAR szDir[_MAX_DIR];
	WCHAR szFName[_MAX_FNAME];
	WCHAR szExt[_MAX_EXT];
	::_wsplitpath_s(szPath, szDrive, szDir, szFName, szExt);

	// �t�H���_���擾
	WCHAR szDLLName[_MAX_PATH + 1];
	::swprintf_s(szDLLName, _MAX_PATH + 1, L"%s%s%s.dll", szDrive, szDir, m_aTunerParam.sDLLBaseName.c_str());

	if ((m_hModuleTunerSpecials = ::LoadLibraryW(szDLLName)) == NULL) {
		// ���[�h�ł��Ȃ��ꍇ�A�ǂ�����? 
		//  �� �f�o�b�O���b�Z�[�W�����o���āA�ŗL�֐����g��Ȃ����̂Ƃ��Ĉ���
		OutputDebug(L"DLL Not found.\n");
		return S_OK;
	} else {
		OutputDebug(L"Load Library successfully.\n");
	}

	HRESULT(*func)(IBaseFilter*, const WCHAR*, const WCHAR*, const WCHAR*) =
		(HRESULT(*)(IBaseFilter*, const WCHAR*, const WCHAR*, const WCHAR*))::GetProcAddress(m_hModuleTunerSpecials, "CheckAndInitTuner");
	if (!func) {
		// �������R�[�h������
		// ���������s�v
		return S_OK;
	}

	return (*func)(m_pTunerDevice, tunerGUID.c_str(), tunerFriendlyName.c_str(), m_szIniFilePath);
}

// �`���[�i�ŗLDll�ł̃L���v�`���f�o�C�X�m�F
HRESULT CBonTuner::CheckCapture(wstring tunerGUID, wstring tunerFriendlyName, wstring captureGUID, wstring captureFriendlyName)
{
	if (m_hModuleTunerSpecials == NULL) {
		return S_OK;
	}

	HRESULT(*func)(const WCHAR*, const WCHAR*, const WCHAR*, const WCHAR*, const WCHAR*) =
		(HRESULT(*)(const WCHAR*, const WCHAR*, const WCHAR*, const WCHAR*, const WCHAR*))::GetProcAddress(m_hModuleTunerSpecials, "CheckCapture");
	if (!func) {
		return S_OK;
	}

	return (*func)(tunerGUID.c_str(), tunerFriendlyName.c_str(), captureGUID.c_str(), captureFriendlyName.c_str(), m_szIniFilePath);
}

// �`���[�i�ŗL�֐��̃��[�h
void CBonTuner::LoadTunerDependCode(void)
{
	if (!m_hModuleTunerSpecials)
		return;

	IBdaSpecials* (*func)(CComPtr<IBaseFilter>);
	func = (IBdaSpecials* (*)(CComPtr<IBaseFilter>))::GetProcAddress(m_hModuleTunerSpecials, "CreateBdaSpecials");
	if (!func) {
		OutputDebug(L"Cannot find CreateBdaSpecials.\n");
		::FreeLibrary(m_hModuleTunerSpecials);
		m_hModuleTunerSpecials = NULL;
		return;
	}
	else {
		OutputDebug(L"CreateBdaSpecials found.\n");
	}

	m_pIBdaSpecials = func(m_pTunerDevice);

	m_pIBdaSpecials2 = dynamic_cast<IBdaSpecials2a1 *>(m_pIBdaSpecials);
	if (!m_pIBdaSpecials2)
		OutputDebug(L"Not IBdaSpecials2 Interface DLL.\n");

	//  BdaSpecials��ini�t�@�C����ǂݍ��܂���
	HRESULT hr;
	if (m_pIBdaSpecials2) {
		hr = m_pIBdaSpecials2->ReadIniFile(m_szIniFilePath);
	}

	// �`���[�i�ŗL�������֐��������Ŏ��s���Ă���
	if (m_pIBdaSpecials)
		m_pIBdaSpecials->InitializeHook();

	return;
}

// �`���[�i�ŗL�֐���Dll�̉��
void CBonTuner::ReleaseTunerDependCode(void)
{
	HRESULT hr;

	// �`���[�i�ŗL�֐�����`����Ă���΁A�����Ŏ��s���Ă���
	if (m_pIBdaSpecials) {
		if ((hr = m_pIBdaSpecials->FinalizeHook()) == E_NOINTERFACE) {
			// �ŗLFinalize�֐����Ȃ������Ȃ̂ŁA��������
		}
		else if (SUCCEEDED(hr)) {
			OutputDebug(L"Tuner Special Finalize successfully.\n");
		}
		else {
			OutputDebug(L"Tuner Special Finalize failed.\n");
		}

		SAFE_RELEASE(m_pIBdaSpecials);
		m_pIBdaSpecials2 = NULL;
	}

	if (m_hModuleTunerSpecials) {
		if (::FreeLibrary(m_hModuleTunerSpecials) == 0) {
			OutputDebug(L"FreeLibrary failed.\n");
		}
		else {
			OutputDebug(L"FreeLibrary Success.\n");
			m_hModuleTunerSpecials = NULL;
		}
	}
}

HRESULT CBonTuner::InitializeGraphBuilder(void)
{
	HRESULT hr;
	if (FAILED(hr = ::CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&m_pIGraphBuilder))) {
		OutputDebug(L"Fail to create Graph.\n");
		return hr;
	}

	if (FAILED(hr = m_pIGraphBuilder->QueryInterface(&m_pIMediaControl))) {
		OutputDebug(L"Fail to get IMediaControl.\n");
		return hr;
	}

	return S_OK;
}

void CBonTuner::CleanupGraph(void)
{
	DisconnectAll(m_pTif);
	DisconnectAll(m_pDemux);
	DisconnectAll(m_pTsWriter);
	DisconnectAll(m_pCaptureDevice);
	DisconnectAll(m_pTunerDevice);
	DisconnectAll(m_pNetworkProvider);

	UnloadTif();
	UnloadDemux();
	UnloadTsWriter();

	// Tuner �� Capture �̏��� Release ���Ȃ���
	// ���������[�N���N�����f�o�C�X������
	UnloadTunerDevice();
	UnloadCaptureDevice();

	UnloadNetworkProvider();
	UnloadTuningSpace();

	SAFE_RELEASE(m_pIMediaControl);
	SAFE_RELEASE(m_pIGraphBuilder);

	return;
}

HRESULT CBonTuner::RunGraph(void)
{
	HRESULT hr;
	if (!m_pIMediaControl)
		return E_POINTER;

	if (FAILED(hr =  m_pIMediaControl->Run())) {
		m_pIMediaControl->Stop();
		OutputDebug(L"Failed to Run Graph.\n");
		return hr;
	}

	return S_OK;
}

void CBonTuner::StopGraph(void)
{
	HRESULT hr;
	if (m_pIMediaControl) {
		if (SUCCEEDED(hr = m_pIMediaControl->Pause())) {
			OutputDebug(L"IMediaControl::Pause Success.\n");
		} else {
			OutputDebug(L"IMediaControl::Pause failed.\n");
		}

		if (SUCCEEDED(hr = m_pIMediaControl->Stop())) {
			OutputDebug(L"IMediaControl::Stop Success.\n");
		} else {
			OutputDebug(L"IMediaControl::Stop failed.\n");
		}
	}
}

HRESULT CBonTuner::CreateTuningSpace(void)
{
	CLSID clsidTuningSpace = CLSID_NULL;
	IID iidITuningSpace = IID_NULL;
	CLSID clsidLocator = CLSID_NULL;
	IID iidNetworkType = IID_NULL;
	ModulationType modulationType = BDA_MOD_NOT_SET;
	_bstr_t bstrUniqueName;
	_bstr_t bstrFriendlyName;
	DVBSystemType dvbSystemType = DVB_Satellite;
	long networkID = -1;
	long highOscillator = -1;
	long lowOscillator = -1;
	long lnbSwitch = -1;
	TunerInputType tunerInputType = TunerInputCable;
	long minChannel = 0;
	long maxChannel = 0;
	long minPhysicalChannel = 0;
	long maxPhysicalChannel = 0;
	long minMinorChannel = 0;
	long maxMinorChannel = 0;
	long minMajorChannel = 0;
	long maxMajorChannel = 0;
	long minSourceID = 0;
	long maxSourceID = 0;

	switch (m_nDVBSystemType) {
	case eTunerTypeDVBT:
	case eTunerTypeDVBT2:
		bstrUniqueName = L"DVB-T";
		bstrFriendlyName = L"Local DVB-T Digital Antenna";
		clsidTuningSpace = __uuidof(DVBTuningSpace);
		iidITuningSpace = __uuidof(IDVBTuningSpace2);
		if (m_nDVBSystemType == eTunerTypeDVBT2) {
			clsidLocator = __uuidof(DVBTLocator2);
		}
		else {
			clsidLocator = __uuidof(DVBTLocator);
		}
		iidNetworkType = { STATIC_DVB_TERRESTRIAL_TV_NETWORK_TYPE };
		modulationType = BDA_MOD_NOT_SET;
		dvbSystemType = DVB_Terrestrial;
		networkID = 0;
		break;

	case eTunerTypeDVBC:
		bstrUniqueName = L"DVB-C";
		bstrFriendlyName = L"Local DVB-C Digital Cable";
		clsidTuningSpace = __uuidof(DVBTuningSpace);
		iidITuningSpace = __uuidof(IDVBTuningSpace2);
		clsidLocator = __uuidof(DVBCLocator);
		iidNetworkType = { STATIC_DVB_CABLE_TV_NETWORK_TYPE };
		modulationType = BDA_MOD_NOT_SET;
		dvbSystemType = DVB_Cable;
		networkID = 0;
		break;

	case eTunerTypeISDBT:
		bstrUniqueName = L"ISDB-T";
		bstrFriendlyName = L"Local ISDB-T Digital Antenna";
		clsidTuningSpace = __uuidof(DVBTuningSpace);
		iidITuningSpace = __uuidof(IDVBTuningSpace2);
		clsidLocator = __uuidof(DVBTLocator);
		iidNetworkType = { STATIC_ISDB_TERRESTRIAL_TV_NETWORK_TYPE };
		modulationType = BDA_MOD_NOT_SET;
		dvbSystemType = ISDB_Terrestrial;
		networkID = -1;
		break;

	case eTunerTypeISDBS:
		bstrUniqueName = L"ISDB-S";
		bstrFriendlyName = L"Default Digital ISDB-S Tuning Space";
		clsidTuningSpace = __uuidof(DVBSTuningSpace);
		iidITuningSpace = __uuidof(IDVBSTuningSpace);
		clsidLocator = __uuidof(DVBSLocator);
		iidNetworkType = { STATIC_ISDB_SATELLITE_TV_NETWORK_TYPE };
		modulationType = BDA_MOD_NOT_SET;
		dvbSystemType = ISDB_Satellite;
		networkID = -1;
		highOscillator = -1;
		lowOscillator = -1;
		lnbSwitch = -1;
		break;

	case eTunerTypeATSC_Antenna:
		bstrUniqueName = L"ATSC";
		bstrFriendlyName = L"Local ATSC Digital Antenna";
		clsidTuningSpace = __uuidof(ATSCTuningSpace);
		iidITuningSpace = __uuidof(IATSCTuningSpace);
		clsidLocator = __uuidof(ATSCLocator);
		iidNetworkType = { STATIC_ATSC_TERRESTRIAL_TV_NETWORK_TYPE };
		modulationType = BDA_MOD_128QAM;
		tunerInputType = TunerInputAntenna;
		minChannel = 1;
		maxChannel = 99;
		minPhysicalChannel = 2;
		maxPhysicalChannel = 158;
		minMinorChannel = 0;
		minMinorChannel = 999;
		break;

	case eTunerTypeATSC_Cable:
		bstrUniqueName = L"ATSCCable";
		bstrFriendlyName = L"Local ATSC Digital Cable";
		clsidTuningSpace = __uuidof(ATSCTuningSpace);
		iidITuningSpace = __uuidof(IATSCTuningSpace);
		clsidLocator = __uuidof(ATSCLocator);
		iidNetworkType = { STATIC_ATSC_TERRESTRIAL_TV_NETWORK_TYPE };
		modulationType = BDA_MOD_128QAM;
		tunerInputType = TunerInputCable;
		minChannel = 1;
		maxChannel = 99;
		minPhysicalChannel = 1;
		maxPhysicalChannel = 158;
		minMinorChannel = 0;
		minMinorChannel = 999;
		break;

	case eTunerTypeDigitalCable:
		bstrUniqueName = L"Digital Cable";
		bstrFriendlyName = L"Local Digital Cable";
		clsidTuningSpace = __uuidof(DigitalCableTuningSpace);
		iidITuningSpace = __uuidof(IDigitalCableTuningSpace);
		clsidLocator = __uuidof(DigitalCableLocator);
		iidNetworkType = { STATIC_DIGITAL_CABLE_NETWORK_TYPE };
		modulationType = BDA_MOD_NOT_SET;
		tunerInputType = TunerInputCable;
		minChannel = 2;
		maxChannel = 9999;
		minPhysicalChannel = 2;
		maxPhysicalChannel = 158;
		minMinorChannel = 0;
		minMinorChannel = 999;
		minMajorChannel = 1;
		minMajorChannel = 99;
		minSourceID = 0;
		maxSourceID = 0x7fffffff;
		break;

	case eTunerTypeDVBS:
	default:
		bstrUniqueName = L"DVB-S";
		bstrFriendlyName = L"Default Digital DVB-S Tuning Space";
		clsidTuningSpace = __uuidof(DVBSTuningSpace);
		iidITuningSpace = __uuidof(IDVBSTuningSpace);
		clsidLocator = __uuidof(DVBSLocator);
		iidNetworkType = { STATIC_DVB_SATELLITE_TV_NETWORK_TYPE };
		modulationType = BDA_MOD_NOT_SET;
		dvbSystemType = DVB_Satellite;
		networkID = -1;
		highOscillator = 10600000;
		lowOscillator = 9750000;
		lnbSwitch = 11700000;
		break;
	}

	HRESULT hr;

	// ITuningSpace���쐬
	//
	// ITuningSpace�p�����F
	//   ITuningSpace �� IDVBTuningSpace �� IDVBTuningSpace2 �� IDVBSTuningSpace
	//                �� IAnalogTVTuningSpace �� IATSCTuningSpace �� IDigitalCableTuningSpace
	if (FAILED(hr = ::CoCreateInstance(clsidTuningSpace, NULL, CLSCTX_INPROC_SERVER, iidITuningSpace, (void**)&m_pITuningSpace))) {
		OutputDebug(L"FAILED: CoCreateInstance(ITuningSpace)\n");
		return hr;
	}
	if (!m_pITuningSpace) {
		OutputDebug(L"Failed to get DVBSTuningSpace\n");
		return E_FAIL;
	}

	// ITuningSpace
	if (FAILED(hr = m_pITuningSpace->put__NetworkType(iidNetworkType))) {
		OutputDebug(L"put_NetworkType failed\n");
		return hr;
	}
	m_pITuningSpace->put_FrequencyMapping(L"");
	m_pITuningSpace->put_UniqueName(bstrUniqueName);
	m_pITuningSpace->put_FriendlyName(bstrFriendlyName);

	// IDVBTuningSpace���L
	{
		CComQIPtr<IDVBTuningSpace> pIDVBTuningSpace(m_pITuningSpace);
		if (pIDVBTuningSpace) {
			pIDVBTuningSpace->put_SystemType(dvbSystemType);
		}
	}

	// IDVBTuningSpace2���L
	{
		CComQIPtr<IDVBTuningSpace2> pIDVBTuningSpace2(m_pITuningSpace);
		if (pIDVBTuningSpace2) {
			pIDVBTuningSpace2->put_NetworkID(networkID);
		}
	}

	// IDVBSTuningSpace���L
	{
		CComQIPtr<IDVBSTuningSpace> pIDVBSTuningSpace(m_pITuningSpace);
		if (pIDVBSTuningSpace) {
			pIDVBSTuningSpace->put_HighOscillator(highOscillator);
			pIDVBSTuningSpace->put_LowOscillator(lowOscillator);
			pIDVBSTuningSpace->put_LNBSwitch(lnbSwitch);
			pIDVBSTuningSpace->put_SpectralInversion(BDA_SPECTRAL_INVERSION_NOT_SET);
		}
	}

	// IAnalogTVTuningSpace���L
	{
		CComQIPtr<IAnalogTVTuningSpace> pIAnalogTVTuningSpace(m_pITuningSpace);
		if (pIAnalogTVTuningSpace) {
			pIAnalogTVTuningSpace->put_InputType(tunerInputType);
			pIAnalogTVTuningSpace->put_MinChannel(minChannel);
			pIAnalogTVTuningSpace->put_MaxChannel(maxChannel);
			pIAnalogTVTuningSpace->put_CountryCode(0);
		}
	}

	// IATSCTuningSpace���L
	{
		CComQIPtr<IATSCTuningSpace> pIATSCTuningSpace(m_pITuningSpace);
		if (pIATSCTuningSpace) {
			pIATSCTuningSpace->put_MinPhysicalChannel(minPhysicalChannel);
			pIATSCTuningSpace->put_MaxPhysicalChannel(maxPhysicalChannel);
			pIATSCTuningSpace->put_MinMinorChannel(minMinorChannel);
			pIATSCTuningSpace->put_MaxMinorChannel(maxMinorChannel);
		}
	}

	// IDigitalCableTuningSpace���L
	{
		CComQIPtr<IDigitalCableTuningSpace> pIDigitalCableTuningSpace(m_pITuningSpace);
		if (pIDigitalCableTuningSpace) {
			pIDigitalCableTuningSpace->put_MinMajorChannel(minMajorChannel);
			pIDigitalCableTuningSpace->put_MaxMajorChannel(maxMajorChannel);
			pIDigitalCableTuningSpace->put_MinSourceID(minSourceID);
			pIDigitalCableTuningSpace->put_MaxSourceID(maxSourceID);
		}
	}

	// pILocator���쐬
	//
	// ILocator�p�����F
	//   ILocator �� IDVBTLocator �� IDVBTLocator2
	//            �� IDVBSLocator �� IDVBSLocator2
	//            �� IDVBCLocator
	//            �� IDigitalLocator �� IATSCLocator �� IATSCLocator2 �� IDigitalCableLocator
	CComPtr<ILocator> pILocator;
	if (FAILED(hr = pILocator.CoCreateInstance(clsidLocator)) || !pILocator) {
		OutputDebug(L"Fail to get ILocator.\n");
		return FALSE;
	}

	// ILocator
	pILocator->put_CarrierFrequency(-1);
	pILocator->put_SymbolRate(-1);
	pILocator->put_InnerFEC(BDA_FEC_METHOD_NOT_SET);
	pILocator->put_InnerFECRate(BDA_BCC_RATE_NOT_SET);
	pILocator->put_OuterFEC(BDA_FEC_METHOD_NOT_SET);
	pILocator->put_OuterFECRate(BDA_BCC_RATE_NOT_SET);
	pILocator->put_Modulation(modulationType);

	// IDVBSLocator���L
	{
		CComQIPtr<IDVBSLocator> pIDVBSLocator(pILocator);
		if (pIDVBSLocator) {
			pIDVBSLocator->put_WestPosition(FALSE);
			pIDVBSLocator->put_OrbitalPosition(-1);
			pIDVBSLocator->put_Elevation(-1);
			pIDVBSLocator->put_Azimuth(-1);
			pIDVBSLocator->put_SignalPolarisation(BDA_POLARISATION_NOT_SET);
		}
	}

	// IDVBSLocator2���L
	{
		CComQIPtr<IDVBSLocator2> pIDVBSLocator2(pILocator);
		if (pIDVBSLocator2) {
			pIDVBSLocator2->put_LocalOscillatorOverrideHigh(-1);
			pIDVBSLocator2->put_LocalOscillatorOverrideLow(-1);
			pIDVBSLocator2->put_LocalLNBSwitchOverride(-1);
			pIDVBSLocator2->put_LocalSpectralInversionOverride(BDA_SPECTRAL_INVERSION_NOT_SET);
			pIDVBSLocator2->put_DiseqLNBSource(BDA_LNB_SOURCE_NOT_SET);
			pIDVBSLocator2->put_SignalPilot(BDA_PILOT_NOT_SET);
			pIDVBSLocator2->put_SignalRollOff(BDA_ROLL_OFF_NOT_SET);
		}
	}

	// IDVBTLocator���L
	{
		CComQIPtr<IDVBTLocator> pIDVBTLocator(pILocator);
		if (pIDVBTLocator) {
			pIDVBTLocator->put_Bandwidth(-1);
			pIDVBTLocator->put_Guard(BDA_GUARD_NOT_SET);
			pIDVBTLocator->put_HAlpha(BDA_HALPHA_NOT_SET);
			pIDVBTLocator->put_LPInnerFEC(BDA_FEC_METHOD_NOT_SET);
			pIDVBTLocator->put_LPInnerFECRate(BDA_BCC_RATE_NOT_SET);
			pIDVBTLocator->put_Mode(BDA_XMIT_MODE_NOT_SET);
			pIDVBTLocator->put_OtherFrequencyInUse(VARIANT_FALSE);
		}
	}

	// IDVBTLocator2���L
	{
		CComQIPtr<IDVBTLocator2> pIDVBTLocator2(pILocator);
		if (pIDVBTLocator2) {
			pIDVBTLocator2->put_PhysicalLayerPipeId(-1);
		}
	}

	// IATSCLocator���L
	{
		CComQIPtr<IATSCLocator> pIATSCLocator(pILocator);
		if (pIATSCLocator) {
			pIATSCLocator->put_PhysicalChannel(-1);
			pIATSCLocator->put_TSID(-1);
		}
	}

	// IATSCLocator2���L
	{
		CComQIPtr<IATSCLocator2> pIATSCLocator2(pILocator);
		if (pIATSCLocator2) {
			pIATSCLocator2->put_ProgramNumber(-1);
		}
	}

	m_pITuningSpace->put_DefaultLocator(pILocator);

	return S_OK;
}

void CBonTuner::UnloadTuningSpace(void)
{
	SAFE_RELEASE(m_pITuningSpace);
}

// Tuning Request �𑗂��� Tuning Space ������������
//   ��������Ȃ��� output pin ���o�����Ȃ��`���[�i�t�B���^��
//   ����炵��
HRESULT CBonTuner::InitTuningSpace(void)
{
	if (!m_pITuningSpace) {
		OutputDebug(L"TuningSpace NOT SET.\n");
		return E_POINTER;
	}

	if (!m_pITuner) {
		OutputDebug(L"ITuner NOT SET.\n");
		return E_POINTER;
	}

	HRESULT hr;

	// ITuneRequest�쐬
	CComPtr<ITuneRequest> pITuneRequest;
	if (FAILED(hr = m_pITuningSpace->CreateTuneRequest(&pITuneRequest))) {
		OutputDebug(L"Fail to create ITuneRequest.\n");
		return hr;
	}

	// IDVBTuneRequest���L
	{
		CComQIPtr<IDVBTuneRequest> pIDVBTuneRequest(pITuneRequest);
		if (pIDVBTuneRequest) {
			pIDVBTuneRequest->put_ONID(-1);
			pIDVBTuneRequest->put_TSID(-1);
			pIDVBTuneRequest->put_SID(-1);
		}
	}

	// IChannelTuneRequest���L
	{
		CComQIPtr<IChannelTuneRequest> pIChannelTuneRequest(pITuneRequest);
		if (pIChannelTuneRequest) {
			pIChannelTuneRequest->put_Channel(-1);
		}
	}

	// IATSCChannelTuneRequest���L
	{
		CComQIPtr<IATSCChannelTuneRequest> pIATSCChannelTuneRequest(pITuneRequest);
		if (pIATSCChannelTuneRequest) {
			pIATSCChannelTuneRequest->put_MinorChannel(-1);
		}
	}

	// IDigitalCableTuneRequest���L
	{
		CComQIPtr<IDigitalCableTuneRequest> pIDigitalCableTuneRequest(pITuneRequest);
		if (pIDigitalCableTuneRequest) {
			pIDigitalCableTuneRequest->put_MajorChannel(-1);
			pIDigitalCableTuneRequest->put_SourceID(-1);
		}
	}

	m_pITuner->put_TuningSpace(m_pITuningSpace);

	m_pITuner->put_TuneRequest(pITuneRequest);

	return S_OK;
}

HRESULT CBonTuner::LoadNetworkProvider(void)
{
	static const WCHAR * const FILTER_GRAPH_NAME_NETWORK_PROVIDER[] = {
		L"Microsoft Network Provider",
		L"Microsoft DVB-S Network Provider",
		L"Microsoft DVB-T Network Provider",
		L"Microsoft DVB-C Network Provider",
		L"Microsoft ATSC Network Provider",
	};

	const WCHAR *strName = NULL;
	CLSID clsidNetworkProvider = CLSID_NULL;

	switch (m_nNetworkProvider) {
	case eNetworkProviderGeneric:
		clsidNetworkProvider = CLSID_NetworkProvider;
		strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[0];
		break;
	case eNetworkProviderDVBS:
		clsidNetworkProvider = CLSID_DVBSNetworkProvider;
		strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[1];
		break;
	case eNetworkProviderDVBT:
		clsidNetworkProvider = CLSID_DVBTNetworkProvider;
		strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[2];
		break;
	case eNetworkProviderDVBC:
		clsidNetworkProvider = CLSID_DVBCNetworkProvider;
		strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[3];
		break;
	case eNetworkProviderATSC:
		clsidNetworkProvider = CLSID_ATSCNetworkProvider;
		strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[4];
		break;
	case eNetworkProviderAuto:
	default:
		switch (m_nDVBSystemType) {
		case eTunerTypeDVBS:
		case eTunerTypeISDBS:
			clsidNetworkProvider = CLSID_DVBSNetworkProvider;
			strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[1];
			break;
		case eTunerTypeDVBT:
		case eTunerTypeDVBT2:
		case eTunerTypeISDBT:
			clsidNetworkProvider = CLSID_DVBTNetworkProvider;
			strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[2];
			break;
		case eTunerTypeDVBC:
			clsidNetworkProvider = CLSID_DVBCNetworkProvider;
			strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[3];
			break;
		case eTunerTypeATSC_Antenna:
		case eTunerTypeATSC_Cable:
		case eTunerTypeDigitalCable:
			clsidNetworkProvider = CLSID_ATSCNetworkProvider;
			strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[4];
			break;
		default:
			clsidNetworkProvider = CLSID_NetworkProvider;
			strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[0];
			break;
		}
		break;
	}

	HRESULT hr;

	if (FAILED(hr = ::CoCreateInstance(clsidNetworkProvider, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void **)(&m_pNetworkProvider)))) {
		OutputDebug(L"Fail to create network-provider.\n");
		return hr;
	}

	if (FAILED(hr = m_pIGraphBuilder->AddFilter(m_pNetworkProvider, strName))) {
		OutputDebug(L"Fail to add network-provider into graph.\n");
		SAFE_RELEASE(m_pNetworkProvider);
		return hr;
	}

	if (FAILED(hr = m_pNetworkProvider->QueryInterface(__uuidof(ITuner), (void **)&m_pITuner))) {
		OutputDebug(L"Fail to get ITuner.\n");
		return E_FAIL;
	}

	return S_OK;
}

void CBonTuner::UnloadNetworkProvider(void)
{
	HRESULT hr;
	if (m_pIGraphBuilder && m_pNetworkProvider)
		hr = m_pIGraphBuilder->RemoveFilter(m_pNetworkProvider);

	SAFE_RELEASE(m_pITuner);
	SAFE_RELEASE(m_pNetworkProvider);
}

// ini �t�@�C���Ŏw�肳�ꂽ�`���[�i�E�L���v�`���̑g����List���쐬
HRESULT CBonTuner::InitDSFilterEnum(void)
{
	HRESULT hr;

	// �V�X�e���ɑ��݂���`���[�i�E�L���v�`���̃��X�g
	vector<DSListData> TunerList;
	vector<DSListData> CaptureList;

	ULONG order;

	SAFE_DELETE(m_pDSFilterEnumTuner);
	SAFE_DELETE(m_pDSFilterEnumCapture);

	try {
		m_pDSFilterEnumTuner = new CDSFilterEnum(KSCATEGORY_BDA_NETWORK_TUNER, CDEF_DEVMON_PNP_DEVICE);
	}
	catch (...) {
		OutputDebug(L"[InitDSFilterEnum] Fail to construct CDSFilterEnum(KSCATEGORY_BDA_NETWORK_TUNER).\n");
		return E_FAIL;
	}

	order = 0;
	while (SUCCEEDED(hr = m_pDSFilterEnumTuner->next()) && hr == S_OK) {
		wstring sDisplayName;
		wstring sFriendlyName;

		// �`���[�i�� DisplayName, FriendlyName �𓾂�
		m_pDSFilterEnumTuner->getDisplayName(&sDisplayName);
		m_pDSFilterEnumTuner->getFriendlyName(&sFriendlyName);

		// �ꗗ�ɒǉ�
		TunerList.emplace_back(sDisplayName, sFriendlyName, order);

		order++;
	}

	try {
		m_pDSFilterEnumCapture = new CDSFilterEnum(KSCATEGORY_BDA_RECEIVER_COMPONENT, CDEF_DEVMON_PNP_DEVICE);
	}
	catch (...) {
		OutputDebug(L"[InitDSFilterEnum] Fail to construct CDSFilterEnum(KSCATEGORY_BDA_RECEIVER_COMPONENT).\n");
		return E_FAIL;
	}

	order = 0;
	while (SUCCEEDED(hr = m_pDSFilterEnumCapture->next()) && hr == S_OK) {
		wstring sDisplayName;
		wstring sFriendlyName;

		// �`���[�i�� DisplayName, FriendlyName �𓾂�
		m_pDSFilterEnumCapture->getDisplayName(&sDisplayName);
		m_pDSFilterEnumCapture->getFriendlyName(&sFriendlyName);

		// �ꗗ�ɒǉ�

		CaptureList.emplace_back(sDisplayName, sFriendlyName, order);

		order++;
	}

	unsigned int total = 0;
	m_UsableTunerCaptureList.clear();

	for (unsigned int i = 0; i < m_aTunerParam.Tuner.size(); i++) {
		for (vector<DSListData>::iterator it = TunerList.begin(); it != TunerList.end(); it++) {
			// DisplayName �� GUID ���܂܂�邩�������āANO�������玟�̃`���[�i��
			if (m_aTunerParam.Tuner[i]->TunerGUID.compare(L"") != 0 && it->GUID.find(m_aTunerParam.Tuner[i]->TunerGUID) == wstring::npos) {
				continue;
			}

			// FriendlyName ���܂܂�邩�������āANO�������玟�̃`���[�i��
			if (m_aTunerParam.Tuner[i]->TunerFriendlyName.compare(L"") != 0 && it->FriendlyName.find(m_aTunerParam.Tuner[i]->TunerFriendlyName) == wstring::npos) {
				continue;
			}

			// �Ώۂ̃`���[�i�f�o�C�X������
			OutputDebug(L"[InitDSFilterEnum] Found tuner device=FriendlyName:%s,  GUID:%s\n", it->FriendlyName.c_str(), it->GUID.c_str());
			if (!m_aTunerParam.bNotExistCaptureDevice) {
				// Capture�f�o�C�X���g�p����
				vector<DSListData> TempCaptureList;
				for (vector<DSListData>::iterator it2 = CaptureList.begin(); it2 != CaptureList.end(); it2++) {
					// DisplayName �� GUID ���܂܂�邩�������āANO�������玟�̃L���v�`����
					if (m_aTunerParam.Tuner[i]->CaptureGUID.compare(L"") != 0 && it2->GUID.find(m_aTunerParam.Tuner[i]->CaptureGUID) == wstring::npos) {
						continue;
					}

					// FriendlyName ���܂܂�邩�������āANO�������玟�̃L���v�`����
					if (m_aTunerParam.Tuner[i]->CaptureFriendlyName.compare(L"") != 0 && it2->FriendlyName.find(m_aTunerParam.Tuner[i]->CaptureFriendlyName) == wstring::npos) {
						continue;
					}

					// �Ώۂ̃L���v�`���f�o�C�X������
					OutputDebug(L"[InitDSFilterEnum]   Found capture device=FriendlyName:%s,  GUID:%s\n", it2->FriendlyName.c_str(), it2->GUID.c_str());
					TempCaptureList.emplace_back(*it2);
				}

				if (TempCaptureList.empty()) {
					// �L���v�`���f�o�C�X��������Ȃ������̂Ŏ��̃`���[�i��
					OutputDebug(L"[InitDSFilterEnum]   No combined capture devices.\n");
					continue;
				}

				// �`���[�i��List�ɒǉ�
				m_UsableTunerCaptureList.emplace_back(*it);

				unsigned int count = 0;
				if (m_aTunerParam.bCheckDeviceInstancePath) {
					// �`���[�i�f�o�C�X�ƃL���v�`���f�o�C�X�̃f�o�C�X�C���X�^���X�p�X����v���Ă��邩�m�F
					OutputDebug(L"[InitDSFilterEnum]   Checking device instance path.\n");
					wstring::size_type n, last;
					n = last = 0;
					while ((n = it->GUID.find(L'#', n)) != wstring::npos) {
						last = n;
						n++;
					}
					if (last != 0) {
						wstring path = it->GUID.substr(0, last);
						for (vector<DSListData>::iterator it2 = TempCaptureList.begin(); it2 != TempCaptureList.end(); it2++) {
							if (it2->GUID.find(path) != wstring::npos) {
								// �f�o�C�X�p�X����v������̂�List�ɒǉ�
								OutputDebug(L"[InitDSFilterEnum]     Adding matched tuner and capture device.\n");
								OutputDebug(L"[InitDSFilterEnum]       tuner=FriendlyName:%s,  GUID:%s\n", it->FriendlyName.c_str(), it->GUID.c_str());
								OutputDebug(L"[InitDSFilterEnum]       capture=FriendlyName:%s,  GUID:%s\n", it2->FriendlyName.c_str(), it2->GUID.c_str());
								m_UsableTunerCaptureList.back().CaptureList.emplace_back(*it2);
								count++;
							}
						}
					}
				}

				if (count == 0) {
					// �f�o�C�X�p�X����v������̂��Ȃ����� or �m�F���Ȃ�
					if (m_aTunerParam.bCheckDeviceInstancePath) {
						OutputDebug(L"[InitDSFilterEnum]     No matched devices.\n");
					}
					for (vector<DSListData>::iterator it2 = TempCaptureList.begin(); it2 != TempCaptureList.end(); it2++) {
						// ���ׂ�List�ɒǉ�
						OutputDebug(L"[InitDSFilterEnum]   Adding tuner and capture device.\n");
						OutputDebug(L"[InitDSFilterEnum]     tuner=FriendlyName:%s,  GUID:%s\n", it->FriendlyName.c_str(), it->GUID.c_str());
						OutputDebug(L"[InitDSFilterEnum]     capture=FriendlyName:%s,  GUID:%s\n", it2->FriendlyName.c_str(), it2->GUID.c_str());
						m_UsableTunerCaptureList.back().CaptureList.emplace_back(*it2);
						count++;
					}
				}

				OutputDebug(L"[InitDSFilterEnum]   %d of combination was added.\n", count);
				total += count;
			}
			else
			{
				// Capture�f�o�C�X���g�p���Ȃ�
				OutputDebug(L"[InitDSFilterEnum]   Adding tuner device only.\n");
				OutputDebug(L"[InitDSFilterEnum]     tuner=FriendlyName:%s,  GUID:%s\n", it->FriendlyName.c_str(), it->GUID.c_str());
				m_UsableTunerCaptureList.emplace_back(*it);
			}
		}
	}
	if (m_UsableTunerCaptureList.empty()) {
		OutputDebug(L"[InitDSFilterEnum] No devices found.\n");
		return E_FAIL;
	}

	OutputDebug(L"[InitDSFilterEnum] Total %d of combination was added.\n", total);
	return S_OK;
}

// �`���[�i�E�L���v�`���̑g���킹���X�g���瓮�삷����̂�T��
HRESULT CBonTuner::LoadAndConnectDevice(void)
{
	HRESULT hr;
	if (!m_pITuningSpace || !m_pNetworkProvider) {
		OutputDebug(L"[P->T] TuningSpace or NetworkProvider NOT SET.\n");
		return E_POINTER;
	}

	if (!m_pDSFilterEnumTuner || !m_pDSFilterEnumCapture) {
		OutputDebug(L"[P->T] DSFilterEnum NOT SET.\n");
		return E_POINTER;
	}

	for (list<TunerCaptureList>::iterator it = m_UsableTunerCaptureList.begin(); it != m_UsableTunerCaptureList.end(); it++) {
		OutputDebug(L"[P->T] Trying tuner device=FriendlyName:%s,  GUID:%s\n", it->Tuner.FriendlyName.c_str(), it->Tuner.GUID.c_str());
		// �`���[�i�f�o�C�X���[�v
		// �r�������p�ɃZ�}�t�H�p��������쐬 ('\' -> '/')
		wstring::size_type n = 0;
		wstring semName = it->Tuner.GUID;
		while ((n = semName.find(L'\\', n)) != wstring::npos) {
			semName.replace(n, 1, 1, L'/');
		}
		semName = L"Global\\" + semName;

		// �r������
		m_hSemaphore = ::CreateSemaphoreW(NULL, 1, 1, semName.c_str());
		DWORD result = ::WaitForSingleObject(m_hSemaphore, 0);
		if (result != WAIT_OBJECT_0) {
			OutputDebug(L"[P->T] Another is using.\n");
			// �g�p���Ȃ̂Ŏ��̃`���[�i��T��
		} 
		else {
			// �r���m�FOK
			// �`���[�i�f�o�C�X�̃t�B���^���擾
			if (FAILED(hr = m_pDSFilterEnumTuner->getFilter(&m_pTunerDevice, it->Tuner.Order))) {
				OutputDebug(L"[P->T] Error in Get Filter\n");
			}
			else {
				// �t�B���^�擾����
				// �`���[�i�f�o�C�X�̃t�B���^��ǉ�
				if (FAILED(hr = m_pIGraphBuilder->AddFilter(m_pTunerDevice, it->Tuner.FriendlyName.c_str()))) {
					OutputDebug(L"[P->T] Error in AddFilter\n");
				}
				else {
					// �t�B���^�擾����
					// �`���[�i�f�o�C�X��connect ���Ă݂�
					if (FAILED(hr = Connect(L"Provider->Tuner", m_pNetworkProvider, m_pTunerDevice))) {
						// NetworkProvider���قȂ铙�̗��R��connect�Ɏ��s
						// ���̃`���[�i��
						OutputDebug(L"[P->T] Connect Failed.\n");
					}
					else {
						// connect ����
						OutputDebug(L"[P->T] Connect OK.\n");

						// �`���[�i�ŗLDll���K�v�Ȃ�Ǎ��݁A�ŗL�̏���������������ΌĂяo��
						if (FAILED(hr = CheckAndInitTunerDependDll(it->Tuner.GUID, it->Tuner.FriendlyName))) {
							// ���炩�̗��R�Ŏg�p�ł��Ȃ��݂����Ȃ̂Ŏ��̃`���[�i��
							OutputDebug(L"[P->T] Discarded by BDASpecials.\n");
						}
						else {
							// �ŗLDll����OK
							if (!m_aTunerParam.bNotExistCaptureDevice) {
								// �L���v�`���f�o�C�X���g�p����
								for (vector<DSListData>::iterator it2 = it->CaptureList.begin(); it2 != it->CaptureList.end(); it2++) {
									OutputDebug(L"[T->C] Trying capture device=FriendlyName:%s,  GUID:%s\n", it2->FriendlyName.c_str(), it2->GUID.c_str());
									// �L���v�`���f�o�C�X���[�v
									// �`���[�i�ŗLDll�ł̊m�F����������ΌĂяo��
									if (FAILED(hr = CheckCapture(it->Tuner.GUID, it->Tuner.FriendlyName, it2->GUID, it2->FriendlyName))) {
										// �ŗLDll���_���ƌ����Ă���̂Ŏ��̃L���v�`���f�o�C�X��
										OutputDebug(L"[T->C] Discarded by BDASpecials.\n");
									}
									else {
										// �ŗLDll�̊m�FOK
										// �L���v�`���f�o�C�X�̃t�B���^���擾
										if (FAILED(hr = m_pDSFilterEnumCapture->getFilter(&m_pCaptureDevice, it2->Order))) {
											OutputDebug(L"[T->C] Error in Get Filter\n");
										}
										else {
											// �t�B���^�擾����
											// �L���v�`���f�o�C�X�̃t�B���^��ǉ�
											if (FAILED(hr = m_pIGraphBuilder->AddFilter(m_pCaptureDevice, it2->FriendlyName.c_str()))) {
												OutputDebug(L"[T->C] Error in AddFilter\n");
											}
											else {
												// �t�B���^�ǉ�����
												// �L���v�`���f�o�C�X��connect ���Ă݂�
												if (FAILED(hr = Connect(L"Tuner->Capture", m_pTunerDevice, m_pCaptureDevice))) {
													// connect �ł��Ȃ���΃`���[�i�Ƃ̑g�������������Ȃ��Ǝv����
													// ���̃L���v�`���f�o�C�X��
													OutputDebug(L"[T->C] Connect Failed.\n");
												}
												else {
													// connect ����
													OutputDebug(L"[T->C] Connect OK.\n");

													// TsWriter�ȍ~�Ɛڑ��`Run
													if (FAILED(LoadAndConnectMiscFilters())) {
														// ���s�����玟�̃L���v�`���f�o�C�X��
													}
													else {
														// ���ׂĐ���
														LoadTunerDependCode();
														if (m_bTryAnotherTuner)
															m_UsableTunerCaptureList.splice(m_UsableTunerCaptureList.end(), m_UsableTunerCaptureList, it);
														return S_OK;
													} // ���ׂĐ���
													DisconnectAll(m_pCaptureDevice);
												} // connect ����
												m_pIGraphBuilder->RemoveFilter(m_pCaptureDevice);
											} // �t�B���^�ǉ�����
											SAFE_RELEASE(m_pCaptureDevice);
										} // �t�B���^�擾����
									} // �ŗLDll�̊m�FOK
								} // �L���v�`���f�o�C�X���[�v
								// ���삷��g������������Ȃ������̂Ŏ��̃`���[�i��
							} // �L���v�`���f�o�C�X���g�p����
							else {
								// �L���v�`���f�o�C�X���g�p���Ȃ�
								// TsWriter�ȍ~�Ɛڑ��`Run
								if (FAILED(hr = LoadAndConnectMiscFilters())) {
									// ���s�����玟�̃`���[�i��
								}
								else {
									// ����
									LoadTunerDependCode();
									if (m_bTryAnotherTuner)
										m_UsableTunerCaptureList.splice(m_UsableTunerCaptureList.end(), m_UsableTunerCaptureList, it);
									return S_OK;
								} // ����
							} // �L���v�`���f�o�C�X���g�p���Ȃ�
						} // �ŗLDll����OK
						ReleaseTunerDependCode();
						DisconnectAll(m_pTunerDevice);
					} // connect ����
					m_pIGraphBuilder->RemoveFilter(m_pTunerDevice);
				} // �t�B���^�擾����
				SAFE_RELEASE(m_pTunerDevice);
			} // �t�B���^�擾����
			::ReleaseSemaphore(m_hSemaphore, 1, NULL);
		} // �r������OK
		::CloseHandle(m_hSemaphore);
		m_hSemaphore = NULL;
	} // �`���[�i�f�o�C�X���[�v

	// ���삷��g�ݍ��킹��������Ȃ�����
	OutputDebug(L"[P->T] Tuner not found.\n");
	return E_FAIL;
}

void CBonTuner::UnloadTunerDevice(void)
{
	HRESULT hr;

	ReleaseTunerDependCode();

	if (m_pIGraphBuilder && m_pTunerDevice)
		hr = m_pIGraphBuilder->RemoveFilter(m_pTunerDevice);

	SAFE_RELEASE(m_pTunerDevice);
}

void CBonTuner::UnloadCaptureDevice(void)
{
	HRESULT hr;

	if (m_pIGraphBuilder && m_pCaptureDevice)
		hr = m_pIGraphBuilder->RemoveFilter(m_pCaptureDevice);

	SAFE_RELEASE(m_pCaptureDevice);
}

HRESULT CBonTuner::LoadAndConnectMiscFilters(void)
{
	HRESULT hr;

	// TsWriter�Ɛڑ�
	if (FAILED(hr = LoadAndConnectTsWriter())) {
		return hr;
	}

	// TsDemuxer�Ɛڑ�
	if (FAILED(hr = LoadAndConnectDemux())) {
		DisconnectAll(m_pTsWriter);
		UnloadTsWriter();
		return hr;
	}

	// TIF�Ɛڑ�
	if (FAILED(hr = LoadAndConnectTif())) {
		DisconnectAll(m_pDemux);
		DisconnectAll(m_pTsWriter);
		UnloadDemux();
		UnloadTsWriter();
		return hr;
	}

	// Run���Ă݂�
	if (FAILED(hr = RunGraph())) {
		OutputDebug(L"RunGraph Failed.\n");
		DisconnectAll(m_pTif);
		DisconnectAll(m_pDemux);
		DisconnectAll(m_pTsWriter);
		UnloadTif();
		UnloadDemux();
		UnloadTsWriter();
		return hr;
	}

	// ����
	OutputDebug(L"RunGraph OK.\n");
	return S_OK;
}

HRESULT CBonTuner::LoadAndConnectTsWriter(void)
{
	HRESULT hr = E_FAIL;

	if (!m_pTunerDevice || (!m_pCaptureDevice && !m_aTunerParam.bNotExistCaptureDevice)) {
		OutputDebug(L"[C->W] TunerDevice or CaptureDevice NOT SET.\n");
		return E_POINTER;
	}

	// �C���X�^���X�쐬
	m_pCTsWriter = static_cast<CTsWriter *>(CTsWriter::CreateInstance(NULL, &hr));
	if (!m_pCTsWriter) {
		OutputDebug(L"[C->W] Fail to load TsWriter filter.\n");
		return E_NOINTERFACE;
	}

	m_pCTsWriter->AddRef();

	// �t�B���^���擾
	if (FAILED(hr = m_pCTsWriter->QueryInterface(IID_IBaseFilter, (void**)(&m_pTsWriter)))) {
		OutputDebug(L"[C->W] Fail to get TsWriter interface.\n");
		SAFE_RELEASE(m_pCTsWriter);
		return hr;
	}

	// �t�B���^��ǉ�
	if (FAILED(hr = m_pIGraphBuilder->AddFilter(m_pTsWriter, FILTER_GRAPH_NAME_TSWRITER))) {
		OutputDebug(L"[C->W] Fail to add TsWriter filter into graph.\n");
		SAFE_RELEASE(m_pTsWriter);
		SAFE_RELEASE(m_pCTsWriter);
		return hr;
	}

	// connect ���Ă݂�
	if (m_aTunerParam.bNotExistCaptureDevice) {
		// Capture�f�o�C�X�����݂��Ȃ��ꍇ��Tuner�Ɛڑ�
		if (FAILED(hr = Connect(L"Tuner->TsWriter", m_pTunerDevice, m_pTsWriter))) {
			OutputDebug(L"[T->W] Failed to connect.\n");
			SAFE_RELEASE(m_pTsWriter);
			SAFE_RELEASE(m_pCTsWriter);
			return hr;
		}
	}
	else {
		if (FAILED(hr = Connect(L"Capture->TsWriter", m_pCaptureDevice, m_pTsWriter))) {
			OutputDebug(L"[C->W] Failed to connect.\n");
			SAFE_RELEASE(m_pTsWriter);
			SAFE_RELEASE(m_pCTsWriter);
			return hr;
		}
	}

	// connect �����Ȃ̂ł��̂܂܏I��
	OutputDebug(L"[C->W] Connect OK.\n");
	return S_OK;
}

void CBonTuner::UnloadTsWriter(void)
{
	HRESULT hr;
	if (m_pIGraphBuilder && m_pTsWriter)
		hr = m_pIGraphBuilder->RemoveFilter(m_pTsWriter);

	SAFE_RELEASE(m_pTsWriter);
	SAFE_RELEASE(m_pCTsWriter);
}

HRESULT CBonTuner::LoadAndConnectDemux(void)
{
	HRESULT hr;

	if (!m_pTsWriter) {
			OutputDebug(L"[W->M] TsWriter NOT SET.\n");
			return E_POINTER;
	}

	// �C���X�^���X�쐬
	if (FAILED(hr = ::CoCreateInstance(CLSID_MPEG2Demultiplexer, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void **)(&m_pDemux)))) {
		OutputDebug(L"[W->M] Fail to load MPEG2-Demultiplexer.\n");
		return hr;
	}

	// �t�B���^��ǉ�
	if (FAILED(hr = m_pIGraphBuilder->AddFilter(m_pDemux, FILTER_GRAPH_NAME_DEMUX))) {
		OutputDebug(L"[W->M] Fail to add MPEG2-Demultiplexer into graph.\n");
		SAFE_RELEASE(m_pDemux);
		return hr;
	}

	// connect ���Ă݂�
	if (FAILED(hr = Connect(L"Grabber->Demux", m_pTsWriter, m_pDemux))) {
		OutputDebug(L"[W->M] Fail to connect Grabber->Demux.\n");
		SAFE_RELEASE(m_pDemux);
		return hr;
	}

	// connect �����Ȃ̂ł��̂܂܏I��
	OutputDebug(L"[W->M] Connect OK.\n");
	return S_OK;
}

void CBonTuner::UnloadDemux(void)
{
	HRESULT hr;
	if (m_pIGraphBuilder && m_pDemux)
		hr = m_pIGraphBuilder->RemoveFilter(m_pDemux);

	SAFE_RELEASE(m_pDemux);
}

HRESULT CBonTuner::LoadAndConnectTif(void)
{
	HRESULT hr;

	if (!m_pDemux) {
			OutputDebug(L"[M->I] Demux NOT SET.\n");
			return E_POINTER;
	}

	wstring friendlyName;

	try {
		CDSFilterEnum dsfEnum(KSCATEGORY_BDA_TRANSPORT_INFORMATION, CDEF_DEVMON_FILTER);
		while (SUCCEEDED(hr = dsfEnum.next()) && hr == S_OK) {
			// MPEG-2 Sections and Tables Filter �ɐڑ����Ă��܂��� RunGraph �Ɏ��s���Ă��܂��̂�
			// BDA MPEG2 Transport Information Filter �ȊO�̓X�L�b�v
			dsfEnum.getFriendlyName(&friendlyName);
			if (friendlyName.find(FILTER_GRAPH_NAME_TIF) == wstring::npos)
				continue;

			// �t�B���^���擾
			if (FAILED(hr = dsfEnum.getFilter(&m_pTif))) {
				OutputDebug(L"[M->I] Error in Get Filter\n");
				return hr;
			}

			// �t�B���^��ǉ�
			if (FAILED(hr = m_pIGraphBuilder->AddFilter(m_pTif, FILTER_GRAPH_NAME_TIF))) {
				SAFE_RELEASE(m_pTif);
				OutputDebug(L"[M->I] Error in AddFilter.\n");
				return hr;
			}

			// connect ���Ă݂�
			if (FAILED(hr = Connect(L"Demux -> Tif", m_pDemux, m_pTif))) {
				m_pIGraphBuilder->RemoveFilter(m_pTif);
				SAFE_RELEASE(m_pTif);
				return hr;
			}

			// connect �����Ȃ̂ł��̂܂܏I��
			OutputDebug(L"[M->I] Connect OK.\n");
			return S_OK;
		}
		OutputDebug(L"[M->I] MPEG2 Transport Information Filter not found.\n");
		return E_FAIL;
	} catch (...) {
		OutputDebug(L"[M->I] Fail to construct CDSFilterEnum.\n");
		SAFE_RELEASE(m_pTif);
		return E_FAIL;
	}
}

void CBonTuner::UnloadTif(void)
{
	HRESULT hr;
	if (m_pIGraphBuilder && m_pTif)
		hr = m_pIGraphBuilder->RemoveFilter(m_pTif);

	SAFE_RELEASE(m_pTif);
}

HRESULT CBonTuner::LoadTunerSignalStatistics(void)
{
	HRESULT hr;

	if (m_pTunerDevice == NULL) {
		OutputDebug(L"[LoadTunerSignalStatistics] TunerDevice NOT SET.\n");
		return E_POINTER;
	}

	CComQIPtr<IBDA_Topology> pIBDA_Topology(m_pTunerDevice);
	if (!pIBDA_Topology) {
		OutputDebug(L"[LoadTunerSignalStatistics] Fail to get IBDA_Topology.\n");
		return E_FAIL;
	}

	ULONG NodeTypes;
	ULONG NodeType[32];
	if (FAILED(hr = pIBDA_Topology->GetNodeTypes(&NodeTypes, 32, NodeType))) {
		OutputDebug(L"[LoadTunerSignalStatistics] Fail to get NodeTypes.\n");
		return E_FAIL;
	}

	BOOL bFound = FALSE;
	for (ULONG i = 0; i < NodeTypes; i++) {
		IUnknown *pControlNode = NULL;
		if (SUCCEEDED(hr = pIBDA_Topology->GetControlNode(0UL, 1UL, NodeType[i], &pControlNode))) {
			if (SUCCEEDED(hr = pControlNode->QueryInterface(__uuidof(IBDA_SignalStatistics), (void **)(&m_pIBDA_SignalStatistics)))) {
				OutputDebug(L"[LoadTunerSignalStatistics] SUCCESS.\n");
				bFound = TRUE;
			}
			SAFE_RELEASE(pControlNode);
		}
		if (bFound)
			break;
	}

	if (!m_pIBDA_SignalStatistics) {
		OutputDebug(L"[LoadTunerSignalStatistics] Fail to get IBDA_SignalStatistics.\n");
		return E_FAIL;
	}

	return S_OK;
}

void CBonTuner::UnloadTunerSignalStatistics(void)
{
	SAFE_RELEASE(m_pIBDA_SignalStatistics);
}


// Connect pins (Common subroutine)
//  �S�Ẵs����ڑ����Đ���������I��
//
HRESULT CBonTuner::Connect(const WCHAR* pszName, IBaseFilter* pFilterUp, IBaseFilter* pFilterDown)
{
	HRESULT hr;

	IEnumPins *pIEnumPinsUp = NULL;
	IEnumPins *pIEnumPinsDown = NULL;
	do {
		// �㗬�t�B���^�̃s����
		if (FAILED(hr = pFilterUp->EnumPins(&pIEnumPinsUp))) {
			OutputDebug(L"Fatal Error; Cannot enumerate upstream filter's pins.\n");
			break;
		}

		// �����t�B���^�̃s����
		if (FAILED(hr = pFilterDown->EnumPins(&pIEnumPinsDown))) {
			OutputDebug(L"Fatal Error; Cannot enumerate downstream filter's pins.\n");
			break;
		}

		BOOL bExitLoop = FALSE;
		// �㗬�t�B���^�̃s���̐��������[�v
		IPin *pIPinUp = NULL;
		while (SUCCEEDED(hr = pIEnumPinsUp->Next(1, &pIPinUp, 0)) && hr == S_OK) {
			PIN_INFO PinInfoUp = { NULL, };
			IPin *pIPinPeerOfUp = NULL;
			do {
				if (FAILED(hr = pIPinUp->QueryPinInfo(&PinInfoUp))) {
					OutputDebug(L"Fatal Error; Cannot get upstream filter's pinInfo.\n");
					bExitLoop = TRUE;
					break;
				}

				// ���ڃs���� INPUT�s���Ȃ玟�̏㗬�s����
				if (PinInfoUp.dir == PINDIR_INPUT) {
					OutputDebug(L"This is an INPUT pin.\n");
					break;
				}

				// �㗬�t�B���^�̒��ڃs�����ڑ���or�G���[�������玟�̏㗬�s����
				if (pIPinUp->ConnectedTo(&pIPinPeerOfUp) != VFW_E_NOT_CONNECTED){
					OutputDebug(L"Target pin is already connected.\n");
					break;
				}

				// �����t�B���^�̃s���̐��������[�v
				IPin *pIPinDown = NULL;
				pIEnumPinsDown->Reset();
				while (SUCCEEDED(hr = pIEnumPinsDown->Next(1, &pIPinDown, 0)) && hr == S_OK) {
					PIN_INFO PinInfoDown = { NULL, };
					IPin *pIPinPeerOfDown = NULL;
					do {
						if (FAILED(hr = pIPinDown->QueryPinInfo(&PinInfoDown))) {
							OutputDebug(L"Fatal Error; cannot get downstream filter's pinInfo.\n");
							bExitLoop = TRUE;
							break;
						}

						// ���ڃs���� OUTPUT �s���Ȃ玟�̉����s����
						if (PinInfoDown.dir == PINDIR_OUTPUT) {
							OutputDebug(L"This is an OUTPUT pin.\n");
							break;
						}

						// �����t�B���^�̒��ڃs�����ڑ���or�G���[�������玟�̉����s����
						if (pIPinDown->ConnectedTo(&pIPinPeerOfDown) != VFW_E_NOT_CONNECTED) {
							OutputDebug(L"Target pin is already connected.\n");
							break;
						}

						// �ڑ������݂�
						if (SUCCEEDED(hr = m_pIGraphBuilder->ConnectDirect(pIPinUp, pIPinDown, NULL))) {
							OutputDebug(L"%s CBonTuner::Connect successfully.\n", pszName);
							bExitLoop = TRUE;
							break;
						} else {
							// �Ⴄ�`���[�i���j�b�g�̃t�B���^��ڑ����悤�Ƃ��Ă�ꍇ�Ȃ�
							// �R�l�N�g�ł��Ȃ��ꍇ�A���̉����s����
							OutputDebug(L"Can't connect to unconnected pin, Maybe differenct unit?\n");
						}
					} while(0);
					SAFE_RELEASE(pIPinPeerOfDown);
					SAFE_RELEASE(PinInfoDown.pFilter);
					SAFE_RELEASE(pIPinDown);
					if (bExitLoop)
						break;
					OutputDebug(L"Trying next downstream pin.\n");
				} // while; ���̉����s����
				break;
			} while (0);
			SAFE_RELEASE(pIPinPeerOfUp);
			SAFE_RELEASE(PinInfoUp.pFilter);
			SAFE_RELEASE(pIPinUp);
			if (bExitLoop)
				break;
			OutputDebug(L"Trying next upstream pin.\n");
		} // while ; ���̏㗬�s����
		if (!bExitLoop) {
			OutputDebug(L"Can't connect.\n");
			hr = E_FAIL;
		}
	} while(0);
	SAFE_RELEASE(pIEnumPinsDown);
	SAFE_RELEASE(pIEnumPinsUp);

	return hr;
}

void CBonTuner::DisconnectAll(IBaseFilter* pFilter)
{
	if (!m_pIGraphBuilder || !pFilter)
		return;
	
	HRESULT hr;

	IEnumPins *pIEnumPins = NULL;
	// �t�B���^�̃s����
	if (SUCCEEDED(hr = pFilter->EnumPins(&pIEnumPins))) {
		// �s���̐��������[�v
		IPin *pIPin = NULL;
		while (SUCCEEDED(hr = pIEnumPins->Next(1, &pIPin, 0)) && hr == S_OK) {
			// �s�����ڑ��ς�������ؒf
			IPin *pIPinPeerOf = NULL;
			if (SUCCEEDED(hr = pIPin->ConnectedTo(&pIPinPeerOf))) {
				hr = m_pIGraphBuilder->Disconnect(pIPinPeerOf);
				hr = m_pIGraphBuilder->Disconnect(pIPin);
				SAFE_RELEASE(pIPinPeerOf);
			}
			SAFE_RELEASE(pIPin);
		}
		SAFE_RELEASE(pIEnumPins);
	}
}

CBonTuner::TS_BUFF::TS_BUFF(void)
	: BufferingItem(NULL),
	BuffSize(0),
	Count(0),
	MaxCount(0),
	MinCount(0)
{
	::InitializeCriticalSection(&cs);
}

CBonTuner::TS_BUFF::~TS_BUFF(void)
{
	Purge();
	::DeleteCriticalSection(&cs);
}

void CBonTuner::TS_BUFF::SetSize(DWORD dwBuffSize, DWORD dwMaxCount, DWORD dwMinCount)
{
	Purge();

	BuffSize = dwBuffSize;
	MaxCount = dwMaxCount;
	MinCount = dwMinCount ? dwMinCount : dwMaxCount;
}

void CBonTuner::TS_BUFF::Purge()
{
	::EnterCriticalSection(&cs);

	for (auto it = TsBuff.begin(); it != TsBuff.end(); it++) {
		SAFE_DELETE(*it);
	}
	TsBuff.clear();

	for (auto it = FreeBuff.begin(); it != FreeBuff.end(); it++) {
		SAFE_DELETE(*it);
	}
	FreeBuff.clear();

	SAFE_DELETE(BufferingItem);
	Count = 0;

	::LeaveCriticalSection(&cs);
}

BOOL CBonTuner::TS_BUFF::AddData(BYTE *pbyData, DWORD dwSize)
{
	BOOL ret = FALSE;

	while (dwSize)
	{
		TS_DATA *pItem = NULL;

		::EnterCriticalSection(&cs);

		if (BufferingItem != NULL) {
			// �������̃o�b�t�@
			pItem = BufferingItem;
			BufferingItem = NULL;
		}
		else if (!FreeBuff.empty()) {
			// �g�p����Ă��Ȃ��o�b�t�@
			pItem = FreeBuff.front();
			FreeBuff.pop_front();
			pItem->dwSize = 0;
		}
		else if (Count >= MaxCount) {
			// �I�[�o�[�t���[
			if (!TsBuff.empty()) {
				// �擾����Ă��Ȃ�TsBuff���g�p����
				pItem = TsBuff.back();
				TsBuff.pop_back();
				pItem->dwSize = 0;
			}
			else {
				// �o�b�t�@���Ȃ� ���f
				::LeaveCriticalSection(&cs);
				break;
			}
		}
		else {
			// TS_DATA���쐬
			pItem = new TS_DATA(NULL, BuffSize ? BuffSize : dwSize);
			Count++;
		}

		if (BuffSize != 0)
		{
			// ini�t�@�C����BuffSize���w�肳��Ă���ꍇ�͂��̃T�C�Y�ɍ��킹��

			DWORD dwCopySize;

			dwCopySize = pItem->Put(pbyData, dwSize);
			pbyData += dwCopySize;
			dwSize -= dwCopySize;

			if (pItem->dwSize < BuffSize) {
				// �܂��o�b�t�@���ɋ󂫂�����
				BufferingItem = pItem;
				::LeaveCriticalSection(&cs);
				continue;
			}
		}
		else
		{
			// BuffSize���w�肳��Ă��Ȃ��ꍇ�͏㗬����󂯎�����T�C�Y�ł��̂܂ܒǉ�

			if (pItem->dwBuffSize < dwSize) {
				// �e�ʂ̑���Ȃ�TS_DATA���g��
				pItem->Expand(NULL, dwSize);
			}

			pItem->Put(pbyData, dwSize);
			dwSize = 0;
		}

		// FIFO�֒ǉ�
		TsBuff.push_back(pItem);

		ret = TRUE;
		::LeaveCriticalSection(&cs);
	}

	return ret;
}

CBonTuner::TS_DATA * CBonTuner::TS_BUFF::Get(void)
{
	TS_DATA *ts = NULL;
	::EnterCriticalSection(&cs);
	if (!TsBuff.empty()) {
		ts = TsBuff.front();
		TsBuff.pop_front();
	}
	::LeaveCriticalSection(&cs);
	return ts;
}

void CBonTuner::TS_BUFF::Free(CBonTuner::TS_DATA *ts)
{
	if (ts != NULL) {
		::EnterCriticalSection(&cs);
		if (FreeBuff.size() >= MinCount) {
			delete ts;
			Count--;
		}
		else {
			FreeBuff.push_front(ts);
		}
		::LeaveCriticalSection(&cs);
	}
}
