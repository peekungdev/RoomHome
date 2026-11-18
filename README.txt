RoomHome 1.3.0 - meeting-room home screen for Windows 11 (x64, x86, ARM64)
==========================================================================
(ภาษาไทย: README.th.txt)

RoomHome fills the screen with a clock, a calendar and a dock of meeting apps, so a
meeting-room PC always shows something useful. Meeting apps open on top of it; when they
close, RoomHome is back. It works with touch, mouse, keyboard and presentation remotes
(arrow keys + Enter).

Language: English or Thai (ภาษาไทย), switched in Settings > Language. "Auto" uses
the language chosen in setup, or else the Windows display language. In Thai, dates use
Thai month and day names and Buddhist-era years (2569).

On screen
---------
Top-left       Room name and subtitle. Tap it to rename (the subtitle shows the IP address
               when left empty).
Top bar        Wi-Fi (tap: pick a network, type the password, connect / forget, Wi-Fi on/off)
               LAN (tap: speed, IP, gateway, DNS, MAC for wired and Wi-Fi). The dot shows
               internet: green = online, amber = network but no internet, red = offline.
               Camera: the webcam button has a light - green = the room camera is there
               and works, red = it is missing, blocked or broken, grey = still looking.
               Tap it for a live test picture (see Camera below).
               Volume: tap the speaker to mute, - / + (hold to repeat), or drag the bar.
               Sun/moon: switch light and dark wallpaper/theme.
               Gear: Settings.   Power: Shut down / Restart (press and hold 1.5 seconds).
Centre         Up to 4 widgets (see below). Out of the box: analog clock, digital clock
               with seconds, month calendar (< > to browse; back to this month after a minute).
Dock           Up to 6 apps. Tap to open.

Editing the home screen: press and hold a widget or an app (or right-click it, tap the
pencil next to the dock, or use Settings > Edit home screen). Everything jiggles:
  - drag a widget or an app to move it,
  - tap it to change it (each widget has a live preview, its own options, move < >, Remove),
  - the red minus removes it (with Undo),
  - "+ Add widget" / "+ Add app" fill free places,
  - Done (or Esc) finishes.

Widgets
-------
Analog clock    second hand, numbers, date - each on/off
Digital clock   seconds, greeting, date - each on/off; Normal or Large
Calendar        ISO week numbers on/off (first day of the week is in Settings)
World clock     city name, any Windows time zone (type to search: "tokyo", "UTC+9"),
                digital or analog, seconds; shows "Today/Tomorrow" and the difference
Note            title, message (Enter = new line, Ctrl+Enter = save), text size, colour
Photo           one picture, or a folder shown as a slideshow (cross-fade every
                10 s / 30 s / 1 min / 5 min, optional shuffle); Landscape, Square or
                Portrait; Fill (crop) or Fit (whole picture); optional caption.
                Phone photos are turned upright automatically. A folder is re-read every
                5 minutes, so pictures copied into it appear by themselves.
When more widgets are chosen than fit across the screen, the row shrinks to fit.

Dock apps
---------
Zoom             Opens the Zoom app; without it, zoom.us/join opens in Microsoft Edge.
Microsoft Teams  Opens new Teams (or Teams free / classic); without it, Teams on the web.
Google Meet      Opens https://meet.google.com in Microsoft Edge.
Webex            Opens the Webex app; without it, web.webex.com.
Also available   Microsoft Edge, Google Chrome, File Explorer (This PC, for USB sticks),
                 Cast screen (Win+K), Display mode (Win+P), PowerPoint, any website, any
                 program / shortcut / document. Store apps: shell:AppsFolder\<AppUserModelId>.
Each app can use one of 32 built-in icons and 12 colours, or "App" to show the
program's own icon.

Camera
------
Tap the webcam button (top bar, left of the volume) to check the room camera before a
meeting. It shows the live picture (like a mirror, as meeting apps do; switch "Mirror
image" off to see it the other way round), its size and frame rate, and every camera
Windows knows: USB, built-in and virtual ones. Tap a camera in the list to make it the
room camera: the light on the home screen watches that one. The first USB camera
RoomHome sees becomes the room camera by itself.
When something is wrong the test says what, and how to fix it: not connected, blocked by
Windows privacy settings (with a button to that page), turned off or a driver problem
(with the Device Manager code), no picture, a very dark picture (closed privacy shutter?),
or another app using the camera. That last one keeps the light green: the camera works,
it is just busy.
RoomHome opens the camera only while the test is on screen. Closing the test, another app
coming to the front, or 3 minutes without a touch release it, so meeting apps always get
the camera. The light never opens the camera: it only asks Windows about the devices.

Settings
--------
Language (Auto / English / ไทย), room name, subtitle, 12/24-hour clock, first day of the
week, Start with Windows, Keep screen on, Ambient clock (dims to a slowly moving clock
after 5/15/30 idle minutes, which also protects TVs from burn-in), Admin PIN, Appearance
(Auto / Dark / Light), accent colour, a dark-mode wallpaper and a light-mode wallpaper
(any JPG/PNG; empty = built-in aurora), Edit home screen, Exit RoomHome.

Admin PIN (optional): when set, renaming, Settings and dock editing ask for it; the
unlock lasts 5 minutes. Alt+F4 does not close RoomHome; use Settings > Exit RoomHome.

Wi-Fi list shows "Windows is hiding the network list"?
  Windows 11 (24H2 and later) only lets apps list Wi-Fi networks when location access is
  on: Settings > Privacy & security > Location > "Let desktop apps access your location".
  The panel has buttons for that page and for the Windows Wi-Fi list. Company (802.1X)
  networks need the Windows Wi-Fi list.

Settings file
-------------
%APPDATA%\RoomHome\config.json (per Windows user; written whenever something changes).
To pre-configure many rooms, edit RoomHome.defaults.sample.json and save it next to
RoomHome.exe as RoomHome.defaults.json: it is used for every user who has no settings yet.
Extra keys: "language" ("auto", "en", "th"), "dockSlots" (3-8), "volumeStep" (1-20 %),
"monitor" (0 = main display, 1..n = a specific display), "camera" and "cameraId" (the
room camera's name and USB ID, written "USB\\VID_046D&PID_085E" in the file; empty = the
first USB camera seen), "cameraMirror" (true / false).

Installing
----------
RoomHome-1.3.0-setup.exe picks the right build for the PC and asks for a language
(English or Thai), which RoomHome then uses until Settings > Language says otherwise.
Options during setup: Start with Windows (for all users) and a desktop shortcut.
Silent install (language = the Windows display language unless /LANG is given):
    RoomHome-1.3.0-setup.exe /S [/LANG=en|th] [/NOAUTOSTART] [/DESKTOP] [/ARCH=x64|x86|arm64] [/D=C:\Folder]
Uninstall from Settings > Apps, or "C:\Program Files\RoomHome\Uninstall.exe" /S.
The portable files RoomHome-1.3.0-<arch>.exe need no installation.

Command line
------------
RoomHome.exe --quit            close a running RoomHome (it saves first)
RoomHome.exe --config <file>   use another settings file
RoomHome.exe --lang en|th      start in this language (same as choosing it in Settings > Language)
RoomHome.exe --dry-run         show what would be launched / powered off, without doing it
RoomHome.exe --window 1280x720 run in a window instead of full screen (testing)
RoomHome.exe --diag <file>     write a report: found apps, audio, Wi-Fi, LAN, internet, cameras
                               (it lists the cameras without turning any of them on)

Kiosk tips
----------
- For a dedicated room PC, combine Start with Windows with automatic sign-in of a
  local "Meeting Room" account.
- Turn on Keep screen on if the TV should never sleep, or leave it off and let
  Windows' power plan switch the display off at night.
- Shut down / Restart act like the Start menu; apps that hang are closed.
