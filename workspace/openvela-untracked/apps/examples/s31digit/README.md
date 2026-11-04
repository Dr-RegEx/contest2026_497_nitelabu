# ESP32-S31 handwritten digit demo

Touch launcher: `s31digit`. Choose CAMERA or HANDWRITE DIGITS.
In handwriting, write one digit in the left box and tap RECOGNIZE;
CLEAR erases. EXIT in either application returns to the launcher.
Camera recording must be stopped before EXIT; opening CAMERA does not record.
`--home-test` runs three inference/camera open-close cycles without saving files. `s31digit --demo` shows the first ten
held-out MNIST images (not live handwriting). `s31digit --selftest` evaluates
the first 100 held-out images. No network, camera, recording, or SD writes.

Model: MNIST 784→64 ReLU→10 MLP, INT8 weights/activations and float rescaling.
Host training uses scikit-learn; firmware inference is standalone C, no Python
or cloud. Test-set accuracy 97.43% (10,000 images), not a promised accuracy for
all touchscreen handwriting. Preprocessing crops a bounding box, scales it
within 20×20 and centers mass on 28×28. Write a large single digit; ambiguous
or non-digit strokes can still receive a digit prediction. No confidence
calibration or rejection model is claimed.

Current `korvo-digit` profile is flat/single-core and runs scalar INT8. It
retains the Korvo camera/touch/SD commands but applications must run separately
to avoid framebuffer/touch conflicts. The launcher closes its devices before
entering either application and reopens them only after application cleanup. This profile does not include USB Host.
`CONFIG_EXAMPLES_S31DIGIT_SIMD` provides a CPU1-pinned PIE assembly dot backend,
requires ESP32S31_PIE and its SMP/S-mode configuration. That backend has not
been model-tested on this profile; do not call the current demo SIMD accelerated.
Existing s31simd arithmetic/context tests are independent prior evidence.

Reproduce:

```sh
python3 -m venv tools/s31-digit-venv
tools/s31-digit-venv/bin/pip install -r tools/s31-digit/requirements.txt
OPENBLAS_NUM_THREADS=2 tools/s31-digit-venv/bin/python tools/s31-digit/train.py
OPENBLAS_NUM_THREADS=2 tools/s31-digit-venv/bin/python tools/s31-digit/check.py
bash backups/korvo-camera-display-and-peripherals/build-digit.sh
```

Training seed=31, split=90%/10% of 60,000 training images; test images are never
used to fit weights or choose the hidden activation scale. 25 epochs max.
Generated model header is committed as source for builds without training.
Metrics in metrics.json and c-validation.json. Test subset uses first100,
not cherry-picked correct samples. Snapshot evidence under
backups/korvo-camera-display-and-peripherals/digit-demo-board.

Dataset attribution: MNIST by Yann LeCun and Corinna Cortes, derived from NIST,
CC BY-SA 3.0 per Keras dataset documentation:
https://github.com/keras-team/keras/blob/master/keras/src/datasets/mnist.py
https://creativecommons.org/licenses/by-sa/3.0/
Dataset mirror: https://storage.googleapis.com/tensorflow/tf-keras-datasets/mnist.npz
SHA256 731c5ac602752760c8e48fbffcf8c3b850d9dc2a2aedcf2cc48468fc17b673d1.
The bundled MNIST sample arrays retain this attribution and license. Training
code, C inference/UI, and assembly are Apache-2.0. Generated model weights
are distributed with the MNIST attribution and CC BY-SA 3.0 notice.
