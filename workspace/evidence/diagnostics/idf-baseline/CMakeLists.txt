cmake_minimum_required(VERSION 3.22)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(s31_wifi_baseline)

# Diagnostic-only single-library control. Both inputs are already local and
# locked. Do not edit either dependency or the openvela production linkage.
if(S31_BASELINE_HAL_PHY)
    set(s31_phy_archive "/home/regex/work/esp32s31-openvela/s31-reference/deps/esp-hal-3rdparty/components/esp_phy/lib/esp32s31/libphy.a")
    file(SHA256 "${s31_phy_archive}" s31_phy_hash)
    message(STATUS "S31 diagnostic PHY control: ${s31_phy_archive} SHA256=${s31_phy_hash}")
    set_property(TARGET esp_phy_libphy PROPERTY IMPORTED_LOCATION "${s31_phy_archive}")
endif()

if(S31_BASELINE_HAL_WIFI)
    foreach(blob core espnow mesh net80211 pp smartconfig wapi)
        set(s31_wifi_archive "/home/regex/work/esp32s31-openvela/s31-reference/deps/esp-hal-3rdparty/components/esp_wifi/lib/esp32s31/lib${blob}.a")
        file(SHA256 "${s31_wifi_archive}" s31_wifi_hash)
        message(STATUS "S31 diagnostic Wi-Fi control: ${blob} SHA256=${s31_wifi_hash}")
        set_property(TARGET esp_wifi_${blob} PROPERTY IMPORTED_LOCATION "${s31_wifi_archive}")
    endforeach()
endif()
