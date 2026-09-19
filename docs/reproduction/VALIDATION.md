# 独立目录复现验证（2026-09-19）

- Ubuntu 22.04 / x86_64、Python 3.10.12、GCC 15.2.0、esptool 5.4.0。
- 公共源码按最终 manifest 在独立目录检出，本机 Git 对象缓存仅用于减少下载。
- 原来未发布的 NuttX / Apps / libcxx / tests HEAD 已改为公共基线；四个基线均实际从 Gitee fetch 成功。
- 14 个有源码修改的项目完成补丁应用；577 个修改/新增文件与原开发目录逐字节比较一致。
- NSH：成功生成 nuttx.bin。
- Camera：成功生成 nuttx.bin；不代表外接摄像头采集通过。
- demo-rmt-netapps-competition：成功生成 nuttx.bin 和配套 appfs.img。
- 三套构建使用独立输出目录，未引用原开发树的 HAL、生成头文件、ESP-IDF export 或构建输出。
- 真实选定 Codex 会话经官方 validate-log.py 验证通过。

产物大小及 SHA256 见 artifacts.json；完整最终配置和对应构建日志同目录保存。
NSH 日志是修正 esptool 版本后的最终构建，前一轮已完成所有源文件编译。
本次没有执行板上刷写、启动复测或重新认定任何 xTS 验收结果。

## 远端回拉核验

上传后通过 HTTPS 重新 clone 复刻仓，195 项复现载荷 SHA256 全部一致，
AI 日志再次通过官方校验。README 中的 repo init 命令实际执行成功；
展开 manifest 为 232 个公共源码项目和 1 个团队项目，团队项目 repo sync 成功。
旧开发目录另有 18 个通用 prebuilts 项目，不在当前 manifest 内；S31 构建通过
独立锁定的 Espressif 工具链完成，不依赖这些通用工具链。

未执行 232 个源码仓全部通过网络重新下载；独立构建使用 Git 对象缓存检出，
四个修正基线已单独通过 Gitee 网络 fetch 验证。
