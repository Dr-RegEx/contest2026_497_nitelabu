// SPDX-License-Identifier: Apache-2.0
#include "FactoryActivity.h"
#include <app/ApplicationThread.h>

namespace os::app::factory {
class FactoryApplication : public Application {
    void onCreate() override
    {
        // In Lite this runs on the shared system UI loop, after the launcher
        // returns. Initialize the process owner here, not in main(). Failures
        // are reported/retried by Activity acquisition before backend creation.
        factoryLifecycle().initialize();
        REGISTER_ACTIVITY(FactoryActivity)
    }
    void onForeground() override {}
    void onBackground() override {}
    void onDestroy() override
    {
        // Activity releases its reservation. The shared process executor stays
        // joinable across app removal/relaunch; never run static destructors or
        // wait for MEDIA here (SYSTEM_SERVER_LITE shares this UI loop).
    }
};
#define APPLICATION FactoryApplication
#include <app/AppMain.h>
}
