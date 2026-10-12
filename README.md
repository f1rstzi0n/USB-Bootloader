# USB-Bootloader

[f1rstzi0n/USB-Bootloader](https://github.com/f1rstzi0n/USB-Bootloader) 是 STM32F446VET6 裸机 HID Bootloader 的私有仓库，负责签名镜像验证、内部 Flash 升级、最后提交及应用启动。本地文件夹与 CubeIDE 工程名保持 `Test446-Boot`。

伙伴 [USB-HID-release](https://github.com/f1rstzi0n/USB-HID-release)（`Test446`）负责 FreeRTOS、HID + 只读 MSC 及参数业务。两个工程各自包含源码、驱动与 Debug / Release 构建定义，可独立克隆和构建；共享源码与公钥通过各自 SHA-256 清单同步。

开发先读 [AGENTS.md](AGENTS.md)。`docs/reference` 保存原工程的协议与历史设计快照，目标结构和 12 MHz 配置按本地移植说明；研究与历史验收从下方归档标签追溯。按任务查阅：

- [移植说明与首装边界](docs/PORTING.md)
- [本地协议 v4.3](docs/reference/PROTOCOL.md) 与 [Boot 设计](docs/reference/BOOTLOADER_DESIGN.md)
- [软件验证、复现命令及实板待验项](docs/validation/USB-PORTING-20261012.md)

[tools/cubeide](tools/cubeide) 提供 CubeMX 后恢复、配置 / 共享核验、clean build 与 ELF 布局检查；主机回归位于 [tests/host](tests/host)。Boot Debug / Release 均使用 `-Os`，软件基线为 42,196 B、2/2 主机回归通过；12 MHz 目标板尚未烧录，硬件验收待执行。调试 launch 仅加载符号，首装必须使用匹配公钥的签名工厂 HEX。

来源是 [USB-HID 归档标签 `usb-hid-archive-20261012`](https://github.com/f1rstzi0n/USB-HID/tree/usb-hid-archive-20261012)，保留通信原型与源板历史证据；后续代码在 APP / Boot 两仓库开发。本仓库不包含签名发布工具，现有离线 CNG 打包与首装工具说明位于[归档发布工具](https://github.com/f1rstzi0n/USB-HID/blob/usb-hid-archive-20261012/tools/firmware/README.md)。源板历史实测不替代本目标验收。

来源归档提交：[3e3431e8dc19ff9b2ef52c7b1172894fe702392d](https://github.com/f1rstzi0n/USB-HID/commit/3e3431e8dc19ff9b2ef52c7b1172894fe702392d)。
