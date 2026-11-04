/* SPDX-License-Identifier: Apache-2.0 */

#include <nuttx/config.h>
#ifdef CONFIG_STACK_COLORATION
#include <nuttx/arch.h>
#include <nuttx/sched.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <uv.h>
#include <quickjs.h>
#include <feature_main_exports.h>
#include <feature_context.h>
#include <yoga/Yoga.h>
#include <protobuf-c/protobuf-c.h>
#include <png.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "synthetic_png.h"

static void step(const char *name)
{
  printf("FOUNDATION STEP %s\n", name);
  fflush(stdout);
}

static void timer_done(uv_timer_t *timer)
{
  *(int *)timer->data = 1;
  uv_close((uv_handle_t *)timer, NULL);
}

static int check_jump_context(void)
{
  struct
  {
    uint32_t before[4];
    jmp_buf context;
    uint32_t after[16];
  } guarded;

  memset(&guarded, 0xa5, sizeof(guarded));
  int result = setjmp(guarded.context);
  if (result == 0) longjmp(guarded.context, 37);
  if (result != 37) return -1;
  for (unsigned i = 0; i < 4; ++i)
    if (guarded.before[i] != UINT32_C(0xa5a5a5a5)) return -1;
  for (unsigned i = 0; i < 16; ++i)
    if (guarded.after[i] != UINT32_C(0xa5a5a5a5)) return -1;
  return 0;
}

static int check_js_feature_loop(void)
{
  static const char script[] =
    "globalThis.answer=0;Promise.resolve(6).then(x=>answer=x*7);";
  static const char json[] = "{\"values\":[17,25]}";
  JSRuntime *rt = NULL;
  JSContext *ctx = NULL;
  FeatureManagerHandle manager = NULL;
  uv_loop_t loop;
  uv_timer_t timer;
  int loop_ready = 0;
  int timer_ready = 0;
  int fired = 0;
  int ok = 0;
  int32_t value = 0;

  rt = JS_NewRuntime();
  if (!rt) goto done;
  JS_SetMemoryLimit(rt, 2 * 1024 * 1024);
  JS_SetMaxStackSize(rt, 32768);
  ctx = JS_NewContext(rt);
  if (!ctx) goto done;
  JSValue result = JS_Eval(ctx, script, sizeof(script) - 1,
                           "foundation.js", JS_EVAL_TYPE_GLOBAL);
  int js_ok = !JS_IsException(result);
  JS_FreeValue(ctx, result);
  if (!js_ok) goto done;
  for (int i = 0; JS_IsJobPending(rt) && i < 16; ++i)
    {
      JSContext *job_ctx = NULL;
      if (JS_ExecutePendingJob(rt, &job_ctx) < 0) goto done;
    }
  if (JS_IsJobPending(rt)) goto done;
  result = JS_Eval(ctx, "answer", 6, "foundation.js", JS_EVAL_TYPE_GLOBAL);
  js_ok = !JS_IsException(result) && JS_ToInt32(ctx, &value, result) == 0;
  JS_FreeValue(ctx, result);
  if (!js_ok || value != 42) goto done;
  step("js_promise_ok");

  if (uv_loop_init(&loop) != 0) goto done;
  loop_ready = 1;
  FeatureManagerCreateInfo info = {
    .raw_ctx = ctx, .release_cb = NULL, .manager_type = FEATURE_MANAGER_JS,
    .package_name = "org.vela.s31.foundation"
  };
  step("feature_create_begin");
  manager = FeatureCreateManager(&info);
  if (!manager) goto done;
  step("feature_created");
  FeatureSetUVLoop(manager, &loop);
  step("feature_uv_set");
  ft_context_ref fctx = FeatureManagerGetContext(manager);
  if (!fctx) goto done;
  ft_value_t obj = ft_parse_json(fctx, json, sizeof(json) - 1, "fixture.json");
  step("feature_json_parsed");
  if (ft_get_type(fctx, obj) != FT_TYPE_OBJECT)
    {
      ft_free_value(fctx, obj);
      goto done;
    }
  ft_value_t values = ft_obj_get_property(fctx, obj, "values");
  step("feature_property_read");
  int array_ok = ft_get_type(fctx, values) == FT_TYPE_ARRAY &&
                 ft_array_size(fctx, values) == 2;
  int32_t sum = 0;
  if (array_ok)
    for (unsigned i = 0; i < 2; ++i)
      {
        ft_value_t element = ft_array_at(fctx, values, i);
        if (!ft_to_int(fctx, element, &value)) array_ok = 0;
        sum += value;
        ft_free_value(fctx, element);
      }
  ft_free_value(fctx, values);
  ft_free_value(fctx, obj);
  if (!array_ok || sum != 42) goto done;
  step("feature_array_ok");
  if (uv_timer_init(&loop, &timer) != 0) goto done;
  timer_ready = 1;
  timer.data = &fired;
  if (uv_timer_start(&timer, timer_done, 5, 0) != 0) goto done;
  for (int i = 0; i < 200 && !fired; ++i)
    {
      uv_run(&loop, UV_RUN_NOWAIT);
      usleep(1000);
    }
  ok = fired;
  step("uv_timer_finished");
done:
  if (timer_ready && !uv_is_closing((uv_handle_t *)&timer))
    uv_close((uv_handle_t *)&timer, NULL);
  if (manager)
    {
      step("feature_cleanup_begin");
      FeatureUnsetUVLoop(manager);
      FeatureUninit(manager);
      FeatureFreeManager(manager);
      step("feature_cleanup_done");
    }
  if (loop_ready)
    {
      for (int i = 0; i < 10; ++i) uv_run(&loop, UV_RUN_NOWAIT);
      if (uv_loop_close(&loop) != 0) ok = 0;
    }
  if (ctx) JS_FreeContext(ctx);
  if (rt) JS_FreeRuntime(rt);
  step("js_cleanup_done");
  return ok ? 0 : -1;
}

static int check_layout(void)
{
  YGNodeRef root = YGNodeNew();
  YGNodeRef first = YGNodeNew();
  YGNodeRef second = YGNodeNew();
  if (!root || !first || !second)
    {
      if (first) YGNodeFree(first);
      if (second) YGNodeFree(second);
      if (root) YGNodeFree(root);
      return -1;
    }
  YGNodeStyleSetFlexDirection(root, YGFlexDirectionRow);
  YGNodeStyleSetWidth(root, 100);
  YGNodeStyleSetHeight(root, 50);
  YGNodeStyleSetWidth(first, 30);
  YGNodeStyleSetFlexGrow(second, 1);
  YGNodeInsertChild(root, first, 0);
  YGNodeInsertChild(root, second, 1);
  YGNodeCalculateLayout(root, 100, 50, YGDirectionLTR);
  int ok = YGNodeLayoutGetWidth(first) == 30 &&
           YGNodeLayoutGetWidth(second) == 70 &&
           YGNodeLayoutGetLeft(second) == 30 &&
           YGNodeLayoutGetHeight(second) == 50;
  YGNodeFreeRecursive(root);
  return ok ? 0 : -1;
}

/* Schema: message Sample { required uint32 number=1; required string name=2; } */
struct sample_s { ProtobufCMessage base; uint32_t number; char *name; };
static const ProtobufCMessageDescriptor sample_descriptor;
static void sample_init(ProtobufCMessage *message)
{
  struct sample_s *sample = (struct sample_s *)message;
  memset(sample, 0, sizeof(*sample));
  sample->base.descriptor = &sample_descriptor;
}
static const ProtobufCFieldDescriptor sample_fields[] = {
  {.name="number", .id=1, .label=PROTOBUF_C_LABEL_REQUIRED,
   .type=PROTOBUF_C_TYPE_UINT32, .offset=offsetof(struct sample_s, number)},
  {.name="name", .id=2, .label=PROTOBUF_C_LABEL_REQUIRED,
   .type=PROTOBUF_C_TYPE_STRING, .offset=offsetof(struct sample_s, name)}
};
static const unsigned sample_name_indices[] = {1, 0};
static const ProtobufCIntRange sample_ranges[] = {{1, 0}, {0, 2}};
static const ProtobufCMessageDescriptor sample_descriptor = {
  .magic=PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  .name="foundation.Sample", .short_name="Sample", .c_name="Sample",
  .package_name="foundation", .sizeof_message=sizeof(struct sample_s),
  .n_fields=2, .fields=sample_fields,
  .fields_sorted_by_name=sample_name_indices, .n_field_ranges=1,
  .field_ranges=sample_ranges, .message_init=sample_init
};

static int check_serialization_storage(void)
{
  static const char path[] = "/data/data/foundation-check.bin";
  static const uint8_t golden[] = {8, 0x96, 1, 0x12, 3, 's', '3', '1'};
  struct sample_s sample;
  uint8_t bytes[sizeof(golden)];
  int fd = -1;
  int created = 0;
  int ok = 0;
  const char *stage = "protobuf_pack";
  sample_init(&sample.base);
  sample.number = 150;
  sample.name = "s31";
  if (protobuf_c_message_get_packed_size(&sample.base) != sizeof(golden) ||
      protobuf_c_message_pack(&sample.base, bytes) != sizeof(golden) ||
      memcmp(bytes, golden, sizeof(golden))) goto done;
  stage = "open_write";
  fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0) goto done;
  created = 1;
  stage = "write";
  if (write(fd, bytes, sizeof(bytes)) != sizeof(bytes)) goto done;
  stage = "fsync";
  if (fsync(fd)) goto done;
  stage = "close_write";
  int closed = close(fd); fd = -1;
  if (closed) goto done;
  memset(bytes, 0, sizeof(bytes));
  stage = "open_read";
  fd = open(path, O_RDONLY);
  if (fd < 0) goto done;
  stage = "read";
  if (read(fd, bytes, sizeof(bytes)) != sizeof(bytes)) goto done;
  stage = "close_read_compare";
  closed = close(fd); fd = -1;
  if (closed || memcmp(bytes, golden, sizeof(golden))) goto done;
  stage = "protobuf_unpack";
  struct sample_s *decoded = (struct sample_s *)protobuf_c_message_unpack(
    &sample_descriptor, NULL, sizeof(bytes), bytes);
  if (!decoded) goto done;
  ok = decoded->number == 150 && decoded->name && !strcmp(decoded->name, "s31");
  protobuf_c_message_free_unpacked(&decoded->base, NULL);
done:
  if (!ok) printf("FOUNDATION storage failure stage=%s errno=%d\n", stage, errno);
  if (fd >= 0 && close(fd)) ok = 0;
  if (created && unlink(path)) ok = 0;
  return ok ? 0 : -1;
}

static int check_png(void)
{
  static const uint8_t expected[] = {255, 0, 0, 255, 0, 255, 0, 255};
  png_image image;
  uint8_t pixels[sizeof(expected)];
  memset(&image, 0, sizeof(image));
  image.version = PNG_IMAGE_VERSION;
  int ok = png_image_begin_read_from_memory(&image, synthetic_png,
                                            sizeof(synthetic_png));
  if (ok && image.width == 2 && image.height == 1)
    {
      image.format = PNG_FORMAT_RGBA;
      ok = png_image_finish_read(&image, NULL, pixels, 0, NULL) &&
           !memcmp(pixels, expected, sizeof(expected));
    }
  else ok = 0;
  png_image_free(&image);
  return ok ? 0 : -1;
}

static int check_font(void)
{
  FT_Library library = NULL;
  FT_Face face = NULL;
  int ok = 0;
  if (FT_Init_FreeType(&library)) goto done;
  if (FT_New_Face(library, "/etc/fonts/Montserrat-Regular.ttf", 0, &face))
    goto done;
  if (FT_Set_Pixel_Sizes(face, 0, 24)) goto done;
  if (FT_Load_Char(face, 'A', FT_LOAD_RENDER)) goto done;
  ok = face->glyph->bitmap.width > 0 && face->glyph->bitmap.rows > 0 &&
       face->glyph->bitmap.buffer != NULL;
done:
  if (face) FT_Done_Face(face);
  if (library) FT_Done_FreeType(library);
  return ok ? 0 : -1;
}

int main(int argc, char **argv)
{
  (void)argc; (void)argv;
  struct mallinfo before = mallinfo();
  static const struct
  {
    const char *name;
    int (*run)(void);
  } cases[] = {{"jump_context", check_jump_context},
               {"yoga", check_layout},
               {"protobuf_littlefs", check_serialization_storage},
               {"png", check_png}, {"freetype", check_font},
               {"js_feature_uv", check_js_feature_loop}};
  int failures = 0;
  for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
      step(cases[i].name);
      int result = cases[i].run();
      printf("FOUNDATION %s %s\n", cases[i].name, result ? "FAIL" : "PASS");
      fflush(stdout);
      failures += result != 0;
    }
  struct mallinfo after = mallinfo();
#ifdef CONFIG_STACK_COLORATION
  printf("FOUNDATION stack_used=%zu stack_available=%zu\n",
         up_check_tcbstack(this_task(), this_task()->adj_stack_size),
         this_task()->adj_stack_size);
#endif
  printf("FOUNDATION heap_before=%lu heap_after=%lu\n",
         (unsigned long)before.uordblks, (unsigned long)after.uordblks);
  printf("FOUNDATION RESULT %s cases=6\n", failures ? "FAIL" : "PASS");
  return failures ? 1 : 0;
}
