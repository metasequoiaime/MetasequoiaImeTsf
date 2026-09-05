# Changelog

## [0.1.2](https://github.com/metasequoiaime/MSIME-Windows/compare/v0.1.1...v0.1.2) (2026-09-05)


### Bug Fixes

* **ci:** match native Windows job names to the required checks ([1e7632b](https://github.com/metasequoiaime/MSIME-Windows/commit/1e7632b564f272c7b1e356fbe4b15fb53c73e42c))
* connect Windows monorepo to consolidated Engine voice and data ([b690980](https://github.com/metasequoiaime/MSIME-Windows/commit/b690980ab80e3f8619f15546c95276dc9205b964))
* finish local monorepo packaging and required Windows checks ([e1946bb](https://github.com/metasequoiaime/MSIME-Windows/commit/e1946bbec8c9ad2003ba3a64a7f8777f967be08a))
* **installer:** use Engine tables in the local monorepo layout ([5f8d75d](https://github.com/metasequoiaime/MSIME-Windows/commit/5f8d75d79e3d6fd3ae2e2cb4008594fa91b6e705))

## [0.1.1](https://github.com/metasequoiaime/MSIME-Windows/compare/v0.1.0...v0.1.1) (2026-09-05)


### Bug Fixes

* **ci:** check out the engine submodule in the product inputs job ([6bb0c1e](https://github.com/metasequoiaime/MSIME-Windows/commit/6bb0c1e10ec9fdf5a59f5793f0392c506641dba9))
* **ci:** persist vcpkg binaries before application builds ([947aa16](https://github.com/metasequoiaime/MSIME-Windows/commit/947aa16fcbd7f13d171bbd459c0a73e9471c4eeb))
* **ci:** resolve the pipe probe contracts from the repository, not the server ([b48dc82](https://github.com/metasequoiaime/MSIME-Windows/commit/b48dc826526fdb41e268f92d60bf49530d00291d))
* **server:** always answer explicit settings snapshot requests ([9a9c6f3](https://github.com/metasequoiaime/MSIME-Windows/commit/9a9c6f328f7e05949bfda35ab7f9e70e479ee6da))
* **server:** keep settings I/O off UI thread and bound dictionary rendering ([06bba15](https://github.com/metasequoiaime/MSIME-Windows/commit/06bba15d4f2738d3e93c34819c7bad8813a137c5))
* **server:** point the panel resources at the ui component ([ecc1bf4](https://github.com/metasequoiaime/MSIME-Windows/commit/ecc1bf4f6bae9b7e9e385af67743c153bf12f7f3))
* **server:** reduce WebView message and first-paint overhead ([b5d07f1](https://github.com/metasequoiaime/MSIME-Windows/commit/b5d07f11eb4fe3e98aec48c84f7e0870f24ea42f))

## [0.1.0](https://github.com/metasequoiaime/MSIME-Windows/compare/v0.0.17...v0.1.0) (2026-09-05)


### Features

* also sync double/single char status when switch windows ([7890e81](https://github.com/metasequoiaime/MSIME-Windows/commit/7890e815de1370a8cb969c32cf400a2cf6a00cbf))
* **langbar:** switch IME mode icons by system light/dark theme ([0b00c00](https://github.com/metasequoiaime/MSIME-Windows/commit/0b00c0042b6664b17e1d05a46128c7051b449b14))
* notify UI process on punctuation mode change ([d299f53](https://github.com/metasequoiaime/MSIME-Windows/commit/d299f5328661be7c998163b622d979b732b2c4bc))
* **product:** keep WebView and native contracts in one locked combination ([9b28445](https://github.com/metasequoiaime/MSIME-Windows/commit/9b2844534ef796cfaf90cbc4b62e7c2849e4275f))
* **product:** lock release inputs and share negotiated IPC contracts ([487011c](https://github.com/metasequoiaime/MSIME-Windows/commit/487011c80269322127629c6e878653ee61726431))
* **product:** lock the shared-session stack and validate dictionary formats ([1c6c956](https://github.com/metasequoiaime/MSIME-Windows/commit/1c6c95653070c931e00783dd87e63b0a1d5ed5b5))
* **product:** require locked commits to be on their default branch at release ([a81bc47](https://github.com/metasequoiaime/MSIME-Windows/commit/a81bc47c719228c93bf2dfcad1d1c55511e06346))
* **product:** validate and lock the shared-session release combination ([e08f434](https://github.com/metasequoiaime/MSIME-Windows/commit/e08f434e311b55668c5dcd7951563a01a65c102a))
* set max len limit for pinyin input ([bde777f](https://github.com/metasequoiaime/MSIME-Windows/commit/bde777f359c22bbc9f000dc1d1760d23029b5aa2))
* support exact candidate character commits ([5e36ba1](https://github.com/metasequoiaime/MSIME-Windows/commit/5e36ba1d01f229214be543c1d35a4748275868ed))
* sync IME status to UI on thread focus and punctuation switch ([55dae98](https://github.com/metasequoiaime/MSIME-Windows/commit/55dae980691b5754ba56acfe61a5591ab94e9bb9))
* synchronous tsf and server double/single char status via ipc ([25ea1ef](https://github.com/metasequoiaime/MSIME-Windows/commit/25ea1efe39f5b90015e036df4532f10fb260fa53))


### Bug Fixes

* **ci:** build the Server in build/, the only directory name it links in ([ca0a20b](https://github.com/metasequoiaime/MSIME-Windows/commit/ca0a20b50e9f65bb43be97940a334a5944322ec5))
* **ci:** build the Server in build/, the only directory name it links in ([053c987](https://github.com/metasequoiaime/MSIME-Windows/commit/053c987af3b629a6d91d97c9ccd0f5d1e85236f4))
* **ci:** find the release pull request by branch, not by prs_created ([0b9b272](https://github.com/metasequoiaime/MSIME-Windows/commit/0b9b2728befbfe5afe08668944b31c8e6fd5504b))
* **ci:** find the release pull request by branch, not by prs_created ([550e0a1](https://github.com/metasequoiaime/MSIME-Windows/commit/550e0a1c70b675184efab104942c1c4bcf1e7ab6))
* **ci:** install the Chinese language file Inno Setup needs ([22a132e](https://github.com/metasequoiaime/MSIME-Windows/commit/22a132e3bd4d48c6185176c0855debdc3a1cfd73))
* **ci:** supply the Inno Setup language file, and move release shell into scripts/ci ([0761b41](https://github.com/metasequoiaime/MSIME-Windows/commit/0761b410e378ac71ab1c6239b5c2cd7db81dab61))
* **ci:** take the release scripts from the default branch, not the built commit ([ca87304](https://github.com/metasequoiaime/MSIME-Windows/commit/ca87304f0df71f0aacde4c0ff4a3ab6db6547669))
* **ci:** take the release scripts from the default branch, not the built commit ([e90ac18](https://github.com/metasequoiaime/MSIME-Windows/commit/e90ac18723a119ff7217f3ce84f2902b0ca7a89d))
* **ci:** verify the version resource at the source, not on the built DLL ([aca9c8a](https://github.com/metasequoiaime/MSIME-Windows/commit/aca9c8a31debde83bd50583c054cefd7f0914148))
* **ci:** verify the version resource at the source, not on the built DLL ([2b17384](https://github.com/metasequoiaime/MSIME-Windows/commit/2b173841f59402b1d3f5abdfb818369bd72537a3))
* **composition:** deduplicate consecutive pinyin separators ([ccedf77](https://github.com/metasequoiaime/MSIME-Windows/commit/ccedf777602f567ac88cfd750bb7ebcc7f77fe49))
* **image:** add small icon frames to msime.ico ([facfe60](https://github.com/metasequoiaime/MSIME-Windows/commit/facfe60761c4f3b23e2ab58cbfa66d25385dcee0))
* **image:** add small icon frames to msime.ico ([f6a4567](https://github.com/metasequoiaime/MSIME-Windows/commit/f6a4567b5e7fcf58454d394a12aed9ba3a20e9ae))
* **ime:** defer CN→EN close until after Shift composition commit ([79b3155](https://github.com/metasequoiaime/MSIME-Windows/commit/79b31550994965144093740c3faf5aee9ae8c787))
* **ime:** keep first-key candidate window when entering Excel cell edit, do not suspend client on transient focus loss during Excel cell edit. ([0c95aa2](https://github.com/metasequoiaime/MSIME-Windows/commit/0c95aa272a46fe86270ef7f8c0072e89a3e233c7))
* **ime:** keep numpad decimal as '.' in Chinese punctuation mode ([4ea2386](https://github.com/metasequoiaime/MSIME-Windows/commit/4ea238661128e913a64c5164167860d7e92f980a))
* **ime:** launch Server off the UI thread and wake it on TIP activation ([881dba5](https://github.com/metasequoiaime/MSIME-Windows/commit/881dba501f2d1bb0ebf7cac836c9952c3e4497e4))
* **langbar:** refresh theme icons immediately on system theme change ([34ff194](https://github.com/metasequoiaime/MSIME-Windows/commit/34ff19482d8991c0a175d4a82d94c5d46d1f334f))
* make debugview-like 32-bit apps get proper coordinates for caret ([315f202](https://github.com/metasequoiaime/MSIME-Windows/commit/315f202195da0dca60bf6cd5436cf976e6c9474a))
* **product:** pin Server with corrected subproject test registration ([1f126a1](https://github.com/metasequoiaime/MSIME-Windows/commit/1f126a193a428c393c6f06bb1d7325b622098fff))
* **punctuation:** let the book title nest pair actually run ([b2d5c55](https://github.com/metasequoiaime/MSIME-Windows/commit/b2d5c55971b0a9635a44d60ea6ff104c8257cab0))
* **release:** install the Inno Setup language file where ISCC looks ([7c6dc8b](https://github.com/metasequoiaime/MSIME-Windows/commit/7c6dc8b6a06b170282aeff4e4e37f754171e3b31))
* **release:** 语言文件装到真正的 Inno Setup 目录,修掉八个版本都卡住的打包失败 ([a3610b7](https://github.com/metasequoiaime/MSIME-Windows/commit/a3610b7180f856246db94932613574ca8109ed25))
* reliably notify server when switching away from IME ([87ba31f](https://github.com/metasequoiaime/MSIME-Windows/commit/87ba31f2ec0d7c71947b5f033242beaacb93a60a))
* reset pipe handle in some cases, to ensure reconnect ([9512ee4](https://github.com/metasequoiaime/MSIME-Windows/commit/9512ee4a7ece3c4e18226010b4ca148a62a2573b))
* **scripts:** read the vcpkg root from the environment ([0259f7a](https://github.com/metasequoiaime/MSIME-Windows/commit/0259f7a9c57d29445f91896662c31f616e0ab7b0))
* **scripts:** read vcpkg and boost roots from the environment ([f9e52d9](https://github.com/metasequoiaime/MSIME-Windows/commit/f9e52d98a84b99b36800aff6ff55ecd71b2b26b9))
* show langbar menu via session-less Aux pipe ([30c0942](https://github.com/metasequoiaime/MSIME-Windows/commit/30c09425c520170d594c62758d4717503a5881df))
* **tsf:** commit the create-word prefix when Enter finalizes the composition ([75d8366](https://github.com/metasequoiaime/MSIME-Windows/commit/75d83660cd5b41be32e3089936fd22f5bd3a91b8))
* **tsf:** commit the create-word prefix when Enter finalizes the composition ([634b6ce](https://github.com/metasequoiaime/MSIME-Windows/commit/634b6ceaef48e87fd46349bcf371801fa2f5e340))
* **tsf:** convert candidate anchors to physical coordinates ([089e47b](https://github.com/metasequoiaime/MSIME-Windows/commit/089e47b60cd12cf3f876cd89dd1f19fdf81612ee))
* **tsf:** let the host see the bare Ctrl release that toggles input mode ([a13ae2f](https://github.com/metasequoiaime/MSIME-Windows/commit/a13ae2fdbee620f2280e0d21cae3bb632d87b16a))
* **tsf:** let the host see the bare Ctrl release that toggles input mode ([3854331](https://github.com/metasequoiaime/MSIME-Windows/commit/38543317a2aadd94f15f8ee68e95b1e83f203976))
* **tsf:** pass application modifier shortcuts through ([4b1696c](https://github.com/metasequoiaime/MSIME-Windows/commit/4b1696ced320360c8490d453ca407145b4ed0f4e))
* **tsf:** pass application modifier shortcuts through in Chinese mode ([469aefe](https://github.com/metasequoiaime/MSIME-Windows/commit/469aefed86c9cdf203217bccc69c91abcd26762d))
* **tsf:** place candidate window with per-monitor DPI coordinates ([a6f94de](https://github.com/metasequoiaime/MSIME-Windows/commit/a6f94debbaf3ffc7c18419f620a6c8c2156be669))
* **tsf:** support Shift language switching in mintty (refs [#32](https://github.com/metasequoiaime/MSIME-Windows/issues/32)) ([99cbc17](https://github.com/metasequoiaime/MSIME-Windows/commit/99cbc172e1555bc616627797258a7e3fa1d6c9ee))
* Wrong interface conversion (ITfcompositionSink) ([27e7110](https://github.com/metasequoiaime/MSIME-Windows/commit/27e7110f047b9eb859a8ad2c956c6700a70aa844))


### Reverts

* leave the unused boost_path alone ([a7a3fc4](https://github.com/metasequoiaime/MSIME-Windows/commit/a7a3fc42b53d852910640fdd21e1cd5b4dfae812))
* move back off calendar versioning ([07d9ce9](https://github.com/metasequoiaime/MSIME-Windows/commit/07d9ce91e5d04d9db91e22bd7eaa80070f6d37ef))

## [0.0.17](https://github.com/metasequoiaime/MSIME-Windows/compare/v0.0.16...v0.0.17) (2026-09-05)


### Bug Fixes

* **ci:** find the release pull request by branch, not by prs_created ([0b9b272](https://github.com/metasequoiaime/MSIME-Windows/commit/0b9b2728befbfe5afe08668944b31c8e6fd5504b))
* **ci:** find the release pull request by branch, not by prs_created ([550e0a1](https://github.com/metasequoiaime/MSIME-Windows/commit/550e0a1c70b675184efab104942c1c4bcf1e7ab6))

## [0.0.16](https://github.com/metasequoiaime/MSIME-Windows/compare/v0.0.15...v0.0.16) (2026-09-05)


### Bug Fixes

* **tsf:** commit the create-word prefix when Enter finalizes the composition ([75d8366](https://github.com/metasequoiaime/MSIME-Windows/commit/75d83660cd5b41be32e3089936fd22f5bd3a91b8))

## [0.0.15](https://github.com/metasequoiaime/MSIME-Windows/compare/v0.0.14...v0.0.15) (2026-09-05)


### Bug Fixes

* **ci:** take the release scripts from the default branch, not the built commit ([ca87304](https://github.com/metasequoiaime/MSIME-Windows/commit/ca87304f0df71f0aacde4c0ff4a3ab6db6547669))

## [0.0.14](https://github.com/metasequoiaime/MSIME-Windows/compare/v0.0.13...v0.0.14) (2026-09-05)


### Bug Fixes

* **image:** add small icon frames to msime.ico ([facfe60](https://github.com/metasequoiaime/MSIME-Windows/commit/facfe60761c4f3b23e2ab58cbfa66d25385dcee0))
* **image:** add small icon frames to msime.ico ([f6a4567](https://github.com/metasequoiaime/MSIME-Windows/commit/f6a4567b5e7fcf58454d394a12aed9ba3a20e9ae))

## [0.0.13](https://github.com/metasequoiaime/MSIME-Windows/compare/v0.0.12...v0.0.13) (2026-09-05)


### Bug Fixes

* **ci:** install the Chinese language file Inno Setup needs ([22a132e](https://github.com/metasequoiaime/MSIME-Windows/commit/22a132e3bd4d48c6185176c0855debdc3a1cfd73))
* **ci:** supply the Inno Setup language file, and move release shell into scripts/ci ([0761b41](https://github.com/metasequoiaime/MSIME-Windows/commit/0761b410e378ac71ab1c6239b5c2cd7db81dab61))

## [0.0.12](https://github.com/metasequoiaime/MSIME-Windows/compare/v0.0.11...v0.0.12) (2026-09-05)


### Bug Fixes

* **scripts:** read the vcpkg root from the environment ([0259f7a](https://github.com/metasequoiaime/MSIME-Windows/commit/0259f7a9c57d29445f91896662c31f616e0ab7b0))

## [0.0.11](https://github.com/metasequoiaime/MSIME-Windows/compare/v0.0.10...v0.0.11) (2026-09-05)


### Bug Fixes

* **ci:** build the Server in build/, the only directory name it links in ([ca0a20b](https://github.com/metasequoiaime/MSIME-Windows/commit/ca0a20b50e9f65bb43be97940a334a5944322ec5))
* **ci:** build the Server in build/, the only directory name it links in ([053c987](https://github.com/metasequoiaime/MSIME-Windows/commit/053c987af3b629a6d91d97c9ccd0f5d1e85236f4))
* **ci:** verify the version resource at the source, not on the built DLL ([aca9c8a](https://github.com/metasequoiaime/MSIME-Windows/commit/aca9c8a31debde83bd50583c054cefd7f0914148))
* **ci:** verify the version resource at the source, not on the built DLL ([2b17384](https://github.com/metasequoiaime/MSIME-Windows/commit/2b173841f59402b1d3f5abdfb818369bd72537a3))

## [0.0.10](https://github.com/metasequoiaime/MSIME-Windows/compare/v0.0.9...v0.0.10) (2026-09-05)


### Bug Fixes

* **tsf:** convert candidate anchors to physical coordinates ([089e47b](https://github.com/metasequoiaime/MSIME-Windows/commit/089e47b60cd12cf3f876cd89dd1f19fdf81612ee))
* **tsf:** pass application modifier shortcuts through ([4b1696c](https://github.com/metasequoiaime/MSIME-Windows/commit/4b1696ced320360c8490d453ca407145b4ed0f4e))
* **tsf:** pass application modifier shortcuts through in Chinese mode ([469aefe](https://github.com/metasequoiaime/MSIME-Windows/commit/469aefed86c9cdf203217bccc69c91abcd26762d))
* **tsf:** place candidate window with per-monitor DPI coordinates ([a6f94de](https://github.com/metasequoiaime/MSIME-Windows/commit/a6f94debbaf3ffc7c18419f620a6c8c2156be669))
