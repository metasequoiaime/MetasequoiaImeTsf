#include "Private.h"
#include "CandidateSessionState.h"
#include <corecrt_wstring.h>
#include <debugapi.h>
#include "fmt/xchar.h"

CCandidateSessionState::CCandidateSessionState(_In_ CCandidateRange *pIndexRange)
{
    _currentSelection = 0;
    _pIndexRange = pIndexRange;
    _dontAdjustOnEmptyItemPage = FALSE;
}

CCandidateSessionState::~CCandidateSessionState()
{
    Clear();
}

void CCandidateSessionState::Clear()
{
    for (UINT index = 0; index < _candidateList.Count(); index++)
    {
        CCandidateListItem *pItemList = _candidateList.GetAt(index);
        delete[] pItemList->_ItemString.Get();
        delete[] pItemList->_FindKeyCode.Get();
    }
    _currentSelection = 0;
    _candidateList.Clear();
    _pageIndex.Clear();
    _dontAdjustOnEmptyItemPage = FALSE;
}

void CCandidateSessionState::AddCandidate(_Inout_ CCandidateListItem *pCandidateItem, _In_ BOOL isAddFindKeyCode)
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
            delete[] pwchString;
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

UINT CCandidateSessionState::GetCount() const
{
    return _candidateList.Count();
}

UINT CCandidateSessionState::GetSelection() const
{
    return _currentSelection;
}

void CCandidateSessionState::SetScrollInfo(_In_ int nMax, _In_ int nPage)
{
    nMax;
    nPage;
}

DWORD CCandidateSessionState::GetCandidateString(_In_ int iIndex,
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

DWORD CCandidateSessionState::GetSelectedCandidateString(
    _Outptr_result_maybenull_ const WCHAR **ppwchCandidateString)
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

BOOL CCandidateSessionState::SetSelectionInPage(_In_ int nPos)
{
    if ((nPos < 0) || (_candidateList.Count() == 0) || (_pageIndex.Count() == 0) || (_pIndexRange == nullptr))
    {
        return FALSE;
    }

    const UINT pageSize = _pIndexRange->Count();
    if (pageSize == 0)
    {
        return FALSE;
    }

    int currentPage = 0;
    if (FAILED(GetCurrentPage(&currentPage)))
    {
        return FALSE;
    }

    const UINT pageStart = *_pageIndex.GetAt(currentPage);
    const UINT selection = pageStart + static_cast<UINT>(nPos);
    if ((selection >= _candidateList.Count()) || (selection >= pageStart + pageSize))
    {
        return FALSE;
    }

    _currentSelection = selection;
    return TRUE;
}

BOOL CCandidateSessionState::MoveSelection(_In_ int offSet)
{
    if (_candidateList.Count() == 0)
    {
        return FALSE;
    }

    if (_currentSelection + offSet >= _candidateList.Count())
    {
        return FALSE;
    }

    _currentSelection += offSet;
    _dontAdjustOnEmptyItemPage = TRUE;
    return TRUE;
}

BOOL CCandidateSessionState::SetSelection(_In_ int selectedIndex)
{
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
    return AdjustPageIndexForSelection();
}

void CCandidateSessionState::SetSelectionSilently(_In_ int nIndex)
{
    _currentSelection = nIndex;
}

BOOL CCandidateSessionState::MovePage(_In_ int offSet)
{
    if (offSet == 0)
    {
        return TRUE;
    }

    if ((_candidateList.Count() == 0) || (_pageIndex.Count() == 0) || (_pIndexRange == nullptr))
    {
        return FALSE;
    }

    const UINT pageSize = _pIndexRange->Count();
    if (pageSize == 0)
    {
        return FALSE;
    }

    int currentPage = 0;
    if (FAILED(GetCurrentPage(&currentPage)))
    {
        return FALSE;
    }

    int newPage = currentPage + offSet;
    if ((newPage < 0) || (newPage >= static_cast<int>(_pageIndex.Count())))
    {
        return FALSE;
    }

    if ((_currentSelection % pageSize) == 0 && _currentSelection == *_pageIndex.GetAt(currentPage))
    {
        _dontAdjustOnEmptyItemPage = TRUE;
    }

    const UINT currentPageStart = *_pageIndex.GetAt(currentPage);
    const UINT newPageStart = *_pageIndex.GetAt(newPage);
    if (newPageStart >= _candidateList.Count())
    {
        return FALSE;
    }

    const UINT selectionOffset = _currentSelection - currentPageStart;
    const UINT maxSelectionInPage = min(pageSize - 1, _candidateList.Count() - 1 - newPageStart);
    _currentSelection = newPageStart + min(selectionOffset, maxSelectionInPage);

    return TRUE;
}

HRESULT CCandidateSessionState::GetPageIndex(UINT *pIndex, _In_ UINT uSize, _Inout_ UINT *puPageCnt)
{
    HRESULT hr = S_OK;

    if (uSize > _pageIndex.Count())
    {
        uSize = _pageIndex.Count();
    }
    else
    {
        hr = S_FALSE;
    }

    if (pIndex)
    {
        for (UINT i = 0; i < uSize; i++)
        {
            *pIndex = *_pageIndex.GetAt(i);
            pIndex++;
        }
    }

    *puPageCnt = _pageIndex.Count();
    return hr;
}

HRESULT CCandidateSessionState::SetPageIndex(UINT *pIndex, _In_ UINT uPageCnt)
{
    _pageIndex.Clear();

    for (UINT i = 0; i < uPageCnt; i++)
    {
        UINT *pLastNewPageIndex = _pageIndex.Append();
        if (pLastNewPageIndex != nullptr)
        {
            *pLastNewPageIndex = *pIndex;
            pIndex++;
        }
    }

    return S_OK;
}

HRESULT CCandidateSessionState::GetCurrentPage(_Inout_ UINT *pCurrentPage)
{
    HRESULT hr = S_OK;

    if (pCurrentPage == nullptr)
    {
        return E_INVALIDARG;
    }

    *pCurrentPage = 0;

    if (_pageIndex.Count() == 0)
    {
        return E_UNEXPECTED;
    }

    if (_pageIndex.Count() == 1)
    {
        return S_OK;
    }

    UINT i = 0;
    for (i = 1; i < _pageIndex.Count(); i++)
    {
        UINT uPageIndex = *_pageIndex.GetAt(i);
        if (uPageIndex > _currentSelection)
        {
            break;
        }
    }

    *pCurrentPage = i - 1;
    return hr;
}

HRESULT CCandidateSessionState::GetCurrentPage(_Inout_ int *pCurrentPage)
{
    if (nullptr == pCurrentPage)
    {
        return E_FAIL;
    }

    UINT needCastCurrentPage = 0;
    HRESULT hr = GetCurrentPage(&needCastCurrentPage);
    if (FAILED(hr))
    {
        return hr;
    }

    *pCurrentPage = static_cast<int>(needCastCurrentPage);
    return S_OK;
}

BOOL CCandidateSessionState::AdjustPageIndexForSelection()
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

    SetPageIndex(pNewPageIndex, newPageCnt);
    delete[] pNewPageIndex;

    return TRUE;
}

HRESULT CCandidateSessionState::CurrentPageHasEmptyItems(_Inout_ BOOL *hasEmptyItems)
{
    int candidateListPageCnt = _pIndexRange->Count();
    UINT currentPage = 0;

    if (FAILED(GetCurrentPage(&currentPage)))
    {
        return S_FALSE;
    }

    if ((currentPage == 0 || currentPage == _pageIndex.Count() - 1) && (_pageIndex.Count() > 0) &&
        (*_pageIndex.GetAt(currentPage) > (UINT)(_candidateList.Count() - candidateListPageCnt)))
    {
        *hasEmptyItems = TRUE;
    }
    else
    {
        *hasEmptyItems = FALSE;
    }

    return S_OK;
}

HRESULT CCandidateSessionState::AdjustPageIndex(_Inout_ UINT &currentPage, _Inout_ UINT &currentPageIndex)
{
    HRESULT hr = E_FAIL;
    UINT candidateListPageCnt = _pIndexRange->Count();

    currentPageIndex = *_pageIndex.GetAt(currentPage);

    BOOL hasEmptyItems = FALSE;
    if (FAILED(CurrentPageHasEmptyItems(&hasEmptyItems)))
    {
        return hr;
    }

    if (FALSE == hasEmptyItems || TRUE == _dontAdjustOnEmptyItemPage)
    {
        return hr;
    }

    UINT tempSelection = _currentSelection;
    UINT candNum = _candidateList.Count();
    UINT pageNum = _pageIndex.Count();

    if ((currentPageIndex > candNum - candidateListPageCnt) && (pageNum > 0) && (currentPage == (pageNum - 1)))
    {
        _currentSelection = candNum - candidateListPageCnt;
        AdjustPageIndexForSelection();
        _currentSelection = tempSelection;

        if (FAILED(GetCurrentPage(&currentPage)))
        {
            return hr;
        }

        currentPageIndex = *_pageIndex.GetAt(currentPage);
    }
    else if ((currentPageIndex < candidateListPageCnt) && (currentPage == 0))
    {
        _currentSelection = 0;
        AdjustPageIndexForSelection();
        _currentSelection = tempSelection;
    }

    _dontAdjustOnEmptyItemPage = FALSE;
    return S_OK;
}
