# STC32G 开发工作流

本仓库以逐飞 STC32G144K246 100Pin CoreBoard、Keil C251 和 Windows PowerShell 5.1 为开发环境，包含 LED 示例和独立的双电机测试工程。以下命令均从克隆后的 `car` 仓库根目录执行。

## 新电脑的必要配置

### 1. 安装基础工具

在 Windows 上安装 Git、VS Code 和包含 **C251** 工具链的 Keil。安装 MDK-ARM 或 C51 本身不能代替 C251。本库说明使用的工具链基线是 C251 V5.60；迁移电脑后要重新验证完整工程。

打开 Windows PowerShell，确认基础命令可用：

```powershell
git --version
code --version
$PSVersionTable.PSVersion
```

构建脚本要求 PowerShell 5.1 或更高版本，VS Code 任务调用 Windows 自带的 `powershell.exe`。若 `code` 暂未识别，可从开始菜单启动 VS Code；如果需要命令行入口，检查安装器是否启用了 PATH 集成并重新打开终端。

主机逻辑测试另外需要 Windows 上可运行的 GCC，例如 MSYS2 UCRT64 GCC。它只用于测试控制逻辑，不替代 C251 固件编译。相关安装程序与需要管理员权限的步骤由开发者手动完成；仓库脚本不会自动安装软件或修改系统 Keil 配置。

### 2. 确认 Keil 路径和 C251 工具链

默认脚本使用 `C:\Keil\UV4\UV4.exe`。以下是默认安装结构示例，不要求把工具复制进仓库：

| 工具 | 默认安装路径 |
| --- | --- |
| µVision 命令行构建入口 | `C:\Keil\UV4\UV4.exe` |
| C251 编译器 | `C:\Keil\C251\BIN\C251.EXE` |
| A251 汇编器 | `C:\Keil\C251\BIN\A251.EXE` |
| L251 链接器 | `C:\Keil\C251\BIN\L251.EXE` |
| HEX 转换工具 | `C:\Keil\C251\BIN\OH251.EXE` |

确认自己实际安装目录内存在上述程序；多套 Keil 并存时，以任务真正调用的 `UV4.exe` 为准。可在 PowerShell 查看文件版本：

```powershell
$keilRoot = 'C:\Keil' # 改成自己的实际安装根目录
Get-Item -LiteralPath (Join-Path $keilRoot 'UV4\UV4.exe') |
    Select-Object FullName, @{Name='Version'; Expression={$_.VersionInfo.FileVersion}}
Get-Item -LiteralPath (Join-Path $keilRoot 'C251\BIN\C251.EXE') |
    Select-Object FullName, @{Name='Version'; Expression={$_.VersionInfo.FileVersion}}
```

在这套 µVision 的 `File → License Management` 中确认 C251 产品具备有效使用权限，按个人或单位的正规授权流程完成配置。仅有 ARM 产品授权或能够启动 `C251.EXE`，都不足以证明完整 C251 工程可用。C251 评估工具有 2 KiB 目标代码限制；日志出现 `L250`、`0800H` 或 `CODE SIZE LIMIT` 时，应检查工具链使用权限，不要修改源码或器件型号来掩盖限制。本文不包含激活材料或授权绕过步骤。参见 [Keil 评估限制](https://www2.keil.com/limits) 和 [官方授权管理说明](https://www.keil.com/support/man/docs/license/license_management.asp)。

### 3. 配置 STC32G144K246 器件支持

从 [STC 官方工具软件页面](https://www.stcai.com/gjrj) 获取当前官方 STC-ISP / AiCube-ISP。按照软件自带说明，在与 Keil 集成有关的配置页面中选择上一步确认的 Keil 根目录，导入准确的 STC32G144K246 器件记录和头文件。界面名称随版本变化，以实际界面与官方说明为准，不按旧截图猜按钮。

完成导入后重新打开目标工程，核对 Keil 的器件选择及 `.uvproj` 中的值：

```text
Device:       STC32G144K246-32Bit Series
Vendor:       STC
ToolsetName:  MCS-251
RegisterFile: STC32G144K246.H
```

旧工程中的 `STC32G144K-32Bit Series` / `AI` 与此记录不同，可能导致 `Device not found`。应核对当前官方数据库中的准确型号，并与工程元数据对应；不要选择其他 MCU 或手工伪造数据库记录。需要使用 STC 硬件调试时，再按官方说明向同一套 Keil 安装相应调试支持；编译、烧录和硬件调试是不同步骤。

### 4. 处理 C251 中断支持

如果官方例程报告 `C168: interrupt: value not in range 0 ... 31`，按 STC 官方说明使用配套的中断扩展功能，目标必须是当前实际使用的 C251 安装。完成后重新执行全量构建检查。不要修改共享库的官方中断号或删除中断处理来绕过错误，也不要使用来源不明的补丁。

该步骤与器件数据库导入相互独立：设备列表能够找到芯片，并不证明编译器已支持相应中断号。系统文件安装、管理员确认和图形界面操作由开发者手动完成；仓库的构建脚本不执行这些操作。

### 5. 克隆并打开仓库

从 GitHub 仓库页面复制 HTTPS 克隆地址，执行：

```powershell
$repoUrl = 'https://github.com/cyyye704/car.git'
git clone $repoUrl car
Set-Location car
code .
```

也可在 VS Code 使用“文件 → 打开文件夹”，选择 `car` 根目录。任务文件是 [`.vscode/tasks.json`](../.vscode/tasks.json)，因此应打开包含 `.vscode`、`scripts`、`tests` 和源码子目录的这一层，而不是只打开某个 C 文件。

### 6. 让 VS Code 使用正确的 Keil

安装位置不是默认 `C:\Keil` 时，可通过 `-Uv4Path` 指定构建入口。例如把下面示例目录替换为实际目录：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-led.ps1 -Uv4Path 'D:\Tools\Keil\UV4\UV4.exe'
```

VS Code 中，编辑 `.vscode/tasks.json` 的 LED、电机构建任务，在各自 `args` 的脚本路径之后追加参数。电机任务示例：

```json
"args": [
  "-NoProfile",
  "-ExecutionPolicy",
  "Bypass",
  "-File",
  "${workspaceFolder}\\scripts\\build-motor.ps1",
  "-Uv4Path",
  "D:\\Tools\\Keil\\UV4\\UV4.exe"
]
```

这是现有任务的 `args` 片段，保留任务其余字段。LED 任务使用 `build-led.ps1` 并传相同的实际工具路径。查看日志任务不启动 Keil，不必追加此参数。脚本的 `-ExecutionPolicy Bypass` 只作用于该 PowerShell 进程，不修改系统执行策略。

配置完成后先执行下面的 LED 全量构建，确认成功和新 HEX，再进行电机构建及烧录。

## 选择工程并构建

| 用途 | VS Code“终端 → 运行任务” | 脚本 |
| --- | --- | --- |
| LED 全量构建 | `STC32G: Rebuild LED (Keil C251)` | [`scripts/build-led.ps1`](../scripts/build-led.ps1) |
| LED 最近日志 | `STC32G: Show last LED build log` | `build-led.ps1 -ShowLog` |
| 双电机全量构建 | `STC32G: Rebuild motor test (Keil C251)` | [`scripts/build-motor.ps1`](../scripts/build-motor.ps1) |
| 双电机最近日志 | `STC32G: Show last motor test build log` | `build-motor.ps1 -ShowLog` |

**`Ctrl+Shift+B` 当前默认编译 LED。电机程序需明确选择电机任务。** 修改了电机源码后只编译 LED，不会更新电机 HEX。

命令行与 VS Code 任务使用相同脚本：

```powershell
# LED
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-led.ps1

# 双电机
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-motor.ps1

# 查看电机最近一次构建日志
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-motor.ps1 -ShowLog
```

两个工程的目标名均为 `stc32g144k246`。对应工程文件：

- `STC32G144K246_100Pin_Library/Example/Coreboard_Demo/E01_led_demo/mdk/seekfree.uvproj`
- `STC32G144K246_100Pin_Library/Example/Coreboard_Demo/E14_motor_test/mdk/seekfree.uvproj`

默认执行 Keil 全量重建 `-r`，使用 `-t` 指定目标、`-j0` 关闭消息提示、`-o` 保存日志，默认超时 120 秒。可通过 `-TimeoutSeconds` 调整超时；`-b` / `-Incremental` 使用 Keil 增量构建，但没有生成新 HEX 的空构建不满足本流程的新产物验收。

任务只执行构建，不会自动进入调试或烧录。VS Code 的 `F5` 不能视为已经配置好的硬件调试入口。

## 日志与 HEX 验证

终端显示完整构建输出，Problems 面板解析源码错误和链接错误。LED 日志位于 `.build/led/`，电机日志位于 `.build/motor/`：

| 相对文件 | 内容 |
| --- | --- |
| `latest.log` | 最近一次构建的 UTF-8 日志 |
| `latest-result.json` | 退出码、失败原因、HEX 验证状态及 SHA256 |
| `<时间戳>-<进程号>/keil.log` | Keil 原始日志 |
| `<时间戳>-<进程号>/build.log` | 本次任务日志 |
| `<时间戳>-<进程号>/result.json` | 本次构建结果 |
| `<时间戳>-<进程号>/previous.hex` | 构建前已有 HEX 的副本，仅在原文件存在时生成 |

脚本只有同时满足以下条件才报告成功：

1. Keil 退出码为 0 或 1，日志确认 `0 Error(s)`，且没有匹配到编译或链接错误。
2. HEX 存在且修改时间不早于本次构建开始时间。
3. Intel HEX 记录长度、校验和、EOF 和非空数据检查通过。

验证成功后，LED 归档 `SEEKFREE.verified.hex`，电机归档 `MOTOR_TEST.verified.hex`，并记录 SHA256。烧录前检查 `success=true`、`hexVerified=true` 和当前所需工程；旧 HEX 的存在不能证明本次构建成功。

生成位置分别为：

```text
STC32G144K246_100Pin_Library/Example/Coreboard_Demo/E01_led_demo/mdk/out_file/SEEKFREE.hex
STC32G144K246_100Pin_Library/Example/Coreboard_Demo/E14_motor_test/mdk/out_file/MOTOR_TEST.hex
```

构建日志、编译产物与本机备份用于本地复验，不作为源码提交。共享问题时摘录必要错误信息，并检查日志是否包含本机路径。

## 常见问题按层定位

| 问题层 | 常见现象 | 核对与处理 |
| --- | --- | --- |
| 编译、汇编与链接工具链 | 找不到 C251；`C168`；链接器 `L250` | 先核对任务使用的 `UV4.exe` 及其 C251 工具。`C168` 按官方说明检查中断扩展；`L250` 检查 C251 评估限制与合法使用权限。器件数据库和调试器设置不能解决代码大小限制。 |
| Keil 器件数据库 | `Device not found`；Keil 退出码 12 | 确认官方支持导入了任务实际使用的 Keil，再逐字核对 `STC32G144K246-32Bit Series` / `STC`。在其他 Keil 安装里能够找到芯片不代表当前这一套已配置。 |
| 工程配置与构建目标 | 缺少源码/头文件/库；没有 HEX；修改电机后结果仍是 LED | 核对打开的仓库根目录、目标 `stc32g144k246`、`MCS-251`、包含路径、链接配置和 `CreateHexFile=1`。LED 输出名应为 `SEEKFREE`，电机应为 `MOTOR_TEST`，目录为各工程的 `mdk/out_file/`。不要随意改变官方内存布局。 |
| 调试器与硬件连接 | 构建通过，但启动调试失败或找不到调试驱动 | 这是另外的硬件调试流程。按官方说明检查 STC 调试支持和实际连接；本工作流使用 ISP 手动烧录，构建任务不启动调试。不要用切换调试器来代替修复编译或设备数据库错误。 |

若只看到旧 HEX 或旧成功日志，重新执行对应全量任务并查看本次 `result.json`。若 USB HID 下载未枚举设备，先核对数据线、P32/RST 顺序及实际供电；它与 Keil 的 `Device not found` 并不是同一个设备发现过程。

## 电机硬件与 USB HID 烧录

本电机工程的接线对象是 DRV8701E Dual MotorDriver V1.3，固定映射如下；连接前按实际板卡丝印核对。

| 驱动信号 | CoreBoard 引脚 | 程序配置 |
| --- | --- | --- |
| GND | GND | 共地 |
| NC | 不接 | 悬空 |
| PWM1 | P7.4 | `PWMB_CH1_P74` |
| DIR1 | P7.6 | 固定低电平 |
| PWM2 | P7.5 | `PWMB_CH2_P75` |
| DIR2 | P7.7 | 固定高电平 |

核心板由 USB 供电，12 V 只接驱动板对应电源端子，不能接核心板的 5V、3V3 或 GPIO。两板必须共地。首次测试架空车轮，整理接线前断开 12 V。

**每次烧录、复位、插拔 USB 或重新上电前，驱动开关必须置 OFF。** 逐飞 `pwm_init()` 在 PWM 接管前会先把相应 GPIO 置高，因此程序不能保证复位和初始化期间没有瞬态输出。OFF 控制驱动休眠，不等于物理断开 12 V。

1. 驱动 OFF、断开 12 V，仅给核心板连接 USB 数据线。打开官方 STC-ISP / AiCube-ISP，选择 `STC32G144K246`，载入刚验证的目标 HEX。
2. 按住 P32，按下再松开 RST，最后松开 P32，进入 USB HID 下载模式。
3. 确认软件识别 `USB-Writer`，执行“下载/编程”，等待明确显示成功。HID 下载不要求程序运行时提供 CDC 串口。
4. 驱动继续保持 OFF，按 RST 运行程序并松开 P32。电机工程等待至少 2 秒，观察停止就绪状态的蓝灯短闪。
5. 关闭外部电源输出时接好驱动电源，再开启 12 V，最后将驱动置 ON。尚未按 P32 时两路电机应保持停止。
6. 按一下 P32 后松开，两个电机持续运行，蓝灯常亮。连续松开至少 0.1 秒后再次按 P32，停止两路 PWM；松开后恢复就绪短闪。长按不会反复切换。
7. 测试结束先将驱动置 OFF，再断开 12 V。出现异常运动、复位或无法按键停止时，也通过 OFF 和断电处理。

驱动 OFF 后，程序可能仍处于运行状态；直接重新拨 ON 会恢复输出。重新开启驱动前先确认程序已停止。软件按键停止依赖 MCU 正常运行，不是独立的硬件保护。

电机工程没有启动运行时 USB CDC，运行后 CDC 串口消失属于预期行为；再次烧录仍使用 P32 + RST 进入 HID。LED 示例则用于观察板载蓝灯是否闪烁。

## 电机逻辑与参数

主程序位于 [`STC32G144K246_100Pin_Library/Example/Coreboard_Demo/E14_motor_test/user/main.c`](../STC32G144K246_100Pin_Library/Example/Coreboard_Demo/E14_motor_test/user/main.c)。它是独立测试工程，不是完整小车控制程序。

| 位置 | 含义 |
| --- | --- |
| `MOTOR_DUTY_PERCENT` | 两路共同的占空比百分数，当前为 30，即 30%；实际值以源码为准 |
| `MOTOR_PWM_HZ` | PWM 频率，当前为 17 kHz |
| `TICK_MS` | Timer0 回调周期，当前为 5 ms |
| `motor_set_running()` | 在同一回调中依次设置两路 PWM 为运行值或 0 |
| `motor_tick()` | P32 采样、启动/停止切换与蓝灯提示 |
| `main()` | 方向、GPIO、PWM 和定时中断初始化 |

开机先锁定 1 秒，并要求 P32 连续松开 100 ms；停止状态下连续按下 20 ms 才启动。启动后持续运行，没有旧版的 0.3 秒自动停止。重新连续松开 100 ms 后，下一次按下在 5 ms 采样回调中停止两路；此后同样要重新松键才能再次启动。

当前版本把两路运行占空比设为 30%，方向保持 DIR1 低、DIR2 高；此前 15% 版本使用相同的启停逻辑。占空比百分数不是实际转速百分数；相同占空比也不保证两轮实际转速相同。本工程没有编码器闭环。

后续改变占空比时，测试期望应同步到当前 `MOTOR_DUTY_PERCENT`，并更新版本说明。测试桩的 `PWM_DUTY_MAX` 为 10000，所以 15% 对应 1500，30% 对应 3000。当前测试工作流会读取源码参数来同步期望值，无需每次手工替换固定数值；修改后仍须重新运行主机测试和 Keil 构建，不能沿用旧版本的通过记录。

## 主机逻辑测试与版本记录

测试入口是 [`tests/motor/run.ps1`](../tests/motor/run.ps1)，测试代码 [`tests/motor/test_motor.c`](../tests/motor/test_motor.c) 直接包含真实电机 `main.c`，并以本地头文件替换硬件 API。GCC 已在 PATH 时可运行：

```powershell
$gcc = (Get-Command gcc.exe -ErrorAction Stop).Source
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\motor\run.ps1 -Compiler $gcc
```

若使用脚本配置的默认 GCC，也可不传 `-Compiler`。结果与被测源码 SHA256 写入 `.build/motor-logic/latest.log`。

测试包含 14 个固定场景和 4 个固定种子随机序列，覆盖启动锁定、去抖、松键要求、两路输出一致、超过 60 秒持续运行、再次按下停止、长按不重复触发和蓝灯状态。主机测试不测量 MCU 中断时序、PWM 波形、电流、接线或真实电机运动；仍需独立完成 Keil 构建和上板验证。

已记录的 15% 版本在 2026-10-17 通过 Keil 全量构建，结果为 0 Errors、0 Warnings。该 15% 版本的实际电机表现尚未确认；此前 USB 单独供电时，P32 启停与蓝灯状态曾得到人工观察，但不能据此证明带载运行正常。

当前源码为 30% 占空比，已在 2026-10-17 完成全量构建：0 Errors、0 Warnings，18:17:36（Asia/Shanghai）生成新 HEX。14 个固定场景及 4 个固定种子随机序列的主机逻辑验证通过。硬件表现尚未验证；历史版本的实测不能用于认定该版本已上板通过。发布或烧录新参数版本前，应先统一源码、测试与说明，再重新执行两项验证。

该次 HEX 的 SHA256 为 `0D6DAA46B6D828BAB2E2DB3F1440B283691FFED8273BA33B15CD94DC63221300`。这是本机工具链生成物的追溯记录；不同工具链版本可能产生不同哈希。后续构建以本次 `latest-result.json` 为准。

每次记录硬件测试时，至少写明对应提交、占空比、HEX SHA256、供电、接线、轮子是否架空、方向、启动/停止表现和异常现象。把“源码已修改”“主机测试通过”“Keil 构建通过”和“实物验证通过”分别记录。

## Git 日常开发与评审

新电脑首次提交前，在本仓库配置提交身份。将下面的值改成自己的 GitHub 用户名和 GitHub 邮箱设置中显示的提交邮箱；可以使用 GitHub 提供的 noreply 邮箱。下面命令只影响当前仓库：

```powershell
git config user.name '你的用户名'
git config user.email '你的提交邮箱'
git remote -v
```

公开仓库克隆不需要令牌；推送需要拥有仓库写入权限。Git for Windows 的 Git Credential Manager 通常会在首次推送时引导浏览器登录 GitHub。按提示完成登录，不要将密码或访问令牌写进脚本、文档或远程 URL。没有写入权限的参与者应先 Fork，再从自己的分支提交 Pull Request。

以 `car` 根目录作为版本管理入口。开始工作前查看状态，确认已有改动，再创建独立分支：

```powershell
git status --short
git switch -c feat/motor-control
```

修改相关源码和测试后，先运行主机逻辑测试，再执行对应工程的全量构建；参数变化还需同步本文及相关说明。检查差异，确认没有把编译产物、本机日志或工具配置加入提交：

```powershell
git diff --check
git diff
git status --short
```

按实际变更逐个暂存。例如一次电机控制修改可以包含：

```powershell
git add -- STC32G144K246_100Pin_Library/Example/Coreboard_Demo/E14_motor_test/user/main.c
git add -- tests/motor/test_motor.c tests/motor/run.ps1 tests/motor/README.txt
git add -- docs/development-workflow.md
git diff --cached
git commit -m "Update motor control and matching checks"
git push -u origin HEAD
```

推送后在 GitHub 创建 Pull Request，目标选择仓库默认分支。描述本次行为变化、主机测试结果、Keil 构建结果，以及硬件是否实际验证。没有执行的步骤明确标为未验证。评审通过后按项目流程合并，再同步本地默认分支。保留逐飞源码中的版权与许可证声明。

Keil 命令行参数依据：[Keil µVision Command Line](https://www.keil.com/support/man/docs/uv4/uv4_commandline.asp)。VS Code 默认构建任务及 Problems 解析依据：[VS Code Tasks](https://code.visualstudio.com/docs/debugtest/tasks)。板卡与库的使用说明见上游目录中的 [`README.md`](../STC32G144K246_100Pin_Library/README.md) 及随仓库提供的核心板手册。
