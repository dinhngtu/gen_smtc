#pragma once

#include "framework.h"
#include "GEN.H"

#include <windows.media.control.h>
#include <winrt/Windows.Media.Control.h>

using namespace winrt;

class Control {
public:
    explicit Control(const winampGeneralPurposePlugin*);
    Control(const Control&) = delete;
    Control& operator=(const Control&) = delete;
    ~Control() = default;

    void enable();

private:
    Windows::Media::SystemMediaTransportControls smtc{ nullptr };
};

Windows::Media::SystemMediaTransportControls make_smtc(const winampGeneralPurposePlugin* plugin);
