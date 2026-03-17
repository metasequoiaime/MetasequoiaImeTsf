# Metasequoia IME TSF(水杉输入法 TSF 端)

This is the main part of the Metasequoia IME. To learn more about this project, please visit the docs site:

- [Metasequoia IME](https://github.com/metasequoiaime/MetasequoiaIME).

## How to build

### Prerequisites

- Visual Studio 2026
- CMake
- vcpkg
- Python3.10+
- gsudo

Make sure vcpkg and gsudo is installed by **Scoop**.

## Build steps

### Build

First, clone the repository,

```powershell
git clone --recursive https://github.com/metasequoiaime/MetasequoiaImeTsf.git
```

Then, prepare the environment,

```powershell
cd MetasequoiaImeTsf
python .\scripts\prepare_env.py
```

Then, build both 64-bit and 32-bit dll files,

```powershell
.\scripts\lcompile.ps1 64
.\scripts\lcompile.ps1 32
```

### Install

Launch powershell as administrator, make sure you turn on the system `Enable sudo` option.

![](https://i.postimg.cc/zJCn9Cnn/image.png)

Then, create a folder in `C:\Program Files\` named `MetasequoiaImeTsf`, and copy the 64-bit version `MetasequoiaImeTsf.dll` to it, alse create a folder in `C:\Program Files (x86)\` named `MetasequoiaImeTsf`, and copy the 32-bit version `MetasequoiaImeTsf.dll` to it.

```powershell
gsudo
Copy-Item -Path ".\build64\Debug\MetasequoiaImeTsf.dll" -Destination "C:\Program Files\MetasequoiaImeTsf"
Copy-Item -Path ".\build32\Debug\MetasequoiaImeTsf.dll" -Destination "C:\Program Files (x86)\MetasequoiaImeTsf"
```

Then, install it,

```powershell
cd "C:\Program Files\MetasequoiaImeTsf"
sudo regsvr32 .\MetasequoiaImeTsf.dll
cd "C:\Program Files (x86)\MetasequoiaImeTsf"
sudo regsvr32 .\MetasequoiaImeTsf.dll
```

### Uninstall

```powershell
cd "C:\Program Files\MetasequoiaImeTsf"
sudo regsvr32 /u .\MetasequoiaImeTsf.dll
cd "C:\Program Files (x86)\MetasequoiaImeTsf"
sudo regsvr32 /u .\MetasequoiaImeTsf.dll
```

## Screenshots

![](https://i.imgur.com/hYPpwK4.png)

![](https://i.imgur.com/1Tzd7zn.png)

![](https://i.imgur.com/SsLCCCX.png)

![](https://i.imgur.com/jwnWQLI.png)

![](https://i.postimg.cc/2m9WJTgR/image.png)

![](https://i.postimg.cc/L96qQZT8/image.png)

![](https://i.postimg.cc/FNcz9QTv/image.png)

## Roadmap

Currently only support Xiaohe Shuangpin.

### Chinese

- Xiaohe Shuangpin
- Quanpin
- Help codes in use of Hanzi Components
- Dictionary that can be customized
- Customized IME engine
- Customized skins
- Toggle between Simplified Chinese and Traditional Chinese
- English autocomplete
- Open-Sourced Cloud IME api
- Toggle candidate window UI between vertical mode and horizontal mode
- Feature switches: most features should be freely toggled or customized by users

### Japanese Support

Maybe another project.

And maybe some other languages support.

### References

- [MS-TSF-IME-Demo](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/IME/cpp/SampleIME)
