#1279 bounded Flash handoff trace

1273/1277 first read emitted FLBCREMU in full, followed by no completion180s.
This candidate extends both read and dispatch traces to the first8 audio-active
reads, and traces callback completion/wakeup. Default-off flag remains enabled
only in the diagnostic profile. It is not an acceptance image.

D=PSRAM dispatch entered, Q=work_queue returned, X=worker execute returned,
H=result published/before semaphore post, J=semaphore post returned,
W=caller wait returned/result read, Z=caller cancellation state restored.
F L B C R E M U retain1273 meanings. J may follow W/Z due to preemption;
the callback snapshots its trace flag before posting and never dereferences
the caller-owned request after posting. Up to8 requests are logged.

Original persistent /flash/audio_file.aac remains unchanged since1270 full
readback. Boot1280 mounts without formatting and checks its length; stage1281,
start1282 and run1283 reuse it. Do not confuse prior SHA verification with a
new readback. Full original WAV remains the required acceptance after repair.
