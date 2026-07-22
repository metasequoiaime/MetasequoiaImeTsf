import os


def normpath(path):
    return path.replace("\\", "/")


local_app_data_dir = os.environ.get("LOCALAPPDATA")
home_dir = os.path.expanduser("~")

cur_file_path = os.path.dirname(os.path.abspath(__file__))
project_root_path = os.path.dirname(cur_file_path)

user_home = os.path.expanduser("~")

MetasequoiaImeTsf_root_path = normpath(project_root_path)
MetasequoiaImeTsf_src_path = normpath(os.path.join(MetasequoiaImeTsf_root_path, "src"))
vcpkg_include_path = normpath(
    os.path.join(
        MetasequoiaImeTsf_root_path,
        "build",
        "vcpkg_installed",
        "x64-windows-static",
        "include",
    )
)
utfcpp_path = normpath(os.path.join(MetasequoiaImeTsf_root_path, "utfcpp", "source"))
webview2_path = normpath(
    os.path.join(
        user_home,
        ".nuget",
        "packages",
        "microsoft.web.webview2",
        "1.0.3240.44",
        "build",
        "native",
        "include",
    )
)
wim_path = normpath(
    os.path.join(
        user_home,
        ".nuget",
        "packages",
        "microsoft.windows.implementationlibrary",
        "1.0.240803.1",
        "include",
    )
)
boost_path = normpath(os.path.join(user_home, "scoop", "apps", "boost", "current"))

#
# project_root/.clangd
#
dot_clangd_file = os.path.join(
    MetasequoiaImeTsf_root_path, "scripts", "config_files", ".clangd"
)
dot_clangd_output_file = os.path.join(MetasequoiaImeTsf_root_path, ".clangd")
with open(dot_clangd_file, "r", encoding="utf-8") as f:
    lines = f.readlines()
lines[8] = f'      "-I{MetasequoiaImeTsf_root_path}/src",\n'
lines[9] = f'      "-I{MetasequoiaImeTsf_root_path}/src/Candidate",\n'
lines[10] = f'      "-I{MetasequoiaImeTsf_root_path}/src/Compartment",\n'
lines[11] = f'      "-I{MetasequoiaImeTsf_root_path}/src/Composition",\n'
lines[12] = f'      "-I{MetasequoiaImeTsf_root_path}/src/DictEngine",\n'
lines[13] = f'      "-I{MetasequoiaImeTsf_root_path}/src/DisplayAttribute",\n'
lines[14] = f'      "-I{MetasequoiaImeTsf_root_path}/src/Drawing",\n'
lines[15] = f'      "-I{MetasequoiaImeTsf_root_path}/src/Edit",\n'
lines[16] = f'      "-I{MetasequoiaImeTsf_root_path}/src/FanyLog",\n'
lines[17] = f'      "-I{MetasequoiaImeTsf_root_path}/src/File",\n'
lines[18] = f'      "-I{MetasequoiaImeTsf_root_path}/src/Global",\n'
lines[19] = f'      "-I{MetasequoiaImeTsf_root_path}/src/Header",\n'
lines[20] = f'      "-I{MetasequoiaImeTsf_root_path}/src/IME",\n'
lines[21] = f'      "-I{MetasequoiaImeTsf_root_path}/src/IPC",\n'
lines[22] = f'      "-I{MetasequoiaImeTsf_root_path}/src/Key",\n'
lines[23] = f'      "-I{MetasequoiaImeTsf_root_path}/src/LanguageBar",\n'
lines[24] = f'      "-I{MetasequoiaImeTsf_root_path}/src/Register",\n'
lines[25] = f'      "-I{MetasequoiaImeTsf_root_path}/src/Tf",\n'
lines[26] = f'      "-I{MetasequoiaImeTsf_root_path}/src/Thread",\n'
lines[27] = f'      "-I{MetasequoiaImeTsf_root_path}/src/UI",\n'
lines[28] = f'      "-I{MetasequoiaImeTsf_root_path}/src/Utils",\n'
lines[29] = f'      "-I{MetasequoiaImeTsf_root_path}/src/Window",\n'
# utfcpp
lines[31] = f'      "-I{utfcpp_path}",\n'
# vcpkg
lines[33] = (
    f'      "-I{MetasequoiaImeTsf_root_path}/build64/vcpkg_installed/x64-windows-static/include",\n'
)
lines[34] = (
    f'      "-I{MetasequoiaImeTsf_root_path}/build32/vcpkg_installed/x86-windows-static/include",\n'
)
with open(dot_clangd_output_file, "w", encoding="utf-8") as f:
    f.writelines(lines)

#
# project_root/tests/CMakeLists.txt
#
CMakeLists_file = os.path.join(
    MetasequoiaImeTsf_root_path, "scripts", "config_files", "CMakeLists.txt"
)
CMakeLists_output_file = os.path.join(MetasequoiaImeTsf_root_path, "CMakeLists.txt")
with open(CMakeLists_file, "r", encoding="utf-8") as f:
    lines = f.readlines()
with open(CMakeLists_output_file, "w", encoding="utf-8") as f:
    f.writelines(lines)

#
# CMakePresets.json
#
CMakePresets_file = os.path.join(
    MetasequoiaImeTsf_root_path, "scripts", "config_files", "CMakePresets.json"
)
CMakePresets_file_output_file = os.path.join(MetasequoiaImeTsf_root_path, "CMakePresets.json")
with open(CMakePresets_file, "r", encoding="utf-8") as f:
    lines = f.readlines()
lines[8] = f'        "VCPKG_ROOT": "{normpath(user_home)}/scoop/apps/vcpkg/current/"\n'
lines[11] = (
    f'        "CMAKE_TOOLCHAIN_FILE": "{normpath(user_home)}/scoop/apps/vcpkg/current/scripts/buildsystems/vcpkg.cmake",\n'
)
with open(CMakePresets_file_output_file, "w", encoding="utf-8") as f:
    f.writelines(lines)
