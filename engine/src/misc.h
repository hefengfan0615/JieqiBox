/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2022 The Stockfish developers (see AUTHORS file)

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

#ifndef MISC_H_INCLUDED
#define MISC_H_INCLUDED

#include <cassert>
#include <chrono>
#include <memory>
#include <new>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <type_traits>
#include <utility>

#include "types.h"

namespace Stockfish {

std::string engine_info(bool to_uci = false);
std::string compiler_info();
void prefetch(void* addr);
void start_logger(const std::string& fname);
void* std_aligned_alloc(size_t alignment, size_t size);
void std_aligned_free(void* ptr);
void* aligned_large_pages_alloc(size_t size); // memory aligned by page size, min alignment: 4096 bytes
void aligned_large_pages_free(void* mem); // nop if mem == nullptr
std::stringstream read_zipped_nnue(const std::string& fpath);

// Frees memory which was placed there with placement new.
// Works for both single objects and arrays of unknown bound.
template<typename T, typename FREE_FUNC>
void memory_deleter(T* ptr, FREE_FUNC free_func) {
    if (!ptr)
        return;

    // Explicitly needed to call the destructor
    if constexpr (!std::is_trivially_destructible_v<T>)
        ptr->~T();

    free_func(ptr);
    return;
}

// Frees memory which was placed there with placement new.
// Works for both single objects and arrays of unknown bound.
template<typename T, typename FREE_FUNC>
void memory_deleter_array(T* ptr, FREE_FUNC free_func) {
    if (!ptr)
        return;


    // Move back on the pointer to where the size is allocated
    const size_t array_offset = std::max(sizeof(size_t), alignof(T));
    char*        raw_memory   = reinterpret_cast<char*>(ptr) - array_offset;

    if constexpr (!std::is_trivially_destructible_v<T>)
    {
        const size_t size = *reinterpret_cast<size_t*>(raw_memory);

        // Explicitly call the destructor for each element in reverse order
        for (size_t i = size; i-- > 0;)
            ptr[i].~T();
    }

    free_func(raw_memory);
}

// Allocates memory for a single object and places it there with placement new
template<typename T, typename ALLOC_FUNC, typename... Args>
inline std::enable_if_t<!std::is_array_v<T>, T*> memory_allocator(ALLOC_FUNC alloc_func,
                                                                  Args&&... args) {
    void* raw_memory = alloc_func(sizeof(T));
    assert((reinterpret_cast<uintptr_t>(raw_memory) & (alignof(T) - 1)) == 0);
    return new (raw_memory) T(std::forward<Args>(args)...);
}

// Allocates memory for an array of unknown bound and places it there with placement new
template<typename T, typename ALLOC_FUNC>
inline std::enable_if_t<std::is_array_v<T>, std::remove_extent_t<T>*>
memory_allocator(ALLOC_FUNC alloc_func, size_t num) {
    using ElementType = std::remove_extent_t<T>;

    const size_t array_offset = std::max(sizeof(size_t), alignof(ElementType));

    // Save the array size in the memory location
    char* raw_memory =
      reinterpret_cast<char*>(alloc_func(array_offset + num * sizeof(ElementType)));
    assert((reinterpret_cast<uintptr_t>(raw_memory) & (alignof(T) - 1)) == 0);

    new (raw_memory) size_t(num);

    for (size_t i = 0; i < num; ++i)
        new (raw_memory + array_offset + i * sizeof(ElementType)) ElementType();

    // Need to return the pointer at the start of the array so that
    // the indexing in unique_ptr<T[]> works.
    return reinterpret_cast<ElementType*>(raw_memory + array_offset);
}

//
//
// aligned large page unique ptr
//
//

template<typename T>
struct LargePageDeleter {
    void operator()(T* ptr) const { return memory_deleter<T>(ptr, aligned_large_pages_free); }
};

template<typename T>
struct LargePageArrayDeleter {
    void operator()(T* ptr) const { return memory_deleter_array<T>(ptr, aligned_large_pages_free); }
};

template<typename T>
using LargePagePtr =
  std::conditional_t<std::is_array_v<T>,
                     std::unique_ptr<T, LargePageArrayDeleter<std::remove_extent_t<T>>>,
                     std::unique_ptr<T, LargePageDeleter<T>>>;

// make_unique_large_page for single objects
template<typename T, typename... Args>
std::enable_if_t<!std::is_array_v<T>, LargePagePtr<T>> make_unique_large_page(Args&&... args) {
    static_assert(alignof(T) <= 4096,
                  "aligned_large_pages_alloc() may fail for such a big alignment requirement of T");

    T* obj = memory_allocator<T>(aligned_large_pages_alloc, std::forward<Args>(args)...);

    return LargePagePtr<T>(obj);
}

// make_unique_large_page for arrays of unknown bound
template<typename T>
std::enable_if_t<std::is_array_v<T>, LargePagePtr<T>> make_unique_large_page(size_t num) {
    using ElementType = std::remove_extent_t<T>;

    static_assert(alignof(ElementType) <= 4096,
                  "aligned_large_pages_alloc() may fail for such a big alignment requirement of T");

    ElementType* memory = memory_allocator<T>(aligned_large_pages_alloc, num);

    return LargePagePtr<T>(memory);
}

//
//
// aligned unique ptr
//
//

template<typename T>
struct AlignedDeleter {
    void operator()(T* ptr) const { return memory_deleter<T>(ptr, std_aligned_free); }
};

template<typename T>
struct AlignedArrayDeleter {
    void operator()(T* ptr) const { return memory_deleter_array<T>(ptr, std_aligned_free); }
};

template<typename T>
using AlignedPtr =
  std::conditional_t<std::is_array_v<T>,
                     std::unique_ptr<T, AlignedArrayDeleter<std::remove_extent_t<T>>>,
                     std::unique_ptr<T, AlignedDeleter<T>>>;

// make_unique_aligned for single objects
template<typename T, typename... Args>
std::enable_if_t<!std::is_array_v<T>, AlignedPtr<T>> make_unique_aligned(Args&&... args) {
    const auto func = [](size_t size) { return std_aligned_alloc(alignof(T), size); };
    T*         obj  = memory_allocator<T>(func, std::forward<Args>(args)...);

    return AlignedPtr<T>(obj);
}

// make_unique_aligned for arrays of unknown bound
template<typename T>
std::enable_if_t<std::is_array_v<T>, AlignedPtr<T>> make_unique_aligned(size_t num) {
    using ElementType = std::remove_extent_t<T>;

    const auto   func   = [](size_t size) { return std_aligned_alloc(alignof(ElementType), size); };
    ElementType* memory = memory_allocator<T>(func, num);

    return AlignedPtr<T>(memory);
}

void dbg_hit_on(bool b);
void dbg_hit_on(bool c, bool b);
void dbg_mean_of(int v);
void dbg_print();

typedef std::chrono::milliseconds::rep TimePoint; // A value in milliseconds
static_assert(sizeof(TimePoint) == sizeof(int64_t), "TimePoint should be 64 bits");
inline TimePoint now() {
  return std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::steady_clock::now().time_since_epoch()).count();
}

template<class Entry, int Size>
struct HashTable {
  Entry* operator[](Key key) { return &table[(uint32_t)key & (Size - 1)]; }

private:
  std::vector<Entry> table = std::vector<Entry>(Size); // Allocate on the heap
};

struct BloomFilter {
    constexpr static uint64_t FILTER_SIZE = 1 << 14;
    uint8_t  operator[](Key key) const { return table[key & (FILTER_SIZE - 1)]; }
    uint8_t& operator[](Key key)       { return table[key & (FILTER_SIZE - 1)]; }

private:
    uint8_t table[1 << 14];
};

enum SyncCout { IO_LOCK, IO_UNLOCK };
std::ostream& operator<<(std::ostream&, SyncCout);

#define sync_cout std::cout << IO_LOCK
#define sync_endl std::endl << IO_UNLOCK


// align_ptr_up() : get the first aligned element of an array.
// ptr must point to an array of size at least `sizeof(T) * N + alignment` bytes,
// where N is the number of elements in the array.
template <uintptr_t Alignment, typename T>
T* align_ptr_up(T* ptr)
{
  static_assert(alignof(T) < Alignment);

  const uintptr_t ptrint = reinterpret_cast<uintptr_t>(reinterpret_cast<char*>(ptr));
  return reinterpret_cast<T*>(reinterpret_cast<char*>((ptrint + (Alignment - 1)) / Alignment * Alignment));
}


// IsLittleEndian : true if and only if the binary is compiled on a little endian machine
static inline const union { uint32_t i; char c[4]; } Le = { 0x01020304 };
static inline const bool IsLittleEndian = (Le.c[0] == 4);


// RunningAverage : a class to calculate a running average of a series of values.
// For efficiency, all computations are done with integers.
class RunningAverage {
  public:

      // Reset the running average to rational value p / q
      void set(int64_t p, int64_t q)
        { average = p * PERIOD * RESOLUTION / q; }

      // Update average with value v
      void update(int64_t v)
        { average = RESOLUTION * v + (PERIOD - 1) * average / PERIOD; }

      // Test if average is strictly greater than rational a / b
      bool is_greater(int64_t a, int64_t b) const
        { return b * average > a * (PERIOD * RESOLUTION); }

      int64_t value() const
        { return average / (PERIOD * RESOLUTION); }

  private :
      static constexpr int64_t PERIOD     = 4096;
      static constexpr int64_t RESOLUTION = 1024;
      int64_t average;
};

template <typename T, std::size_t MaxSize>
class ValueList {

public:
  std::size_t size() const { return size_; }
  void push_back(const T& value) { assert(size_ < MaxSize); values_[size_++] = value; }
  const T* begin() const { return values_; }
  const T* end() const { return values_ + size_; }
  const T& operator[](int index) const { return values_[index]; }

protected:
  T values_[MaxSize];
  std::size_t size_ = 0;
};

/// xorshift64star Pseudo-Random Number Generator
/// This class is based on original code written and dedicated
/// to the public domain by Sebastiano Vigna (2014).
/// It has the following characteristics:
///
///  -  Outputs 64-bit numbers
///  -  Passes Dieharder and SmallCrush test batteries
///  -  Does not require warm-up, no zeroland to escape
///  -  Internal state is a single 64-bit integer
///  -  Period is 2^64 - 1
///  -  Speed: 1.60 ns/call (Core i7 @3.40GHz)
///
/// For further analysis see
///   <http://vigna.di.unimi.it/ftp/papers/xorshift.pdf>

class PRNG {

  uint64_t s;

  uint64_t rand64() {

    s ^= s >> 12, s ^= s << 25, s ^= s >> 27;
    return s * 2685821657736338717LL;
  }

public:
  PRNG(uint64_t seed) : s(seed) { assert(seed); }

  template<typename T> T rand() { return T(rand64()); }

  /// Special generator used to fast init magic numbers.
  /// Output values only have 1/8th of their bits set on average.
  template<typename T> T sparse_rand()
  { return T(rand64() & rand64() & rand64()); }
};


template <typename T, std::size_t MaxSize>
class RestListTmp :protected ValueList<T, MaxSize> {

public:
    void clear() { this->size_ = 0; }
    std::size_t size() const { return this->size_; }
    void push_back(const T& value) { ValueList<T, MaxSize>::push_back(value); }
    T pop_back() { assert(this->size_); return this->values_[--this->size_]; }
    const T peek() const { assert(this->size_); return this->values_[this->size_ - 1]; }
    const T at(int i) const { assert(i >= 0 && i < this->size_); return this->values_[i]; }
    void set(int i, const T v) { assert(i >= 0 && i < this->size_);  this->values_[i] = v; }

    void swap(int index1, int index2) {
        assert(index1 >= 0 && index1 < this->size_);
        assert(index2 >= 0 && index2 < this->size_);

        T tmp = this->values_[index1];
        this->values_[index1] = this->values_[index2];
        this->values_[index2] = tmp;
    }

};

class RestList :public RestListTmp<Piece, 15> {

public:
    void clear() { 
        memset(typeNum, 0, sizeof(int) * PIECE_TYPE_NB); 
        for (int i = 0; i < PIECE_TYPE_NB; i++)
        {
            typePos[i].clear();
        }
        RestListTmp<Piece, 15>::clear();
        //clear sumValue
        _sumValue = 0;
        _evgValue = 0;
    }

    void push_back(const Piece& value) { 
        PieceType t = type_of(value);
        if (typeNum[type_of(value)])assert(this->size_ > typePos[t].peek());
        typePos[t].push_back(this->size_);
        typeNum[type_of(value)]++;
        RestListTmp<Piece, 15>::push_back(value);
        //update sumValue
        _sumValue += PieceValue[MG][value ^ 16];
        _evgValue = _sumValue / this->size_;
    }

    Piece pop_back() { 
        assert(this->size_); 
        Piece p = RestListTmp<Piece, 15>::pop_back();
        PieceType t = type_of(p);
        typePos[t].pop_back();
        typeNum[t]--;
        //update sumValue
        _sumValue -= PieceValue[MG][p ^ 16];
        _evgValue = this->size_ ? _sumValue / this->size_ : 0;
        return p; 
    }

    Piece pop_back(PieceType t) {
        if (typeNum[t] == 0)return NO_PIECE;
        typeNum[t]--;
        int index = typePos[t].pop_back();
        assert(index < this->size_);
        int back = this->size_ - 1;
        if (back != index) {
            //swap index and back
            Piece tmp = this->values_[back];
            this->values_[back] = this->values_[index];
            this->values_[index] = tmp;
            //uudate typePos
            PieceType t1 = type_of(tmp);
            typePos[t1].pop_back();
            typePos[t1].push_back(index);

            for (int i = typePos[t1].size() - 1; i >0; i--)
            {
                if (typePos[t1].at(i) > typePos[t1].at(i - 1))break;
                typePos[t1].swap(i, i - 1);
            }

        }
        //return pop_back
        Piece p = RestListTmp<Piece, 15>::pop_back();
        //update sumValue
        _sumValue -= PieceValue[MG][p ^ 16];
        _evgValue = this->size_ ? _sumValue / this->size_ : 0;
        return p;
    }

    int evgValue()const {
        assert(this->size_);
        assert(_evgValue < 1500 && _evgValue>0);
        return _evgValue;
    }

    int countType(const PieceType t) const {
        return typeNum[t];
    }

    int notNullTypeCount() {
        int TypeCount = 0;
        for (int t = 0; t < PIECE_TYPE_NB; t++)
        {
            if (typeNum[t] == 0)continue;
            TypeCount++;
        }
        return TypeCount;
    }


    void print() {
        printf("rest db (size=%d):", (int)this->size_);
        if (!this->size_)printf("null");
        for (size_t i = 0; i < this->size_; i++)
        {
            if (this->values_[i] > 8) {
                printf("B%d ", this->values_[i] - 8);
            }
            else
            {
                printf("W%d ", this->values_[i]);
            }

        }
        printf("\r\n");

        printf("pos info [type-num] index...:");
        for (int t = 0; t < PIECE_TYPE_NB; t++)
        {
            if (typeNum[t] == 0)continue;
            printf(" [%d-%d]", t, typeNum[t]);
            for (size_t j = 0; j < typePos[t].size(); j++)
            {
                printf(" %d", typePos[t].at(j));
            }
            printf(",");
        }
        printf("\r\n\r\n");

    }

    void shuffle() { 
        static int seed = 52808;
        PRNG rng(seed);
        seed = rng.rand<int>();

        int maxIndex = this->size_ - 1;
        memset(typeNum, 0, sizeof(int) * PIECE_TYPE_NB);
        for (int i = 0; i < PIECE_TYPE_NB; i++)
        {
            typePos[i].clear();
        }
        for (int i = 0; i < maxIndex; i++)
        {
            int p = abs(rng.rand<int>()) % (maxIndex - i) + 1 + i;
            RestListTmp<Piece, 15>::swap(i, p);
            PieceType t = type_of(RestListTmp<Piece, 15>::at(i));
            typePos[t].push_back(i);
            typeNum[t]++;
        }
        PieceType t = type_of(RestListTmp<Piece, 15>::at(maxIndex));
        typePos[t].push_back(maxIndex);
        typeNum[t]++;
    }


private:
    int typeNum[PIECE_TYPE_NB] = { 0 };
    RestListTmp<int, 5> typePos[PIECE_TYPE_NB];
    int _sumValue = 0, _evgValue = 0;
};


class ScoreCalc {
public:
    ScoreCalc(int Ldepth, int depth, bool us) :
        _us(us), _Ldepth(Ldepth), _depth(depth){}

    void setUs(bool us) { _us = !us; }

    void append([[maybe_unused]] Piece p, int score, int count) {
        if (!_us) score *= -1;
        if (_min > score)_min = score;
        if (_max < score)_max = score;
        score = std::clamp(score, -DARKVALRATE, DARKVALRATE);
        _totalScore += score * count;
        _totalCount += count;
    }

    Value CalcEvg() {
        int min = std::clamp(_min, -DARKVALRATE, DARKVALRATE);
        int v;
        int evg = _totalScore / _totalCount;
        if (evg - min > DARKMAXDIFF) {
            v = _min;
        }
        else if(evg == DARKVALRATE)
        {
            v = _max;
        }        
        else if (evg == -DARKVALRATE)
        {
            v = _min;
        }        
        else
        {
            v = evg;
        }
        if (!_us) v *= -1;
        assert(v > -VALUE_INFINITE && v < VALUE_INFINITE);
        return Value(v);
    }

private:
    bool _us;
    [[maybe_unused]] int _Ldepth;
    [[maybe_unused]] int _depth;
    int _totalScore = 0;
    int _totalCount = 0;
    int _min = 99999999;
    int _max = -99999999;
};


inline uint64_t mul_hi64(uint64_t a, uint64_t b) {
#if defined(__GNUC__) && defined(IS_64BIT)
    __extension__ typedef unsigned __int128 uint128;
    return ((uint128)a * (uint128)b) >> 64;
#else
    uint64_t aL = (uint32_t)a, aH = a >> 32;
    uint64_t bL = (uint32_t)b, bH = b >> 32;
    uint64_t c1 = (aL * bL) >> 32;
    uint64_t c2 = aH * bL + c1;
    uint64_t c3 = aL * bH + (uint32_t)c2;
    return aH * bH + (c2 >> 32) + (c3 >> 32);
#endif
}

/// Under Windows it is not possible for a process to run on more than one
/// logical processor group. This usually means to be limited to use max 64
/// cores. To overcome this, some special platform specific API should be
/// called to set group affinity for each thread. Original code from Texel by
/// Peter Österlund.

namespace WinProcGroup {
  void bindThisThread(size_t idx);
}

namespace CommandLine {
  void init(int argc, char* argv[]);

  extern std::string binaryDirectory;  // path of the executable directory
  extern std::string workingDirectory; // path of the working directory
}

} // namespace Stockfish

#endif // #ifndef MISC_H_INCLUDED
