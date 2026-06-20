/** \file spo2_algorithm.h
 * Heart rate / SpO2 calculation algorithm (Maxim Integrated / SparkFun).
 * See spo2_algorithm.cpp for license.
 */

#ifndef SPO2_ALGORITHM_H_
#define SPO2_ALGORITHM_H_

#include <cstdint>

#define FreqS 25
#define BUFFER_SIZE (FreqS * 4)
#define MA4_SIZE 4
/** Minimum samples between PPG peaks at 25 Hz (~11 => ~136 bpm max; reduces dicrotic double-count). */
#define HR_MIN_PEAK_DISTANCE 11

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
void maxim_heart_rate_and_oxygen_saturation(
    uint16_t* pun_ir_buffer,
    int32_t n_ir_buffer_length,
    uint16_t* pun_red_buffer,
    int32_t* pn_spo2,
    int8_t* pch_spo2_valid,
    int32_t* pn_heart_rate,
    int8_t* pch_hr_valid
);
#else
void maxim_heart_rate_and_oxygen_saturation(
    uint32_t* pun_ir_buffer,
    int32_t n_ir_buffer_length,
    uint32_t* pun_red_buffer,
    int32_t* pn_spo2,
    int8_t* pch_spo2_valid,
    int32_t* pn_heart_rate,
    int8_t* pch_hr_valid
);
#endif

#endif
