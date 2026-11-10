/*
 * SPDX-FileCopyrightText: The PixelOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "RichtapVibrator.h"

#include <android-base/logging.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>

namespace aidl::vendor::aac::hardware::richtap::vibrator {

// Oplus livetap haptics core, with the same API as the AAC RichTap library.
// It is loaded at runtime as it ships with the device blobs.
struct Livetap {
    int (*init)(uint32_t* deviceType);
    void (*looper_start)();
    int32_t (*looper_on)(uint32_t timeoutMs);
    int (*off)();
    int (*setAmplitude)(uint8_t amplitude);
    int32_t (*looper_prebaked_effect)(uint32_t effectId, int32_t strength);
    int32_t (*looper_envelope)(const int32_t* data, uint32_t length, bool fastFlag);
    int32_t (*looper_rtp)(int32_t fd);
    int32_t (*looper_post)(const int32_t* pattern, int32_t length, int32_t intervalMs,
                           int32_t loop, int32_t amplitude, int32_t freq);
    bool (*looper_performParam)(int32_t intervalMs, int32_t amplitude, int32_t freq);
    bool (*looper_stopPerformHe)();
    bool (*set_drc_mode)(int mode);
    int (*update_parameter)(const int32_t* data, int32_t length);
    int (*dynamic_scale)(uint8_t scale);
    int (*setting_f0)(int f0);
};

static const Livetap* loadLivetap() {
    static const Livetap* livetap = []() -> const Livetap* {
        void* handle = dlopen("libsivibrator.so", RTLD_NOW);
        if (handle == nullptr) {
            LOG(ERROR) << "Failed to load livetap: " << dlerror();
            return nullptr;
        }
        static Livetap l;
        bool ok = true;
        auto bind = [&](auto& fn, const char* name) {
            fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(
                    dlsym(handle, (std::string("si_vibra_") + name).c_str()));
            if (fn == nullptr) {
                LOG(ERROR) << "Missing livetap symbol " << name;
                ok = false;
            }
        };
        bind(l.init, "init");
        bind(l.looper_start, "looper_start");
        bind(l.looper_on, "looper_on");
        bind(l.off, "off");
        bind(l.setAmplitude, "setAmplitude");
        bind(l.looper_prebaked_effect, "looper_prebaked_effect");
        bind(l.looper_envelope, "looper_envelope");
        bind(l.looper_rtp, "looper_rtp");
        bind(l.looper_post, "looper_post");
        bind(l.looper_performParam, "looper_performParam");
        bind(l.looper_stopPerformHe, "looper_stopPerformHe");
        bind(l.set_drc_mode, "set_drc_mode");
        bind(l.update_parameter, "update_parameter");
        bind(l.dynamic_scale, "dynamic_scale");
        bind(l.setting_f0, "setting_f0");
        return ok ? &l : nullptr;
    }();
    return livetap;
}

namespace {

// Time after which a call without a duration is reported.
constexpr int32_t kDefaultReportMs = 10;

constexpr int32_t kHapticParamDrcMark = 0x01;
constexpr int32_t kHapticParamMaxDrc = 0x03;

}  // namespace

bool RichtapVibrator::initialize() {
    static std::mutex lock;
    std::lock_guard<std::mutex> guard(lock);
    if (mReady) return true;

    mLivetap = loadLivetap();
    if (mLivetap == nullptr) return false;

    uint32_t deviceType = 0;
    const int ret = mLivetap->init(&deviceType);
    if (ret != 0) {
        LOG(ERROR) << "livetap init failed: " << ret;
        return false;
    }
    mLivetap->looper_start();
    mReady = true;
    LOG(INFO) << "livetap ready, device type " << deviceType;
    return true;
}

void RichtapVibrator::report(const std::shared_ptr<IRichtapCallback>& callback, int32_t delayMs,
                             Result result) {
    if (callback == nullptr) return;
    std::thread([callback, delayMs, result] {
        if (delayMs > 0) usleep(delayMs * 1000);
        callback->onCallback(result);
    }).detach();
}

ndk::ScopedAStatus RichtapVibrator::finish(const std::shared_ptr<IRichtapCallback>& callback,
                                           int32_t durationMs, const char* what) {
    if (durationMs < 0) {
        LOG(ERROR) << what << " failed: " << durationMs;
        report(callback, kDefaultReportMs, FAILED);
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    report(callback, durationMs, SUCCESS);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus RichtapVibrator::notSupported(
        const std::shared_ptr<IRichtapCallback>& callback) {
    report(callback, kDefaultReportMs, NOT_SUPPORT);
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus RichtapVibrator::init(const std::shared_ptr<IRichtapCallback>& callback) {
    if (!initialize()) return notSupported(callback);
    report(callback, kDefaultReportMs, SUCCESS);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus RichtapVibrator::setDynamicScale(
        int32_t scale, const std::shared_ptr<IRichtapCallback>& callback) {
    if (!initialize()) return notSupported(callback);
    return finish(callback, mLivetap->dynamic_scale(scale) == 0 ? kDefaultReportMs : -1,
                  "setDynamicScale");
}

ndk::ScopedAStatus RichtapVibrator::setF0(int32_t f0,
                                          const std::shared_ptr<IRichtapCallback>& callback) {
    if (!initialize()) return notSupported(callback);
    return finish(callback, mLivetap->setting_f0(f0) == 0 ? kDefaultReportMs : -1, "setF0");
}

ndk::ScopedAStatus RichtapVibrator::stop(const std::shared_ptr<IRichtapCallback>& callback) {
    if (!initialize()) return notSupported(callback);
    return finish(callback, mLivetap->looper_stopPerformHe() ? kDefaultReportMs : -1, "stop");
}

ndk::ScopedAStatus RichtapVibrator::setAmplitude(
        int32_t amplitude, const std::shared_ptr<IRichtapCallback>& callback) {
    if (!initialize()) return notSupported(callback);
    return finish(callback, mLivetap->setAmplitude(amplitude) == 0 ? kDefaultReportMs : -1,
                  "setAmplitude");
}

ndk::ScopedAStatus RichtapVibrator::performHeParam(
        int32_t interval, int32_t amplitude, int32_t freq,
        const std::shared_ptr<IRichtapCallback>& callback) {
    if (!initialize()) return notSupported(callback);
    return finish(callback,
                  mLivetap->looper_performParam(interval, amplitude, freq) ? kDefaultReportMs : -1,
                  "performHeParam");
}

ndk::ScopedAStatus RichtapVibrator::off(const std::shared_ptr<IRichtapCallback>& callback) {
    if (!initialize()) return notSupported(callback);
    return finish(callback, mLivetap->off() == 0 ? kDefaultReportMs : -1, "off");
}

ndk::ScopedAStatus RichtapVibrator::on(int32_t timeoutMs,
                                       const std::shared_ptr<IRichtapCallback>& callback) {
    if (!initialize()) return notSupported(callback);
    return finish(callback, mLivetap->looper_on(timeoutMs), "on");
}

ndk::ScopedAStatus RichtapVibrator::perform(int32_t effectId, int8_t strength,
                                            const std::shared_ptr<IRichtapCallback>& callback,
                                            int32_t* _aidl_return) {
    if (!initialize()) return notSupported(callback);
    const int32_t durationMs = mLivetap->looper_prebaked_effect(effectId, strength);
    *_aidl_return = durationMs;
    return finish(callback, durationMs, "perform");
}

ndk::ScopedAStatus RichtapVibrator::performEnvelope(
        const std::vector<int32_t>& envInfo, bool fastFlag,
        const std::shared_ptr<IRichtapCallback>& callback) {
    if (!initialize()) return notSupported(callback);
    if (envInfo.empty()) return finish(callback, -1, "performEnvelope");
    return finish(callback, mLivetap->looper_envelope(envInfo.data(), envInfo.size(), fastFlag),
                  "performEnvelope");
}

ndk::ScopedAStatus RichtapVibrator::performRtp(const ndk::ScopedFileDescriptor& hdl,
                                               const std::shared_ptr<IRichtapCallback>& callback) {
    if (!initialize()) return notSupported(callback);
    // The haptics core owns the descriptor it is given.
    const int fd = hdl.get() >= 0 ? fcntl(hdl.get(), F_DUPFD_CLOEXEC, 0) : -1;
    if (fd < 0) return finish(callback, -1, "performRtp");
    const int32_t durationMs = mLivetap->looper_rtp(fd);
    return finish(callback, durationMs > 0 ? durationMs : -1, "performRtp");
}

ndk::ScopedAStatus RichtapVibrator::performHe(int32_t looper, int32_t interval, int32_t amplitude,
                                              int32_t freq, const std::vector<int32_t>& he,
                                              const std::shared_ptr<IRichtapCallback>& callback) {
    if (!initialize()) return notSupported(callback);
    if (he.empty()) return finish(callback, -1, "performHe");
    return finish(callback,
                  mLivetap->looper_post(he.data(), he.size(), interval, looper, amplitude, freq),
                  "performHe");
}

ndk::ScopedAStatus RichtapVibrator::setHapticParam(
        const std::vector<int32_t>& data, int32_t length,
        const std::shared_ptr<IRichtapCallback>& callback) {
    if (!initialize()) return notSupported(callback);
    length = std::min<int32_t>(length, data.size());
    if (length == 2 && data[0] == kHapticParamDrcMark) {
        if (data[1] >= 0 && data[1] <= kHapticParamMaxDrc) mLivetap->set_drc_mode(data[1]);
    } else if (length > 0) {
        mLivetap->update_parameter(data.data(), length);
    }
    report(callback, kDefaultReportMs, SUCCESS);
    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::vendor::aac::hardware::richtap::vibrator
