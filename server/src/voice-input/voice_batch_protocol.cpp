#include "voice_batch_protocol.h"
#include "voice_providers.h"
#include <msime/voice/wav_writer.h>
#include <nlohmann/json.hpp>

namespace VoiceInput
{
metasequoia::voice::MultipartRequest BuildBatchTranscription(const std::vector<float> &samples,
                                                             const VoiceInputConfig &config)
{
    const bool siliconflow = NormalizeProviderId(config.asr_provider) == "siliconflow";
    std::vector<float> padded;
    if (siliconflow)
    {
        constexpr std::size_t pad_frames = metasequoia::voice::sample_rate / 5;
        padded.reserve(samples.size() + pad_frames * 2);
        padded.insert(padded.end(), pad_frames, 0.0f);
        padded.insert(padded.end(), samples.begin(), samples.end());
        padded.insert(padded.end(), pad_frames, 0.0f);
        if (padded.size() < metasequoia::voice::sample_rate)
            padded.resize(metasequoia::voice::sample_rate, 0.0f);
    }
    constexpr std::size_t upload_sample_limit = (metasequoia::voice::maximum_encoded_audio_bytes - 44) / 2;
    const auto wav = metasequoia::voice::WavWriter::create_wav(siliconflow ? padded : samples,
                                                               metasequoia::voice::sample_rate, upload_sample_limit);
    // SiliconFlow rejects the optional language field. Keep the existing Windows mapping elsewhere.
    const std::string language = !siliconflow && config.language == "zh-cn" ? "zh"
                                 : !siliconflow && config.language == "en"  ? "en"
                                                                            : "";
    return metasequoia::voice::make_transcription_request(
        std::string_view(reinterpret_cast<const char *>(wav.data()), wav.size()), ResolveAsrModel(config), language);
}

std::string BuildBatchPolish(const std::string &text, const VoiceInputConfig &config)
{
    auto body = nlohmann::json::parse(metasequoia::voice::make_polish_request(
        ResolvePolishModel(config), ResolvePolishSystemPrompt(config), WrapAsrUserMessage(text)));
    if (config.polish_provider == "siliconflow")
        body["enable_thinking"] = false;
    else if (config.polish_provider == "deepseek")
        body["thinking"] = {{"type", "disabled"}};
    return body.dump();
}
} // namespace VoiceInput
