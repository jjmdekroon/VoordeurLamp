// T011: tests voor de LD2410-frameparser (contracts/ld2410-uart-protocol.md).
#include <unity.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ld2410_frame_parser.h"

using ld2410::FrameParser;
using ld2410::FrameType;
using ld2410::TargetState;

namespace {

// Testvector "basis" uit de user manual §2.3.2.
const std::vector<uint8_t> BASIC_FRAME = {0xF4, 0xF3, 0xF2, 0xF1, 0x0D, 0x00, 0x02, 0xAA,
                                          0x02, 0x51, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x00,
                                          0x00, 0x55, 0x00, 0xF8, 0xF7, 0xF6, 0xF5};

// Testvector "engineering" uit de user manual §2.3.2.
const std::vector<uint8_t> ENGINEERING_FRAME = {
    0xF4, 0xF3, 0xF2, 0xF1, 0x23, 0x00, 0x01, 0xAA, 0x03, 0x1E, 0x00, 0x3C, 0x00, 0x00, 0x39,
    0x00, 0x00, 0x08, 0x08, 0x3C, 0x22, 0x05, 0x03, 0x03, 0x04, 0x03, 0x06, 0x05, 0x00, 0x00,
    0x39, 0x10, 0x13, 0x06, 0x06, 0x08, 0x04, 0x03, 0x05, 0x55, 0x00, 0xF8, 0xF7, 0xF6, 0xF5};

// ACK op "enable configuration" (succes).
const std::vector<uint8_t> ENABLE_CONFIG_ACK = {0xFD, 0xFC, 0xFB, 0xFA, 0x08, 0x00, 0xFF, 0x01, 0x00,
                                                0x00, 0x01, 0x00, 0x40, 0x00, 0x04, 0x03, 0x02, 0x01};

struct FeedResult {
  int reports{0};
  int acks{0};
};

FeedResult feedAll(FrameParser &parser, const std::vector<uint8_t> &bytes, uint32_t nowMs = 1000) {
  FeedResult r;
  for (uint8_t b : bytes) {
    switch (parser.feed(b, nowMs)) {
      case FrameParser::Result::Report:
        r.reports++;
        break;
      case FrameParser::Result::Ack:
        r.acks++;
        break;
      default:
        break;
    }
  }
  return r;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_basic_frame_is_decoded() {
  FrameParser parser;
  FeedResult r = feedAll(parser, BASIC_FRAME, 1234);
  TEST_ASSERT_EQUAL(1, r.reports);
  const auto &f = parser.frame();
  TEST_ASSERT_EQUAL(static_cast<int>(FrameType::Basic), static_cast<int>(f.type));
  TEST_ASSERT_EQUAL(static_cast<int>(TargetState::Stationary), static_cast<int>(f.targetState));
  TEST_ASSERT_EQUAL_UINT16(81, f.movingDistanceCm);
  TEST_ASSERT_EQUAL_UINT8(0, f.movingEnergy);
  TEST_ASSERT_EQUAL_UINT16(0, f.stationaryDistanceCm);
  TEST_ASSERT_EQUAL_UINT8(59, f.stationaryEnergy);
  TEST_ASSERT_EQUAL_UINT16(0, f.detectionDistanceCm);
  TEST_ASSERT_EQUAL_UINT32(1234, f.timestampMs);
}

void test_engineering_frame_is_decoded() {
  FrameParser parser;
  FeedResult r = feedAll(parser, ENGINEERING_FRAME);
  TEST_ASSERT_EQUAL(1, r.reports);
  const auto &f = parser.frame();
  TEST_ASSERT_EQUAL(static_cast<int>(FrameType::Engineering), static_cast<int>(f.type));
  TEST_ASSERT_EQUAL(static_cast<int>(TargetState::Both), static_cast<int>(f.targetState));
  TEST_ASSERT_EQUAL_UINT16(30, f.movingDistanceCm);
  TEST_ASSERT_EQUAL_UINT8(60, f.movingEnergy);
  TEST_ASSERT_EQUAL_UINT16(0, f.stationaryDistanceCm);
  TEST_ASSERT_EQUAL_UINT8(57, f.stationaryEnergy);
  TEST_ASSERT_EQUAL_UINT8(8, f.maxMovingGate);
  TEST_ASSERT_EQUAL_UINT8(8, f.maxStationaryGate);
  const uint8_t moving[9] = {0x3C, 0x22, 0x05, 0x03, 0x03, 0x04, 0x03, 0x06, 0x05};
  const uint8_t stationary[9] = {0x00, 0x00, 0x39, 0x10, 0x13, 0x06, 0x06, 0x08, 0x04};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(moving, f.movingGateEnergy, 9);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(stationary, f.stationaryGateEnergy, 9);
}

void test_frame_split_over_multiple_calls() {
  FrameParser parser;
  std::vector<uint8_t> first(ENGINEERING_FRAME.begin(), ENGINEERING_FRAME.begin() + 10);
  std::vector<uint8_t> second(ENGINEERING_FRAME.begin() + 10, ENGINEERING_FRAME.end());
  TEST_ASSERT_EQUAL(0, feedAll(parser, first).reports);
  TEST_ASSERT_EQUAL(1, feedAll(parser, second).reports);
}

void test_noise_before_header_is_skipped_without_error() {
  FrameParser parser;
  std::vector<uint8_t> bytes = {0x00, 0x13, 0xF4, 0xF3, 0x99, 0xFD, 0x42};
  bytes.insert(bytes.end(), BASIC_FRAME.begin(), BASIC_FRAME.end());
  TEST_ASSERT_EQUAL(1, feedAll(parser, bytes).reports);
  TEST_ASSERT_EQUAL_UINT32(0, parser.invalidFrameCount());
}

void test_back_to_back_frames() {
  FrameParser parser;
  std::vector<uint8_t> bytes = ENGINEERING_FRAME;
  bytes.insert(bytes.end(), BASIC_FRAME.begin(), BASIC_FRAME.end());
  bytes.insert(bytes.end(), ENGINEERING_FRAME.begin(), ENGINEERING_FRAME.end());
  TEST_ASSERT_EQUAL(3, feedAll(parser, bytes).reports);
}

void test_bad_footer_is_invalid() {
  FrameParser parser;
  std::vector<uint8_t> bytes = BASIC_FRAME;
  bytes.back() = 0x00;
  TEST_ASSERT_EQUAL(0, feedAll(parser, bytes).reports);
  TEST_ASSERT_EQUAL_UINT32(1, parser.invalidFrameCount());
  // Parser herstelt zich voor het volgende frame.
  TEST_ASSERT_EQUAL(1, feedAll(parser, BASIC_FRAME).reports);
}

void test_bad_length_is_invalid() {
  FrameParser parser;
  std::vector<uint8_t> bytes = BASIC_FRAME;
  bytes[4] = 0xFF;  // lengte 255 > MAX_PAYLOAD
  feedAll(parser, bytes);
  TEST_ASSERT_EQUAL_UINT32(1, parser.invalidFrameCount());
  TEST_ASSERT_EQUAL(1, feedAll(parser, BASIC_FRAME).reports);
}

void test_bad_head_or_tail_is_invalid() {
  FrameParser parser;
  std::vector<uint8_t> badHead = BASIC_FRAME;
  badHead[7] = 0xAB;
  std::vector<uint8_t> badTail = BASIC_FRAME;
  badTail[17] = 0x56;
  TEST_ASSERT_EQUAL(0, feedAll(parser, badHead).reports);
  TEST_ASSERT_EQUAL(0, feedAll(parser, badTail).reports);
  TEST_ASSERT_EQUAL_UINT32(2, parser.invalidFrameCount());
}

void test_invalid_target_state_is_rejected() {
  // data-model.md RadarFrame.targetState: alleen 0x00..0x03.
  FrameParser parser;
  std::vector<uint8_t> bytes = BASIC_FRAME;
  bytes[8] = 0x04;
  TEST_ASSERT_EQUAL(0, feedAll(parser, bytes).reports);
  TEST_ASSERT_EQUAL_UINT32(1, parser.invalidFrameCount());
}

void test_energy_above_100_is_rejected() {
  // data-model.md RadarFrame.movingEnergy / stationaryEnergy: 0..100.
  FrameParser parser;
  std::vector<uint8_t> moving = BASIC_FRAME;
  moving[11] = 101;
  std::vector<uint8_t> stationary = BASIC_FRAME;
  stationary[14] = 200;
  TEST_ASSERT_EQUAL(0, feedAll(parser, moving).reports);
  TEST_ASSERT_EQUAL(0, feedAll(parser, stationary).reports);
  TEST_ASSERT_EQUAL_UINT32(2, parser.invalidFrameCount());
}

void test_max_gate_above_8_is_rejected() {
  // data-model.md RadarFrame.maxMovingGate / maxStationaryGate: <= 8.
  FrameParser parser;
  std::vector<uint8_t> moving = ENGINEERING_FRAME;
  moving[17] = 9;
  std::vector<uint8_t> stationary = ENGINEERING_FRAME;
  stationary[18] = 9;
  TEST_ASSERT_EQUAL(0, feedAll(parser, moving).reports);
  TEST_ASSERT_EQUAL(0, feedAll(parser, stationary).reports);
  TEST_ASSERT_EQUAL_UINT32(2, parser.invalidFrameCount());
}

void test_ack_is_recognised_separately() {
  FrameParser parser;
  std::vector<uint8_t> bytes = ENGINEERING_FRAME;
  bytes.insert(bytes.end(), ENABLE_CONFIG_ACK.begin(), ENABLE_CONFIG_ACK.end());
  FeedResult r = feedAll(parser, bytes);
  TEST_ASSERT_EQUAL(1, r.reports);
  TEST_ASSERT_EQUAL(1, r.acks);
  TEST_ASSERT_EQUAL_HEX16(0x01FF, parser.ack().command);
  TEST_ASSERT_EQUAL_UINT16(0, parser.ack().status);
  TEST_ASSERT_EQUAL_UINT8(4, parser.ack().dataLength);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_basic_frame_is_decoded);
  RUN_TEST(test_engineering_frame_is_decoded);
  RUN_TEST(test_frame_split_over_multiple_calls);
  RUN_TEST(test_noise_before_header_is_skipped_without_error);
  RUN_TEST(test_back_to_back_frames);
  RUN_TEST(test_bad_footer_is_invalid);
  RUN_TEST(test_bad_length_is_invalid);
  RUN_TEST(test_bad_head_or_tail_is_invalid);
  RUN_TEST(test_invalid_target_state_is_rejected);
  RUN_TEST(test_energy_above_100_is_rejected);
  RUN_TEST(test_max_gate_above_8_is_rejected);
  RUN_TEST(test_ack_is_recognised_separately);
  return UNITY_END();
}
