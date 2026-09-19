# 公共仓修改与复现

唯一补丁入口是仓库根目录的 `workspace/apply-openvela-snapshot.sh`。

`workspace/manifest-locked.xml` 锁定 Gitee open-vela 公共仓可获取的基线。
`workspace/snapshot.json` 明确记录项目路径、基线和新增文件；`workspace/patches/`
包含本地已提交和未提交的全部源码差异。不能仅对本机 HEAD 导出 diff，
否则其他机器会缺失尚未发布的适配提交。

本目录是实际板级代码的可读副本；复现脚本已通过公共仓补丁将同一代码放入
`nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/`，无需再复制。

本次仅用于复刻仓团队复现，尚未向比赛官仓或公共仓发起 PR。
