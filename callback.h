#pragma once

#include "framework.h"

#include "api/core/api_core.h"
#include "api/syscb/api_syscb.h"
#include "api/service/waservicefactory.h"
#include "api/syscb/callbacks/corecb.h"
#include "api/syscb/callbacks/svccb.h"
#include "Component/ifc_wa5component.h"
#include "bfc/multipatch.h"

enum Patches {
    wa5,
    core,
    sys,
};

class GenSMTC :
    public MultiPatch<Patches::wa5, ifc_wa5component>,
    public MultiPatch<Patches::core, CoreCallback>,
    public MultiPatch<Patches::sys, SysCallback> {
public:
    GenSMTC() = default;
    GenSMTC(const GenSMTC&) = delete;
    GenSMTC& operator=(const GenSMTC&) = delete;
    ~GenSMTC() = default;

    // wa5
    void RegisterServices(api_service* service);
    void DeregisterServices(api_service* service);

    // core
    int ccb_notify(int msg, int param1 = 0, int param2 = 0);

    // sys
    FOURCC getEventType();
    int notify(int msg, intptr_t param1 = 0, intptr_t param2 = 0);

private:
    api_core* core = NULL;
    api_syscb* sysCallbackApi = NULL;
    api_service* serviceManager = NULL;

protected:
    RECVS_MULTIPATCH;
};
