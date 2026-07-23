#pragma once

#include "KeyHandlerEditSession.h"
#include "MetasequoiaIMEBaseStructure.h"
#include <atomic>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>

class CLangBarItemButton;
class CCandidateListUIPresenter;
class CCompositionProcessorEngine;

const DWORD WM_CheckGlobalCompartment = WM_USER;
const DWORD WM_ConnectNamedpipe = WM_USER + 1;
const DWORD WM_DisconnectNamedpipe = WM_USER + 2;
const DWORD WM_ConnectToTsfNamedpipe = WM_USER + 3;
const DWORD WM_IMEActivation = WM_USER + 4;
const DWORD WM_ThreadFocus = WM_USER + 5;
const DWORD WM_UpdateIMEStatus = WM_USER + 6;
const DWORD WM_UpdateDoubleSingleByte = WM_USER + 7;
const DWORD WM_UpdatePuncMode = WM_USER + 8;
const DWORD WM_CommitCandidate = WM_USER + 9;
const DWORD WM_CleanupCandidatePresenter = WM_USER + 10;
const DWORD WM_AsyncFinalizeCandidate = WM_USER + 11;
const DWORD WM_AsyncPunctuationCommit = WM_USER + 12;
const DWORD WM_AsyncNumberCandidateCommit = WM_USER + 13;
const DWORD WM_AsyncServerCandidateKey = WM_USER + 14;
const DWORD WM_IpcWorkerDisconnected = WM_USER + 15;
const DWORD WM_IpcReconnect = WM_USER + 16;
const DWORD WM_IpcSessionDirty = WM_USER + 17;
const DWORD WM_DrainDeferredKeyDown = WM_USER + 18;
LRESULT CALLBACK CMetasequoiaIME_WindowProc(HWND wndHandle, UINT uMsg, WPARAM wParam, LPARAM lParam);

class CMetasequoiaIME : public ITfTextInputProcessorEx,
                        public ITfThreadMgrEventSink,
                        public ITfTextEditSink,
                        public ITfKeyEventSink,
                        public ITfCompositionSink,
                        public ITfDisplayAttributeProvider,
                        public ITfActiveLanguageProfileNotifySink,
                        public ITfThreadFocusSink,
                        public ITfFunctionProvider,
                        public ITfFnGetPreferredTouchKeyboardLayout
{
    friend class CCompositionProcessorEngine;

  public:
    CMetasequoiaIME();
    ~CMetasequoiaIME();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, _Outptr_ void **ppvObj);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    // ITfTextInputProcessor
    STDMETHODIMP Activate(ITfThreadMgr *pThreadMgr, TfClientId tfClientId)
    {
        return ActivateEx(pThreadMgr, tfClientId, 0);
    }
    // ITfTextInputProcessorEx
    STDMETHODIMP ActivateEx(ITfThreadMgr *pThreadMgr, TfClientId tfClientId, DWORD dwFlags);
    STDMETHODIMP Deactivate();

    // ITfThreadMgrEventSink
    STDMETHODIMP OnInitDocumentMgr(_In_ ITfDocumentMgr *pDocMgr);
    STDMETHODIMP OnUninitDocumentMgr(_In_ ITfDocumentMgr *pDocMgr);
    STDMETHODIMP OnSetFocus(_In_ ITfDocumentMgr *pDocMgrFocus, _In_ ITfDocumentMgr *pDocMgrPrevFocus);
    STDMETHODIMP OnPushContext(_In_ ITfContext *pContext);
    STDMETHODIMP OnPopContext(_In_ ITfContext *pContext);

    // ITfTextEditSink
    STDMETHODIMP OnEndEdit(__RPC__in_opt ITfContext *pContext, TfEditCookie ecReadOnly,
                           __RPC__in_opt ITfEditRecord *pEditRecord);

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL fForeground);
    STDMETHODIMP OnTestKeyDown(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pIsEaten);
    STDMETHODIMP OnKeyDown(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pIsEaten);
    STDMETHODIMP OnTestKeyUp(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pIsEaten);
    STDMETHODIMP OnKeyUp(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pIsEaten);
    STDMETHODIMP OnPreservedKey(ITfContext *pContext, REFGUID rguid, BOOL *pIsEaten);

    // ITfCompositionSink
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, _In_ ITfComposition *pComposition);

    // ITfDisplayAttributeProvider
    STDMETHODIMP EnumDisplayAttributeInfo(__RPC__deref_out_opt IEnumTfDisplayAttributeInfo **ppEnum);
    STDMETHODIMP GetDisplayAttributeInfo(__RPC__in REFGUID guidInfo,
                                         __RPC__deref_out_opt ITfDisplayAttributeInfo **ppInfo);

    // ITfActiveLanguageProfileNotifySink
    STDMETHODIMP OnActivated(_In_ REFCLSID clsid, _In_ REFGUID guidProfile, _In_ BOOL isActivated);

    // ITfThreadFocusSink
    STDMETHODIMP OnSetThreadFocus();
    STDMETHODIMP OnKillThreadFocus();

    // ITfFunctionProvider
    STDMETHODIMP GetType(__RPC__out GUID *pguid);
    STDMETHODIMP GetDescription(__RPC__deref_out_opt BSTR *pbstrDesc);
    STDMETHODIMP GetFunction(__RPC__in REFGUID rguid, __RPC__in REFIID riid, __RPC__deref_out_opt IUnknown **ppunk);

    // ITfFunction
    STDMETHODIMP GetDisplayName(_Out_ BSTR *pbstrDisplayName);

    // ITfFnGetPreferredTouchKeyboardLayout, it is the Optimized layout feature.
    STDMETHODIMP GetLayout(_Out_ TKBLayoutType *ptkblayoutType, _Out_ WORD *pwPreferredLayoutId);

    // CClassFactory factory callback
    static HRESULT CreateInstance(_In_ IUnknown *pUnkOuter, REFIID riid, _Outptr_ void **ppvObj);

    // utility function for thread manager.
    ITfThreadMgr *_GetThreadMgr()
    {
        return _pThreadMgr;
    }
    TfClientId _GetClientId()
    {
        return _tfClientId;
    }
    bool _IsServerUnavailableFallbackActive() const;

    // functions for the composition object.
    void _SetComposition(_In_ ITfComposition *pComposition);
    void _TerminateComposition(TfEditCookie ec, _In_ ITfContext *pContext, BOOL isCalledFromDeactivate = FALSE);
    void _SaveCompositionContext(_In_ ITfContext *pContext);

    // key event handlers for composition/candidate/phrase common objects.
    HRESULT _HandleComplete(TfEditCookie ec, _In_ ITfContext *pContext);
    HRESULT _HandleCompleteCommitFirst(TfEditCookie ec, _In_ ITfContext *pContext);
    HRESULT _HandleCancel(TfEditCookie ec, _In_ ITfContext *pContext);
    HRESULT _HandleToogleIMEMode(TfEditCookie ec, _In_ ITfContext *pContext);

    // key event handlers for composition object.
    HRESULT _HandleCompositionInput(TfEditCookie ec, _In_ ITfContext *pContext, WCHAR wch, uint64_t requestId);
    HRESULT _HandleCompositionFinalize(TfEditCookie ec, _In_ ITfContext *pContext, BOOL fCandidateList);
    HRESULT _HandleCompositionConvert(TfEditCookie ec, _In_ ITfContext *pContext, BOOL isWildcardSearch);
    HRESULT _HandleCompositionBackspace(TfEditCookie ec, _In_ ITfContext *pContext, uint64_t requestId);
    HRESULT _HandleCompositionArrowKey(TfEditCookie ec, _In_ ITfContext *pContext, KEYSTROKE_FUNCTION keyFunction);
    HRESULT _HandleCompositionPunctuation(TfEditCookie ec, _In_ ITfContext *pContext, WCHAR wch, uint64_t requestId,
                                          const std::wstring &prefetchedText);
    HRESULT _HandleCompositionDoubleSingleByte(TfEditCookie ec, _In_ ITfContext *pContext, WCHAR wch);

    // key event handlers for candidate object.
    HRESULT _HandleCandidateFinalize(TfEditCookie ec, _In_ ITfContext *pContext, uint64_t requestId,
                                     const std::wstring &prefetchedText);
    HRESULT _HandleCandidateFinalizeForVKReturn(TfEditCookie ec, _In_ ITfContext *pContext);
    HRESULT _HandleCandidateConvert(TfEditCookie ec, _In_ ITfContext *pContext, uint64_t requestId,
                                    const std::wstring &prefetchedText);
    HRESULT _HandleCandidateArrowKey(TfEditCookie ec, _In_ ITfContext *pContext, _In_ KEYSTROKE_FUNCTION keyFunction);
    HRESULT _HandleCandidateSelectByNumber(TfEditCookie ec, _In_ ITfContext *pContext, _In_ UINT uCode,
                                           uint64_t requestId, const std::wstring &prefetchedText);

    BOOL _IsSecureMode(void)
    {
        return (_dwActivateFlags & TF_TMAE_SECUREMODE) ? TRUE : FALSE;
    }
    BOOL _IsComLess(void)
    {
        return (_dwActivateFlags & TF_TMAE_COMLESS) ? TRUE : FALSE;
    }
    BOOL _IsStoreAppMode(void)
    {
        return (_dwActivateFlags & TF_TMF_IMMERSIVEMODE) ? TRUE : FALSE;
    };

    CCompositionProcessorEngine *GetCompositionProcessorEngine()
    {
        return (_pCompositionProcessorEngine);
    };

    // Async TSF edit sessions may be granted after a focus transition. Bind
    // every text-producing session to the exact focus token that scheduled it
    // so an old worker/key message cannot commit into the next document.
    uint64_t _CaptureFocusSessionToken() const;
    bool _IsFocusSessionCurrent(uint64_t focusToken, _In_opt_ ITfContext *expectedContext = nullptr) const;
    uint64_t _CaptureCompositionEpoch() const;
    bool _IsCompositionEpochCurrent(uint64_t compositionEpoch) const;
    bool _IsCompositionCurrent(_In_opt_ ITfComposition *expectedComposition) const;
    bool _IsLocalSessionResetCurrent(UINT resetToken) const;
    void _CompleteLocalSessionReset(UINT resetToken);
    bool _IsDeferredKeyReplayCurrent(uint64_t replayToken,
                                     uint64_t focusGeneration,
                                     _In_opt_ ITfContext *expectedContext) const;
    void _CompleteDeferredKeyReplay(uint64_t replayToken);
    void _RetryDeferredKeyReplay(uint64_t replayToken);

    // comless helpers
    static HRESULT CMetasequoiaIME::CreateInstance(REFCLSID rclsid, REFIID riid, _Outptr_result_maybenull_ LPVOID *ppv,
                                                   _Out_opt_ HINSTANCE *phInst, BOOL isComLessMode);
    static HRESULT CMetasequoiaIME::ComLessCreateInstance(REFGUID rclsid, REFIID riid,
                                                          _Outptr_result_maybenull_ void **ppv,
                                                          _Out_opt_ HINSTANCE *phInst);
    static HRESULT CMetasequoiaIME::GetComModuleName(REFGUID rclsid, _Out_writes_(cchPath) WCHAR *wchPath,
                                                     DWORD cchPath);

    static void IpcWorkerThread(CMetasequoiaIME *pIME);
    void _QueuePendingServerCandidate(UINT msgType, _In_z_ const WCHAR *pCandidateString);
    bool _TakePendingServerCandidate(_Out_ UINT *pMsgType, _Out_ std::wstring *pCandidateString);
    void _ScheduleCandidatePresenterCleanup(_In_ CCandidateListUIPresenter *pPresenter);
    void _DrainPendingCandidatePresenterCleanup();

  private:
    // functions for the composition object.
    HRESULT _HandleCompositionInputWorker(_In_ CCompositionProcessorEngine *pCompositionProcessorEngine,
                                          TfEditCookie ec, _In_ ITfContext *pContext, uint64_t requestId);
    HRESULT _CreateAndStartCandidate(_In_ CCompositionProcessorEngine *pCompositionProcessorEngine, TfEditCookie ec,
                                     _In_ ITfContext *pContext);
    HRESULT _HandleCandidateWorker(TfEditCookie ec, _In_ ITfContext *pContext, uint64_t requestId,
                                   const std::wstring &prefetchedText);

    struct AsyncKeyRequest
    {
        UINT message = 0;
        UINT code = 0;
        WCHAR wch = L'\0';
        uint64_t requestId = UINT64_MAX;
        uint64_t focusToken = 0;
        uint64_t compositionEpoch = 0;
        uint64_t deferredReplayToken = 0;
        std::wstring prefetchedText;
    };
    bool _PostAsyncKeyRequest(UINT message, UINT code, WCHAR wch, uint64_t requestId,
                               std::wstring prefetchedText = {},
                               uint64_t expectedFocusToken = 0,
                               uint64_t expectedCompositionEpoch = 0,
                               uint64_t deferredReplayToken = 0);
    bool _TakeAsyncKeyRequest(UINT message, UINT token, _Out_ AsyncKeyRequest &request);
    struct WorkerCandidateCommit
    {
        std::wstring text;
        uint64_t focusToken = 0;
        uint64_t compositionEpoch = 0;
    };
    bool _PostServerCandidateCommit(_In_z_ const WCHAR *candidateText);
    bool _TakeServerCandidateCommit(UINT token, _Out_ WorkerCandidateCommit &request);
    struct WorkerCompartmentSwitch
    {
        UINT messageType = 0;
        uint64_t focusToken = 0;
        uint64_t compositionEpoch = 0;
    };
    bool _PostWorkerCompartmentSwitch(UINT messageType, uint64_t focusToken);
    bool _TakeWorkerCompartmentSwitch(UINT token, _Out_ WorkerCompartmentSwitch &request);
    void _ClearAsyncKeyRequests();
    void _ClearPendingIpcRequests();
    void _RequestLocalSessionReset(_In_opt_ ITfContext *preferredContext, UINT resetToken);
    bool _CaptureWindowsTextInputHostFocusLoss();

    struct DeferredKeyDown
    {
        enum class Kind
        {
            KeyDown,
            PreservedKey,
            ApplicationText
        };

        Kind kind = Kind::KeyDown;
        ITfContext *context = nullptr;
        WPARAM wParam = 0;
        LPARAM lParam = 0;
        WCHAR translatedWch = L'\0';
        UINT modifiersDown = 0;
        _KEYSTROKE_STATE keyState = {};
        GUID preservedKey = {};
        uint64_t focusGeneration = 0;
        UINT replayAttempts = 0;
        bool preservedApplied = false;
    };
    enum class KeyDownDispatchResult
    {
        Complete,
        Retry,
        AwaitingCompletion
    };
    bool _HasDeferredKeyBarrier() const;
    bool _DeferredKeyQueueHasCapacity() const;
    void _EnsureDeferredKeyProjection();
    void _ApplyDeferredKeyProjection(const _KEYSTROKE_STATE &keyState, WCHAR wch);
    void _ApplyDeferredPreservedKeyProjection(REFGUID preservedKey);
    bool _RefreshDeferredRecoveryPrefix(_In_ ITfContext *pContext);
    void _ArmDeferredRecoveryForTransport(_In_opt_ ITfContext *pContext);
    bool _ClassifyDeferredKeyDown(_In_ ITfContext *pContext, WPARAM wParam,
                                  _In_opt_ const WCHAR *translatedWch,
                                  _In_opt_ const UINT *modifiersDown,
                                  _Out_ WCHAR *classifiedWch,
                                  _Out_ UINT *classifiedCode,
                                  _Out_ _KEYSTROKE_STATE *keyState);
    bool _QueueDeferredKeyDown(_In_ ITfContext *pContext, WPARAM wParam, LPARAM lParam,
                               WCHAR translatedWch, UINT modifiersDown,
                               const _KEYSTROKE_STATE &keyState);
    bool _QueueDeferredPreservedKey(_In_ ITfContext *pContext, REFGUID preservedKey);
    void _ClearDeferredKeyDowns();
    void _ScheduleDeferredKeyDownDrain();
    void _DrainOneDeferredKeyDown();
    void _TryLeaveServerUnavailableFallback();
    HRESULT _RequestDeferredApplicationTextEditSession(
        _In_ ITfContext *pContext, WCHAR wch, uint64_t expectedFocusToken,
        uint64_t expectedFocusGeneration, uint64_t deferredReplayToken);
    KeyDownDispatchResult _DispatchKeyDown(_In_ ITfContext *pContext, WPARAM wParam,
                                           LPARAM lParam, _Out_ BOOL *pIsEaten,
                                           _In_opt_ const WCHAR *translatedWch,
                                           _In_opt_ const UINT *modifiersDown,
                                           _In_opt_ const _KEYSTROKE_STATE *prevalidatedKeyState,
                                           bool canDefer,
                                           uint64_t expectedFocusGeneration,
                                           uint64_t deferredReplayToken = 0);
    void _DispatchPreservedKey(_In_ ITfContext *pContext, REFGUID preservedKey,
                               _Out_ BOOL *pIsEaten,
                               uint64_t expectedFocusGeneration,
                               bool isPrevalidated,
                               uint64_t deferredReplayToken = 0);

    void _StartComposition(_In_ ITfContext *pContext);
    HRESULT _EndComposition(_In_opt_ ITfContext *pContext,
                            _In_opt_ ITfComposition *expectedComposition = nullptr,
                            bool bypassFocusValidation = false);
    BOOL _IsComposing();
    BOOL _IsKeyboardDisabled();

    HRESULT _AddComposingAndChar(TfEditCookie ec, _In_ ITfContext *pContext, _In_ CStringRange *pstrAddString);
    HRESULT _AddCharAndFinalize(TfEditCookie ec, _In_ ITfContext *pContext, _In_ CStringRange *pstrAddString);
    HRESULT _InsertTextToComposition(TfEditCookie ec, _In_ ITfContext *pContext, _In_ CStringRange *pstrAddString);
    HRESULT _SetCompositionTextAndSelection(TfEditCookie ec, _In_ ITfContext *pContext,
                                            _In_ CStringRange *pstrAddString);

    BOOL _FindComposingRange(TfEditCookie ec, _In_ ITfContext *pContext, _In_ ITfRange *pSelection,
                             _Outptr_result_maybenull_ ITfRange **ppRange);
    HRESULT _SetInputString(TfEditCookie ec, _In_ ITfContext *pContext, _Out_opt_ ITfRange *pRange,
                            _In_ CStringRange *pstrAddString, BOOL exist_composing);
    HRESULT _InsertAtSelection(TfEditCookie ec, _In_ ITfContext *pContext, _In_ CStringRange *pstrAddString,
                               _Outptr_ ITfRange **ppCompRange);

    HRESULT _RemoveDummyCompositionForComposing(TfEditCookie ec, _In_ ITfComposition *pComposition);

    // Invoke key handler edit session
    HRESULT _InvokeKeyHandler(_In_ ITfContext *pContext, UINT code, WCHAR wch, DWORD flags,
                              _KEYSTROKE_STATE keyState, uint64_t requestId,
                               std::wstring prefetchedText = {}, UINT localResetToken = 0,
                               uint64_t expectedCompositionEpoch = 0,
                               uint64_t expectedFocusToken = 0,
                               uint64_t deferredReplayToken = 0);
    HRESULT _RequestDirectPunctuationEditSession(_In_ ITfContext *pContext, UINT code, WCHAR wch,
                                                   uint64_t requestId, std::wstring prefetchedText,
                                                   uint64_t expectedFocusToken = 0,
                                                   uint64_t expectedCompositionEpoch = 0,
                                                   uint64_t deferredReplayToken = 0);

    // function for the language property
    BOOL _SetCompositionLanguage(TfEditCookie ec, _In_ ITfContext *pContext);

    // function for the display attribute
    void _ClearCompositionDisplayAttributes(
        TfEditCookie ec, _In_ ITfContext *pContext,
        _In_opt_ ITfComposition *expectedComposition = nullptr);
    BOOL _SetCompositionDisplayAttributes(TfEditCookie ec, _In_ ITfContext *pContext, TfGuidAtom gaDisplayAttribute);
    BOOL _SetCompositionDisplayAttributesForRange(TfEditCookie ec, _In_ ITfContext *pContext,
                                                  _In_ ITfRange *pRangeComposition,
                                                  TfGuidAtom gaDisplayAttribute);
    BOOL _InitDisplayAttributeGuidAtom();

    BOOL _InitThreadMgrEventSink();
    void _UninitThreadMgrEventSink();
    void _HandleFocusedContextStackChange(_In_opt_ ITfContext *changedContext);

    BOOL _InitTextEditSink(_In_opt_ ITfDocumentMgr *pDocMgr);

    void _UpdateLanguageBarOnSetFocus(_In_ ITfDocumentMgr *pDocMgrFocus);

    BOOL _InitKeyEventSink();
    void _UninitKeyEventSink();

    BOOL _InitActiveLanguageProfileNotifySink();
    void _UninitActiveLanguageProfileNotifySink();

    BOOL _IsKeyEaten(_In_ ITfContext *pContext, UINT codeIn, _Out_ UINT *pCodeOut, _Out_writes_(1) WCHAR *pwch,
                     _Out_opt_ _KEYSTROKE_STATE *pKeyState,
                     _In_opt_ const WCHAR *translatedWch = nullptr,
                     bool freshCompositionState = false);

    BOOL _IsRangeCovered(TfEditCookie ec, _In_ ITfRange *pRangeTest, _In_ ITfRange *pRangeCover);
    VOID _DeleteCandidateList(BOOL fForce, _In_opt_ ITfContext *pContext);

    WCHAR ConvertVKey(UINT code);

    BOOL _InitThreadFocusSink();
    void _UninitThreadFocusSink();

    BOOL _InitFunctionProviderSink();
    void _UninitFunctionProviderSink();

    BOOL _AddTextProcessorEngine();

    BOOL VerifyMetasequoiaIMECLSID(_In_ REFCLSID clsid);

    friend LRESULT CALLBACK CMetasequoiaIME_WindowProc(HWND wndHandle, UINT uMsg, WPARAM wParam, LPARAM lParam);

  private:
    ITfThreadMgr *_pThreadMgr;
    TfClientId _tfClientId;
    DWORD _dwActivateFlags;

    // The cookie of ThreadMgrEventSink
    DWORD _threadMgrEventSinkCookie;

    ITfContext *_pTextEditSinkContext;
    DWORD _textEditSinkCookie;

    // The cookie of ActiveLanguageProfileNotifySink
    DWORD _activeLanguageProfileNotifySinkCookie;

    // The cookie of ThreadFocusSink
    DWORD _dwThreadFocusSinkCookie;

    // Composition Processor Engine object.
    CCompositionProcessorEngine *_pCompositionProcessorEngine;

    // Language bar item object.
    CLangBarItemButton *_pLangBarItem;

    // the current composition object.
    ITfComposition *_pComposition;

    // guidatom for the display attibute.
    TfGuidAtom _gaDisplayAttributeInput;
    TfGuidAtom _gaDisplayAttributeConverted;

    CANDIDATE_MODE _candidateMode;
    CCandidateListUIPresenter *_pCandidateListUIPresenter;
    BOOL _isCandidateWithWildcard : 1;

    ITfDocumentMgr *_pDocMgrLastFocused;

    ITfContext *_pContext;

    ITfCompartment *_pSIPIMEOnOffCompartment;
    DWORD _dwSIPIMEOnOffCompartmentSinkCookie;

    HWND _msgWndHandle;
    std::thread *_pIpcThread;
    std::atomic<HANDLE> _hToTsfWorkerThreadPipe;
    std::atomic<UINT> _workerPipeGeneration;
    HANDLE _ipcStopEvent;
    std::atomic<bool> _shouldStopIpcThread;
    UINT _ipcReconnectDelayMs;
    std::mutex _pendingCommitCandidateMutex;
    std::map<UINT, WorkerCandidateCommit> _pendingServerCommitMessages;
    std::map<UINT, WorkerCompartmentSwitch> _pendingWorkerSwitchMessages;
    std::map<UINT, AsyncKeyRequest> _pendingAsyncKeyMessages;
    std::atomic<bool> _workerCommitReady;
    std::atomic<uint64_t> _expectedWorkerFocusToken;
    std::atomic<uint64_t> _acknowledgedWorkerFocusToken;
    std::atomic<uint64_t> _compositionEpoch;
    std::atomic<bool> _localSessionResetPending;
    std::atomic<UINT> _localSessionResetToken;
    bool _localResetEditSessionQueued;
    UINT _queuedLocalResetToken;
    bool _focusResetPending;
    bool _activationRequired;
    bool _focusLostToWindowsTextInputHost;
    bool _hasPendingServerCandidate;
    UINT _pendingServerCandidateMsgType;
    std::wstring _pendingServerCandidateString;
    std::deque<CCandidateListUIPresenter *> _pendingCandidatePresenterCleanup;
    std::deque<DeferredKeyDown> _deferredKeyDowns;
    std::deque<DeferredKeyDown> _deferredAppliedPrefix;
    DeferredKeyDown _deferredKeyInFlight;
    bool _hasDeferredKeyInFlight;
    uint64_t _deferredKeyReplayToken;
    uint64_t _nextDeferredKeyReplayToken;
    bool _deferredKeyProjectionValid;
    bool _deferredProjectedImeOpen;
    bool _deferredProjectedPunctuationOpen;
    bool _deferredProjectedDoubleSingleByteOpen;
    size_t _deferredProjectedInputLength;
    bool _deferredProjectedCandidateActive;
    bool _deferredProjectedUnicodeMode;
    uint64_t _deferredKeyFocusGeneration;
    bool _deferredKeyDrainPosted;
    bool _serverUnavailableFallbackActive;

    LONG _refCount;

    // Support the search integration
    ITfFnSearchCandidateProvider *_pITfFnSearchCandidateProvider;
};
