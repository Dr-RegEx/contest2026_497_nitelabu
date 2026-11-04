/****************************************************************************
 * apps/examples/jpegtest/jpegtest_main.c
 *
 * Software JPEG/RGB888 checks using synthetic scanlines only.  This command
 * does not capture camera frames or establish camera/DMA/display acceptance.
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <setjmp.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jpeglib.h>
#include <jerror.h>

#define MAX_WIDTH     1920
#define MAX_HEIGHT    1080
#define ROW_GUARD     7
#define MAX_ERROR     32
#define MEAN_ERROR    4

struct test_error_s
{
  struct jpeg_error_mgr pub;
  jmp_buf jump;
  char message[JMSG_LENGTH_MAX];
};

/* Mutable ownership lives on the heap so that it remains defined after
 * longjmp.  A failed jpeg_create_* can also leave a partially created codec.
 */

struct test_codec_s
{
  struct jpeg_compress_struct enc;
  struct jpeg_decompress_struct dec;
  struct test_error_s error;
  unsigned char *data;
  unsigned long size;
  uint8_t *row;
  uint64_t error_sum;
  unsigned int error_max;
};

static void codec_error(j_common_ptr info)
{
  struct test_error_s *error = (struct test_error_s *)info->err;
  (*info->err->format_message)(info, error->message);
  longjmp(error->jump, 1);
}

static void codec_message(j_common_ptr info, int level)
{
  /* The memory source inserts EOI on premature EOF.  Treat warnings as
   * rejection instead of reporting a truncated image as a successful decode.
   */

  if (level < 0)
    {
      codec_error(info);
    }
}

static boolean output_full(j_compress_ptr info)
{
  /* jpeg_mem_dest publishes a grown buffer only at finish_compress.  Bound
   * this test's output to caller-owned memory so an error cannot strand an
   * unpublished replacement buffer in the bundled destination manager.
   */

  ERREXIT(info, JERR_BUFFER_SIZE);
  return FALSE;
}

static struct test_codec_s *codec_new(void)
{
  struct test_codec_s *ctx = calloc(1, sizeof(*ctx));
  if (ctx != NULL)
    {
      jpeg_std_error(&ctx->error.pub);
      ctx->error.pub.error_exit = codec_error;
      ctx->error.pub.emit_message = codec_message;
      ctx->enc.err = &ctx->error.pub;
      ctx->dec.err = &ctx->error.pub;
    }

  return ctx;
}

static void codec_free(struct test_codec_s *ctx)
{
  if (ctx != NULL)
    {
      jpeg_destroy_compress(&ctx->enc);
      jpeg_destroy_decompress(&ctx->dec);
      free(ctx->row);
      free(ctx->data);
      free(ctx);
    }
}

static uint16_t rgb888_to_rgb565(const uint8_t *p)
{
  return (uint16_t)(((uint16_t)(p[0] >> 3) << 11) |
                    ((uint16_t)(p[1] >> 2) << 5) | (p[2] >> 3));
}

static void rgb565_to_rgb888(uint16_t pixel, uint8_t *p)
{
  uint8_t r = (uint8_t)((pixel >> 11) & 0x1f);
  uint8_t g = (uint8_t)((pixel >> 5) & 0x3f);
  uint8_t b = (uint8_t)(pixel & 0x1f);
  p[0] = (uint8_t)((r << 3) | (r >> 2));
  p[1] = (uint8_t)((g << 2) | (g >> 4));
  p[2] = (uint8_t)((b << 3) | (b >> 2));
}

static int valid_size(unsigned int width, unsigned int height)
{
  return width > 0 && width <= MAX_WIDTH &&
         height > 0 && height <= MAX_HEIGHT;
}

static void test_pixel(unsigned int x, unsigned int y, unsigned int width,
                       unsigned int height, uint8_t *p)
{
  /* Unequal horizontal/vertical RGB ramps expose channel, row and stride
   * mistakes.  Each row is interleaved R,G,B bytes, with no native-endian
   * 24-bit loads.  RGB565 above is a numeric word, not a wire byte order.
   */

  unsigned int sx = width > 1 ? x * 255 / (width - 1) : 0;
  unsigned int sy = height > 1 ? y * 255 / (height - 1) : 0;
  p[0] = (uint8_t)((sx * 199 + sy * 37) / 255);
  p[1] = (uint8_t)((sx * 31 + sy * 211) / 255);
  p[2] = (uint8_t)(255 - (sx * 181 + sy * 53) / 255);
}

static int guard_ok(const uint8_t *row, size_t bytes)
{
  size_t i;
  for (i = 0; i < ROW_GUARD; i++)
    {
      if (row[bytes + i] != 0xa5)
        {
          return 0;
        }
    }

  return 1;
}

static int encode_image(unsigned int width, unsigned int height,
                        unsigned long capacity, unsigned char **data,
                        unsigned long *size)
{
  struct test_codec_s *ctx;
  size_t bytes = (size_t)width * 3;
  unsigned int x;
  JSAMPROW row;

  *data = NULL;
  *size = 0;
  if (!valid_size(width, height) || capacity == 0)
    {
      return -1;
    }

  ctx = codec_new();
  if (ctx == NULL)
    {
      return -1;
    }

  if (setjmp(ctx->error.jump))
    {
      printf("jpegtest: encode rejected: %s\n", ctx->error.message);
      goto fail;
    }

  ctx->row = malloc(bytes + ROW_GUARD);
  ctx->data = malloc(capacity);
  ctx->size = capacity;
  if (ctx->row == NULL || ctx->data == NULL)
    {
      goto fail;
    }

  jpeg_create_compress(&ctx->enc);
  jpeg_mem_dest(&ctx->enc, &ctx->data, &ctx->size);
  ctx->enc.dest->empty_output_buffer = output_full;
  ctx->enc.image_width = width;
  ctx->enc.image_height = height;
  ctx->enc.input_components = 3;
  ctx->enc.in_color_space = JCS_RGB;
  jpeg_set_defaults(&ctx->enc);
  jpeg_set_quality(&ctx->enc, 95, TRUE);

  /* 4:4:4 makes the RGB fidelity check independent of chroma subsampling. */

  for (x = 0; x < 3; x++)
    {
      ctx->enc.comp_info[x].h_samp_factor = 1;
      ctx->enc.comp_info[x].v_samp_factor = 1;
    }

  jpeg_start_compress(&ctx->enc, TRUE);
  while (ctx->enc.next_scanline < height)
    {
      memset(ctx->row, 0xa5, bytes + ROW_GUARD);
      for (x = 0; x < width; x++)
        {
          test_pixel(x, ctx->enc.next_scanline, width, height,
                     ctx->row + (size_t)x * 3);
        }

      row = ctx->row;
      if (jpeg_write_scanlines(&ctx->enc, &row, 1) != 1 ||
          !guard_ok(ctx->row, bytes))
        {
          goto fail;
        }
    }

  jpeg_finish_compress(&ctx->enc);
  if (ctx->size < 4 || ctx->data[0] != 0xff || ctx->data[1] != 0xd8 ||
      ctx->data[ctx->size - 2] != 0xff || ctx->data[ctx->size - 1] != 0xd9)
    {
      goto fail;
    }

  *data = ctx->data;
  *size = ctx->size;
  ctx->data = NULL;
  codec_free(ctx);
  return 0;

fail:
  codec_free(ctx);
  return -1;
}

static int decode_image(const unsigned char *data, unsigned long size,
                        unsigned int width, unsigned int height,
                        int report)
{
  struct test_codec_s *ctx;
  size_t bytes = (size_t)width * 3;
  unsigned int x;
  unsigned int c;
  unsigned int error;
  uint8_t expected[3];
  uint8_t expanded[3];
  JSAMPROW row;

  if (data == NULL || size == 0 || !valid_size(width, height))
    {
      return -1;
    }

  ctx = codec_new();
  if (ctx == NULL)
    {
      return -1;
    }

  if (setjmp(ctx->error.jump))
    {
      if (report)
        {
          printf("jpegtest: decode rejected: %s\n", ctx->error.message);
        }

      goto fail;
    }

  jpeg_create_decompress(&ctx->dec);
  jpeg_mem_src(&ctx->dec, data, size);
  if (jpeg_read_header(&ctx->dec, TRUE) != JPEG_HEADER_OK ||
      ctx->dec.image_width != width || ctx->dec.image_height != height)
    {
      goto fail;
    }

  ctx->dec.out_color_space = JCS_RGB;
  if (!jpeg_start_decompress(&ctx->dec) ||
      ctx->dec.output_width != width || ctx->dec.output_height != height ||
      ctx->dec.output_components != 3)
    {
      goto fail;
    }

  ctx->row = malloc(bytes + ROW_GUARD);
  if (ctx->row == NULL)
    {
      goto fail;
    }

  while (ctx->dec.output_scanline < height)
    {
      unsigned int y = ctx->dec.output_scanline;
      memset(ctx->row, 0xa5, bytes + ROW_GUARD);
      row = ctx->row;
      if (jpeg_read_scanlines(&ctx->dec, &row, 1) != 1 ||
          !guard_ok(ctx->row, bytes))
        {
          goto fail;
        }

      for (x = 0; x < width; x++)
        {
          const uint8_t *pixel = ctx->row + (size_t)x * 3;
          test_pixel(x, y, width, height, expected);
          rgb565_to_rgb888(rgb888_to_rgb565(pixel), expanded);
          for (c = 0; c < 3; c++)
            {
              error = (unsigned int)abs((int)pixel[c] - expected[c]);
              ctx->error_sum += error;
              if (error > ctx->error_max)
                {
                  ctx->error_max = error;
                }

              if (error > MAX_ERROR ||
                  abs((int)expanded[c] - pixel[c]) > (c == 1 ? 3 : 7))
                {
                  goto fail;
                }
            }
        }
    }

  if (!jpeg_finish_decompress(&ctx->dec) ||
      ctx->error_sum > (uint64_t)width * height * 3 * MEAN_ERROR)
    {
      goto fail;
    }

  if (report)
    {
      printf("jpegtest: pixels=%lu max_error=%u mean_error_milli=%lu "
             "RGB888_row_bytes=%lu guard=7:PASS\n",
             (unsigned long)width * height, ctx->error_max,
             (unsigned long)(ctx->error_sum * 1000 / ((uint64_t)width * height * 3)),
             (unsigned long)bytes);
    }

  codec_free(ctx);
  return 0;

fail:
  codec_free(ctx);
  return -1;
}

static int run_jpeg_roundtrip(unsigned int width, unsigned int height)
{
  unsigned char *data = NULL;
  unsigned char *small = NULL;
  unsigned long size = 0;
  unsigned long small_size = 0;
  unsigned long capacity = (unsigned long)width * height * 3 + 4096;
  static const unsigned char invalid[] = {0x00, 0x11, 0x22, 0x33};
  int ret = -1;

  if (encode_image(width, height, capacity, &data, &size) < 0 ||
      decode_image(data, size, width, height, 1) < 0)
    {
      goto out;
    }

  if (decode_image(invalid, sizeof(invalid), width, height, 0) == 0 ||
      decode_image(data, 1, width, height, 0) == 0 ||
      decode_image(data, 20, width, height, 0) == 0 ||
      decode_image(data, size / 2, width, height, 0) == 0 ||
      decode_image(data, size - 2, width, height, 0) == 0 ||
      decode_image(data, size, width == 1 ? 2 : width - 1, height, 0) == 0 ||
      encode_image(width, height, 16, &small, &small_size) == 0 ||
      small != NULL || small_size != 0 ||
      decode_image(data, size, width, height, 0) < 0)
    {
      goto out;
    }

  printf("jpegtest: PASS JPEG bytes=%lu decode=%ux%u RGB888=%lu bytes "
         "RGB565=%lu bytes negative=7 recovery=PASS\n",
         size, width, height, (unsigned long)width * height * 3,
         (unsigned long)width * height * 2);
  ret = 0;

out:
  free(small);
  free(data);
  return ret;
}

static int parse_number(const char *arg, unsigned int max, unsigned int *value)
{
  char *end;
  unsigned long parsed;
  if (*arg < '0' || *arg > '9')
    {
      return -1;
    }

  errno = 0;
  parsed = strtoul(arg, &end, 10);
  if (errno != 0 || *end != '\0' || parsed == 0 || parsed > max)
    {
      return -1;
    }

  *value = (unsigned int)parsed;
  return 0;
}

int main(int argc, FAR char *argv[])
{
  static const unsigned int sizes[][2] =
    {{2, 2}, {320, 240}, {640, 480}, {800, 480}, {641, 479}};
  unsigned int width = 0;
  unsigned int height = 0;
  unsigned int repeats = 1;
  unsigned int i;

  if (argc != 1 &&
      ((argc != 3 && argc != 4) ||
       parse_number(argv[1], MAX_WIDTH, &width) < 0 ||
       parse_number(argv[2], MAX_HEIGHT, &height) < 0 ||
       (argc == 4 && parse_number(argv[3], 100, &repeats) < 0)))
    {
      printf("Usage: jpegtest [width height [repeats]] "
             "(1..%u x 1..%u, repeats 1..100)\n", MAX_WIDTH, MAX_HEIGHT);
      return 1;
    }

  for (i = 0; i < (argc == 1 ? sizeof(sizes) / sizeof(sizes[0]) : repeats); i++)
    {
      if (run_jpeg_roundtrip(argc == 1 ? sizes[i][0] : width,
                             argc == 1 ? sizes[i][1] : height) < 0)
        {
          printf("jpegtest: FAIL\n");
          return 1;
        }
    }

  return 0;
}
