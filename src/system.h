#pragma once
#include "common.h"
#include "config.h"
#include <mmdeviceapi.h>
#include <endpointvolume.h>

// ---------------- network ----------------
struct WifiNet {
    std::wstring ssid;
    std::string ssidRaw;
    int quality = 0;              // 0..100
    bool secure = false, connected = false, hasProfile = false;
    bool supported = true;        // false: enterprise / OWE -> use the Windows picker
    std::wstring profile;
    int auth = 0, cipher = 0;
};

struct NetInfo {
    bool wlanService = true;      // WLAN AutoConfig reachable
    bool wifiPresent = false;
    bool wifiRadioOn = true;
    bool wifiConnected = false;
    bool wifiConnecting = false;
    bool wifiDenied = false;      // Windows 11 24H2+: location access is off for desktop apps
    std::wstring wifiSsid, wifiAdapter, wifiIp, wifiGateway, wifiDns, wifiMac;
    int wifiQuality = 0;
    std::vector<WifiNet> nets;
    bool netsValid = false;

    bool lanPresent = false, lanConnected = false;
    uint64_t lanSpeed = 0;        // bits per second
    std::wstring lanAdapter, lanIp, lanGateway, lanDns, lanMac;

    bool internet = false, localOnly = false;
    uint32_t seq = 0;
};

struct WifiEvent {
    enum Code { Info, Connected, Failed, Disconnected, ActionFailed } code = Info;
    std::wstring text;
};

class NetMonitor {
public:
    void Start(HWND notify);
    void Stop();
    NetInfo Snapshot();
    void SetWantList(bool on);         // poll the network list while the Wi-Fi sheet is open
    void Scan();
    void Connect(const WifiNet& net, const std::wstring& password);
    void Disconnect();
    void Forget(const std::wstring& profile);
    void SetRadio(bool on);
    bool WaitFirst(DWORD ms);

    // used by the WLAN callback
    void OnWlan(void* data);

private:
    void Run();
    void Post(std::function<void(void*)> cmd);
    void Gather(NetInfo& n);
    void Emit(WifiEvent::Code c, const std::wstring& text);

    HWND notify_ = nullptr;
    std::thread th_;
    std::mutex mu_;
    std::vector<std::function<void(void*)>> cmds_;
    NetInfo info_;
    HANDLE wake_ = nullptr, first_ = nullptr;
    std::atomic<bool> stop_{ false }, wantList_{ false }, scanNow_{ false };
    void* wlan_ = nullptr;
    GUID ifGuid_{};
    bool haveIf_ = false;
    // pending connect, for failure clean-up / messages
    std::wstring pendingProfile_, pendingSsid_;
    bool pendingCreated_ = false;
    ULONGLONG lastScan_ = 0;
};
extern NetMonitor g_net;

// ---------------- audio ----------------
class AudioCtl : public IAudioEndpointVolumeCallback, public IMMNotificationClient {
public:
    bool Init(HWND notify);
    void Reacquire();
    void Shutdown();
    bool Ok() const { return vol_ != nullptr; }
    float Level();
    bool Muted();
    std::wstring Device() const { return name_; }
    void SetLevel(float v);
    void Step(int dir, int percent);
    void ToggleMute();

    // IUnknown (static lifetime object)
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override { return 2; }
    STDMETHODIMP_(ULONG) Release() override { return 1; }
    // IAudioEndpointVolumeCallback
    STDMETHODIMP OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA) override;
    // IMMNotificationClient
    STDMETHODIMP OnDeviceStateChanged(LPCWSTR, DWORD) override;
    STDMETHODIMP OnDeviceAdded(LPCWSTR) override { return S_OK; }
    STDMETHODIMP OnDeviceRemoved(LPCWSTR) override { return S_OK; }
    STDMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override;
    STDMETHODIMP OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

private:
    HWND hwnd_ = nullptr;
    ComPtr<IMMDeviceEnumerator> en_;
    ComPtr<IAudioEndpointVolume> vol_;
    std::wstring name_;
    bool registered_ = false;
};
extern AudioCtl g_audio;

// ---------------- launching ----------------
struct LaunchMsg { uint32_t uid = 0; bool ok = false; std::wstring text; };
void LaunchAsync(const DockItem& item, HWND notify, bool dryRun);
std::wstring DescribeLaunch(const DockItem& item);   // --diag
std::wstring IconSourceFor(const DockItem& item);    // file / parsing name for the program's own icon

struct IconMsg { std::wstring key; int w = 0, h = 0; std::vector<uint8_t> px; bool ok = false; };
void LoadIconAsync(const std::wstring& key, HWND notify);
void LoadIconSync(const std::wstring& key, IconMsg& out);   // caller has COM initialised

// ---------------- photos ----------------
// Decodes a picture on a worker thread, upright (EXIF orientation) and scaled to cover tw x th pixels.
struct PhotoMsg { std::wstring key; int w = 0, h = 0; std::vector<uint8_t> px; bool ok = false; };
void LoadPhotoSync(const std::wstring& path, int tw, int th, PhotoMsg& out);   // caller has COM initialised
void LoadPhotoAsync(const std::wstring& key, const std::wstring& path, int tw, int th, HWND notify);
bool PickFolder(HWND owner, std::wstring& out);
bool IsImageFile(const std::wstring& name);

// ---------------- time zones (world clock) ----------------
struct ZoneInfo { std::wstring key, display; int offsetMin; };
const std::vector<ZoneInfo>& TimeZones();                       // sorted by UTC offset
const ZoneInfo* FindZone(const std::wstring& key);
bool ZoneNow(const std::wstring& key, SYSTEMTIME& out, int* minutesAheadOfLocal);

// ---------------- power / misc ----------------
bool PowerAction(bool restart, std::wstring& err);
bool AutostartUser();
bool AutostartMachine();
bool SetAutostartUser(bool on);
bool SetAutostartMachine(bool on);
bool RunSelfElevated(const std::wstring& args);
bool SystemPrefersLight();
bool OpenUri(const std::wstring& uri);
bool PickFile(HWND owner, bool image, std::wstring& out);
std::wstring FindEdge();
std::wstring ComputerName();
int LocaleFirstDayIsMonday();     // 1 Monday, 0 Sunday
bool LocaleUses24h();
