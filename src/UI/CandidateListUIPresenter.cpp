#include "FanyDefines.h"
#include "Globals.h"
#include "Private.h"
#include "MetasequoiaIME.h"
#include "CandidateWindow.h"
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
#include "FanyUtils.h"

//////////////////////////////////////////////////////////////////////
//
// CMetasequoiaIME candidate key handler methods
//
//////////////////////////////////////////////////////////////////////

const int MOVEUP_ONE = -1;
const int MOVEDOWN_ONE = 1;
const int MOVETO_TOP = 0;
const int MOVETO_BOTTOM = -1;

//+---------------------------------------------------------------------------
//
// _HandleCandidateFinalize
//
//----------------------------------------------------------------------------

HRESULT CMetasequoiaIME::_HandleCandidateFinalize(TfEditCookie ec, _In_ ITfContext *pContext)
{
    HRESULT hr = S_OK;

    CStringRange keyStrokebuffer = _pCompositionProcessorEngine->GetKeystrokeBuffer();
    DWORD_PTR keystrokeBufLen = keyStrokebuffer.GetLength();
    DWORD_PTR candidateLen = keystrokeBufLen;
    CStringRange candidateString(keyStrokebuffer);

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
        struct FanyImeNamedpipeDataToTsf *receivedData = TryReadDataFromServerPipeWithTimeout();
        if (receivedData->msg_type == Global::DataFromServerMsgType::OutofRange) // Candidate index out of range
        {
            return hr;
        }
        else if (receivedData->msg_type == Global::DataFromServerMsgType::Normal) // 只有正常情况下才会上屏
        {
            GlobalIme::word_for_creating_word = L"";
#ifdef FANY_DEBUG
            OutputDebugString(fmt::format(L"[msime]: create_word, normal???").c_str());
#endif
            candidateString.Set(receivedData->candidate_string, wcslen(receivedData->candidate_string));
            hr = _AddComposingAndChar(ec, pContext, &candidateString);
            if (FAILED(hr))
            {
                return hr;
            }
        }
        /* 处理造词的逻辑 */
        // 先把拼音拿到，将其和上屏的汉字串比较一下，
        //  - 如果分词后发现是纯拼音，
        //  - 并且没有使用辅助码，
        //  - 并且上屏的汉字串的拼音比起实际输入的拼音要短，则认为是造词
        // TSF 端需要做的事情是，将 composingString
        // 替换成造词的状态，即把选中的汉字串替换相应的拼音部分，同时，其他的拼音部分保持不变
        // Server 端自己会判断并处理的
        else if (receivedData->msg_type == Global::DataFromServerMsgType::NeedToCreateWord)
        {
            /* 更新 TSF 的 preedit 的前面的部分为已选定的拼音，并且，去除辅助码部分 */
            std::wstring data = receivedData->candidate_string;
            if (data.find(L',') != std::wstring::npos)
            {
                /* pureFullPinyin 一直都是最初的那个完整的拼音 */
                std::wstring pureFullPinyin = data.substr(0, data.find(L','));
                std::wstring curWord = data.substr(data.find(L',') + 1);
#ifdef FANY_DEBUG
                OutputDebugString(
                    fmt::format(L"[msime]: create_word, pureFullPinyin: {}, curWord: {}\n", pureFullPinyin, curWord)
                        .c_str());
#endif
                GlobalIme::word_for_creating_word = curWord;
                CCompositionProcessorEngine *pCompositionProcessorEngine = nullptr;
                pCompositionProcessorEngine = _pCompositionProcessorEngine;

                //
                // 计算出现在的拼音部分，比如，server 端传过来了 davsai 和 大塚，那么，TSF 端的拼音部分就是 ai
                //
                std::wstring preeditStr =
                    pureFullPinyin.substr(FanyUtils::count_utf8_chars(FanyUtils::wstring_to_string(curWord)) * 2);

                DWORD_PTR vKeyLen = pCompositionProcessorEngine->GetVirtualKeyLength();

                for (DWORD_PTR i = 0; i < vKeyLen; i++)
                {
                    DWORD_PTR curVkeyLen = pCompositionProcessorEngine->GetVirtualKeyLength();
                    if (curVkeyLen)
                    {
                        pCompositionProcessorEngine->RemoveVirtualKey(curVkeyLen - 1);
                    }
                }
                for (DWORD_PTR i = 0; i < preeditStr.length(); i++)
                {
                    pCompositionProcessorEngine->AddVirtualKey(preeditStr[i]);
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
                fmt::format(L"[msime]: create_word: current word part {}\n", GlobalIme::word_for_creating_word)
                    .c_str());
#endif
            return hr;
        }
    }

NoPresenter:

    _HandleComplete(ec, pContext);

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
    BSTR pbstr = nullptr;
    CStringRange candidateString;
    CMetasequoiaImeArray<CCandidateListItem> candidatePhraseList;

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

    BOOL fMakePhraseFromText = _pCompositionProcessorEngine->IsMakePhraseFromText();
    if (fMakePhraseFromText) // NOTICE: always no
    {
        _pCompositionProcessorEngine->GetCandidateStringInConverted(candidateString, &candidatePhraseList);
        LCID locale = _pCompositionProcessorEngine->GetLocale();

        _pCandidateListUIPresenter->RemoveSpecificCandidateFromList(locale, candidatePhraseList, candidateString);
    }

    // We have a candidate list if candidatePhraseList.Cnt is not 0
    // If we are showing reverse conversion, use CCandidateListUIPresenter
    CANDIDATE_MODE tempCandMode = CANDIDATE_NONE;
    CCandidateListUIPresenter *pTempCandListUIPresenter = nullptr;
    if (candidatePhraseList.Count()) // NOTICE: always 0
    {
        tempCandMode = CANDIDATE_WITH_NEXT_COMPOSITION;

        pTempCandListUIPresenter = new (std::nothrow)
            CCandidateListUIPresenter(this, Global::AtomCandidateWindow, CATEGORY_CANDIDATE,
                                      _pCompositionProcessorEngine->GetCandidateListIndexRange(), FALSE);
        if (nullptr == pTempCandListUIPresenter)
        {
            hrReturn = E_OUTOFMEMORY;
            goto Exit;
        }
    }

    // call _Start*Line for CCandidateListUIPresenter or CReadingLine
    // we don't cache the document manager object so get it from pContext.
    ITfDocumentMgr *pDocumentMgr = nullptr;
    HRESULT hrStartCandidateList = E_FAIL;
    if (pContext->GetDocumentMgr(&pDocumentMgr) == S_OK)
    {
        ITfRange *pRange = nullptr;
        if (_pComposition->GetRange(&pRange) == S_OK)
        {
            if (pTempCandListUIPresenter)
            {
                hrStartCandidateList = pTempCandListUIPresenter->_StartCandidateList(
                    _tfClientId, pDocumentMgr, pContext, ec, pRange,
                    _pCompositionProcessorEngine->GetCandidateWindowWidth());
            }

            pRange->Release();
        }
        pDocumentMgr->Release();
    }

    // set up candidate list if it is being shown
    if (SUCCEEDED(hrStartCandidateList)) // NOTICE: always false
    {
        pTempCandListUIPresenter->_SetTextColor(RGB(0, 0x80, 0), GetSysColor(COLOR_WINDOW)); // Text color is green
        pTempCandListUIPresenter->_SetFillColor((HBRUSH)(COLOR_WINDOW + 1)); // Background color is window
        pTempCandListUIPresenter->_SetText(&candidatePhraseList, FALSE);

#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Fany here candidateString = {}", candidateString.Get()).c_str());
#endif

#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Create word here?").c_str());
#endif

        // Add composing character
        hrReturn = _AddComposingAndChar(ec, pContext, &candidateString);

        // close candidate list
        if (_pCandidateListUIPresenter)
        {
            _pCandidateListUIPresenter->_EndCandidateList();
            delete _pCandidateListUIPresenter;
            _pCandidateListUIPresenter = nullptr;

            _candidateMode = CANDIDATE_NONE;
            _isCandidateWithWildcard = FALSE;
        }

        if (hrReturn == S_OK)
        {
            // copy temp candidate
            _pCandidateListUIPresenter = pTempCandListUIPresenter;

            _candidateMode = tempCandMode;
            _isCandidateWithWildcard = FALSE;
        }
    }
    else
    {
        // When VK_SPACE, number(for selecting candidate) comes here
        hrReturn = _HandleCandidateFinalize(ec, pContext);
    }

    if (pbstr)
    {
        SysFreeString(pbstr);
    }

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

//+---------------------------------------------------------------------------
//
// _HandlePhraseFinalize
//
//----------------------------------------------------------------------------

HRESULT CMetasequoiaIME::_HandlePhraseFinalize(TfEditCookie ec, _In_ ITfContext *pContext)
{
    HRESULT hr = S_OK;

    DWORD phraseLen = 0;
    const WCHAR *pPhraseString = nullptr;

    phraseLen = (DWORD)_pCandidateListUIPresenter->_GetSelectedCandidateString(&pPhraseString);

    CStringRange phraseString;
    phraseString.Set(pPhraseString, phraseLen);

    if (phraseLen)
    {
        if ((hr = _AddCharAndFinalize(ec, pContext, &phraseString)) != S_OK)
        {
            return hr;
        }
    }

    _HandleComplete(ec, pContext);

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// _HandlePhraseArrowKey
//
//----------------------------------------------------------------------------

HRESULT CMetasequoiaIME::_HandlePhraseArrowKey(TfEditCookie ec, _In_ ITfContext *pContext,
                                               _In_ KEYSTROKE_FUNCTION keyFunction)
{
    ec;
    pContext;

    _pCandidateListUIPresenter->AdviseUIChangedByArrowKey(keyFunction);

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// _HandlePhraseSelectByNumber
//
//----------------------------------------------------------------------------

HRESULT CMetasequoiaIME::_HandlePhraseSelectByNumber(TfEditCookie ec, _In_ ITfContext *pContext, _In_ UINT uCode)
{
    // isSelectAsNumber starts from 0
    int iSelectAsNumber = _pCompositionProcessorEngine->GetCandidateListIndexRange()->GetIndex(uCode);
    if (iSelectAsNumber == -1)
    {
        return S_FALSE;
    }

    if (_pCandidateListUIPresenter)
    {
        if (_pCandidateListUIPresenter->_SetSelectionInPage(iSelectAsNumber))
        {
            return _HandlePhraseFinalize(ec, pContext);
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

CCandidateListUIPresenter::CCandidateListUIPresenter(_In_ CMetasequoiaIME *pTextService, ATOM atom,
                                                     KEYSTROKE_CATEGORY Category, _In_ CCandidateRange *pIndexRange,
                                                     BOOL hideWindow)
    : CTfTextLayoutSink(pTextService)
{
    _atom = atom;

    _pIndexRange = pIndexRange;

    _parentWndHandle = nullptr;
    _pCandidateWnd = nullptr;

    _Category = Category;

    _updatedFlags = 0;

    _uiElementId = (DWORD)-1;
    _isShowMode = TRUE;       // store return value from BeginUIElement
    _hideWindow = hideWindow; // Hide window flag from [Configuration] CandidateList.Phrase.HideWindow

    _pTextService = pTextService;
    _pTextService->AddRef();

    _refCount = 1;
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
    if (showCandidateWindow)
    {
        ToShowCandidateWindow();
    }
    else
    {
        ToHideCandidateWindow();
    }
    return S_OK;
}

HRESULT CCandidateListUIPresenter::ToShowCandidateWindow()
{
    if (_hideWindow)
    {
        _pCandidateWnd->_Show(FALSE);
    }
    else
    {
        _MoveWindowToTextExt();

        _pCandidateWnd->_Show(TRUE);
    }

    return S_OK;
}

HRESULT CCandidateListUIPresenter::ToHideCandidateWindow()
{
    if (_pCandidateWnd)
    {
        _pCandidateWnd->_Show(FALSE);
    }

    _updatedFlags = TF_CLUIE_SELECTION | TF_CLUIE_CURRENTPAGE;
    _UpdateUIElement();

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::ITfUIElement::IsShown
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::IsShown(BOOL *pIsShow)
{
    *pIsShow = _pCandidateWnd->_IsWindowVisible();
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
    if (_pCandidateWnd)
    {
        *pCandidateCount = _pCandidateWnd->_GetCount();
    }
    else
    {
        *pCandidateCount = 0;
    }
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::GetSelection
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::GetSelection(UINT *pSelectedCandidateIndex)
{
    if (_pCandidateWnd)
    {
        *pSelectedCandidateIndex = _pCandidateWnd->_GetSelection();
    }
    else
    {
        *pSelectedCandidateIndex = 0;
    }
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::GetString
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::GetString(UINT uIndex, BSTR *pbstr)
{
    if (!_pCandidateWnd || (uIndex > _pCandidateWnd->_GetCount()))
    {
        return E_FAIL;
    }

    DWORD candidateLen = 0;
    const WCHAR *pCandidateString = nullptr;

    candidateLen = _pCandidateWnd->_GetCandidateString(uIndex, &pCandidateString);

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
    if (!_pCandidateWnd)
    {
        if (pIndex)
        {
            *pIndex = 0;
        }
        *puPageCnt = 0;
        return S_OK;
    }
    return _pCandidateWnd->_GetPageIndex(pIndex, uSize, puPageCnt);
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::SetPageIndex
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::SetPageIndex(UINT *pIndex, UINT uPageCnt)
{
    if (!_pCandidateWnd)
    {
        return E_FAIL;
    }
    return _pCandidateWnd->_SetPageIndex(pIndex, uPageCnt);
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElement::GetCurrentPage
//
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::GetCurrentPage(UINT *puPage)
{
    if (!_pCandidateWnd)
    {
        *puPage = 0;
        return S_OK;
    }
    return _pCandidateWnd->_GetCurrentPage(puPage);
}

//+---------------------------------------------------------------------------
//
// ITfCandidateListUIElementBehavior::SetSelection
// It is related of the mouse clicking behavior upon the suggestion window
//----------------------------------------------------------------------------

STDAPI CCandidateListUIPresenter::SetSelection(UINT nIndex)
{
    if (_pCandidateWnd)
    {
        _pCandidateWnd->_SetSelection(nIndex);
    }

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

    HRESULT hr = E_FAIL;

    if (FAILED(_StartLayout(pContextDocument, ec, pRangeComposition)))
    {
        goto Exit;
    }

    BeginUIElement();

    hr = MakeCandidateWindow(pContextDocument, wndWidth);
    if (FAILED(hr))
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: MakeCandidateWindow failed\n").c_str());
#endif
        // goto Exit;
    }

    // Show(_isShowMode);

    RECT rcTextExt;
    if (SUCCEEDED(_GetTextExt(&rcTextExt)))
    {
        Global::Point[0] = rcTextExt.left * Global::DpiScale;
        Global::Point[1] = rcTextExt.bottom * Global::DpiScale;
        _LayoutChangeNotification(&rcTextExt);
    }

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
    EndUIElement();

    /* 告诉 Server 端隐藏候选框窗口 */
    DisposeCandidateWindow();

    _EndLayout();
}

void CCandidateListUIPresenter::_NotifyUI()
{
    CStringRange keyStringBuffer = _pTextService->GetCompositionProcessorEngine()->GetKeystrokeBuffer();
    std::wstring pinyinString(keyStringBuffer.Get(), keyStringBuffer.GetLength());
    Global::PinyinLength = pinyinString.length();
    WriteDataToSharedMemory(   //
        Global::Keycode,       //
        Global::wch,           //
        Global::ModifiersDown, //
        Global::Point,         //
        Global::PinyinLength,  //
        Global::PinyinString,  //
        0b111111               //
    );
    SendShowCandidateWndEventToUIProcess();
}

//+---------------------------------------------------------------------------
//
// _SetText
//
//----------------------------------------------------------------------------

void CCandidateListUIPresenter::_SetText(_In_ CMetasequoiaImeArray<CCandidateListItem> *pCandidateList,
                                         BOOL isAddFindKeyCode)
{
    AddCandidateToCandidateListUI(pCandidateList, isAddFindKeyCode);

    SetPageIndexWithScrollInfo(pCandidateList);

    if (_isShowMode)
    {
        // _pCandidateWnd->_InvalidateRect();
        _NotifyUI();
    }
    else
    {
        _updatedFlags =
            TF_CLUIE_COUNT | TF_CLUIE_SELECTION | TF_CLUIE_STRING | TF_CLUIE_PAGEINDEX | TF_CLUIE_CURRENTPAGE;
        _UpdateUIElement();
    }
}

void CCandidateListUIPresenter::AddCandidateToCandidateListUI(     //
    _In_ CMetasequoiaImeArray<CCandidateListItem> *pCandidateList, //
    BOOL isAddFindKeyCode                                          //
)
{
    for (UINT index = 0; index < pCandidateList->Count(); index++)
    {
        _pCandidateWnd->_AddString(pCandidateList->GetAt(index), isAddFindKeyCode);
    }
}

void CCandidateListUIPresenter::SetPageIndexWithScrollInfo(       //
    _In_ CMetasequoiaImeArray<CCandidateListItem> *pCandidateList //
)
{
    UINT candCntInPage = _pIndexRange->Count();
    UINT bufferSize = pCandidateList->Count() / candCntInPage + 1;
    UINT *puPageIndex = new (std::nothrow) UINT[bufferSize];
    if (puPageIndex != nullptr)
    {
        for (UINT i = 0; i < bufferSize; i++)
        {
            puPageIndex[i] = i * candCntInPage;
        }

        _pCandidateWnd->_SetPageIndex(puPageIndex, bufferSize);
        delete[] puPageIndex;
    }
    _pCandidateWnd->_SetScrollInfo(pCandidateList->Count(),
                                   candCntInPage); // nMax:range of max, nPage:number of items in page
}
//+---------------------------------------------------------------------------
//
// _ClearList
//
//----------------------------------------------------------------------------

void CCandidateListUIPresenter::_ClearList()
{
    _pCandidateWnd->_ClearList();
}

//+---------------------------------------------------------------------------
//
// _SetTextColor
// _SetFillColor
//
//----------------------------------------------------------------------------

void CCandidateListUIPresenter::_SetTextColor(COLORREF crColor, COLORREF crBkColor)
{
    _pCandidateWnd->_SetTextColor(crColor, crBkColor);
}

void CCandidateListUIPresenter::_SetFillColor(HBRUSH hBrush)
{
    _pCandidateWnd->_SetFillColor(hBrush);
}

//+---------------------------------------------------------------------------
//
// _GetSelectedCandidateString
//
//----------------------------------------------------------------------------

DWORD_PTR CCandidateListUIPresenter::_GetSelectedCandidateString(
    _Outptr_result_maybenull_ const WCHAR **ppwchCandidateString)
{
    return _pCandidateWnd->_GetSelectedCandidateString(ppwchCandidateString);
}

//+---------------------------------------------------------------------------
//
// _MoveSelection
//
//----------------------------------------------------------------------------

BOOL CCandidateListUIPresenter::_MoveSelection(_In_ int offSet)
{
    BOOL ret = _pCandidateWnd->_MoveSelection(offSet, TRUE);
    if (ret)
    {
        if (_isShowMode)
        {
            // _pCandidateWnd->_InvalidateRect();
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
    BOOL ret = _pCandidateWnd->_SetSelection(selectedIndex, TRUE);
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
    BOOL ret = _pCandidateWnd->_MovePage(offSet, TRUE);
    if (ret)
    {
        if (_isShowMode)
        {
            // _pCandidateWnd->_InvalidateRect();
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

    _pCandidateWnd->_Move(rc.left, rc.bottom);
}
//+---------------------------------------------------------------------------
//
// _LayoutChangeNotification
//
//----------------------------------------------------------------------------

VOID CCandidateListUIPresenter::_LayoutChangeNotification(_In_ RECT *lpRect)
{
    // OutputDebugString(fmt::format(L"[msime]: LayoutChangeNotification\n").c_str());
#ifdef FANY_DEBUG
    // TODO: Log _LayoutChangeNotification firefox cnt: Global::firefox_like_cnt
#endif
    WriteDataToSharedMemory(  //
        Global::Keycode,      //
        Global::wch,          //
        0,                    //
        Global::Point,        //
        Global::PinyinLength, //
        Global::PinyinString, //
        0b001000              //
    );
    SendMoveCandidateWndEventToUIProcess();
}

//+---------------------------------------------------------------------------
//
// _LayoutDestroyNotification
//
//----------------------------------------------------------------------------

VOID CCandidateListUIPresenter::_LayoutDestroyNotification()
{
    _EndCandidateList();
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

// static
HRESULT CCandidateListUIPresenter::_CandWndCallback(_In_ void *pv, _In_ enum CANDWND_ACTION action)
{
    CCandidateListUIPresenter *fakeThis = (CCandidateListUIPresenter *)pv;

    return fakeThis->_CandidateChangeNotification(action);
}

//+---------------------------------------------------------------------------
//
// _UpdateUIElement
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

void CCandidateListUIPresenter::RemoveSpecificCandidateFromList(     //
    _In_ LCID Locale,                                                //
    _Inout_ CMetasequoiaImeArray<CCandidateListItem> &candidateList, //
    _In_ CStringRange &candidateString                               //
)
{
    for (UINT index = 0; index < candidateList.Count();)
    {
        CCandidateListItem *pLI = candidateList.GetAt(index);

        if (CStringRange::Compare(Locale, &candidateString, &pLI->_ItemString) == CSTR_EQUAL)
        {
            candidateList.RemoveAt(index);
            continue;
        }

        index++;
    }
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
    HRESULT hr = S_OK;

    ITfThreadMgr *pThreadMgr = _pTextService->_GetThreadMgr();
    if (nullptr == pThreadMgr)
    {
        hr = E_FAIL;
        goto Exit;
    }

    ITfUIElementMgr *pUIElementMgr = nullptr;
    hr = pThreadMgr->QueryInterface(IID_ITfUIElementMgr, (void **)&pUIElementMgr);
    if (hr == S_OK)
    {
        pUIElementMgr->BeginUIElement(this, &_isShowMode, &_uiElementId);
        pUIElementMgr->Release();
    }

Exit:
    return hr;
}

HRESULT CCandidateListUIPresenter::EndUIElement()
{
    HRESULT hr = S_OK;

    ITfThreadMgr *pThreadMgr = _pTextService->_GetThreadMgr();
    if ((nullptr == pThreadMgr) || (-1 == _uiElementId))
    {
        hr = E_FAIL;
        goto Exit;
    }

    ITfUIElementMgr *pUIElementMgr = nullptr;
    hr = pThreadMgr->QueryInterface(IID_ITfUIElementMgr, (void **)&pUIElementMgr);
    if (hr == S_OK)
    {
        pUIElementMgr->EndUIElement(_uiElementId);
        pUIElementMgr->Release();
    }

Exit:
    return hr;
}

//+---------------------------------------------------------------------------
// MakeCandidateWindow
//
// Create the candidate window
//----------------------------------------------------------------------------

HRESULT CCandidateListUIPresenter::MakeCandidateWindow(_In_ ITfContext *pContextDocument, _In_ UINT wndWidth)
{
    HRESULT hr = S_OK;

    if (nullptr != _pCandidateWnd)
    {
        return hr;
    }

    _pCandidateWnd = new (std::nothrow)
        CCandidateWindow(_CandWndCallback, this, _pIndexRange, _pTextService->_IsStoreAppMode(), _pTextService);

    if (_pCandidateWnd == nullptr)
    {
        hr = E_OUTOFMEMORY;
        goto Exit;
    }

    HWND parentWndHandle = nullptr;
    ITfContextView *pView = nullptr;
    if (SUCCEEDED(pContextDocument->GetActiveView(&pView)))
    {
        // pView->GetWnd(&parentWndHandle);
        if (FAILED(pView->GetWnd(&parentWndHandle)) || (parentWndHandle == nullptr))
        {
            parentWndHandle = GetFocus();
        }
    }

    if (!_pCandidateWnd->_Create(_atom, wndWidth, parentWndHandle))
    {
        hr = E_OUTOFMEMORY;
        goto Exit;
    }

Exit:
    return hr;
}

void CCandidateListUIPresenter::DisposeCandidateWindow()
{
    if (nullptr == _pCandidateWnd)
    {
        return;
    }

    //
    // Hide the global candidate window
    //
    // ShowWindow(Global::MainWindowHandle, SW_HIDE);
    // HWND UIHwnd = FindWindow(L"global_candidate_window", NULL);
    // UINT WM_HIDE_MAIN_WINDOW = RegisterWindowMessage(L"WM_HIDE_MAIN_WINDOW");
    SendHideCandidateWndEventToUIProcess();

    _pCandidateWnd->_Destroy();

    delete _pCandidateWnd;
    _pCandidateWnd = nullptr;
}
