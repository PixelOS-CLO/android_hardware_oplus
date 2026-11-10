/*
 * SPDX-FileCopyrightText: The PixelOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include "RichtapVibrator.h"

using aidl::vendor::aac::hardware::richtap::vibrator::RichtapVibrator;

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(4);

    std::shared_ptr<RichtapVibrator> vibrator = ndk::SharedRefBase::make<RichtapVibrator>();
    vibrator->initialize();

    const std::string instance = std::string(RichtapVibrator::descriptor) + "/default";
    const binder_status_t status =
            AServiceManager_addService(vibrator->asBinder().get(), instance.c_str());
    CHECK_EQ(status, STATUS_OK) << "Failed to register " << instance;

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not reach
}
