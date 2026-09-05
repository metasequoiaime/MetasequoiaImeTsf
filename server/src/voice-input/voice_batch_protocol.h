#pragma once
#include "config/ime_config.h"
#include <msime/voice/provider_protocol.h>
#include <string>
#include <vector>

namespace VoiceInput
{
metasequoia::voice::MultipartRequest BuildBatchTranscription(const std::vector<float> &samples,
                                                             const VoiceInputConfig &config);
std::string BuildBatchPolish(const std::string &text, const VoiceInputConfig &config);
} // namespace VoiceInput
