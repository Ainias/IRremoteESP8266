// Copyright 2017 David Conran
// Manchester

#include <algorithm>
#include "IRrecv.h"
#include "IRsend.h"
#include "IRutils.h"

// Constants
const uint16_t kManchesterMinSamples = 13;
const uint16_t kManchesterTick = 333;
const uint32_t kManchesterMinGap = kDefaultMessageGap;  // Just a guess.
const uint8_t kManchesterTolerance = 0;     // Percentage error margin.
const uint16_t kManchesterExcess = 0;       // See kMarkExcess.
const uint16_t kManchesterDelta = 150;  // Use instead of Excess and Tolerance.
const int16_t kSpace = 1;
const int16_t kMark = 0;
const uint8_t frequency = 38;
const uint8_t dutyCycle = 25;

#if SEND_MANCHESTER
// Send a Manchester packet.
// This protocol is pretty much just raw Manchester encoding.
//
// Args:
//   data:    The message you wish to send.
//   nbits:   Bit size of the protocol you want to send.
//   repeat:  Nr. of extra times the data will be sent.
//
// Status: STABLE / Working.
//
void IRsend::sendManchester(uint64_t data, uint16_t nbits, uint16_t repeat) {
    if (nbits > sizeof(data) * 8) return;  // We can't send something that big.

    // Set 36kHz IR carrier frequency & a 1/4 (25%) duty cycle.
    // NOTE: duty cycle is not confirmed. Just guessing based on RC5/6 protocols.
    enableIROut(frequency, dutyCycle);

    for (uint16_t i = 0; i <= repeat; i++) {
        // Data
        for (uint64_t mask = 1ULL << (nbits - 1); mask; mask >>= 1)
            if (data & mask) {       // 1
                space(kManchesterTick);  // 1 is space, then mark.
                mark(kManchesterTick);
            } else {                // 0
                mark(kManchesterTick);  // 0 is mark, then space.
                space(kManchesterTick);
            }
        // Footer
        space(kManchesterMinGap);
    }
}
#endif  // SEND_MANCHESTER

#if DECODE_MANCHESTER
// Decode the supplied Manchester message.
// This protocol is pretty much just raw Manchester encoding.
//
// Args:
//   results: Ptr to the data to decode and where to store the decode result.
//   offset:  The starting index to use when attempting to decode the raw data.
//            Typically/Defaults to kStartOffset.
//   nbits:   The number of data bits to expect.
//   strict:  Flag indicating if we should perform strict matching.
// Returns:
//   boolean: True if it can decode it, false if it can't.
//
// Status: BETA / Appears to be working 90% of the time.
//
// Ref:
//   http://www.sbprojects.com/knowledge/ir/rc5.php
//   https://en.wikipedia.org/wiki/RC-5
//   https://en.wikipedia.org/wiki/Manchester_code
bool IRrecv::decodeManchester(decode_results *results, uint16_t offset,
                            const uint16_t nbits, const bool strict) {
    if (results->rawlen <= kManchesterMinSamples + offset) return false;

    // Compliance
    if (strict && nbits != kManchesterBits) return false;
    uint16_t used = 0;
    uint64_t data = 0;
    uint16_t actual_bits = 0;

    // No Header

    // Data
    for (; offset <= results->rawlen; actual_bits++) {
        int16_t levelA =
                getRClevel(results, &offset, &used, kManchesterTick, kManchesterTolerance,
                           kManchesterExcess, kManchesterDelta);
        int16_t levelB =
                getRClevel(results, &offset, &used, kManchesterTick, kManchesterTolerance,
                           kManchesterExcess, kManchesterDelta);
        if (levelA == kSpace && levelB == kMark) {
            data = (data << 1) | 1;  // 1
        } else {
            if (levelA == kMark && levelB == kSpace) {
                data <<= 1;  // 0
            } else {
                break;
            }
        }
    }
    // Footer (None)

    // Compliance
    if (actual_bits < nbits) return false;  // Less data than we expected.
    if (strict && actual_bits != kManchesterBits) return false;

    // Success
    results->decode_type = MANCHESTER;
    results->value = data;
    results->address = data & 0xF;  // Unit
    results->command = data >> 4;   // Team
    results->repeat = false;
    results->bits = actual_bits;
    return true;
}
#endif  // DECODE_MANCHESTER
