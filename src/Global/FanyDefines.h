#pragma once
#include <string>
#include <unordered_set>
#include <Windows.h>

namespace Global
{
inline std::wstring ZEN_BROWSER = L"zen.exe";
// inline std::unordered_set<std::wstring> VSCodeSeries = {L"Code.exe", L"Code - Insiders.exe", L"VSCodium.exe"};
// inline bool IsVSCodeLike = false;
inline LONG INVALID_Y = -100000;
inline thread_local float DpiScale = 1.0f;
} // namespace Global

namespace GlobalSettings
{
//
// 支持的 TSF 预编辑格式
//  - raw: 原始按键序列
//  - pinyin: 分词后的拼音序列
//  - cand: 当前高亮的候选词序列
//
namespace TsfPreeditStyle
{
constexpr std::string_view Raw = "raw";
constexpr std::string_view Pinyin = "pinyin";
constexpr std::string_view Cand = "cand";
} // namespace TsfPreeditStyle

inline std::string &tsfPreeditStyleStorage()
{
    static std::string style = std::string(TsfPreeditStyle::Raw); // 默认的原始按键序列
    return style;
}

inline const std::string &getTsfPreeditStyle()
{
    return tsfPreeditStyleStorage();
}

inline void setTsfPreeditStyle(std::string_view newStyle)
{
    tsfPreeditStyleStorage() = std::string(newStyle);
}
} // namespace GlobalSettings

namespace GlobalIme
{
inline thread_local std::wstring word_for_creating_word = L"";
// One-shot override for TSF inline preedit after NeedToCreateWord (pinyin mode).
// Consumed by _HandleCompositionInputWorker, then cleared.
inline thread_local std::wstring pending_create_word_preedit = L"";
}

namespace Global
{
inline thread_local HWND msgWndHandle = nullptr;
}
