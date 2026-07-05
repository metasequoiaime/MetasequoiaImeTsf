#include "FanyDefines.h"
#include "Globals.h"
#include "Private.h"
#include "MetasequoiaIME.h"
#include "CandidateListUIPresenter.h"
#include "CompositionProcessorEngine.h"
#include "MetasequoiaIMEBaseStructure.h"
#include "define.h"
#include <cwchar>
#include <debugapi.h>
#include <intsafe.h>
#include <minwindef.h>
#include <winuser.h>
#include "Ipc.h"
#include "fmt/xchar.h"
#include "../Utils/PerfTimer.h"

//////////////////////////////////////////////////////////////////////
//
// CMetasequoiaIME candidate key handler methods
//
//////////////////////////////////////////////////////////////////////

const int MOVEUP_ONE = -1;
const int MOVEDOWN_ONE = 1;
const int MOVETO_TOP = 0;
const int MOVETO_BOTTOM = -1;

namespace
{
bool IsTimeoutSentinelCandidate(UINT msgType, const std::wstring &candidateString)
{
    return msgType == Global::DataFromServerMsgType::Normal && candidateString == L"T";
}
}

//+---------------------------------------------------------------------------
//
// _HandleCandidateFinalize
//
//----------------------------------------------------------------------------

HRESULT CMetasequoiaIME::_HandleCandidateFinalize(TfEditCookie ec, _In_ ITfContext *pContext)
{
    HRESULT hr = S_OK;
    PerfTimer finalizeTimer;

    CStringRange keyStrokebuffer = _pCompositionProcessorEngine->GetKeystrokeBuffer();
    DWORD_PTR keystrokeBufLen = keyStrokebuffer.GetLength();
    DWORD_PTR candidateLen = keystrokeBufLen;
    CStringRange candidateString(keyStrokebuffer);
    std::wstring pendingCommitCandidate = _TakePendingCommitCandidate();


#ifdef FANY_DEBUG
    OutputDebugString(fmt::format(L"[msime]: create_word, keystrokeBuffer: {}", keyStrokebuffer.ToWString()).c_str());
#endif

    // _pCandidateListUIPresenter would be null in uwp/metro apps
    if (nullptr == _pCandidateListUIPresenter)
    {
        // goto NoPresenter;
    }

    if (candidateLen)
    {
        if (!pendingCommitCandidate.empty())
        {
            ClearNamedpipeDataIfExists(true);
            candidateString.Set(pendingCommitCandidate.c_str(), pendingCommitCandidate.length());
            PerfTimer insertTextTimer;
            hr = _InsertTextToComposition(ec, pContext, &candidateString);
            if (FAILED(hr))
            {
                hr = _AddComposingAndChar(ec, pContext, &candidateString);
            }
            if (FAILED(hr))
            {
                return hr;
            }

            PerfTimer completeTimer;
            _HandleCompleteCommitFirst(ec, pContext);
            return hr;
        }

        UINT serverMsgType = Global::DataFromServerMsgType::OutofRange;
        std::wstring serverCandidateString;
        const bool hasPrefetchedServerCandidate =
            _TakePendingServerCandidate(&serverMsgType, &serverCandidateString);
        if (hasPrefetchedServerCandidate)
        {
            if (IsTimeoutSentinelCandidate(serverMsgType, serverCandidateString))
            {
                struct FanyImeNamedpipeDataToTsf *retryData = TryReadDataFromServerPipeWithTimeout();
                serverMsgType = retryData->msg_type;
                serverCandidateString = retryData->candidate_string;
            }
        }
        else
        {
            PerfTimer pipeReadTimer;
            struct FanyImeNamedpipeDataToTsf *receivedData = TryReadDataFromServerPipeWithTimeout();
            serverMsgType = receivedData->msg_type;
            serverCandidateString = receivedData->candidate_string;
            if (IsTimeoutSentinelCandidate(serverMsgType, serverCandidateString))
            {
                struct FanyImeNamedpipeDataToTsf *retryData = TryReadDataFromServerPipeWithTimeout();
                serverMsgType = retryData->msg_type;
                serverCandidateString = retryData->candidate_string;
            }
        }

        if (serverMsgType == Global::DataFromServerMsgType::OutofRange) // Candidate index out of range
        {
            return hr;
        }
        else if (serverMsgType == Global::DataFromServerMsgType::Normal) // 只有正常情况下才会上屏
        {
            GlobalIme::word_for_creating_word = L"";
#ifdef FANY_DEBUG
            OutputDebugString(fmt::format(L"[msime]: create_word, normal???").c_str());
#endif
            candidateString.Set(serverCandidateString.c_str(), serverCandidateString.length());
            PerfTimer insertTextTimer;
            hr = _InsertTextToComposition(ec, pContext, &candidateString);
            if (FAILED(hr))
            {
                hr = _AddComposingAndChar(ec, pContext, &candidateString);
            }
            if (FAILED(hr))
            {
                return hr;
            }
        }
        /* 处理造词的逻辑 */
        else if (serverMsgType == Global::DataFromServerMsgType::NeedToCreateWord)
        {
            std::wstring data = serverCandidateString;
            const size_t separator = data.find(L'\t');
            if (separator != std::wstring::npos)
            {
                std::wstring remainingRawInput = data.substr(0, separator);
                std::wstring curWord = data.substr(separator + 1);
#ifdef FANY_DEBUG
                OutputDebugString(
                    fmt::format(L"[msime]: create_word, remainingRawInput: {}, curWord: {}", remainingRawInput, curWord)
                        .c_str());
#endif
                GlobalIme::word_for_creating_word = curWord;
                CCompositionProcessorEngine *pCompositionProcessorEngine = nullptr;
                pCompositionProcessorEngine = _pCompositionProcessorEngine;

                DWORD_PTR vKeyLen = pCompositionProcessorEngine->GetVirtualKeyLength();

                for (DWORD_PTR i = 0; i < vKeyLen; i++)
                {
                    DWORD_PTR curVkeyLen = pCompositionProcessorEngine->GetVirtualKeyLength();
                    if (curVkeyLen)
                    {
                        pCompositionProcessorEngine->RemoveVirtualKey(curVkeyLen - 1);
                    }
                }
                for (DWORD_PTR i = 0; i < remainingRawInput.length(); i++)
                {
                    pCompositionProcessorEngine->AddVirtualKey(remainingRawInput[i]);
                }

                if (pCompositionProcessorEngine->GetVirtualKeyLength())
                {
                    _HandleCompositionInputWorker(pCompositionProcessorEngine, ec, pContext);
                }
                else
                {
                    _HandleCancel(ec, pContext);
                }
            }
#ifdef FANY_DEBUG
            OutputDebugString(
                fmt::format(L"[msime]: create_word: current word part {}", GlobalIme::word_for_creating_word).c_str());
#endif
            return hr;
        }
    }

NoPresenter:

    PerfTimer completeTimer;
    _HandleCompleteCommitFirst(ec, pContext);

    return hr;
}

//+---------------------------------------------------------------------------
//
// _HandleCandidateFinalizeForVKReturn
//
//----------------------------------------------------------------------------

HRESULT CMetasequoiaIME::_HandleCandidateFinalizeForVKReturn(TfEditCookie ec, _In_ ITfContext *pContext)
{
    HRESULT hr = S_OK;

    CStringRange keyStrokebuffer = _pCompositionProcessorEngine->GetKeystrokeBuffer();
    DWORD_PTR keystrokeBufLen = keyStrokebuffer.GetLength();
    DWORD_PTR candidateLen = keystrokeBufLen;
    CStringRange candidateString(keyStrokebuffer);

    if (nullptr == _pCandidateListUIPresenter)
    {
        // goto NoPresenter;
    }

#ifdef FANY_DEBUG
    std::wstring msg(candidateString.Get(), candidateLen);
    OutputDebugString(fmt::format(L"[msime]: {}", msg).c_str());
#endif

    if (candidateLen)
    {
        hr = _AddComposingAndChar(ec, pContext, &candidateString);

        if (FAILED(hr))
        {
            return hr;
        }
    }

NoPresenter:

    _HandleComplete(ec, pContext);

    return hr;
}

//+---------------------------------------------------------------------------
//
// _HandleCandidateConvert
//
//----------------------------------------------------------------------------

HRESULT CMetasequoiaIME::_HandleCandidateConvert(TfEditCookie ec, _In_ ITfContext *pContext)
{
    return _HandleCandidateWorker(ec, pContext);
}

//+---------------------------------------------------------------------------
//
// _HandleCandidateWorker
//
//----------------------------------------------------------------------------

HRESULT CMetasequoiaIME::_HandleCandidateWorker(TfEditCookie ec, _In_ ITfContext *pContext)
{
    HRESULT hrReturn = E_FAIL;
    DWORD_PTR candidateLen = 0;
    const WCHAR *pCandidateString = nullptr;
    CStringRange candidateString;

    if (nullptr == _pCandidateListUIPresenter)
    {
        hrReturn = S_OK;
        goto Exit;
    }

    candidateLen = _pCandidateListUIPresenter->_GetSelectedCandidateString(&pCandidateString);
    if (0 == candidateLen)
    {
        hrReturn = S_FALSE;
        goto Exit;
    }

    candidateString.Set(pCandidateString, candidateLen);
    hrReturn = _HandleCandidateFinalize(ec, pContext);

Exit:
    return hrReturn;
}

//+---------------------------------------------------------------------------
//
// _HandleCandidateArrowKey
//
//----------------------------------------------------------------------------

HRESULT CMetasequoiaIME::_HandleCandidateArrowKey( //
    TfEditCookie ec,                               //
    _In_ ITfContext *pContext,                     //
    _In_ KEYSTROKE_FUNCTION keyFunction            //
)
{
    ec;
    pContext;

    if (_pCandidateListUIPresenter == nullptr)
    {
        return S_OK;
    }

    if ((keyFunction == FUNCTION_MOVE_PAGE_UP) || (keyFunction == FUNCTION_MOVE_PAGE_DOWN) ||
        (keyFunction == FUNCTION_MOVE_PAGE_TOP) || (keyFunction == FUNCTION_MOVE_PAGE_BOTTOM))
    {
        if (_pCandidateListUIPresenter->_GetCount() <= 1)
        {
            return S_OK;
        }
    }

    _pCandidateListUIPresenter->AdviseUIChangedByArrowKey(keyFunction);

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// _HandleCandidateSelectByNumber
//
//----------------------------------------------------------------------------

HRESULT CMetasequoiaIME::_HandleCandidateSelectByNumber(TfEditCookie ec, _In_ ITfContext *pContext, _In_ UINT uCode)
{
    int iSelectAsNumber = _pCompositionProcessorEngine->GetCandidateListIndexRange()->GetIndex(uCode);
    if (iSelectAsNumber == -1)
    {
        return S_FALSE;
    }

    if (_pCandidateListUIPresenter)
    {
        if (_pCandidateListUIPresenter->_SetSelectionInPage(iSelectAsNumber))
        {
            return _HandleCandidateConvert(ec, pContext);
        }
    }

    return S_FALSE;
}

//////////////////////////////////////////////////////////////////////
//
// CCandidateListUIPresenter class
//
//////////////////////////////////////////////////////////////////////

//+---------------------------------------------------------------------------
//
// ctor
//
//----------------------------------------------------------------------------

CCandidateListUIPresenter::CCandidateListUIPresenter(_In_ CMetasequoiaIME *pTextService,
                                                     KEYSTROKE_CATEGORY Category, _In_ CCandidateRange *pIndexRange,
                                                     BOOL hideWindow)
    : CTfTextLayoutSink(pTextService), _candidateState(pIndexRange)
{
    _pIndexRange = pIndexRange;

    _Category = Category;

    _updatedFlags = 0;

    _uiElementId = (DWORD)-1;
    _isShowMode = TRUE;       // store return value from BeginUIElement
    _hideWindow = hideWindow; // Hide window flag from [Configuration] CandidateList.Phrase.HideWindow

    _pTextService = pTextService;
    _pTextService->AddRef();

    _refCount = 1;
    _candidateUiSessionActive = FALSE;
    _candidateWindowVisible = FALSE;
    _asyncCleanupPending = FALSE;
}

//+---------------------------------------------------------------------------
//
// dtor
//
//----------------------------------------------------------------------------

CCandidateListUIPresenter::~CCandidateListUIPresenter()
{
    _EndCandidateList();
    _pTextService->Release();
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::IUnknown::QueryInterface
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::QueryInterface(REFIID riid, _Outptr_ void **ppvObj)
{
    if (CTfTextLayoutSink::QueryInterface(riid, ppvObj) == S_OK)
    {
        return S_OK;
    }

    if (ppvObj == nullptr)
    {
        return E_INVALIDARG;
    }

    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_ITfUIElement) || IsEqualIID(riid, IID_ITfCandidateListUIElement))
    {
        *ppvObj = (ITfCandidateListUIElement *)this;
    }
    else if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfCandidateListUIElementBehavior))
    {
        *ppvObj = (ITfCandidateListUIElementBehavior *)this;
    }
    else if (IsEqualIID(riid, __uuidof(ITfIntegratableCandidateListUIElement)))
    {
        *ppvObj = (ITfIntegratableCandidateListUIElement *)this;
    }

    if (*ppvObj)
    {
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::IUnknown::AddRef
//
//----------------------------------------------------------------------------

STDAPI_(ULONG) CCandidateListUIPresenter::AddRef()
{
    CTfTextLayoutSink::AddRef();
    return ++_refCount;
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::IUnknown::Release
//
//----------------------------------------------------------------------------

STDAPI_(ULONG) CCandidateListUIPresenter::Release()
{
    CTfTextLayoutSink::Release();

    LONG cr = --_refCount;

    assert(_refCount >= 0);

    if (_refCount == 0)
    {
        delete this;
    }

    return cr;
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::ITfUIElement::GetDescription
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::GetDescription(BSTR *pbstr)
{
    if (pbstr)
    {
        *pbstr = SysAllocString(L"Cand");
    }
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::ITfUIElement::GetGUID
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::GetGUID(GUID *pguid)
{
    *pguid = Global::MetasequoiaIMEGuidCandUIElement;
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::ITfUIElement::Show
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::Show(BOOL showCandidateWindow)
{
    PerfTimer timer;
    double moveWindowElapsedMs = 0;
    double updateUiElapsedMs = 0;
    if (showCandidateWindow)
    {
        if (_hideWindow)
        {
            _candidateWindowVisible = FALSE;
        }
        else
        {
            PerfTimer moveWindowTimer;
            _MoveWindowToTextExt();
            moveWindowElapsedMs = moveWindowTimer.ElapsedMs();
            _candidateWindowVisible = TRUE;
        }
    }
    else
    {
        _candidateWindowVisible = FALSE;
        _updatedFlags = TF_CLUIE_SELECTION | TF_CLUIE_CURRENTPAGE;
        PerfTimer updateUiTimer;
        _UpdateUIElement();
        updateUiElapsedMs = updateUiTimer.ElapsedMs();
    }
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::ITfUIElement::IsShown
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::IsShown(BOOL *pIsShow)
{
    *pIsShow = _candidateWindowVisible;
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::GetUpdatedFlags
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::GetUpdatedFlags(DWORD *pdwFlags)
{
    *pdwFlags = _updatedFlags;
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::GetDocumentMgr
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::GetDocumentMgr(ITfDocumentMgr **ppdim)
{
    *ppdim = nullptr;

    return E_NOTIMPL;
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::GetCount
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::GetCount(UINT *pCandidateCount)
{
    *pCandidateCount = _candidateState.GetCount();
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::GetSelection
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::GetSelection(UINT *pSelectedCandidateIndex)
{
    *pSelectedCandidateIndex = _candidateState.GetSelection();
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::GetString
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::GetString(UINT uIndex, BSTR *pbstr)
{
    if (uIndex >= _candidateState.GetCount())
    {
        return E_FAIL;
    }

    DWORD candidateLen = 0;
    const WCHAR *pCandidateString = nullptr;

    candidateLen = _candidateState.GetCandidateString(static_cast<int>(uIndex), &pCandidateString);

    *pbstr = (candidateLen == 0) ? nullptr : SysAllocStringLen(pCandidateString, candidateLen);

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::GetPageIndex
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::GetPageIndex(UINT *pIndex, UINT uSize, UINT *puPageCnt)
{
    return _candidateState.GetPageIndex(pIndex, uSize, puPageCnt);
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::SetPageIndex
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::SetPageIndex(UINT *pIndex, UINT uPageCnt)
{
    return _candidateState.SetPageIndex(pIndex, uPageCnt);
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::GetCurrentPage
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::GetCurrentPage(UINT *puPage)
{
    return _candidateState.GetCurrentPage(puPage);
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElementBehavior::SetSelection
// It is related of the mouse clicking behavior upon the suggestion window
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::SetSelection(UINT nIndex)
{
    _candidateState.SetSelectionSilently(nIndex);

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElementBehavior::Finalize
// It is related of the mouse clicking behavior upon the suggestion window
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::Finalize(void)
{
    _CandidateChangeNotification(CAND_ITEM_SELECT);
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElementBehavior::Abort
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::Abort(void)
{
    return E_NOTIMPL;
}

//+---------------------------------------------------------------------------
//
// ITfIntegratableCandidateListUIElement::SetIntegrationStyle
// To show candidateNumbers on the suggestion window
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::SetIntegrationStyle(GUID guidIntegrationStyle)
{
    return (guidIntegrationStyle == GUID_INTEGRATIONSTYLE_SEARCHBOX) ? S_OK : E_NOTIMPL;
}

//+---------------------------------------------------------------------------
//
// ITfIntegratableCandidateListUIElement::GetSelectionStyle
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::GetSelectionStyle(_Out_ TfIntegratableCandidateListSelectionStyle *ptfSelectionStyle)
{
    *ptfSelectionStyle = STYLE_ACTIVE_SELECTION;
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfIntegratableCandidateListUIElement::OnKeyDown
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::OnKeyDown(_In_ WPARAM wParam, _In_ LPARAM lParam, _Out_ BOOL *pIsEaten)
{
    wParam;
    lParam;

    *pIsEaten = TRUE;
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfIntegratableCandidateListUIElement::ShowCandidateNumbers
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::ShowCandidateNumbers(_Out_ BOOL *pIsShow)
{
    *pIsShow = TRUE;
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfIntegratableCandidateListUIElement::FinalizeExactCompositionString
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::FinalizeExactCompositionString()
{
    return E_NOTIMPL;
}

//+---------------------------------------------------------------------------
//
// _StartCandidateList
//
//----------------------------------------------------------------------------

HRESULT CCandidateListUIPresenter::_StartCandidateList(TfClientId tfClientId, _In_ ITfDocumentMgr *pDocumentMgr,
                                                       _In_ ITfContext *pContextDocument, TfEditCookie ec,
                                                       _In_ ITfRange *pRangeComposition, UINT wndWidth)
{
    pDocumentMgr;
    tfClientId;
    pContextDocument;
    wndWidth;

    PerfTimer timer;
    HRESULT hr = E_FAIL;

    PerfTimer startLayoutTimer;
    if (FAILED(_StartLayout(pContextDocument, ec, pRangeComposition)))
    {
        goto Exit;
    }
    double startLayoutElapsedMs = startLayoutTimer.ElapsedMs();

    PerfTimer beginUiTimer;
    BeginUIElement();
    _candidateWindowVisible = FALSE;
    double beginUiElapsedMs = beginUiTimer.ElapsedMs();

    RECT rcTextExt;
    double getTextExtElapsedMs = 0;
    double layoutChangeElapsedMs = 0;
    if (SUCCEEDED(_GetTextExt(&rcTextExt)))
    {
        getTextExtElapsedMs = 0; // _GetTextExt already internally timed
        Global::Point[0] = rcTextExt.left * Global::DpiScale;
        Global::Point[1] = rcTextExt.bottom * Global::DpiScale;
        PerfTimer layoutChangeTimer;
        _LayoutChangeNotification(&rcTextExt);
        layoutChangeElapsedMs = layoutChangeTimer.ElapsedMs();
    }

    hr = S_OK;

Exit:
    if (FAILED(hr))
    {
        _EndCandidateList();
    }
    return hr;
}

//+---------------------------------------------------------------------------
//
// _EndCandidateList
//
//----------------------------------------------------------------------------

void CCandidateListUIPresenter::_EndCandidateList()
{
    PerfTimer timer;
    const bool hadUiElement = (_uiElementId != static_cast<DWORD>(-1));
    const bool hadUiSession = (_candidateUiSessionActive != FALSE);
    PerfTimer endUiTimer;
    EndUIElement();
    double endUiElapsedMs = endUiTimer.ElapsedMs();

    PerfTimer endSessionTimer;
    EndCandidateUiSession();
    double endSessionElapsedMs = endSessionTimer.ElapsedMs();

    PerfTimer clearStateTimer;
    _candidateState.Clear();
    _candidateWindowVisible = FALSE;
    double clearStateElapsedMs = clearStateTimer.ElapsedMs();

    PerfTimer endLayoutTimer;
    _EndLayout();
    double endLayoutElapsedMs = endLayoutTimer.ElapsedMs();

}

void CCandidateListUIPresenter::_PrepareForAsyncCleanup()
{
    _asyncCleanupPending = TRUE;
}

void CCandidateListUIPresenter::_NotifyUI()
{
    PerfTimer timer;
    if (_candidateUiSessionActive)
    {
        UpdateCandidateUiSession();
    }
    else
    {
        BeginCandidateUiSession();
    }
}

//+---------------------------------------------------------------------------
//
// _SetText
//
//----------------------------------------------------------------------------

void CCandidateListUIPresenter::_SetText(_In_ CMetasequoiaImeArray<CCandidateListItem> *pCandidateList,
                                         BOOL isAddFindKeyCode)
{
    PerfTimer timer;
    PerfTimer addCandidateTimer;
    AddCandidateToCandidateListUI(pCandidateList, isAddFindKeyCode);
    double addCandidateElapsedMs = addCandidateTimer.ElapsedMs();

    PerfTimer setPageIndexTimer;
    SetPageIndexWithScrollInfo(pCandidateList);
    double setPageIndexElapsedMs = setPageIndexTimer.ElapsedMs();

    double uiRefreshElapsedMs = 0;
    if (_isShowMode)
    {
        _NotifyUI(); // has its own timing
    }
    else
    {
        PerfTimer uiRefreshTimer;
        _updatedFlags =
            TF_CLUIE_COUNT | TF_CLUIE_SELECTION | TF_CLUIE_STRING | TF_CLUIE_PAGEINDEX | TF_CLUIE_CURRENTPAGE;
        _UpdateUIElement();
        uiRefreshElapsedMs = uiRefreshTimer.ElapsedMs();
    }

}

void CCandidateListUIPresenter::AddCandidateToCandidateListUI(     //
    _In_ CMetasequoiaImeArray<CCandidateListItem> *pCandidateList, //
    BOOL isAddFindKeyCode                                          //
)
{
    for (UINT index = 0; index < pCandidateList->Count(); index++)
    {
        _candidateState.AddCandidate(pCandidateList->GetAt(index), isAddFindKeyCode);
    }
}

void CCandidateListUIPresenter::SetPageIndexWithScrollInfo(       //
    _In_ CMetasequoiaImeArray<CCandidateListItem> *pCandidateList //
)
{
    if ((pCandidateList == nullptr) || (_pIndexRange == nullptr))
    {
        return;
    }

    const UINT candCntInPage = _pIndexRange->Count();
    if (candCntInPage == 0)
    {
        return;
    }

    const UINT candidateCount = pCandidateList->Count();
    const UINT bufferSize = (candidateCount == 0) ? 0 : ((candidateCount - 1) / candCntInPage + 1);
    UINT *puPageIndex = new (std::nothrow) UINT[bufferSize];
    if (puPageIndex != nullptr)
    {
        for (UINT i = 0; i < bufferSize; i++)
        {
            puPageIndex[i] = i * candCntInPage;
        }

        _candidateState.SetPageIndex(puPageIndex, bufferSize);
        delete[] puPageIndex;
    }
    else if (bufferSize == 0)
    {
        _candidateState.SetPageIndex(nullptr, 0);
    }

    _candidateState.SetScrollInfo(candidateCount,
                                  candCntInPage); // nMax:range of max, nPage:number of items in page
}
//+---------------------------------------------------------------------------
//
// _ClearList
//
//----------------------------------------------------------------------------

void CCandidateListUIPresenter::_ClearList()
{
    _candidateState.Clear();
}

//+---------------------------------------------------------------------------
//
// _GetSelectedCandidateString
//
//----------------------------------------------------------------------------

DWORD_PTR CCandidateListUIPresenter::_GetSelectedCandidateString(
    _Outptr_result_maybenull_ const WCHAR **ppwchCandidateString)
{
    return _candidateState.GetSelectedCandidateString(ppwchCandidateString);
}

//+---------------------------------------------------------------------------
//
// _MoveSelection
//
//----------------------------------------------------------------------------

BOOL CCandidateListUIPresenter::_MoveSelection(_In_ int offSet)
{
    BOOL ret = _candidateState.MoveSelection(offSet);
    if (ret)
    {
        if (_isShowMode)
        {
            _NotifyUI();
        }
        else
        {
            _updatedFlags = TF_CLUIE_SELECTION;
            _UpdateUIElement();
        }
    }
    return ret;
}

//+---------------------------------------------------------------------------
//
// _SetSelection
//
//----------------------------------------------------------------------------

BOOL CCandidateListUIPresenter::_SetSelection(_In_ int selectedIndex)
{
    BOOL ret = _candidateState.SetSelection(selectedIndex);
    if (ret)
    {
        if (_isShowMode)
        {
        }
        else
        {
            _updatedFlags = TF_CLUIE_SELECTION | TF_CLUIE_CURRENTPAGE;
            _UpdateUIElement();
        }
    }
    return ret;
}

//+---------------------------------------------------------------------------
//
// _MovePage
//
//----------------------------------------------------------------------------

BOOL CCandidateListUIPresenter::_MovePage(_In_ int offSet)
{
    BOOL ret = _candidateState.MovePage(offSet);
    if (ret)
    {
        if (_isShowMode)
        {
            _NotifyUI();
        }
        else
        {
            _updatedFlags = TF_CLUIE_SELECTION | TF_CLUIE_CURRENTPAGE;
            _UpdateUIElement();
        }
    }
    return ret;
}

//+---------------------------------------------------------------------------
//
// _MoveWindowToTextExt
//
//----------------------------------------------------------------------------

void CCandidateListUIPresenter::_MoveWindowToTextExt()
{
    RECT rc;

    if (FAILED(_GetTextExt(&rc)))
    {
        return;
    }

    Global::Point[0] = rc.left * Global::DpiScale;
    Global::Point[1] = rc.bottom * Global::DpiScale;
}
//+---------------------------------------------------------------------------
//
// _LayoutChangeNotification
//
//----------------------------------------------------------------------------

VOID CCandidateListUIPresenter::_LayoutChangeNotification(_In_ RECT *lpRect)
{
    lpRect;
    PerfTimer timer;
    MoveCandidateUiSession();
}

//+---------------------------------------------------------------------------
//
// _LayoutDestroyNotification
//
//----------------------------------------------------------------------------

VOID CCandidateListUIPresenter::_LayoutDestroyNotification()
{
    PerfTimer timer;
    if (_asyncCleanupPending)
    {
        return;
    }


    EndUIElement();
    EndCandidateUiSession();
    _candidateState.Clear();
    _candidateWindowVisible = FALSE;
    _EndLayout();

}

//+---------------------------------------------------------------------------
//
// _CandidateChangeNotifiction
//
//----------------------------------------------------------------------------

HRESULT CCandidateListUIPresenter::_CandidateChangeNotification(_In_ enum CANDWND_ACTION action)
{
    HRESULT hr = E_FAIL;

    TfClientId tfClientId = _pTextService->_GetClientId();
    ITfThreadMgr *pThreadMgr = nullptr;
    ITfDocumentMgr *pDocumentMgr = nullptr;
    ITfContext *pContext = nullptr;

    _KEYSTROKE_STATE KeyState;
    KeyState.Category = _Category;
    KeyState.Function = FUNCTION_FINALIZE_CANDIDATELIST;

    if (CAND_ITEM_SELECT != action)
    {
        goto Exit;
    }

    pThreadMgr = _pTextService->_GetThreadMgr();
    if (nullptr == pThreadMgr)
    {
        goto Exit;
    }

    hr = pThreadMgr->GetFocus(&pDocumentMgr);
    if (FAILED(hr))
    {
        goto Exit;
    }

    hr = pDocumentMgr->GetTop(&pContext);
    if (FAILED(hr))
    {
        pDocumentMgr->Release();
        goto Exit;
    }

    CKeyHandlerEditSession *pEditSession =
        new (std::nothrow) CKeyHandlerEditSession(_pTextService, pContext, 0, 0, KeyState);
    if (nullptr != pEditSession)
    {
        HRESULT hrSession = S_OK;
        hr = pContext->RequestEditSession(tfClientId, pEditSession, TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
        if (hrSession == TF_E_SYNCHRONOUS || hrSession == TS_E_READONLY)
        {
            hr = pContext->RequestEditSession(tfClientId, pEditSession, TF_ES_ASYNC | TF_ES_READWRITE, &hrSession);
        }
        pEditSession->Release();
    }

    pContext->Release();
    pDocumentMgr->Release();

Exit:
    return hr;
}

//+---------------------------------------------------------------------------
//
// _CandWndCallback
//
//----------------------------------------------------------------------------

HRESULT CCandidateListUIPresenter::_UpdateUIElement()
{
    HRESULT hr = S_OK;

    ITfThreadMgr *pThreadMgr = _pTextService->_GetThreadMgr();
    if (nullptr == pThreadMgr)
    {
        return S_OK;
    }

    ITfUIElementMgr *pUIElementMgr = nullptr;

    hr = pThreadMgr->QueryInterface(IID_ITfUIElementMgr, (void **)&pUIElementMgr);
    if (hr == S_OK)
    {
        pUIElementMgr->UpdateUIElement(_uiElementId);
        pUIElementMgr->Release();
    }

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// OnSetThreadFocus
//
//----------------------------------------------------------------------------

HRESULT CCandidateListUIPresenter::OnSetThreadFocus()
{
    if (_isShowMode)
    {
        Show(TRUE);
    }
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// OnKillThreadFocus
//
//----------------------------------------------------------------------------

HRESULT CCandidateListUIPresenter::OnKillThreadFocus()
{
    if (_isShowMode)
    {
        Show(FALSE);
    }
    return S_OK;
}

void CCandidateListUIPresenter::AdviseUIChangedByArrowKey(_In_ KEYSTROKE_FUNCTION arrowKey)
{
    switch (arrowKey)
    {
    case FUNCTION_MOVE_UP: {
        _MoveSelection(MOVEUP_ONE);
        break;
    }
    case FUNCTION_MOVE_DOWN: {
        _MoveSelection(MOVEDOWN_ONE);
        break;
    }
    case FUNCTION_MOVE_PAGE_UP: {
        // Page prev
        _MovePage(MOVEUP_ONE);
        break;
    }
    case FUNCTION_MOVE_PAGE_DOWN: {
        // Page next
        _MovePage(MOVEDOWN_ONE);
        break;
    }
    case FUNCTION_MOVE_PAGE_TOP: {
        _SetSelection(MOVETO_TOP);
        break;
    }
    case FUNCTION_MOVE_PAGE_BOTTOM: {
        _SetSelection(MOVETO_BOTTOM);
        break;
    }
    default:
        break;
    }
}

HRESULT CCandidateListUIPresenter::BeginUIElement()
{
    PerfTimer timer;
    HRESULT hr = S_OK;

    ITfThreadMgr *pThreadMgr = _pTextService->_GetThreadMgr();
    if (nullptr == pThreadMgr)
    {
        hr = E_FAIL;
        goto Exit;
    }

    ITfUIElementMgr *pUIElementMgr = nullptr;
    PerfTimer queryUiMgrTimer;
    hr = pThreadMgr->QueryInterface(IID_ITfUIElementMgr, (void **)&pUIElementMgr);
    double queryUiMgrElapsedMs = queryUiMgrTimer.ElapsedMs();
    if (hr == S_OK)
    {
        PerfTimer beginUiTimer;
        pUIElementMgr->BeginUIElement(this, &_isShowMode, &_uiElementId);
        double beginUiElapsedMs = beginUiTimer.ElapsedMs();
        pUIElementMgr->Release();
    }

Exit:
    return hr;
}

HRESULT CCandidateListUIPresenter::EndUIElement()
{
    PerfTimer timer;
    HRESULT hr = S_OK;
    const DWORD currentUiElementId = _uiElementId;

    ITfThreadMgr *pThreadMgr = _pTextService->_GetThreadMgr();
    if ((nullptr == pThreadMgr) || (-1 == _uiElementId))
    {
        hr = E_FAIL;
        goto Exit;
    }

    ITfUIElementMgr *pUIElementMgr = nullptr;
    PerfTimer queryUiMgrTimer;
    hr = pThreadMgr->QueryInterface(IID_ITfUIElementMgr, (void **)&pUIElementMgr);
    double queryUiMgrElapsedMs = queryUiMgrTimer.ElapsedMs();
    if (hr == S_OK)
    {
        PerfTimer endUiTimer;
        pUIElementMgr->EndUIElement(_uiElementId);
        double endUiElapsedMs = endUiTimer.ElapsedMs();
        pUIElementMgr->Release();
        _uiElementId = static_cast<DWORD>(-1);
    }

Exit:
    return hr;
}

void CCandidateListUIPresenter::WriteCandidateUiPayload(_In_ UINT writeFlag)
{
    PerfTimer timer;
    CStringRange keyStringBuffer = _pTextService->GetCompositionProcessorEngine()->GetKeystrokeBuffer();
    std::wstring pinyinString(keyStringBuffer.Get(), keyStringBuffer.GetLength());
    Global::PinyinLength = static_cast<int>(pinyinString.length());

    PerfTimer writeTimer;
    WriteDataToSharedMemory(   //
        Global::Keycode,       //
        Global::wch,           //
        Global::ModifiersDown, //
        Global::Point,         //
        Global::PinyinLength,  //
        Global::PinyinString,  //
        writeFlag              //
    );
}

void CCandidateListUIPresenter::BeginCandidateUiSession()
{
    PerfTimer timer;
    PerfTimer writePayloadTimer;
    WriteCandidateUiPayload(0b111111);
    double writePayloadElapsedMs = writePayloadTimer.ElapsedMs();
    PerfTimer sendEventTimer;
    SendShowCandidateWndEventToUIProcess();
    double sendEventElapsedMs = sendEventTimer.ElapsedMs();
    _candidateUiSessionActive = TRUE;
}

void CCandidateListUIPresenter::UpdateCandidateUiSession()
{
    PerfTimer timer;
    PerfTimer writePayloadTimer;
    WriteCandidateUiPayload(0b111111);
    double writePayloadElapsedMs = writePayloadTimer.ElapsedMs();
    PerfTimer sendEventTimer;
    SendShowCandidateWndEventToUIProcess();
    double sendEventElapsedMs = sendEventTimer.ElapsedMs();
}

void CCandidateListUIPresenter::MoveCandidateUiSession()
{
    PerfTimer timer;
    PerfTimer writePayloadTimer;
    WriteCandidateUiPayload(0b001000);
    double writePayloadElapsedMs = writePayloadTimer.ElapsedMs();
    PerfTimer sendEventTimer;
    SendMoveCandidateWndEventToUIProcess();
    double sendEventElapsedMs = sendEventTimer.ElapsedMs();
}

void CCandidateListUIPresenter::EndCandidateUiSession()
{
    PerfTimer timer;
    if (!_candidateUiSessionActive)
    {
        return;
    }

    PerfTimer sendEventTimer;
    SendHideCandidateWndEventToUIProcess();
    double sendEventElapsedMs = sendEventTimer.ElapsedMs();
    _candidateUiSessionActive = FALSE;
}
