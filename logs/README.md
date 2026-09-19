# AI Coding 日志

`Dr-RegEx/manifest.json` 及其引用的 JSONL 来自本次复现修复的真实 Codex 会话。
这是截至导出时的选定可见对话，不是全部开发历史，也不是编造的模板。
导出不包括内部推理、系统指令或工具载荷。

Codex 原始 rollout 使用 `response_item` 格式，官方当前转换器按另一种
`message` 格式解析，因此先用 `workspace/tools/log-export/export-codex.py`
读取真实的用户/助手文本，再调用官方 `snapshot_core.append_events` 和
manifest 写入器。官方校验器已通过；来源快照哈希与采集器版本记录在
`export-provenance.json`，便于本机追溯原始会话。

队员的其余历史会话尚需各自导出补齐。流程参考
[官方日志手册](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/ai_coding_log_guide.md)。
