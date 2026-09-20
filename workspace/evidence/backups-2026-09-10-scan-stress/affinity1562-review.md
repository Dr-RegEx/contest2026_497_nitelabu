# Affinity migration candidate1562

Observed1556: a pthread requested CPU1 but its first PIE opcode trapped onCPU0.1558/1560 create-time affinity passed, so this is not taken as a failure of every PIE instruction.

Source: nxsched_set_affinity updates the mask and asks nxsched_set_priority to reposition an excluded running task. The running path calls nxsched_switch_running, whose previous search only considered ready tasks above a priority threshold. With no eligible peer, it returnedfalse and retained the task on an excluded CPU.

Candidate uses the existing remove-readytorun path (which installs idle or an eligible replacement) followed by add-readytorun (which selects a CPU from the new mask and delivers an IPI). It preserves the scheduler-lock deferral and CPU_LOCKED handling. This shared scheduler edit is not yet target accepted.

Host test extracts the actual function, models queue operations, and checks unchanged mask, scheduler lock, and excludedCPU with empty queue. Old source exits10 at the required migration, candidate exits0. First harness compile had an output-file/directory name collision; original log kept, corrected harness uses .elf. This is a selection-logic regression, not a claim of host-emulated SMP correctness.

Target1563 starts both workers explicitly onCPU0, alternates affinity20times (checks actualCPU immediately each time), endsCPU1, then runs100 vector-add/full-bank sleep-switch rounds each. This tests actual scheduler/exception integration. Context preservation while intentionally using PIE onCPU0 is not supported or promised.
