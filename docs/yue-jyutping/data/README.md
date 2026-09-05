# 数据资产说明

| 文件 | 内容 | 来源 |
|---|---|---|
| `unihan_kcantonese.txt` | 29,936 个汉字的粤拼读音，格式 `U+码位<TAB>读音1 读音2 …`（带调号 1–6） | Unicode Unihan 数据库 `kCantonese` 字段 |
| `jyutping_syllables.json` | 659 个实证音节：可用声调、声母/韵母分析、字数、例字 | 由上表统计生成 |
| `regen_unihan_data.py` | 重新生成以上两个文件的脚本（需要网络） | — |

## 来源与授权

Unihan 数据库下载自 <https://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip>。

> Copyright © 1991–2025 Unicode, Inc. 本目录的派生数据依 Unicode 数据授权条款
> （Unicode Data License）使用；如对数据做过修订，发布时请一并保留本声明并注明修改。

`kCantonese` 字段的拼写遵循香港语言学学会粤语拼音方案（粤拼），
见 <https://jyutping.org/jyutping/>。

## 重新生成

```powershell
python .\regen_unihan_data.py
```

生成后请运行 `..\scheme\test_jyutping_segment.py` 确认音节表基线（659 个实证音节）。
若官方数据库更新导致音节数变化，需同步更新设计文档 §2.5 的统计数字。
