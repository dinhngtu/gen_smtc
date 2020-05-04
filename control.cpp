#include "control.h"

#include <windows.media.control.h>
#include <winrt/Windows.Media.Control.h>
#include <SystemMediaTransportControlsInterop.h>

Control::Control(const winampGeneralPurposePlugin* plugin) {
    winrt::com_ptr<ABI::Windows::Media::ISystemMediaTransportControls> ismtc;
    auto factory = get_activation_factory<Windows::Media::SystemMediaTransportControls, ISystemMediaTransportControlsInterop>();
    auto result = factory->GetForWindow(plugin->hwndParent, winrt::guid_of<Windows::Media::ISystemMediaTransportControls>(), ismtc.put_void());
    this->smtc = { ismtc.as<Windows::Media::SystemMediaTransportControls>() };
}

void Control::enable() {
    this->smtc.IsEnabled(true);
    this->smtc.IsPlayEnabled(true);
}

Windows::Media::SystemMediaTransportControls make_smtc(const winampGeneralPurposePlugin* plugin) {
    com_ptr<ABI::Windows::Media::ISystemMediaTransportControls> ismtc;
    auto factory = get_activation_factory<Windows::Media::SystemMediaTransportControls, ISystemMediaTransportControlsInterop>();
    auto result = factory->GetForWindow(plugin->hwndParent, guid_of<Windows::Media::ISystemMediaTransportControls>(), ismtc.put_void());
    return ismtc.as<Windows::Media::SystemMediaTransportControls>();
}
