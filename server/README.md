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

**First**, clone the product repository,

```powershell
git clone --recursive https://github.com/metasequoiaime/MSIME-Windows.git
```

**Then**, put the dictionary data where the engine reads it. It is not built locally any more: the
product lock names a published release and records the SHA256 of every asset, which is the same
data Windows, macOS and Linux all ship, so building your own would only produce something the
product never runs against.

```powershell
python .\MSIME-Windows\scripts\product_lock.py fetch-dictionaries --staging-root $env:TEMP\msime-data
Copy-Item -Path "$env:TEMP\msime-data\MetasequoiaImeDict\out\*" -Destination $env:LOCALAPPDATA\metasequoiaime
Copy-Item -Path .\MSIME-Windows\vendor\MetasequoiaImeEngine\helpcode\helpcodes -Destination $env:LOCALAPPDATA\metasequoiaime -Recurse
```

Prepare environment,

```powershell
cd MSIME-Windows/server
python .\scripts\prepare_env.py
Copy-Item -Path .\assets\tables\* -Destination $env:LOCALAPPDATA\metasequoiaime
New-Item -ItemType SymbolicLink -Path "$env:LOCALAPPDATA\metasequoiaime\config.toml" -Target ".\assets\config\config.toml"
```

e.g.

```powershell
cd MSIME-Windows\server
python .\scripts\prepare_env.py
Copy-Item -Path .\assets\tables\* -Destination $env:LOCALAPPDATA\metasequoiaime
New-Item -ItemType SymbolicLink -Path "$env:LOCALAPPDATA\metasequoiaime\config.toml" -Target (Resolve-Path .\assets\config\config.toml)
```

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
