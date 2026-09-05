#define NOMINMAX
#include "voice-input/voice_batch_protocol.h"
#include "tests/includes/test_framework.h"
#include <msime/voice/stt_service.h>
#include <nlohmann/json.hpp>

TEST_CASE(voice_batch_protocol_preserves_provider_fields_and_padding)
{
    VoiceInputConfig config;
    config.asr_provider = "openai";
    config.language = "zh-cn";
    config.asr_model = "fixture-asr";
    const std::vector<float> samples(160, 0.125f);
    const auto openai = VoiceInput::BuildBatchTranscription(samples, config);
    REQUIRE(openai.body.find("name=\"language\"\r\n\r\nzh") != std::string::npos);
    REQUIRE(openai.body.find("fixture-asr") != std::string::npos);
    config.asr_provider = "siliconflow";
    const auto siliconflow = VoiceInput::BuildBatchTranscription(samples, config);
    REQUIRE(siliconflow.body.find("name=\"language\"") == std::string::npos);
    const auto wave = siliconflow.body.find("RIFF");
    REQUIRE(wave != std::string::npos);
    // Tiny clips still contain a full second of padded PCM, as required by the existing provider adapter.
    const auto *bytes = reinterpret_cast<const unsigned char *>(siliconflow.body.data() + wave);
    const unsigned data_bytes = bytes[40] | (bytes[41] << 8) | (bytes[42] << 16) | (bytes[43] << 24);
    REQUIRE_EQ(data_bytes, 32000u);
    const std::vector<float> long_clip(61 * metasequoia::voice::sample_rate, 0.125f);
    REQUIRE(VoiceInput::BuildBatchTranscription(long_clip, config).body.size() > long_clip.size() * 2);
}

TEST_CASE(voice_batch_protocol_preserves_polish_prompt_and_thinking_settings)
{
    VoiceInputConfig config;
    config.polish_model = "fixture-polish";
    config.polish_prompt = "fixture prompt";
    config.polish_provider = "deepseek";
    auto body = nlohmann::json::parse(VoiceInput::BuildBatchPolish("原始文本", config));
    REQUIRE_EQ(body["model"].get<std::string>(), "fixture-polish");
    REQUIRE_EQ(body["messages"][0]["content"].get<std::string>(), "fixture prompt");
    REQUIRE_EQ(body["messages"][1]["content"].get<std::string>(), "<asr_text>\n原始文本\n</asr_text>");
    REQUIRE_EQ(body["thinking"]["type"].get<std::string>(), "disabled");
    config.polish_provider = "siliconflow";
    body = nlohmann::json::parse(VoiceInput::BuildBatchPolish("原始文本", config));
    REQUIRE_EQ(body["enable_thinking"].get<bool>(), false);
    REQUIRE(!body.contains("thinking"));
}
