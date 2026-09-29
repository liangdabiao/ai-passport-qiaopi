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
| [`fonts/qpq_font_16.c`](fonts/qpq_font_16.c) | 16 px、4bpp、未压缩 LVGL 点阵 | 顶栏标题与计数、底部提示条、列表行右侧的注、判卷页的小标题与出处行，以及标题页与结算页的统计行。 |
| [`fonts/qpq_font_24.c`](fonts/qpq_font_24.c) | 24 px、4bpp、未压缩 LVGL 点阵 | 答题页的四个候选，以及判卷页的结果、解析、已填空的句子与完整原文。 |
| [`fonts/qpq_font_32.c`](fonts/qpq_font_32.c) | 32 px、4bpp、未压缩 LVGL 点阵 | 读者绝对不能看错的那一处：正在填的那句话，另有结算页的评级大字与标题页的大标题。 |
| [`fonts/charset.txt`](fonts/charset.txt) | UTF-8 文本 | 三档共用的 1303 个码点清单，便于在不打开生成代码的前提下复核子集内容。 |

三档子集来自同一份母字体：

- **来源**：Noto Sans CJK SC Regular（`NotoSansCJKsc-Regular.otf`，16,437,364 字节，
  SHA-256 `2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`），取自
  [`notofonts/noto-cjk`](https://github.com/notofonts/noto-cjk)。
- **许可**：SIL Open Font License 1.1。允许子集化与再分发，前提是许可文本与版权声明
  随字体一同提供。
- **字符范围**：1303 个码点——1079 个汉字，另有 224 个其它码点：可打印 ASCII、空格、
  界面自绘的中文标点，以及画填空位用的全角下划线。这里刻意**不**用 LVGL 内置 Montserrat
  兜底：子集里缺的码点会渲染成空白框，所以生成脚本在转换前会逐个码点核对母字体的 `cmap`。
- **转换工具**：`lv_font_conv` 1.5.3，参数 `--bpp 4 --no-compress --format lvgl`。
- **重新生成（在仓库根目录执行）：**

  ```sh
  python3 tools/qiaopi/gen_font.py          # 重建三档字库
  python3 tools/qiaopi/gen_font.py --check  # 只校验清单与覆盖，不转换
  ```

  两种方式都会读取 `tools/qiaopi/bank.txt` 与 `main/*.c|*.h` 里的字符串字面量，
  也就是说**新增界面文案或新增一道题后必须重新跑生成脚本**。`--check` 的作用是把「漏字」变成
  构建期错误，而不是设备上的一块空白。
- **目标放置路径**：由 `main/CMakeLists.txt` 的 `target_sources` 编入 `main` 组件；使用时用
  `LV_FONT_DECLARE` 声明，并按控件逐个指定。`sdkconfig.defaults` 里的
  `CONFIG_LV_FONT_FMT_TXT_LARGE=y` 是必需的——LVGL 默认的文本格式字库用 16 位存放字形位图
  偏移量，而**三档全部**超出这个 65,536 字节的字段：

  | 字级 | 字形位图数据 | 是否超出 16 位偏移字段 |
  | --- | --- | --- |
  | 16 px | 145,642 字节（142.2 KB） | 是 |
  | 24 px | 322,451 字节（314.9 KB） | 是 |
  | 32 px | 551,508 字节（538.6 KB） | 是 |

  这个开关按**字体格式**生效、不是按单个字库，所以开一次就覆盖了三档。
- **生成的源码体积（实测）**：1,068,125 / 2,116,944 / 3,482,000 字节。十六进制字面量的源码
  开销是位图的数倍，所以目录列表把 Flash 占用高估了约六倍——要看上表的字节数，不要看 `ls`。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

| 文件 | 尺寸与格式 | 用途与来源 |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160，JPEG | 嵌入中英文项目 README 的产品主图，突出 AI Passport 产品形象与开放、人人可创作的理念。 |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724，PNG RGBA | 保留为可选技术参考图，不再用于首页主视觉。于 2026-09-17 使用内置图像生成工具为本仓库生成；已根据文档中的硬件能力契约核对图中的六项标签与参数。 |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336，PNG RGBA | 从仓库原始 `images/logo.png` 中精确裁切并去除背景的黑色字标；用于中英文项目 README 的浅色主题。 |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336，PNG RGBA | 提取字标的白色版本；README 使用 `<picture>` 在 GitHub 深色主题下显示。 |

### 侨批游戏的四个页面

`images/qiaopi/` 存放首页 README 用的四张界面图：

| 文件 | 尺寸与格式 | 内容 |
| --- | --- | --- |
| [`images/qiaopi/qiaopi-title.png`](images/qiaopi/qiaopi-title.png) | 1152 × 1536，PNG RGB | 标题页：侨批大字、三项菜单、成绩行，第一项为选中态。 |
| [`images/qiaopi/qiaopi-ask.png`](images/qiaopi/qiaopi-ask.png) | 1152 × 1536，PNG RGB | 答题页：顶栏分类与题号、一句带空槽位的侨批、四个候选（甲乙丙丁）。 |
| [`images/qiaopi/qiaopi-reveal.png`](images/qiaopi/qiaopi-reveal.png) | 1152 × 1536，PNG RGB | 判卷页：对错、正确答案、已填空整句、解析；内容在 248px 处被裁，正是「要按下键翻页」的由来。 |
| [`images/qiaopi/qiaopi-summary.png`](images/qiaopi/qiaopi-summary.png) | 1152 × 1536，PNG RGB | 结算页：评级与评语、正确数、最高连对、用时、得分。 |
| [`images/qiaopi/screens-source.py`](images/qiaopi/screens-source.py) | Python | 生成器：把版式常量与真实题目写成 HTML。 |
| [`images/qiaopi/screens-source.html`](images/qiaopi/screens-source.html) | HTML，13 KB | 生成结果：四个 240 × 320 设备框并排，供人查看与截图。 |

- **这四张是按源码版式参数重绘的示意图，不是真机照片。** 几何全部取自
  `main/qpq_ui.{h,c}`（页面卡内缩 5、顶栏 36 / 底栏 26、正文 210 × 248、行高 36 + 间隔 4）、
  `main/qpq_page_*.c`（每一页的元素坐标）与 `main/qpq_wrap.c`（折行算法，生成器逐字复刻，
  断字位置与设备一致）；题目内容取自 `main/qpq_text.c` 的第 1 题，结算数字按
  `QPQ_SCORE_CORRECT` / `QPQ_SCORE_STREAK_BONUS` 的规则自洽（13 对 7 错、最高连对 5 ⇒ 115 分 ⇒ 65% ⇒ 番客三级）。
- **不是固件资产**：只用于文档，不烧进镜像、不占 Flash。
- **重新生成**：`python3 screens-source.py` 重出 HTML；再把每个 240 × 320 设备框单独渲染，
  按 4.8 倍设备缩放截图（`--window-size=240,320 --force-device-scale-factor=4.8`），
  正好得到 1152 × 1536 的竖版 3:4 图。字体用系统里的 Noto Sans CJK / 微软雅黑代
  Noto Sans CJK SC，字形与设备略有差异。

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

### 侨批填字问答

本应用是这一族里**第一个真正播放录音**的应用，不再只有合成音效。它交付的是一个自描述的
单一二进制，而不是一堆媒体文件：

| 文件 | 尺寸与格式 | 用途 |
| --- | --- | --- |
| [`audio/qpq_audio.bin`](audio/qpq_audio.bin) | 5,298,976 字节，IMA-ADPCM 4bit，16 kHz 单声道 | 一个 blob 装下全部 92 个片段，作为一整片内存映射的 `rodata`。 |
| [`audio/clips.txt`](audio/clips.txt) | UTF-8 文本 | 片段清单（含时长与字节数），便于不开十六进制编辑器就能复核 blob。 |

- **内容**：92 个片段，合计 662 秒（11 分 02 秒）。第 0–90 号是作答之后播放的配音——
  正确答案的整句方言朗读，一题一段；第 91 号是背景音乐。
- **格式**：IMA-ADPCM、4bit、16 kHz 单声道。每 8 个源样本恰好编码成 4 字节，相对同码率的
  16 位 PCM 是 4.00:1；而部分源文件其实是 24 kHz 单声道 PCM，在那里比值达到 6.00:1——
  降到 16 kHz 这一步也计入了这个收益。解码是每样本一次查表加一次移位，这正是它在无 PSRAM
  的单核 C3 上可行的原因；换真正的 MP3 解码器要付出约 28 KB 堆内存和一个大得多的依赖，
  而在这种喇叭尺寸下听不出差别。
- **blob 布局**：先 16 字节头部（魔数、版本、片段数、采样率与一个保留字），再一张定长索引表，
  每个片段一项 8 字节的 `(偏移, 样本数)`，最后是各片段本体。每个片段是 int16 首样本、
  uint8 初始步长索引、一个保留字节，随后是半字节流，每字节两个样本。所有字段定宽且逐字节
  读取——blob 由链接器摆在 Flash 里，RISC-V 的未对齐访问会直接异常，所以这里没有任何一处
  走强制转换。打开时会校验整张索引，任何一处不自洽就整体拒绝；这些拒绝路径都有宿主测试覆盖。
  `audio/clips.txt` 还记录了每个源文件的 SHA-256，`--check` 在源素材可达时会逐个复核。
- **重新生成（在仓库根目录执行）：**

  ```sh
  python3 tools/qiaopi/gen_audio.py          # 重新编码全部 92 个片段
  python3 tools/qiaopi/gen_audio.py --check  # 只校验 blob 与源文件是否一致
  ```

  两种方式都需要 `PATH` 上有 `ffmpeg` 用来解码源文件。编码器是 `tools/qiaopi/adpcm.py`，
  同一次运行还会产出 `tests/qpq_adpcm_fixture.h`——一组固定的源/编码/期望三元组，C 解码器
  就是对着它断言的，两份实现因此不会悄悄跑偏。
- **目标放置路径**：由 `main/CMakeLists.txt` 的 `target_add_binary_data` 附到 `main` 组件。
  符号名由文件名生成（即 `_binary_qpq_audio_bin_start`），而声明它的只有
  `main/qpq_audio_blob.c` 一个文件。不要再把同一个 blob 同时加进 `SRCS`，也不要把它生成
  成一个 C 数组：5 MB 的数组会让编译与链接明显变慢，而且生成出来的源文件无法审阅。
- **来源**：`novel-to-game` 仓库的 `game-adaptations/qiaopi/build/app/audio/`，该仓库带 MIT
  许可（`Copyright (c) 2026 NovelToGame contributors`）。发布前请注意一处矛盾：那个仓库的
  构建说明里写着「音效使用 Web Audio API 合成，**零音频文件**」，而实际构建出来的应用里
  有 92 个音频文件。这些录音各自的来源在那里没有任何记录，所以在把这个应用分发到自用设备
  之外以前，应先向上游作者确认旁白与音乐的再分发许可。
- **排查记录**：拿到的源音频合计 14.94 MB，对一台靠 Flash 的设备来说看着几乎放不下。其实
  放得下——92 个文件里有 31 个（9.37 MB，占总量 63%）文件名叫 `.mp3`，内容却是未压缩的
  24 kHz 单声道 PCM。管线用 `ffmpeg` 解码每个源文件、按**实际返回的编码**判断而不是看扩展名，
  这就是结果能落在 5 MB 里而不是 15 MB 的原因。
