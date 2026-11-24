// T012: tests voor de LD2410-commando's (contracts/ld2410-uart-protocol.md).
#include <unity.h>

#include <cstdint>

#include "ld2410_commands.h"

using namespace ld2410;

void setUp() {}
void tearDown() {}

void test_enable_configuration_bytes() {
  uint8_t buf[MAX_COMMAND_FRAME];
  const uint8_t expected[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFF, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
  size_t n = buildEnableConfiguration(buf, sizeof(buf));
  TEST_ASSERT_EQUAL(sizeof(expected), n);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, n);
}

void test_enable_engineering_mode_bytes() {
  uint8_t buf[MAX_COMMAND_FRAME];
  const uint8_t expected[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0x62, 0x00, 0x04, 0x03, 0x02, 0x01};
  size_t n = buildEnableEngineeringMode(buf, sizeof(buf));
  TEST_ASSERT_EQUAL(sizeof(expected), n);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, n);
}

void test_end_configuration_bytes() {
  uint8_t buf[MAX_COMMAND_FRAME];
  const uint8_t expected[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xFE, 0x00, 0x04, 0x03, 0x02, 0x01};
  size_t n = buildEndConfiguration(buf, sizeof(buf));
  TEST_ASSERT_EQUAL(sizeof(expected), n);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, n);
}

void test_buffer_too_small_returns_zero() {
  uint8_t buf[8];
  TEST_ASSERT_EQUAL(0, buildEnableConfiguration(buf, sizeof(buf)));
}

void test_ack_success_and_failure() {
  AckFrame ack;
  ack.command = 0x0162;
  ack.status = 0;
  TEST_ASSERT_TRUE(isAckFor(ack, CMD_ENABLE_ENGINEERING_MODE));
  TEST_ASSERT_TRUE(isAckSuccess(ack, CMD_ENABLE_ENGINEERING_MODE));
  TEST_ASSERT_FALSE(isAckFor(ack, CMD_END_CONFIGURATION));

  ack.status = 1;
  TEST_ASSERT_TRUE(isAckFor(ack, CMD_ENABLE_ENGINEERING_MODE));
  TEST_ASSERT_FALSE(isAckSuccess(ack, CMD_ENABLE_ENGINEERING_MODE));
}

void test_ack_without_flag_is_not_an_ack() {
  AckFrame ack;
  ack.command = CMD_ENABLE_CONFIGURATION;  // zonder | 0x0100
  ack.status = 0;
  TEST_ASSERT_FALSE(isAckFor(ack, CMD_ENABLE_CONFIGURATION));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_enable_configuration_bytes);
  RUN_TEST(test_enable_engineering_mode_bytes);
  RUN_TEST(test_end_configuration_bytes);
  RUN_TEST(test_buffer_too_small_returns_zero);
  RUN_TEST(test_ack_success_and_failure);
  RUN_TEST(test_ack_without_flag_is_not_an_ack);
  return UNITY_END();
}
