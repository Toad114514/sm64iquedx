# ---------------------------------------------------------------------------
# 只负责「把已经构建好的成品装成 RPM」，不自己编译。
#
# 成品由 apply_patches.sh + 构建流程产出在 <仓库根>/dist/，那里应当是一个完整
# 可运行的目录（sm64coopdx + res/ + lang/ + 各 .ini …）。
#
# 正常入口是同目录的 mkrpm.sh：它先把「成品 + 启动器 + 图标」铺进一个 staging
# 目录，再用 --define 'stage …' 交给本 spec。手工跑也行：
#
#   rpmbuild -bb platform/rpm/sm64coopdx-iquedx.spec \
#       --define "_topdir $(mktemp -d)" \
#       --define "stage  $PWD/stage" \
#       --define "ver 1.5.1"
#
# %{stage} 下的目录结构 = 要装进 %{buildroot} 的结构，本 spec 不关心仓库布局，
# 所以仓库目录怎么搬都不会影响这里。
# ---------------------------------------------------------------------------

%{!?stage:%global stage %{_sourcedir}/stage}
%{!?ver:%global ver 1.5.1}

Name:           sm64coopdx-iquedx
Version:        %{ver}
Release:        1%{?dist}
Summary:        Super Mario 64 Co-op Deluxe with iQue Chinese text support
Summary(zh_CN): 带神游（iQue）简体中文文本包的超级马力欧 64 联机豪华版
License:        MIT
Group:          Amusements/Games
URL:            https://github.com/Toad114514/sm64iquedx

# 成品是别处编好的 ELF，这里三个开关都必须关掉：
#   * debug_package：没有源码可分，开着会让 rpmbuild 报「找不到调试信息」
#   * __os_install_post：其中包含 strip 与 build-id 重链接，会改掉二进制，
#     还会给每个文件写 .build-id 符号链接
#   * AutoReqProv：自动依赖按构建机的 rpm 宏探测，交叉打包（x86_64 上打
#     aarch64）会写出构建机的库名；依赖改为下面按发行版手写
%global debug_package %{nil}
%global __os_install_post %{nil}
AutoReqProv:    no

# 库名按发行版分支写：openSUSE 不叫 SDL2，Fedora/RHEL 也不叫 libSDL2-2_0-0。
%if 0%{?suse_version}
Requires:       libSDL2-2_0-0
Requires:       libglvnd
Requires:       libz1
Requires:       libcurl4
Requires:       libstdc++6
%else
Requires:       SDL2
Requires:       mesa-libGL
Requires:       zlib
Requires:       libcurl
Requires:       libstdc++
%endif

%description
Super Mario 64 Co-op Deluxe patched with the iQue (Shenyou) Simplified Chinese
text pack, bundled with iQue glyph tables and localised textures.

The game itself lives in /opt/sm64coopdx-iquedx together with a freedesktop
launcher and icon. A ROM is required to play and is NOT included.

%description -l zh_CN
带神游（iQue）简体中文文本包的超级马力欧 64 联机豪华版，附神游字模与本地化贴图。

游戏本体安装在 /opt/sm64coopdx-iquedx，并附带 freedesktop 启动器与图标。
启动需要自备 ROM，包里不包含任何 ROM。

%prep
# 没有源码可解包：%{stage} 是上游脚本铺好的目录。

%build
# 不编译：成品已经在 %{stage} 里。

%install
rm -rf %{buildroot}
cp -a %{stage}/. %{buildroot}/

%files
# 直接列目录：rpm 会把目录下所有内容递归收进包（包括 . 开头的文件），
# 比写 /opt/sm64coopdx-iquedx/* 稳妥——后者会漏掉隐藏文件并触发
# 「Installed (but unpackaged) file(s) found」。
/opt/sm64coopdx-iquedx
/usr/share/applications/sm64coopdx-iquedx.desktop
/usr/share/icons/hicolor/256x256/apps/sm64coopdx-iquedx.png

%post
# 刷新桌面/图标缓存；没有对应命令时静默跳过（最小化安装的容器里常见）。
update-desktop-database -q /usr/share/applications >/dev/null 2>&1 || :
gtk-update-icon-cache -q -t /usr/share/icons/hicolor >/dev/null 2>&1 || :

%postun
update-desktop-database -q /usr/share/applications >/dev/null 2>&1 || :
gtk-update-icon-cache -q -t /usr/share/icons/hicolor >/dev/null 2>&1 || :

%changelog
* Mon Nov 09 2026 Toad114514 <xiaolan2332021@163.com> - 1.5.1-1
- 首次打包：只装成品目录 + 启动器 + 图标，依赖按发行版分支
