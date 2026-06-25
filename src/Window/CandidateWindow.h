#pragma once

#include "Private.h"
#include "BaseWindow.h"
#include "MetasequoiaIME.h"
#include "MetasequoiaIMEBaseStructure.h"

enum CANDWND_ACTION
{
    CAND_ITEM_SELECT
};

typedef HRESULT (*CANDWNDCALLBACK)(void *pv, enum CANDWND_ACTION action);

class CCandidateWindow : public CBaseWindow
{
  public:
    CCandidateWindow(_In_ CANDWNDCALLBACK pfnCallback, _In_ void *pv, _In_ CCandidateRange *pIndexRange,
                     _In_ BOOL isStoreAppMode, _In_ CMetasequoiaIME *pTextService);
    virtual ~CCandidateWindow();

    BOOL _Create(_In_ UINT wndWidth, _In_opt_ HWND parentWndHandle);
    void _Destroy();

    void _Move(int x, int y);
    void _Show(BOOL isShowWnd);

    VOID _SetTextColor(_In_ COLORREF crColor, _In_ COLORREF crBkColor);
    VOID _SetFillColor(_In_ HBRUSH hBrush);
    LRESULT CALLBACK _WindowProcCallback(_In_ HWND wndHandle, UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam);

    void _AddString(_Inout_ CCandidateListItem *pCandidateItem, _In_ BOOL isAddFindKeyCode);
    void _ClearList();
    UINT _GetCount()
    {
        return _candidateList.Count();
    }
    UINT _GetSelection()
    {
        return _currentSelection;
    }
    void _SetScrollInfo(_In_ int nMax, _In_ int nPage);

    DWORD _GetCandidateString(_In_ int iIndex, _Outptr_result_maybenull_z_ const WCHAR **ppwchCandidateString);
    DWORD _GetSelectedCandidateString(_Outptr_result_maybenull_ const WCHAR **ppwchCandidateString);

    BOOL _MoveSelection(_In_ int offSet, _In_ BOOL isNotify);
    BOOL _SetSelection(_In_ int iPage, _In_ BOOL isNotify);
    void _SetSelection(_In_ int nIndex);
    BOOL _MovePage(_In_ int offSet, _In_ BOOL isNotify);
    BOOL _SetSelectionInPage(int nPos);

    HRESULT _GetPageIndex(UINT *pIndex, _In_ UINT uSize, _Inout_ UINT *puPageCnt);
    HRESULT _SetPageIndex(UINT *pIndex, _In_ UINT uPageCnt);
    HRESULT _GetCurrentPage(_Inout_ UINT *pCurrentPage);
    HRESULT _GetCurrentPage(_Inout_ int *pCurrentPage);

  private:
    BOOL _AdjustPageIndexForSelection();
    HRESULT _CurrentPageHasEmptyItems(_Inout_ BOOL *pfHasEmptyItems);
    HRESULT _AdjustPageIndex(_Inout_ UINT &currentPage, _Inout_ UINT &currentPageIndex);
    void _ResizeWindow();

    friend COLORREF _AdjustTextColor(_In_ COLORREF crColor, _In_ COLORREF crBkColor);

  private:
    UINT _currentSelection;
    CMetasequoiaImeArray<CCandidateListItem> _candidateList;
    CMetasequoiaImeArray<UINT> _PageIndex;
    CMetasequoiaIME *_pTextService;

    COLORREF _crTextColor;
    COLORREF _crBkColor;
    HBRUSH _brshBkColor;

    int _cyRow;
    int _cxTitle;
    UINT _wndWidth;

    CCandidateRange *_pIndexRange;

    CANDWNDCALLBACK _pfnCallback;
    void *_pObj;

    BOOL _dontAdjustOnEmptyItemPage;
    BOOL _isStoreAppMode;
};
