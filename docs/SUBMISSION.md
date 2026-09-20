# 官方提交状态

本次从官仓 `4fd374b67f5149d9190f8e03b50de0949b69538e` 重新建立提交，
不继承旧复刻仓提交链。作品载荷来源为旧仓 `b80bd7b6e93e897712c5966ba0049f0496b4d00c`，
保留既有实测边界与证据，更新正式仓入口。旧仓已在本机完整备份。

## 尚未完成的提交要求

- 作品仓 PR、CLA 检查及合入：以 GitHub 实际状态为准。
- 下列公共源码修改需另向官方 `dev-ai-contest-2026` 发起 PR，由组委会审核；当前补丁归档不是这些 PR 的替代品。
- AI 日志为选定真实会话，其他队员与历史会话尚待归集。

| 本地项目 | 官方仓库 | 当前状态 |
|---|---|---|
| `external` | [open-vela/external](https://github.com/open-vela/external) | 已归档补丁，尚未提交公共仓 PR |
| `external/ffmpeg/ffmpeg` | [open-vela/external_ffmpeg](https://github.com/open-vela/external_ffmpeg) | 已归档补丁，尚未提交公共仓 PR |
| `external/liblc3/liblc3` | [open-vela/external_liblc3](https://github.com/open-vela/external_liblc3) | 已归档补丁，尚未提交公共仓 PR |
| `external/libssh/libssh` | [open-vela/external_libssh](https://github.com/open-vela/external_libssh) | 已归档补丁，尚未提交公共仓 PR |
| `external/unqlite/unqlite` | [open-vela/external_unqlite](https://github.com/open-vela/external_unqlite) | 已归档补丁，尚未提交公共仓 PR |
| `external/zblue/zblue` | [open-vela/external_zblue](https://github.com/open-vela/external_zblue) | 已归档补丁，尚未提交公共仓 PR |
| `frameworks/connectivity/bluetooth` | [open-vela/frameworks_bluetooth](https://github.com/open-vela/frameworks_bluetooth) | 已归档补丁，尚未提交公共仓 PR |
| `frameworks/multimedia/media` | [open-vela/frameworks_multimedia_media](https://github.com/open-vela/frameworks_multimedia_media) | 已归档补丁，尚未提交公共仓 PR |
| `nuttx` | [open-vela/nuttx](https://github.com/open-vela/nuttx) | 已归档补丁，尚未提交公共仓 PR |
| `apps` | [open-vela/nuttx-apps](https://github.com/open-vela/nuttx-apps) | 已归档补丁，尚未提交公共仓 PR |
| `nuttx/fs/littlefs/littlefs` | [open-vela/nuttx_fs_littlefs_littlefs](https://github.com/open-vela/nuttx_fs_littlefs_littlefs) | 已归档补丁，尚未提交公共仓 PR |
| `packages/ai_agent` | [open-vela/packages_ai_agent](https://github.com/open-vela/packages_ai_agent) | 已归档补丁，尚未提交公共仓 PR |
| `tests` | [open-vela/tests](https://github.com/open-vela/tests) | 已归档补丁，尚未提交公共仓 PR |
| `nuttx/libs/libxx/libcxx/libcxx` | [open-vela/nuttx_libs_libxx_libcxx](https://github.com/open-vela/nuttx_libs_libxx_libcxx) | 已归档补丁，尚未提交公共仓 PR |

HAL 另有第三方依赖补丁，版本、来源及校验见 `workspace/dependencies/`。

## 验证边界

本次重建复核源码载荷 SHA256、manifest XML、提交历史和文件完整性。
已有独立构建记录见 `docs/reproduction/`；本次没有重复构建、刷板或新增实板通过结论。
