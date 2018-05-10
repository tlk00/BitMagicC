#ifndef BMUTIL__H__INCLUDED__
#define BMUTIL__H__INCLUDED__
/*
Copyright(c) 2002-2017 Anatoliy Kuznetsov(anatoliy_kuznetsov at yahoo.com)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

For more information please visit:  http://bitmagic.io
*/

/*! \file bmutil.h
    \brief Bit manipulation primitives (internal)
*/

#include "bmdef.h"
#include "bmconst.h"

#if defined(_M_AMD64) || defined(_M_X64) 
#include <intrin.h>
#endif

namespace bm
{

        /**
        bit-block array wrapped into union for correct interpretation of
        32-bit vs 64-bit access vs SIMD
        @internal
        */
        struct bit_block_t
        {
            union bunion_t
            {
                bm::word_t BM_VECT_ALIGN w32[bm::set_block_size] BM_VECT_ALIGN_ATTR;
                bm::id64_t BM_VECT_ALIGN w64[bm::set_block_size / 2] BM_VECT_ALIGN_ATTR;
#ifdef BMAVX2OPT
                __m256i  BM_VECT_ALIGN w256[bm::set_block_size / 8] BM_VECT_ALIGN_ATTR;
#endif
#if defined(BMSSE2OPT) || defined(BMSSE42OPT)
                __m128i  BM_VECT_ALIGN w128[bm::set_block_size / 4] BM_VECT_ALIGN_ATTR;
#endif
            } b_;

            operator bm::word_t*() { return &(b_.w32[0]); }
            operator const bm::word_t*() const { return &(b_.w32[0]); }
            explicit operator bm::id64_t*() { return &b_.w64[0]; }
            explicit operator const bm::id64_t*() const { return &b_.w64[0]; }
#ifdef BMAVX2OPT
            explicit operator __m256i*() { return &b_.w256[0]; }
            explicit operator const __m256i*() const { return &b_.w256[0]; }
#endif
#if defined(BMSSE2OPT) || defined(BMSSE42OPT)
            explicit operator __m128i*() { return &b_.w128[0]; }
            explicit operator const __m128i*() const { return &b_.w128[0]; }
#endif

            const bm::word_t* begin() const { return (b_.w32 + 0); }
            bm::word_t* begin() { return (b_.w32 + 0); }
            const bm::word_t* end() const { return (b_.w32 + bm::set_block_size); }
            bm::word_t* end() { return (b_.w32 + bm::set_block_size); }
        };

/**
    Get minimum of 2 values
*/
template<typename T>
T min_value(T v1, T v2)
{
    return v1 < v2 ? v1 : v2;
}


/**
    Fast loop-less function to find LOG2
*/
template<typename T>
T ilog2(T x)
{
    unsigned int l = 0;
    
    if (x >= 1<<16) { x = (T)(x >> 16); l |= 16; }
    if (x >= 1<<8)  { x = (T)(x >> 8);  l |= 8; }
    if (x >= 1<<4)  { x = (T)(x >> 4);  l |= 4; }
    if (x >= 1<<2)  { x = (T)(x >> 2);  l |= 2; }
    if (x >= 1<<1)  l |=1;
    return (T)l;
}

template<>
inline bm::gap_word_t ilog2(gap_word_t x)
{
    unsigned int l = 0;
    if (x >= 1<<8)  { x = (bm::gap_word_t)(x >> 8); l |= 8; }
    if (x >= 1<<4)  { x = (bm::gap_word_t)(x >> 4); l |= 4; }
    if (x >= 1<<2)  { x = (bm::gap_word_t)(x >> 2); l |= 2; }
    if (x >= 1<<1)  l |=1;
    return (bm::gap_word_t)l;
}

/**
     Mini auto-pointer for internal memory management
     @internal
*/
template<class T>
class ptr_guard
{
public:
    ptr_guard(T* p) : ptr_(p) {}
    ~ptr_guard() { delete ptr_; }
private:
    ptr_guard(const ptr_guard<T>& p);
    ptr_guard& operator=(const ptr_guard<T>& p);
private:
    T* ptr_;
};

/**
    Lookup table based integer LOG2
*/
template<typename T>
T ilog2_LUT(T x)
{
    unsigned l = 0;
    if (x & 0xffff0000) 
    {
        l += 16; x >>= 16;
    }
    
    if (x & 0xff00) 
    {
        l += 8; x >>= 8;
    }
    return l + first_bit_table<true>::_idx[x];
}

/**
    Lookup table based short integer LOG2
*/
template<>
inline bm::gap_word_t ilog2_LUT<bm::gap_word_t>(bm::gap_word_t x)
{
    unsigned l = 0;    
    if (x & 0xff00) 
    {
        l += 8;
        x = (bm::gap_word_t)(x >> 8);
    }
    return (bm::gap_word_t)(l + first_bit_table<true>::_idx[x]);
}


// if we are running on x86 CPU we can use inline ASM 

#ifdef BM_x86
#ifdef __GNUG__

inline 
unsigned bsf_asm32(unsigned int v)
{
    unsigned r;
    asm volatile(" bsfl %1, %0": "=r"(r): "rm"(v) );
    return r;
}
 
BMFORCEINLINE unsigned bsr_asm32(unsigned int v)
{
    unsigned r;
    asm volatile(" bsrl %1, %0": "=r"(r): "rm"(v) );
    return r;
}

#endif  // __GNUG__

#ifdef _MSC_VER

#if defined(_M_AMD64) || defined(_M_X64) // inline assembly not supported

BMFORCEINLINE unsigned int bsr_asm32(unsigned int value)
{
    unsigned long r;
    _BitScanReverse(&r, value);
    return r;
}

BMFORCEINLINE unsigned int bsf_asm32(unsigned int value)
{
    unsigned long r;
    _BitScanForward(&r, value);
    return r;
}

#else

BMFORCEINLINE unsigned int bsr_asm32(unsigned int value)
{   
  __asm  bsr  eax, value
}

BMFORCEINLINE unsigned int bsf_asm32(unsigned int value)
{   
  __asm  bsf  eax, value
}

#endif

#endif // _MSC_VER

#endif // BM_x86


// From:
// http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.37.8562
//
template<typename T>
T bit_scan_fwd(T v)
{
    return
        DeBruijn_bit_position<true>::_multiply[((word_t)((v & -v) * 0x077CB531U)) >> 27];
}

inline
unsigned bit_scan_reverse32(unsigned value)
{
    BM_ASSERT(value);
#if defined(BM_x86)
    return bm::bsr_asm32(value);
#else
    return bm::ilog2_LUT<unsigned int>(value);
#endif
}


} // bm

#endif
