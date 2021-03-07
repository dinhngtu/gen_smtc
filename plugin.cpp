// SPDX-License-Identifier: GPL-3.0-or-later

#include "framework.h"

#include <OCIdl.h>
#include <OleCtl.h>

#include <winrt/Windows.Foundation.h>
#include <windows.storage.streams.h>
#include <winrt/Windows.Storage.Streams.h>
#include <windows.media.control.h>
#include <winrt/Windows.Media.Control.h>
#include <SystemMediaTransportControlsInterop.h>
#include <robuffer.h>
#include <shcore.h>

#include "GEN.H"
#include "control.h"
#include "wa_ipc.h"
#include "ipc_pe.h"
#include "winampcmd.h"
#include "api/service/api_service.h"
#include "api/service/waServiceFactory.h"
#include "api/service/svcs/svc_imgload.h"
#include "api/service/svcs/svc_imgwrite.h"
#include "api/memmgr/api_memmgr.h"
#include "AlbumArt/api_albumart.h"

constexpr auto THUMBNAIL_SIZE = 100;

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media;
using namespace winrt::Windows::Storage::Streams;
using namespace ::Windows::Storage::Streams;

static int init();
static void config();
static void quit();
static void onButtonPressed(const SystemMediaTransportControls&, const SystemMediaTransportControlsButtonPressedEventArgs&);

static winampGeneralPurposePlugin plugin = {
    GPPHDR_VER,
    _strdup("Windows 10 Media Controls Integration"),
    init,
    config,
    quit,
};
extern "C" __declspec(dllexport) winampGeneralPurposePlugin * winampGetGeneralPurposePlugin() {
    return &plugin;
}

static api_service* serviceApi;
static api_memmgr* memmgrApi;
static api_albumart* albumartApi;

static SystemMediaTransportControls smtc{ nullptr };
static winrt::event_token buttonPressedToken{};
static TypedEventHandler<SystemMediaTransportControls, SystemMediaTransportControlsButtonPressedEventArgs> buttonPressedHandler(onButtonPressed);

static bool enabled = true;
static WNDPROC windowProcOld;

static HBITMAP srcBMP = nullptr;

static SystemMediaTransportControls makeSmtc(const winampGeneralPurposePlugin* plugin) {
    winrt::com_ptr<ABI::Windows::Media::ISystemMediaTransportControls> ismtc;
    auto factory = winrt::get_activation_factory<SystemMediaTransportControls, ISystemMediaTransportControlsInterop>();
    auto result = factory->GetForWindow(plugin->hwndParent, IID_PPV_ARGS(ismtc.put()));
    return ismtc.as<SystemMediaTransportControls>();
}

static void updateStatus() {
    int status = SendMessage(plugin.hwndParent, WM_WA_IPC, 0, IPC_ISPLAYING);
    switch (status) {
    case 0:
        smtc.PlaybackStatus(MediaPlaybackStatus::Stopped);
        break;
    case 1:
        smtc.PlaybackStatus(MediaPlaybackStatus::Playing);
        break;
    case 3:
        smtc.PlaybackStatus(MediaPlaybackStatus::Paused);
        break;
    }
}

static std::wstring getMetadata(const std::wstring& filename, const std::wstring& field) {
    extendedFileInfoStructW fi{};
    fi.filename = filename.c_str();
    fi.metadata = field.c_str();
    std::vector<wchar_t> ret(1024);
    fi.ret = &ret[0];
    fi.retlen = ret.size();
    SendMessage(plugin.hwndParent, WM_WA_IPC, (WPARAM)&fi, IPC_GET_EXTENDED_FILE_INFOW);
    *ret.end() = 0;
    return std::wstring(&ret[0]);
}

static HBITMAP getAlbumArt(const std::wstring& filename, const std::wstring& type) {
    int cur_w, cur_h;
    ARGB32* cur_image = nullptr;

    if (albumartApi->GetAlbumArt(filename.c_str(), type.c_str(), &cur_w, &cur_h, &cur_image) == ALBUMART_SUCCESS) {
        if (srcBMP) {
            DeleteObject(srcBMP);
        }

        BITMAPINFO bmi;
        ZeroMemory(&bmi, sizeof bmi);
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = cur_w;
        bmi.bmiHeader.biHeight = -cur_h;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        bmi.bmiHeader.biSizeImage = 0;
        bmi.bmiHeader.biXPelsPerMeter = 0;
        bmi.bmiHeader.biYPelsPerMeter = 0;
        bmi.bmiHeader.biClrUsed = 0;
        bmi.bmiHeader.biClrImportant = 0;
        void* bits = 0;

        srcBMP = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
        winrt::check_pointer(srcBMP);
        memcpy(bits, cur_image, cur_w * cur_h * 4);
        if (cur_image) {
            memmgrApi->sysFree(cur_image);
            cur_image = nullptr;
        }

        int sw = cur_w;
        int sh = cur_h;
        if (sw > sh && sw > THUMBNAIL_SIZE) {
            sh = sh * THUMBNAIL_SIZE / sw;
            sw = THUMBNAIL_SIZE;
        } else if (sh > sw && sh > THUMBNAIL_SIZE) {
            sw = sw * THUMBNAIL_SIZE / sh;
            sh = THUMBNAIL_SIZE;
        }
        if (sw != cur_w) {
            HDC srcDC = CreateCompatibleDC(NULL);
            winrt::check_pointer(srcDC);
            HGDIOBJ oldSrc = SelectObject(srcDC, srcBMP);

            HDC screenDC = GetDC(NULL);
            HBITMAP sizedBmp = CreateCompatibleBitmap(screenDC, sw, sh);
            winrt::check_pointer(sizedBmp);

            HDC sizedDC = CreateCompatibleDC(srcDC);
            winrt::check_pointer(sizedDC);
            HGDIOBJ oldSized = SelectObject(sizedDC, sizedBmp);

            winrt::check_bool(SetStretchBltMode(sizedDC, HALFTONE));
            winrt::check_bool(StretchBlt(sizedDC, 0, 0, sw, sh, srcDC, 0, 0, cur_w, cur_h, SRCCOPY));

            SelectObject(srcDC, oldSrc);
            SelectObject(sizedDC, oldSized);
            DeleteDC(srcDC);
            DeleteDC(sizedDC);
            ReleaseDC(NULL, screenDC);

            DeleteObject(srcBMP);
            srcBMP = sizedBmp;
        }

        return srcBMP;
    }

    return 0;
}

static RandomAccessStreamReference getThumbnailStream(HBITMAP bmp) {
    PICTDESC pd{};
    pd.cbSizeofstruct = sizeof(pd);
    pd.picType = PICTYPE_BITMAP;
    pd.bmp.hbitmap = bmp;

    winrt::com_ptr<IPicture> pict;
    winrt::check_hresult(OleCreatePictureIndirect(&pd, winrt::guid_of<IPicture>(), TRUE, pict.put_void()));

    winrt::com_ptr<IStream> stream;
    winrt::check_hresult(CreateStreamOnHGlobal(nullptr, TRUE, stream.put()));

    LONG fsz = 0;
    winrt::check_hresult(pict->SaveAsFile(stream.get(), TRUE, &fsz));

    winrt::com_ptr<ABI::Windows::Storage::Streams::IRandomAccessStream> outstream;
    winrt::check_hresult(CreateRandomAccessStreamOverStream(stream.get(), BSOS_DEFAULT, IID_PPV_ARGS(outstream.put())));

    return RandomAccessStreamReference::CreateFromStream(outstream.as<IRandomAccessStream>());
}

static void updateMeta() {
    auto du = smtc.DisplayUpdater();

    du.ClearAll();
    du.Type(MediaPlaybackType::Music);

    auto mp = du.MusicProperties();

    auto _filename = (wchar_t*)SendMessage(plugin.hwndParent, WM_WA_IPC, 0, IPC_GET_PLAYING_FILENAME);
    if (_filename && *_filename) {
        auto filename = std::wstring(_filename);
        mp.Title(getMetadata(filename, L"title"));
        mp.Artist(getMetadata(filename, L"artist"));
        mp.AlbumTitle(getMetadata(filename, L"album"));

        auto art = getAlbumArt(filename, L"cover");
        if (art) {
            du.Thumbnail(getThumbnailStream(art));
        }
    } else {
        auto title = std::wstring((wchar_t*)SendMessage(plugin.hwndParent, WM_WA_IPC, 0, IPC_GET_PLAYING_TITLE));
        mp.Title(title);
    }

    du.Update();
}

static void onButtonPressed(const SystemMediaTransportControls& sender, const SystemMediaTransportControlsButtonPressedEventArgs& args) {
    switch (args.Button()) {
    case SystemMediaTransportControlsButton::Play:
        SendMessage(plugin.hwndParent, WM_COMMAND, WINAMP_BUTTON2, 0);
        break;
    case SystemMediaTransportControlsButton::Pause:
        SendMessage(plugin.hwndParent, WM_COMMAND, WINAMP_BUTTON3, 0);
        break;
    case SystemMediaTransportControlsButton::Next:
        SendMessage(plugin.hwndParent, WM_COMMAND, WINAMP_BUTTON5, 0);
        break;
    case SystemMediaTransportControlsButton::Previous:
        SendMessage(plugin.hwndParent, WM_COMMAND, WINAMP_BUTTON1, 0);
        break;
    }
}

static LRESULT CALLBACK windowProc(
    _In_ HWND   hwnd,
    _In_ UINT   uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
) {
    if (enabled && lParam == IPC_CB_MISC) {
        switch (wParam) {
        case IPC_CB_MISC_STATUS:
            updateStatus();
            break;
        case IPC_CB_MISC_TITLE:
            updateMeta();
            break;
        }
    }

    return CallWindowProc(windowProcOld, hwnd, uMsg, wParam, lParam);
}

static int init() {
    if (!IsWindows10OrGreater()) {
        return 1;
    }

    serviceApi = (api_service*)SendMessage(plugin.hwndParent, WM_WA_IPC, 0, IPC_GET_API_SERVICE);
    if (!serviceApi || serviceApi == (api_service*)1) {
        return 1;
    }

    memmgrApi = reinterpret_cast<api_memmgr*>(serviceApi->service_getServiceByGuid(memMgrApiServiceGuid)->getInterface());
    albumartApi = reinterpret_cast<api_albumart*>(serviceApi->service_getServiceByGuid(albumArtGUID)->getInterface());

    smtc = makeSmtc(&plugin);
    buttonPressedToken = smtc.ButtonPressed(buttonPressedHandler);

    smtc.IsPlayEnabled(true);
    smtc.IsPauseEnabled(true);
    smtc.IsNextEnabled(true);
    smtc.IsPreviousEnabled(true);
    smtc.IsEnabled(true);

    windowProcOld = (WNDPROC)SetWindowLong(plugin.hwndParent, GWL_WNDPROC, (LONG)windowProc);

    return 0;
}

static void config() {
}

static void quit() {
    enabled = false;
    smtc.ButtonPressed(buttonPressedToken);
    smtc = { nullptr };
}
