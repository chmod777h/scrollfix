# ScrollFix

让 macOS 的**触控板保持自然滚动**，同时让**滚轮鼠标使用传统滚动**。系统本身只有一个开关，两种设备只能一起改；ScrollFix 在后台把其中一种设备的滚动方向翻转过来。

- 用 C++ 写成，只有一个几十 KB 的后台程序
- 没有窗口、Dock 图标、菜单栏图标，不写日志
- 登录时自动启动
- 只安装在你的用户目录里，不需要管理员密码

*macOS only. Keeps natural scrolling on the trackpad and classic scrolling on a wheel mouse. The setup guide is shown in English on non-Chinese systems.*

## 安装

需要 Xcode 命令行工具（没有的话脚本会提示安装）。在终端里依次运行：

```bash
git clone https://github.com/chmod777h/scrollfix.git
```

```bash
bash scrollfix/install.sh
```

脚本会编译程序并弹出安装向导，只有两步：

1. **安装** — 点“安装”，程序被复制到用户目录并注册为登录自启。
2. **授权** — 向导会打开“系统设置 → 隐私与安全性 → 辅助功能”，把列表里 `scrollfix` 的开关打开。开关一打开，向导窗口自动关闭并提示完成。

向导结束后会删除自己和所有临时编译文件。下载下来的 `scrollfix` 文件夹之后也可以删掉。

## 它是怎么判断设备的

触控板产生的是“连续”滚动事件，普通滚轮鼠标不是，ScrollFix 据此区分两者：

| 系统“自然滚动”开关 | ScrollFix 的行为 |
| --- | --- |
| 开 | 翻转滚轮鼠标 |
| 关 | 翻转触控板 |

所以无论开关是哪种状态，结果都是触控板自然滚动、鼠标传统滚动。

注意：

- 开关状态只在程序启动时读取一次。改了系统的“自然滚动”开关后，注销重新登录即可。
- Magic Mouse 也产生连续事件，会被当作触控板。
- 装了厂商驱动并开启“平滑滚动”的鼠标（如 Logi Options+）可能被误判成触控板，关掉平滑滚动即可。

## 安装了哪些东西

| 路径 | 作用 |
| --- | --- |
| `~/Library/Application Support/ScrollFix/scrollfix` | 后台程序 |
| `~/Library/LaunchAgents/local.scrollfix.plist` | 登录自启配置 |

它在界面上不可见，但 macOS 会在以下位置列出它：活动监视器、“系统设置 → 通用 → 登录项”、“系统设置 → 隐私与安全性 → 辅助功能”。

## 卸载

在终端里依次运行：

```bash
launchctl bootout gui/$(id -u)/local.scrollfix
```

```bash
rm -rf ~/Library/LaunchAgents/local.scrollfix.plist ~/Library/Application\ Support/ScrollFix
```

然后到“系统设置 → 隐私与安全性 → 辅助功能”，选中 `scrollfix` 点 “−” 把它从列表里移除。

## 常见问题

**装好了但方向没变。** 确认“辅助功能”里 `scrollfix` 的开关是开着的。如果是重新安装或更新过，旧的授权可能失效：在列表里把 `scrollfix` 用 “−” 删掉，再点 “+”，按 `Cmd+Shift+G` 输入 `~/Library/Application Support/ScrollFix/scrollfix` 重新添加。

**想暂时停用。** 运行卸载里的第一条命令即可停止；重新登录后会再次启动。

## 手动编译

```bash
clang++ -O2 -std=c++17 scrollfix.cpp -framework ApplicationServices -o scrollfix
```
