TreeVisual {version} 使用说明
==========================

功能
- 美观目录树输出（Unicode/ASCII 分支字符）
- 多线程并行扫描（2-5倍加速）
- WebUI 模式，延迟加载树浏览器（--web）
- TUI 交互模式（-v）
- 国际化支持（中文/英文）
- WebGL 力导向图可视化（Beta）
- 大目录自动复制到剪贴板或保存为文件
- 显示隐藏文件开关（--hidden）
- 后台守护进程模式（--service）

用法
  ./tree [路径] [选项]

选项
  --hidden         显示隐藏文件
  -v               TUI 交互模式
  -w, --web        启动 WebUI 服务器 (http://127.0.0.1:7200/)
  --service [on|off|status]  后台守护进程模式
  --setting        打开设置面板
  -h, --help       显示帮助

示例
  ./tree                     当前目录树
  ./tree /path/to/dir        指定目录树
  ./tree --web               启动 WebUI 浏览器界面
  ./tree -v                  交互式终端界面
  ./tree --hidden            包含隐藏文件

许可证
MIT License (c) 2026 WinTerminal
