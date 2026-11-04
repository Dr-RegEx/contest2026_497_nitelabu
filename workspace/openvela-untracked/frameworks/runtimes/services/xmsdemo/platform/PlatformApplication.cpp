// SPDX-License-Identifier: Apache-2.0
#include "PlatformActivity.h"
#include <app/ApplicationThread.h>

namespace os::app::s31product {
class PlatformApplication : public Application {
    void onCreate() override { REGISTER_ACTIVITY(PlatformActivity) }
    void onForeground() override {}
    void onBackground() override {}
    void onDestroy() override {}
};
#define APPLICATION PlatformApplication
#include <app/AppMain.h>
}
