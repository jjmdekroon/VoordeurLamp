#pragma once
// Byte-voor-byte parser voor LD2410-rapport- en ACK-frames uit één bytestroom.
// Geen heap, geen framework-headers (constitutie I, II, III).

#include <cstdint>

#include "ld2410_types.h"

namespace ld2410 {

class FrameParser {
 public:
  enum class Result : uint8_t {
    None,    ///< Nog geen compleet frame.
    Report,  ///< Geldig rapportframe beschikbaar via frame().
    Ack,     ///< Geldig ACK-frame beschikbaar via ack().
  };

  /// Grootste toegestane frame-inhoud (lengteveld) in bytes.
  static constexpr uint16_t MAX_PAYLOAD = 64;

  /// Verwerk één byte. nowMs wordt als tijdstempel in een compleet rapportframe gezet.
  Result feed(uint8_t byte, uint32_t nowMs);

  const RadarFrame &frame() const { return frame_; }
  const AckFrame &ack() const { return ack_; }

  /// Aantal frames dat na een geldige header ongeldig bleek.
  uint32_t invalidFrameCount() const { return invalidFrames_; }

  void reset();

 private:
  enum class State : uint8_t { Header, Length, Payload, Footer };
  enum class Kind : uint8_t { None, Report, Ack };

  void restartWith(uint8_t byte);
  void markInvalid();
  Result finishFrame(uint32_t nowMs);
  bool decodeReport(RadarFrame &out) const;
  bool decodeAck(AckFrame &out) const;

  State state_{State::Header};
  Kind kind_{Kind::None};
  uint8_t pos_{0};
  uint16_t length_{0};
  uint16_t received_{0};
  uint8_t payload_[MAX_PAYLOAD]{};
  RadarFrame frame_{};
  AckFrame ack_{};
  uint32_t invalidFrames_{0};
};

}  // namespace ld2410
