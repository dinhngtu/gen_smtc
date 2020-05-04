#include "callback.h"

static GenSMTC gen_smtc;
extern "C" __declspec(dllexport) ifc_wa5component * GetWinamp5SystemComponent() {
    return &gen_smtc;
}

void GenSMTC::RegisterServices(api_service* service) {
    this->serviceManager = service;
    auto sf = this->serviceManager->service_getServiceByGuid(syscbApiServiceGuid);
    if (sf) {
        this->sysCallbackApi = reinterpret_cast<api_syscb*>(sf->getInterface());
    }

    this->sysCallbackApi->syscb_registerCallback(static_cast<SysCallback*>(this));
    sf = this->serviceManager->service_getServiceByGuid(coreApiServiceGuid);
    if (sf) {
        core = reinterpret_cast<api_core*>(sf->getInterface());
    }

    if (this->core) {
        this->core->core_addCallback(0, this);
    }
}

void GenSMTC::DeregisterServices(api_service* service) {
}

int GenSMTC::ccb_notify(int msg, int param1, int param2) {
    return 0;
}

FOURCC GenSMTC::getEventType() {
    return SysCallback::SERVICE;
}

int GenSMTC::notify(int msg, intptr_t param1, intptr_t param2) {
    switch (msg) {
    case SvcCallback::ONREGISTER:
    {
        auto sf = (waServiceFactory*)param2;
        if (sf->getGuid() == coreApiServiceGuid) {
            this->core = reinterpret_cast<api_core*>(sf->getInterface());
            //this->core->core_addCallback(0, this);
        }
        break;
    }

    case SvcNotify::ONDEREGISTERED:
    {
        auto sf = (waServiceFactory*)param2;
        if (sf->getGuid() == coreApiServiceGuid) {
            if (this->core) {
                //this->core->core_delCallback(0, this);
            }
            this->core = NULL;
        }
        break;
    }
    }
    return 0;
}

#define CBCLASS GenSMTC
START_MULTIPATCH;
START_PATCH(Patches::wa5)
M_VCB(Patches::wa5, ifc_wa5component, API_WA5COMPONENT_REGISTERSERVICES, RegisterServices);
M_VCB(Patches::wa5, ifc_wa5component, API_WA5COMPONENT_DEREEGISTERSERVICES, DeregisterServices);
NEXT_PATCH(Patches::core)
M_CB(Patches::core, CoreCallback, CCB_NOTIFY, ccb_notify);
NEXT_PATCH(Patches::sys)
M_CB(Patches::sys, SysCallback, SYSCALLBACK_GETEVENTTYPE, getEventType);
M_CB(Patches::sys, SysCallback, SYSCALLBACK_NOTIFY, notify);
END_PATCH
END_MULTIPATCH;
#undef CBCLASS
