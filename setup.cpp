// scrollfix-setup: 一次性的安装向导。
// 用系统对话框带用户走完：安装 -> 登录自启 -> 辅助功能授权 -> 完成，结束后删除自己。
// 由 install.sh 编译并运行，不需要单独使用。
#include <CoreFoundation/CoreFoundation.h>
#include <mach-o/dyld.h>
#include <notify.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static bool gZh = false;

static CFStringRef S(const char *zh, const char *en) {
    return CFStringCreateWithCString(nullptr, gZh ? zh : en, kCFStringEncodingUTF8);
}

// 阻塞式对话框。点了主按钮返回 true。
static bool ask(CFStringRef header, CFStringRef message, CFStringRef ok, CFStringRef cancel) {
    CFOptionFlags r = 0;
    CFUserNotificationDisplayAlert(0, kCFUserNotificationNoteAlertLevel, nullptr, nullptr, nullptr,
                                   header, message, ok, cancel, nullptr, &r);
    return (r & 0x3) == kCFUserNotificationDefaultResponse;
}

static int gReadyToken = 0;
static bool ready() {
    uint64_t state = 0;
    notify_get_state(gReadyToken, &state);
    return state == 1;
}

static std::string xmlEscape(const std::string &s) {
    std::string out;
    for (char c : s) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else out += c;
    }
    return out;
}

static void openAccessibilityPane() {
    system("open 'x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility'");
}

// 显示“等待授权”对话框，授权生效后自动关闭。返回 false 表示用户取消。
static bool waitForPermission() {
    for (;;) {
        if (ready()) return true;

        const void *keys[] = {kCFUserNotificationAlertHeaderKey, kCFUserNotificationAlertMessageKey,
                              kCFUserNotificationDefaultButtonTitleKey,
                              kCFUserNotificationAlternateButtonTitleKey};
        const void *vals[] = {
            S("第 2 步 / 共 2 步：授权", "Step 2 of 2: Grant permission"),
            S("请在“系统设置 → 隐私与安全性 → 辅助功能”的列表里打开 scrollfix 的开关。\n\n"
              "打开后这个窗口会自动关闭。",
              "In System Settings → Privacy & Security → Accessibility, turn on the switch for "
              "scrollfix.\n\nThis window closes by itself once it is on."),
            S("打开系统设置", "Open System Settings"), S("取消", "Cancel")};
        CFDictionaryRef dict = CFDictionaryCreate(nullptr, keys, vals, 4,
                                                  &kCFTypeDictionaryKeyCallBacks,
                                                  &kCFTypeDictionaryValueCallBacks);
        SInt32 err = 0;
        CFUserNotificationRef note =
            CFUserNotificationCreate(nullptr, 0, kCFUserNotificationNoteAlertLevel, &err, dict);
        CFRelease(dict);
        if (!note) {  // 无法显示对话框时退回到静默等待
            while (!ready()) sleep(1);
            return true;
        }

        for (;;) {
            CFOptionFlags r = 0;
            if (CFUserNotificationReceiveResponse(note, 1.0, &r) != 0) {  // 超时，还没人点
                if (ready()) {
                    CFUserNotificationCancel(note);
                    CFRelease(note);
                    return true;
                }
                continue;
            }
            CFRelease(note);
            if ((r & 0x3) != kCFUserNotificationDefaultResponse) return false;
            openAccessibilityPane();
            break;  // 重新显示等待对话框
        }
    }
}

int main() {
    if (CFArrayRef langs = CFLocaleCopyPreferredLanguages()) {
        if (CFArrayGetCount(langs) > 0)
            gZh = CFStringHasPrefix((CFStringRef)CFArrayGetValueAtIndex(langs, 0), CFSTR("zh"));
        CFRelease(langs);
    }

    // 自己的位置；编译好的 scrollfix 放在同一个目录
    char buf[4096];
    uint32_t len = sizeof(buf);
    if (_NSGetExecutablePath(buf, &len) != 0) return 1;
    std::error_code ec;
    fs::path self = fs::canonical(buf, ec);
    if (ec) return 1;
    fs::path staged = self.parent_path() / "scrollfix";
    auto cleanup = [&] {
        fs::remove(staged, ec);
        fs::remove(self, ec);  // 向导用完即删
    };

    const char *homeEnv = getenv("HOME");
    if (!homeEnv || !fs::exists(staged)) return 1;
    fs::path home = homeEnv;
    fs::path destDir = home / "Library/Application Support/ScrollFix";
    fs::path dest = destDir / "scrollfix";
    fs::path agentDir = home / "Library/LaunchAgents";
    fs::path plist = agentDir / "local.scrollfix.plist";
    std::string domain = "gui/" + std::to_string(getuid());

    if (!ask(S("安装 ScrollFix", "Install ScrollFix"),
             S("ScrollFix 让触控板保持自然滚动、滚轮鼠标使用传统滚动。\n\n"
               "它会安装到你的用户目录并在登录时自动启动，没有窗口和图标。\n\n"
               "第 1 步 / 共 2 步：安装",
               "ScrollFix keeps natural scrolling on the trackpad and classic scrolling on a "
               "wheel mouse.\n\nIt installs into your user folder and starts at login, with no "
               "window or icon.\n\nStep 1 of 2: Install"),
             S("安装", "Install"), S("取消", "Cancel"))) {
        cleanup();
        return 0;
    }

    // ---- 安装 ----
    notify_register_check("local.scrollfix.ready", &gReadyToken);
    system(("launchctl bootout " + domain + "/local.scrollfix 2>/dev/null").c_str());
    notify_set_state(gReadyToken, 0);  // 清掉旧进程留下的状态

    fs::create_directories(destDir, ec);
    fs::create_directories(agentDir, ec);
    fs::copy_file(staged, dest, fs::copy_options::overwrite_existing, ec);
    bool ok = !ec;
    if (ok) {
        std::ofstream f(plist);
        f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
             "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
             "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
             "<plist version=\"1.0\">\n<dict>\n"
             "  <key>Label</key><string>local.scrollfix</string>\n"
             "  <key>ProgramArguments</key><array><string>"
          << xmlEscape(dest.string())
          << "</string></array>\n"
             "  <key>RunAtLoad</key><true/>\n"
             "  <key>KeepAlive</key><true/>\n"
             "  <key>ProcessType</key><string>Interactive</string>\n"
             "</dict>\n</plist>\n";
        ok = f.good();
    }
    if (ok)
        ok = system(("launchctl bootstrap " + domain + " '" + plist.string() + "'").c_str()) == 0;
    if (!ok) {
        ask(S("安装失败", "Installation failed"),
            S("无法写入用户目录或注册登录自启。", "Could not write to your user folder or "
                                                  "register the login item."),
            S("好", "OK"), nullptr);
        cleanup();
        return 1;
    }

    // ---- 授权 ----
    // 之前授权过(重装)的话会直接生效，给它一点时间
    for (int i = 0; i < 15 && !ready(); i++) usleep(100000);
    if (!ready()) openAccessibilityPane();

    if (waitForPermission()) {
        ask(S("完成", "All set"),
            S("ScrollFix 已经在后台运行，以后登录时会自动启动。\n\n这个向导现在会删除自己。",
              "ScrollFix is running in the background and will start at login.\n\n"
              "This setup guide will now delete itself."),
            S("好", "OK"), nullptr);
        // 系统“自然滚动”关着时只能靠翻转触控板实现，Safari 等 App 里会出现滚动冲突
        Boolean exists = false;
        Boolean natural = CFPreferencesGetAppBooleanValue(
            CFSTR("com.apple.swipescrolldirection"), kCFPreferencesAnyApplication, &exists);
        if (exists && !natural &&
            ask(S("建议打开“自然滚动”", "Turn on “Natural scrolling”"),
                S("你的系统“自然滚动”目前是关闭的。这样用的话，触控板在 Safari 等 App 里滚动"
                  "可能会冲突。\n\n请在“系统设置 → 触控板 → 滚动与缩放”里打开“自然滚动”，"
                  "ScrollFix 会在几秒内自动适应，鼠标仍然是传统滚动。",
                  "“Natural scrolling” is currently off. In that mode trackpad scrolling can "
                  "misbehave in Safari and some other apps.\n\nTurn it on under System Settings → "
                  "Trackpad → Scroll & Zoom. ScrollFix adapts within a few seconds and the mouse "
                  "keeps classic scrolling."),
                S("打开触控板设置", "Open Trackpad Settings"), S("以后再说", "Later"))) {
            system("open 'x-apple.systempreferences:com.apple.Trackpad-Settings.extension'");
        }
    } else {
        ask(S("尚未授权", "Permission not granted yet"),
            S("ScrollFix 已安装，但要等你在“辅助功能”里打开 scrollfix 的开关后才会生效。"
              "之后随时可以去打开，不需要重新安装。",
              "ScrollFix is installed but will only work once you turn on scrollfix under "
              "Accessibility. You can do that any time; no reinstall needed."),
            S("好", "OK"), nullptr);
    }
    cleanup();
    return 0;
}
