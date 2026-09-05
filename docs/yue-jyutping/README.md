# 水杉输入法 · 粤语（粤拼 Jyutping）输入方案设计

> 状态：设计提案（Design Proposal）
> 方案依据：[香港语言学学会粤语拼音方案（粤拼）](https://jyutping.org/jyutping/)，即 `jyutping.org/jyutping/` 所载之现行方案（含 2018 年新增的 `a`、`oet` 等韵母）。
> 本目录同时包含可运行的参考实现（`scheme/`）与数据资产（`data/`），详见文末〈文件清单〉。

---

## 1. 背景与目标

水杉输入法（MSIME）README 明确列出「参与日语、注音、**粤语**等输入方案的设计与实现」为开放贡献方向。本提案为粤语输入给出完整设计：

- **拼写方案**：采用粤拼（Jyutping）——香港语言学学会方案，是目前学术、教育与信息技术界的事实标准（Unicode Unihan 数据库的 `kCantonese` 字段即采用粤拼）。
- **产品定位**：粤语作为与「中文」「日文」并列的第三种**输入模式**（`input.mode = "cantonese"`），其下的拼写方案为粤拼（预留 `input.cantonese_schema` 以容纳未来的耶鲁拼音、教院式等变体）。
- **交付范围**：本文档给出方案表、输入规则、切音算法、跨仓实现计划、词库数据与授权评估、分阶段路线图；并附上基于 Unihan `kCantonese` 构建的音节表（659 个实证音节）与 Python 参考切音器（26 个单元测试全部通过）。

非目标（留待后续提案）：粤语语音输入、粤语口语专属语法模型、方言变体（如懒音）的自动纠正策略细节。

---

## 2. 粤拼方案表（据 jyutping.org 官方页面）

以下表格逐字摘自 <https://jyutping.org/jyutping/>（宽式国际音标转写）。

### 2.1 声母表（19 个）

| 声母 | IPA | 例字 | 声母 | IPA | 例字 |
|---|---|---|---|---|---|
| b | [p] | 巴 | p | [pʰ] | 怕 |
| m | [m] | 妈 | f | [f] | 花 |
| d | [t] | 打 | t | [tʰ] | 他 |
| n | [n] | 那 | l | [l] | 啦 |
| g | [k] | 家 | k | [kʰ] | 卡 |
| ng | [ŋ] | 牙 | h | [h] | 虾 |
| gw | [kʷ] | 瓜 | kw | [kʷʰ] | 夸 |
| w | [w] | 蛙 | z | [t͡s] | 渣 |
| c | [t͡sʰ] | 叉 | s | [s] | 沙 |
| j | [j] | 也 | | | |

**零声母**不用字母标记，例如「呀」只拼作 `aa`。

### 2.2 鼻音单独成韵（2 个）

| 韵 | IPA | 例字 |
|---|---|---|
| m | [m̩] | 唔 |
| ng | [ŋ̩] | 吴 |

> 注意：`m`、`ng` 既是声母也可独立成韵，这是粤拼切分的关键歧义来源（见 §3.3）。

### 2.3 韵母表（现行官页共 60 个韵母，另有 `m`/`ng` 两个鼻音单独成韵）

韵母 = 韵腹 + 韵尾（`-i -u -m -n -ng -p -t -k`）。摘录官方韵母表（行＝韵腹，列＝韵尾）：

| 韵腹 | 单元音 | -i | -u | -m | -n | -ng | -p | -t | -k |
|---|---|---|---|---|---|---|---|---|---|
| i [iː] | i 思 | | iu 消 | im 闪 | in 先 | ing 升 | ip 摄 | it 泄 | ik 识 |
| yu [yː] | yu 书 | | | | yun 孙 | | | yut 雪 | |
| u [uː] | u 夫 | ui 灰 | | um* | un 欢 | ung 风 | up* | ut 阔 | uk 福 |
| e [e] | | ei 四 | | | en* | | | | |
| e [ɛː] | e 些 | | eu 掉 | em 𦧷 | | eng 郑 | ep 夹 | et 坺 | ek 石 |
| eo [ɵ] | | eoi 需 | | | eon 询 | | | eot 摔 | |
| oe [œː] | oe 锯 | | | | | oeng 疆 | | oet* | oek 脚 |
| o [o]/[ɔː] | o 可 | oi 开 | ou 好 | | on 看 | ong 康 | | ot 喝 | ok 学 |
| a [ɐ] | a 嘞 | ai 挤 | au 周 | am 斟 | an 珍 | ang 增 | ap 汁 | at 侄 | ak 则 |
| aa [aː] | aa 渣 | aai 斋 | aau 嘲 | aam 站 | aan 赞 | aang 挣 | aap 集 | aat 扎 | aak 责 |

\* 标星为 2018 年新增或官页未附例字的边际韵母：`a`、`oet` 于 2018 年新增；`um`、`up`、`en` 在官页无例字。`i 后接 -ng/-k` 时韵腹读 [e]（星、识），`u 后接 -ng/-k` 时读 [o]（风、福）——拼写不变，属音位规则，不影响输入法编码。

### 2.4 字调（6 个调号）

| | 平 | 上 | 去 | 入声 |
|---|---|---|---|---|
| 阴 | 1 [˥] 诗 | 2 [˧˥] 史 | 3 [˧] 试 | 1 [˥] 识 / 3 [˧] 洩 |
| 阳 | 4 [˨˩] 时 | 5 [˩˧] 市 | 6 [˨] 事 | 6 [˨] 蚀 |

声调以数字 **1–6** 标于音节之后：`fu1` 夫、`fu2` 虎、`fu3` 副、`fu4` 扶、`fu5` 妇、`fu6` 父。入声韵（`-p -t -k` 韵尾）只配 1/3/6 调，调号写法与舒声相同。

### 2.5 音节规模（实测）

以 Unicode Unihan 数据库 `kCantonese` 字段（29,936 字）统计：

| 指标 | 数值 |
|---|---|
| 有粤拼读音的汉字 | 29,936 |
| 实证基本音节（不带调） | **659** |
| 音节×声调组合 | **1,869** |
| 各调音节数 | 1 调 453、2 调 295、3 调 384、4 调 236、5 调 157、6 调 344 |
| 最长音节 | 6 字母（如 `gwaang`、`kwaang`、`ngaang`） |
| 同音节最多字 | `zi`（387 字）、`ji`（320）、`jyu`（297）、`wai`（288）、`ci`（261） |

完整清单见 `data/jyutping_syllables.json`；逐字读音见 `data/unihan_kcantonese.txt`。
官方方案中合法但 Unihan 未收录的组合（如零声母 `a`、`oet`）已在切音器中作为可选项支持（`allow_unattested`）。

---

## 3. 输入交互设计

### 3.1 编码字符集

| 按键 | 行为 |
|---|---|
| `a`–`z` | 音节字母 |
| `'`（撇号） | 强制音节分界（与全拼一致，沿用 `QuanpinScheme::handle_key` 的既有行为） |
| `1`–`6` | 声调号（仅当「带调输入」开启时；见 §3.4） |
| `Backspace` / `Esc` / `Return` | 与现有方案一致 |

### 3.2 切音算法（粤拼 → 音节序列）

粤拼连写存在大量前缀歧义（实测 476 组「甲音节是乙音节前缀」的情况），例如：

- `gwongzau`（广州）= `gwong` + `zau`，但也可切作 `gwo` + `ng` + `zau`；
- `ngong` 本身是一个音节（12 字），同时可切作 `ngo`+`ng`、`ng`+`ong`；
- `ng` 既是声母（牙 `ngaa4`）又是韵母（吴 `ng4`）。

**算法**（参考实现：`scheme/jyutping_segment.py`；C++ 引擎应 1:1 移植）：

1. 先按 `'` 分段，段内独立切分，结果按笛卡尔积组合；
2. 段内用动态规划枚举所有由**实证音节表**组成的覆盖：
   - 第一优先：**覆盖最多字符**（取最远可达位置）；
   - 第二优先：**音节数最少**（长音节优先，避免 `gwo`+`ng` 胜过 `gwong`）；
   - 第三优先：音节字频权重（`char_count` 截断于 256，仅作平手裁决）；
3. 结尾未能成节的残片若为某音节的前缀，标记为 **pending**（保留在 composition 中，与全拼行为一致）；否则以较重罚分保留，由候选层决定是否显示「无候选」；
4. 声调数字（带调模式）在音节边界处被消费：`jyut6ping3` → `jyut` + `ping`；未实证的「音节+调」组合仍被接受（打字不阻塞），由候选查询自然落空；
5. 输出 top-N 切分（对应引擎 `cut_pinyin_by_mode` 返回多组切分的形态），供候选层做歧义回退。

**测试基线**：`scheme/test_jyutping_segment.py` 26 个用例（含 `jyutping`、`gwongzau`、`hoenggong`、`ng`、`m`、`ngong` 候选排序、pending、带调、非法输入），全部通过。新增/修改切音逻辑必须保持该基线。

### 3.3 `ng` 问题与歧义处理原则

- 音节表驱动：只有实证音节参与切分。`ngaa`（牙）只能切作 `ng`+`aa`，因为 `nga` 并非实证音节；`ngam`（暗）优先于 `n`+`gam`（字频裁决）；
- 用户可用 `'` 显式指定边界：`n'gam`、`ng'am` 等；
- 多候选切分（§3.2 第 5 点）使罕见切法仍可通过翻页/候选项抵达。

### 3.4 声调输入（可选，默认关闭）

新增配置 `input.cantonese_tone`：

| 值 | 行为 |
|---|---|
| `off`（默认） | 数字键沿用现行候选选择行为；不带调检索 |
| `on` | 音节完整后输入 `1`–`6` 附著为声调，候选按调过滤 |

带调模式下的**数字键冲突处理**（重要）：本仓 U 模式已有先例——「裸数字进编码、`Shift+1..9` 选候选」（`src/Key/KeyEventSink.cpp:1357`、`src/Composition/CompositionProcessorEngine.cpp:2046`）。粤语带调模式沿用同一交互：

- 裸数字 `1`–`6`：作为声调附著到当前音节（由切音器判定边界）；
- `Shift+1..9`：选择候选（与 U 模式一致）；
- 数字小键盘（`VK_NUMPAD0..9`）：保留候选选择（TSF 已有此分类，见 `MetasequoiaIMEBaseStructure.cpp:258`），不与声调冲突；
- 音节未完整时输入数字：不附著，回退为候选选择或忽略（由引擎按当前候选状态决定，遵循「Test 与实际处理必须一致」的 TSF 硬约定）。

### 3.5 简拼与快捷模式

- **简拼**：取各音节首字母（零声母音节取其首字母，即元音），如 `zgw` → `zung1 gwok3`。超级简拼（`Shift+J`）直接复用，引擎侧把「声母」概念换成「音节首字母」即可。
- **以词定字、快捷短语（K）、日期（T）、Unicode（U）、Emoji（E）、颜文字（M）**：模式无关，全部复用。
- **临时粤语**：暂不需要（粤语本身即独立模式）。

### 3.6 预编辑（preedit）显示

复用 `src/Global/FanyDefines.h` 的 `raw` / `pinyin`（分词）/ `empty` 三态，无需新增样式枚举：

- 原始按键：`jyutping`（含用户输入的 `'` 与调号原样）；
- 分词样式：`jyut'ping`（或 `jyut6'ping3`，若带调）；
- 候选窗内预编辑同上。

### 3.7 繁简与标点

- 粤语模式**默认繁体输出**（粤语书写以繁体/传承字形为主），但跟随现行 `input.character_set` 设置，允许用户显式改为简体；建议模式切换时不覆盖用户已有的 `character_set` 选择。
- 标点沿用中文标点集与智能标点规则；粤语专用标点不在 P0 范围。

---

## 4. 跨仓实现计划（遵循根目录 `AGENTS.md` 边界）

> 产品级硬约定以本仓 `AGENTS.md` 为准。下列改动分布在多个独立仓库，**任何协议/表名/配置改动必须双端同步**。

### 4.1 引擎（`MSIME-Engine`）——核心

```
core/scheme_type.h            enum class SchemeType 追加 CantoneseJyutping（只追加，不重排）
schemes/cantonese_jyutping_scheme.{h,cpp}   IInputScheme 实现（仿 japanese_romaji_scheme）
cantonese/jyutping_syllabary.h   生成的音节表（由 data/ 产出，编译期常量）
cantonese/jyutping_utils.{h,cpp} 切音 DP（移植 scheme/jyutping_segment.py）
cantonese/cantonese_query.{h,cpp} build_table_name（见 §5.2 分表规则）
providers/cantonese_candidate_provider.{h,cpp} 候选查询（仿 japanese_candidate_provider）
providers/provider_registry.cpp  resolve()/reset_cache() 追加 case
```

`QueryRequest` 无需改结构：`normalized_segmentation` 放 `'` 连接的音节序列；带调时调号保留在 `raw_input`，由 provider 侧解析过滤。

### 4.2 服务端（`MSIME-Server`）

| 位置 | 改动 |
|---|---|
| `src/config/ime_config.cpp` | `input.mode` 接受 `"cantonese"`；`GetConfiguredActiveInputScheme()` 在 cantonese 时返回 `SchemeType::CantoneseJyutping`；新增 `input.cantonese_schema`、`input.cantonese_tone` 读写（仿 `SetConfiguredInputScheme` 的白名单写法） |
| `src/session/session_factory.cpp` | cantonese → `EngineInputSession(SchemeType::CantoneseJyutping)` |
| `src/session/engine_input_session.cpp` | 粤语模式下禁用全拼/双拼专用路径（helpcode、autocorrect），与 `Wubi`/`JapaneseRomaji` 同级处理 |
| `src/ipc/ipc.h` | **只追加**新 opcode。现行 `DataToTsfWorkerThreadMsgType::InputModeChanged = 18`（payload `"1"`=日文）、`MaxKnown = PunctuationLockChanged (21)`。新增 `CantoneseInputModeChanged = 22`（payload `"0"/"1"`）并把 `MaxKnown` 指向新值；已发布值 0–21 一律不动 |
| `src/ipc/event_listener.cpp` | 带调模式下数字键归属判定（§3.4）；粤语模式下跳过全拼专用的云联想/AI 联想前置条件（参考 `non_pinyin` 判断处） |
| `src/webview2/windows_webview2.cpp` | `input.mode` 变更的 WebView2 同步路径补 `"cantonese"` 分支 |

### 4.3 TSF 前端（本仓 `MSIME-Windows`）

改动力求最小——按键判定主要依赖 Server 回复，本地仅需知道「是否粤语模式」：

| 位置 | 改动 |
|---|---|
| `src/Utils/FanyUtils.{h,cpp}` | `ReadConfiguredJapaneseInputMode()` 推广为 `ReadConfiguredInputModeKind()`（返回 chinese/japanese/cantonese 三态），保留旧函数作包装以免破坏既有调用 |
| `src/IPC/Ipc.h` | 与 Server 端**同步**追加 `CantoneseInputModeChanged = 22` 消息类型、`Global::CantoneseInputModeEnabled` 原子标志，并同步 `MaxKnown`。现行日文标志名为 `Global::JapaneseInputModeEnabled`（消息 `InputModeChanged = 18`），两者结构体/枚举布局不动 |
| `src/IME/MetasequoiaIME.cpp` | Worker 消息分发追加新 msg_type（仿现有 `InputModeChanged` 分支写 `Global::JapaneseInputModeEnabled` 的模式） |
| `src/LanguageBar/LanguageBar.cpp` + `.rc` + 资源 | 语言栏新增粤语图标（浅色/深色成对，同步资源 ID、`.rc`、`CMakeLists.txt` 与索引，遵循资源注册约定） |
| `src/Key/KeyEventSink.cpp` | 带调模式下数字键 `_IsKeyEaten` 判定（必须与 `_DispatchKeyDown` 实际分支一致，含离线 fallback；参考 U 模式既有注释） |

**不得改动**：CLSID/profile GUID/compartment GUID（持久身份）；`DllMain`；IPC 既有结构体布局（只追加消息类型，不改结构体）。

### 4.4 设置前端（`MSIME-UiHtml` → `webview2/settings/ime-settings/`）

- 输入模式下拉框追加「粤语（粤拼）」选项（值 `"cantonese"`）；
- 新增「带调输入」开关（`input.cantonese_tone`），默认关；
- 候选窗、悬浮工具栏无需结构改动（内容权威在 Server）。

### 4.5 词库（`MSIME-Dict` / `MSIME-Server` 资产）

见 §5。词库资产由 Server 加载，TSF 不解析词库（AGENTS.md 边界）。

### 4.6 安装（`Installer`）

`Prepare-PackageFiles.ps1` 收集词库时加入粤语词库文件；升级版本目录时同步检查。

---

## 5. 词库数据

### 5.1 数据来源与授权评估

| 来源 | 内容 | 授权 | 结论 |
|---|---|---|---|
| Unicode Unihan DB `kCantonese` | 29,936 字 × 粤拼（含调） | Unicode 数据授权（宽松，需署名） | ✅ 本提案已使用；发布时需在词库文档中保留署名与修改声明 |
| 粤语审音配词字库（CUHK） | 权威审音 | 学术用途，不可再分发 | ❌ 仅供人工校对参考 |
| rime-cantonese | 字表+词表（粤拼） | LGPL-3.0（需核实当前版本） | ⚠️ 需与 GPL-3.0 兼容性核实后使用 |
| words.hk 粤典 | 词汇+粤拼+释义 | 待核实（疑似含 NC 条款） | ⚠️ 若含非商业条款，不得并入分发包 |
| 社区贡献（README 欢迎词库 PR） | 行业/地区/兴趣词库 | 贡献者声明 | ✅ 长期路线 |

**P0 最小可用集**：Unihan 单字表 + 人工整理的高频词条（建议首批 5,000–20,000 条常用粤语词，以粤拼编码、Tab 分隔「词语、编码、权重」三列，与现有批量导入格式一致）。

### 5.2 表结构与分表规则

为避免与全拼主库 `msime.db` 的升级回放硬约定（`AGENTS.md` §全拼音库分表命名）耦合，粤语词库建议**独立成库** `msime_cantonese.db`，但分表算法与全拼保持一致：

| 音节数（字数） | 表名 | 示例 |
|---|---|---|
| 1–7 | `yue_tbl_{N}_{首字母}` | `nei'hou` → `yue_tbl_2_n` |
| ≥ 8 | `yue_tbl_others_{首字母}` | 同全拼规则，禁止 `yue_tbl_8_*` |

另设单字表 `yue_single(code, value, weight)`（`code` 为带调音节如 `jyut6`；查询时对不带调输入做前缀匹配，与 `japanese_lexicon` 的 `code LIKE ? ESCAPE '#'` 模式一致）。

> ⚠️ 决策点：若维护者倾向把粤语表并入 `msime.db`（如 `japanese_lexicon` 的先例），则建库脚本、`build_table_name`、设置页加词、`user_dictionary_journal` 回放四处必须同步（AGENTS.md 明列），且需在回放测试中覆盖粤语表名。两种取态均需维护者拍板；本设计推荐独立库以降低回放风险。

### 5.3 建库脚本（建议落位 `MSIME-Dict/makecikudb/yuedb/`）

```
create_db_and_table.py    建库建表（规则同 §5.2）
import_unihan_single.py   由 data/unihan_kcantonese.txt 生成单字表（本目录已附原型数据）
import_phrases.py         三列 TSV → 分表（首字母 = 第一音节首字符）
```

权重策略：P0 用 Unihan 字频近似 + 人工词频；P2 接入粤语语料 n-gram（参考 `Metasequoia-n-gram` 仓）。

---

## 6. 分阶段路线图

| 阶段 | 内容 | 验收 |
|---|---|---|
| **P0 MVP** | 引擎方案+切音+单字候选；`input.mode="cantonese"` 全链路；设置页选项；语言栏图标 | 能以粤拼输入任意 Unihan 收录汉字；本文档 §3.2 测试基线通过 |
| **P1** | 词表与词组输入、简拼、加词（`create_word`）、独立词库打包 | 常用 5,000 词命中率 ≥ 90%（抽样） |
| **P2** | 带调输入与调号过滤、调频权重、懒音/模糊选项、用户词库回放 | 带调回归测试；升级回放不失败 |
| **P3** | n-gram 联想、云候选与 AI 联想接入（沿用现有管道）、粤语专用标点/口语词库 | 与全拼同等功能面 |

每阶段均须执行 `AGENTS.md` §构建与验证 的手动回归清单（x64/x86、首键/连打/选词/翻页/失焦回焦/断管恢复、Excel/Chromium/UWP 宿主）。

---

## 7. 测试与验收

- **引擎单测**（`MSIME-Engine/tests/src/test_cantonese.cpp`，仿 `test_pinyin.cpp`）：把 `scheme/test_jyutping_segment.py` 的用例移植为 C++ 向量；另加带调、`'` 边界、`ng` 歧义、空输入、超长输入（≥ 64 字符）用例。
- **IPC 协议测试**：追加新消息类型常量后，运行 `tests/src/test_ipc_protocol_constants.cpp` 所属目标（AGENTS.md 要求）。
- **回放测试**：若采用 §5.2 决策，需补粤语表名的升级回放用例。
- **手动回归**：见 §6 各阶段验收栏。

---

## 8. 文件清单（本目录）

```
docs/yue-jyutping/
├── README.md                        ← 本文档
├── scheme/
│   ├── jyutping_segment.py          ← 参考切音器（C++ 移植基准）
│   └── test_jyutping_segment.py     ← 26 个基线测试（全部通过）
└── data/
    ├── jyutping_syllables.json      ← 659 个实证音节（含调、字数、例字）
    └── unihan_kcantonese.txt        ← 29,936 字的粤拼读音（Unihan kCantonese）
```

### 数据来源声明

`data/unihan_kcantonese.txt` 与 `data/jyutping_syllables.json` 派生自 Unicode® Unihan 数据库（<https://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip>）的 `kCantonese` 字段。
Copyright © 1991–2025 Unicode, Inc. 依 Unicode 数据授权条款使用；派生数据可能含修订，使用时请一并保留本声明。

### 参考

- 粤拼方案（官方页）：<https://jyutping.org/jyutping/>
- 香港语言学学会粤拼页：<https://lshk.org/jyutping-scheme/>
- 粤语审音配词字库：<https://humanum.arts.cuhk.edu.hk/Lexis/lexi-mf/>
- Unihan 数据库：<https://www.unicode.org/charts/unihan.html>
- rime-cantonese：<https://github.com/rime/rime-cantonese>
