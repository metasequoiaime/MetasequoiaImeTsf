#pragma once

#include "Private.h"
#include "MetasequoiaIME.h"
#include "CandidateSessionState.h"
#include "CompositionProcessorEngine.h"
#include "MetasequoiaIMEBaseStructure.h"
#include "KeyHandlerEditSession.h"
#include "TfTextLayoutSink.h"

class CReadingLine;

enum CANDWND_ACTION
{
    CAND_ITEM_SELECT
};

//+---------------------------------------------------------------------------
//
// CCandidateListUIPresenter
//
// ITfCandidateListUIElement / ITfIntegratableCandidateListUIElement is used for
// UILess mode support
// ITfCandidateListUIElementBehavior sends the Selection behavior message to
// 3rd party IME.
//----------------------------------------------------------------------------

class CCandidateListUIPresenter : public CTfTextLayoutSink,
                                  public ITfCandidateListUIElementBehavior,
                                  public ITfIntegratableCandidateListUIElement
{
  public:
    CCandidateListUIPresenter(_In_ CMetasequoiaIME *pTextService, KEYSTROKE_CATEGORY Category,
                              _In_ CCandidateRange *pIndexRange, BOOL hideWindow);
    virtual ~CCandidateListUIPresenter();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, _Outptr_ void **ppvObj);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    // ITfUIElement
    STDMETHODIMP GetDescription(BSTR *pbstr);
    STDMETHODIMP GetGUID(GUID *pguid);
    STDMETHODIMP Show(BOOL showCandidateWindow);
    STDMETHODIMP IsShown(BOOL *pIsShow);

    // ITfCandidateListUIElement
    STDMETHODIMP GetUpdatedFlags(DWORD *pdwFlags);
    STDMETHODIMP GetDocumentMgr(ITfDocumentMgr **ppdim);
    STDMETHODIMP GetCount(UINT *pCandidateCount);
    STDMETHODIMP GetSelection(UINT *pSelectedCandidateIndex);
    STDMETHODIMP GetString(UINT uIndex, BSTR *pbstr);
    STDMETHODIMP GetPageIndex(UINT *pIndex, UINT uSize, UINT *puPageCnt);
    STDMETHODIMP SetPageIndex(UINT *pIndex, UINT uPageCnt);
    STDMETHODIMP GetCurrentPage(UINT *puPage);

    // ITfCandidateListUIElementBehavior methods
    STDMETHODIMP SetSelection(UINT nIndex);
    STDMETHODIMP Finalize(void);
    STDMETHODIMP Abort(void);

    // ITfIntegratableCandidateListUIElement
    STDMETHODIMP SetIntegrationStyle(GUID guidIntegrationStyle);
    STDMETHODIMP GetSelectionStyle(_Out_ TfIntegratableCandidateListSelectionStyle *ptfSelectionStyle);
    STDMETHODIMP OnKeyDown(_In_ WPARAM wParam, _In_ LPARAM lParam, _Out_ BOOL *pIsEaten);
    STDMETHODIMP ShowCandidateNumbers(_Out_ BOOL *pIsShow);
    STDMETHODIMP FinalizeExactCompositionString();

    virtual HRESULT _StartCandidateList(TfClientId tfClientId, _In_ ITfDocumentMgr *pDocumentMgr,
                                        _In_ ITfContext *pContextDocument, TfEditCookie ec,
                                        _In_ ITfRange *pRangeComposition, UINT wndWidth);
    void _EndCandidateList();
    void _PrepareForAsyncCleanup();
    BOOL _IsAsyncCleanupPending() const
    {
        return _asyncCleanupPending;
    }

    void _NotifyUI();
    void _SetText(_In_ CMetasequoiaImeArray<CCandidateListItem> *pCandidateList, BOOL isAddFindKeyCode);
    void _ClearList();
    UINT _GetCount() const
    {
        return _candidateState.GetCount();
    }

    DWORD_PTR _GetSelectedCandidateString(_Outptr_result_maybenull_ const WCHAR **ppwchCandidateString);
    BOOL _SetSelectionInPage(int nPos)
    {
        return _candidateState.SetSelectionInPage(nPos);
    }

    BOOL _MoveSelection(_In_ int offSet);
    BOOL _SetSelection(_In_ int selectedIndex);
    BOOL _MovePage(_In_ int offSet);

    void _MoveWindowToTextExt();

    // CTfTextLayoutSink
    virtual VOID _LayoutChangeNotification(_In_ RECT *lpRect);
    virtual VOID _LayoutDestroyNotification();

    // Event for ITfThreadFocusSink
    virtual HRESULT OnSetThreadFocus();
    virtual HRESULT OnKillThreadFocus();

    void AdviseUIChangedByArrowKey(_In_ KEYSTROKE_FUNCTION arrowKey);

  private:
    virtual HRESULT CALLBACK _CandidateChangeNotification(_In_ enum CANDWND_ACTION action);

    HRESULT _UpdateUIElement();

    HRESULT BeginUIElement();
    HRESULT EndUIElement();

    void AddCandidateToCandidateListUI(_In_ CMetasequoiaImeArray<CCandidateListItem> *pCandidateList,
                                       BOOL isAddFindKeyCode);

    void SetPageIndexWithScrollInfo(_In_ CMetasequoiaImeArray<CCandidateListItem> *pCandidateList);
    void WriteCandidateUiPayload(_In_ UINT writeFlag);
    void BeginCandidateUiSession();
    void UpdateCandidateUiSession();
    void MoveCandidateUiSession();
    void EndCandidateUiSession();

  protected:
    BOOL _isShowMode;
    BOOL _hideWindow;
    BOOL _candidateWindowVisible;

  private:
    CCandidateSessionState _candidateState;
    CCandidateRange *_pIndexRange;
    KEYSTROKE_CATEGORY _Category;
    DWORD _updatedFlags;
    DWORD _uiElementId;
    CMetasequoiaIME *_pTextService;
    LONG _refCount;
    BOOL _candidateUiSessionActive;
    BOOL _asyncCleanupPending;
};
