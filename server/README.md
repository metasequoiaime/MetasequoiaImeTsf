# Metasequoia IME Server(水杉输入法 Server 端)

This is the server end of [MSIME-Windows](https://github.com/metasequoiaime/MSIME-Windows).

## How to build

### Prerequisites

- Visual Studio 2026
- CMake
- vcpkg
- Python3.10+
- Boost

Make sure vcpkg and Boost are installed by **Scoop**.

### Build steps(For Dev)

Clone the product and provision the reviewed dictionary release from its root:

```powershell
git clone --recursive https://github.com/metasequoiaime/MSIME-Windows.git
cd MSIME-Windows
python scripts/product_lock.py fetch-dictionaries --staging-root .
if ($LASTEXITCODE -ne 0) { throw 'Dictionary verification failed' }
$devData = Join-Path $PWD 'build/dev-data'
New-Item -ItemType Directory -Force $devData | Out-Null
Copy-Item MetasequoiaImeDict/out/* $devData -Force
Copy-Item vendor/MetasequoiaImeEngine/helpcode/helpcodes $devData -Recurse -Force
Copy-Item server/assets/tables/* $devData -Force
Copy-Item server/assets/config/config.toml $devData -Force
$env:METASEQUOIA_IME_DATA_DIR = $devData
cd server
python scripts/prepare_env.py
```

This uses the same locked data as CI and a separate development data directory.
Dictionary source changes belong in Engine's `dictionary/`; use its root `build_profile.py`
to build data instead of calling internal stages in the archived Dict repository.

Then, build and run,

```powershell
.\scripts\lcompile.ps1
.\scripts\lrun.ps1
```

If you want to build and run in **one step**, run the following command,

```powershell
.\scripts\llaunch.ps1
```

## Watchdog

`MetasequoiaImeWatchdog.exe` is built next to the server. Starting the server directly also starts the watchdog,
which monitors the server from the same directory and restarts it after an unexpected exit. Repeated early crashes use
an exponential restart delay (up to 30 seconds) to avoid a restart storm.

The developer terminate shortcut stops both processes. The restart shortcut asks the watchdog to start a fresh server
after the current process exits.

## English prefix candidates

Place `english.db` next to `msime.db` in `%LOCALAPPDATA%\metasequoiaime`. Set
`general.cn_en_mixed_input = true` in `config.toml` to enable asynchronous English prefix candidates for Quanpin and
Shuangpin.

## Developer Shortcuts

The server process includes a few built-in global shortcuts that are useful during development:

- `Ctrl + Shift + Alt + T`
  - terminate `MetasequoiaImeServer` immediately
- `Ctrl + Shift + Alt + R`
  - restart `MetasequoiaImeServer`
- `Ctrl + Shift + Alt + C`
  - clear IME engine cache
- `Ctrl + Shift + Alt + 1` to `Ctrl + Shift + Alt + 8`
  - delete candidate `1` to `8` from the current candidate window
  - this only works while the candidate window is visible

## Screenshots

![](https://i.postimg.cc/c402J3KR/image.png)

![](https://i.postimg.cc/v8Bpx6Gf/image.png)

![](https://i.postimg.cc/ssBgtM5M/image.png)

![](https://i.postimg.cc/ryDqXH0B/image.png)

![](https://i.postimg.cc/2m9WJTgR/image.png)

![](https://i.postimg.cc/L96qQZT8/image.png)

![](https://i.postimg.cc/FNcz9QTv/image.png)

## Shared voice providers

The server links Engine VoiceCapture and Voice. WAV encoding and batch transcription/polish codecs come from the public library; Windows keeps provider configuration, SiliconFlow padding/retry behavior and Doubao streaming. Batch uploads are bounded to 20 MiB and responses to 1 MiB. Failed or empty polish responses retain the original transcript.
