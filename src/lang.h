// UI language: English (the source strings) or Thai.
#pragma once
#include "common.h"

enum { LANG_EN = 0, LANG_TH = 1 };
extern int g_lang;

void LangInit();                                     // build the table once, before any thread starts
int LangDetect(int configChoice);                    // 0 auto, 1 English, 2 Thai -> LANG_EN / LANG_TH
const wchar_t* Tr(const wchar_t* en);                // translated text, or en itself
std::wstring Tr(const std::wstring& en);
const std::wstring* TrFind(const std::wstring& en);  // nullptr when there is no translation (or English)

// dates
const wchar_t* DayName(int dow);                     // Thursday / พฤหัสบดี
const wchar_t* DayShort(int dow);                    // MON / จ  (calendar header)
const wchar_t* MonthName(int month);                 // 1..12
int DisplayYear(int year);                           // Thai: Buddhist era (+543)
std::wstring DateLong(const SYSTEMTIME& t);          // Thursday, 13 November 2026 / วันพฤหัสบดีที่ 13 พฤศจิกายน 2569
std::wstring DateDayMonth(const SYSTEMTIME& t, bool longForm);   // without the year
std::wstring DateTag(const SYSTEMTIME& t);           // THU 13 / พฤ. 13
std::wstring MonthYear(int year, int month);         // November 2026 / พฤศจิกายน 2569
