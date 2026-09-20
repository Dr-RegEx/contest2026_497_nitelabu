/****************************************************************************
 * apps/testing/drivers/drivertest/devicetree/api/src/devicetree_test_api.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/devicetree.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <cmocka.h>

/****************************************************************************
 * Marco
 ****************************************************************************/

#define TEST_CHILDREN         DT_PATH(test, test_children)
#define TEST_DEADBEEF         DT_PATH(test, gpio_deadbeef)
#define TEST_ABCD1234         DT_PATH(test, gpio_abcd1234)
#define TEST_ALIAS            DT_ALIAS(test_alias)
#define TEST_NODELABEL        DT_NODELABEL(test_nodelabel)
#define TEST_INST             DT_INST(0, vnd_gpio_device)
#define TEST_ARRAYS           DT_NODELABEL(test_arrays)
#define TEST_PH               DT_NODELABEL(test_phandles)
#define TEST_INTC             DT_NODELABEL(test_intc)
#define TEST_IRQ              DT_NODELABEL(test_irq)
#define TEST_IRQ_EXT          DT_NODELABEL(test_irq_extended)
#define TEST_TEMP             DT_NODELABEL(test_temp_sensor)
#define TEST_REG              DT_NODELABEL(test_reg)
#define TEST_ENUM_0           DT_NODELABEL(test_enum_0)
#define TEST_64BIT            DT_NODELABEL(test_reg_64)
#define TEST_INTC             DT_NODELABEL(test_intc)

#define TEST_I2C              DT_NODELABEL(test_i2c)
#define TEST_I2C_DEV          DT_PATH(test, i2c_11112222, test_i2c_dev_10)
#define TEST_I2C_BUS          DT_BUS(TEST_I2C_DEV)

#define TEST_I2C_MUX          DT_NODELABEL(test_i2c_mux)
#define TEST_I2C_MUX_CTLR_1   DT_CHILD(TEST_I2C_MUX, i2c_mux_ctlr_1)
#define TEST_I2C_MUX_CTLR_2   DT_CHILD(TEST_I2C_MUX, i2c_mux_ctlr_2)
#define TEST_MUXED_I2C_DEV_1  DT_NODELABEL(test_muxed_i2c_dev_1)
#define TEST_MUXED_I2C_DEV_2  DT_NODELABEL(test_muxed_i2c_dev_2)

#define TEST_I3C              DT_NODELABEL(test_i3c)
#define TEST_I3C_DEV          DT_PATH(test, i3c_88889999, \
                              test_i3c_dev_420000abcd12345678)
#define TEST_I3C_BUS          DT_BUS(TEST_I3C_DEV)

#define TEST_GPIO_1           DT_NODELABEL(test_gpio_1)
#define TEST_GPIO_2           DT_NODELABEL(test_gpio_2)
#define TEST_GPIO_4           DT_NODELABEL(test_gpio_4)

#define TEST_SPI              DT_NODELABEL(test_spi)

#define TEST_SPI_DEV_0        DT_PATH(test, spi_33334444, test_spi_dev_0)
#define TEST_SPI_BUS_0        DT_BUS(TEST_SPI_DEV_0)

#define TEST_SPI_DEV_1        DT_PATH(test, spi_33334444, test_spi_dev_1)
#define TEST_SPI_BUS_1        DT_BUS(TEST_SPI_DEV_1)

#define TEST_RANGES_PCIE      DT_NODELABEL(test_ranges_pcie)
#define TEST_RANGES_OTHER     DT_NODELABEL(test_ranges_other)
#define TEST_RANGES_EMPTY     DT_NODELABEL(test_ranges_empty)

#define NUTTX_USER            DT_PATH(nuttx_user)

#define TA_HAS_COMPAT(compat) DT_NODE_HAS_COMPAT(TEST_ARRAYS, compat)

#define TO_STRING(x)          TO_STRING_(x)
#define TO_STRING_(x)         #x

#define assert_within(a, b, d) \
  assert(((a) >= ((b) - (d))) && ((a) <= ((b) + (d))))

#ifdef CONFIG_TESTING_DEVICETREE_DYNAMIC_API
int g_board_id;
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void test_path_props(FAR void** state)
{
  assert_int_equal(DT_NUM_REGS(TEST_DEADBEEF), 1);
  assert_int_equal(DT_REG_ADDR(TEST_DEADBEEF), 0xdeadbeef);
  assert_int_equal(DT_REG_SIZE(TEST_DEADBEEF), 0x1000);
  assert_int_equal(DT_PROP(TEST_DEADBEEF, gpio_controller), 1);
  assert_int_equal(DT_PROP(TEST_DEADBEEF, ngpios), 100);
  assert_true(!strcmp(DT_PROP(TEST_DEADBEEF, status), "okay"));
  assert_int_equal(DT_PROP_LEN(TEST_DEADBEEF, compatible), 1);
  assert_true(!strcmp(DT_PROP_BY_IDX(TEST_DEADBEEF, compatible, 0),
    "vnd,gpio-device"));
  assert_true(!strcmp(DT_PROP_LAST(TEST_DEADBEEF, compatible),
    "vnd,gpio-device"));
  assert_true(DT_NODE_HAS_PROP(TEST_DEADBEEF, status));
  assert_false(DT_NODE_HAS_PROP(TEST_DEADBEEF, foobar));

  assert_true(DT_SAME_NODE(TEST_ABCD1234, TEST_GPIO_2));
  assert_int_equal(DT_NUM_REGS(TEST_ABCD1234), 2);
  assert_int_equal(DT_PROP(TEST_ABCD1234, gpio_controller), 1);
  assert_int_equal(DT_PROP(TEST_ABCD1234, ngpios), 200);
  assert_true(!strcmp(DT_PROP(TEST_ABCD1234, status), "okay"));
  assert_int_equal(DT_PROP_LEN(TEST_ABCD1234, compatible), 1);
  assert_int_equal(DT_PROP_LEN_OR(TEST_ABCD1234, compatible, 4), 1);
  assert_int_equal(DT_PROP_LEN_OR(TEST_ABCD1234, invalid_property, 0), 0);
  assert_true(!strcmp(DT_PROP_BY_IDX(TEST_ABCD1234, compatible, 0),
    "vnd,gpio-device"));

#ifdef CONFIG_TESTING_DEVICETREE_DYNAMIC_API
  g_board_id = 1;
  assert_int_equal(DT_NUM_REGS_DYNAMIC(g_board_id, TEST_DEADBEEF), 1);
  assert_int_equal(
    DT_PROP_DYNAMIC(g_board_id, TEST_DEADBEEF, gpio_controller), 1);
  assert_int_equal(
    DT_PROP_DYNAMIC(g_board_id, TEST_DEADBEEF, ngpios), 100);
  assert_true(!strcmp(DT_PROP_DYNAMIC(g_board_id, TEST_DEADBEEF, status),
                      "okay"));
  assert_int_equal(DT_PROP_LEN_DYNAMIC(g_board_id, TEST_DEADBEEF,
                   compatible), 1);
  assert_true(
    !strcmp(DT_PROP_BY_IDX_DYNAMIC(g_board_id, TEST_DEADBEEF, compatible, 0),
            "vnd,gpio-device"));

  assert_int_equal(DT_NUM_REGS_DYNAMIC(g_board_id, TEST_ABCD1234), 2);
  assert_int_equal(
    DT_PROP_DYNAMIC(g_board_id, TEST_ABCD1234, gpio_controller), 1);
  assert_int_equal(
    DT_PROP_DYNAMIC(g_board_id, TEST_ABCD1234, ngpios), 200);
  assert_true(
    !strcmp(DT_PROP_DYNAMIC(g_board_id, TEST_ABCD1234, status), "okay"));
  assert_int_equal(
    DT_PROP_LEN_DYNAMIC(g_board_id, TEST_ABCD1234, compatible), 1);
  assert_true(
    !strcmp(DT_PROP_BY_IDX_DYNAMIC(g_board_id, TEST_ABCD1234, compatible, 0),
            "vnd,gpio-device"));
  g_board_id = 2;
  assert_int_equal(DT_NUM_REGS_DYNAMIC(g_board_id, TEST_DEADBEEF), 2);
  assert_int_equal(
    DT_PROP_DYNAMIC(g_board_id, TEST_DEADBEEF, gpio_controller), 1);
  assert_int_equal(
    DT_PROP_DYNAMIC(g_board_id, TEST_DEADBEEF, ngpios), 300);
  assert_true(
    !strcmp(DT_PROP_DYNAMIC(g_board_id, TEST_DEADBEEF, status), "disabled"));
  assert_int_equal(
    DT_PROP_LEN_DYNAMIC(g_board_id, TEST_DEADBEEF, compatible), 1);
  assert_true(
    !strcmp(DT_PROP_BY_IDX_DYNAMIC(g_board_id, TEST_DEADBEEF, compatible, 0),
            "vnd,gpio-device"));

  assert_int_equal(DT_NUM_REGS_DYNAMIC(g_board_id, TEST_ABCD1234), 2);
  assert_int_equal(
    DT_PROP_DYNAMIC(g_board_id, TEST_ABCD1234, gpio_controller), 1);
  assert_int_equal(
    DT_PROP_DYNAMIC(g_board_id, TEST_ABCD1234, ngpios), 600);
  assert_true(
    !strcmp(DT_PROP_DYNAMIC(g_board_id, TEST_ABCD1234, status), "disabled"));
  assert_int_equal(
    DT_PROP_LEN_DYNAMIC(g_board_id, TEST_ABCD1234, compatible), 1);
  assert_true(
    !strcmp(DT_PROP_BY_IDX_DYNAMIC(g_board_id, TEST_ABCD1234, compatible, 0),
            "vnd,gpio-device"));
#endif
}

static void test_alias_props(FAR void** state)
{
  assert_int_equal(DT_HAS_ALIAS(test_alias), 1);
  assert_int_equal(DT_HAS_ALIAS(test_alias_none), 0);
  assert_int_equal(DT_NUM_REGS(TEST_ALIAS), 1);
  assert_int_equal(DT_REG_ADDR(TEST_ALIAS), 0xdeadbeef);
  assert_int_equal(DT_REG_SIZE(TEST_ALIAS), 0x1000);
  assert_true(DT_SAME_NODE(TEST_ALIAS, TEST_GPIO_1));
  assert_int_equal(DT_PROP(TEST_ALIAS, gpio_controller), 1);
  assert_int_equal(DT_PROP(TEST_ALIAS, ngpios), 100);
  assert_true(!strcmp(DT_PROP(TEST_ALIAS, status), "okay"));
  assert_int_equal(DT_PROP_LEN(TEST_ALIAS, compatible), 1);
  assert_true(!strcmp(DT_PROP_BY_IDX(TEST_ALIAS, compatible, 0),
    "vnd,gpio-device"));
}

static void test_nodelabel_props(FAR void** state)
{
  assert_int_equal(DT_NUM_REGS(TEST_NODELABEL), 1);
  assert_int_equal(DT_REG_ADDR(TEST_NODELABEL), 0xdeadbeef);
  assert_int_equal(DT_REG_SIZE(TEST_NODELABEL), 0x1000);
  assert_int_equal(DT_PROP(TEST_NODELABEL, gpio_controller), 1);
  assert_int_equal(DT_PROP(TEST_NODELABEL, ngpios), 100);
  assert_true(!strcmp(DT_PROP(TEST_NODELABEL, status), "okay"));
  assert_int_equal(DT_PROP_LEN(TEST_NODELABEL, compatible), 1);
  assert_true(!strcmp(DT_PROP_BY_IDX(TEST_NODELABEL, compatible, 0),
    "vnd,gpio-device"));
  assert_int_equal(DT_PROP_LEN(TEST_ENUM_0, val), 1);
}

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_gpio_device
static void test_inst_props(FAR void** state)
{
/* Careful:
 *
 * We can only test properties that are shared across all
 * instances of this compatible here. This includes instances
 * with status "disabled".
 */

  assert_int_equal(DT_PROP(TEST_INST, gpio_controller), 1);
  assert_true(!strcmp(DT_PROP(TEST_INST, status), "okay")
              || !strcmp(DT_PROP(TEST_INST, status), "disabled"));
  assert_int_equal(DT_PROP_LEN(TEST_INST, compatible), 1);
  assert_true(!strcmp(DT_PROP_BY_IDX(TEST_INST, compatible, 0),
    "vnd,gpio-device"));

  assert_int_equal(DT_INST_NODE_HAS_PROP(0, gpio_controller), 1);
  assert_int_equal(DT_INST_PROP(0, gpio_controller), 1);
  assert_int_equal(DT_INST_NODE_HAS_PROP(0, xxxx), 0);
  assert_true(!strcmp(DT_INST_PROP(0, status), "okay") ||
              !strcmp(DT_PROP(TEST_INST, status), "disabled"));
  assert_int_equal(DT_INST_PROP_LEN(0, compatible), 1);
  assert_true(!strcmp(DT_INST_PROP_BY_IDX(0, compatible, 0),
    "vnd,gpio-device"));
}

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_device_with_props

static void test_any_inst_prop(FAR void** state)
{
  assert_int_equal(DT_ANY_INST_HAS_PROP_STATUS_OKAY(foo), 1);
  assert_int_equal(DT_ANY_INST_HAS_PROP_STATUS_OKAY(bar), 1);
  assert_int_equal(DT_ANY_INST_HAS_PROP_STATUS_OKAY(baz), 0);
  assert_int_equal(DT_ANY_INST_HAS_PROP_STATUS_OKAY(does_not_exist), 0);

  assert_int_equal(COND_CODE_1(
    DT_ANY_INST_HAS_PROP_STATUS_OKAY(foo), (5), (6)), 5);
  assert_int_equal(COND_CODE_0(
    DT_ANY_INST_HAS_PROP_STATUS_OKAY(foo), (5), (6)), 6);
  assert_int_equal(COND_CODE_1(
    DT_ANY_INST_HAS_PROP_STATUS_OKAY(baz), (5), (6)), 6);
  assert_int_equal(COND_CODE_0(
    DT_ANY_INST_HAS_PROP_STATUS_OKAY(baz), (5), (6)), 5);
  assert_true(IS_ENABLED(DT_ANY_INST_HAS_PROP_STATUS_OKAY(foo)));
  assert_true(!IS_ENABLED(DT_ANY_INST_HAS_PROP_STATUS_OKAY(baz)));
  assert_int_equal(IF_ENABLED(
    DT_ANY_INST_HAS_PROP_STATUS_OKAY(foo), (1)) + 1, 2);
  assert_int_equal(IF_ENABLED(
    DT_ANY_INST_HAS_PROP_STATUS_OKAY(baz), (1)) + 1, 1);
}

#undef DT_DRV_COMPAT

static void test_any_compat_inst_prop(FAR void** state)
{
  assert_int_equal(DT_ANY_COMPAT_HAS_PROP_STATUS_OKAY(
    vnd_device_with_props, foo), 1);
  assert_int_equal(DT_ANY_COMPAT_HAS_PROP_STATUS_OKAY(
    vnd_device_with_props, bar), 1);
  assert_int_equal(DT_ANY_COMPAT_HAS_PROP_STATUS_OKAY(
    vnd_device_with_props, baz), 0);
  assert_int_equal(DT_ANY_COMPAT_HAS_PROP_STATUS_OKAY(
    vnd_device_with_props, does_not_exist), 0);
}

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_device_with_props

static void test_any_inst_bool(FAR void** state)
{
  assert_int_equal(DT_ANY_INST_HAS_BOOL_STATUS_OKAY(bool_foo), 1);
  assert_int_equal(DT_ANY_INST_HAS_BOOL_STATUS_OKAY(bool_bar), 1);
  assert_int_equal(DT_ANY_INST_HAS_BOOL_STATUS_OKAY(bool_baz), 0);
  assert_int_equal(DT_ANY_INST_HAS_BOOL_STATUS_OKAY(does_not_exist), 0);

  assert_int_equal(COND_CODE_1(
    DT_ANY_INST_HAS_BOOL_STATUS_OKAY(bool_foo), (5), (6)), 5);
  assert_int_equal(COND_CODE_0(
    DT_ANY_INST_HAS_BOOL_STATUS_OKAY(bool_foo), (5), (6)), 6);
  assert_int_equal(COND_CODE_1(
    DT_ANY_INST_HAS_BOOL_STATUS_OKAY(bool_baz), (5), (6)), 6);
  assert_int_equal(COND_CODE_0(
    DT_ANY_INST_HAS_BOOL_STATUS_OKAY(bool_baz), (5), (6)), 5);
  assert_true(IS_ENABLED(DT_ANY_INST_HAS_BOOL_STATUS_OKAY(bool_foo)));
  assert_true(!IS_ENABLED(DT_ANY_INST_HAS_BOOL_STATUS_OKAY(bool_baz)));
  assert_int_equal(IF_ENABLED(
    DT_ANY_INST_HAS_BOOL_STATUS_OKAY(bool_foo), (1)) + 1, 2);
  assert_int_equal(IF_ENABLED(
    DT_ANY_INST_HAS_BOOL_STATUS_OKAY(bool_baz), (1)) + 1, 1);
}

static void test_default_prop_access(FAR void** state)
{
#undef X
#define X "do.not.expand.this.argument"

  assert_int_equal(DT_PROP_OR(TEST_REG, misc_prop, X), 1234);
  assert_int_equal(DT_PROP_OR(TEST_REG, not_a_property, -1), -1);

  assert_int_equal(DT_PHA_BY_IDX_OR(TEST_TEMP, dmas, 1, channel, X), 3);
  assert_int_equal(DT_PHA_BY_IDX_OR(TEST_TEMP, dmas, 1, not_a_cell, -1), -1);

  assert_int_equal(DT_PHA_OR(TEST_TEMP, dmas, channel, X), 1);
  assert_int_equal(DT_PHA_OR(TEST_TEMP, dmas, not_a_cell, -1), -1);

  assert_int_equal(DT_PHA_BY_NAME_OR(
    TEST_TEMP, dmas, tx, channel, X), 1);
  assert_int_equal(DT_PHA_BY_NAME_OR(
    TEST_TEMP, dmas, tx, not_a_cell, -1), -1);

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_reg_holder
  assert_int_equal(DT_INST_PROP_OR(0, misc_prop, X), 1234);
  assert_int_equal(DT_INST_PROP_OR(0, not_a_property, -1), -1);

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_array_holder
  assert_int_equal(DT_INST_PROP_LEN_OR(0, a, X), 3);
  assert_int_equal(DT_INST_PROP_LEN_OR(0, not_a_property, -1), -1);

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_adc_temp_sensor
  assert_int_equal(DT_INST_PHA_BY_IDX_OR(0, dmas, 1, channel, X), 3);
  assert_int_equal(DT_INST_PHA_BY_IDX_OR(0, dmas, 1, not_a_cell, -1), -1);

  assert_int_equal(DT_INST_PHA_OR(0, dmas, channel, X), 1);
  assert_int_equal(DT_INST_PHA_OR(0, dmas, not_a_cell, -1), -1);

  assert_int_equal(DT_INST_PHA_BY_NAME_OR(0, dmas, tx, channel, X), 1);
  assert_int_equal(DT_INST_PHA_BY_NAME_OR(0, dmas, tx, not_a_cell, -1), -1);

#undef X
}

static void test_has_path(FAR void** state)
{
  assert_int_equal(DT_NODE_HAS_STATUS(DT_PATH(test, gpio_0), okay), 0);
  assert_int_equal(
    DT_NODE_HAS_STATUS(DT_PATH(test, gpio_deadbeef), okay), 1);
  assert_int_equal(
    DT_NODE_HAS_STATUS(DT_PATH(test, gpio_abcd1234), okay), 1);
}

static void test_has_alias(FAR void** state)
{
  assert_int_equal(DT_NODE_HAS_STATUS(DT_ALIAS(test_alias), okay), 1);
  assert_int_equal(DT_NODE_HAS_STATUS(DT_ALIAS(test_undef), okay), 0);
}

static void test_node_hashes(FAR void** state)
{
  assert_string_equal(TO_STRING(DT_NODE_HASH(DT_ROOT)),
                      "il7asoJjJEMhngUeSt4tHVu8Zxx4EFG_FDeJfL3_oPE");
  assert_string_equal(TO_STRING(DT_NODE_HASH(TEST_DEADBEEF)),
                      "kPPqtBX5DX_QDQMO0_cOls2ebJMevAWHhAPY1JCKTyU");
  assert_string_equal(TO_STRING(DT_NODE_HASH(TEST_ABCD1234)),
                      "Bk4fvF6o3Mgslz_xiIZaJcuwo6_IeelozwOaxtUsSos");
}

static void test_inst_checks(FAR void** state)
{
  assert_int_equal(DT_NODE_EXISTS(DT_INST(0, vnd_gpio_device)), 1);
  assert_int_equal(DT_NODE_EXISTS(DT_INST(1, vnd_gpio_device)), 1);
  assert_int_equal(DT_NODE_EXISTS(DT_INST(2, vnd_gpio_device)), 1);

  assert_int_equal(DT_NUM_INST_STATUS_OKAY(vnd_gpio_device), 2);
  assert_int_equal(DT_NUM_INST_STATUS_OKAY(xxxx), 0);
}

static void test_has_nodelabel(FAR void** state)
{
  assert_int_equal(DT_NODE_HAS_STATUS(DT_NODELABEL(disabled_gpio), okay), 0);
  assert_int_equal(
    DT_NODE_HAS_STATUS(DT_NODELABEL(test_nodelabel), okay), 1);
  assert_int_equal(
    DT_NODE_HAS_STATUS(DT_NODELABEL(test_nodelabel_allcaps), okay), 1);
}

static void test_has_compat(FAR void** state)
{
  unsigned int compats;

  assert_true(DT_HAS_COMPAT_STATUS_OKAY(vnd_gpio_device));
  assert_true(DT_HAS_COMPAT_STATUS_OKAY(vnd_gpio_device));
  assert_false(DT_HAS_COMPAT_STATUS_OKAY(vnd_disabled_compat));
  assert_false(DT_HAS_COMPAT_STATUS_OKAY(vnd_reserved_compat));

  assert_int_equal(TA_HAS_COMPAT(vnd_array_holder), 1);
  assert_int_equal(TA_HAS_COMPAT(vnd_undefined_compat), 1);
  assert_int_equal(TA_HAS_COMPAT(vnd_not_a_test_array_compat), 0);
  compats = ((TA_HAS_COMPAT(vnd_array_holder) << 0) |
             (TA_HAS_COMPAT(vnd_undefined_compat) << 1) |
             (TA_HAS_COMPAT(vnd_not_a_test_array_compat) << 2));
  assert_int_equal(compats, 0x3);
}

static void test_has_status(FAR void** state)
{
  assert_int_equal(DT_NODE_HAS_STATUS(DT_NODELABEL(test_gpio_1), okay), 1);
  assert_int_equal(DT_NODE_HAS_STATUS(DT_NODELABEL(test_gpio_1), disabled),
                   0);
  assert_int_equal(DT_NODE_HAS_STATUS(DT_NODELABEL(test_gpio_1), reserved),
                   0);

  assert_int_equal(DT_NODE_HAS_STATUS(DT_NODELABEL(test_no_status), okay),
                   1);
  assert_int_equal(DT_NODE_HAS_STATUS(DT_NODELABEL(test_no_status),
                                      disabled), 0);
  assert_int_equal(DT_NODE_HAS_STATUS(DT_NODELABEL(test_no_status),
                                      reserved), 0);

  assert_int_equal(DT_NODE_HAS_STATUS(DT_NODELABEL(disabled_gpio),
                                      disabled), 1);
  assert_int_equal(DT_NODE_HAS_STATUS(DT_NODELABEL(disabled_gpio),
                                      okay), 0);
  assert_int_equal(DT_NODE_HAS_STATUS(DT_NODELABEL(disabled_gpio),
                                      reserved), 0);

  assert_int_equal(DT_NODE_HAS_STATUS(DT_NODELABEL(reserved_gpio),
                                      reserved), 1);
  assert_int_equal(DT_NODE_HAS_STATUS(DT_NODELABEL(reserved_gpio),
                                      disabled), 0);
  assert_int_equal(DT_NODE_HAS_STATUS(DT_NODELABEL(reserved_gpio),
                                      okay), 0);
}

static void test_has_status_okay(FAR void** state)
{
  assert_int_equal(DT_NODE_HAS_STATUS_OKAY(
    DT_NODELABEL(test_gpio_1)), 1);
  assert_int_equal(DT_NODE_HAS_STATUS_OKAY(
    DT_NODELABEL(test_no_status)), 1);
  assert_int_equal(DT_NODE_HAS_STATUS_OKAY(
    DT_NODELABEL(disabled_gpio)), 0);
  assert_int_equal(DT_NODE_HAS_STATUS_OKAY(
    DT_NODELABEL(reserved_gpio)), 0);
}

static void test_bus(FAR void** state)
{
  assert_true(DT_SAME_NODE(TEST_I3C_BUS, TEST_I3C));
  assert_true(DT_SAME_NODE(TEST_I2C_BUS, TEST_I2C));
  assert_true(DT_SAME_NODE(TEST_SPI_BUS_0, TEST_SPI));
  assert_true(DT_SAME_NODE(TEST_SPI_BUS_1, TEST_SPI));

  /* Test a nested I2C bus using vnd,i2c-mux. */

  assert_true(DT_SAME_NODE(TEST_I2C_MUX_CTLR_1,
              DT_BUS(TEST_MUXED_I2C_DEV_1)));
  assert_true(DT_SAME_NODE(TEST_I2C_MUX_CTLR_2,
              DT_BUS(TEST_MUXED_I2C_DEV_2)));

  assert_int_equal(DT_ON_BUS(TEST_SPI_DEV_0, spi), 1);
  assert_int_equal(DT_ON_BUS(TEST_SPI_DEV_0, i2c), 0);
  assert_int_equal(DT_ON_BUS(TEST_SPI_DEV_0, i3c), 0);

  assert_int_equal(DT_ON_BUS(TEST_I2C_DEV, i2c), 1);
  assert_int_equal(DT_ON_BUS(TEST_I2C_DEV, i3c), 0);
  assert_int_equal(DT_ON_BUS(TEST_I2C_DEV, spi), 0);

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_spi_device
  assert_int_equal(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT), 2);
  assert_int_equal(DT_INST_ON_BUS(0, spi), 1);
  assert_int_equal(DT_INST_ON_BUS(0, i2c), 0);
  assert_int_equal(DT_INST_ON_BUS(0, i3c), 0);

  assert_int_equal(DT_ANY_INST_ON_BUS_STATUS_OKAY(spi), 1);
  assert_int_equal(DT_ANY_INST_ON_BUS_STATUS_OKAY(i2c), 0);
  assert_int_equal(DT_ANY_INST_ON_BUS_STATUS_OKAY(i3c), 0);

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_i2c_device
  assert_int_equal(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT), 2);
  assert_int_equal(DT_INST_ON_BUS(0, i2c), 1);
  assert_int_equal(DT_INST_ON_BUS(0, i3c), 0);
  assert_int_equal(DT_INST_ON_BUS(0, spi), 0);

  assert_int_equal(DT_ANY_INST_ON_BUS_STATUS_OKAY(i2c), 1);
  assert_int_equal(DT_ANY_INST_ON_BUS_STATUS_OKAY(i3c), 0);
  assert_int_equal(DT_ANY_INST_ON_BUS_STATUS_OKAY(spi), 0);

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_i3c_device
  assert_int_equal(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT), 1);
  assert_int_equal(DT_INST_ON_BUS(0, i2c), 1);
  assert_int_equal(DT_INST_ON_BUS(0, i3c), 1);
  assert_int_equal(DT_INST_ON_BUS(0, spi), 0);

  assert_int_equal(DT_ANY_INST_ON_BUS_STATUS_OKAY(i2c), 1);
  assert_int_equal(DT_ANY_INST_ON_BUS_STATUS_OKAY(i3c), 1);
  assert_int_equal(DT_ANY_INST_ON_BUS_STATUS_OKAY(spi), 0);

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_i3c_i2c_device
  assert_int_equal(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT), 1);
  assert_int_equal(DT_INST_ON_BUS(0, i2c), 1);
  assert_int_equal(DT_INST_ON_BUS(0, i3c), 1);
  assert_int_equal(DT_INST_ON_BUS(0, spi), 0);

  assert_int_equal(DT_ANY_INST_ON_BUS_STATUS_OKAY(i2c), 1);
  assert_int_equal(DT_ANY_INST_ON_BUS_STATUS_OKAY(i3c), 1);
  assert_int_equal(DT_ANY_INST_ON_BUS_STATUS_OKAY(spi), 0);

#undef DT_DRV_COMPAT

  assert_int_equal(DT_HAS_COMPAT_ON_BUS_STATUS_OKAY(vnd_spi_device, spi), 1);
  assert_int_equal(DT_HAS_COMPAT_ON_BUS_STATUS_OKAY(vnd_spi_device, i2c), 0);

  assert_int_equal(DT_HAS_COMPAT_ON_BUS_STATUS_OKAY(vnd_i2c_device, i2c), 1);
  assert_int_equal(DT_HAS_COMPAT_ON_BUS_STATUS_OKAY(vnd_i2c_device, spi), 0);

  assert_int_equal(DT_HAS_COMPAT_ON_BUS_STATUS_OKAY(vnd_gpio_expander, i2c),
                   1);
  assert_int_equal(DT_HAS_COMPAT_ON_BUS_STATUS_OKAY(vnd_gpio_expander, spi),
                   1);
}

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_reg_holder
static void test_reg(FAR void** state)
{
  /* DT_REG_HAS_IDX */

  assert_true(DT_REG_HAS_IDX(TEST_ABCD1234, 0));
  assert_true(DT_REG_HAS_IDX(TEST_ABCD1234, 1));
  assert_false(DT_REG_HAS_IDX(TEST_ABCD1234, 2));

  /* DT_REG_HAS_NAME */

  assert_true(DT_REG_HAS_NAME(TEST_ABCD1234, one));
  assert_true(DT_REG_HAS_NAME(TEST_ABCD1234, two));
  assert_false(DT_REG_HAS_NAME(TEST_ABCD1234, three));

  /* DT_REG_ADDR_BY_IDX */

  assert_int_equal(DT_REG_ADDR_BY_IDX(TEST_ABCD1234, 0), 0xabcd1234);
  assert_int_equal(DT_REG_ADDR_BY_IDX(TEST_ABCD1234, 1), 0x98765432);

#ifdef CONFIG_TESTING_DEVICETREE_DYNAMIC_API
  g_board_id = 1;
  assert_int_equal(
    DT_REG_ADDR_BY_IDX_DYNAMIC(g_board_id, TEST_ABCD1234, 0), 0xabcd1234);
  assert_int_equal(
    DT_REG_ADDR_BY_IDX_DYNAMIC(g_board_id, TEST_ABCD1234, 1), 0x98765432);
  g_board_id = 2;
  assert_int_equal(
    DT_REG_ADDR_BY_IDX_DYNAMIC(g_board_id, TEST_ABCD1234, 0), 0xaabb5678);
  assert_int_equal(
    DT_REG_ADDR_BY_IDX_DYNAMIC(g_board_id, TEST_ABCD1234, 1), 0x11223344);
#endif

  /* DT_REG_SIZE_BY_IDX */

  assert_int_equal(DT_REG_SIZE_BY_IDX(TEST_ABCD1234, 0), 0x500);
  assert_int_equal(DT_REG_SIZE_BY_IDX(TEST_ABCD1234, 1), 0xff);

#ifdef CONFIG_TESTING_DEVICETREE_DYNAMIC_API
  g_board_id = 1;
  assert_int_equal(
    DT_REG_SIZE_BY_IDX_DYNAMIC(g_board_id, TEST_ABCD1234, 0), 0x500);
  assert_int_equal(
    DT_REG_SIZE_BY_IDX_DYNAMIC(g_board_id, TEST_ABCD1234, 1), 0xff);
  g_board_id = 2;
  assert_int_equal(
    DT_REG_SIZE_BY_IDX_DYNAMIC(g_board_id, TEST_ABCD1234, 0), 0x600);
  assert_int_equal(
    DT_REG_SIZE_BY_IDX_DYNAMIC(g_board_id, TEST_ABCD1234, 1), 0xaa);
#endif

  /* DT_REG_ADDR */

  assert_int_equal(DT_REG_ADDR(TEST_ABCD1234), 0xabcd1234);

  /* DT_REG_ADDR_U64 */

  assert_int_equal(DT_REG_ADDR_U64(TEST_ABCD1234), 0xabcd1234);

  /* DT_REG_SIZE */

  assert_int_equal(DT_REG_SIZE(TEST_ABCD1234), 0x500);

  /* DT_REG_ADDR_BY_NAME */

  assert_int_equal(DT_REG_ADDR_BY_NAME(TEST_ABCD1234, one),
                   0xabcd1234);
  assert_int_equal(DT_REG_ADDR_BY_NAME(TEST_ABCD1234, two),
                   0x98765432);

#ifdef CONFIG_TESTING_DEVICETREE_DYNAMIC_API
  g_board_id = 1;
  assert_int_equal(
    DT_REG_ADDR_BY_NAME_DYNAMIC(g_board_id, TEST_ABCD1234, one), 0xabcd1234);
  assert_int_equal(
    DT_REG_ADDR_BY_NAME_DYNAMIC(g_board_id, TEST_ABCD1234, two), 0x98765432);
  g_board_id = 2;
  assert_int_equal(
    DT_REG_ADDR_BY_NAME_DYNAMIC(g_board_id, TEST_ABCD1234, one), 0xaabb5678);
  assert_int_equal(
    DT_REG_ADDR_BY_NAME_DYNAMIC(g_board_id, TEST_ABCD1234, two), 0x11223344);
#endif

  /* DT_REG_ADDR_BY_NAME_OR */

  assert_int_equal(DT_REG_ADDR_BY_NAME_OR(TEST_ABCD1234, one, 0x10),
                   0xabcd1234);
  assert_int_equal(DT_REG_ADDR_BY_NAME_OR(TEST_ABCD1234, two, 0x11),
                   0x98765432);
  assert_int_equal(DT_REG_ADDR_BY_NAME_OR(TEST_ABCD1234, three, 0x12),
                   0x12);

  /* DT_REG_ADDR_BY_NAME_U64 */

  assert_int_equal(DT_REG_ADDR_BY_NAME_U64(TEST_ABCD1234, one), 0xabcd1234);
  assert_int_equal(DT_REG_ADDR_BY_NAME_U64(TEST_ABCD1234, two), 0x98765432);

  /* DT_REG_SIZE_BY_NAME */

  assert_int_equal(DT_REG_SIZE_BY_NAME(TEST_ABCD1234, one), 0x500);
  assert_int_equal(DT_REG_SIZE_BY_NAME(TEST_ABCD1234, two), 0xff);

#ifdef CONFIG_TESTING_DEVICETREE_DYNAMIC_API
  g_board_id = 1;
  assert_int_equal(
    DT_REG_SIZE_BY_NAME_DYNAMIC(g_board_id, TEST_ABCD1234, one), 0x500);
  assert_int_equal(
    DT_REG_SIZE_BY_NAME_DYNAMIC(g_board_id, TEST_ABCD1234, two), 0xff);
  g_board_id = 2;
  assert_int_equal(
    DT_REG_SIZE_BY_NAME_DYNAMIC(g_board_id, TEST_ABCD1234, one), 0x600);
  assert_int_equal(
    DT_REG_SIZE_BY_NAME_DYNAMIC(g_board_id, TEST_ABCD1234, two), 0xaa);
#endif

  /* DT_REG_SIZE_BY_NAME_OR */

  assert_int_equal(DT_REG_SIZE_BY_NAME_OR(TEST_ABCD1234, one, 0x10), 0x500);
  assert_int_equal(DT_REG_SIZE_BY_NAME_OR(TEST_ABCD1234, two, 0x11), 0xff);
  assert_int_equal(DT_REG_SIZE_BY_NAME_OR(TEST_ABCD1234, three, 0x12), 0x12);

  /* DT_INST */

  assert_int_equal(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT), 1);

  /* DT_INST_REG_HAS_IDX */

  assert_true(DT_INST_REG_HAS_IDX(0, 0));
  assert_true(DT_INST_REG_HAS_IDX(0, 1));
  assert_false(DT_INST_REG_HAS_IDX(0, 2));

  /* DT_INST_REG_HAS_NAME */

  assert_true(DT_INST_REG_HAS_NAME(0, first));
  assert_true(DT_INST_REG_HAS_NAME(0, second));
  assert_false(DT_INST_REG_HAS_NAME(0, third));

  /* DT_INST_REG_ADDR_BY_IDX */

  assert_int_equal(DT_INST_REG_ADDR_BY_IDX(0, 0), 0x9999aaaa);
  assert_int_equal(DT_INST_REG_ADDR_BY_IDX(0, 1), 0xbbbbcccc);

  /* DT_INST_REG_SIZE_BY_IDX */

  assert_int_equal(DT_INST_REG_SIZE_BY_IDX(0, 0), 0x1000);
  assert_int_equal(DT_INST_REG_SIZE_BY_IDX(0, 1), 0x3f);

  /* DT_INST_REG_ADDR */

  assert_int_equal(DT_INST_REG_ADDR(0), 0x9999aaaa);

  /* DT_INST_REG_ADDR_U64 */

  assert_int_equal(DT_INST_REG_ADDR_U64(0), 0x9999aaaa);

  /* DT_INST_REG_SIZE */

  assert_int_equal(DT_INST_REG_SIZE(0), 0x1000);

  /* DT_INST_REG_ADDR_BY_NAME */

  assert_int_equal(DT_INST_REG_ADDR_BY_NAME(0, first), 0x9999aaaa);
  assert_int_equal(DT_INST_REG_ADDR_BY_NAME(0, second), 0xbbbbcccc);

  /* DT_INST_REG_ADDR_BY_NAME_OR */

  assert_int_equal(DT_INST_REG_ADDR_BY_NAME_OR(0, first, 0x10), 0x9999aaaa);
  assert_int_equal(DT_INST_REG_ADDR_BY_NAME_OR(0, second, 0x11), 0xbbbbcccc);
  assert_int_equal(DT_INST_REG_SIZE_BY_NAME_OR(0, third, 0x12), 0x12);

  /* DT_INST_REG_ADDR_BY_NAME_U64 */

  assert_int_equal(DT_INST_REG_ADDR_BY_NAME_U64(0, first), 0x9999aaaa);
  assert_int_equal(DT_INST_REG_ADDR_BY_NAME_U64(0, second), 0xbbbbcccc);

  /* DT_INST_REG_SIZE_BY_NAME */

  assert_int_equal(DT_INST_REG_SIZE_BY_NAME(0, first), 0x1000);
  assert_int_equal(DT_INST_REG_SIZE_BY_NAME(0, second), 0x3f);

  /* DT_REG_SIZE_BY_NAME_OR */

  assert_int_equal(DT_INST_REG_SIZE_BY_NAME_OR(0, first, 0x10), 0x1000);
  assert_int_equal(DT_INST_REG_SIZE_BY_NAME_OR(0, second, 0x11), 0x3f);
  assert_int_equal(DT_INST_REG_SIZE_BY_NAME_OR(0, third, 0x12), 0x12);
}

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_reg_holder_64
static void test_reg_64(FAR void** state)
{
  /* DT_REG_ADDR_U64 */

  assert_int_equal(DT_REG_ADDR_U64(TEST_64BIT), 0xffffffff11223344);

  /* DT_REG_ADDR_BY_NAME_U64 */

  assert_int_equal(DT_REG_ADDR_BY_NAME_U64(TEST_64BIT, test_name),
                   0xffffffff11223344);

  /* DT_INST_REG_ADDR_U64 */

  assert_int_equal(DT_INST_REG_ADDR_U64(0), 0xffffffff11223344);

  /* DT_INST_REG_ADDR_BY_NAME_U64 */

  assert_int_equal(DT_INST_REG_ADDR_BY_NAME_U64(0, test_name),
                   0xffffffff11223344);
}

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_interrupt_holder
static void test_irq(void** state)
{
  /* DT_NUM_IRQS */

  assert_int_equal(DT_NUM_IRQS(TEST_DEADBEEF), 1);
  assert_int_equal(DT_NUM_IRQS(TEST_I2C_BUS), 2);
  assert_int_equal(DT_NUM_IRQS(TEST_SPI), 3);

  /* DT_IRQ_HAS_IDX */

  assert_true(DT_IRQ_HAS_IDX(TEST_SPI_BUS_0, 0));
  assert_true(DT_IRQ_HAS_IDX(TEST_SPI_BUS_0, 1));
  assert_true(DT_IRQ_HAS_IDX(TEST_SPI_BUS_0, 2));
  assert_false(DT_IRQ_HAS_IDX(TEST_SPI_BUS_0, 3));

  assert_true(DT_IRQ_HAS_IDX(TEST_DEADBEEF, 0));
  assert_false(DT_IRQ_HAS_IDX(TEST_DEADBEEF, 1));

  assert_true(DT_IRQ_HAS_IDX(TEST_I2C_BUS, 0));
  assert_true(DT_IRQ_HAS_IDX(TEST_I2C_BUS, 1));
  assert_false(DT_IRQ_HAS_IDX(TEST_I2C_BUS, 2));

  /* DT_IRQ_BY_IDX */

  assert_int_equal(DT_IRQ_BY_IDX(TEST_SPI_BUS_0, 0, irq), 8);
  assert_int_equal(DT_IRQ_BY_IDX(TEST_SPI_BUS_0, 1, irq), 9);
  assert_int_equal(DT_IRQ_BY_IDX(TEST_SPI_BUS_0, 2, irq), 10);
  assert_int_equal(DT_IRQ_BY_IDX(TEST_SPI_BUS_0, 0, priority), 3);
  assert_int_equal(DT_IRQ_BY_IDX(TEST_SPI_BUS_0, 1, priority), 0);
  assert_int_equal(DT_IRQ_BY_IDX(TEST_SPI_BUS_0, 2, priority), 1);

  /* DT_IRQ_BY_NAME */

  assert_int_equal(DT_IRQ_BY_NAME(TEST_I2C_BUS, status, irq), 6);
  assert_int_equal(DT_IRQ_BY_NAME(TEST_I2C_BUS, error, irq), 7);
  assert_int_equal(DT_IRQ_BY_NAME(TEST_I2C_BUS, status, priority), 2);
  assert_int_equal(DT_IRQ_BY_NAME(TEST_I2C_BUS, error, priority), 1);

  /* DT_IRQ_HAS_CELL_AT_IDX */

  assert_true(DT_IRQ_HAS_CELL_AT_IDX(TEST_IRQ, 0, irq));
  assert_true(DT_IRQ_HAS_CELL_AT_IDX(TEST_IRQ, 0, priority));
  assert_false(DT_IRQ_HAS_CELL_AT_IDX(TEST_IRQ, 0, foo));
  assert_true(DT_IRQ_HAS_CELL_AT_IDX(TEST_IRQ, 2, irq));
  assert_true(DT_IRQ_HAS_CELL_AT_IDX(TEST_IRQ, 2, priority));
  assert_false(DT_IRQ_HAS_CELL_AT_IDX(TEST_IRQ, 2, foo));

  /* DT_IRQ_HAS_CELL */

  assert_true(DT_IRQ_HAS_CELL(TEST_IRQ, irq));
  assert_true(DT_IRQ_HAS_CELL(TEST_IRQ, priority));
  assert_false(DT_IRQ_HAS_CELL(TEST_IRQ, foo));

  /* DT_IRQ_HAS_NAME */

  assert_true(DT_IRQ_HAS_NAME(TEST_IRQ, err));
  assert_true(DT_IRQ_HAS_NAME(TEST_IRQ, stat));
  assert_true(DT_IRQ_HAS_NAME(TEST_IRQ, done));
  assert_false(DT_IRQ_HAS_NAME(TEST_IRQ, alpha));

  /* DT_IRQ */

  assert_int_equal(DT_IRQ(TEST_I2C_BUS, irq), 6);
  assert_int_equal(DT_IRQ(TEST_I2C_BUS, priority), 2);

  /* DT_IRQN */

#ifndef CONFIG_MULTI_LEVEL_INTERRUPTS
  assert_int_equal(DT_IRQN(TEST_I2C_BUS), 6);
  assert_int_equal(DT_IRQN(DT_INST(0, DT_DRV_COMPAT)), 30);
#else
  assert_int_equal(DT_IRQN(TEST_I2C_BUS),
    ((6 + 1) << CONFIG_1ST_LEVEL_INTERRUPT_BITS) | 11);
  assert_int_equal(DT_IRQN(DT_INST(0, DT_DRV_COMPAT)),
    ((30 + 1) << CONFIG_1ST_LEVEL_INTERRUPT_BITS) | 11);
#endif

  /* DT_IRQN_BY_IDX */

#ifndef CONFIG_MULTI_LEVEL_INTERRUPTS
  assert_int_equal(DT_IRQN_BY_IDX(DT_INST(0, DT_DRV_COMPAT), 0), 30);
  assert_int_equal(DT_IRQN_BY_IDX(DT_INST(0, DT_DRV_COMPAT), 1), 40);
  assert_int_equal(DT_IRQN_BY_IDX(DT_INST(0, DT_DRV_COMPAT), 2), 60);

#ifdef CONFIG_TESTING_DEVICETREE_DYNAMIC_API
  g_board_id = 1;
  assert_int_equal(
    DT_IRQN_BY_IDX_DYNAMIC(g_board_id, DT_INST(0, DT_DRV_COMPAT), 0), 30);
  assert_int_equal(
    DT_IRQN_BY_IDX_DYNAMIC(g_board_id, DT_INST(0, DT_DRV_COMPAT), 1), 40);
  assert_int_equal(
    DT_IRQN_BY_IDX_DYNAMIC(g_board_id, DT_INST(0, DT_DRV_COMPAT), 2), 60);
  g_board_id = 2;
  assert_int_equal(
    DT_IRQN_BY_IDX_DYNAMIC(g_board_id, DT_INST(0, DT_DRV_COMPAT), 0), 40);
  assert_int_equal(
    DT_IRQN_BY_IDX_DYNAMIC(g_board_id, DT_INST(0, DT_DRV_COMPAT), 1), 50);
  assert_int_equal(
    DT_IRQN_BY_IDX_DYNAMIC(g_board_id, DT_INST(0, DT_DRV_COMPAT), 2), 70);
#endif

#else
  assert_int_equal(DT_IRQN_BY_IDX(DT_INST(0, DT_DRV_COMPAT), 0),
    ((30 + 1) << CONFIG_1ST_LEVEL_INTERRUPT_BITS) | 11);
  assert_int_equal(DT_IRQN_BY_IDX(DT_INST(0, DT_DRV_COMPAT), 1),
    ((40 + 1) << CONFIG_1ST_LEVEL_INTERRUPT_BITS) | 11);
  assert_int_equal(DT_IRQN_BY_IDX(DT_INST(0, DT_DRV_COMPAT), 2),
    ((60 + 1) << CONFIG_1ST_LEVEL_INTERRUPT_BITS) | 11);
#endif

  /* DT_INST */

  assert_int_equal(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT), 1);

  /* DT_INST_NUM_IRQS */

  assert_int_equal(DT_INST_NUM_IRQS(0), 3);

  /* DT_INST_IRQ_HAS_IDX */

  assert_int_equal(DT_INST_IRQ_HAS_IDX(0, 0), 1);
  assert_int_equal(DT_INST_IRQ_HAS_IDX(0, 1), 1);
  assert_int_equal(DT_INST_IRQ_HAS_IDX(0, 2), 1);
  assert_int_equal(DT_INST_IRQ_HAS_IDX(0, 3), 0);

  /* DT_INST_IRQ_BY_IDX */

  assert_int_equal(DT_INST_IRQ_BY_IDX(0, 0, irq), 30);
  assert_int_equal(DT_INST_IRQ_BY_IDX(0, 1, irq), 40);
  assert_int_equal(DT_INST_IRQ_BY_IDX(0, 2, irq), 60);
  assert_int_equal(DT_INST_IRQ_BY_IDX(0, 0, priority), 3);
  assert_int_equal(DT_INST_IRQ_BY_IDX(0, 1, priority), 5);
  assert_int_equal(DT_INST_IRQ_BY_IDX(0, 2, priority), 7);

  /* DT_INST_IRQ_BY_NAME */

  assert_int_equal(DT_INST_IRQ_BY_NAME(0, err, irq), 30);
  assert_int_equal(DT_INST_IRQ_BY_NAME(0, stat, irq), 40);
  assert_int_equal(DT_INST_IRQ_BY_NAME(0, done, irq), 60);
  assert_int_equal(DT_INST_IRQ_BY_NAME(0, err, priority), 3);
  assert_int_equal(DT_INST_IRQ_BY_NAME(0, stat, priority), 5);
  assert_int_equal(DT_INST_IRQ_BY_NAME(0, done, priority), 7);

  /* DT_INST_IRQ */

  assert_int_equal(DT_INST_IRQ(0, irq), 30);
  assert_int_equal(DT_INST_IRQ(0, priority), 3);

  /* DT_INST_IRQN */

#ifndef CONFIG_MULTI_LEVEL_INTERRUPTS
  assert_int_equal(DT_INST_IRQN(0), 30);
#else
  assert_int_equal(DT_INST_IRQN(0),
    ((30 + 1) << CONFIG_1ST_LEVEL_INTERRUPT_BITS) | 11);
#endif

  /* DT_INST_IRQN_BY_IDX */

#ifndef CONFIG_MULTI_LEVEL_INTERRUPTS
  assert_int_equal(DT_INST_IRQN_BY_IDX(0, 0), 30);
  assert_int_equal(DT_INST_IRQN_BY_IDX(0, 1), 40);
  assert_int_equal(DT_INST_IRQN_BY_IDX(0, 2), 60);
#else
  assert_int_equal(DT_INST_IRQN_BY_IDX(0, 0),
    ((30 + 1) << CONFIG_1ST_LEVEL_INTERRUPT_BITS) | 11);
  assert_int_equal(DT_INST_IRQN_BY_IDX(0, 1),
    ((40 + 1) << CONFIG_1ST_LEVEL_INTERRUPT_BITS) | 11);
  assert_int_equal(DT_INST_IRQN_BY_IDX(0, 2),
    ((60 + 1) << CONFIG_1ST_LEVEL_INTERRUPT_BITS) | 11);
#endif

  /* DT_INST_IRQ_HAS_CELL_AT_IDX */

  assert_true(DT_INST_IRQ_HAS_CELL_AT_IDX(0, 0, irq));
  assert_true(DT_INST_IRQ_HAS_CELL_AT_IDX(0, 0, priority));
  assert_false(DT_INST_IRQ_HAS_CELL_AT_IDX(0, 0, foo));
  assert_true(DT_INST_IRQ_HAS_CELL_AT_IDX(0, 2, irq));
  assert_true(DT_INST_IRQ_HAS_CELL_AT_IDX(0, 2, priority));
  assert_false(DT_INST_IRQ_HAS_CELL_AT_IDX(0, 2, foo));

  /* DT_INST_IRQ_HAS_CELL */

  assert_true(DT_INST_IRQ_HAS_CELL(0, irq));
  assert_true(DT_INST_IRQ_HAS_CELL(0, priority));
  assert_false(DT_INST_IRQ_HAS_CELL(0, foo));

  /* DT_INST_IRQ_HAS_NAME */

  assert_true(DT_INST_IRQ_HAS_NAME(0, err));
  assert_true(DT_INST_IRQ_HAS_NAME(0, stat));
  assert_true(DT_INST_IRQ_HAS_NAME(0, done));
  assert_false(DT_INST_IRQ_HAS_NAME(0, alpha));

#ifdef CONFIG_MULTI_LEVEL_INTERRUPTS
  assert_int_equal(DT_IRQN_BY_IDX(TEST_IRQ_EXT, 0),
    ((70 + 1) << CONFIG_1ST_LEVEL_INTERRUPT_BITS) | 11);
  assert_int_equal(DT_IRQN_BY_IDX(TEST_IRQ_EXT, 2),
    ((42 + 1) << CONFIG_1ST_LEVEL_INTERRUPT_BITS) | 12);
#else
  assert_int_equal(DT_IRQN_BY_IDX(TEST_IRQ_EXT, 0), 70);
  assert_int_equal(DT_IRQN_BY_IDX(TEST_IRQ_EXT, 2), 42);

#ifdef CONFIG_TESTING_DEVICETREE_DYNAMIC_API
  g_board_id = 1;
  assert_int_equal(DT_IRQN_BY_IDX_DYNAMIC(g_board_id, TEST_IRQ_EXT, 0), 70);
  assert_int_equal(DT_IRQN_BY_IDX_DYNAMIC(g_board_id, TEST_IRQ_EXT, 2), 42);
  g_board_id = 2;
  assert_int_equal(DT_IRQN_BY_IDX_DYNAMIC(g_board_id, TEST_IRQ_EXT, 0), 65);
  assert_int_equal(DT_IRQN_BY_IDX_DYNAMIC(g_board_id, TEST_IRQ_EXT, 2), 47);
#endif

#endif
}

static void test_irq_level(void** state)
{
  /* DT_IRQ_LEVEL */

  assert_int_equal(DT_IRQ_LEVEL(TEST_TEMP), 0);
  assert_int_equal(DT_IRQ_LEVEL(TEST_INTC), 1);
  assert_int_equal(DT_IRQ_LEVEL(TEST_SPI), 2);

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_adc_temp_sensor
  assert_int_equal(DT_INST_IRQ_LEVEL(0), 0);

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_intc
  assert_int_equal(DT_INST_IRQ_LEVEL(1), 1);

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_spi
  assert_int_equal(DT_INST_IRQ_LEVEL(0), 2);
}

#define CLOCK_FREQUENCY_AND_COMMA(node_id, prop, idx) \
  DT_PROP_BY_PHANDLE_IDX(node_id, prop, idx, clock_frequency),

/* Helper macro that UTIL_LISTIFY can use and produces an
 * element with comma
 */

#define DT_GPIO_ELEM(idx, node_id, prop)        \
  {                         \
  DT_PHA_BY_IDX(node_id, prop, idx, pin),   \
    DT_PHA_BY_IDX(node_id, prop, idx, flags), \
  }
#define DT_GPIO_LISTIFY(node_id, prop)              \
  {                               \
    LISTIFY(DT_PROP_LEN(node_id, prop), DT_GPIO_ELEM, (, ), \
    node_id, prop)              \
  }

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_phandle_holder

static void test_phandles(void** state)
{
  bool gpio_controller = DT_PROP_BY_PHANDLE(TEST_PH, ph, gpio_controller);
  size_t phs_freqs[] =
    {
      DT_FOREACH_PROP_ELEM(TEST_PH, phs, CLOCK_FREQUENCY_AND_COMMA)
    };

  /* phandle */

  assert_true(DT_NODE_HAS_PROP(TEST_PH, ph));
  assert_true(DT_SAME_NODE(DT_PROP(TEST_PH, ph),
              DT_NODELABEL(test_gpio_1)));
  assert_int_equal(DT_PROP_LEN(TEST_PH, ph), 1);
  assert_true(DT_SAME_NODE(DT_PROP_BY_IDX(TEST_PH, ph, 0),
              DT_NODELABEL(test_gpio_1)));

  /* DT_PROP_BY_PHANDLE */

  assert_int_equal(gpio_controller, true);

  /* phandles */

  assert_true(DT_NODE_HAS_PROP(TEST_PH, phs));
  assert_int_equal(ARRAY_SIZE(phs_freqs), 2);
  assert_int_equal(DT_PROP_LEN(TEST_PH, phs), 2);
  assert_true(DT_SAME_NODE(DT_PROP_BY_IDX(TEST_PH, phs, 1), TEST_SPI));

  /* DT_FOREACH_PROP_ELEM on a phandles type property */

  assert_int_equal(phs_freqs[0], 100000);
  assert_int_equal(phs_freqs[1], 2000000);

  /* DT_PROP_BY_PHANDLE_IDX on a phandles type property */

  assert_int_equal(
    DT_PROP_BY_PHANDLE_IDX(TEST_PH, phs, 0, clock_frequency), 100000);
  assert_int_equal(
    DT_PROP_BY_PHANDLE_IDX(TEST_PH, phs, 1, clock_frequency), 2000000);

  /* DT_PROP_BY_PHANDLE_IDX on a phandle-array type property */

  assert_int_equal(DT_PROP_BY_PHANDLE_IDX(TEST_PH, gpios, 0, ngpios), 100);
  assert_int_equal(DT_PROP_BY_PHANDLE_IDX(TEST_PH, gpios, 1, ngpios), 200);
  assert_true(!strcmp(DT_PROP_BY_PHANDLE_IDX(TEST_PH, gpios, 0, status),
                      "okay"));
  assert_true(!strcmp(DT_PROP_BY_PHANDLE_IDX(TEST_PH, gpios, 1, status),
                      "okay"));

  /* DT_PROP_BY_PHANDLE_IDX_OR */

  assert_true(!strcmp(DT_PROP_BY_PHANDLE_IDX_OR(
    TEST_PH, phs_or, 0, val, "zero"), "one"));
  assert_true(!strcmp(DT_PROP_BY_PHANDLE_IDX_OR(
    TEST_PH, phs_or, 1, val, "zero"), "zero"));

  /* phandle-array */

  assert_true(DT_NODE_HAS_PROP(TEST_PH, gpios));
  assert_int_equal(DT_PROP_LEN(TEST_PH, gpios), 2);

  /* DT_PROP_HAS_IDX */

  assert_true(DT_PROP_HAS_IDX(TEST_PH, gpios, 0));
  assert_true(DT_PROP_HAS_IDX(TEST_PH, gpios, 1));
  assert_false(DT_PROP_HAS_IDX(TEST_PH, gpios, 2));

  /* DT_PROP_HAS_NAME */

  assert_false(DT_PROP_HAS_NAME(TEST_PH, foos, A));
  assert_true(DT_PROP_HAS_NAME(TEST_PH, foos, a));
  assert_false(DT_PROP_HAS_NAME(TEST_PH, foos, b - c));
  assert_true(DT_PROP_HAS_NAME(TEST_PH, foos, b_c));
  assert_false(DT_PROP_HAS_NAME(TEST_PH, bazs, jane));

  /* DT_PHA_HAS_CELL_AT_IDX */

  assert_true(DT_PHA_HAS_CELL_AT_IDX(TEST_PH, gpios, 1, pin));
  assert_true(DT_PHA_HAS_CELL_AT_IDX(TEST_PH, gpios, 1, flags));

  /* pha-gpios index 1 has nothing, not even a phandle */

  assert_false(DT_PROP_HAS_IDX(TEST_PH, pha_gpios, 1));
  assert_false(DT_PHA_HAS_CELL_AT_IDX(TEST_PH, pha_gpios, 1, pin));
  assert_false(DT_PHA_HAS_CELL_AT_IDX(TEST_PH, pha_gpios, 1, flags));

  /* index 2 only has a pin cell, no flags */

  assert_true(DT_PHA_HAS_CELL_AT_IDX(TEST_PH, pha_gpios, 2, pin));
  assert_false(DT_PHA_HAS_CELL_AT_IDX(TEST_PH, pha_gpios, 2, flags));

  /* index 3 has both pin and flags cells */

  assert_true(DT_PHA_HAS_CELL_AT_IDX(TEST_PH, pha_gpios, 3, pin));
  assert_true(DT_PHA_HAS_CELL_AT_IDX(TEST_PH, pha_gpios, 3, flags));

  /* even though index 1 has nothing, the length is still 4 */

  assert_int_equal(DT_PROP_LEN(TEST_PH, pha_gpios), 4);

  /* DT_PHA_HAS_CELL */

  assert_true(DT_PHA_HAS_CELL(TEST_PH, gpios, flags));
  assert_false(DT_PHA_HAS_CELL(TEST_PH, gpios, bar));

  /* DT_PHANDLE_BY_IDX */

  assert_true(DT_SAME_NODE(DT_PHANDLE_BY_IDX(TEST_PH, gpios, 0),
              TEST_GPIO_1));
  assert_true(DT_SAME_NODE(DT_PHANDLE_BY_IDX(TEST_PH, gpios, 1),
              TEST_GPIO_2));

  /* DT_PHANDLE */

  assert_true(DT_SAME_NODE(DT_PHANDLE(TEST_PH, gpios), TEST_GPIO_1));

  /* DT_PHA */

  assert_int_equal(DT_PHA(TEST_PH, gpios, pin), 10);
  assert_int_equal(DT_PHA(TEST_PH, gpios, flags), 20);

  /* DT_PHA_BY_IDX */

  assert_int_equal(DT_PHA_BY_IDX(TEST_PH, gpios, 0, pin), 10);
  assert_int_equal(DT_PHA_BY_IDX(TEST_PH, gpios, 0, flags), 20);
  assert_int_equal(DT_PHA_BY_IDX(TEST_PH, gpios, 1, pin), 30);
  assert_int_equal(DT_PHA_BY_IDX(TEST_PH, gpios, 1, flags), 40);

  /* DT_PHA_BY_NAME */

  assert_int_equal(DT_PHA_BY_NAME(TEST_PH, foos, a, foocell), 100);
  assert_int_equal(DT_PHA_BY_NAME(TEST_PH, foos, b_c, foocell), 110);

  /* DT_PHANDLE_BY_NAME */

  assert_true(DT_SAME_NODE(DT_PHANDLE_BY_NAME(TEST_PH, foos, a),
              TEST_GPIO_1));
  assert_true(DT_SAME_NODE(DT_PHANDLE_BY_NAME(TEST_PH, foos, b_c),
              TEST_GPIO_2));

  /* DT_INST */

  assert_int_equal(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT), 1);

  /* DT_INST_PROP_BY_PHANDLE */

  assert_int_equal(DT_INST_PROP_BY_PHANDLE(0, ph, ngpios), 100);

  /* DT_INST_PROP_BY_PHANDLE_IDX */

  assert_int_equal(DT_INST_PROP_BY_PHANDLE_IDX(0, phs, 0, clock_frequency),
                   100000);
  assert_int_equal(DT_INST_PROP_BY_PHANDLE_IDX(0, phs, 1, clock_frequency),
                   2000000);
  assert_int_equal(
    DT_INST_PROP_BY_PHANDLE_IDX(0, gpios, 0, gpio_controller), 1);
  assert_int_equal(
    DT_INST_PROP_BY_PHANDLE_IDX(0, gpios, 1, gpio_controller), 1);
  assert_int_equal(DT_INST_PROP_BY_PHANDLE_IDX(0, gpios, 0, ngpios),
                   100);
  assert_int_equal(DT_INST_PROP_BY_PHANDLE_IDX(0, gpios, 1, ngpios),
                   200);

  /* DT_INST_PROP_HAS_IDX */

  assert_true(DT_INST_PROP_HAS_IDX(0, gpios, 0));
  assert_true(DT_INST_PROP_HAS_IDX(0, gpios, 1));
  assert_false(DT_INST_PROP_HAS_IDX(0, gpios, 2));

  /* DT_INST_PROP_HAS_NAME */

  assert_false(DT_INST_PROP_HAS_NAME(0, foos, A));
  assert_true(DT_INST_PROP_HAS_NAME(0, foos, a));
  assert_false(DT_INST_PROP_HAS_NAME(0, foos, b - c));
  assert_true(DT_INST_PROP_HAS_NAME(0, foos, b_c));
  assert_false(DT_INST_PROP_HAS_NAME(0, bazs, jane));

  /* DT_INST_PHA_HAS_CELL_AT_IDX */

  assert_true(DT_INST_PHA_HAS_CELL_AT_IDX(0, gpios, 1, pin));
  assert_true(DT_INST_PHA_HAS_CELL_AT_IDX(0, gpios, 1, flags));

  /* index 1 has nothing, not even a phandle */

  assert_false(DT_INST_PROP_HAS_IDX(0, pha_gpios, 1));
  assert_false(DT_INST_PHA_HAS_CELL_AT_IDX(0, pha_gpios, 1, pin));
  assert_false(DT_INST_PHA_HAS_CELL_AT_IDX(0, pha_gpios, 1, flags));

  /* index 2 only has pin, no flags */

  assert_true(DT_INST_PHA_HAS_CELL_AT_IDX(0, pha_gpios, 2, pin));
  assert_false(DT_INST_PHA_HAS_CELL_AT_IDX(0, pha_gpios, 2, flags));

  /* index 3 has both pin and flags */

  assert_true(DT_INST_PHA_HAS_CELL_AT_IDX(0, pha_gpios, 3, pin));
  assert_true(DT_INST_PHA_HAS_CELL_AT_IDX(0, pha_gpios, 3, flags));

  /* even though index 1 has nothing, the length is still 4 */

  assert_int_equal(DT_INST_PROP_LEN(0, pha_gpios), 4);

  /* DT_INST_PHA_HAS_CELL */

  assert_true(DT_INST_PHA_HAS_CELL(0, gpios, flags));
  assert_false(DT_INST_PHA_HAS_CELL(0, gpios, bar));

  /* DT_INST_PHANDLE_BY_IDX */

  assert_true(DT_SAME_NODE(DT_INST_PHANDLE_BY_IDX(0, gpios, 0),
              TEST_GPIO_1));
  assert_true(DT_SAME_NODE(DT_INST_PHANDLE_BY_IDX(0, gpios, 1),
              TEST_GPIO_2));

  /* DT_INST_PHANDLE */

  assert_true(DT_SAME_NODE(DT_INST_PHANDLE(0, gpios), TEST_GPIO_1));

  /* DT_INST_PHA */

  assert_int_equal(DT_INST_PHA(0, gpios, pin), 10);
  assert_int_equal(DT_INST_PHA(0, gpios, flags), 20);

  /* DT_INST_PHA_BY_IDX */

  assert_int_equal(DT_INST_PHA_BY_IDX(0, gpios, 0, pin), 10);
  assert_int_equal(DT_INST_PHA_BY_IDX(0, gpios, 0, flags), 20);
  assert_int_equal(DT_INST_PHA_BY_IDX(0, gpios, 1, pin), 30);
  assert_int_equal(DT_INST_PHA_BY_IDX(0, gpios, 1, flags), 40);

  /* DT_INST_PHA_BY_NAME */

  assert_int_equal(DT_INST_PHA_BY_NAME(0, foos, a, foocell), 100);
  assert_int_equal(DT_INST_PHA_BY_NAME(0, foos, b_c, foocell), 110);

  /* DT_INST_PHANDLE_BY_NAME */

  assert_true(DT_SAME_NODE(DT_INST_PHANDLE_BY_NAME(0, foos, a),
              TEST_GPIO_1));
  assert_true(DT_SAME_NODE(DT_INST_PHANDLE_BY_NAME(0, foos, b_c),
              TEST_GPIO_2));
}

static void test_macro_names(void** state)
{
  /* white box */

  assert_true(!strcmp(TO_STRING(DT_PATH(test, gpio_deadbeef)),
    "DT_N_S_test_S_gpio_deadbeef"));
  assert_true(!strcmp(TO_STRING(DT_ALIAS(test_alias)),
    "DT_N_S_test_S_gpio_deadbeef"));
  assert_true(!strcmp(TO_STRING(DT_NODELABEL(test_nodelabel)),
    "DT_N_S_test_S_gpio_deadbeef"));
  assert_true(!strcmp(TO_STRING(DT_NODELABEL(test_nodelabel_allcaps)),
    "DT_N_S_test_S_gpio_deadbeef"));

#define CHILD_NODE_ID DT_CHILD(DT_PATH(test, i2c_11112222), test_i2c_dev_10)
#define FULL_PATH_ID DT_PATH(test, i2c_11112222, test_i2c_dev_10)

  assert_true(!strcmp(TO_STRING(CHILD_NODE_ID),
    TO_STRING(FULL_PATH_ID)));

#undef CHILD_NODE_ID
#undef FULL_PATH_ID
}

static int a[] = DT_PROP(TEST_ARRAYS, a);
static unsigned char b[] = DT_PROP(TEST_ARRAYS, b);
static char *c[] = DT_PROP(TEST_ARRAYS, c);

static void test_arrays(void** state)
{
  int ok;

  assert_int_equal(ARRAY_SIZE(a), 3);
  assert_int_equal(ARRAY_SIZE(b), 4);
  assert_int_equal(ARRAY_SIZE(c), 2);

  assert_int_equal(a[0], 1000);
  assert_int_equal(a[1], 2000);
  assert_int_equal(a[2], 3000);

  assert_true(DT_PROP_HAS_IDX(TEST_ARRAYS, a, 0));
  assert_true(DT_PROP_HAS_IDX(TEST_ARRAYS, a, 1));
  assert_true(DT_PROP_HAS_IDX(TEST_ARRAYS, a, 2));
  assert_false(DT_PROP_HAS_IDX(TEST_ARRAYS, a, 3));

  /* Verify that DT_PROP_HAS_IDX can be used with COND_CODE_1()
   * and COND_CODE_0(), i.e. its expansion is a literal 1 or 0,
   * not an equivalent expression that evaluates to 1 or 0.
   */

  ok = 0;
  COND_CODE_1(DT_PROP_HAS_IDX(TEST_ARRAYS, a, 0), (ok = 1;
                                                  ), ());
  assert_int_equal(ok, 1);
  ok = 0;
  COND_CODE_0(DT_PROP_HAS_IDX(TEST_ARRAYS, a, 3), (ok = 1;
                                                  ), ());
  assert_int_equal(ok, 1);

  assert_int_equal(DT_PROP_BY_IDX(TEST_ARRAYS, a, 0), a[0]);
  assert_int_equal(DT_PROP_BY_IDX(TEST_ARRAYS, a, 1), a[1]);
  assert_int_equal(DT_PROP_BY_IDX(TEST_ARRAYS, a, 2), a[2]);
  assert_int_equal(DT_PROP_LAST(TEST_ARRAYS, a), a[2]);

  assert_int_equal(DT_PROP_LEN(TEST_ARRAYS, a), 3);

  assert_int_equal(b[0], 0xaa);
  assert_int_equal(b[1], 0xbb);
  assert_int_equal(b[2], 0xcc);
  assert_int_equal(b[3], 0xdd);

  assert_true(DT_PROP_HAS_IDX(TEST_ARRAYS, b, 0));
  assert_true(DT_PROP_HAS_IDX(TEST_ARRAYS, b, 1));
  assert_true(DT_PROP_HAS_IDX(TEST_ARRAYS, b, 2));
  assert_true(DT_PROP_HAS_IDX(TEST_ARRAYS, b, 3));
  assert_false(DT_PROP_HAS_IDX(TEST_ARRAYS, b, 4));

  assert_int_equal(DT_PROP_BY_IDX(TEST_ARRAYS, b, 0), b[0]);
  assert_int_equal(DT_PROP_BY_IDX(TEST_ARRAYS, b, 1), b[1]);
  assert_int_equal(DT_PROP_BY_IDX(TEST_ARRAYS, b, 2), b[2]);
  assert_int_equal(DT_PROP_BY_IDX(TEST_ARRAYS, b, 3), b[3]);

  assert_int_equal(DT_PROP_LEN(TEST_ARRAYS, b), 4);

  assert_true(!strcmp(c[0], "bar"));
  assert_true(!strcmp(c[1], "baz"));

  assert_true(DT_PROP_HAS_IDX(TEST_ARRAYS, c, 0));
  assert_true(DT_PROP_HAS_IDX(TEST_ARRAYS, c, 1));
  assert_false(DT_PROP_HAS_IDX(TEST_ARRAYS, c, 2));

  assert_true(!strcmp(DT_PROP_BY_IDX(TEST_ARRAYS, c, 0), c[0]));
  assert_true(!strcmp(DT_PROP_BY_IDX(TEST_ARRAYS, c, 1), c[1]));

  assert_int_equal(DT_PROP_LEN(TEST_ARRAYS, c), 2);
}

static void test_foreach(void** state)
{
#define IS_ALIASES(node_id) +DT_SAME_NODE(DT_PATH(aliases), node_id)
#define IS_DISABLED_GPIO(node_id) +DT_SAME_NODE(DT_NODELABEL(disabled_gpio), \
  node_id)
  assert_int_equal(1, DT_FOREACH_NODE(IS_ALIASES));
  assert_int_equal(1, DT_FOREACH_NODE(IS_DISABLED_GPIO));
  assert_int_equal(1, DT_FOREACH_STATUS_OKAY_NODE(IS_ALIASES));
  assert_int_equal(0, DT_FOREACH_STATUS_OKAY_NODE(IS_DISABLED_GPIO));

#define IS_ALIASES_VARGS(node_id, mul) +((mul)*DT_SAME_NODE(DT_PATH(aliases), node_id))
#define IS_DISABLED_GPIO_VARGS(node_id, mul) +((mul)*DT_SAME_NODE(DT_NODELABEL(disabled_gpio), node_id))
  assert_int_equal(2, DT_FOREACH_NODE_VARGS(IS_ALIASES_VARGS, 2));
  assert_int_equal(2, DT_FOREACH_NODE_VARGS(IS_DISABLED_GPIO_VARGS, 2));
  assert_int_equal(2,
    DT_FOREACH_STATUS_OKAY_NODE_VARGS(IS_ALIASES_VARGS, 2));
  assert_int_equal(0,
    DT_FOREACH_STATUS_OKAY_NODE_VARGS(IS_DISABLED_GPIO_VARGS, 2));

#undef IS_ALIASES
#undef IS_DISABLED_GPIO
#undef IS_ALIASES_VARGS
#undef IS_DISABLED_GPIO_VARGS
}

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_gpio_device
static void test_foreach_status_okay(void** state)
{
  /* For-each-node type macro tests.
   *
   * See test_foreach_prop_elem*() for tests of
   * for-each-property type macros.
   */

  unsigned int val;
  const char *str;

  /* This should expand to something like:
   *
   * "/test/enum-0" "/test/enum-1"
   *
   * but there is no guarantee about the order of nodes in the
   * expansion, so we test both.
   */

  str = DT_FOREACH_STATUS_OKAY(vnd_enum_holder, DT_NODE_PATH);
  assert_true(!strcmp(str, "/test/enum-0/test/enum-1") ||
              !strcmp(str, "/test/enum-1/test/enum-0"));

#undef MY_FN
#define MY_FN(node_id, operator) DT_ENUM_IDX(node_id, val) operator
  /* This should expand to something like:
   *
   * 0 + 2 + 3
   *
   * and order of expansion doesn't matter, since we're adding
   * the values all up.
   */

  val = DT_FOREACH_STATUS_OKAY_VARGS(vnd_enum_holder, MY_FN, +) 3;
  assert_int_equal(val, 5);

#undef MY_FN
#define MY_FN(inst, compat, operator) DT_ENUM_IDX(DT_INST(inst, compat), val) operator
  /* This should expand to something like:
   *
   * 0 + 2 + 3
   *
   * and order of expansion doesn't matter, since we're adding
   * the values all up.
   */

  val = DT_COMPAT_FOREACH_STATUS_OKAY_VARGS(vnd_enum_holder, MY_FN, +) 3;
  assert_int_equal(val, 5);

  /* Make sure DT_INST_FOREACH_STATUS_OKAY can be called from functions
   * using macros with side effects in the current scope.
   */

  val = 0;
#define INC(inst_ignored) \
  do {          \
    val++;      \
  } while (0);
  DT_INST_FOREACH_STATUS_OKAY(INC)
  assert_int_equal(val, 2);
#undef INC

  val = 0;
#define INC_ARG(arg) \
  do {       \
    val++;     \
    val += arg;  \
  } while (0)
#define INC(inst_ignored, arg) INC_ARG(arg);
  DT_INST_FOREACH_STATUS_OKAY_VARGS(INC, 1)
  assert_int_equal(val, 4);
#undef INC_ARG
#undef INC

  /* Make sure DT_INST_FOREACH_STATUS_OKAY works with 0 instances, and does
   * not expand its argument at all.
   */

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT xxxx
#define BUILD_BUG_ON_EXPANSION (there is a bug in devicetree.h)
  DT_INST_FOREACH_STATUS_OKAY(BUILD_BUG_ON_EXPANSION)
#undef BUILD_BUG_ON_EXPANSION

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT xxxx
#define BUILD_BUG_ON_EXPANSION(arg) (there is a bug in devicetree.h)
  DT_INST_FOREACH_STATUS_OKAY_VARGS(BUILD_BUG_ON_EXPANSION, 1)
#undef BUILD_BUG_ON_EXPANSION
}

static void test_foreach_nodelabel(void** state)
{
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_adc_temp_sensor
#define ENTRY(nodelabel) enum_##nodelabel,
#define VAR_PLUS(nodelabel, to_add) int nodelabel##_added = enum_##nodelabel + to_add;

  /* DT_FOREACH_NODELABEL */

  enum {
    DT_FOREACH_NODELABEL(DT_NODELABEL(test_nodelabel), ENTRY)
  };

  assert_int_equal(enum_test_nodelabel, 0);
  assert_int_equal(enum_TEST_NODELABEL_ALLCAPS, 1);
  assert_int_equal(enum_test_gpio_1, 2);

  /* DT_FOREACH_NODELABEL_VARGS */

  DT_FOREACH_NODELABEL_VARGS(DT_NODELABEL(test_nodelabel), VAR_PLUS, 1);
  assert_int_equal(test_nodelabel_added, 1);
  assert_int_equal(TEST_NODELABEL_ALLCAPS_added, 2);
  assert_int_equal(test_gpio_1_added, 3);

  /* DT_NODELABEL_STRING_ARRAY is tested here since it's closely related */

  const char *nodelabels[] =
    DT_NODELABEL_STRING_ARRAY(DT_NODELABEL(test_nodelabel));

  assert_int_equal(ARRAY_SIZE(nodelabels), 3);
  assert_true(!strcmp(nodelabels[0], "test_nodelabel"));
  assert_true(!strcmp(nodelabels[1], "TEST_NODELABEL_ALLCAPS"));
  assert_true(!strcmp(nodelabels[2], "test_gpio_1"));

  /* DT_NUM_NODELABELS */

  assert_int_equal(DT_NUM_NODELABELS(DT_NODELABEL(test_nodelabel)), 3);
  assert_int_equal(DT_NUM_NODELABELS(DT_PATH(chosen)), 0);
  assert_int_equal(DT_NUM_NODELABELS(DT_ROOT), 0);

  /* DT_INST_FOREACH_NODELABEL */

  enum {
    DT_INST_FOREACH_NODELABEL(0, ENTRY)
  };

  assert_int_equal(enum_test_temp_sensor, 0);

  /* DT_INST_FOREACH_NODELABEL_VARGS */

  DT_INST_FOREACH_NODELABEL_VARGS(0, VAR_PLUS, 1);
  assert_int_equal(test_temp_sensor_added, 1);

  /* DT_INST_NODELABEL_STRING_ARRAY */

  const char *inst_nodelabels[] = DT_INST_NODELABEL_STRING_ARRAY(0);

  assert_int_equal(ARRAY_SIZE(inst_nodelabels), 1);
  assert_true(!strcmp(inst_nodelabels[0], "test_temp_sensor"));

  /* DT_INST_NUM_NODELABELS */

  assert_int_equal(DT_INST_NUM_NODELABELS(0), 1);

#undef VAR_PLUS
#undef ENTRY
#undef DT_DRV_COMPAT
}

static void test_foreach_prop_elem(void** state)
{
  /* We don't know what platform we are running on, so settle for
   * some basic checks related to nodes we know are in our overlay.
   */

#define TIMES_TWO(node_id, prop, idx) \
  (2 * DT_PROP_BY_IDX(node_id, prop, idx)),
#define BY_IDX_COMMA(node_id, prop, idx) DT_PROP_BY_IDX(node_id, prop, idx),

  int array_a[] = {
    DT_FOREACH_PROP_ELEM(TEST_ARRAYS, a, TIMES_TWO)
  };

  assert_int_equal(ARRAY_SIZE(array_a), 3);
  assert_int_equal(array_a[0], 2000);
  assert_int_equal(array_a[1], 4000);
  assert_int_equal(array_a[2], 6000);

  int array_b[] = {
    DT_FOREACH_PROP_ELEM(TEST_ARRAYS, b, TIMES_TWO)
  };

  assert_int_equal(ARRAY_SIZE(array_b), 4);
  assert_int_equal(array_b[0], 2 * 0xaa);
  assert_int_equal(array_b[1], 2 * 0xbb);
  assert_int_equal(array_b[2], 2 * 0xcc);
  assert_int_equal(array_b[3], 2 * 0xdd);

  static const char *const array_c[] = {
    DT_FOREACH_PROP_ELEM(TEST_ARRAYS, c, BY_IDX_COMMA)
  };

  assert_int_equal(ARRAY_SIZE(array_c), 2);
  assert_true(!strcmp(array_c[0], "bar"));
  assert_true(!strcmp(array_c[1], "baz"));

  static const char *const array_val[] = {
    DT_FOREACH_PROP_ELEM(TEST_ENUM_0, val, BY_IDX_COMMA)
  };

  assert_int_equal(ARRAY_SIZE(array_val), 1);
  assert_true(!strcmp(array_val[0], "zero"));

  static const char *const string_nuttx_user[] = {
    DT_FOREACH_PROP_ELEM(NUTTX_USER, string, BY_IDX_COMMA)
  };

  assert_int_equal(ARRAY_SIZE(string_nuttx_user), 1);
  assert_true(!strcmp(string_nuttx_user[0], "foo"));

#undef BY_IDX_COMMA

#define PATH_COMMA(node_id, prop, idx) \
  DT_NODE_PATH(DT_PROP_BY_IDX(node_id, prop, idx)),

  static const char *const array_ph[] = {
    DT_FOREACH_PROP_ELEM(TEST_PH, ph, PATH_COMMA)
  };

  assert_int_equal(ARRAY_SIZE(array_ph), 1);
  assert_true(!strcmp(array_ph[0], DT_NODE_PATH(TEST_GPIO_1)));

  static const char *const array_nuttx_user_ph[] = {
    DT_FOREACH_PROP_ELEM(NUTTX_USER, ph, PATH_COMMA)
  };

  assert_int_equal(ARRAY_SIZE(array_nuttx_user_ph), 1);
  assert_true(!strcmp(array_nuttx_user_ph[0], DT_NODE_PATH(TEST_GPIO_1)));

  static const char *const array_phs[] = {
    DT_FOREACH_PROP_ELEM(TEST_PH, phs, PATH_COMMA)
  };

  assert_int_equal(ARRAY_SIZE(array_phs), 2);
  assert_true(!strcmp(array_phs[0], DT_NODE_PATH(TEST_I2C)));
  assert_true(!strcmp(array_phs[1], DT_NODE_PATH(TEST_SPI)));

#undef PATH_COMMA

  int array_sep[] = {
    DT_FOREACH_PROP_ELEM_SEP(TEST_ARRAYS, a, DT_PROP_BY_IDX, (, ))
  };

  assert_int_equal(ARRAY_SIZE(array_sep), 3);
  assert_int_equal(array_sep[0], 1000);
  assert_int_equal(array_sep[1], 2000);
  assert_int_equal(array_sep[2], 3000);

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_array_holder

  int inst_array[] = {
    DT_INST_FOREACH_PROP_ELEM(0, a, TIMES_TWO)
  };

  assert_int_equal(ARRAY_SIZE(inst_array), ARRAY_SIZE(array_a));
  assert_int_equal(inst_array[0], array_a[0]);
  assert_int_equal(inst_array[1], array_a[1]);
  assert_int_equal(inst_array[2], array_a[2]);

  int inst_array_sep[] = {
    DT_INST_FOREACH_PROP_ELEM_SEP(0, a, DT_PROP_BY_IDX, (, ))
  };

  assert_int_equal(ARRAY_SIZE(inst_array_sep), ARRAY_SIZE(array_sep));
  assert_int_equal(inst_array_sep[0], array_sep[0]);
  assert_int_equal(inst_array_sep[1], array_sep[1]);
  assert_int_equal(inst_array_sep[2], array_sep[2]);

#undef TIMES_TWO
}

static void test_foreach_prop_elem_varg(void** state)
{
#define TIMES_TWO_ADD(node_id, prop, idx, arg) \
  ((2 * DT_PROP_BY_IDX(node_id, prop, idx)) + arg),

  int array[] = {
    DT_FOREACH_PROP_ELEM_VARGS(TEST_ARRAYS, a, TIMES_TWO_ADD, 3)
  };

  assert_int_equal(ARRAY_SIZE(array), 3);
  assert_int_equal(array[0], 2003);
  assert_int_equal(array[1], 4003);
  assert_int_equal(array[2], 6003);

#define PROP_PLUS_ARG(node_id, prop, idx, arg) \
  (DT_PROP_BY_IDX(node_id, prop, idx) + arg)

  int array_sep[] = {
    DT_FOREACH_PROP_ELEM_SEP_VARGS(TEST_ARRAYS, a, PROP_PLUS_ARG,
      (, ), 3)
  };

  assert_int_equal(ARRAY_SIZE(array_sep), 3);
  assert_int_equal(array_sep[0], 1003);
  assert_int_equal(array_sep[1], 2003);
  assert_int_equal(array_sep[2], 3003);

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_array_holder

  int inst_array[] = {
    DT_INST_FOREACH_PROP_ELEM_VARGS(0, a, TIMES_TWO_ADD, 3)
  };

  assert_int_equal(ARRAY_SIZE(inst_array), ARRAY_SIZE(array));
  assert_int_equal(inst_array[0], array[0]);
  assert_int_equal(inst_array[1], array[1]);
  assert_int_equal(inst_array[2], array[2]);

  int inst_array_sep[] = {
    DT_INST_FOREACH_PROP_ELEM_SEP_VARGS(0, a, PROP_PLUS_ARG, (, ),
      3)
  };

  assert_int_equal(ARRAY_SIZE(inst_array_sep), ARRAY_SIZE(array_sep));
  assert_int_equal(inst_array_sep[0], array_sep[0]);
  assert_int_equal(inst_array_sep[1], array_sep[1]);
  assert_int_equal(inst_array_sep[2], array_sep[2]);
#undef TIMES_TWO
}

static void test_chosen(void** state)
{
  assert_int_equal(DT_HAS_CHOSEN(ztest_xxxx), 0);
  assert_int_equal(DT_HAS_CHOSEN(ztest_gpio), 1);
  assert_true(!strcmp(TO_STRING(DT_CHOSEN(ztest_gpio)),
    "DT_N_S_test_S_gpio_deadbeef"));
}

#define TO_MY_ENUM(token) TO_MY_ENUM_2(token) /* force another expansion */
#define TO_MY_ENUM_2(token) MY_ENUM_##token
static void test_enums(void** state)
{
  /* DT_ENUM_IDX and DT_ENUM_HAS_VALUE on string enum */

  assert_int_equal(DT_ENUM_IDX(TEST_ENUM_0, val), 0);
  assert_true(DT_ENUM_HAS_VALUE(TEST_ENUM_0, val, zero));
  assert_false(DT_ENUM_HAS_VALUE(TEST_ENUM_0, val, one));
  assert_false(DT_ENUM_HAS_VALUE(TEST_ENUM_0, val, two));

  /* DT_ENUM_IDX and DT_ENUM_HAS_VALUE on int enum */

  assert_int_equal(DT_ENUM_IDX(
    DT_NODELABEL(test_enum_int_default_0), val), 0);
  assert_true(DT_ENUM_HAS_VALUE(
    DT_NODELABEL(test_enum_int_default_0), val, 5));
  assert_false(DT_ENUM_HAS_VALUE(
    DT_NODELABEL(test_enum_int_default_0), val, 6));
  assert_false(DT_ENUM_HAS_VALUE(
    DT_NODELABEL(test_enum_int_default_0), val, 7));

  /* DT_ENUM_IDX_BY_IDX and DT_ENUM_HAS_VALUE_BY_IDX on string-array enum */

  assert_int_equal(DT_ENUM_IDX_BY_IDX(
    DT_NODELABEL(test_enum_string_array), val, 0), 0);
  assert_int_equal(DT_ENUM_IDX_BY_IDX(
    DT_NODELABEL(test_enum_string_array), val, 1), 3);
  assert_int_equal(DT_ENUM_IDX_BY_IDX(
    DT_NODELABEL(test_enum_string_array), val, 2), 0);
  assert_true(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_string_array), val, 0, foo));
  assert_false(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_string_array), val, 0, bar));
  assert_false(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_string_array), val, 0, baz));
  assert_false(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_string_array), val, 0, zoo));
  assert_true(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_string_array), val, 1, zoo));
  assert_false(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_string_array), val, 1, foo));
  assert_false(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_string_array), val, 1, bar));
  assert_false(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_string_array), val, 1, baz));
  assert_true(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_string_array), val, 2, foo));
  assert_false(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_string_array), val, 2, baz));
  assert_false(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_string_array), val, 2, bar));
  assert_false(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_string_array), val, 2, zoo));

  /* DT_ENUM_IDX_BY_IDX and DT_ENUM_HAS_VALUE_BY_IDX on int-array enum */

  assert_int_equal(DT_ENUM_IDX_BY_IDX(
    DT_NODELABEL(test_enum_int_array), val, 0), 3);
  assert_int_equal(DT_ENUM_IDX_BY_IDX(
    DT_NODELABEL(test_enum_int_array), val, 1), 4);
  assert_int_equal(DT_ENUM_IDX_BY_IDX(
    DT_NODELABEL(test_enum_int_array), val, 2), 3);
  assert_int_equal(DT_ENUM_IDX_BY_IDX(
    DT_NODELABEL(test_enum_int_array), val, 3), 7);
  assert_true(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_int_array), val, 0, 4));
  assert_false(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_int_array), val, 0, 5));
  assert_false(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_int_array), val, 0, 6));
  assert_true(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_int_array), val, 1, 3));
  assert_false(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_int_array), val, 1, 0));
  assert_false(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_int_array), val, 1, 1));
  assert_true(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_int_array), val, 2, 4));
  assert_false(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_int_array), val, 2, 3));
  assert_false(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_int_array), val, 2, 7));
  assert_true(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_int_array), val, 3, 0));
  assert_false(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_int_array), val, 3, 2));
  assert_false(DT_ENUM_HAS_VALUE_BY_IDX(
    DT_NODELABEL(test_enum_int_array), val, 3, 1));
}
#undef TO_MY_ENUM
#undef TO_MY_ENUM_2

static void test_enums_required_false(void** state)
{
  /* DT_ENUM_IDX_OR on string value */

  assert_int_equal(DT_ENUM_IDX_OR(
    DT_NODELABEL(test_enum_default_0), val, 2), 1);
  assert_int_equal(DT_ENUM_IDX_OR(
    DT_NODELABEL(test_enum_default_1), val, 2), 2);

  /* DT_ENUM_IDX_OR on int value */

  assert_int_equal(DT_ENUM_IDX_OR(
    DT_NODELABEL(test_enum_int_default_0), val, 4), 0);
  assert_int_equal(DT_ENUM_IDX_OR(
    DT_NODELABEL(test_enum_int_default_1), val, 4), 4);

  /* DT_ENUM_IDX_OR on string-array value */

  assert_int_equal(DT_ENUM_IDX_BY_IDX_OR(
    DT_NODELABEL(test_enum_string_array), val, 0, 2), 0);
  assert_int_equal(DT_ENUM_IDX_BY_IDX_OR(
    DT_NODELABEL(test_enum_string_array), val, 5, 2), 2);

  /* DT_ENUM_IDX_OR on int-array value */

  assert_int_equal(
    DT_ENUM_IDX_BY_IDX_OR(DT_NODELABEL(test_enum_int_array), val, 0, 7), 3);
  assert_int_equal(
    DT_ENUM_IDX_BY_IDX_OR(DT_NODELABEL(test_enum_int_array), val, 4, 7), 7);
}

static void test_inst_enums(void** state)
{
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_enum_holder_inst
  assert_int_equal(DT_INST_ENUM_IDX(0, val), 0);
  assert_int_equal(DT_INST_ENUM_IDX_OR(0, val, 2), 0);
  assert_true(DT_INST_ENUM_HAS_VALUE(0, val, zero));
  assert_false(DT_INST_ENUM_HAS_VALUE(0, val, one));
  assert_false(DT_INST_ENUM_HAS_VALUE(0, val, two));

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_enum_required_false_holder_inst
  assert_int_equal(DT_INST_ENUM_IDX_OR(0, val, 2), 2);
  assert_false(DT_INST_ENUM_HAS_VALUE(0, val, zero));
  assert_false(DT_INST_ENUM_HAS_VALUE(0, val, one));
  assert_false(DT_INST_ENUM_HAS_VALUE(0, val, two));

/* Also add tests for these:
 * DT_INST_ENUM_IDX_BY_IDX
 * DT_INST_ENUM_IDX_BY_IDX_OR
 * DT_INST_ENUM_HAS_VALUE_BY_IDX
 */
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_enum_string_array_holder
  assert_int_equal(DT_INST_ENUM_IDX_BY_IDX(0, val, 0), 0);
  assert_int_equal(DT_INST_ENUM_IDX_BY_IDX(0, val, 1), 3);
  assert_true(DT_INST_ENUM_HAS_VALUE_BY_IDX(0, val, 0, foo));
  assert_false(DT_INST_ENUM_HAS_VALUE_BY_IDX(0, val, 0, zoo));
  assert_true(DT_INST_ENUM_HAS_VALUE_BY_IDX(0, val, 1, zoo));
  assert_false(DT_INST_ENUM_HAS_VALUE_BY_IDX(0, val, 2, baz));
  assert_int_equal(DT_INST_ENUM_IDX_BY_IDX_OR(0, val, 0, 10), 0);
  assert_int_equal(DT_INST_ENUM_IDX_BY_IDX_OR(0, val, 4, 10), 10);

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_enum_int_array_holder
  assert_int_equal(DT_INST_ENUM_IDX_BY_IDX(0, val, 0), 3);
  assert_int_equal(DT_INST_ENUM_IDX_BY_IDX(0, val, 3), 7);
  assert_int_equal(DT_INST_ENUM_IDX_BY_IDX_OR(0, val, 1, 10), 4);
  assert_int_equal(DT_INST_ENUM_IDX_BY_IDX_OR(0, val, 123654, 10), 10);
}

static void test_parent(void** state)
{
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_spi_device

  assert_true(DT_SAME_NODE(DT_PARENT(TEST_SPI_DEV_0), TEST_SPI_BUS_0));

  /* The parent's label for the first instance of vnd,spi-device,
   * child of TEST_SPI, is the same as TEST_SPI.
   */

  assert_true(DT_SAME_NODE(DT_INST_PARENT(0), TEST_SPI));

  /* We should be able to use DT_PARENT() even with nodes, like /test,
   * that have no matching compatible.
   */

  assert_true(DT_SAME_NODE(DT_CHILD(DT_PARENT(TEST_SPI_BUS_0), spi_33334444),
    TEST_SPI_BUS_0));
}

static void test_parent_nodes_list(void** state)
{
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_parent_bindings

  /* When traversing upwards, there are no fixed attributes and labels */

#define TEST_FUNC(parent) \
  { /* No operation */  \
  }
#define TEST_FUNC_AND_COMMA(parent) TEST_FUNC(parent),

  struct vnd_parent_binding {
    int val;
  };

  struct vnd_parent_binding vals_a[] = {
    DT_FOREACH_ANCESTOR(DT_NODELABEL(test_parent_a), TEST_FUNC_AND_COMMA)
  };

  struct vnd_parent_binding vals_b[] = {
    DT_FOREACH_ANCESTOR(DT_NODELABEL(test_parent_b), TEST_FUNC_AND_COMMA)
  };

  assert_int_equal(ARRAY_SIZE(vals_a), 3);
  assert_int_equal(ARRAY_SIZE(vals_b), 4);

#undef TEST_FUNC_AND_COMMA
#undef TEST_FUNC
}

static void test_gparent(void** state)
{
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_i2c_mux_controller

  assert_true(DT_SAME_NODE(DT_GPARENT(TEST_I2C_MUX_CTLR_1), TEST_I2C));
  assert_true(DT_SAME_NODE(DT_INST_GPARENT(0), TEST_I2C));
  assert_true(DT_SAME_NODE(DT_INST_GPARENT(1), TEST_I2C));
}

static void test_children(void** state)
{
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_child_bindings

  assert_int_equal(DT_PROP(DT_CHILD(DT_NODELABEL(test_children), child_a),
             val),
    0);
  assert_int_equal(DT_PROP(DT_CHILD(DT_NODELABEL(test_children), child_b),
             val),
    1);
  assert_int_equal(DT_PROP(DT_CHILD(DT_NODELABEL(test_children), child_c),
             val),
    2);

  assert_int_equal(DT_PROP(DT_INST_CHILD(0, child_a), val), 0);
  assert_int_equal(DT_PROP(DT_INST_CHILD(0, child_b), val), 1);
  assert_int_equal(DT_PROP(DT_INST_CHILD(0, child_c), val), 2);
}

static void test_child_nodes_list(void** state)
{
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_child_bindings

#define TEST_FUNC(child)  \
  {             \
    DT_PROP(child, val) \
  }
#define TEST_FUNC_AND_COMMA(child) TEST_FUNC(child),
#define TEST_PARENT DT_PARENT(DT_NODELABEL(test_child_a))

  struct vnd_child_binding {
    int val;
  };

  struct vnd_child_binding vals[] = {
    DT_FOREACH_CHILD(TEST_PARENT, TEST_FUNC_AND_COMMA)
  };

  struct vnd_child_binding vals_sep[] = {
    DT_FOREACH_CHILD_SEP(TEST_PARENT, TEST_FUNC, (, ))
  };

  struct vnd_child_binding vals_inst[] = {
    DT_INST_FOREACH_CHILD(0, TEST_FUNC_AND_COMMA)
  };

  struct vnd_child_binding vals_inst_sep[] = {
    DT_INST_FOREACH_CHILD_SEP(0, TEST_FUNC, (, ))
  };

  struct vnd_child_binding vals_status_okay[] = {
    DT_FOREACH_CHILD_STATUS_OKAY(TEST_PARENT, TEST_FUNC_AND_COMMA)
  };

  struct vnd_child_binding vals_status_okay_inst[] = {
    DT_INST_FOREACH_CHILD_STATUS_OKAY(0, TEST_FUNC_AND_COMMA)
  };

  struct vnd_child_binding vals_status_okay_sep[] = {
    DT_FOREACH_CHILD_STATUS_OKAY_SEP(TEST_PARENT, TEST_FUNC, (, ))
  };

  struct vnd_child_binding vals_status_okay_inst_sep[] = {
    DT_INST_FOREACH_CHILD_STATUS_OKAY_SEP(0, TEST_FUNC, (, ))
  };

  assert_int_equal(ARRAY_SIZE(vals), 3);
  assert_int_equal(ARRAY_SIZE(vals_sep), 3);
  assert_int_equal(ARRAY_SIZE(vals_inst), 3);
  assert_int_equal(ARRAY_SIZE(vals_inst_sep), 3);
  assert_int_equal(ARRAY_SIZE(vals_status_okay), 2);
  assert_int_equal(ARRAY_SIZE(vals_status_okay_inst), 2);
  assert_int_equal(ARRAY_SIZE(vals_status_okay_sep), 2);
  assert_int_equal(ARRAY_SIZE(vals_status_okay_inst_sep), 2);

  assert_int_equal(vals[0].val, 0);
  assert_int_equal(vals[1].val, 1);
  assert_int_equal(vals[2].val, 2);
  assert_int_equal(vals_sep[0].val, 0);
  assert_int_equal(vals_sep[1].val, 1);
  assert_int_equal(vals_sep[2].val, 2);
  assert_int_equal(vals_inst[0].val, 0);
  assert_int_equal(vals_inst[1].val, 1);
  assert_int_equal(vals_inst[2].val, 2);
  assert_int_equal(vals_inst_sep[0].val, 0);
  assert_int_equal(vals_inst_sep[1].val, 1);
  assert_int_equal(vals_inst_sep[2].val, 2);
  assert_int_equal(vals_status_okay[0].val, 0);
  assert_int_equal(vals_status_okay[1].val, 1);
  assert_int_equal(vals_status_okay_inst[0].val, 0);
  assert_int_equal(vals_status_okay_inst[1].val, 1);
  assert_int_equal(vals_status_okay_sep[0].val, 0);
  assert_int_equal(vals_status_okay_sep[1].val, 1);
  assert_int_equal(vals_status_okay_inst_sep[0].val, 0);
  assert_int_equal(vals_status_okay_inst_sep[1].val, 1);

#undef TEST_PARENT
#undef TEST_FUNC_AND_COMMA
#undef TEST_FUNC
}

static void test_child_nodes_list_varg(void** state)
{
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_child_bindings

#define TEST_FUNC(child, arg)   \
  {               \
    DT_PROP(child, val) + arg \
  }
#define TEST_FUNC_AND_COMMA(child, arg) TEST_FUNC(child, arg),
#define TEST_PARENT DT_PARENT(DT_NODELABEL(test_child_a))

  struct vnd_child_binding {
    int val;
  };

  struct vnd_child_binding vals[] = {
    DT_FOREACH_CHILD_VARGS(TEST_PARENT, TEST_FUNC_AND_COMMA, 1)
  };

  struct vnd_child_binding vals_sep[] = {
    DT_FOREACH_CHILD_SEP_VARGS(TEST_PARENT, TEST_FUNC, (, ), 1)
  };

  struct vnd_child_binding vals_inst[] = {
    DT_INST_FOREACH_CHILD_VARGS(0, TEST_FUNC_AND_COMMA, 1)
  };

  struct vnd_child_binding vals_inst_sep[] = {
    DT_INST_FOREACH_CHILD_SEP_VARGS(0, TEST_FUNC, (, ), 1)
  };

  struct vnd_child_binding vals_status_okay[] = {
    DT_FOREACH_CHILD_STATUS_OKAY_VARGS(TEST_PARENT, TEST_FUNC_AND_COMMA, 1)
  };

  struct vnd_child_binding vals_status_okay_inst[] = {
    DT_INST_FOREACH_CHILD_STATUS_OKAY_VARGS(0, TEST_FUNC_AND_COMMA, 1)
  };

  struct vnd_child_binding vals_status_okay_sep[] = {
    DT_FOREACH_CHILD_STATUS_OKAY_SEP_VARGS(TEST_PARENT, TEST_FUNC, (, ), 1)
  };

  struct vnd_child_binding vals_status_okay_inst_sep[] = {
    DT_INST_FOREACH_CHILD_STATUS_OKAY_SEP_VARGS(0, TEST_FUNC, (, ), 1)
  };

  assert_int_equal(ARRAY_SIZE(vals), 3);
  assert_int_equal(ARRAY_SIZE(vals_sep), 3);
  assert_int_equal(ARRAY_SIZE(vals_inst), 3);
  assert_int_equal(ARRAY_SIZE(vals_inst_sep), 3);
  assert_int_equal(ARRAY_SIZE(vals_status_okay), 2);
  assert_int_equal(ARRAY_SIZE(vals_status_okay_inst), 2);
  assert_int_equal(ARRAY_SIZE(vals_status_okay_sep), 2);
  assert_int_equal(ARRAY_SIZE(vals_status_okay_inst_sep), 2);

  assert_int_equal(vals[0].val, 1);
  assert_int_equal(vals[1].val, 2);
  assert_int_equal(vals[2].val, 3);
  assert_int_equal(vals_sep[0].val, 1);
  assert_int_equal(vals_sep[1].val, 2);
  assert_int_equal(vals_sep[2].val, 3);
  assert_int_equal(vals_inst[0].val, 1);
  assert_int_equal(vals_inst[1].val, 2);
  assert_int_equal(vals_inst[2].val, 3);
  assert_int_equal(vals_inst_sep[0].val, 1);
  assert_int_equal(vals_inst_sep[1].val, 2);
  assert_int_equal(vals_inst_sep[2].val, 3);
  assert_int_equal(vals_status_okay[0].val, 1);
  assert_int_equal(vals_status_okay[1].val, 2);
  assert_int_equal(vals_status_okay_inst[0].val, 1);
  assert_int_equal(vals_status_okay_inst[1].val, 2);
  assert_int_equal(vals_status_okay_sep[0].val, 1);
  assert_int_equal(vals_status_okay_sep[1].val, 2);
  assert_int_equal(vals_status_okay_inst_sep[0].val, 1);
  assert_int_equal(vals_status_okay_inst_sep[1].val, 2);

#undef TEST_PARENT
#undef TEST_FUNC_AND_COMMA
#undef TEST_FUNC
}

static void test_child_nodes_number(void** state)
{
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_child_bindings

  assert_int_equal(DT_CHILD_NUM(TEST_CHILDREN), 3);
  assert_int_equal(DT_INST_CHILD_NUM(0), 3);
  assert_int_equal(DT_CHILD_NUM_STATUS_OKAY(TEST_CHILDREN), 2);
  assert_int_equal(DT_INST_CHILD_NUM_STATUS_OKAY(0), 2);

#ifdef CONFIG_TESTING_DEVICETREE_DYNAMIC_API
  g_board_id = 1;
  assert_int_equal(DT_CHILD_NUM_DYNAMIC(g_board_id, TEST_CHILDREN), 3);
  assert_int_equal(
    DT_CHILD_NUM_STATUS_OKAY_DYNAMIC(g_board_id, TEST_CHILDREN), 2);
  g_board_id = 2;
  assert_int_equal(DT_CHILD_NUM_DYNAMIC(g_board_id, TEST_CHILDREN), 4);
  assert_int_equal(
    DT_CHILD_NUM_STATUS_OKAY_DYNAMIC(g_board_id, TEST_CHILDREN), 3);
#endif
}

static void test_great_grandchild(void** state)
{
  assert_int_equal(DT_PROP(DT_NODELABEL(test_ggc), ggc_prop), 42);
}

static void test_ranges_pcie(void** state)
{
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_test_ranges_pcie

#define FLAGS(node_id, idx) \
  DT_RANGES_CHILD_BUS_FLAGS_BY_IDX(node_id, idx),
#define CHILD_BUS_ADDR(node_id, idx) \
  DT_RANGES_CHILD_BUS_ADDRESS_BY_IDX(node_id, idx),
#define PARENT_BUS_ADDR(node_id, idx) \
  DT_RANGES_PARENT_BUS_ADDRESS_BY_IDX(node_id, idx),
#define LENGTH(node_id, idx) DT_RANGES_LENGTH_BY_IDX(node_id, idx),

  unsigned int count = DT_NUM_RANGES(TEST_RANGES_PCIE);

  const uint64_t ranges_pcie_flags[] = {
    DT_FOREACH_RANGE(TEST_RANGES_PCIE, FLAGS)
  };

  const uint64_t ranges_child_bus_addr[] = {
    DT_FOREACH_RANGE(TEST_RANGES_PCIE, CHILD_BUS_ADDR)
  };

  const uint64_t ranges_parent_bus_addr[] = {
    DT_FOREACH_RANGE(TEST_RANGES_PCIE, PARENT_BUS_ADDR)
  };

  const uint64_t ranges_length[] = {
    DT_FOREACH_RANGE(TEST_RANGES_PCIE, LENGTH)
  };

  assert_int_equal(count, 3);

  assert_int_equal(DT_RANGES_HAS_IDX(TEST_RANGES_PCIE, 0), 1);
  assert_int_equal(DT_RANGES_HAS_IDX(TEST_RANGES_PCIE, 1), 1);
  assert_int_equal(DT_RANGES_HAS_IDX(TEST_RANGES_PCIE, 2), 1);
  assert_int_equal(DT_RANGES_HAS_IDX(TEST_RANGES_PCIE, 3), 0);

  assert_int_equal(
    DT_RANGES_HAS_CHILD_BUS_FLAGS_AT_IDX(TEST_RANGES_PCIE, 0), 1);
  assert_int_equal(
    DT_RANGES_HAS_CHILD_BUS_FLAGS_AT_IDX(TEST_RANGES_PCIE, 1), 1);
  assert_int_equal(
    DT_RANGES_HAS_CHILD_BUS_FLAGS_AT_IDX(TEST_RANGES_PCIE, 2), 1);
  assert_int_equal(
    DT_RANGES_HAS_CHILD_BUS_FLAGS_AT_IDX(TEST_RANGES_PCIE, 3), 0);

  assert_int_equal(ranges_pcie_flags[0], 0x1000000);
  assert_int_equal(ranges_pcie_flags[1], 0x2000000);
  assert_int_equal(ranges_pcie_flags[2], 0x3000000);
  assert_int_equal(ranges_child_bus_addr[0], 0);
  assert_int_equal(ranges_child_bus_addr[1], 0x10000000);
  assert_int_equal(ranges_child_bus_addr[2], 0x8000000000);
  assert_int_equal(ranges_parent_bus_addr[0], 0x3eff0000);
  assert_int_equal(ranges_parent_bus_addr[1], 0x10000000);
  assert_int_equal(ranges_parent_bus_addr[2], 0x8000000000);
  assert_int_equal(ranges_length[0], 0x10000);
  assert_int_equal(ranges_length[1], 0x2eff0000);
  assert_int_equal(ranges_length[2], 0x8000000000);

#undef FLAGS
#undef CHILD_BUS_ADDR
#undef PARENT_BUS_ADDR
#undef LENGTH
}

static void test_ranges_other(void** state)
{
#define HAS_FLAGS(node_id, idx) \
  DT_RANGES_HAS_CHILD_BUS_FLAGS_AT_IDX(node_id, idx)
#define FLAGS(node_id, idx) \
  DT_RANGES_CHILD_BUS_FLAGS_BY_IDX(node_id, idx),
#define CHILD_BUS_ADDR(node_id, idx) \
  DT_RANGES_CHILD_BUS_ADDRESS_BY_IDX(node_id, idx),
#define PARENT_BUS_ADDR(node_id, idx) \
  DT_RANGES_PARENT_BUS_ADDRESS_BY_IDX(node_id, idx),
#define LENGTH(node_id, idx) DT_RANGES_LENGTH_BY_IDX(node_id, idx),

  unsigned int count = DT_NUM_RANGES(TEST_RANGES_OTHER);

  const uint32_t ranges_child_bus_addr[] = {
    DT_FOREACH_RANGE(TEST_RANGES_OTHER, CHILD_BUS_ADDR)
  };

  const uint32_t ranges_parent_bus_addr[] = {
    DT_FOREACH_RANGE(TEST_RANGES_OTHER, PARENT_BUS_ADDR)
  };

  const uint32_t ranges_length[] = {
    DT_FOREACH_RANGE(TEST_RANGES_OTHER, LENGTH)
  };

  assert_int_equal(count, 2);

  assert_int_equal(DT_RANGES_HAS_IDX(TEST_RANGES_OTHER, 0), 1);
  assert_int_equal(DT_RANGES_HAS_IDX(TEST_RANGES_OTHER, 1), 1);
  assert_int_equal(DT_RANGES_HAS_IDX(TEST_RANGES_OTHER, 2), 0);
  assert_int_equal(DT_RANGES_HAS_IDX(TEST_RANGES_OTHER, 3), 0);

  assert_int_equal(HAS_FLAGS(TEST_RANGES_OTHER, 0), 0);
  assert_int_equal(HAS_FLAGS(TEST_RANGES_OTHER, 1), 0);
  assert_int_equal(HAS_FLAGS(TEST_RANGES_OTHER, 2), 0);
  assert_int_equal(HAS_FLAGS(TEST_RANGES_OTHER, 3), 0);

  assert_int_equal(ranges_child_bus_addr[0], 0);
  assert_int_equal(ranges_child_bus_addr[1], 0x10000000);
  assert_int_equal(ranges_parent_bus_addr[0], 0x3eff0000);
  assert_int_equal(ranges_parent_bus_addr[1], 0x10000000);
  assert_int_equal(ranges_length[0], 0x10000);
  assert_int_equal(ranges_length[1], 0x2eff0000);

#undef HAS_FLAGS
#undef FLAGS
#undef CHILD_BUS_ADDR
#undef PARENT_BUS_ADDR
#undef LENGTH
}

static void test_ranges_empty(void** state)
{
  assert_int_equal(DT_NODE_HAS_PROP(TEST_RANGES_EMPTY, ranges), 1);
  assert_int_equal(DT_NUM_RANGES(TEST_RANGES_EMPTY), 0);
  assert_int_equal(DT_RANGES_HAS_IDX(TEST_RANGES_EMPTY, 0), 0);
  assert_int_equal(DT_RANGES_HAS_IDX(TEST_RANGES_EMPTY, 1), 0);

#define FAIL(node_id, idx) cmocka_assert(0);

  DT_FOREACH_RANGE(TEST_RANGES_EMPTY, FAIL);

#undef FAIL
}

static void test_compat_get_any_status_okay(void** state)
{
  assert_true(
    DT_SAME_NODE(
      DT_COMPAT_GET_ANY_STATUS_OKAY(vnd_reg_holder),
      TEST_REG));

  /* DT_SAME_NODE requires that both its arguments are valid
   * node identifiers, so we can't pass it DT_INVALID_NODE,
   * which is what this DT_COMPAT_GET_ANY_STATUS_OKAY() expands to.
   */

  assert_false(
    DT_NODE_EXISTS(
      DT_COMPAT_GET_ANY_STATUS_OKAY(this_is_not_a_real_compat)));
}

static void test_path(void** state)
{
  assert_true(!strcmp(DT_NODE_PATH(DT_ROOT), "/"));
  assert_true(!strcmp(DT_NODE_PATH(TEST_DEADBEEF),
    "/test/gpio@deadbeef"));
}

static void test_node_name(void** state)
{
  assert_true(!strcmp(DT_NODE_FULL_NAME(DT_ROOT), "/"));
  assert_true(!strcmp(DT_NODE_FULL_NAME(TEST_DEADBEEF),
    "gpio@deadbeef"));
  assert_true(!strcmp(DT_NODE_FULL_NAME(TEST_TEMP),
    "temperature-sensor"));
  assert_true(strcmp(DT_NODE_FULL_NAME(TEST_REG),
    "reg-holder"));
}

static void test_node_child_idx(void** state)
{
  assert_int_equal(DT_NODE_CHILD_IDX(DT_NODELABEL(test_child_a)), 0);
  assert_int_equal(DT_NODE_CHILD_IDX(DT_NODELABEL(test_child_b)), 1);
  assert_int_equal(DT_NODE_CHILD_IDX(DT_NODELABEL(test_child_c)), 2);
}

static void test_same_node(void** state)
{
  assert_true(DT_SAME_NODE(TEST_DEADBEEF, TEST_DEADBEEF));
  assert_false(DT_SAME_NODE(TEST_DEADBEEF, TEST_ABCD1234));
}

static void test_string_token(void** state)
{
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_string_token
  enum {
    token_zero,
    token_one,
    token_two,
    token_no_inst,
  };

  enum {
    TOKEN_ZERO = token_no_inst + 1,
    TOKEN_ONE,
    TOKEN_TWO,
    TOKEN_NO_INST,
  };

  int i;

  /* Test DT_INST_STRING_TOKEN */
#define STRING_TOKEN_TEST_INST_EXPANSION(inst) \
  DT_INST_STRING_TOKEN(inst, val),
  int array_inst[] = {
    DT_INST_FOREACH_STATUS_OKAY(STRING_TOKEN_TEST_INST_EXPANSION)
  };

  for (i = 0; i < ARRAY_SIZE(array_inst); i++)
    {
      assert_in_range(array_inst[i], token_zero, token_two);
    }

  /* Test DT_INST_STRING_UPPER_TOKEN */
#define STRING_UPPER_TOKEN_TEST_INST_EXPANSION(inst) \
  DT_INST_STRING_UPPER_TOKEN(inst, val),
  int array_inst_upper[] = {
    DT_INST_FOREACH_STATUS_OKAY(STRING_UPPER_TOKEN_TEST_INST_EXPANSION)
  };

  for (i = 0; i < ARRAY_SIZE(array_inst_upper); i++)
    {
      assert_in_range(array_inst_upper[i], TOKEN_ZERO, TOKEN_TWO);
    }

  /* Test DT_INST_STRING_TOKEN_OR when property is found */
#define STRING_TOKEN_OR_TEST_INST_EXPANSION(inst) \
  DT_INST_STRING_TOKEN_OR(inst, val, token_no_inst),
  int array_inst_or[] = {
    DT_INST_FOREACH_STATUS_OKAY(STRING_TOKEN_OR_TEST_INST_EXPANSION)
  };

  for (i = 0; i < ARRAY_SIZE(array_inst_or); i++)
    {
      assert_in_range(array_inst_or[i], token_zero, token_two);
    }

  /* Test DT_INST_STRING_UPPER_TOKEN_OR when property is found */

#define STRING_UPPER_TOKEN_OR_TEST_INST_EXPANSION(inst) \
  DT_INST_STRING_UPPER_TOKEN_OR(inst, val, TOKEN_NO_INST),
  int array_inst_upper_or[] = {
    DT_INST_FOREACH_STATUS_OKAY(STRING_UPPER_TOKEN_OR_TEST_INST_EXPANSION)
  };

  for (i = 0; i < ARRAY_SIZE(array_inst_upper_or); i++)
    {
      assert_in_range(array_inst_upper_or[i], TOKEN_ZERO, TOKEN_TWO);
    }

  /* Test DT_STRING_TOKEN_OR when property is found */

  assert_int_equal(DT_STRING_TOKEN_OR(
    DT_NODELABEL(test_string_token_0), val, token_one), token_zero);
  assert_int_equal(DT_STRING_TOKEN_OR(
    DT_NODELABEL(test_string_token_1), val, token_two), token_one);

  /* Test DT_STRING_TOKEN_OR is not found */

  assert_int_equal(DT_STRING_TOKEN_OR(
    DT_NODELABEL(test_string_token_1), no_inst, token_zero), token_zero);

  /* Test DT_STRING_UPPER_TOKEN_OR when property is found */

  assert_int_equal(DT_STRING_UPPER_TOKEN_OR(
    DT_NODELABEL(test_string_token_0), val, TOKEN_ONE), TOKEN_ZERO);
  assert_int_equal(DT_STRING_UPPER_TOKEN_OR(
    DT_NODELABEL(test_string_token_1), val, TOKEN_TWO), TOKEN_ONE);

  /* Test DT_STRING_UPPER_TOKEN_OR is not found */

  assert_int_equal(DT_STRING_UPPER_TOKEN_OR(
    DT_NODELABEL(test_string_token_1), no_inst, TOKEN_ZERO), TOKEN_ZERO);

  /* Test DT_INST_STRING_TOKEN_OR when property is not found */

#define STRING_TOKEN_TEST_NO_INST_EXPANSION(inst) \
  DT_INST_STRING_TOKEN_OR(inst, no_inst, token_no_inst),
  int array_no_inst_or[] = {
    DT_INST_FOREACH_STATUS_OKAY(STRING_TOKEN_TEST_NO_INST_EXPANSION)
  };

  for (i = 0; i < ARRAY_SIZE(array_no_inst_or); i++)
    {
      assert_int_equal(array_no_inst_or[i], token_no_inst);
    }

  /* Test DT_INST_STRING_UPPER_TOKEN_OR when property is not found */
#define STRING_UPPER_TOKEN_TEST_NO_INST_EXPANSION(inst) \
  DT_INST_STRING_UPPER_TOKEN_OR(inst, no_inst, TOKEN_NO_INST),
  int array_no_inst_upper_or[] =
    {
      DT_INST_FOREACH_STATUS_OKAY(STRING_UPPER_TOKEN_TEST_NO_INST_EXPANSION)
    };

  for (i = 0; i < ARRAY_SIZE(array_no_inst_upper_or); i++)
    {
      assert_int_equal(array_no_inst_upper_or[i], TOKEN_NO_INST);
    }
}

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_string_array_token
static void test_string_idx_token(void** state)
{
  enum token_string_idx {
    /* Tokens */

    token_first_idx_zero,
    token_first_idx_one,
    token_first_idx_two,
    token_second_idx_zero,
    token_second_idx_one,
    token_second_idx_two,
    token_second_idx_three,

    /* Upper tokens */

    TOKEN_FIRST_IDX_ZERO,
    TOKEN_FIRST_IDX_ONE,
    TOKEN_FIRST_IDX_TWO,
    TOKEN_SECOND_IDX_ZERO,
    TOKEN_SECOND_IDX_ONE,
    TOKEN_SECOND_IDX_TWO,
    TOKEN_SECOND_IDX_THREE
  };

  /* Test direct idx access */

  assert_int_equal(DT_STRING_TOKEN_BY_IDX(
    DT_NODELABEL(test_str_array_token_0), val, 0), token_first_idx_zero);
  assert_int_equal(DT_STRING_TOKEN_BY_IDX(
    DT_NODELABEL(test_str_array_token_0), val, 1), token_first_idx_one);
  assert_int_equal(DT_STRING_TOKEN_BY_IDX(
    DT_NODELABEL(test_str_array_token_0), val, 2), token_first_idx_two);
  assert_int_equal(DT_STRING_TOKEN_BY_IDX(
    DT_NODELABEL(test_str_array_token_1), val, 0), token_second_idx_zero);
  assert_int_equal(DT_STRING_TOKEN_BY_IDX(
    DT_NODELABEL(test_str_array_token_1), val, 1), token_second_idx_one);
  assert_int_equal(DT_STRING_TOKEN_BY_IDX(
    DT_NODELABEL(test_str_array_token_1), val, 2), token_second_idx_two);
  assert_int_equal(DT_STRING_TOKEN_BY_IDX(
    DT_NODELABEL(test_str_array_token_1), val, 3), token_second_idx_three);

  assert_int_equal(DT_STRING_UPPER_TOKEN_BY_IDX(
    DT_NODELABEL(test_str_array_token_0), val, 0), TOKEN_FIRST_IDX_ZERO);
  assert_int_equal(DT_STRING_UPPER_TOKEN_BY_IDX(
    DT_NODELABEL(test_str_array_token_0), val, 1), TOKEN_FIRST_IDX_ONE);
  assert_int_equal(DT_STRING_UPPER_TOKEN_BY_IDX(
    DT_NODELABEL(test_str_array_token_0), val, 2), TOKEN_FIRST_IDX_TWO);
  assert_int_equal(DT_STRING_UPPER_TOKEN_BY_IDX(
    DT_NODELABEL(test_str_array_token_1), val, 0), TOKEN_SECOND_IDX_ZERO);
  assert_int_equal(DT_STRING_UPPER_TOKEN_BY_IDX(
    DT_NODELABEL(test_str_array_token_1), val, 1), TOKEN_SECOND_IDX_ONE);
  assert_int_equal(DT_STRING_UPPER_TOKEN_BY_IDX(
    DT_NODELABEL(test_str_array_token_1), val, 2), TOKEN_SECOND_IDX_TWO);
  assert_int_equal(DT_STRING_UPPER_TOKEN_BY_IDX(
    DT_NODELABEL(test_str_array_token_1), val, 3), TOKEN_SECOND_IDX_THREE);

  /* Test instances */

  /* concatenate the values of the arguments into one */

#define _DO_CONCAT(x, y) x##y
#define _CONCAT(x, y) _DO_CONCAT(x, y)
#define STRING_TOKEN_BY_IDX_VAR(node_id) _CONCAT(var_token_, node_id)
#define STRING_TOKEN_BY_IDX_TEST_INST_EXPANSION(inst)            \
  enum token_string_idx STRING_TOKEN_BY_IDX_VAR(DT_DRV_INST(inst))[] = { \
    DT_INST_STRING_TOKEN_BY_IDX(inst, val, 0),             \
    DT_INST_STRING_TOKEN_BY_IDX(inst, val, 1),             \
    DT_INST_STRING_TOKEN_BY_IDX(inst, val, 2)              \
  };
  DT_INST_FOREACH_STATUS_OKAY(STRING_TOKEN_BY_IDX_TEST_INST_EXPANSION);

  assert_int_equal(STRING_TOKEN_BY_IDX_VAR(
    DT_NODELABEL(test_str_array_token_0))[0], token_first_idx_zero);
  assert_int_equal(STRING_TOKEN_BY_IDX_VAR(
    DT_NODELABEL(test_str_array_token_0))[1], token_first_idx_one);
  assert_int_equal(STRING_TOKEN_BY_IDX_VAR(
    DT_NODELABEL(test_str_array_token_0))[2], token_first_idx_two);
  assert_int_equal(STRING_TOKEN_BY_IDX_VAR(
    DT_NODELABEL(test_str_array_token_1))[0], token_second_idx_zero);
  assert_int_equal(STRING_TOKEN_BY_IDX_VAR(
    DT_NODELABEL(test_str_array_token_1))[1], token_second_idx_one);
  assert_int_equal(STRING_TOKEN_BY_IDX_VAR(
    DT_NODELABEL(test_str_array_token_1))[2], token_second_idx_two);

#define STRING_UPPER_TOKEN_BY_IDX_VAR(node_id) _CONCAT(var_upper_token, node_id)
#define STRING_UPPER_TOKEN_BY_IDX_TEST_INST_EXPANSION(inst)            \
  enum token_string_idx STRING_UPPER_TOKEN_BY_IDX_VAR(DT_DRV_INST(inst))[] = { \
    DT_INST_STRING_UPPER_TOKEN_BY_IDX(inst, val, 0),             \
    DT_INST_STRING_UPPER_TOKEN_BY_IDX(inst, val, 1),             \
    DT_INST_STRING_UPPER_TOKEN_BY_IDX(inst, val, 2)              \
  };
  DT_INST_FOREACH_STATUS_OKAY(STRING_UPPER_TOKEN_BY_IDX_TEST_INST_EXPANSION);

  assert_int_equal(STRING_UPPER_TOKEN_BY_IDX_VAR(
    DT_NODELABEL(test_str_array_token_0))[0], TOKEN_FIRST_IDX_ZERO);
  assert_int_equal(STRING_UPPER_TOKEN_BY_IDX_VAR(
    DT_NODELABEL(test_str_array_token_0))[1], TOKEN_FIRST_IDX_ONE);
  assert_int_equal(STRING_UPPER_TOKEN_BY_IDX_VAR(
    DT_NODELABEL(test_str_array_token_0))[2], TOKEN_FIRST_IDX_TWO);
  assert_int_equal(STRING_UPPER_TOKEN_BY_IDX_VAR(
    DT_NODELABEL(test_str_array_token_1))[0], TOKEN_SECOND_IDX_ZERO);
  assert_int_equal(STRING_UPPER_TOKEN_BY_IDX_VAR(
    DT_NODELABEL(test_str_array_token_1))[1], TOKEN_SECOND_IDX_ONE);
  assert_int_equal(STRING_UPPER_TOKEN_BY_IDX_VAR(
    DT_NODELABEL(test_str_array_token_1))[2], TOKEN_SECOND_IDX_TWO);
}

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_string_unquoted
static void test_string_unquoted(void** state)
{
#define XA 12.0
#define XB 34.0
#define XPLUS +
#define XSTR1 "one"
#define XSTR2 "two"
  const double f0_expected = 0.1234;
  const double f1_expected = 0.9e-3;
  const double delta = 0.1e-4;

  /* Test DT_STRING_UNQUOTED */

  assert_within(DT_STRING_UNQUOTED(
    DT_NODELABEL(test_str_unquoted_f0), val), f0_expected, delta);
  assert_within(DT_STRING_UNQUOTED(
    DT_NODELABEL(test_str_unquoted_f1), val), f1_expected, delta);
  assert_within(DT_STRING_UNQUOTED(
    DT_NODELABEL(test_str_unquoted_t), val), XA XPLUS XB, delta);
  assert_within(DT_STRING_UNQUOTED(
    DT_NODELABEL(test_str_unquoted_esc_t), val), XA XPLUS XB, delta);
  assert_string_equal(DT_STRING_UNQUOTED(
    DT_NODELABEL(test_str_unquoted_esc_s), val), "one plus two");

  /* Test DT_STRING_UNQUOTED_OR */

  assert_within(DT_STRING_UNQUOTED_OR(
    DT_NODELABEL(test_str_unquoted_f0), val, (0.0)), f0_expected, delta);
  assert_within(DT_STRING_UNQUOTED_OR(
    DT_NODELABEL(test_str_unquoted_f1), val, (0.0)), f1_expected, delta);
  assert_within(DT_STRING_UNQUOTED_OR(
    DT_NODELABEL(test_str_unquoted_t), val, (0.0)), XA XPLUS XB, delta);
  assert_within(DT_STRING_UNQUOTED_OR(
    DT_NODELABEL(test_str_unquoted_esc_t), val, (0.0)), XA XPLUS XB, delta);
  assert_string_equal(DT_STRING_UNQUOTED_OR(
    DT_NODELABEL(test_str_unquoted_esc_s), val, "nak"), "one plus two");
  assert_within(DT_STRING_UNQUOTED_OR(
    DT_NODELABEL(test_str_unquoted_f0), nak, (0.0)), 0.0, delta);
  assert_within(DT_STRING_UNQUOTED_OR(
    DT_NODELABEL(test_str_unquoted_f1), nak, (0.0)), 0.0, delta);
  assert_within(DT_STRING_UNQUOTED_OR(
    DT_NODELABEL(test_str_unquoted_t), nak, (0.0)), 0.0, delta);
  assert_within(DT_STRING_UNQUOTED_OR(
    DT_NODELABEL(test_str_unquoted_esc_t), nak, (0.0)), 0.0, delta);
  assert_string_equal(DT_STRING_UNQUOTED_OR(
    DT_NODELABEL(test_str_unquoted_esc_s), nak, "nak"), "nak");

  /* Test DT_INST_STRING_UNQUOTED */

#define STRING_UNQUOTED_VAR(node_id) _CONCAT(var_, node_id)
#define STRING_UNQUOTED_TEST_INST_EXPANSION(inst) \
  double STRING_UNQUOTED_VAR(DT_DRV_INST(inst)) = DT_INST_STRING_UNQUOTED(inst, val);
  DT_INST_FOREACH_STATUS_OKAY(STRING_UNQUOTED_TEST_INST_EXPANSION);

  assert_within(STRING_UNQUOTED_VAR(
    DT_NODELABEL(test_str_unquoted_f0)), f0_expected, delta);
  assert_within(STRING_UNQUOTED_VAR(
    DT_NODELABEL(test_str_unquoted_f1)), f1_expected, delta);
  assert_within(STRING_UNQUOTED_VAR(
    DT_NODELABEL(test_str_unquoted_t)), XA XPLUS XB, delta);
  assert_within(STRING_UNQUOTED_VAR(
    DT_NODELABEL(test_str_unquoted_esc_t)), XA XPLUS XB, delta);

  /* Test DT_INST_STRING_UNQUOTED_OR */

#define STRING_UNQUOTED_OR_VAR(node_id) _CONCAT(var_or_, node_id)
#define STRING_UNQUOTED_OR_TEST_INST_EXPANSION(inst)    \
  double STRING_UNQUOTED_OR_VAR(DT_DRV_INST(inst))[2] = { \
    DT_INST_STRING_UNQUOTED_OR(inst, val, (1.0e10)),  \
    DT_INST_STRING_UNQUOTED_OR(inst, dummy, (1.0e10))   \
  };
  DT_INST_FOREACH_STATUS_OKAY(STRING_UNQUOTED_OR_TEST_INST_EXPANSION);

  assert_within(STRING_UNQUOTED_OR_VAR(
    DT_NODELABEL(test_str_unquoted_f0))[0], f0_expected, delta);
  assert_within(STRING_UNQUOTED_OR_VAR(
    DT_NODELABEL(test_str_unquoted_f1))[0], f1_expected, delta);
  assert_within(STRING_UNQUOTED_OR_VAR(
    DT_NODELABEL(test_str_unquoted_t))[0], XA XPLUS XB, delta);
  assert_within(STRING_UNQUOTED_OR_VAR(
    DT_NODELABEL(test_str_unquoted_esc_t))[0], XA XPLUS XB, delta);
  assert_within(STRING_UNQUOTED_OR_VAR(
    DT_NODELABEL(test_str_unquoted_f0))[1], 1.0e10, delta);
  assert_within(STRING_UNQUOTED_OR_VAR(
    DT_NODELABEL(test_str_unquoted_f1))[1], 1.0e10, delta);
  assert_within(STRING_UNQUOTED_OR_VAR(
    DT_NODELABEL(test_str_unquoted_t))[1], 1.0e10, delta);
  assert_within(STRING_UNQUOTED_OR_VAR(
    DT_NODELABEL(test_str_unquoted_esc_t))[1], 1.0e10, delta);
#undef XA
#undef XB
#undef XPLUS
#undef XSTR1
#undef XSTR2
}

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_string_array_unquoted
static void test_string_idx_unquoted(void** state)
{
#define XA 12.0
#define XB 34.0
#define XC 56.0
#define XD 78.0
#define XPLUS +
#define XMINUS -
#define XSTR1 "one"
#define XSTR2 "two"
  const double delta = 0.1e-4;

  /* DT_STRING_UNQUOTED_BY_IDX */

  assert_within(DT_STRING_UNQUOTED_BY_IDX(
    DT_NODELABEL(test_stra_unquoted_f0), val, 0), 1.0e2, delta);
  assert_within(DT_STRING_UNQUOTED_BY_IDX(
    DT_NODELABEL(test_stra_unquoted_f0), val, 1), 2.0e2, delta);
  assert_within(DT_STRING_UNQUOTED_BY_IDX(
    DT_NODELABEL(test_stra_unquoted_f0), val, 2), 3.0e2, delta);
  assert_within(DT_STRING_UNQUOTED_BY_IDX(
    DT_NODELABEL(test_stra_unquoted_f0), val, 3), 4.0e2, delta);

  assert_within(DT_STRING_UNQUOTED_BY_IDX(
    DT_NODELABEL(test_stra_unquoted_f1), val, 0), 0.01, delta);
  assert_within(DT_STRING_UNQUOTED_BY_IDX(
    DT_NODELABEL(test_stra_unquoted_f1), val, 1), 0.1, delta);
  assert_within(DT_STRING_UNQUOTED_BY_IDX(
    DT_NODELABEL(test_stra_unquoted_f1), val, 2), 1.0, delta);
  assert_within(DT_STRING_UNQUOTED_BY_IDX(
    DT_NODELABEL(test_stra_unquoted_f1), val, 3), 10.0, delta);

  assert_within(DT_STRING_UNQUOTED_BY_IDX(
    DT_NODELABEL(test_stra_unquoted_t), val, 0), XA XPLUS XB, delta);
  assert_within(DT_STRING_UNQUOTED_BY_IDX(
    DT_NODELABEL(test_stra_unquoted_t), val, 1), XC XPLUS XD, delta);
  assert_within(DT_STRING_UNQUOTED_BY_IDX(
    DT_NODELABEL(test_stra_unquoted_t), val, 2), XA XMINUS XB, delta);
  assert_within(DT_STRING_UNQUOTED_BY_IDX(
    DT_NODELABEL(test_stra_unquoted_t), val, 3), XC XMINUS XD, delta);

  assert_within(DT_STRING_UNQUOTED_BY_IDX(
    DT_NODELABEL(test_stra_unquoted_esc), val, 0), XA XPLUS XB, delta);
  assert_string_equal(DT_STRING_UNQUOTED_BY_IDX(
    DT_NODELABEL(test_stra_unquoted_esc), val, 1), "one plus two");

#define STRING_UNQUOTED_BY_IDX_VAR(node_id) _CONCAT(var_, node_id)
#define STRING_UNQUOTED_BY_IDX_TEST_INST_EXPANSION(inst)     \
  double STRING_UNQUOTED_BY_IDX_VAR(DT_DRV_INST(inst))[] = { \
    DT_INST_STRING_UNQUOTED_BY_IDX(inst, val, 0),      \
    DT_INST_STRING_UNQUOTED_BY_IDX(inst, val, 1),      \
    DT_INST_STRING_UNQUOTED_BY_IDX(inst, val, 2),      \
    DT_INST_STRING_UNQUOTED_BY_IDX(inst, val, 3),      \
  };
  DT_INST_FOREACH_STATUS_OKAY(STRING_UNQUOTED_BY_IDX_TEST_INST_EXPANSION);

  assert_within(STRING_UNQUOTED_BY_IDX_VAR(
    DT_NODELABEL(test_stra_unquoted_f0))[0], 1.0e2, delta);
  assert_within(STRING_UNQUOTED_BY_IDX_VAR(
    DT_NODELABEL(test_stra_unquoted_f0))[1], 2.0e2, delta);
  assert_within(STRING_UNQUOTED_BY_IDX_VAR(
    DT_NODELABEL(test_stra_unquoted_f0))[2], 3.0e2, delta);
  assert_within(STRING_UNQUOTED_BY_IDX_VAR(
    DT_NODELABEL(test_stra_unquoted_f0))[3], 4.0e2, delta);

  assert_within(STRING_UNQUOTED_BY_IDX_VAR(
    DT_NODELABEL(test_stra_unquoted_f1))[0], 0.01, delta);
  assert_within(STRING_UNQUOTED_BY_IDX_VAR(
    DT_NODELABEL(test_stra_unquoted_f1))[1], 0.1, delta);
  assert_within(STRING_UNQUOTED_BY_IDX_VAR(
    DT_NODELABEL(test_stra_unquoted_f1))[2], 1.0, delta);
  assert_within(STRING_UNQUOTED_BY_IDX_VAR(
    DT_NODELABEL(test_stra_unquoted_f1))[3], 10.0, delta);

  assert_within(STRING_UNQUOTED_BY_IDX_VAR(
    DT_NODELABEL(test_stra_unquoted_t))[0], XA XPLUS XB, delta);
  assert_within(STRING_UNQUOTED_BY_IDX_VAR(
    DT_NODELABEL(test_stra_unquoted_t))[1], XC XPLUS XD, delta);
  assert_within(STRING_UNQUOTED_BY_IDX_VAR(
    DT_NODELABEL(test_stra_unquoted_t))[2], XA XMINUS XB, delta);
  assert_within(STRING_UNQUOTED_BY_IDX_VAR(
    DT_NODELABEL(test_stra_unquoted_t))[3], XC XMINUS XD, delta);
#undef XA
#undef XB
#undef XC
#undef XD
#undef XPLUS
#undef XMINUS
#undef XSTR1
#undef XSTR2
}

#undef DT_DRV_COMPAT
static void test_string_escape(void** state)
{
  assert_string_equal(DT_PROP(DT_NODELABEL(test_str_escape_0), val),
                      "\a\b\f\n\r\t\v");
  assert_string_equal(DT_PROP(DT_NODELABEL(test_str_escape_1), val),
                      "\'single\' \"double\"");
  assert_string_equal(DT_PROP(DT_NODELABEL(test_str_escape_2), val),
                      "first\nsecond");
  assert_string_equal(DT_PROP(DT_NODELABEL(test_str_escape_3), val),
                      "\x01\x7F");
}

static void test_string_array_escape(void** state)
{
  assert_string_equal(DT_PROP_BY_IDX(DT_NODELABEL(test_stra_escape), val, 0),
                      "\a\b\f\n\r\t\v");
  assert_string_equal(DT_PROP_BY_IDX(DT_NODELABEL(test_stra_escape), val, 1),
                      "\'single\' \"double\"");
  assert_string_equal(DT_PROP_BY_IDX(DT_NODELABEL(test_stra_escape), val, 2),
                      "first\nsecond");
  assert_string_equal(DT_PROP_BY_IDX(DT_NODELABEL(test_stra_escape), val, 3),
                      "\x01\x7F");
}

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT vnd_interrupt_holder_extended
static void test_interrupt_controller(void** state)
{
  /* DT_IRQ_INTC_BY_IDX */

  assert_true(DT_SAME_NODE(DT_IRQ_INTC_BY_IDX(TEST_IRQ_EXT, 0),
                                              TEST_INTC));
  assert_true(DT_SAME_NODE(DT_IRQ_INTC_BY_IDX(TEST_IRQ_EXT, 1),
                                              TEST_GPIO_4));

  /* DT_IRQ_INTC_BY_NAME */

  assert_true(DT_SAME_NODE(DT_IRQ_INTC_BY_NAME(TEST_IRQ_EXT, int1),
                                               TEST_INTC));
  assert_true(DT_SAME_NODE(DT_IRQ_INTC_BY_NAME(TEST_IRQ_EXT, int2),
                                               TEST_GPIO_4));

  /* DT_IRQ_INTC */

  assert_true(DT_SAME_NODE(DT_IRQ_INTC(TEST_IRQ_EXT), TEST_INTC));

  /* DT_INST_IRQ_INTC_BY_IDX */

  assert_true(DT_SAME_NODE(DT_INST_IRQ_INTC_BY_IDX(0, 0), TEST_INTC));
  assert_true(DT_SAME_NODE(DT_INST_IRQ_INTC_BY_IDX(0, 1), TEST_GPIO_4));

  /* DT_INST_IRQ_INTC_BY_NAME */

  assert_true(DT_SAME_NODE(DT_INST_IRQ_INTC_BY_NAME(0, int1), TEST_INTC));
  assert_true(DT_SAME_NODE(DT_INST_IRQ_INTC_BY_NAME(0, int2),
               TEST_GPIO_4));

  /* DT_INST_IRQ_INTC */

  assert_true(DT_SAME_NODE(DT_INST_IRQ_INTC(0), TEST_INTC));
}

#ifdef CONFIG_TESTING_DEVICETREE_DYNAMIC_API

#define USER_ARRAY_MAX_SIZE 5
uint32_t g_user_array[USER_ARRAY_MAX_SIZE];
struct user_config_s
{
  int m_child_enable;
  int m_child_bits;
};

struct user_config_s g_user_config_0[USER_ARRAY_MAX_SIZE];
struct user_config_s g_user_config_1[USER_ARRAY_MAX_SIZE];
struct user_config_s g_user_config_2[USER_ARRAY_MAX_SIZE];
struct user_config_s g_user_config_3[USER_ARRAY_MAX_SIZE];

static void test_dynamic_foreach_usage(void** state)
{
  /* Test foreach macros to dynamically select and assign values to
   * user arrays
   */

#define DT_INIT_USER_ARRAY(node_id) \
  g_user_array[DT_PROP(node_id, device_id)] = DT_PROP(node_id, device_id);

  g_board_id = 1;
  memset(g_user_array, 0, USER_ARRAY_MAX_SIZE * sizeof(uint32_t));
  DT_FOREACH_DYNAMIC(g_board_id, DT_NODELABEL(test_foreach), \
    DT_FOREACH_CHILD, DT_INIT_USER_ARRAY);
  for (int i = 0;
       i < (DT_CHILD_NUM_DYNAMIC(g_board_id, DT_NODELABEL(test_foreach)));
       i++)
    {
      assert_int_equal(g_user_array[i], i);
    }

  g_board_id = 2;
  memset(g_user_array, 0, USER_ARRAY_MAX_SIZE * sizeof(uint32_t));
  DT_FOREACH_DYNAMIC(g_board_id, DT_NODELABEL(test_foreach), \
    DT_FOREACH_CHILD, DT_INIT_USER_ARRAY);
  for (int i = 0;
       i < (DT_CHILD_NUM_DYNAMIC(g_board_id, DT_NODELABEL(test_foreach)));
       i++)
    {
      assert_int_equal(g_user_array[i], i);
    }

#undef DT_INIT_USER_ARRAY

  /* Test foreach macros with vargs to dynamically select and
   * assign values to user arrays
   */

#define DT_INIT_USER_ARRAY(node_id, prop) \
  g_user_array[DT_PROP(node_id, device_id)] = DT_PHA_BY_IDX(node_id, prop, 0, flags);

  g_board_id = 1;
  memset(g_user_array, 0, USER_ARRAY_MAX_SIZE * sizeof(uint32_t));
  DT_FOREACH_VARGS_DYNAMIC(g_board_id, DT_NODELABEL(test_foreach), \
    DT_FOREACH_CHILD_VARGS, DT_INIT_USER_ARRAY, gpios);
  assert_int_equal(g_user_array[0], 32);
  assert_int_equal(g_user_array[1], 56);
  assert_int_equal(g_user_array[2], 35);
  assert_int_equal(g_user_array[3], 37);
  assert_int_equal(g_user_array[4], 0);

  g_board_id = 2;
  memset(g_user_array, 0, USER_ARRAY_MAX_SIZE * sizeof(uint32_t));
  DT_FOREACH_VARGS_DYNAMIC(g_board_id, DT_NODELABEL(test_foreach), \
    DT_FOREACH_CHILD_VARGS, DT_INIT_USER_ARRAY, gpios);
  assert_int_equal(g_user_array[0], 34);
  assert_int_equal(g_user_array[1], 66);
  assert_int_equal(g_user_array[2], 75);
  assert_int_equal(g_user_array[3], 0);
  assert_int_equal(g_user_array[4], 0);

#undef DT_INIT_USER_ARRAY

/* Test nested foreach macros to dynamically select and assign
 * values to user configuration arrays
 */

#define DT_INIT_DEVICE_CHANNEL(node_id, var) \
  { \
    DT_CAT(g_user_config_, var)[DT_PROP(node_id, child_id)].m_child_enable = DT_PROP(node_id, m_child_enable); \
    DT_CAT(g_user_config_, var)[DT_PROP(node_id, child_id)].m_child_bits = DT_PROP(node_id, m_child_bits); \
  } \

#define DT_INIT_DEVICE(node_id) \
  DT_FOREACH_CHILD_VARGS(node_id, DT_INIT_DEVICE_CHANNEL, DT_PROP(node_id, device_id))

  g_board_id = 1;
  DT_FOREACH_DYNAMIC(g_board_id, DT_NODELABEL(test_foreach), \
    DT_FOREACH_CHILD_STATUS_OKAY, DT_INIT_DEVICE);
  assert_int_equal(g_user_config_0[0].m_child_enable, 1);
  assert_int_equal(g_user_config_0[0].m_child_bits, 1);
  assert_int_equal(g_user_config_0[1].m_child_enable, 0);
  assert_int_equal(g_user_config_0[1].m_child_bits, 2);
  assert_int_equal(g_user_config_1[0].m_child_enable, 1);
  assert_int_equal(g_user_config_1[0].m_child_bits, 4);
  assert_int_equal(g_user_config_1[1].m_child_enable, 1);
  assert_int_equal(g_user_config_1[1].m_child_bits, 8);
  assert_int_equal(g_user_config_2[0].m_child_enable, 0);
  assert_int_equal(g_user_config_2[0].m_child_bits, 16);
  assert_int_equal(g_user_config_2[1].m_child_enable, 0);
  assert_int_equal(g_user_config_2[1].m_child_bits, 32);
  assert_int_equal(g_user_config_3[0].m_child_enable, 0);
  assert_int_equal(g_user_config_3[0].m_child_bits, 64);
  assert_int_equal(g_user_config_3[1].m_child_enable, 1);
  assert_int_equal(g_user_config_3[1].m_child_bits, 128);

  g_board_id = 2;
  DT_FOREACH_DYNAMIC(g_board_id, DT_NODELABEL(test_foreach), \
    DT_FOREACH_CHILD_STATUS_OKAY, DT_INIT_DEVICE);
  assert_int_equal(g_user_config_0[0].m_child_enable, 0);
  assert_int_equal(g_user_config_0[0].m_child_bits, 3);
  assert_int_equal(g_user_config_0[1].m_child_enable, 1);
  assert_int_equal(g_user_config_0[1].m_child_bits, 5);
  assert_int_equal(g_user_config_1[0].m_child_enable, 0);
  assert_int_equal(g_user_config_1[0].m_child_bits, 15);

#undef DT_INIT_DEVICE
#undef DT_INIT_DEVICE_CHANNEL

  /* Test foreach okays macros with compats to dynamically
   * select and foreach nodes with compatibles
   */

#define FOREACH_NODE_PATH_MAX 50
#define FOREACH_NODE_NUM_MAX  8

  char foreach_compats_okay_nodes_path \
  [FOREACH_NODE_NUM_MAX][FOREACH_NODE_PATH_MAX] =
  {
    "/test/test-foreach-1/device_1/child_1_1",
    "/test/test-foreach-1/device_1/child_1_2",
    "/test/test-foreach-1/device_2/child_2_1",
    "/test/test-foreach-1/device_2/child_2_2",
    "/test/test-foreach-1/device_3/child_3_1",
    "/test/test-foreach-1/device_3/child_3_2",
    "/test/test-foreach-1/device_4/child_4_1",
    "/test/test-foreach-1/device_4/child_4_2"
  };

#define DT_GET_DEVICE_PATH(node_id) \
  assert_string_equal(DT_NODE_PATH(node_id), \
    foreach_compats_okay_nodes_path[DT_PROP(node_id, m_child_idx) - 1]);

  g_board_id = 1;
  DT_FOREACH_STATUS_OKAY_DYNAMIC(g_board_id, \
    vnd_foreach_child_test, DT_GET_DEVICE_PATH);

  g_board_id = 2;
  DT_FOREACH_STATUS_OKAY_DYNAMIC(g_board_id, \
    vnd_foreach_child_test, DT_GET_DEVICE_PATH);

#undef DT_GET_DEVICE_PATH

#define DT_GET_DEVICE_PATH(node_id, off) \
  assert_string_equal(DT_NODE_PATH(node_id), \
    foreach_compats_okay_nodes_path[DT_PROP(node_id, m_child_idx) - off]);

  g_board_id = 1;
  DT_FOREACH_STATUS_OKAY_VARGS_DYNAMIC(g_board_id, \
    vnd_foreach_child_test, DT_GET_DEVICE_PATH, 1);

  g_board_id = 2;
  DT_FOREACH_STATUS_OKAY_VARGS_DYNAMIC(g_board_id, \
    vnd_foreach_child_test, DT_GET_DEVICE_PATH, 1);

#undef DT_GET_DEVICE_PATH
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  const struct CMUnitTest tests[] = {
  cmocka_unit_test(test_path_props),
  cmocka_unit_test(test_alias_props),
  cmocka_unit_test(test_nodelabel_props),
  cmocka_unit_test(test_inst_props),
  cmocka_unit_test(test_any_inst_prop),
  cmocka_unit_test(test_any_compat_inst_prop),
  cmocka_unit_test(test_any_inst_bool),
  cmocka_unit_test(test_default_prop_access),
  cmocka_unit_test(test_has_path),
  cmocka_unit_test(test_has_alias),
  cmocka_unit_test(test_node_hashes),
  cmocka_unit_test(test_inst_checks),
  cmocka_unit_test(test_has_nodelabel),
  cmocka_unit_test(test_has_compat),
  cmocka_unit_test(test_has_status),
  cmocka_unit_test(test_has_status_okay),
  cmocka_unit_test(test_bus),
  cmocka_unit_test(test_reg),
  cmocka_unit_test(test_reg_64),
  cmocka_unit_test(test_irq),
  cmocka_unit_test(test_irq_level),
  cmocka_unit_test(test_phandles),
  cmocka_unit_test(test_macro_names),
  cmocka_unit_test(test_arrays),
  cmocka_unit_test(test_foreach),
  cmocka_unit_test(test_foreach_status_okay),
  cmocka_unit_test(test_foreach_nodelabel),
  cmocka_unit_test(test_foreach_prop_elem),
  cmocka_unit_test(test_foreach_prop_elem_varg),
  cmocka_unit_test(test_chosen),
  cmocka_unit_test(test_enums),
  cmocka_unit_test(test_enums_required_false),
  cmocka_unit_test(test_inst_enums),
  cmocka_unit_test(test_parent),
  cmocka_unit_test(test_parent_nodes_list),
  cmocka_unit_test(test_gparent),
  cmocka_unit_test(test_children),
  cmocka_unit_test(test_child_nodes_list),
  cmocka_unit_test(test_child_nodes_list_varg),
  cmocka_unit_test(test_child_nodes_number),
  cmocka_unit_test(test_great_grandchild),
  cmocka_unit_test(test_ranges_pcie),
  cmocka_unit_test(test_ranges_other),
  cmocka_unit_test(test_ranges_empty),
  cmocka_unit_test(test_compat_get_any_status_okay),
  cmocka_unit_test(test_path),
  cmocka_unit_test(test_node_name),
  cmocka_unit_test(test_node_child_idx),
  cmocka_unit_test(test_same_node),
  cmocka_unit_test(test_string_token),
  cmocka_unit_test(test_string_idx_token),
  cmocka_unit_test(test_string_unquoted),
  cmocka_unit_test(test_string_idx_unquoted),
  cmocka_unit_test(test_string_escape),
  cmocka_unit_test(test_string_array_escape),
  cmocka_unit_test(test_interrupt_controller),
#ifdef CONFIG_TESTING_DEVICETREE_DYNAMIC_API
  cmocka_unit_test(test_dynamic_foreach_usage),
#endif
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
