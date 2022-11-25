#define LOG_TAG "UsecaseValidator"
// #define LOG_NDEBUG 0

#include <inttypes.h>

#include <utils/Log.h>

#include "media/UsecaseValidator.h"
#include "media/UsecaseLookup.h"

namespace android {
namespace media {
namespace {

class UsecaseValidatorImpl : public UsecaseValidator {
 public:
    UsecaseValidatorImpl() {}

    /**
     * Register a new mixer/stream.
     * Called when the stream is opened at the HAL and communicates
     * immutable stream attributes like flags, sampling rate, format.
     */
    status_t registerStream(audio_io_handle_t streamId,
                            const audio_config_base_t& audioConfig __attribute__((unused)),
                            const audio_output_flags_t outputFlags) override {
        ALOGV("%s output: %d flags: %#x", __func__, streamId, outputFlags);

        // Check if FAST or MMAP output flag has been set.
        bool outputFlagGame = outputFlags & (AUDIO_OUTPUT_FLAG_FAST | AUDIO_OUTPUT_FLAG_MMAP_NOIRQ);
        m_lookup.addStream(streamId, outputFlagGame);
        return OK;
    };

    /**
     * Unregister a stream/mixer.
     * Called when the stream is closed.
     */
    status_t unregisterStream(audio_io_handle_t streamId) override {
        ALOGV("%s output: %d", __func__, streamId);

        m_lookup.removeStream(streamId);
        return OK;
    };

    /**
     * Indicates that some playback activity started on the stream.
     * Called each time an audio track starts or resumes.
     */
    error::Result<audio_attributes_t> startClient(audio_io_handle_t streamId,
            audio_port_handle_t portId, const content::AttributionSourceState& attributionSource,
            const audio_attributes_t& attributes,
            const AttributesChangedCallback *callback __attribute__((unused))) override {
        ALOGV("%s output: %d portId: %d usage: %d pid: %d package: %s",
                __func__, streamId, portId, attributes.usage, attributionSource.pid,
                attributionSource.packageName.value_or("").c_str());

        m_lookup.addTrack(streamId, portId);

        return verifyAudioAttributes(streamId, attributionSource, attributes);
    };

    /**
     * Indicates that some playback activity stopped on the stream.
     * Called each time an audio track stops or pauses.
     */
    status_t stopClient(audio_io_handle_t streamId, audio_port_handle_t portId) override {
        ALOGV("%s output: %d portId: %d", __func__, streamId, portId);

        m_lookup.removeTrack(streamId, portId);
        return OK;
    };

    /**
     * Called to verify and update audio attributes for a track that is connected
     * to the specified stream.
     */
    error::Result<audio_attributes_t> verifyAudioAttributes(audio_io_handle_t streamId,
            const content::AttributionSourceState& attributionSource,
            const audio_attributes_t& attributes) override {
        ALOGV("%s output: %d usage: %d pid: %d package: %s",
                __func__, streamId, attributes.usage, attributionSource.pid,
                attributionSource.packageName.value_or("").c_str());

        audio_attributes_t attrRet = attributes;

        // Check if attribute usage media or unknown has been set.
        bool isUsageValid = this->isUsageValid(attributes);

        if (isUsageValid && m_lookup.isGameStream(streamId)) {
            ALOGI("%s update usage: %d to AUDIO_USAGE_GAME for output: %d pid: %d package: %s",
                    __func__, attributes.usage, streamId, attributionSource.pid,
                    attributionSource.packageName.value_or("").c_str());
            // Set attribute usage Game.
            attrRet.usage = AUDIO_USAGE_GAME;
        }

        return {attrRet};
    };

 protected:
    /**
     * Check if attribute usage valid.
     */
    bool isUsageValid(const audio_attributes_t& attr) {
        ALOGV("isUsageValid attr.usage: %d", attr.usage);
        switch (attr.usage) {
            case AUDIO_USAGE_MEDIA:
            case AUDIO_USAGE_UNKNOWN:
                return true;
            default:
                break;
        }
        return false;
    }

 protected:
    UsecaseLookup m_lookup;
};

}  // namespace

std::unique_ptr<UsecaseValidator> createUsecaseValidator() {
    return std::make_unique<UsecaseValidatorImpl>();
}

}  // namespace media
}  // namespace android
