/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2025 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// SIMD functions for the NNUE evaluation function

#ifndef NNUE_LAYERS_SIMD_H_INCLUDED
#define NNUE_LAYERS_SIMD_H_INCLUDED

#include <cstdint>

#include "../nnue_common.h"

#if defined(USE_AVX512)
#include <immintrin.h>
#elif defined(USE_AVX2)
#include <immintrin.h>
#elif defined(USE_SSE41)
#include <smmintrin.h>
#elif defined(USE_SSSE3)
#include <tmmintrin.h>
#elif defined(USE_SSE2)
#include <emmintrin.h>
#elif defined(USE_MMX)
#include <mmintrin.h>
#elif defined(USE_NEON)
#include <arm_neon.h>
#endif

namespace Stockfish::Eval::NNUE::SIMD {

#if defined(USE_AVX512)

using vec_t      = __m512i;
using psqt_vec_t = __m256i;

constexpr IndexType MaxChunkSize = 64;

#define vec_zero _mm512_setzero_si512
#define vec_set_32 _mm512_set1_epi32
#define vec_set_16 _mm512_set1_epi16
inline vec_t vec_max_16(vec_t a, vec_t b) { return _mm512_max_epi16(a, b); }
inline vec_t vec_min_16(vec_t a, vec_t b) { return _mm512_min_epi16(a, b); }
inline vec_t vec_slli_16(vec_t a, int b) { return _mm512_slli_epi16(a, b); }
inline vec_t vec_mulhi_16(vec_t a, vec_t b) { return _mm512_mulhi_epi16(a, b); }
inline vec_t vec_packus_16(vec_t a, vec_t b) { return _mm512_packus_epi16(a, b); }

#elif defined(USE_AVX2)

using vec_t      = __m256i;
using psqt_vec_t = __m256i;

constexpr IndexType MaxChunkSize = 32;

#define vec_zero _mm256_setzero_si256
#define vec_set_32 _mm256_set1_epi32
#define vec_set_16 _mm256_set1_epi16
inline vec_t vec_max_16(vec_t a, vec_t b) { return _mm256_max_epi16(a, b); }
inline vec_t vec_min_16(vec_t a, vec_t b) { return _mm256_min_epi16(a, b); }
inline vec_t vec_slli_16(vec_t a, int b) { return _mm256_slli_epi16(a, b); }
inline vec_t vec_mulhi_16(vec_t a, vec_t b) { return _mm256_mulhi_epi16(a, b); }
inline vec_t vec_packus_16(vec_t a, vec_t b) { return _mm256_packus_epi16(a, b); }

#elif defined(USE_SSE2)

using vec_t      = __m128i;
using psqt_vec_t = __m128i;

constexpr IndexType MaxChunkSize = 16;

#define vec_zero _mm_setzero_si128
#define vec_set_32 _mm_set1_epi32
#define vec_set_16 _mm_set1_epi16
inline vec_t vec_max_16(vec_t a, vec_t b) { return _mm_max_epi16(a, b); }
inline vec_t vec_min_16(vec_t a, vec_t b) { return _mm_min_epi16(a, b); }
inline vec_t vec_slli_16(vec_t a, int b) { return _mm_slli_epi16(a, b); }
inline vec_t vec_mulhi_16(vec_t a, vec_t b) { return _mm_mulhi_epi16(a, b); }
inline vec_t vec_packus_16(vec_t a, vec_t b) { return _mm_packus_epi16(a, b); }

#elif defined(USE_NEON)

using vec_t      = int16x8_t;
using psqt_vec_t = int32x4_t;

constexpr IndexType MaxChunkSize = 16;

inline vec_t vec_zero() { return veorq_s16(vdupq_n_s16(0), vdupq_n_s16(0)); }
inline vec_t vec_set_16(int16_t a) { return vdupq_n_s16(a); }
inline vec_t vec_max_16(vec_t a, vec_t b) { return vmaxq_s16(a, b); }
inline vec_t vec_min_16(vec_t a, vec_t b) { return vminq_s16(a, b); }
inline vec_t vec_slli_16(vec_t a, int b) { return vshlq_n_s16(a, b); }
inline vec_t vec_mulhi_16(vec_t a, vec_t b) {
    return vreinterpretq_s16_s32(vshrq_n_s32(vmull_s16(a, b), 16));
}
inline vec_t vec_packus_16(vec_t a, vec_t b) {
    return vcombine_u8(vqmovun_s16(b), vqmovun_s16(a));
}

#else

using vec_t      = void;
using psqt_vec_t = void;

constexpr IndexType MaxChunkSize = 16;

#endif

}  // namespace Stockfish::Eval::NNUE::SIMD

#endif  // #ifndef NNUE_LAYERS_SIMD_H_INCLUDED