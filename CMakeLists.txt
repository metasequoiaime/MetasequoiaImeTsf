cmake_minimum_required(VERSION 3.25.0)
set(PROJECT_NAME MetasequoiaImeTsf)
project(${PROJECT_NAME} LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(
    /D_UNICODE=1
    /DUNICODE=1
)

# For utf8cpp, make it use C++11 and later
add_compile_options(/Zc:__cplusplus)

# add_definitions(
#     /DFANY_DEBUG=1
#     /DFANY_IPC_DEBUG=1
# )

# fmt
find_package(fmt CONFIG REQUIRED)

# For pure-header libs installed by vcpkg like utf8cpp
include_directories("./build/vcpkg_installed/x64-windows/include")

include_directories("./src")
include_directories("./src/Candidate")
include_directories("./src/Compartment")
include_directories("./src/Composition")
include_directories("./src/DisplayAttribute")
include_directories("./src/Edit")
include_directories("./src/FanyLog")
include_directories("./src/Global")
include_directories("./src/Header")
include_directories("./src/IME")
include_directories("./src/IPC")
include_directories("./src/Key")
include_directories("./src/LanguageBar")
include_directories("./src/Register")
include_directories("./src/Tf")
include_directories("./src/Thread")
include_directories("./src/UI")
include_directories("./src/Utils")

add_library(${PROJECT_NAME} SHARED
    # Candidate
    "./src/Candidate/EnumTfCandidates.cpp"
    "./src/Candidate/EnumTfCandidates.h"
    "./src/Candidate/SearchCandidateProvider.cpp"
    "./src/Candidate/SearchCandidateProvider.h"
    "./src/Candidate/TipCandidateList.cpp"
    "./src/Candidate/TipCandidateList.h"
    "./src/Candidate/TipCandidateString.cpp"
    "./src/Candidate/TipCandidateString.h"
    # Compartment
    "./src/Compartment/Compartment.cpp"
    "./src/Compartment/Compartment.h"
    # Composition
    "./src/Composition/Composition.cpp"
    "./src/Composition/CompositionProcessorEngine.cpp"
    "./src/Composition/CompositionProcessorEngine.h"
    "./src/Composition/EndComposition.cpp"
    "./src/Composition/StartComposition.cpp"
    # DisplayAttribute
    "./src/DisplayAttribute/DisplayAttribute.cpp"
    "./src/DisplayAttribute/DisplayAttributeInfo.cpp"
    "./src/DisplayAttribute/DisplayAttributeInfo.h"
    "./src/DisplayAttribute/DisplayAttributeProvider.cpp"
    "./src/DisplayAttribute/DisplayString.h"
    "./src/DisplayAttribute/EnumDisplayAttributeInfo.cpp"
    "./src/DisplayAttribute/EnumDisplayAttributeInfo.h"
    # Edit
    "./src/Edit/EditSession.cpp"
    "./src/Edit/EditSession.h"
    "./src/Edit/GetTextExtentEditSession.cpp"
    "./src/Edit/GetTextExtentEditSession.h"
    "./src/Edit/TextEditSink.cpp"
    # Global
    "./src/Global/FanyDefines.h"
    "./src/Global/Globals.cpp"
    "./src/Global/Globals.h"
    # Header
    "./src/Header/Define.h"
    "./src/Header/Private.h"
    "./src/Header/resource.h"
    "./src/Header/stdafx.h"
    # IME
    "./src/IME/ActiveLanguageProfileNotifySink.cpp"
    "./src/IME/FunctionProviderSink.cpp"
    "./src/IME/MetasequoiaIME.cpp"
    "./src/IME/MetasequoiaIME.def"
    "./src/IME/MetasequoiaIME.h"
    "./src/IME/MetasequoiaIME.rc"
    "./src/IME/MetasequoiaIMEBaseStructure.cpp"
    "./src/IME/MetasequoiaIMEBaseStructure.h"
    "./src/IME/MetasequoiaIMEStructureArray.h"
    # IPC
    "./src/IPC/Ipc.cpp"
    "./src/IPC/Ipc.h"
    # Key
    "./src/Key/KeyEventSink.cpp"
    "./src/Key/KeyHandler.cpp"
    "./src/Key/KeyHandlerEditSession.cpp"
    "./src/Key/KeyHandlerEditSession.h"
    "./src/Key/KeyStateCategory.cpp"
    "./src/Key/KeyStateCategory.h"
    # LanguageBar
    "./src/LanguageBar/LanguageBar.cpp"
    "./src/LanguageBar/LanguageBar.h"
    # Register
    "./src/Register/Register.cpp"
    "./src/Register/RegKey.cpp"
    "./src/Register/RegKey.h"
    # Tf
    "./src/Tf/TfInputProcessorProfile.cpp"
    "./src/Tf/TfInputProcessorProfile.h"
    "./src/Tf/TfTextLayoutSink.cpp"
    "./src/Tf/TfTextLayoutSink.h"
    # Thread
    "./src/Thread/ThreadFocusSink.cpp"
    "./src/Thread/ThreadMgrEventSink.cpp"
    # UI
    "./src/UI/CandidateSessionState.cpp"
    "./src/UI/CandidateSessionState.h"
    "./src/UI/CandidateListUIPresenter.cpp"
    "./src/UI/CandidateListUIPresenter.h"
    # Utils
    "./src/Utils/CommonUtils.cpp"
    "./src/Utils/CommonUtils.h"
    "./src/Utils/FanyUtils.cpp"
    "./src/Utils/FanyUtils.h"
    # Main
    "./src/DllMain.cpp"
    "./src/Server.cpp"
)

target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt-header-only)
