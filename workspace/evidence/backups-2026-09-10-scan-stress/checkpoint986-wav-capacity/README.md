# Original WAV storage measurement (host only)

The original attachment is 17,473,937 bytes, SHA256
`20d7c680be243cac559c1d390a324c5b6740dc32471647c91a7c544fb9df5ef7`.
The measurement includes the unchanged checkout gencromfs.c and calls its
actual LZF compressor with the original 512-byte block size. No source audio
was modified and no target operation occurred.

LZF data including block headers occupies 16,061,134 bytes, excluding filesystem
metadata. Adding candidate985's 1,637,796-byte kernel gives 17,698,930 bytes,
which exceeds the entire 16 MiB Flash, before existing partition constraints.
Embedding this image in the current kernel therefore cannot solve original
4.1.131. This is a capacity finding, not an xTS PASS.

Candidate987 separately explores an uncompressed temporary LittleFS volume
with 14 MiB RAM and 3 MiB spare Flash. Geometry and runtime decoder memory
still require verification. The volume is volatile and is not durability evidence.
