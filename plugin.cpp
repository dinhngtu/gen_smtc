#include "framework.h"
#include "GEN.H"
#include "control.h"
#include "wa_ipc.h"
#include "ipc_pe.h"
#include "winampcmd.h"

#include <windows.media.control.h>
#include <winrt/Windows.Media.Control.h>
#include <SystemMediaTransportControlsInterop.h>

using namespace Windows::Media;

int init();
void config();
void quit();

winampGeneralPurposePlugin plugin = {
    GPPHDR_VER,
    _strdup("Windows 10 Media Controls Integration"),
    init,
    config,
    quit,
};
extern "C" __declspec(dllexport) winampGeneralPurposePlugin * winampGetGeneralPurposePlugin() {
    return &plugin;
}

Windows::Media::SystemMediaTransportControls make_smtc(const winampGeneralPurposePlugin* plugin) {
    com_ptr<ABI::Windows::Media::ISystemMediaTransportControls> ismtc;
    auto factory = get_activation_factory<Windows::Media::SystemMediaTransportControls, ISystemMediaTransportControlsInterop>();
    auto result = factory->GetForWindow(plugin->hwndParent, guid_of<Windows::Media::ISystemMediaTransportControls>(), ismtc.put_void());
    return ismtc.as<Windows::Media::SystemMediaTransportControls>();
}

SystemMediaTransportControls smtc{ nullptr };
event_token smtc_buttonPressed_token{};

void updateStatus() {
    int status = SendMessage(plugin.hwndParent, WM_USER, 0, IPC_ISPLAYING);
    switch (status) {
    case 0: // not playing
        smtc.PlaybackStatus(MediaPlaybackStatus::Stopped);
        break;
    case 1:
        smtc.PlaybackStatus(MediaPlaybackStatus::Playing);
        break;
    case 3: // paused
        smtc.PlaybackStatus(MediaPlaybackStatus::Paused);
        break;
    }
}

void updateMeta() {
    auto file = (wchar_t*)SendMessage(plugin.hwndParent, WM_WA_IPC, SendMessage(plugin.hwndParent, WM_WA_IPC, 0, IPC_GETLISTPOS), IPC_GETPLAYLISTTITLEW);
    smtc.DisplayUpdater().MusicProperties().Title(hstring(file));
    smtc.DisplayUpdater().Update();
}

void _buttonPressed(const SystemMediaTransportControls& sender, const SystemMediaTransportControlsButtonPressedEventArgs& args) {
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
Windows::Foundation::TypedEventHandler<SystemMediaTransportControls, SystemMediaTransportControlsButtonPressedEventArgs> buttonPressed(_buttonPressed);

WNDPROC lpWndProcOld;

LRESULT CALLBACK WindowProc(
    _In_ HWND   hwnd,
    _In_ UINT   uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
) {
    if (lParam == IPC_CB_MISC) {
        switch (wParam) {
        case IPC_CB_MISC_STATUS:
            updateStatus();
            break;
        case IPC_CB_MISC_TITLE:
            updateMeta();
            break;
        }
    }

    return CallWindowProc(lpWndProcOld, hwnd, uMsg, wParam, lParam);
}

int init() {
    if (!IsWindows10OrGreater()) {
        return 0;
    }

    lpWndProcOld = (WNDPROC)SetWindowLong(plugin.hwndParent, GWL_WNDPROC, (LONG)WindowProc);

    smtc = make_smtc(&plugin);
    smtc_buttonPressed_token = smtc.ButtonPressed(buttonPressed);

    smtc.DisplayUpdater().Type(MediaPlaybackType::Music);
    smtc.IsPlayEnabled(true);
    smtc.IsPauseEnabled(true);
    smtc.IsNextEnabled(true);
    smtc.IsPreviousEnabled(true);
    smtc.IsEnabled(true);

    return 0;
}

void config() {
    std::wostringstream fmt;
    fmt << plugin.hwndParent;
    MessageBox(plugin.hwndParent, fmt.str().c_str(), NULL, MB_OK);
}

void quit() {
    SetWindowLong(plugin.hwndParent, GWL_WNDPROC, (LONG)lpWndProcOld);
    smtc.ButtonPressed(smtc_buttonPressed_token);
    smtc = { nullptr };
}
