// Thai translations. Keys are the exact English strings used in the code; static labels are
// translated where text is laid out (Gfx::Layout), composed messages through Tr() + Format().
#include "common.h"
#include "lang.h"

int g_lang = LANG_EN;

namespace {
struct Pair { const wchar_t* en; const wchar_t* th; };

const Pair kThai[] = {
    // ---- home screen
    { L"Meeting Room", L"ห้องประชุม" },
    { L"Offline", L"ออฟไลน์" },
    { L"No audio device", L"ไม่มีอุปกรณ์เสียง" },
    { L"Mute", L"ปิดเสียง" },
    { L"No Wi-Fi", L"ไม่มี Wi-Fi" },
    { L"Wi-Fi off", L"Wi-Fi ปิดอยู่" },
    { L"Connecting\x2026", L"กำลังเชื่อมต่อ\x2026" },
    { L"Not connected", L"ไม่ได้เชื่อมต่อ" },
    { L"No LAN", L"ไม่มี LAN" },
    { L"Unplugged", L"ไม่ได้เสียบสาย" },
    { L"Connected", L"เชื่อมต่อแล้ว" },
    { L"Good morning", L"สวัสดีตอนเช้า" },
    { L"Good afternoon", L"สวัสดีตอนบ่าย" },
    { L"Good evening", L"สวัสดีตอนเย็น" },
    { L"Tap for today", L"แตะเพื่อกลับไปวันนี้" },
    { L"Choose a time zone", L"เลือกเขตเวลา" },
    { L"Today", L"วันนี้" },
    { L"Tomorrow", L"พรุ่งนี้" },
    { L"Yesterday", L"เมื่อวาน" },
    { L"same time as here", L"เวลาเดียวกับที่นี่" },
    { L"%s%d h", L"%s%d ชม." },
    { L" %d min", L" %d นาที" },
    { L"Choose a photo", L"เลือกรูปภาพ" },
    { L"No pictures in this folder", L"ไม่มีรูปภาพในโฟลเดอร์นี้" },
    { L"Photo not found", L"ไม่พบรูปภาพ" },
    { L"Edit the widget to pick one", L"แก้ไขวิดเจ็ตเพื่อเลือกรูป" },
    { L"Loading\x2026", L"กำลังโหลด\x2026" },
    { L"Can't open this picture", L"เปิดรูปภาพนี้ไม่ได้" },
    { L"Add widget", L"เพิ่มวิดเจ็ต" },
    { L"Tap to edit", L"แตะเพื่อแก้ไข" },
    { L"Add app", L"เพิ่มแอป" },
    { L"Drag to reorder \x00B7 tap a widget or app to change it \x00B7 \x2212 removes it",
      L"ลากเพื่อจัดลำดับ \x00B7 แตะวิดเจ็ตหรือแอปเพื่อแก้ไข \x00B7 \x2212 เพื่อลบออก" },
    { L"Touch the screen to start", L"แตะหน้าจอเพื่อเริ่มใช้งาน" },
    { L"Done", L"เสร็จสิ้น" },
    { L"Couldn't load the wallpaper %s", L"โหลดภาพพื้นหลังไม่ได้: %s" },
    { L"Couldn't save the settings to %s", L"บันทึกการตั้งค่าไปที่ %s ไม่ได้" },
    { L"RoomHome stays open on this screen. To close it, use Settings \x203A Exit RoomHome.",
      L"RoomHome จะเปิดค้างไว้บนหน้าจอนี้ หากต้องการปิด ให้ไปที่ การตั้งค่า \x203A ออกจากโปรแกรม" },
    { L"Couldn't read %s", L"อ่านไฟล์ %s ไม่ได้" },
    { L"Direct2D could not be started on this PC.", L"เริ่ม Direct2D บนพีซีเครื่องนี้ไม่ได้" },

    // ---- widgets
    { L"Analog clock", L"นาฬิกาเข็ม" },
    { L"A clock face with hour, minute and second hands.", L"หน้าปัดนาฬิกาพร้อมเข็มชั่วโมง นาที และวินาที" },
    { L"Digital clock", L"นาฬิกาดิจิทัล" },
    { L"Big hours and minutes, with seconds, a greeting and the date.",
      L"ตัวเลขชั่วโมงและนาทีขนาดใหญ่ พร้อมวินาที คำทักทาย และวันที่" },
    { L"Calendar", L"ปฏิทิน" },
    { L"This month, with today highlighted. Browse with the arrows.", L"ปฏิทินเดือนนี้ เน้นวันนี้ไว้ เลื่อนดูเดือนอื่นด้วยลูกศร" },
    { L"World clock", L"นาฬิกาโลก" },
    { L"The time in another city, for calls across time zones.", L"เวลาของเมืองอื่น สำหรับการประชุมข้ามเขตเวลา" },
    { L"Note", L"โน้ต" },
    { L"A message for everyone in the room: guest Wi-Fi, house rules, who to call.",
      L"ข้อความถึงทุกคนในห้อง เช่น Wi-Fi สำหรับแขก กฎการใช้ห้อง หรือเบอร์ที่ต้องติดต่อ" },
    { L"Photo", L"รูปภาพ" },
    { L"One picture, or a folder of pictures shown as a slideshow.", L"รูปภาพเดียว หรือโฟลเดอร์รูปภาพที่แสดงเป็นสไลด์โชว์" },
    { L"Welcome", L"ยินดีต้อนรับ" },
    { L"Please leave the room as you found it.\r\nNeed help? Call IT on ext. 1234.",
      L"กรุณาจัดห้องให้เรียบร้อยก่อนออก\r\nต้องการความช่วยเหลือ โทรหาฝ่ายไอที ต่อ 1234" },
    { L"London", L"ลอนดอน" },

    // ---- dock presets
    { L"File Explorer", L"ตัวสำรวจไฟล์" },
    { L"Cast screen", L"แคสต์หน้าจอ" },
    { L"Display mode", L"โหมดการแสดงผล" },
    { L"Website", L"เว็บไซต์" },
    { L"Program", L"โปรแกรม" },
    { L"Opens the Zoom app. If Zoom is not installed, Zoom opens on the web in Microsoft Edge.",
      L"เปิดแอป Zoom หากไม่ได้ติดตั้ง จะเปิด Zoom บนเว็บใน Microsoft Edge" },
    { L"Opens the Teams app (new or classic). Without it, Teams opens on the web in Microsoft Edge.",
      L"เปิดแอป Teams (เวอร์ชันใหม่หรือแบบคลาสสิก) หากไม่มีแอป จะเปิด Teams บนเว็บใน Microsoft Edge" },
    { L"Opens meet.google.com in Microsoft Edge.", L"เปิด meet.google.com ใน Microsoft Edge" },
    { L"Opens the Webex app. If it is not installed, Webex opens on the web in Microsoft Edge.",
      L"เปิดแอป Webex หากไม่ได้ติดตั้ง จะเปิด Webex บนเว็บใน Microsoft Edge" },
    { L"Opens a new Microsoft Edge window.", L"เปิดหน้าต่าง Microsoft Edge ใหม่" },
    { L"Opens a new Google Chrome window.", L"เปิดหน้าต่าง Google Chrome ใหม่" },
    { L"Opens This PC, so USB drives are one tap away.", L"เปิด \x201Cพีซีเครื่องนี้\x201D เพื่อเข้าถึงแฟลชไดรฟ์ USB ได้ในแตะเดียว" },
    { L"Opens the Windows Cast panel (Win+K) to send this screen to a wireless display.",
      L"เปิดแผงแคสต์ของ Windows (Win+K) เพื่อส่งภาพหน้าจอนี้ไปยังจอไร้สาย" },
    { L"Opens the Windows Project panel (Win+P): duplicate, extend or second screen only.",
      L"เปิดแผงฉายภาพของ Windows (Win+P): แสดงซ้ำ ขยาย หรือหน้าจอที่สองเท่านั้น" },
    { L"Starts PowerPoint (found through Windows App Paths).", L"เปิด PowerPoint (ค้นหาผ่าน App Paths ของ Windows)" },
    { L"Any web address, opened in Microsoft Edge.", L"ที่อยู่เว็บใดก็ได้ เปิดใน Microsoft Edge" },
    { L"Any program, shortcut or file. Arguments are optional.", L"โปรแกรม ทางลัด หรือไฟล์ใดก็ได้ จะใส่อาร์กิวเมนต์หรือไม่ก็ได้" },

    // ---- Wi-Fi
    { L"Connected to %s", L"เชื่อมต่อกับ %s แล้ว" },
    { L"Connecting to %s\x2026", L"กำลังเชื่อมต่อกับ %s\x2026" },
    { L"Type the Wi-Fi password first.", L"กรุณาพิมพ์รหัสผ่าน Wi-Fi ก่อน" },
    { L"(hidden network)", L"(เครือข่ายที่ซ่อนอยู่)" },
    { L"Saved", L"บันทึกไว้แล้ว" },
    { L"Secured", L"มีรหัสผ่าน" },
    { L"Open", L"ไม่มีรหัสผ่าน" },
    { L" \x00B7 needs Windows sign-in", L" \x00B7 ต้องลงชื่อเข้าใช้ผ่าน Windows" },
    { L"Disconnect", L"ตัดการเชื่อมต่อ" },
    { L"Forget", L"ลืมเครือข่าย" },
    { L"Connect", L"เชื่อมต่อ" },
    { L"Disconnecting\x2026", L"กำลังตัดการเชื่อมต่อ\x2026" },
    { L"Company networks need a Windows sign-in.", L"เครือข่ายองค์กรต้องลงชื่อเข้าใช้ผ่าน Windows" },
    { L"Windows Wi-Fi list", L"รายการ Wi-Fi ของ Windows" },
    { L"Password", L"รหัสผ่าน" },
    { L"Open network: anyone nearby can read the traffic.", L"เครือข่ายไม่มีรหัสผ่าน: คนที่อยู่ใกล้อาจดักอ่านข้อมูลได้" },
    { L"Turning Wi-Fi on\x2026", L"กำลังเปิด Wi-Fi\x2026" },
    { L"Turning Wi-Fi off\x2026", L"กำลังปิด Wi-Fi\x2026" },
    { L"Looking for networks\x2026", L"กำลังค้นหาเครือข่าย\x2026" },
    { L"The Wi-Fi service isn't running", L"บริการ Wi-Fi ไม่ได้ทำงาน" },
    { L"Windows' WLAN AutoConfig service is stopped or not installed.", L"บริการ WLAN AutoConfig ของ Windows หยุดทำงานหรือไม่ได้ติดตั้ง" },
    { L"No Wi-Fi adapter", L"ไม่มีอะแดปเตอร์ Wi-Fi" },
    { L"This PC has no wireless adapter. Use the LAN cable instead.", L"พีซีเครื่องนี้ไม่มีอะแดปเตอร์ไร้สาย กรุณาใช้สาย LAN แทน" },
    { L"Wi-Fi is off", L"Wi-Fi ปิดอยู่" },
    { L"Switch it on to see the networks around this room.", L"เปิด Wi-Fi เพื่อดูเครือข่ายรอบห้องนี้" },
    { L"Turn on Wi-Fi", L"เปิด Wi-Fi" },
    { L"Windows is hiding the network list", L"Windows ซ่อนรายการเครือข่ายไว้" },
    { L"Windows 11 only lets apps list Wi-Fi networks when location access is on. In Settings, open "
      L"Privacy & security \x203A Location and turn on \x201CLet desktop apps access your location\x201D.",
      L"Windows 11 ให้แอปแสดงรายการ Wi-Fi ได้เฉพาะเมื่อเปิดการเข้าถึงตำแหน่งเท่านั้น ไปที่ การตั้งค่า \x203A "
      L"ความเป็นส่วนตัวและความปลอดภัย \x203A ตำแหน่ง แล้วเปิด \x201Cอนุญาตให้แอปเดสก์ท็อปเข้าถึงตำแหน่งของคุณ\x201D" },
    { L"Location settings", L"การตั้งค่าตำแหน่ง" },
    { L"No networks found", L"ไม่พบเครือข่าย" },
    { L"Windows Wi-Fi settings", L"ตั้งค่า Wi-Fi" },
    { L"Couldn't connect to %s: %s", L"เชื่อมต่อกับ %s ไม่ได้: %s" },
    { L"Wi-Fi is not available on this PC.", L"พีซีเครื่องนี้ใช้ Wi-Fi ไม่ได้" },
    { L"This network needs a sign-in RoomHome can't do. Use the Windows Wi-Fi list instead.",
      L"เครือข่ายนี้ต้องลงชื่อเข้าใช้แบบที่ RoomHome ทำไม่ได้ กรุณาใช้รายการ Wi-Fi ของ Windows แทน" },
    { L"A Wi-Fi password has 8 to 63 characters.", L"รหัสผ่าน Wi-Fi ต้องมี 8 ถึง 63 ตัวอักษร" },
    { L"Couldn't save the network: %s", L"บันทึกเครือข่ายไม่ได้: %s" },
    { L"Couldn't connect: %s", L"เชื่อมต่อไม่ได้: %s" },
    { L"Couldn't disconnect: %s", L"ตัดการเชื่อมต่อไม่ได้: %s" },
    { L"Forgot %s", L"ลืมเครือข่าย %s แล้ว" },
    { L"Couldn't forget %s: %s", L"ลืมเครือข่าย %s ไม่ได้: %s" },
    { L"Couldn't switch Wi-Fi on: %s", L"เปิด Wi-Fi ไม่ได้: %s" },
    { L"Couldn't switch Wi-Fi off: %s", L"ปิด Wi-Fi ไม่ได้: %s" },

    // ---- network details
    { L"Network", L"เครือข่าย" },
    { L"Internet is reachable", L"เชื่อมต่ออินเทอร์เน็ตได้" },
    { L"Connected, but no internet", L"เชื่อมต่อแล้ว แต่ไม่มีอินเทอร์เน็ต" },
    { L"Wired (LAN)", L"แบบใช้สาย (LAN)" },
    { L"Status", L"สถานะ" },
    { L"No LAN adapter", L"ไม่มีอะแดปเตอร์ LAN" },
    { L"Cable unplugged", L"ไม่ได้เสียบสาย LAN" },
    { L"Speed", L"ความเร็ว" },
    { L"IP address", L"ที่อยู่ IP" },
    { L"Gateway", L"เกตเวย์" },
    { L"Adapter", L"อะแดปเตอร์" },
    { L"Signal", L"สัญญาณ" },
    { L"Off", L"ปิด" },
    { L"Network settings", L"การตั้งค่าเครือข่าย" },

    // ---- settings
    { L"Settings", L"การตั้งค่า" },
    { L"Language", L"ภาษา" },
    { L"Room name", L"ชื่อห้อง" },
    { L"Title at the top-left", L"ชื่อที่มุมซ้ายบน" },
    { L"Subtitle", L"คำบรรยาย" },
    { L"Blank shows the IP", L"เว้นว่างเพื่อแสดง IP" },
    { L"Clock", L"นาฬิกา" },
    { L"Auto", L"อัตโนมัติ" },
    { L"24-hour", L"24 ชั่วโมง" },
    { L"12-hour", L"12 ชั่วโมง" },
    { L"Week starts", L"วันเริ่มสัปดาห์" },
    { L"Sunday", L"อาทิตย์" },
    { L"Monday", L"จันทร์" },
    { L"Start with Windows", L"เปิดพร้อม Windows" },
    { L"On: opens after sign-in", L"เปิด: เริ่มทำงานหลังลงชื่อเข้าใช้" },
    { L"The all-users start-up entry needs administrator approval to remove.",
      L"การลบรายการเริ่มต้นสำหรับผู้ใช้ทุกคนต้องได้รับอนุมัติจากผู้ดูแลระบบ" },
    { L"Couldn't add RoomHome to start-up.", L"เพิ่ม RoomHome ในรายการเริ่มต้นไม่ได้" },
    { L"RoomHome will start with Windows", L"RoomHome จะเปิดพร้อม Windows" },
    { L"RoomHome won't start with Windows", L"RoomHome จะไม่เปิดพร้อม Windows" },
    { L"Keep screen on", L"เปิดหน้าจอค้างไว้" },
    { L"Stop the display from sleeping", L"ไม่ให้หน้าจอเข้าสู่โหมดสลีป" },
    { L"Ambient clock", L"นาฬิกาพักหน้าจอ" },
    { L"Dim to a clock when idle", L"หรี่เป็นนาฬิกาเมื่อไม่มีการใช้งาน" },
    { L"5 min", L"5 นาที" },
    { L"15 min", L"15 นาที" },
    { L"30 min", L"30 นาที" },
    { L"1 min", L"1 นาที" },
    { L"Admin PIN", L"PIN ผู้ดูแล" },
    { L"Locks settings and dock editing", L"ล็อกการตั้งค่าและการแก้ไขหน้าจอหลัก" },
    { L"None", L"ไม่มี" },
    { L"Appearance", L"รูปลักษณ์" },
    { L"Auto follows Windows", L"อัตโนมัติตาม Windows" },
    { L"Dark", L"มืด" },
    { L"Light", L"สว่าง" },
    { L"Accent", L"สีเน้น" },
    { L"Dark wallpaper", L"พื้นหลัง (มืด)" },
    { L"Light wallpaper", L"พื้นหลัง (สว่าง)" },
    { L"Built-in aurora", L"ออโรราในตัว" },
    { L"Choose\x2026", L"เลือก\x2026" },
    { L"Light wallpaper set", L"ตั้งภาพพื้นหลังโหมดสว่างแล้ว" },
    { L"Dark wallpaper set", L"ตั้งภาพพื้นหลังโหมดมืดแล้ว" },
    { L"Edit home screen", L"แก้ไขหน้าจอหลัก" },
    { L"Exit RoomHome", L"ออกจากโปรแกรม" },
    { L"Settings file: %s", L"ไฟล์การตั้งค่า: %s" },
    { L"The admin PIN needs at least 4 digits, so it was not changed.", L"PIN ผู้ดูแลต้องมีอย่างน้อย 4 หลัก จึงยังไม่ได้เปลี่ยน" },
    { L"Admin PIN removed", L"ลบ PIN ผู้ดูแลแล้ว" },
    { L"Admin PIN set", L"ตั้ง PIN ผู้ดูแลแล้ว" },
    { L"Language changed", L"เปลี่ยนภาษาแล้ว" },

    // ---- power
    { L"Power", L"พลังงาน" },
    { L"Press and hold for 1.5 seconds", L"กดค้างไว้ 1.5 วินาที" },
    { L"Shut down", L"ปิดเครื่อง" },
    { L"Restart", L"รีสตาร์ท" },
    { L"Hold to confirm", L"กดค้างเพื่อยืนยัน" },
    { L"Keep holding\x2026", L"กดค้างไว้\x2026" },
    { L"Test mode: the PC would restart now", L"โหมดทดสอบ: ถ้าใช้งานจริง พีซีจะรีสตาร์ทตอนนี้" },
    { L"Test mode: the PC would shut down now", L"โหมดทดสอบ: ถ้าใช้งานจริง พีซีจะปิดเครื่องตอนนี้" },
    { L"Restarting\x2026", L"กำลังรีสตาร์ท\x2026" },
    { L"Shutting down\x2026", L"กำลังปิดเครื่อง\x2026" },
    { L"Windows refused: %s", L"Windows ปฏิเสธ: %s" },
    { L"Cancel", L"ยกเลิก" },

    // ---- apps
    { L"The dock is full. Remove an app first.", L"แถบแอปเต็มแล้ว กรุณาลบแอปออกก่อน" },
    { L"Added %s to the dock", L"เพิ่ม %s ในแถบแอปแล้ว" },
    { L"Add to the dock", L"เพิ่มลงในแถบแอป" },
    { L"%d of %d places used", L"ใช้ไปแล้ว %d จาก %d ช่อง" },
    { L"Removed %s", L"ลบ %s แล้ว" },
    { L"Undo", L"เลิกทำ" },
    { L"Give the app a name.", L"กรุณาตั้งชื่อแอป" },
    { L"Type a web address.", L"กรุณาพิมพ์ที่อยู่เว็บ" },
    { L"Choose a program, shortcut or file.", L"กรุณาเลือกโปรแกรม ทางลัด หรือไฟล์" },
    { L"Saved %s", L"บันทึก %s แล้ว" },
    { L"Added %s", L"เพิ่ม %s แล้ว" },
    { L"Website \x00B7 opens in Microsoft Edge", L"เว็บไซต์ \x00B7 เปิดใน Microsoft Edge" },
    { L"Program, shortcut or document", L"โปรแกรม ทางลัด หรือเอกสาร" },
    { L"%s launcher", L"ตัวเปิด %s" },
    { L"Edit app", L"แก้ไขแอป" },
    { L"New app", L"แอปใหม่" },
    { L"App name", L"ชื่อแอป" },
    { L"Name", L"ชื่อ" },
    { L"Any web address, opened in a new Microsoft Edge window.", L"ที่อยู่เว็บใดก็ได้ เปิดในหน้าต่าง Microsoft Edge ใหม่" },
    { L"Pick an .exe, a shortcut (.lnk) or a document. Store apps work too: shell:AppsFolder\\<app id>.",
      L"เลือกไฟล์ .exe ทางลัด (.lnk) หรือเอกสาร\nแอปจาก Store ก็ใช้ได้: shell:AppsFolder\\<app id>" },
    { L"Web address", L"ที่อยู่เว็บ" },
    { L"Program, shortcut or file", L"โปรแกรม ทางลัด หรือไฟล์" },
    { L"Browse\x2026", L"เรียกดู\x2026" },
    { L"Arguments (optional)", L"อาร์กิวเมนต์ (ไม่บังคับ)" },
    { L"Icon", L"ไอคอน" },
    { L"App", L"แอป" },
    { L"Colour", L"สี" },
    { L"Remove", L"ลบ" },
    { L"Save", L"บันทึก" },
    { L"Add to dock", L"เพิ่มลงแถบแอป" },

    // ---- widget editor
    { L"There is room for 4 widgets. Remove one first.", L"วางวิดเจ็ตได้สูงสุด 4 ชิ้น กรุณาลบออกก่อน 1 ชิ้น" },
    { L"Add a widget", L"เพิ่มวิดเจ็ต" },
    { L"Widget", L"วิดเจ็ต" },
    { L"Changes show in the preview. Save puts them on the home screen.",
      L"ตัวอย่างจะแสดงการเปลี่ยนแปลงทันที กดบันทึกเพื่อใช้บนหน้าจอหลัก" },
    { L"Second hand", L"เข็มวินาที" },
    { L"Ticks once a second", L"เดินทุกวินาที" },
    { L"Numbers", L"ตัวเลข" },
    { L"1 to 12 around the dial", L"เลข 1 ถึง 12 รอบหน้าปัด" },
    { L"Date", L"วันที่" },
    { L"Weekday and day of the month", L"วันในสัปดาห์และวันที่" },
    { L"Seconds", L"วินาที" },
    { L"Shown next to the minutes", L"แสดงถัดจากนาที" },
    { L"Greeting", L"คำทักทาย" },
    { L"Good morning / afternoon / evening", L"สวัสดีตอนเช้า / บ่าย / เย็น" },
    { L"Full date under the time", L"วันที่แบบเต็มใต้เวลา" },
    { L"Size", L"ขนาด" },
    { L"Normal", L"ปกติ" },
    { L"Large", L"ใหญ่" },
    { L"12- or 24-hour time: Settings \x203A Clock.", L"รูปแบบ 12 หรือ 24 ชั่วโมง: การตั้งค่า \x203A นาฬิกา" },
    { L"Week numbers", L"หมายเลขสัปดาห์" },
    { L"ISO week numbers on the left", L"หมายเลขสัปดาห์แบบ ISO ทางซ้าย" },
    { L"First day of the week: Settings \x203A Week starts.", L"วันเริ่มสัปดาห์: การตั้งค่า \x203A วันเริ่มสัปดาห์" },
    { L"City name", L"ชื่อเมือง" },
    { L"For example: Tokyo", L"ตัวอย่าง: โตเกียว" },
    { L"Digital", L"ดิจิทัล" },
    { L"Analog", L"เข็ม" },
    { L"Time zone", L"เขตเวลา" },
    { L"Search: a city, or UTC+7", L"ค้นหา: ชื่อเมือง หรือ UTC+7" },
    { L"No time zone matches that search.", L"ไม่พบเขตเวลาที่ตรงกับคำค้นหา" },
    { L"Picture", L"รูปภาพ" },
    { L"Choose photo\x2026", L"เลือกรูปภาพ\x2026" },
    { L"Choose folder\x2026", L"เลือกโฟลเดอร์\x2026" },
    { L"No picture chosen yet", L"ยังไม่ได้เลือกรูปภาพ" },
    { L"Slideshow of folder \x201C%s\x201D (%zu pictures)", L"สไลด์โชว์จากโฟลเดอร์ \x201C%s\x201D (%zu รูป)" },
    { L"Picture: %s", L"รูปภาพ: %s" },
    { L"Change every", L"เปลี่ยนทุก" },
    { L"10 s", L"10 วิ" },
    { L"30 s", L"30 วิ" },
    { L"Shuffle", L"สุ่มลำดับ" },
    { L"Mix the order of the pictures", L"สลับลำดับรูปภาพแบบสุ่ม" },
    { L"Shape", L"รูปทรง" },
    { L"Landscape", L"แนวนอน" },
    { L"Square", L"จัตุรัส" },
    { L"Portrait", L"แนวตั้ง" },
    { L"Framing", L"การจัดภาพ" },
    { L"Fill", L"เต็มกรอบ" },
    { L"Fit", L"ทั้งภาพ" },
    { L"Fills the frame, trims edges", L"เต็มกรอบ ตัดขอบบางส่วน" },
    { L"Shows the whole picture", L"แสดงภาพทั้งหมด" },
    { L"Caption (optional)", L"คำบรรยายภาพ (ไม่บังคับ)" },
    { L"For example: Our team, 2026", L"ตัวอย่าง: ทีมของเรา 2569" },
    { L"Title", L"หัวข้อ" },
    { L"Message (Enter starts a new line)", L"ข้อความ (กด Enter เพื่อขึ้นบรรทัดใหม่)" },
    { L"Type the message", L"พิมพ์ข้อความ" },
    { L"Text size", L"ขนาดตัวอักษร" },

    // ---- rename / PIN
    { L"Shown at the top-left of the home screen", L"แสดงที่มุมซ้ายบนของหน้าจอหลัก" },
    { L"Leave empty to show the IP address", L"เว้นว่างไว้เพื่อแสดงที่อยู่ IP" },
    { L"Enter the PIN to continue", L"ใส่ PIN เพื่อดำเนินการต่อ" },
    { L"Wrong PIN, try again", L"PIN ไม่ถูกต้อง ลองอีกครั้ง" },

    // ---- launching
    { L"Opening %s", L"กำลังเปิด %s" },
    { L"%s isn't installed here, so it opened on the web", L"ไม่ได้ติดตั้ง %s ในเครื่องนี้ จึงเปิดบนเว็บแทน" },
    { L"no web address is set", L"ยังไม่ได้ตั้งที่อยู่เว็บ" },
    { L"Google Chrome is not installed", L"ไม่ได้ติดตั้ง Google Chrome" },
    { L"Choose a display to cast to", L"เลือกจอที่จะแคสต์ไป" },
    { L"Windows blocked the shortcut", L"Windows บล็อกคีย์ลัดนี้" },
    { L"Choose a display mode", L"เลือกโหมดการแสดงผล" },
    { L"no program is set", L"ยังไม่ได้ตั้งโปรแกรม" },
    { L"Couldn't open %s", L"เปิด %s ไม่ได้" },
    { L"Couldn't open %s: %s", L"เปิด %s ไม่ได้: %s" },
    { L"Test mode: %s", L"โหมดทดสอบ: %s" },

    // ---- file dialogs
    { L"Images", L"รูปภาพ" },
    { L"All files", L"ไฟล์ทั้งหมด" },
    { L"Programs and shortcuts", L"โปรแกรมและทางลัด" },
    { L"Choose a wallpaper", L"เลือกภาพพื้นหลัง" },
    { L"Choose a program or shortcut", L"เลือกโปรแกรมหรือทางลัด" },
    { L"Choose a folder of pictures", L"เลือกโฟลเดอร์รูปภาพ" },

    // ---- camera
    { L"Camera", L"กล้อง" },
    { L"Check that the room camera works before the meeting", L"ตรวจสอบว่ากล้องของห้องใช้งานได้ก่อนเริ่มประชุม" },
    { L"Looking for cameras\x2026", L"กำลังค้นหากล้อง\x2026" },
    { L"Windows is blocking the camera", L"Windows บล็อกการใช้กล้องอยู่" },
    { L"Turn on camera access in Windows Settings \x203A Privacy & security \x203A Camera, including access for desktop apps.",
      L"เปิดการเข้าถึงกล้องที่ การตั้งค่า Windows \x203A ความเป็นส่วนตัวและความปลอดภัย \x203A กล้อง "
      L"รวมถึงการเข้าถึงสำหรับแอปเดสก์ท็อป" },
    { L"%s is not connected", L"ไม่ได้เชื่อมต่อ %s" },
    { L"Plug it in again, or pick another camera.", L"เสียบกล้องอีกครั้ง หรือเลือกกล้องตัวอื่น" },
    { L"No camera found", L"ไม่พบกล้อง" },
    { L"Plug in the USB camera. It appears here by itself.", L"เสียบกล้อง USB แล้วกล้องจะแสดงที่นี่เอง" },
    { L"%s is turned off", L"%s ถูกปิดใช้งานอยู่" },
    { L"Turn it on again in Windows Settings \x203A Bluetooth & devices \x203A Cameras, or in Device Manager.",
      L"เปิดใช้งานอีกครั้งที่ การตั้งค่า Windows \x203A Bluetooth และอุปกรณ์ \x203A กล้อง หรือใน Device Manager" },
    { L"%s has no driver", L"%s ไม่มีไดรเวอร์" },
    { L"Windows Update can usually install one. Or unplug the camera and plug it in again.",
      L"ปกติ Windows Update จะติดตั้งไดรเวอร์ให้ได้ หรือลองถอดกล้องแล้วเสียบใหม่" },
    { L"%s has a problem (Code\x00A0%d)", L"%s มีปัญหา (รหัส\x00A0%d)" },
    { L"Unplug it and plug it in again. If that doesn't help, reinstall it in Device Manager.",
      L"ถอดแล้วเสียบใหม่ ถ้ายังไม่หาย ให้ติดตั้งใหม่ใน Device Manager" },
    { L"%s is not available", L"ใช้งาน %s ไม่ได้" },
    { L"Unplug it and plug it in again.", L"ถอดแล้วเสียบใหม่" },
    { L"Another app is using the camera", L"มีแอปอื่นใช้กล้องอยู่" },
    { L"Close the other app or meeting, then press Test again.", L"ปิดแอปหรือการประชุมอื่นก่อน แล้วกด ทดสอบอีกครั้ง" },
    { L"The camera sends no picture", L"กล้องไม่ส่งภาพ" },
    { L"Unplug the camera and plug it in again, or try another USB port.", L"ถอดกล้องแล้วเสียบใหม่ หรือลองช่อง USB อื่น" },
    { L"The picture stopped", L"ภาพหยุดไป" },
    { L"The camera was disconnected", L"กล้องถูกถอดออก" },
    { L"Plug it in again. The test starts by itself when it is back.", L"เสียบกล้องอีกครั้ง การทดสอบจะเริ่มเองเมื่อกล้องกลับมา" },
    { L"Camera preview needs the Media Feature Pack", L"การแสดงภาพจากกล้องต้องใช้ Media Feature Pack" },
    { L"This edition of Windows has no video components. Add the Media Feature Pack in Settings \x203A System \x203A Optional features.",
      L"Windows รุ่นนี้ไม่มีส่วนประกอบวิดีโอ ติดตั้ง Media Feature Pack ได้ที่ การตั้งค่า \x203A ระบบ \x203A ฟีเจอร์เสริม" },
    { L"The camera could not start", L"เปิดกล้องไม่ได้" },
    { L"Paused while another app is in front", L"หยุดชั่วคราวขณะที่แอปอื่นอยู่ด้านหน้า" },
    { L"Starting the camera\x2026", L"กำลังเปิดกล้อง\x2026" },
    { L"Working", L"ใช้งานได้" },
    { L"Not working", L"ใช้งานไม่ได้" },
    { L"In use", L"มีแอปอื่นใช้อยู่" },
    { L"Checking\x2026", L"กำลังตรวจสอบ\x2026" },
    { L"The picture is very dark. Is the privacy shutter or lens cover closed?",
      L"ภาพมืดมาก ฝาปิดเลนส์หรือชัตเตอร์กล้องปิดอยู่หรือเปล่า" },
    { L"Mirror image", L"แสดงภาพแบบกระจก" },
    { L"Shows you as in a mirror, like meeting apps do", L"กลับภาพซ้าย-ขวาเหมือนกระจก แบบเดียวกับแอปประชุม" },
    { L"Cameras", L"กล้องที่พบ" },
    { L"Tap the room camera. The light on the home screen follows it.",
      L"แตะเลือกกล้องของห้อง ไฟสถานะบนหน้าจอหลักจะแสดงตามกล้องนี้" },
    { L"USB camera", L"กล้อง USB" },
    { L"Built-in camera", L"กล้องในตัว" },
    { L"Virtual camera", L"กล้องเสมือน" },
    { L"Turned off", L"ปิดใช้งานอยู่" },
    { L"No driver", L"ไม่มีไดรเวอร์" },
    { L"Problem: Code %d", L"มีปัญหา: รหัส %d" },
    { L"and %d more", L"และอีก %d ตัว" },
    { L"Test again", L"ทดสอบอีกครั้ง" },
    { L"Camera settings", L"ตั้งค่ากล้อง" },
    { L"Privacy settings", L"ตั้งค่าความเป็นส่วนตัว" },
};

std::unordered_map<std::wstring, std::wstring>* g_table = nullptr;

const wchar_t* kDayEn[] = { L"Sunday", L"Monday", L"Tuesday", L"Wednesday", L"Thursday", L"Friday", L"Saturday" };
const wchar_t* kDayTh[] = { L"อาทิตย์", L"จันทร์", L"อังคาร", L"พุธ", L"พฤหัสบดี", L"ศุกร์", L"เสาร์" };
const wchar_t* kDayShortEn[] = { L"SUN", L"MON", L"TUE", L"WED", L"THU", L"FRI", L"SAT" };
const wchar_t* kDayShortTh[] = { L"อา", L"จ", L"อ", L"พ", L"พฤ", L"ศ", L"ส" };
const wchar_t* kMonthEn[] = { L"January", L"February", L"March", L"April", L"May", L"June", L"July",
                              L"August", L"September", L"October", L"November", L"December" };
const wchar_t* kMonthTh[] = { L"มกราคม", L"กุมภาพันธ์", L"มีนาคม", L"เมษายน", L"พฤษภาคม", L"มิถุนายน", L"กรกฎาคม",
                              L"สิงหาคม", L"กันยายน", L"ตุลาคม", L"พฤศจิกายน", L"ธันวาคม" };
} // namespace

void LangInit() {
    if (g_table) return;
    g_table = new std::unordered_map<std::wstring, std::wstring>();
    for (const Pair& p : kThai) (*g_table)[p.en] = p.th;
}

int LangDetect(int choice) {
    if (choice == 1) return LANG_EN;
    if (choice == 2) return LANG_TH;
    // auto: the language picked in the installer, else the Windows display language
    wchar_t v[16] = L"";
    DWORD sz = sizeof(v);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"Software\\RoomHome", L"Language", RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, nullptr, v, &sz) == ERROR_SUCCESS ||
        (sz = sizeof(v), RegGetValueW(HKEY_CURRENT_USER, L"Software\\RoomHome", L"Language", RRF_RT_REG_SZ, nullptr, v, &sz) == ERROR_SUCCESS)) {
        if (_wcsicmp(v, L"th") == 0) return LANG_TH;
        if (_wcsicmp(v, L"en") == 0) return LANG_EN;
    }
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_THAI ? LANG_TH : LANG_EN;
}

const std::wstring* TrFind(const std::wstring& en) {
    if (g_lang != LANG_TH || !g_table) return nullptr;
    auto it = g_table->find(en);
    return it == g_table->end() ? nullptr : &it->second;
}

const wchar_t* Tr(const wchar_t* en) {
    if (g_lang != LANG_TH || !g_table || !en) return en;
    auto it = g_table->find(en);
    return it == g_table->end() ? en : it->second.c_str();
}

std::wstring Tr(const std::wstring& en) {
    const std::wstring* t = TrFind(en);
    return t ? *t : en;
}

const wchar_t* DayName(int dow) { return (g_lang == LANG_TH ? kDayTh : kDayEn)[((dow % 7) + 7) % 7]; }
const wchar_t* DayShort(int dow) { return (g_lang == LANG_TH ? kDayShortTh : kDayShortEn)[((dow % 7) + 7) % 7]; }
const wchar_t* MonthName(int m) { return (g_lang == LANG_TH ? kMonthTh : kMonthEn)[std::clamp(m, 1, 12) - 1]; }
int DisplayYear(int y) { return g_lang == LANG_TH ? y + 543 : y; }

std::wstring DateLong(const SYSTEMTIME& t) {
    if (g_lang == LANG_TH) return Format(L"วัน%sที่ %d %s %d", DayName(t.wDayOfWeek), t.wDay, MonthName(t.wMonth), DisplayYear(t.wYear));
    return Format(L"%s, %d %s %d", DayName(t.wDayOfWeek), t.wDay, MonthName(t.wMonth), t.wYear);
}

std::wstring DateDayMonth(const SYSTEMTIME& t, bool longForm) {
    if (g_lang == LANG_TH)
        return longForm ? Format(L"วัน%sที่ %d %s", DayName(t.wDayOfWeek), t.wDay, MonthName(t.wMonth))
                        : Format(L"%s %d %s", DayName(t.wDayOfWeek), t.wDay, MonthName(t.wMonth));
    return Format(longForm ? L"%s, %d %s" : L"%s %d %s", DayName(t.wDayOfWeek), t.wDay, MonthName(t.wMonth));
}

std::wstring DateTag(const SYSTEMTIME& t) {
    if (g_lang == LANG_TH) return Format(L"%s. %d", kDayShortTh[t.wDayOfWeek % 7], t.wDay);
    return Format(L"%s %d", kDayShortEn[t.wDayOfWeek % 7], t.wDay);
}

std::wstring MonthYear(int y, int m) { return Format(L"%s %d", MonthName(m), DisplayYear(y)); }
