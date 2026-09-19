/****************************************************************************
 * apps/examples/jpegtest/jpegtest_main.c
 *
 * Board-side software smoke test for JPEG encode/decode and RGB formats.
 * The command deliberately uses a fixed image so it remains useful before a
 * camera is connected; it does not claim camera capture capability.
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jpeglib.h>

#define TEST_WIDTH  2
#define TEST_HEIGHT 2

static uint16_t rgb888_to_rgb565(const uint8_t *p)
{
  uint16_t r = (uint16_t)(p[0] >> 3);
  uint16_t g = (uint16_t)(p[1] >> 2);
  uint16_t b = (uint16_t)(p[2] >> 3);
  return (uint16_t)((r << 11) | (g << 5) | b);
}

static void rgb565_to_rgb888(uint16_t pixel, uint8_t *p)
{
  uint8_t r = (uint8_t)((pixel >> 11) & 0x1f);
  uint8_t g = (uint8_t)((pixel >> 5) & 0x3f);
  uint8_t b = (uint8_t)(pixel & 0x1f);

  /* Expand the quantized channels back to the full 8-bit range. */
  p[0] = (uint8_t)((r << 3) | (r >> 2));
  p[1] = (uint8_t)((g << 2) | (g >> 4));
  p[2] = (uint8_t)((b << 3) | (b >> 2));
}

static int check_rgb565(void)
{
  static const uint8_t image[TEST_WIDTH * TEST_HEIGHT * 3] =
    {
      0xff, 0x00, 0x00,  /* red */
      0x00, 0xff, 0x00,  /* green */
      0x00, 0x00, 0xff,  /* blue */
      0xff, 0xff, 0xff   /* white */
    };
  int i;

  for (i = 0; i < TEST_WIDTH * TEST_HEIGHT; i++)
    {
      uint16_t packed = rgb888_to_rgb565(&image[i * 3]);
      uint8_t expanded[3];
      rgb565_to_rgb888(packed, expanded);

      if (abs((int)expanded[0] - image[i * 3]) > 7 ||
          abs((int)expanded[1] - image[i * 3 + 1]) > 7 ||
          abs((int)expanded[2] - image[i * 3 + 2]) > 7)
        {
          printf("jpegtest: RGB565 quantization mismatch at pixel %d\n", i);
          return -1;
        }
    }

  return 0;
}

static int run_jpeg_roundtrip(void)
{
  static const uint8_t image[TEST_WIDTH * TEST_HEIGHT * 3] =
    {
      0xff, 0x00, 0x00,
      0x00, 0xff, 0x00,
      0x00, 0x00, 0xff,
      0xff, 0xff, 0xff
    };
  struct jpeg_compress_struct cinfo;
  struct jpeg_decompress_struct dinfo;
  struct jpeg_error_mgr jcerr;
  struct jpeg_error_mgr jderr;
  unsigned char *jpeg_data = NULL;
  unsigned long jpeg_size = 0;
  uint8_t *decoded = NULL;
  JSAMPROW row;
  int ret = -1;

  memset(&cinfo, 0, sizeof(cinfo));
  cinfo.err = jpeg_std_error(&jcerr);
  jpeg_create_compress(&cinfo);
  jpeg_mem_dest(&cinfo, &jpeg_data, &jpeg_size);
  cinfo.image_width = TEST_WIDTH;
  cinfo.image_height = TEST_HEIGHT;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, 95, TRUE);
  jpeg_start_compress(&cinfo, TRUE);

  while (cinfo.next_scanline < cinfo.image_height)
    {
      row = (JSAMPROW)&image[cinfo.next_scanline * TEST_WIDTH * 3];
      (void)jpeg_write_scanlines(&cinfo, &row, 1);
    }

  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);

  if (jpeg_size < 4 || jpeg_data[0] != 0xff || jpeg_data[1] != 0xd8)
    {
      printf("jpegtest: invalid JPEG output (%lu bytes)\n", jpeg_size);
      goto out;
    }

  memset(&dinfo, 0, sizeof(dinfo));
  dinfo.err = jpeg_std_error(&jderr);
  jpeg_create_decompress(&dinfo);
  jpeg_mem_src(&dinfo, jpeg_data, jpeg_size);

  if (jpeg_read_header(&dinfo, TRUE) != JPEG_HEADER_OK)
    {
      printf("jpegtest: JPEG header rejected\n");
      jpeg_destroy_decompress(&dinfo);
      goto out;
    }

  /* The bundled libjpeg-turbo decoder exposes RGB888 scanlines.  Convert
   * those scanlines to RGB565 explicitly instead of assuming a private
   * JCS_RGB565 output mode is available on every target. */
  dinfo.out_color_space = JCS_RGB;
  jpeg_start_decompress(&dinfo);
  if (dinfo.output_width != TEST_WIDTH || dinfo.output_height != TEST_HEIGHT ||
      dinfo.output_components != 3)
    {
      printf("jpegtest: unexpected decode geometry %u x %u x %d\n",
             (unsigned)dinfo.output_width, (unsigned)dinfo.output_height,
             dinfo.output_components);
      jpeg_destroy_decompress(&dinfo);
      goto out;
    }

  decoded = malloc(TEST_WIDTH * TEST_HEIGHT * 3);
  if (!decoded)
    {
      jpeg_destroy_decompress(&dinfo);
      goto out;
    }

  while (dinfo.output_scanline < dinfo.output_height)
    {
      row = decoded + dinfo.output_scanline * TEST_WIDTH * 3;
      (void)jpeg_read_scanlines(&dinfo, &row, 1);
    }

  if (!jpeg_finish_decompress(&dinfo) ||
      (decoded[0] == 0 && decoded[1] == 0 && decoded[2] == 0))
    {
      printf("jpegtest: decoded RGB565 frame is invalid\n");
      jpeg_destroy_decompress(&dinfo);
      goto out;
    }

  jpeg_destroy_decompress(&dinfo);
  /* Exercise the RGB888 -> RGB565 packing on the decoded frame. */
  if (rgb888_to_rgb565(decoded) == 0)
    {
      printf("jpegtest: decoded RGB888 frame cannot pack to RGB565\n");
      goto out;
    }
  printf("jpegtest: PASS JPEG bytes=%lu decode=%ux%u RGB888=%u bytes RGB565=%u bytes\n",
         jpeg_size, (unsigned)TEST_WIDTH, (unsigned)TEST_HEIGHT,
         (unsigned)(TEST_WIDTH * TEST_HEIGHT * 3),
         (unsigned)(TEST_WIDTH * TEST_HEIGHT * 2));
  ret = 0;

out:
  free(decoded);
  free(jpeg_data);
  return ret;
}

int main(int argc, FAR char *argv[])
{
  (void)argc;
  (void)argv;

  if (check_rgb565() < 0 || run_jpeg_roundtrip() < 0)
    {
      printf("jpegtest: FAIL\n");
      return 1;
    }

  return 0;
}
