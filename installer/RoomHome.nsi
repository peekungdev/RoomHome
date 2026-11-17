; RoomHome installer: one setup for x64, x86 (32-bit) and ARM64 Windows, in English and Thai.
;   makensis /DVERSION=1.2.0 RoomHome.nsi
;   makensis /DVERSION=1.2.0 /DTESTBUILD /DOUTFILE=<path> RoomHome.nsi   (per-user build for automated tests)
; Setup switches:
;   /S              silent
;   /LANG=en|th     setup and RoomHome language (default: asks; silent installs use the Windows language)
;   /ARCH=x64|x86|arm64   force a build (default: the PC's native architecture)
;   /NOAUTOSTART    do not start RoomHome with Windows
;   /DESKTOP        also create a desktop shortcut
;   /D=<folder>     install folder (must be last)

Unicode true
ManifestDPIAware true

!ifndef VERSION
  !define VERSION "1.2.0"
!endif
!define APP        "RoomHome"
!define PUBLISHER  "RoomHome"
!define APP_KEY    "Software\${APP}"
!define RUN_KEY    "Software\Microsoft\Windows\CurrentVersion\Run"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP}"

!ifdef TESTBUILD
  !define ROOT HKCU
  !define CONTEXT current
  RequestExecutionLevel user
  InstallDir "$LOCALAPPDATA\Programs\${APP}-test"
  OutFile "${OUTFILE}"
!else
  !define ROOT HKLM
  !define CONTEXT all
  RequestExecutionLevel admin
  InstallDir "$PROGRAMFILES64\${APP}"
  OutFile "..\dist\${APP}-${VERSION}-setup.exe"
!endif

Name "${APP} ${VERSION}"
SetCompressor /SOLID lzma
BrandingText "${APP} ${VERSION}"

!include MUI2.nsh
!include x64.nsh
!include LogicLib.nsh
!include FileFunc.nsh
!include Sections.nsh

Var Arch
Var ArchName

!define MUI_ICON   "..\res\RoomHome.ico"
!define MUI_UNICON "..\res\RoomHome.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "RoomHome ${VERSION}"
!define MUI_WELCOMEPAGE_TEXT "$(TxtWelcome)"
!define MUI_COMPONENTSPAGE_SMALLDESC
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "$(TxtRunNow)"
!define MUI_FINISHPAGE_RUN_FUNCTION LaunchApp

; the language dialog: offered every time (an upgrade may want the other language), with the
; last choice preselected
!define MUI_LANGDLL_REGISTRY_ROOT      ${ROOT}
!define MUI_LANGDLL_REGISTRY_KEY       "${APP_KEY}"
!define MUI_LANGDLL_REGISTRY_VALUENAME "InstallerLanguage"
!define MUI_LANGDLL_ALWAYSSHOW
!define MUI_LANGDLL_WINDOWTITLE "RoomHome ${VERSION}"
!define MUI_LANGDLL_INFO "Please select a language.$\r$\nกรุณาเลือกภาษา"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"   ; first = fallback
!insertmacro MUI_LANGUAGE "Thai"
!insertmacro MUI_RESERVEFILE_LANGDLL

; ---- text in both languages
LangString TxtWelcome ${LANG_ENGLISH} "RoomHome turns this PC into a meeting-room home screen: a full-screen clock, calendar and other widgets, and an app dock for Zoom, Microsoft Teams and Google Meet, with Wi-Fi, LAN, volume and power controls.$\r$\n$\r$\nSetup picks the right build for this PC (x64, 32-bit x86 or ARM64). RoomHome will show in English; you can switch to Thai any time in Settings > Language.$\r$\n$\r$\nTo close RoomHome later, use Settings > Exit RoomHome."
; Thai has no spaces between words and the dialog wraps it mid-word, so the lines are broken by hand
LangString TxtWelcome ${LANG_THAI}    "RoomHome เปลี่ยนพีซีเครื่องนี้ให้เป็นหน้าจอหลักของ$\r$\nห้องประชุม: นาฬิกา ปฏิทิน และวิดเจ็ตอื่น ๆ แบบเต็มจอ$\r$\nพร้อมแถบแอปสำหรับ Zoom, Microsoft Teams และ$\r$\nGoogle Meet และปุ่มควบคุม Wi-Fi, LAN, ระดับเสียง$\r$\nและการปิดเครื่อง$\r$\n$\r$\nตัวติดตั้งจะเลือกเวอร์ชันที่เหมาะกับพีซีเครื่องนี้ให้เอง$\r$\n(x64, x86 แบบ 32 บิต หรือ ARM64)$\r$\nRoomHome จะแสดงเป็นภาษาไทย และเปลี่ยนเป็นภาษาอังกฤษ$\r$\nได้ทุกเมื่อที่ การตั้งค่า > ภาษา$\r$\n$\r$\nหากต้องการปิด RoomHome ภายหลัง$\r$\nให้ไปที่ การตั้งค่า > ออกจากโปรแกรม"
LangString TxtRunNow  ${LANG_ENGLISH} "Start RoomHome now"
LangString TxtRunNow  ${LANG_THAI}    "เปิด RoomHome ตอนนี้"
LangString TxtShortcut ${LANG_ENGLISH} "Meeting-room home screen"
LangString TxtShortcut ${LANG_THAI}    "หน้าจอหลักสำหรับห้องประชุม"
LangString NameMain    ${LANG_ENGLISH} "RoomHome (required)"
LangString NameMain    ${LANG_THAI}    "RoomHome (จำเป็น)"
LangString NameAuto    ${LANG_ENGLISH} "Start with Windows"
LangString NameAuto    ${LANG_THAI}    "เปิดพร้อม Windows"
LangString NameDesktop ${LANG_ENGLISH} "Desktop shortcut"
LangString NameDesktop ${LANG_THAI}    "ทางลัดบนเดสก์ท็อป"
LangString DescMain    ${LANG_ENGLISH} "The RoomHome program for this PC's processor."
LangString DescMain    ${LANG_THAI}    "โปรแกรม RoomHome สำหรับหน่วยประมวลผลของพีซีเครื่องนี้"
LangString DescAuto    ${LANG_ENGLISH} "Open RoomHome full screen every time someone signs in. You can switch this off later in RoomHome's Settings."
LangString DescAuto    ${LANG_THAI}    "เปิด RoomHome แบบเต็มจอทุกครั้งที่มีผู้ลงชื่อเข้าใช้ ปิดตัวเลือกนี้ได้ภายหลังในการตั้งค่าของ RoomHome"
LangString DescDesktop ${LANG_ENGLISH} "Put a RoomHome shortcut on the desktop."
LangString DescDesktop ${LANG_THAI}    "วางทางลัด RoomHome ไว้บนเดสก์ท็อป"
LangString MsgArm64    ${LANG_ENGLISH} "The ARM64 build needs ARM64 Windows."
LangString MsgArm64    ${LANG_THAI}    "เวอร์ชัน ARM64 ต้องใช้ Windows แบบ ARM64"
LangString MsgX64      ${LANG_ENGLISH} "The x64 build needs 64-bit Windows."
LangString MsgX64      ${LANG_THAI}    "เวอร์ชัน x64 ต้องใช้ Windows แบบ 64 บิต"
LangString MsgBadArch  ${LANG_ENGLISH} "Unknown build (use /ARCH=x64, x86 or arm64):"
LangString MsgBadArch  ${LANG_THAI}    "ไม่รู้จักเวอร์ชันนี้ (ใช้ /ARCH=x64, x86 หรือ arm64):"

VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "${APP}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "${APP} setup (x64, x86, ARM64)"
VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" ""

!macro StopApp
!ifndef TESTBUILD
  ; ask a running copy to close (it saves its settings), then make sure it is gone
  IfFileExists "$INSTDIR\${APP}.exe" 0 +3
    nsExec::Exec '"$INSTDIR\${APP}.exe" --quit'
    Pop $0
  Sleep 300
  nsExec::Exec '"$SYSDIR\taskkill.exe" /F /IM ${APP}.exe'
  Pop $0
  Sleep 300
!endif
  ; (the per-user test build never closes RoomHome: a real copy may be running on the test PC)
!macroend

Section "$(NameMain)" SecMain
  SectionIn RO
  SetShellVarContext ${CONTEXT}
  !insertmacro StopApp
  ; the "Start with Windows" section below re-adds this only when it is selected
  DeleteRegValue ${ROOT} "${RUN_KEY}" "${APP}"
  SetOutPath "$INSTDIR"
  ${If} $Arch == "arm64"
    File "/oname=${APP}.exe" "..\build\arm64\${APP}.exe"
  ${ElseIf} $Arch == "x86"
    File "/oname=${APP}.exe" "..\build\x86\${APP}.exe"
  ${Else}
    File "/oname=${APP}.exe" "..\build\x64\${APP}.exe"
  ${EndIf}
  File "..\README.txt"
  File "..\README.th.txt"
  File "..\RoomHome.defaults.sample.json"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateShortcut "$SMPROGRAMS\${APP}.lnk" "$INSTDIR\${APP}.exe" "" "$INSTDIR\${APP}.exe" 0 SW_SHOWNORMAL "" "$(TxtShortcut)"

  ; RoomHome's "Auto" language follows the language chosen here (Settings > Language overrides it)
  ${If} $LANGUAGE == ${LANG_THAI}
    WriteRegStr ${ROOT} "${APP_KEY}" "Language" "th"
  ${Else}
    WriteRegStr ${ROOT} "${APP_KEY}" "Language" "en"
  ${EndIf}
  WriteRegStr ${ROOT} "${APP_KEY}" "InstallerLanguage" $LANGUAGE

  WriteRegStr   ${ROOT} "${UNINST_KEY}" "DisplayName"          "${APP} ($ArchName)"
  WriteRegStr   ${ROOT} "${UNINST_KEY}" "DisplayVersion"       "${VERSION}"
  WriteRegStr   ${ROOT} "${UNINST_KEY}" "Publisher"            "${PUBLISHER}"
  WriteRegStr   ${ROOT} "${UNINST_KEY}" "DisplayIcon"          '"$INSTDIR\${APP}.exe",0'
  WriteRegStr   ${ROOT} "${UNINST_KEY}" "InstallLocation"      "$INSTDIR"
  WriteRegStr   ${ROOT} "${UNINST_KEY}" "UninstallString"      '"$INSTDIR\Uninstall.exe"'
  WriteRegStr   ${ROOT} "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegStr   ${ROOT} "${UNINST_KEY}" "Architecture"         "$Arch"
  WriteRegDWORD ${ROOT} "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD ${ROOT} "${UNINST_KEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  WriteRegDWORD ${ROOT} "${UNINST_KEY}" "EstimatedSize" $0
SectionEnd

Section "$(NameAuto)" SecAuto
  WriteRegStr ${ROOT} "${RUN_KEY}" "${APP}" '"$INSTDIR\${APP}.exe" --autostart'
SectionEnd

Section /o "$(NameDesktop)" SecDesktop
  SetShellVarContext ${CONTEXT}
  CreateShortcut "$DESKTOP\${APP}.lnk" "$INSTDIR\${APP}.exe" "" "$INSTDIR\${APP}.exe" 0
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} "$(DescMain)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecAuto} "$(DescAuto)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "$(DescDesktop)"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Function .onInit
  ; the 64-bit registry view first: RoomHome (any build) reads its keys there, and the
  ; language dialog below preselects the choice saved under them
  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}

  ${GetParameters} $R0
  ClearErrors
  ${GetOptions} $R0 "/LANG=" $R1
  ${If} ${Errors}
    !insertmacro MUI_LANGDLL_DISPLAY
  ${ElseIf} $R1 == "th"
    StrCpy $LANGUAGE ${LANG_THAI}
  ${Else}
    StrCpy $LANGUAGE ${LANG_ENGLISH}
  ${EndIf}

  ClearErrors
  ${GetOptions} $R0 "/ARCH=" $R1
  ${If} ${Errors}
    ${If} ${IsNativeARM64}
      StrCpy $Arch "arm64"
    ${ElseIf} ${IsNativeAMD64}
      StrCpy $Arch "x64"
    ${Else}
      StrCpy $Arch "x86"
    ${EndIf}
  ${Else}
    StrCpy $Arch $R1
  ${EndIf}
  ${If} $Arch == "arm64"
    StrCpy $ArchName "ARM64"
    ${IfNot} ${IsNativeARM64}
      MessageBox MB_ICONSTOP "$(MsgArm64)" /SD IDOK
      Abort
    ${EndIf}
  ${ElseIf} $Arch == "x64"
    StrCpy $ArchName "x64"
    ${IfNot} ${RunningX64}
      MessageBox MB_ICONSTOP "$(MsgX64)" /SD IDOK
      Abort
    ${EndIf}
  ${ElseIf} $Arch == "x86"
    StrCpy $ArchName "x86"
!ifndef TESTBUILD
    ; 32-bit build: Program Files (x86) on 64-bit Windows, unless /D= chose a folder
    ${If} $INSTDIR == "$PROGRAMFILES64\${APP}"
      StrCpy $INSTDIR "$PROGRAMFILES32\${APP}"
    ${EndIf}
!endif
  ${Else}
    MessageBox MB_ICONSTOP "$(MsgBadArch) $Arch" /SD IDOK
    Abort
  ${EndIf}

  ClearErrors
  ${GetOptions} $R0 "/NOAUTOSTART" $R1
  ${IfNot} ${Errors}
    !insertmacro UnselectSection ${SecAuto}
  ${EndIf}
  ClearErrors
  ${GetOptions} $R0 "/DESKTOP" $R1
  ${IfNot} ${Errors}
    !insertmacro SelectSection ${SecDesktop}
  ${EndIf}
FunctionEnd

Function LaunchApp
  ; MUI runs this even for /S installs; start unelevated through Explorer
  ${IfNot} ${Silent}
    Exec '"$WINDIR\explorer.exe" "$INSTDIR\${APP}.exe"'
  ${EndIf}
FunctionEnd

Function un.onInit
  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
  !insertmacro MUI_UNGETLANGUAGE
FunctionEnd

Section "Uninstall"
  SetShellVarContext ${CONTEXT}
  !insertmacro StopApp
  DeleteRegValue ${ROOT} "${RUN_KEY}" "${APP}"
  DeleteRegValue HKCU "${RUN_KEY}" "${APP}"
  Delete "$SMPROGRAMS\${APP}.lnk"
  Delete "$DESKTOP\${APP}.lnk"
  Delete "$INSTDIR\${APP}.exe"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\README.th.txt"
  Delete "$INSTDIR\RoomHome.defaults.sample.json"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey ${ROOT} "${UNINST_KEY}"
  DeleteRegKey ${ROOT} "${APP_KEY}"
  ; each user's settings in %APPDATA%\RoomHome are kept on purpose
SectionEnd
