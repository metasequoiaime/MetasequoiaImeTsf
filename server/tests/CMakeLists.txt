set(TEST_EXECUTABLE_NAME "MetasequoiaImeServerTests")

set(TEST_SOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_date_time_query.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_emoji_query.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_kaomoji_query.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_jianpin_query.cpp"
    "${CMAKE_SOURCE_DIR}/src/clipboard/clipboard_history.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_kaomoji_sql.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_shuangpin_scheme.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_quanpin_scheme.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_wubi.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_japanese_romaji.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_shuangpin_query.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_engine_shuangpin_session.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_english_dictionary.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_translation_gloss.cpp"
    "${CMAKE_SOURCE_DIR}/src/cloud/translation_gloss.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_custom_translation.cpp"
    "${CMAKE_SOURCE_DIR}/src/cloud/custom_translation.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_active_client_state.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_outbound_session_state.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_pipe_write_policy.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_input_key_policy.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_candidate_ui_owner.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_candidate_ui_action_policy.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_candidate_ui_state.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_candidate_view_model.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_candidate_text_policy.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_candidate_window_template.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_inline_protocol.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_settings_performance.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_candidate_size_estimator.cpp"
    "${CMAKE_SOURCE_DIR}/src/utils/candidate_size_estimator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_ipc_protocol_constants.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_voice_composition_pipe.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_voice_providers.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_voice_batch_protocol.cpp"
    "${CMAKE_SOURCE_DIR}/src/voice-input/voice_batch_protocol.cpp"
    "${CMAKE_SOURCE_DIR}/src/voice-input/voice_providers.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_floating_toolbar_visibility_policy.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_terminal_deactivation_policy.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_user_dictionary_journal.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_config_template_merge.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_system_font_family.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_dictionary_validation.cpp"
    "${CMAKE_SOURCE_DIR}/src/config/ime_config.cpp"
    "${CMAKE_SOURCE_DIR}/src/session/engine_input_session.cpp"
    "${CMAKE_SOURCE_DIR}/src/utils/common_utils.cpp"
    "${CMAKE_SOURCE_DIR}/src/utils/ime_paths.cpp"
    "${CMAKE_SOURCE_DIR}/src/settings/dictionary_validation.cpp"
)

add_executable(${TEST_EXECUTABLE_NAME} ${TEST_SOURCE_FILES})

target_include_directories(${TEST_EXECUTABLE_NAME}
    PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/src"
    "${MSIME_VENDOR_DIR}"
    "${MSIME_VENDOR_DIR}/MetasequoiaImeEngine/utfcpp/source"
)

target_link_libraries(${TEST_EXECUTABLE_NAME}
    PRIVATE
    MetasequoiaIme::Engine
    MetasequoiaIme::Voice
    nlohmann_json::nlohmann_json
    fmt::fmt
    spdlog::spdlog
    Boost::locale
    Boost::json
    CURL::libcurl
    unofficial::sqlite3::sqlite3
    dwrite
    shell32
    advapi32
    ole32
)

add_test(NAME ${TEST_EXECUTABLE_NAME} COMMAND ${TEST_EXECUTABLE_NAME})

# The identical fixture file also runs in Engine's JavaScript contract CI.
add_executable(test_webview_contract "${MSIME_VENDOR_DIR}/MetasequoiaImeEngine/contracts/webview/test.cpp")
target_link_libraries(test_webview_contract PRIVATE Boost::json)
add_test(NAME webview_contract COMMAND test_webview_contract
    "${MSIME_VENDOR_DIR}/MetasequoiaImeEngine/contracts/webview/fixtures.json")
