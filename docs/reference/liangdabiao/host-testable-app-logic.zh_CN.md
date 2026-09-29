<p align="right">
  <strong>简体中文</strong> · <a href="host-testable-app-logic.md">English</a>
</p>

# 把应用逻辑留在主机上

为[侨批填字问答](qiaopi-quiz/README.zh_CN.md)而写。这个应用代码约 2300 行——五个页面、一条四层日课流、一个带版本号的存档格式，再加一个手写的中文折行器。其中大约 610 行逻辑，外加 44 行生成的经文表，是在电脑上跑、在电脑上测的，**不接板子**；背后是 727 行主机测试。

## 让这一切成立的那条分界

应用里有两类模块，分界只有一条规则：**纯逻辑模块不得包含 `esp_*.h`、`lvgl.h`、`nvs.h`、`freertos/*` 或任何平台头文件。** 它只接收普通 C 类型、只返回普通 C 值、不持有任何硬件状态。

| 纯（主机可测） | 绑定硬件（只能在设备上） |
| --- | --- |
| 章节表访问、偏移与章号文案 | 那五个 UI 页面 |
| 四层日课状态机 | 按键分发与页面跳转 |
| 中文折行 | NVS 读写 |
| 进度模型、待参队列、序列化 | 音效播放 |

回报不只是「有测试挺好」，而是**整个规则层可以被穷举**，毫秒级跑完，就在你正在打字的这台机器上：

```bash
cc -std=c11 -Wall -Wextra -Werror -I main \
   tests/test_qpq_wrap.c main/qpq_wrap.c main/qpq_chapter.c main/qpq_text.c \
   -o test_qpq_wrap && ./test_qpq_wrap
```

这条命令里有两个细节是刻意的。`-Werror` 让主机编译比固件编译更苛刻。而测试同时连了 `qpq_chapter` 与 `qpq_text`，所以它断言的是**真正会出现在屏上的那批字**，而不是测试文件里另抄的一份样本——内容一改立刻知道，而不是等到下一次固件构建。

## 每个入口都要守住一个承诺

一个返回 `bool` 的模块，只有在**失败路径被写清楚**之后才可测。全局的规矩：**失败时不动输出。** 这样调用方永远不必去检查一个写了一半的结构体，测试也能直接断言这个承诺，而不是只看返回值。

```c
/* screen_1 是该章第一屏原文：8 个字，24 字节 UTF-8。按每行 6 字折成两行，
   于是输出是 25 字节 —— 正文加上一个插入的换行。 */
assert(qpq_wrap_utf8(screen_1, 6, out, 25) == 0);
assert(out[0] == '\0');    /* 25 字节输出，加结尾符需要 26 */
assert(qpq_wrap_utf8(screen_1, 6, out, 26) == 25);
```

注意这个边界量的是**折行后的输出**，不是输入。输入是 24 字节，函数插入的那个换行把它变成 25。照源文本去算这个边界会差一个字节，而测试会「通过」——用着一个在设备上会截断的缓冲区。

这两行合起来比单独任何一行都值：第一行证明边界被识别，第二行证明边界就在注释说的那个位置上。反过来差一个字节，第一行照样过。

无效下标与空指针同理，都显式断言，而不是留一句「应该不会发生」：

```c
assert(qpq_chapter_at(-1) == NULL);
assert(qpq_chapter_at(QPQ_CHAPTER_COUNT) == NULL);
assert(qpq_chapter_passage(NULL, 0) == NULL);
assert(qpq_progress_read_count(NULL) == 0);
assert(qpq_progress_pending_at(NULL, 5, 0) == -1);
```

这些很容易被跳过。在主机测试里被断言过的 `NULL` 检查，是你**知道存在**的检查；只在脑子里过了一遍的那个，会恰好从真正被走到的那条路径上缺席。

有一个边界只能给出更弱的承诺，这件事值得写出来而不是藏起来：容量为 0 的缓冲区根本没法写，所以 `qpq_chapter_label` 在那里只保证返回值，不保证清空字符串。头文件明确写了这一点，测试也只断言返回值。一个承诺得比自己能做到的更多的契约，比承认缺口的契约更糟——照着那个过强的版本写的测试会失败，而失败看起来像代码 bug。

## 全都扫一遍，不抽样

主机测试便宜到可以**穷举每一种情形**，而不是挑一个代表：

- 进度模型的全部 **81 个章位**：表态存取器在区间的两端和越界下标上都返回「未表态」。
- 已收录那一章的**全部 6 屏原文、5 条点拨、3 个选项**，逐个折行，逐个核对每行字数上限、行数上限与禁则。
- **91 题**在待参队列查询里的每一个前缀长度，从 `0` 到 `chapter_count`。
- 存档 blob 的**往返**，外加对它的九种不同破坏。
- 会话状态机的**按键序列空间**：每层的顶端与底端移动、下钻、回退，以及从每个阶段取消。

最后一条值得多说一句。「读到最后一句再往下就进点拨层」「在第一屏回退就直接离开、什么都不记」「表态可以从会话里读出来、不必另存一份」——这些规则，本来要靠人在真机上一颗颗按钮去试；在这里是 133 行断言。

## 要测数据与常量，不只是函数

有两类断言抓到了光靠读代码抓不到的问题：

**生成数据的结构不变量。**

```c
for (int index = 0; index < QPQ_CHAPTER_COUNT; index++) {
    const qpq_chapter_t *chapter = qpq_chapter_at(index);
    assert(chapter->number == index + 1);
    assert(chapter->point_count >= 3 && chapter->point_count <= 5);
    assert(chapter->option_first == (uint16_t)(index * QPQ_PONDER_OPTION_COUNT));
}
assert(passage_sum == QPQ_PASSAGE_COUNT);
assert(point_sum == QPQ_POINT_COUNT);
assert(QPQ_OPTION_COUNT == QPQ_CHAPTER_COUNT * QPQ_PONDER_OPTION_COUNT);
```

`option_first` 那条是关键。每章的三个选项存在一张共享表里，章记录只存一个偏移量。要是某章生成出来的偏移不是 3 的倍数，那一章的三个选项就会**悄悄变成另外三章的答案**——一个读代码绝对看不出来的数据 bug。

**推导常量的自洽。**

```c
assert(QPQ_NOTES_BYTES == 81);
assert(QPQ_BITMAP_BYTES == 11);          /* (81 + 7) / 8 */
assert(QPQ_PROGRESS_BLOB_SIZE == 4 + 81 + 11 + 11 + 1);
assert(QPQ_PROGRESS_BLOB_SIZE == 108);
```

这些常量决定存档 blob 有多少字节。写错一个，存档就短一个字节；校验和会在加载时抓到，但**只在设备上、只在断过一次电之后**。

## 存档格式：先往返，再往死里折腾

序列化值得比「happy path」更多的测试，因为这些数据要穿过一次断电和一颗 Flash 控制器：

```c
assert(qpq_progress_deserialize(&back, blob, size));       // 往返
assert(qpq_progress_slot(&back, 0) == QPQ_SLOT_CHEWING);
assert(qpq_progress_slot(&back, 80) == QPQ_SLOT_MISSED);
assert(qpq_progress_read_count(&back) == 2);
```

把 magic、版本、长度、校验和逐个改坏，每次都断言被拒。其中两种要单独点名，因为它们容易漏：

- **截断与加长都要测。** `sizeof(bad) - 1` 和 `sizeof(bad) + 1` 都要被拒。写成 `< expected` 的长度检查能挡住前者，却会放过后者。
- **校验和正确、但取值越界。** 把表态字节改成一个超出枚举范围的值，再**重新算一遍校验和**让它正确。只信校验和的格式会照单全收，然后拿着这个值去索引表态表、越界读到表外。拒掉它是对**解码后的值**的断言，不是对容器的断言。

前面那条「失败时不动输出」的承诺在这里同样适用，所以测试先放一个哨兵值：

```c
untouched.sessions = 777;
assert(!qpq_progress_deserialize(&untouched, bad, sizeof(bad)));
assert(untouched.sessions == 777);   // 被拒的加载不许「应用一半」
```

一个拒绝不掉的格式，就是会在最糟的时刻被应用一半的格式。

## 语义规则也该待在纯层里

任何读者会当成「这应用怎么怪怪的」的东西都是规则，而规则要待在能被测的地方。待参队列是最好的例子，因为它的全部价值就在于**和读者记下的东西一致**：

```c
qpq_progress_set_slot(&pending, 0, QPQ_SLOT_LANDED);
qpq_progress_set_slot(&pending, 1, QPQ_SLOT_CHEWING);
qpq_progress_set_slot(&pending, 2, QPQ_SLOT_MISSED);
assert(qpq_progress_pending_count(&pending, 3) == 2);   // 「接了」不算待参
assert(qpq_progress_pending_count(&pending, 1) == 0);   // 第 1 章两边都不算
assert(qpq_progress_pending_count(&pending, 2) == 1);
```

三行里掉出两条规则，而且都是读者会注意到的：他说过「接了」的章，不该再回来要求他重读；还没收录的章，压根不该出现。队列是存档状态的**纯函数——是推导出来的，从不另存**，这正是队列与已记表态不可能不一致的原因。要是队列也当成第二份存档数据，那「两边都要更新」就成了一条规则，而存档路径上的规则恰恰是会在设备上崩掉的那些。

饱和那一条也是同样的思路：日课次数在 `UINT16_MAX` 停住，而不是回绕到 0、告诉读者他的日课次数倒退了。

## 哪些别硬塞给主机测试

别把这个模式硬撑到 UI 上。版式、控件生命周期、按键时序和 NVS 都需要设备，或者仓库里那套基于桩的 demo 运行时测试。目标是**缩小「只能在硬件上查」的那一集合**，不是消灭它——上面这层纯逻辑覆盖了规则、队列、折行器与存档格式，把 LVGL 那层留给真板子。

## 这套做法抓到的 bug

**每行字数上限**这条断言，在折行规则还没上屏之前就证明了它是错的。第一版为了不让收尾标点落到行首，用的是「把标点提上一行」——这会让那一行多出一个字，LVGL 于是再折一次，行数就不可控了，整套垂直预算随之作废。把 `max_line_chars(out) <= BUDGET` 对着真实经文断言之后，这个写法再也回不来。现在的规则是「提前一字换行」。

同一个测试接着抓到了**我自己**的一个错误预期：我断言一个六个字的串会折成「五字 + 一字」，结果断言失败——它正好六个字，一行放得下，压根没有换行。错的是测试，不是代码；但一个**拿现实校对过**的测试，才让你知道究竟是哪个错了。要是这个测试是照着测试文件里另抄的一份内容写的，它会「通过」在那份副本的行为上，然后对整个应用一言不发。

## 检查清单

- 纯模块不含任何平台头文件；分界写在每个模块顶部的注释里。
- 每个入口都写明并守住「失败时不动输出」——承诺必须更弱的地方，头文件里说明了。
- 越界下标、空指针、零容量都断言，不假设。
- 循环穷举每一章、每一层、每一个列表长度，不抽样。
- 测试连的是真实内容模块，断言的是会上线的那份数据。
- 数据结构不变量（偏移、计数求和、字节宽度）与推导常量都有断言。
- 存档格式测的是**拒绝**——截断、加长、坏 magic、坏版本、坏校验和、越界取值——不只是往返。
- 用户可见的规则（待参队列、计数器饱和）在纯层里断言。
- 每个测试都接进 `tools/validate.sh --static`，CI 和本地都会跑，不靠谁记得手动执行。

## 相关文档

- [侨批填字问答](qiaopi-quiz/README.zh_CN.md) —— 这里提到的模块与测试。
- [在 Windows 的 Git Bash 里构建 ESP-IDF 固件](windows-git-bash-esp-idf.zh_CN.md) —— 怎么弄到宿主编译器，以及仓库那套基于桩的 demo 测试为何与这些不一样。
- [把章节内容整理成生成的 C 数组](question-bank-pipeline.zh_CN.md) —— 这些测试回头复核的那个生成器。
- `docs/development/engineering/build-and-test.zh_CN.md` —— 共享的验证门禁。
- `tools/validate.sh` —— 每个测试的源文件清单就写在这里。
