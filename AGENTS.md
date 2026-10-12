# AGENTS.md

本文件是 USB-Bootloader 仓库 agent 指南的唯一真源；后续指南只修改本文件。文档与注释用简体中文，代码标识符保留英文。

## 工程与读取入口

- GitHub 私有仓库为 [f1rstzi0n/USB-Bootloader](https://github.com/f1rstzi0n/USB-Bootloader)；本地文件夹与 CubeIDE 工程名保持 `Test446-Boot`。
- 后续代码开发仅在本 Boot 工程及伙伴 APP 工程 [USB-HID-release](https://github.com/f1rstzi0n/USB-HID-release)（文件夹 / CubeIDE 名 `Test446`）进行；[USB-HID](https://github.com/f1rstzi0n/USB-HID/tree/usb-hid-archive-20261012) 保存通信原型与历史验收，作为归档来源。
- **修改通信、板级 adapter、启动或恢复行为前**，先读 [移植说明](docs/PORTING.md)；目标路径、12 MHz 板级配置及当前实现边界以该文为准。
- **修改 HID 报表、分片、服务、包或身份契约前**，先读本地 [协议规范](docs/reference/PROTOCOL.md)，尤其 §7.13；当前为 v4.3、wire major 4。协议是线级真源，两工程保持同版本副本。
- **修改签名、擦写、提交、跳转、链接布局或首装流程前**，先读 [Boot 设计](docs/reference/BOOTLOADER_DESIGN.md) 与 [移植验证记录](docs/validation/USB-PORTING-20261012.md)。`docs/reference` 保存原工程的协议与历史设计快照，研究与历史验收追溯归档标签；源工程路径、旧构建数字与历史实板结论仅供追溯，目标结构按本地移植说明，配置与待验边界以移植验证记录为准。
- **修改时钟、引脚、CubeMX 配置或复核交付前**，先读移植说明 §6–7 与移植验证记录的配置、布局和实板验收部分；新增证据记录在目标工程，不用源板结果替代。

## Boot 边界与安全不变量

- Boot 是裸机、HID-only；职责是镜像验证、签名升级、提交与启动。FreeRTOS、MSC、W25Q64、参数目录和业务服务属于 APP。保留原工程未完成 RS485 实现与调用的原文及注释状态。
- 主循环保持 `usb_io_task()` → `usb_proto_task()` → `fw_platform_task()` → 喂狗。板级 Flash / 复位 / 跳转 adapter 位于 `App/firmware/fw_platform.c`；升级状态机位于 `App/firmware/fw_update.c`，只读镜像验证位于 `fw_image.*`。
- BEGIN 先验签，最后一片应答实际 HID IN 完成才授权擦除；提交成功、入队或迟到完成不能替代该事件。只有内部 Flash 扇区 4–7 可擦写；Boot、公钥与 Option Bytes 维护走 SWD。
- FINISH 完整回读正文，验证摘要与向量，写入并回读 manifest 后最后写入并回读 commit。无效、未提交或被中断的应用留在 Boot，整包重刷；合法签名但运行卡死的应用通过 SWD 维修。
- 长验证 / 擦写保持 IWDG 服务；`fw_validation_progress()` 只 pump IO 与喂狗，不递归协议解析。保留连接代次和实际 IN 授权，旧报表与旧完成事件不能跨重连授权动作。
- 固定布局：Boot 64 KiB 自 `0x08000000`；metadata 512 B 自 `0x08010000`；APP 向量 / 正文自 `0x08010200`；`0x2001FFF8` 保留 8 B `.boot_request (NOLOAD)`，普通 RAM、清零与栈不得覆盖。
- 保留 12 MHz HSE bypass、CPU 180 MHz、板级 PLLSAIP USB 48 MHz、TIM7 HAL 时基与 IWDG。PE4 保持无上下拉输入高阻；各外设 CS 在访问前取消选中。TinyUSB 独占 USB 内核和唯一 OTG_FS IRQ。
- 本次建仓与标签保持 `SW110048.1.4.9`、AFW 包、发布公钥、PID / UID 及地址契约。clean-room：沿用自研 BVPV-USB，不引入竞品代码、报文格式或身份。

## 源码与共享副本

- 自有 C 在每个代码块首声明变量，再赋值和使用；第三方源码及许可保留原样。
- 共享实现、固件契约与公钥是两工程内同版本副本，构建各自独立。变更同时同步两份源码、测试与 [共享 SHA-256 清单](docs/shared-source-manifest.json)，运行 `node tools/cubeide/verify_shared.js`；显式传入伙伴目录可比较两份副本。保持公钥头、`release-public/public-key.bin` 与主机信任公钥一致。

## 构建与完成条件

唯一固件构建定义是本工程 `.project` / `.cproject` / `.settings`；使用 `tools/cubeide`，不维护第二套固件编译清单。CubeMX 再生成后，在工程根目录依次恢复并核验配置、编码和共享副本：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/cubeide/apply_sources.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/cubeide/verify_generated.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/cubeide/verify_owned_c.ps1
node tools/cubeide/verify_shared.js
```

源码或生成配置变更完成后执行 Debug / Release clean build（两配置均 `-Os`），核验布局并运行 Boot 主机回归：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/cubeide/build.ps1 -Config All
node tools/cubeide/verify_builds.js
cmake -S tests/host -B build-host -G Ninja -DCMAKE_C_COMPILER=gcc
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

完成条件是配置 / 编码 / 共享检查通过、两配置零错误且 Boot ≤64 KiB、固定地址与 NOLOAD 保留成立、Boot 2/2 回归通过。共享变更同时完成 APP 的检查与回归；查看生成 GPIO / ISR，自动检查不能替代此复核。当前软件基线与实板待验清单见移植验证记录；本目标尚未烧录，USB 时序、IWDG 余量及升级中断恢复仍待实测，标签不表示硬件验收完成。

首装通过 SWD 安装匹配公钥及契约的签名 `.factory.hex`；已装正确 Boot 的维护使用 `.signed-app.hex`，USB 升级使用 `.afw`。本仓库没有签名发布工具，发布入口见 README 的归档来源；裸 ELF / BIN / HEX 不能替代签名首装。`Test446-Boot Debug.launch` 仅加载符号，首装后再调试，CubeMX 再生成后复核此设置。
