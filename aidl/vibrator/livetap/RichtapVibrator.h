/*
 * SPDX-FileCopyrightText: The PixelOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/aac/hardware/richtap/vibrator/BnRichtapVibrator.h>

#include <atomic>

namespace aidl::vendor::aac::hardware::richtap::vibrator {

struct Livetap;

/*
 * RichTap on the Oplus livetap haptics core, which drives Qualcomm haptics
 * through /dev/awinic_haptic with the actuator model in /odm/etc/si_livetap_conf.
 */
class RichtapVibrator : public BnRichtapVibrator {
  public:
    // Initializes the haptics core once, also done lazily by every call.
    bool initialize();

    ndk::ScopedAStatus init(const std::shared_ptr<IRichtapCallback>& callback) override;
    ndk::ScopedAStatus setDynamicScale(int32_t scale,
                                       const std::shared_ptr<IRichtapCallback>& callback) override;
    ndk::ScopedAStatus setF0(int32_t f0,
                             const std::shared_ptr<IRichtapCallback>& callback) override;
    ndk::ScopedAStatus stop(const std::shared_ptr<IRichtapCallback>& callback) override;
    ndk::ScopedAStatus setAmplitude(int32_t amplitude,
                                    const std::shared_ptr<IRichtapCallback>& callback) override;
    ndk::ScopedAStatus performHeParam(int32_t interval, int32_t amplitude, int32_t freq,
                                      const std::shared_ptr<IRichtapCallback>& callback) override;
    ndk::ScopedAStatus off(const std::shared_ptr<IRichtapCallback>& callback) override;
    ndk::ScopedAStatus on(int32_t timeoutMs,
                          const std::shared_ptr<IRichtapCallback>& callback) override;
    ndk::ScopedAStatus perform(int32_t effectId, int8_t strength,
                               const std::shared_ptr<IRichtapCallback>& callback,
                               int32_t* _aidl_return) override;
    ndk::ScopedAStatus performEnvelope(const std::vector<int32_t>& envInfo, bool fastFlag,
                                       const std::shared_ptr<IRichtapCallback>& callback) override;
    ndk::ScopedAStatus performRtp(const ndk::ScopedFileDescriptor& hdl,
                                  const std::shared_ptr<IRichtapCallback>& callback) override;
    ndk::ScopedAStatus performHe(int32_t looper, int32_t interval, int32_t amplitude,
                                 int32_t freq, const std::vector<int32_t>& he,
                                 const std::shared_ptr<IRichtapCallback>& callback) override;
    ndk::ScopedAStatus setHapticParam(const std::vector<int32_t>& data, int32_t length,
                                      const std::shared_ptr<IRichtapCallback>& callback) override;

  private:
    // Results reported through IRichtapCallback.
    enum Result : int32_t {
        SUCCESS = 1,
        FAILED,
        NOT_SUPPORT,
        INVALID,
    };

    // Reports the result once the given time has passed, as the framework
    // uses the callback to know when an effect finished.
    static void report(const std::shared_ptr<IRichtapCallback>& callback, int32_t delayMs,
                       Result result);
    // Common handling of a call that returns a duration, negative on failure.
    ndk::ScopedAStatus finish(const std::shared_ptr<IRichtapCallback>& callback,
                              int32_t durationMs, const char* what);
    ndk::ScopedAStatus notSupported(const std::shared_ptr<IRichtapCallback>& callback);

    std::atomic<bool> mReady = false;
    const Livetap* mLivetap = nullptr;
};

}  // namespace aidl::vendor::aac::hardware::richtap::vibrator
