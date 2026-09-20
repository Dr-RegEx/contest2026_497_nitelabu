# Camera V4L2 frame interval contract

2026-09-19. Applies to the S31 fixed-configuration OV2640 and OV3660 bridges.

The old bridge returned 1/30 from `get_frame_interval`, while accepting arbitrary nonzero interval requests without configuring sensor timing or frame skipping. Merely returning `-ENOTTY` from that callback is insufficient: the capture upper half falls back to its stored 1/30 default and advertises `V4L2_CAP_TIMEPERFRAME`.

Added an opt-in `imgsensor_ops_s.frame_interval_unsupported` flag. The S31 bridge enables it: G_PARM and S_PARM return `-ENOTTY`, and G_PARM clears its output before returning. Internal capture scheduling defaults remain available for starting the fixed sensor configuration; they are not reported as measured frame timing. Other sensor drivers retain the existing behavior when the new trailing flag is zero. The lower callback no longer invents a 30 fps result.

Validation: `python3 tools/tests/camera-frame-interval/run.py` compiles the actual production G_PARM/S_PARM functions with ASan/UBSan and checks unsupported timing both before and during capture, no change to stored interval, no timing callback/validation on rejected operations, and existing supported-driver read/set/busy/error-fallback behavior. Passed. This is a host API regression, not camera acquisition or frame-rate evidence.

The updated camera candidate must be fully rebuilt because the sensor operations structure changed. Its image/build evidence is tracked with OV3660 integration. Actual timing must be measured with a connected sensor; neither the framework default nor the upstream PLL comment is a measured result.
