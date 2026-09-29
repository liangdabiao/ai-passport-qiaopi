<p align="right">
  <strong>简体中文</strong> · <a href="windows-git-bash-esp-idf.md">English</a>
</p>

# 在 Windows 的 Git Bash 里构建 ESP-IDF 固件

为构建[侨批填字问答](qiaopi-quiz/README.zh_CN.md)时攒下的经验。当时的环境是 Windows 上的 Git Bash（MSYS2）加 ESP-IDF 5.5.3。有两道坎会直接把你挡住，其中一道**在 shell 内部无法解决**；另外还有一批既有测试在这台机器上**确实无法运行**。

先说明：这不是在推荐你这么干。受支持的路线是官方 ESP-IDF 安装器配 `cmd` 或 PowerShell，见 `docs/development/engineering/environment-setup.zh_CN.md`。下面讲的是：当你已经在 POSIX shell 里了——比如你的工具链或 AI 编码助手就住在那儿——并且希望固件验证门能跑起来时，该怎么做。

## 第一道坎：只要设了 `MSYSTEM`，ESP-IDF 就拒绝启动

ESP-IDF 5.5.3 会检测 MSYS shell 然后停下来。在 `tools/idf.py` 里：

```python
if 'MSYSTEM' in os.environ:
    print_warning('MSys/Mingw is no longer supported. ...')
    # ...此后 main() 永远不会被调用
```

这条"警告"很有误导性：它根本不是警告。`main()` 被跳过了，于是 `idf.py` 打印一行然后成功地退出，什么都没干。另外，`tools/idf_tools.py` 的 `__main__` 里也有同样的判断，那里调的是 `fatal()`，直接以状态码 1 退出。结果就是 `idf.py build` 完全跑不起来。

显而易见的那招——把这个变量 unset 掉——没有用，而原因值得搞清楚：**MSYS 运行时会给它启动的每一个原生 Windows 子进程重新注入 `MSYSTEM`。** 在 shell 里 unset 并不能阻止它在边界处注入：

```console
$ unset MSYSTEM
$ echo "'${MSYSTEM:-<unset>}'"
'<unset>'
$ python.exe -c "import os; print(os.environ.get('MSYSTEM'))"
MINGW64
```

只有两条路：改用 `cmd`/PowerShell（那里没有这个变量），或者给本地这份 ESP-IDF 打补丁。

**如果要打补丁，只改本地工具链，绝不要改仓库。** 仓库的检查看不到 `D:\esp\...`，所以被改过的 IDF 是一处没人知道的偏差，下一个人不会知道。在改动处留注释说明上游怎么做的、为什么在这里不能用、以及这是本地修改——然后**写进你团队会看到的地方**，因为一个没人记得的工具链补丁，就是一份迟早要来的故障报告。本机的两处补丁都保留了上游的警告并继续进入 `main()`。

还有一个路径细节：`idf.py` 可能会解析到一个 `idf-exe` 包装器而不是真正的脚本。把 ESP-IDF 自己的 `tools/` 目录放到它前面，让真正的 `idf.py` 胜出。

## 第二道坎：IDF 的虚拟环境和 `PATH` 上的解释器

工具安装器会按它当时用的解释器给 Python 虚拟环境命名——这里是 `idf5.5_py3.12_env`，因为 ESP-IDF 5.5.3 用的是 Python 3.12。如果你 `PATH` 上的 `python3` 是别的小版本，激活脚本就会去找一个不存在的虚拟环境：

```text
idf5.5_py3.13_env ... not found
```

最干净的做法是别跟它较劲：在 `PATH` 最前面放一个一行的转发脚本，`exec` 到虚拟环境自己的解释器，这样 `python` 和 `python3` 都表示"IDF 当初安装用的那个解释器"。

```sh
#!/bin/sh
exec "D:/esp/.espressif/python_env/idf5.5_py3.12_env/Scripts/python.exe" "$@"
```

通过包装器启动时，`idf.py` 仍会警告解释器"not from installed venv"。只要包装器指向的就是同一个解释器，这条是纯装饰性的；用之前先跑 `idf.py --version` 确认。

两道坎都处理完之后，`idf.py --version` 会报 5.5.3，固件门也能正常跑。这个门是冷编译，大约 2000 个 Ninja 步骤，所以给它几分钟，并且**放到后台跑**，不要盯着它。

## 在没有 MSVC 也没有 MinGW 的情况下弄到一个宿主编译器

静态门用 `${CC:-cc}` 编译主机测试。在一台既没有 MSVC 也没有 MinGW 的机器上，它在开始之前就会失败。`zig cc` 是一个可用的替代品，而且它以 Python wheel（`ziglang`）的形式分发——当从 GitHub 下载很慢、而 PyPI 镜像很快时，这一点很省事。

把它包成一行脚本，让验证门指向它：

```sh
#!/bin/sh
exec "/path/to/zig.exe" cc "$@"
```

```bash
CC=/path/to/cc ./tools/validate.sh --static
```

注意 `zig cc` 是靠启动自己的子编译器来工作的，所以如果运行环境限制创建子进程，这一步可能需要在沙箱之外执行。这是环境权限问题，不是项目问题。

## 依然跑不通的部分：demo 运行时测试

有四个既有测试——音频、低功耗、BLE、Wi-Fi 的 demo 运行时测试——在这套工具链下**链接不过**。这里值得把原因说准确，而不是糊过去。

它们包含只**声明**、不定义 LVGL 与 `ui_pixel` 函数的桩头，然后只调用被测 demo 模块的一小部分。模块里剩下的函数会引用这些桩。这些测试依赖链接器把那些没被引用的函数丢掉，而这需要 GNU `--gc-sections` 的语义。`zig cc` 始终使用 `lld`，而它的 PE/COFF 模式并不实现这个开关：

```console
$ cc ... -Wl,--gc-sections -o t          # 开关被接受,然后被静默忽略
lld-link: error: undefined symbol: ui_pixel_screen_create
$ cc ... -Wl,/OPT:REF -o t               # MSVC 的写法
error: unsupported linker arg: /OPT:REF
```

改用 `-target x86_64-windows-gnu` 并不会改变链接器，所以也没用。**没有任何开关组合能修好它。**

正确的应对是：到一个有 GNU 工具链的地方去验这些测试——也就是仓库 workflow 使用的 Linux CI——而**不要**为了在本地跑出绿色而弱化桩、加 `--allow-undefined` 或者跳过测试。三个依赖创建符号链接的 Python 测试（`test_check_repo`、`test_archive_firmware`、`test_install_passport_skills`）同理：没有开发者模式或管理员权限时，它们在 Windows 上失败的原因与代码毫无关系。

**下结论之前先确认。** 让这件事可信的关键一步，是把该提交导出一份纯净副本，在那里复现出完全相同的失败：

```bash
mkdir -p /tmp/baseline && git archive HEAD | tar -x -C /tmp/baseline
cd /tmp/baseline
CC=/path/to/cc ACTIONLINT_BIN=/path/to/actionlint ./tools/validate.sh --static
```

如果失败项和数量都与工作区一致，那它们就是环境问题。这一条命令，就是"我跑不了这些测试"和"我知道这些测试为什么不能在这里跑"之间的区别。

因为验证门在第一个失败处就停了，所以在这台机器上 `--static` 会提前结束。把剩下的检查逐条单独跑，让其余部分仍然被覆盖到：

```bash
python3 tools/check_repo.py
actionlint -color .github/workflows/*.yml
for t in test_deep_sleep_contract test_verify_firmware; do python3 tests/$t.py; done
```

## 检查清单

- 先确认 `MSYSTEM` 是否被设置，再去怀疑别的东西；它会把 `idf.py` 的行为从"能跑"变成"静默退出"。
- 优先用 `cmd`/PowerShell，而不是给 ESP-IDF 打补丁。真要打，只改本地副本，并把补丁写下来。
- 让 `python`/`python3` 解析到 IDF 虚拟环境当初的那个解释器。
- 把 IDF 的 `tools/` 目录放到任何 `idf-exe` 包装器前面。
- 没有别的宿主编译器时，用 `CC` 包装器套一个 `zig cc`。
- 承认那批基于桩的 demo 运行时测试需要 GNU 链接器，到 CI 里去验它们，**永远不要**为了本地通过而弱化它们。
- 任何怀疑是环境问题的失败，都先在纯净导出上复现，再把它报成环境问题。

## 相关文档

- [侨批填字问答](qiaopi-quiz/README.zh_CN.md) —— 这些笔记来自的那次构建，含它的验证结果。
- [把应用逻辑留在主机上](host-testable-app-logic.zh_CN.md) —— 在这里**跑得起来**的那些测试，以及它们为什么可移植。
- `docs/development/engineering/environment-setup.zh_CN.md` —— 受支持的环境搭建路线。
- `docs/development/engineering/build-and-test.zh_CN.md` —— 验证门检查什么、按什么顺序。
