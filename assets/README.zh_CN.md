<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

可复用的字库文件与生成的字库源码放在 `fonts/`。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

### 侨批填字问答

应用内置三档未压缩的 LVGL 点阵子集，保证读者能看到的每一个字都由我们可控的字库绘制：

| 文件 | 尺寸与格式 | 用途 |
| --- | --- | --- |
| [`fonts/qpq_font_16.c`](fonts/qpq_font_16.c) | 16 px、4bpp、未压缩 LVGL 点阵 | 底部提示条、列表行右侧的状态注，以及「参」这一层上方的小标签。 |
| [`fonts/qpq_font_24.c`](fonts/qpq_font_24.c) | 24 px、4bpp、未压缩 LVGL 点阵 | 顶栏标题、点拨正文、列表行文字与三个选取项。 |
| [`fonts/qpq_font_32.c`](fonts/qpq_font_32.c) | 32 px、4bpp、未压缩 LVGL 点阵 | 原文——一屏一句。 |
| [`fonts/charset.txt`](fonts/charset.txt) | UTF-8 文本 | 三档共用的 1303 个码点清单，便于在不打开生成代码的前提下复核子集内容。 |

三档子集来自同一份母字体：

- **来源**：Noto Sans CJK SC Regular（`NotoSansCJKsc-Regular.otf`，16,437,364 字节，
  SHA-256 `2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`），取自
  [`notofonts/noto-cjk`](https://github.com/notofonts/noto-cjk)。
- **许可**：SIL Open Font License 1.1。允许子集化与再分发，前提是许可文本与版权声明
  随字体一同提供。
- **字符范围**：1303 个码点——其中 1079 个汉字取自题库与界面文案，另 224 个
  是可打印 ASCII、空格，以及界面自绘的中文标点。这里刻意**不**用 LVGL 内置 Montserrat
  兜底：子集里缺的码点会渲染成空白框，所以生成脚本在转换前会逐个码点核对母字体的 `cmap`。
- **转换工具**：`lv_font_conv` 1.5.3，参数 `--bpp 4 --no-compress --format lvgl`。
- **重新生成（在仓库根目录执行）：**

  ```sh
  python3 tools/qiaopi/gen_font.py          # 重建三档字库
  python3 tools/qiaopi/gen_font.py --check  # 只校验清单与覆盖，不转换
  ```

  两种方式都会读取 `tools/qiaopi/chapters/*.txt` 与 `main/*.c|*.h` 里的字符串字面量，
  也就是说**新增界面文案或新增一章后必须重新跑生成脚本**。`--check` 的作用是把「漏字」变成
  构建期错误，而不是设备上的一块空白。
- **目标放置路径**：由 `main/CMakeLists.txt` 的 `target_sources` 编入 `main` 组件；使用时用
  `LV_FONT_DECLARE` 声明，并按控件逐个指定。`sdkconfig.defaults` 里的
  `CONFIG_LV_FONT_FMT_TXT_LARGE=y` 是必需的——LVGL 默认的文本格式字库用 16 位存放字形位图
  偏移量，而 24 px 与 32 px 两档分别携带约 114 KB 与 203 KB 位图数据（1303 个字形，每个
  288 与 512 字节），都超出了 64 KB 的字段上限。16 px 那档约 51 KB 本来放得下，但这个开关
  是按字体格式生效的，所以三档一起开。
- **生成的源码体积（实测）**：293,734 / 565,633 / 920,969 字节。十六进制字面量的源码开销
  是位图的数倍，所以目录列表会高估 Flash 占用——要看体积报告，不要看 `ls`。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

| 文件 | 尺寸与格式 | 用途与来源 |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160，JPEG | 嵌入中英文项目 README 的产品主图，突出 AI Passport 产品形象与开放、人人可创作的理念。 |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724，PNG RGBA | 保留为可选技术参考图，不再用于首页主视觉。于 2026-09-17 使用内置图像生成工具为本仓库生成；已根据文档中的硬件能力契约核对图中的六项标签与参数。 |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336，PNG RGBA | 从仓库原始 `images/logo.png` 中精确裁切并去除背景的黑色字标；用于中英文项目 README 的浅色主题。 |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336，PNG RGBA | 提取字标的白色版本；README 使用 `<picture>` 在 GitHub 深色主题下显示。 |

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。
