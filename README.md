# car：STC32G 小车开发工程

基于逐飞 STC32G144K246 100Pin CoreBoard、Keil C251 和 DRV8701E 双路有刷电机驱动，提供可在 VS Code 中使用的编译、日志、HEX 校验和主机逻辑测试流程。

**首次使用请从[环境配置与开发工作流](docs/development-workflow.md)开始。** 文档包含 Keil/STC 支持配置、常见错误定位、VS Code 任务、USB HID 烧录、接线和 Git 协作步骤。

## 当前功能

- E01：官方 LED 示例，用于验证工具链和下载流程。
- E14：P32 按一下，两轮沿固定方向持续运行；松开后再次按下停止。上电默认停止，长按不重复切换。
- 两路使用 17 kHz PWM；当前工作树的 `MOTOR_DUTY_PERCENT` 为 **30**。同占空比不保证实际同转速。
- 日志和新 HEX 必须通过构建脚本校验；编译成功与真实硬件运行结果分别记录。

当前 30% 版本已于 2026-10-17 通过 Keil 全量构建（0 Errors、0 Warnings）及 14 个固定场景、4 个固定种子随机序列的主机逻辑测试，仍需上板验证。此前仅确认过单路轻微转动，以及断开电机 12 V 时按键可切换蓝灯运行/停止状态；不要把主机测试当作电源、电流或轮速的实测。

## 开始开发

```powershell
git clone https://github.com/cyyye704/car.git
cd car
code .
```

完成[必要配置](docs/development-workflow.md)后，在仓库根目录运行：

```powershell
# 官方 LED 示例
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-led.ps1

# 双轮按键控制例程
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-motor.ps1

# 应用逻辑测试，需要本机 GCC
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/motor/run.ps1
```

VS Code 的 `Ctrl+Shift+B` 当前默认编译 **LED**。编译电机程序请使用“终端 → 运行任务 → STC32G: Rebuild motor test (Keil C251)”，或通过“配置默认生成任务”切换默认项。

## 常用文件

| 路径 | 用途 |
| --- | --- |
| [电机 main.c](STC32G144K246_100Pin_Library/Example/Coreboard_Demo/E14_motor_test/user/main.c) | 占空比、P32 按键和双轮启停控制 |
| [电机 Keil 工程](STC32G144K246_100Pin_Library/Example/Coreboard_Demo/E14_motor_test/mdk/seekfree.uvproj) | `stc32g144k246` 目标 |
| [VS Code 任务](.vscode/tasks.json) | LED/电机编译与日志查看 |
| [构建脚本](scripts/build-motor.ps1) | 调用 Keil、保存日志并验证新 HEX |
| [主机测试](tests/motor/README.txt) | 不连接硬件即可检查按键状态逻辑 |
| [环境配置与开发工作流](docs/development-workflow.md) | 新电脑配置、编译、烧录与 Git 协作 |
| [原始环境诊断记录](docs/STC32G-workflow.txt) | 本机历史排查证据，不代表新机器已完成配置 |
| [电机调试记录](docs/STC32G-motor-first-test.txt) | 接线、各版实测状态与操作说明 |

电机 HEX 由本地构建生成，路径为：

```text
STC32G144K246_100Pin_Library/Example/Coreboard_Demo/E14_motor_test/mdk/out_file/MOTOR_TEST.hex
```

构建日志和验证结果分别位于 `.build/motor/latest.log`、`.build/motor/latest-result.json`。这些产物不提交到 Git；不能用旧 HEX 代替失败构建的结果。

## 上板操作

烧录、复位或插拔 USB 前，先把驱动板拨到 **OFF**。核心板通过 USB 供电，12 V 只接电机驱动电源端。烧录后等蓝灯间歇闪烁，再接通电机电源、开启驱动，并架空车轮验证按键启停。运行时蓝灯常亮；异常时拨 OFF 并断开电机电源。完整 HID 下载步骤与接线表见[开发工作流](docs/development-workflow.md)。

## 许可证与来源

保留逐飞源码版权声明和 GPL 许可证，详见 [LICENSE](LICENSE) 与 [UPSTREAM.md](UPSTREAM.md)。本仓库不包含 Keil 安装程序或许可证。
