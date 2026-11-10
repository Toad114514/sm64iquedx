; Windows 安装向导（Inno Setup 6）。
;
; 由 platform/windows/mkexe.sh 调用 ISCC 编译：
;   ISCC.exe /Qp /DMyAppVersion=1.5.1 /DMySourceDir=C:\...\dist
;                /DMyOutputDir=C:\...\sm64iquedx /DMyCnLang=C:\...\ChineseSimplified.isl
;                platform/windows/sm64coopdx-iquedx.iss
; （/DMyCnLang 只在找到简体中文语言文件时才传。）
;
; ⚠️ 本文件是 UTF-8 **带 BOM**，别去掉：Inno Setup 6 只在看到 BOM 时才把 .iss
;    当 UTF-8 解析，否则按系统 ANSI 代码页解释，下面所有中文会变成乱码。
;    改完可以用 `head -c 3 file | od -An -tx1` 确认是 ef bb bf（mkexe.sh 会查）。

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif

#ifndef MySourceDir
  #error 必须用 /DMySourceDir=<成品目录> 指定要打包的 dist
#endif

#ifndef MyOutputDir
  #define MyOutputDir "."
#endif

#define MyAppName "Super Mario 64 Coop Deluxe（神游汉化版）"
#define MyAppShortName "SM64 Coop Deluxe"
#define MyAppPublisher "sm64iquedx"
#define MyAppExeName "sm64coopdx.exe"

[Setup]
; AppId 决定「这是同一个程序」：升级时据此覆盖旧版而不是并存。生成一次就永远别改。
AppId={{6F1D2A9C-8B7E-4C31-9A5D-2E4F7B8C1D03}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppShortName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\sm64coopdx-iquedx
DefaultGroupName={#MyAppShortName} 神游汉化
DisableProgramGroupPage=yes
; 允许不提权装到当前用户目录（不写注册表全局项、不碰 Program Files 就不需要 UAC）
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
; 游戏只有 64 位构建
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
OutputDir={#MyOutputDir}
OutputBaseFilename=sm64iquedx-{#MyAppVersion}-windows-x86_64-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppShortName} 神游汉化
SetupLogging=yes
; 装完不需要立刻重启
RestartIfNeededByRun=no

; 简体中文界面：**官方 Inno Setup 不带简体中文**（Languages\ 里只有 26 种官方
; 语言），所以仓库在 platform/windows/ChineseSimplified.isl 自带一份（取自
; jrsoftware/issrc 的 Files/Languages/）。由 mkexe.sh 找到后传
; /DMyCnLang=<绝对路径>；找不到就退回英文界面，而不是整个编译失败。
#ifdef MyCnLang
[Languages]
Name: "cn"; MessagesFile: "{#MyCnLang}"
Name: "en"; MessagesFile: "compiler:Default.isl"
#else
[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
#endif

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加任务："; Flags: unchecked

[Files]
; 成品目录是平铺的（主程序 + dynos/ lang/ mods/ palettes/ 各 dll），整目录递归装进去
Source: "{#MySourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
; WorkingDir 必须写：从开始菜单/桌面启动时进程的工作目录不是游戏目录，
; 而 Windows 版部分资源是按相对路径找的。
Name: "{group}\{#MyAppShortName} 神游汉化"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{group}\卸载 {#MyAppShortName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppShortName} 神游汉化"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Description: "立即启动游戏"; Flags: nowait postinstall skipifsilent
