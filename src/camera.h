// Webcams: which cameras this PC has and whether Windows lets them work (CamMonitor), and a live
// test picture from one of them (CamPreview). Nothing here keeps a camera open in the background:
// the monitor only asks Windows about the devices, so meeting apps always get the camera.
#pragma once
#include "common.h"

struct CamDevice {
    std::wstring name;        // as Windows shows it
    std::wstring link;        // Media Foundation symbolic link; empty = Windows knows it but can't use it
    std::wstring instance;    // device instance path
    std::wstring hwid;        // "USB\VID_046D&PID_085E" for USB cameras (the same whichever port), else the instance path
    bool external = false;    // plugged in, not part of the PC
    bool virt = false;        // a software camera, no hardware behind it
    int problem = 0;          // Device Manager problem code, 0 = none
};

struct CamStatus {
    bool ready = false;       // the first look is done
    bool mfMissing = false;   // no Media Foundation (Windows "N" without the Media Feature Pack)
    bool denied = false;      // Windows privacy settings or policy block camera access for desktop apps
    std::vector<CamDevice> cams;
    uint32_t seq = 0;         // changes whenever anything above changes
};

CamStatus ScanCameras();      // one look, on the calling thread (COM initialised)
bool CamSame(const CamStatus& a, const CamStatus& b);

class CamMonitor {
public:
    void Start(HWND notify);  // posts WM_APP_CAMERA whenever the status changes
    void Stop();
    void Rescan();            // a device was plugged in or removed
    bool WaitFirst(DWORD ms);
    CamStatus Snapshot();

private:
    void Run();
    HWND notify_ = nullptr;
    std::thread th_;
    std::mutex mu_;
    CamStatus st_;
    HANDLE wake_ = nullptr, stop_ = nullptr, first_ = nullptr;
};
extern CamMonitor g_cams;

// ---- live test picture
enum CamFail { CAMFAIL_NONE, CAMFAIL_DENIED, CAMFAIL_BUSY, CAMFAIL_NOPICTURE, CAMFAIL_STOPPED, CAMFAIL_GONE,
               CAMFAIL_NOMF, CAMFAIL_OTHER };

struct CamPreviewState {
    enum Phase { Idle, Starting, Running, Failed } phase = Idle;
    int fail = CAMFAIL_NONE;  // why it failed (phase Failed)
    HRESULT hr = S_OK;
    int w = 0, h = 0;         // picture size
    float fps = 0;            // measured
    uint32_t frames = 0;
    float luma = -1;          // mean brightness of the newest picture, 0..255
    double firstFrameMs = 0;  // time from start to the first picture
};

class CamPreview {
public:
    // Starts showing one camera; any earlier preview is stopped. WM_APP_CAMFRAME arrives for every new
    // picture or state change (at most one waiting at a time: call FrameShown when it is handled).
    void Start(const std::wstring& link, HWND notify);
    void Stop();              // returns at once; the camera is released on the preview's own thread
    bool Active() const { return (bool)s_; }
    CamPreviewState State();
    bool TakeFrame(std::vector<uint8_t>& px, int& w, int& h, uint32_t& seq);   // BGRX, top-down; false if nothing newer than seq
    void FrameShown();
    struct Session;

private:
    std::shared_ptr<Session> s_;
};
extern CamPreview g_camPreview;
bool CamWaitIdle(DWORD ms);   // at exit: give stopping previews time to release their camera
