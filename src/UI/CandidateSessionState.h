#pragma once

#include "Private.h"
#include "MetasequoiaIMEBaseStructure.h"

class CCandidateSessionState
{
  public:
    explicit CCandidateSessionState(_In_ CCandidateRange *pIndexRange);
    ~CCandidateSessionState();

    void Clear();
    void AddCandidate(_Inout_ CCandidateListItem *pCandidateItem, _In_ BOOL isAddFindKeyCode);

    UINT GetCount() const;
    UINT GetSelection() const;
    void SetScrollInfo(_In_ int nMax, _In_ int nPage);

    DWORD GetCandidateString(_In_ int iIndex, _Outptr_result_maybenull_z_ const WCHAR **ppwchCandidateString);
    DWORD GetSelectedCandidateString(_Outptr_result_maybenull_ const WCHAR **ppwchCandidateString);

    BOOL MoveSelection(_In_ int offSet);
    BOOL SetSelection(_In_ int selectedIndex);
    void SetSelectionSilently(_In_ int nIndex);
    BOOL MovePage(_In_ int offSet);
    BOOL SetSelectionInPage(_In_ int nPos);

    HRESULT GetPageIndex(UINT *pIndex, _In_ UINT uSize, _Inout_ UINT *puPageCnt);
    HRESULT SetPageIndex(UINT *pIndex, _In_ UINT uPageCnt);
    HRESULT GetCurrentPage(_Inout_ UINT *pCurrentPage);
    HRESULT GetCurrentPage(_Inout_ int *pCurrentPage);

  private:
    BOOL AdjustPageIndexForSelection();
    HRESULT CurrentPageHasEmptyItems(_Inout_ BOOL *hasEmptyItems);
    HRESULT AdjustPageIndex(_Inout_ UINT &currentPage, _Inout_ UINT &currentPageIndex);

  private:
    UINT _currentSelection;
    CMetasequoiaImeArray<CCandidateListItem> _candidateList;
    CMetasequoiaImeArray<UINT> _pageIndex;
    CCandidateRange *_pIndexRange;
    BOOL _dontAdjustOnEmptyItemPage;
};
