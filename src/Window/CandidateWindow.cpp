#include "Private.h"
#include "BaseWindow.h"
#include <corecrt_wstring.h>
#include <string>
#include "CandidateWindow.h"

COLORREF _AdjustTextColor(_In_ COLORREF crColor, _In_ COLORREF crBkColor);

CCandidateWindow::CCandidateWindow(_In_ CANDWNDCALLBACK pfnCallback, _In_ void *pv, _In_ CCandidateRange *pIndexRange,
                                   _In_ BOOL isStoreAppMode, _In_ CMetasequoiaIME *pTextService)
{
    _currentSelection = 0;

    _SetTextColor(CANDWND_ITEM_COLOR, GetSysColor(COLOR_WINDOW));
    _SetFillColor((HBRUSH)(COLOR_WINDOW + 1));

    _pIndexRange = pIndexRange;
    _pfnCallback = pfnCallback;
    _pObj = pv;
    _cyRow = CANDWND_ROW_WIDTH;
    _cxTitle = 0;
    _wndWidth = 0;
    _dontAdjustOnEmptyItemPage = FALSE;
    _isStoreAppMode = isStoreAppMode;
    _pTextService = pTextService;
}

CCandidateWindow::~CCandidateWindow()
{
    _ClearList();
}

BOOL CCandidateWindow::_Create(_In_ UINT wndWidth, _In_opt_ HWND parentWndHandle)
{
    parentWndHandle;
    _wndWidth = wndWidth;
    _ResizeWindow();
    return TRUE;
}

void CCandidateWindow::_Destroy()
{
    CBaseWindow::_Destroy();
}

void CCandidateWindow::_ResizeWindow()
{
    CBaseWindow::_Resize(0, 0, 20, 10);
}

void CCandidateWindow::_Move(int x, int y)
{
    CBaseWindow::_Move(x, y);
}

void CCandidateWindow::_Show(BOOL isShowWnd)
{
    CBaseWindow::_Show(isShowWnd);
}

VOID CCandidateWindow::_SetTextColor(_In_ COLORREF crColor, _In_ COLORREF crBkColor)
{
    _crTextColor = _AdjustTextColor(crColor, crBkColor);
    _crBkColor = crBkColor;
}

VOID CCandidateWindow::_SetFillColor(_In_ HBRUSH hBrush)
{
    _brshBkColor = hBrush;
}

LRESULT CALLBACK CCandidateWindow::_WindowProcCallback(_In_ HWND wndHandle, UINT uMsg, _In_ WPARAM wParam,
                                                       _In_ LPARAM lParam)
{
    return DefWindowProc(wndHandle, uMsg, wParam, lParam);
}

void CCandidateWindow::_AddString(_Inout_ CCandidateListItem *pCandidateItem, _In_ BOOL isAddFindKeyCode)
{
    DWORD_PTR dwItemString = pCandidateItem->_ItemString.GetLength();
    const WCHAR *pwchString = nullptr;
    if (dwItemString)
    {
        pwchString = new (std::nothrow) WCHAR[dwItemString];
        if (!pwchString)
        {
            return;
        }
        memcpy((void *)pwchString, pCandidateItem->_ItemString.Get(), dwItemString * sizeof(WCHAR));
    }

    DWORD_PTR itemWildcard = pCandidateItem->_FindKeyCode.GetLength();
    const WCHAR *pwchWildcard = nullptr;
    if (itemWildcard && isAddFindKeyCode)
    {
        pwchWildcard = new (std::nothrow) WCHAR[itemWildcard];
        if (!pwchWildcard)
        {
            if (pwchString)
            {
                delete[] pwchString;
            }
            return;
        }
        memcpy((void *)pwchWildcard, pCandidateItem->_FindKeyCode.Get(), itemWildcard * sizeof(WCHAR));
    }

    CCandidateListItem *pLI = _candidateList.Append();
    if (!pLI)
    {
        delete[] pwchString;
        delete[] pwchWildcard;
        return;
    }

    if (pwchString)
    {
        pLI->_ItemString.Set(pwchString, dwItemString);
    }
    if (pwchWildcard)
    {
        pLI->_FindKeyCode.Set(pwchWildcard, itemWildcard);
    }
}

void CCandidateWindow::_ClearList()
{
    for (UINT index = 0; index < _candidateList.Count(); index++)
    {
        CCandidateListItem *pItemList = _candidateList.GetAt(index);
        delete[] pItemList->_ItemString.Get();
        delete[] pItemList->_FindKeyCode.Get();
    }
    _currentSelection = 0;
    _candidateList.Clear();
    _PageIndex.Clear();
}

void CCandidateWindow::_SetScrollInfo(_In_ int nMax, _In_ int nPage)
{
    nMax;
    nPage;
}

DWORD CCandidateWindow::_GetCandidateString(_In_ int iIndex,
                                            _Outptr_result_maybenull_z_ const WCHAR **ppwchCandidateString)
{
    if (iIndex < 0)
    {
        *ppwchCandidateString = nullptr;
        return 0;
    }

    UINT index = static_cast<UINT>(iIndex);
    if (index >= _candidateList.Count())
    {
        *ppwchCandidateString = nullptr;
        return 0;
    }

    CCandidateListItem *pItemList = _candidateList.GetAt(iIndex);
    if (ppwchCandidateString)
    {
        *ppwchCandidateString = pItemList->_ItemString.Get();
    }
    return (DWORD)pItemList->_ItemString.GetLength();
}

DWORD CCandidateWindow::_GetSelectedCandidateString(_Outptr_result_maybenull_ const WCHAR **ppwchCandidateString)
{
    if (_currentSelection >= _candidateList.Count())
    {
        *ppwchCandidateString = nullptr;
        return 0;
    }

    CCandidateListItem *pItemList = _candidateList.GetAt(_currentSelection);
    if (ppwchCandidateString)
    {
        *ppwchCandidateString = pItemList->_ItemString.Get();
    }
    return (DWORD)pItemList->_ItemString.GetLength();
}

BOOL CCandidateWindow::_SetSelectionInPage(int nPos)
{
    if (nPos < 0)
    {
        return FALSE;
    }

    UINT pos = static_cast<UINT>(nPos);
    if (pos >= _candidateList.Count())
    {
        return FALSE;
    }

    int currentPage = 0;
    if (FAILED(_GetCurrentPage(&currentPage)))
    {
        return FALSE;
    }

    _currentSelection = *_PageIndex.GetAt(currentPage) + nPos;
    return TRUE;
}

BOOL CCandidateWindow::_MoveSelection(_In_ int offSet, _In_ BOOL isNotify)
{
    isNotify;
    if (_currentSelection + offSet >= _candidateList.Count())
    {
        return FALSE;
    }

    _currentSelection += offSet;
    _dontAdjustOnEmptyItemPage = TRUE;
    return TRUE;
}

BOOL CCandidateWindow::_SetSelection(_In_ int selectedIndex, _In_ BOOL isNotify)
{
    isNotify;
    if (selectedIndex == -1)
    {
        selectedIndex = _candidateList.Count() - 1;
    }

    if (selectedIndex < 0)
    {
        return FALSE;
    }

    int candCnt = static_cast<int>(_candidateList.Count());
    if (selectedIndex >= candCnt)
    {
        return FALSE;
    }

    _currentSelection = static_cast<UINT>(selectedIndex);
    return _AdjustPageIndexForSelection();
}

void CCandidateWindow::_SetSelection(_In_ int nIndex)
{
    _currentSelection = nIndex;
}

BOOL CCandidateWindow::_MovePage(_In_ int offSet, _In_ BOOL isNotify)
{
    isNotify;
    if (offSet == 0)
    {
        return TRUE;
    }

    int currentPage = 0;
    if (FAILED(_GetCurrentPage(&currentPage)))
    {
        return FALSE;
    }

    int newPage = currentPage + offSet;
    if ((newPage < 0) || (newPage >= static_cast<int>(_PageIndex.Count())))
    {
        return FALSE;
    }

    if (_currentSelection % _pIndexRange->Count() == 0 && _currentSelection == *_PageIndex.GetAt(currentPage))
    {
        _dontAdjustOnEmptyItemPage = TRUE;
    }

    int selectionOffset = _currentSelection - *_PageIndex.GetAt(currentPage);
    _currentSelection = *_PageIndex.GetAt(newPage) + selectionOffset;
    _currentSelection = _candidateList.Count() > _currentSelection ? _currentSelection : _candidateList.Count() - 1;

    return TRUE;
}

HRESULT CCandidateWindow::_GetPageIndex(UINT *pIndex, _In_ UINT uSize, _Inout_ UINT *puPageCnt)
{
    HRESULT hr = S_OK;

    if (uSize > _PageIndex.Count())
    {
        uSize = _PageIndex.Count();
    }
    else
    {
        hr = S_FALSE;
    }

    if (pIndex)
    {
        for (UINT i = 0; i < uSize; i++)
        {
            *pIndex = *_PageIndex.GetAt(i);
            pIndex++;
        }
    }

    *puPageCnt = _PageIndex.Count();
    return hr;
}

HRESULT CCandidateWindow::_SetPageIndex(UINT *pIndex, _In_ UINT uPageCnt)
{
    _PageIndex.Clear();

    for (UINT i = 0; i < uPageCnt; i++)
    {
        UINT *pLastNewPageIndex = _PageIndex.Append();
        if (pLastNewPageIndex != nullptr)
        {
            *pLastNewPageIndex = *pIndex;
            pIndex++;
        }
    }

    return S_OK;
}

HRESULT CCandidateWindow::_GetCurrentPage(_Inout_ UINT *pCurrentPage)
{
    HRESULT hr = S_OK;

    if (pCurrentPage == nullptr)
    {
        return E_INVALIDARG;
    }

    *pCurrentPage = 0;

    if (_PageIndex.Count() == 0)
    {
        return E_UNEXPECTED;
    }

    if (_PageIndex.Count() == 1)
    {
        return S_OK;
    }

    UINT i = 0;
    for (i = 1; i < _PageIndex.Count(); i++)
    {
        UINT uPageIndex = *_PageIndex.GetAt(i);
        if (uPageIndex > _currentSelection)
        {
            break;
        }
    }

    *pCurrentPage = i - 1;
    return hr;
}

HRESULT CCandidateWindow::_GetCurrentPage(_Inout_ int *pCurrentPage)
{
    if (nullptr == pCurrentPage)
    {
        return E_FAIL;
    }

    UINT needCastCurrentPage = 0;
    HRESULT hr = _GetCurrentPage(&needCastCurrentPage);
    if (FAILED(hr))
    {
        return hr;
    }

    *pCurrentPage = static_cast<int>(needCastCurrentPage);
    return S_OK;
}

BOOL CCandidateWindow::_AdjustPageIndexForSelection()
{
    UINT candidateListPageCnt = _pIndexRange->Count();
    UINT newPageCnt = 0;

    if (_candidateList.Count() < candidateListPageCnt)
    {
        return TRUE;
    }

    BOOL isBefore = _currentSelection;
    BOOL isAfter = _candidateList.Count() > _currentSelection + candidateListPageCnt;

    if (!isBefore && !isAfter)
    {
        newPageCnt = 1;
    }
    else if (!isBefore && isAfter)
    {
        newPageCnt = (_candidateList.Count() - 1) / candidateListPageCnt + 1;
    }
    else if (isBefore && !isAfter)
    {
        newPageCnt = 2 + (_currentSelection - 1) / candidateListPageCnt;
    }
    else
    {
        newPageCnt = (_candidateList.Count() - 2) / candidateListPageCnt + 2;
    }

    UINT *pNewPageIndex = new (std::nothrow) UINT[newPageCnt];
    if (pNewPageIndex == nullptr)
    {
        return FALSE;
    }

    pNewPageIndex[0] = 0;
    UINT firstPage = _currentSelection % candidateListPageCnt;
    if (firstPage && newPageCnt > 1)
    {
        pNewPageIndex[1] = firstPage;
    }

    for (UINT i = firstPage ? 2 : 1; i < newPageCnt; ++i)
    {
        pNewPageIndex[i] = pNewPageIndex[i - 1] + candidateListPageCnt;
    }

    _SetPageIndex(pNewPageIndex, newPageCnt);
    delete[] pNewPageIndex;

    return TRUE;
}

COLORREF _AdjustTextColor(_In_ COLORREF crColor, _In_ COLORREF crBkColor)
{
    if (!Global::IsTooSimilar(crColor, crBkColor))
    {
        return crColor;
    }

    return crColor ^ RGB(255, 255, 255);
}

HRESULT CCandidateWindow::_CurrentPageHasEmptyItems(_Inout_ BOOL *hasEmptyItems)
{
    int candidateListPageCnt = _pIndexRange->Count();
    UINT currentPage = 0;

    if (FAILED(_GetCurrentPage(&currentPage)))
    {
        return S_FALSE;
    }

    if ((currentPage == 0 || currentPage == _PageIndex.Count() - 1) && (_PageIndex.Count() > 0) &&
        (*_PageIndex.GetAt(currentPage) > (UINT)(_candidateList.Count() - candidateListPageCnt)))
    {
        *hasEmptyItems = TRUE;
    }
    else
    {
        *hasEmptyItems = FALSE;
    }

    return S_OK;
}

HRESULT CCandidateWindow::_AdjustPageIndex(_Inout_ UINT &currentPage, _Inout_ UINT &currentPageIndex)
{
    HRESULT hr = E_FAIL;
    UINT candidateListPageCnt = _pIndexRange->Count();

    currentPageIndex = *_PageIndex.GetAt(currentPage);

    BOOL hasEmptyItems = FALSE;
    if (FAILED(_CurrentPageHasEmptyItems(&hasEmptyItems)))
    {
        return hr;
    }

    if (FALSE == hasEmptyItems || TRUE == _dontAdjustOnEmptyItemPage)
    {
        return hr;
    }

    UINT tempSelection = _currentSelection;
    UINT candNum = _candidateList.Count();
    UINT pageNum = _PageIndex.Count();

    if ((currentPageIndex > candNum - candidateListPageCnt) && (pageNum > 0) && (currentPage == (pageNum - 1)))
    {
        _currentSelection = candNum - candidateListPageCnt;
        _AdjustPageIndexForSelection();
        _currentSelection = tempSelection;

        if (FAILED(_GetCurrentPage(&currentPage)))
        {
            return hr;
        }

        currentPageIndex = *_PageIndex.GetAt(currentPage);
    }
    else if ((currentPageIndex < candidateListPageCnt) && (currentPage == 0))
    {
        _currentSelection = 0;
        _AdjustPageIndexForSelection();
        _currentSelection = tempSelection;
    }

    _dontAdjustOnEmptyItemPage = FALSE;
    return S_OK;
}
