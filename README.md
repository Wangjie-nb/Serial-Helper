# Serial-Helper 串口调试助手

基于 Qt 5.12 (Widgets + QSerialPort) 的 Windows 串口调试工具。

## 功能

- 串口参数配置：端口、波特率、数据位、校验位、停止位、流控，支持热插拔自动刷新端口列表
- 文本 / HEX 双模式发送与接收显示，两者互不影响
- 发送新行（自动追加 `\r\n`）、接收时间戳（毫秒精度）、自动换行
- 收/发字节计数，接收内容可保存为 txt
- 多文本区 10 行快捷发送：每行独立勾选「按 HEX 发送」、独立的发送按钮，支持循环输入按间隔轮发
- 多文本配置保存 / 载入 / 重置
- 运行模式 / 下载模式切换（隐藏历史与参数面板）
- 历史记录去重显示

## 构建

环境：Qt 5.12.9 + MinGW（32 位或 64 位均可）

用 Qt Creator 打开 `chaunkoutiaoshi.pro`，选择对应 Kit 直接构建即可。

命令行方式：

```bat
qmake chaunkoutiaoshi.pro -spec win32-g++
mingw32-make -f Makefile.Release
```

## 目录说明

| 文件 | 说明 |
|---|---|
| `main.cpp` | 程序入口 |
| `widget.h/.cpp` | 主窗口逻辑（串口收发、多文本、保存载入等） |
| `widget.ui` | Qt Designer 界面文件 |
| `res.qrc` | 资源文件 |
| `app.ico` / `mianicon.png` | 程序图标 |
| `appicon.rc` | 旧的手写资源脚本（当前未使用，`.pro` 用 `RC_ICONS`） |