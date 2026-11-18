// Webcams: the camera monitor (which cameras exist, Device Manager problems, privacy switches) and the
// live test preview (Media Foundation source reader in asynchronous mode, on a worker thread).
#include "camera.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <cfgmgr32.h>

CamMonitor g_cams;
CamPreview g_camPreview;

// device property keys (declared here so no initguid.h ordering is needed)
static const DEVPROPKEY kPkInstanceId     = { { 0x78c34fc8, 0x104a, 0x4aca, { 0x9e, 0xa4, 0x52, 0x4d, 0x52, 0x99, 0x6e, 0x57 } }, 256 };
static const DEVPROPKEY kPkInLocalMachine = { { 0x8c7ed206, 0x3f8a, 0x4827, { 0xb3, 0xab, 0xae, 0x9e, 0x1f, 0xae, 0xfc, 0x6c } }, 4 };
static const DEVPROPKEY kPkDeviceDesc     = { { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 2 };
static const DEVPROPKEY kPkCompatibleIds  = { { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 4 };
static const DEVPROPKEY kPkFriendlyName   = { { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 14 };

static const wchar_t kConsent[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\webcam";

// Media Foundation is missing on Windows "N" editions without the Media Feature Pack. Its DLLs are
// delay-loaded, so RoomHome still starts there: only the camera features say they are unavailable.
static bool MfAvailable() {
    static const bool ok = [] {
        for (const wchar_t* d : { L"mfplat.dll", L"mf.dll", L"mfreadwrite.dll" })
            if (!LoadLibraryExW(d, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)) return false;
        return true;
    }();
    return ok;
}

// ---------------------------------------------------------------- device information
static std::wstring DevStr(DEVINST dn, const DEVPROPKEY& key) {   // string, or a string list with its NULs
    DEVPROPTYPE type = 0;
    ULONG size = 0;
    if (CM_Get_DevNode_PropertyW(dn, &key, &type, nullptr, &size, 0) != CR_BUFFER_SMALL || size < sizeof(wchar_t)) return L"";
    std::vector<wchar_t> buf(size / sizeof(wchar_t) + 1, 0);
    if (CM_Get_DevNode_PropertyW(dn, &key, &type, (PBYTE)buf.data(), &size, 0) != CR_SUCCESS) return L"";
    if (type != DEVPROP_TYPE_STRING && type != DEVPROP_TYPE_STRING_LIST) return L"";
    std::wstring s(buf.data(), size / sizeof(wchar_t));
    if (type == DEVPROP_TYPE_STRING) while (!s.empty() && s.back() == 0) s.pop_back();
    return s;
}

static bool DevBool(DEVINST dn, const DEVPROPKEY& key, bool& out) {
    DEVPROPTYPE type = 0;
    DEVPROP_BOOLEAN v = DEVPROP_FALSE;
    ULONG size = sizeof(v);
    if (CM_Get_DevNode_PropertyW(dn, &key, &type, (PBYTE)&v, &size, 0) != CR_SUCCESS || type != DEVPROP_TYPE_BOOLEAN) return false;
    out = v != DEVPROP_FALSE;
    return true;
}

static int DevProblem(DEVINST dn) {
    ULONG status = 0, prob = 0;
    if (CM_Get_DevNode_Status(&status, &prob, dn, 0) != CR_SUCCESS) return 0;
    return (status & DN_HAS_PROBLEM) ? (int)prob : 0;
}

static std::wstring InterfaceInstance(const std::wstring& link) {
    DEVPROPTYPE type = 0;
    wchar_t buf[512] = L"";
    ULONG size = sizeof(buf);
    if (CM_Get_Device_Interface_PropertyW(link.c_str(), &kPkInstanceId, &type, (PBYTE)buf, &size, 0) != CR_SUCCESS ||
        type != DEVPROP_TYPE_STRING)
        return L"";
    return buf;
}

// "USB\VID_046D&PID_085E&MI_00\7&2B4E8B4F&0&0000" -> "USB\VID_046D&PID_085E": the same in every USB port
static std::wstring HwIdOf(const std::wstring& instance) {
    std::wstring u = instance;
    for (auto& c : u) c = (wchar_t)towupper(c);
    if (u.rfind(L"USB\\VID_", 0) == 0) {
        size_t end = u.find(L"&MI_");
        if (end == std::wstring::npos) end = u.find(L'\\', 4);
        return u.substr(0, end);
    }
    return u;
}

static std::wstring AttrStr(IMFAttributes* a, REFGUID key) {
    wchar_t* s = nullptr;
    UINT32 n = 0;
    if (FAILED(a->GetAllocatedString(key, &s, &n)) || !s) return L"";
    std::wstring r(s, n);
    CoTaskMemFree(s);
    return r;
}

static void Describe(CamDevice& d) {
    std::wstring link = ToLower(d.link);
    d.instance = InterfaceInstance(d.link);
    std::wstring inst = ToLower(d.instance);
    d.virt = inst.rfind(L"root\\", 0) == 0 || link.find(L"vcamdevapi") != std::wstring::npos;
    d.hwid = HwIdOf(d.instance.empty() ? d.link : d.instance);
    DEVINST dn = 0;
    bool known = !d.instance.empty() && CM_Locate_DevNodeW(&dn, (DEVINSTID_W)d.instance.c_str(), CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS;
    bool inPc = false;
    if (known && DevBool(dn, kPkInLocalMachine, inPc)) d.external = !inPc && !d.virt;
    else d.external = !d.virt && link.rfind(L"\\\\?\\usb#", 0) == 0;
    if (known) d.problem = DevProblem(dn);
}

// A USB camera with a driver problem (or switched off) has no capture interface, so Media Foundation
// never lists it. Find such cameras directly: they are USB functions of the video class (0x0E).
static void AddBrokenUsbCameras(CamStatus& s) {
    const ULONG flags = CM_GETIDLIST_FILTER_ENUMERATOR | CM_GETIDLIST_FILTER_PRESENT;
    ULONG len = 0;
    if (CM_Get_Device_ID_List_SizeW(&len, L"USB", flags) != CR_SUCCESS || len < 2) return;
    std::vector<wchar_t> ids(len, 0);
    if (CM_Get_Device_ID_ListW(L"USB", ids.data(), len, flags) != CR_SUCCESS) return;
    for (const wchar_t* p = ids.data(); *p; p += wcslen(p) + 1) {
        DEVINST dn = 0;
        if (CM_Locate_DevNodeW(&dn, (DEVINSTID_W)p, CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS) continue;
        if (ToLower(DevStr(dn, kPkCompatibleIds)).find(L"usb\\class_0e") == std::wstring::npos) continue;
        int prob = DevProblem(dn);
        if (!prob) continue;   // working: Media Foundation lists it
        bool listed = false;
        for (auto& c : s.cams) listed = listed || _wcsicmp(c.instance.c_str(), p) == 0;
        if (listed) continue;
        CamDevice d;
        d.instance = p;
        d.hwid = HwIdOf(p);
        d.name = DevStr(dn, kPkFriendlyName);
        if (d.name.empty()) d.name = DevStr(dn, kPkDeviceDesc);
        if (d.name.empty()) d.name = d.hwid;
        bool inPc = false;
        d.external = !(DevBool(dn, kPkInLocalMachine, inPc) && inPc);
        d.problem = prob;
        s.cams.push_back(d);
    }
}

static bool RegDeny(HKEY root, const wchar_t* sub, DWORD flags = 0) {
    wchar_t v[16] = L"";
    DWORD sz = sizeof(v);
    return RegGetValueW(root, sub, L"Value", RRF_RT_REG_SZ | flags, nullptr, v, &sz) == ERROR_SUCCESS && _wcsicmp(v, L"Deny") == 0;
}

static bool PrivacyDenied() {
    std::wstring desktop = std::wstring(kConsent) + L"\\NonPackaged";
    if (RegDeny(HKEY_LOCAL_MACHINE, kConsent, RRF_SUBKEY_WOW6464KEY) || RegDeny(HKEY_CURRENT_USER, kConsent) ||
        RegDeny(HKEY_CURRENT_USER, desktop.c_str()))
        return true;
    DWORD allow = 1, sz = sizeof(allow);   // Group Policy "Allow Use of Camera" switched off
    return RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Camera", L"AllowCamera",
                        RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6464KEY, nullptr, &allow, &sz) == ERROR_SUCCESS && allow == 0;
}

CamStatus ScanCameras() {
    CamStatus s;
    s.ready = true;
    s.denied = PrivacyDenied();
    if (!MfAvailable() || FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) {
        s.mfMissing = true;
    } else {
        ComPtr<IMFAttributes> a;
        if (SUCCEEDED(MFCreateAttributes(&a, 1)) &&
            SUCCEEDED(a->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))) {
            IMFActivate** devs = nullptr;
            UINT32 n = 0;
            if (SUCCEEDED(MFEnumDeviceSources(a.Get(), &devs, &n)) && devs) {
                for (UINT32 i = 0; i < n; i++) {
                    CamDevice d;
                    d.name = AttrStr(devs[i], MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME);
                    d.link = AttrStr(devs[i], MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK);
                    if (!d.link.empty()) {
                        Describe(d);
                        if (d.name.empty()) d.name = d.hwid;
                        s.cams.push_back(d);
                    }
                    devs[i]->Release();
                }
                CoTaskMemFree(devs);
            }
        }
        MFShutdown();
    }
    AddBrokenUsbCameras(s);
    return s;
}

bool CamSame(const CamStatus& a, const CamStatus& b) {
    if (a.ready != b.ready || a.denied != b.denied || a.mfMissing != b.mfMissing || a.cams.size() != b.cams.size()) return false;
    for (size_t i = 0; i < a.cams.size(); i++) {
        const CamDevice &x = a.cams[i], &y = b.cams[i];
        if (x.name != y.name || x.link != y.link || x.problem != y.problem || x.external != y.external || x.virt != y.virt) return false;
    }
    return true;
}

// ---------------------------------------------------------------- monitor
void CamMonitor::Start(HWND notify) {
    if (th_.joinable()) return;
    notify_ = notify;
    wake_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    stop_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    first_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    th_ = std::thread([this] { Run(); });
}

void CamMonitor::Stop() {
    if (!th_.joinable()) return;
    SetEvent(stop_);
    th_.join();
    CloseHandle(wake_);
    CloseHandle(stop_);
    CloseHandle(first_);
    wake_ = stop_ = first_ = nullptr;
}

void CamMonitor::Rescan() {
    if (wake_) SetEvent(wake_);
}

bool CamMonitor::WaitFirst(DWORD ms) { return first_ && WaitForSingleObject(first_, ms) == WAIT_OBJECT_0; }

CamStatus CamMonitor::Snapshot() {
    std::lock_guard<std::mutex> l(mu_);
    return st_;
}

void CamMonitor::Run() {
    HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    // the camera privacy switches: a change wakes the loop at once
    struct Watch { HKEY root; std::wstring sub; REGSAM extra; HKEY key; HANDLE ev; };
    Watch watches[] = {
        { HKEY_CURRENT_USER, kConsent, 0, nullptr, nullptr },
        { HKEY_CURRENT_USER, std::wstring(kConsent) + L"\\NonPackaged", 0, nullptr, nullptr },
        { HKEY_LOCAL_MACHINE, kConsent, KEY_WOW64_64KEY, nullptr, nullptr },
    };
    for (auto& w : watches) {
        if (RegOpenKeyExW(w.root, w.sub.c_str(), 0, KEY_NOTIFY | w.extra, &w.key) != ERROR_SUCCESS) { w.key = nullptr; continue; }
        w.ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        RegNotifyChangeKeyValue(w.key, FALSE, REG_NOTIFY_CHANGE_LAST_SET, w.ev, TRUE);
    }
    for (;;) {
        CamStatus s = ScanCameras();
        bool changed;
        {
            std::lock_guard<std::mutex> l(mu_);
            changed = !st_.ready || !CamSame(s, st_);
            if (changed) {
                s.seq = st_.seq + 1;
                st_ = std::move(s);
            }
        }
        SetEvent(first_);
        if (changed && notify_) PostMessageW(notify_, WM_APP_CAMERA, 0, 0);

        HANDLE hs[2 + 3];
        Watch* fired[2 + 3] = {};
        DWORD n = 0;
        hs[n++] = stop_;
        hs[n++] = wake_;
        for (auto& w : watches)
            if (w.ev) { fired[n] = &w; hs[n++] = w.ev; }
        DWORD r = WaitForMultipleObjects(n, hs, FALSE, 30000);   // and a look every 30 s, in case a notice was missed
        if (r == WAIT_OBJECT_0) break;
        if (r == WAIT_OBJECT_0 + 1) {
            // a camera arriving or leaving sends a burst of device notices: let it settle
            if (WaitForSingleObject(stop_, 700) == WAIT_OBJECT_0) break;
        } else if (r >= WAIT_OBJECT_0 + 2 && r < WAIT_OBJECT_0 + n) {
            Watch* w = fired[r - WAIT_OBJECT_0];
            RegNotifyChangeKeyValue(w->key, FALSE, REG_NOTIFY_CHANGE_LAST_SET, w->ev, TRUE);   // one notice per registration
        }
    }
    for (auto& w : watches) {
        if (w.key) RegCloseKey(w.key);
        if (w.ev) CloseHandle(w.ev);
    }
    if (SUCCEEDED(co)) CoUninitialize();
}

// ---------------------------------------------------------------- live preview
static std::atomic<int> s_live{ 0 };   // preview threads still running (they release their camera on the way out)

static int FailKindFor(HRESULT hr) {
    if (hr == E_ACCESSDENIED) return CAMFAIL_DENIED;
    if (hr == MF_E_HW_MFT_FAILED_START_STREAMING || hr == MF_E_VIDEO_RECORDING_DEVICE_PREEMPTED ||
        hr == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION) || hr == HRESULT_FROM_WIN32(ERROR_BUSY))
        return CAMFAIL_BUSY;
    if (hr == MF_E_VIDEO_RECORDING_DEVICE_INVALIDATED || hr == HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED) ||
        hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || hr == HRESULT_FROM_WIN32(ERROR_NO_SUCH_DEVICE) ||
        hr == HRESULT_FROM_WIN32(ERROR_DEVICE_REMOVED))
        return CAMFAIL_GONE;
    return CAMFAIL_OTHER;
}

struct CamPreview::Session {
    std::weak_ptr<Session> selfRef;   // for the reader callback, which keeps the session alive while MF holds it
    std::wstring link;
    HWND notify = nullptr;
    HANDLE stopEv = nullptr;
    LARGE_INTEGER t0{}, freq{};
    std::atomic<bool> stopping{ false }, dead{ false }, posted{ false };

    std::mutex mu;                 // guards the state and the newest picture
    CamPreviewState st;
    std::vector<uint8_t> frame;
    int fw = 0, fh = 0;
    uint32_t fseq = 0;
    double lastFrame = 0, fpsStart = 0;
    uint32_t fpsCount = 0;

    std::mutex readerMu;           // guards 'reader' between the worker and the reader's callbacks
    ComPtr<IMFSourceReader> reader;
    int ow = 0, oh = 0;            // output picture format (callbacks run one at a time)
    LONG ostride = 0;
    std::vector<uint8_t> work;

    ~Session() { if (stopEv) CloseHandle(stopEv); }

    double Secs() {
        LARGE_INTEGER c;
        QueryPerformanceCounter(&c);
        return (double)(c.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
    }
    void Post() {
        if (notify && !posted.exchange(true)) PostMessageW(notify, WM_APP_CAMFRAME, 0, 0);
    }
    void SetFailed(int kind, HRESULT hr) {
        {
            std::lock_guard<std::mutex> l(mu);
            if (st.phase == CamPreviewState::Failed && st.fail == kind) return;
            st.phase = CamPreviewState::Failed;
            st.fail = kind;
            st.hr = hr;
        }
        Post();
    }
    void HardFail(int kind, HRESULT hr) {   // the session ends
        if (stopping) return;
        SetFailed(kind, hr);
        dead = true;
        SetEvent(stopEv);
    }
    bool ReadFormat(IMFSourceReader* r) {
        ComPtr<IMFMediaType> t;
        if (FAILED(r->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &t))) return false;
        UINT32 w = 0, h = 0;
        if (FAILED(MFGetAttributeSize(t.Get(), MF_MT_FRAME_SIZE, &w, &h)) || !w || !h) return false;
        UINT32 stride = 0;
        LONG s = SUCCEEDED(t->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride)) ? (LONG)stride : 0;
        ow = (int)w;
        oh = (int)h;
        ostride = s ? s : (LONG)w * 4;
        return true;
    }
    void ReadNext() {
        ComPtr<IMFSourceReader> r;
        {
            std::lock_guard<std::mutex> l(readerMu);
            if (stopping) return;
            r = reader;
        }
        if (!r) return;
        HRESULT hr = r->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, nullptr, nullptr, nullptr);
        if (FAILED(hr)) HardFail(FailKindFor(hr), hr);
    }
    void CopyOut(IMFSample* sample) {
        int w = ow, h = oh;
        if (w <= 0 || h <= 0) return;
        ComPtr<IMFMediaBuffer> buf;
        if (FAILED(sample->ConvertToContiguousBuffer(&buf))) return;
        BYTE* scan0 = nullptr;
        LONG pitch = 0;
        ComPtr<IMF2DBuffer> b2;
        bool two = SUCCEEDED(buf.As(&b2)) && SUCCEEDED(b2->Lock2D(&scan0, &pitch));   // pitch < 0: bottom-up
        if (!two) {
            BYTE* data = nullptr;
            DWORD len = 0;
            if (FAILED(buf->Lock(&data, nullptr, &len))) return;
            pitch = ostride;
            LONG ap = pitch < 0 ? -pitch : pitch;
            if (ap < w * 4 || (uint64_t)ap * (uint64_t)h > len) { buf->Unlock(); return; }
            scan0 = pitch < 0 ? data + (size_t)ap * (size_t)(h - 1) : data;
        }
        work.resize((size_t)w * (size_t)h * 4);
        uint64_t sum = 0;
        uint32_t cnt = 0;
        for (int y = 0; y < h; y++) {
            const BYTE* src = scan0 + (ptrdiff_t)pitch * y;
            memcpy(&work[(size_t)y * (size_t)w * 4], src, (size_t)w * 4);
            if ((y & 15) == 8)
                for (int x = 8; x < w; x += 16) {
                    const BYTE* p = src + (size_t)x * 4;
                    sum += (p[2] * 54u + p[1] * 183u + p[0] * 19u) >> 8;
                    cnt++;
                }
        }
        if (two) b2->Unlock2D();
        else buf->Unlock();
        double t = Secs();
        {
            std::lock_guard<std::mutex> l(mu);
            frame.swap(work);
            fw = w;
            fh = h;
            fseq++;
            st.frames++;
            if (st.frames == 1) {
                st.firstFrameMs = t * 1000.0;
                fpsStart = t;
                fpsCount = 0;
            } else if (++fpsCount, t - fpsStart >= 1.0) {
                st.fps = (float)(fpsCount / (t - fpsStart));
                fpsStart = t;
                fpsCount = 0;
            }
            lastFrame = t;
            st.w = w;
            st.h = h;
            st.luma = cnt ? (float)sum / (float)cnt : -1.f;
            if (st.phase != CamPreviewState::Running) {
                st.phase = CamPreviewState::Running;
                st.fail = CAMFAIL_NONE;
                st.hr = S_OK;
            }
        }
        Post();
    }
    void OnSample(HRESULT hr, DWORD flags, IMFSample* sample) {
        if (stopping) return;
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ERROR)) { HardFail(FailKindFor(FAILED(hr) ? hr : E_FAIL), FAILED(hr) ? hr : E_FAIL); return; }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) { HardFail(CAMFAIL_GONE, hr); return; }
        if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
            ComPtr<IMFSourceReader> r;
            {
                std::lock_guard<std::mutex> l(readerMu);
                r = reader;
            }
            if (r) ReadFormat(r.Get());
        }
        if (sample) CopyOut(sample);
        ReadNext();
    }
    void Run();
};

class ReaderCb : public IMFSourceReaderCallback {
public:
    explicit ReaderCb(std::shared_ptr<CamPreview::Session> s) : s_(std::move(s)) {}
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (iid == IID_IUnknown || iid == __uuidof(IMFSourceReaderCallback)) {
            *ppv = static_cast<IMFSourceReaderCallback*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++ref_; }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG r = --ref_;
        if (!r) delete this;
        return r;
    }
    STDMETHODIMP OnReadSample(HRESULT hr, DWORD, DWORD flags, LONGLONG, IMFSample* sample) override {
        s_->OnSample(hr, flags, sample);
        return S_OK;
    }
    STDMETHODIMP OnFlush(DWORD) override { return S_OK; }
    STDMETHODIMP OnEvent(DWORD, IMFMediaEvent*) override { return S_OK; }

private:
    std::atomic<ULONG> ref_{ 1 };
    std::shared_ptr<CamPreview::Session> s_;
};

// The camera's own format closest to 1280 x 720 at up to 30 frames a second (the reader converts it).
static ComPtr<IMFMediaType> PickNativeType(IMFSourceReader* r) {
    ComPtr<IMFMediaType> best;
    double bestScore = -1;
    for (DWORD i = 0; i < 1000; i++) {
        ComPtr<IMFMediaType> t;
        if (FAILED(r->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &t))) break;
        UINT32 w = 0, h = 0, num = 0, den = 0;
        if (FAILED(MFGetAttributeSize(t.Get(), MF_MT_FRAME_SIZE, &w, &h)) || !w || !h) continue;
        double fps = SUCCEEDED(MFGetAttributeRatio(t.Get(), MF_MT_FRAME_RATE, &num, &den)) && den ? (double)num / den : 15.0;
        double area = (double)w * h, target = 1280.0 * 720.0;
        double areaScore = std::max(1.0, area <= target ? area : target - (area - target) / 4);
        double score = std::min(fps, 30.0) * 2e6 + areaScore;
        GUID sub{};
        t->GetGUID(MF_MT_SUBTYPE, &sub);
        if (sub == MFVideoFormat_H264 || sub == MFVideoFormat_HEVC) score *= 0.9;   // heavier to decode than MJPG / YUY2 / NV12
        if (score > bestScore) {
            bestScore = score;
            best = t;
        }
    }
    return best;
}

void CamPreview::Session::Run() {
    HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool mf = MfAvailable() && SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE));
    ComPtr<IMFMediaSource> src;
    ComPtr<IMFSourceReader> r;
    HRESULT hr = S_OK;
    if (!mf) {
        HardFail(CAMFAIL_NOMF, E_NOTIMPL);
    } else {
        ComPtr<IMFAttributes> a;
        hr = MFCreateAttributes(&a, 2);
        if (SUCCEEDED(hr)) hr = a->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
        if (SUCCEEDED(hr)) hr = a->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, link.c_str());
        if (SUCCEEDED(hr)) hr = MFCreateDeviceSource(a.Get(), &src);
        ComPtr<IMFAttributes> ra;
        if (SUCCEEDED(hr)) hr = MFCreateAttributes(&ra, 2);
        if (SUCCEEDED(hr)) {
            ComPtr<ReaderCb> cb;
            cb.Attach(new ReaderCb(selfRef.lock()));
            hr = ra->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, cb.Get());
        }
        if (SUCCEEDED(hr)) hr = ra->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);
        if (SUCCEEDED(hr)) hr = MFCreateSourceReaderFromMediaSource(src.Get(), ra.Get(), &r);
        if (SUCCEEDED(hr)) {
            if (ComPtr<IMFMediaType> native = PickNativeType(r.Get()))
                r->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, native.Get());
            ComPtr<IMFMediaType> out;
            hr = MFCreateMediaType(&out);
            if (SUCCEEDED(hr)) hr = out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
            if (SUCCEEDED(hr)) hr = out->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
            if (SUCCEEDED(hr)) hr = r->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, out.Get());
            if (SUCCEEDED(hr) && !ReadFormat(r.Get())) hr = MF_E_INVALIDMEDIATYPE;
        }
        if (FAILED(hr)) {
            HardFail(FailKindFor(hr), hr);
        } else {
            {
                std::lock_guard<std::mutex> l(readerMu);
                if (!stopping) reader = r;
            }
            ReadNext();
        }
    }
    // watch the picture until stopped (or the camera fails for good)
    while (!dead && !stopping) {
        if (WaitForSingleObject(stopEv, 250) == WAIT_OBJECT_0) break;
        double t = Secs();
        bool post = false;
        {
            std::lock_guard<std::mutex> l(mu);
            if (st.frames == 0 && st.phase == CamPreviewState::Starting && t > 8.0) {
                st.phase = CamPreviewState::Failed;   // soft: a late picture still turns it to Running
                st.fail = CAMFAIL_NOPICTURE;
                post = true;
            } else if (st.frames > 0 && st.phase == CamPreviewState::Running && t - lastFrame > 4.0) {
                st.phase = CamPreviewState::Failed;
                st.fail = CAMFAIL_STOPPED;
                post = true;
            }
        }
        if (post) Post();
    }
    // release the camera
    ComPtr<IMFSourceReader> last;
    {
        std::lock_guard<std::mutex> l(readerMu);
        stopping = true;
        last = std::move(reader);
    }
    if (src) src->Shutdown();   // also ends a read that is still waiting for a picture
    last.Reset();
    r.Reset();
    src.Reset();
    if (mf) MFShutdown();
    if (SUCCEEDED(co)) CoUninitialize();
    if (notify) PostMessageW(notify, WM_APP_CAMFRAME, 1, 0);   // wParam 1: this session has let the camera go
}

void CamPreview::Start(const std::wstring& link, HWND notify) {
    Stop();
    auto s = std::make_shared<Session>();
    s->selfRef = s;
    s->link = link;
    s->notify = notify;
    s->stopEv = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    QueryPerformanceFrequency(&s->freq);
    QueryPerformanceCounter(&s->t0);
    s->st.phase = CamPreviewState::Starting;
    s_ = s;
    s_live++;
    std::thread([s] {
        s->Run();
        s_live--;
    }).detach();
}

void CamPreview::Stop() {
    if (!s_) return;
    s_->stopping = true;
    SetEvent(s_->stopEv);
    s_.reset();
}

CamPreviewState CamPreview::State() {
    if (!s_) return CamPreviewState();
    std::lock_guard<std::mutex> l(s_->mu);
    return s_->st;
}

bool CamPreview::TakeFrame(std::vector<uint8_t>& px, int& w, int& h, uint32_t& seq) {
    if (!s_) return false;
    std::lock_guard<std::mutex> l(s_->mu);
    if (s_->fseq == seq || s_->frame.empty()) return false;
    px.swap(s_->frame);   // the old buffer goes back to the session for a later picture
    w = s_->fw;
    h = s_->fh;
    seq = s_->fseq;
    return true;
}

void CamPreview::FrameShown() {
    if (s_) s_->posted = false;
}

bool CamWaitIdle(DWORD ms) {
    ULONGLONG end = GetTickCount64() + ms;
    while (s_live > 0 && GetTickCount64() < end) Sleep(20);
    return s_live == 0;
}
