// Compress/RangeCoder/RangeCoder.h

#ifndef __COMPRESS_RANGECODER_H
#define __COMPRESS_RANGECODER_H

#include "../../Common/InBuffer.h"
#include "../../Common/OutBuffer.h"

namespace NCompress {
namespace NRangeCoder {

const int kNumTopBits = 24;
const UInt32 kTopValue = (1 << kNumTopBits);

class CEncoder
{
  COutBuffer Stream;
  UInt64 Low;
  UInt32 Range;
  UInt32 _ffNum;
  Byte _cache;

public:
  void Init(ISequentialOutStream *stream)
  {
    Stream.Init(stream);
    Low = 0;
    Range = 0xFFFFFFFF;
    _ffNum = 0;
    _cache = 0;
  }

  void FlushData()
  {
    // Low += 1; 
    for(int i = 0; i < 5; i++)
      ShiftLow();
  }

  HRESULT FlushStream() { return Stream.Flush();  }

  /*
  void ReleaseStream()
    { Stream.ReleaseStream(); }
  */

  void Encode(UInt32 start, UInt32 size, UInt32 total)
  {
    Low += start * (Range /= total);
    Range *= size;
    while (Range < kTopValue)
    {
      Range <<= 8;
      ShiftLow();
    }
  }

  /*
  void EncodeDirectBitsDiv(UInt32 value, UInt32 numTotalBits)
  {
    Low += value * (Range >>= numTotalBits);
    Normalize();
  }
  
  void EncodeDirectBitsDiv2(UInt32 value, UInt32 numTotalBits)
  {
    if (numTotalBits <= kNumBottomBits)
      EncodeDirectBitsDiv(value, numTotalBits);
    else
    {
      EncodeDirectBitsDiv(value >> kNumBottomBits, (numTotalBits - kNumBottomBits));
      EncodeDirectBitsDiv(value & ((1 << kBottomValueBits) - 1), kNumBottomBits);
    }
  }
  */
  void ShiftLow()
  {
    if (Low < (UInt32)0xFF000000 || UInt32(Low >> 32) == 1) 
    {
      Stream.WriteByte(Byte(_cache + Byte(Low >> 32)));            
      for (;_ffNum != 0; _ffNum--) 
        Stream.WriteByte(Byte(0xFF + Byte(Low >> 32)));
      _cache = Byte(UInt32(Low) >> 24);                      
    } 
    else 
      _ffNum++;                               
    Low = UInt32(Low) << 8;                           
  }
  
  void EncodeDirectBits(UInt32 value, int numTotalBits)
  {
    for (int i = numTotalBits - 1; i >= 0; i--)
    {
      Range >>= 1;
      if (((value >> i) & 1) == 1)
        Low += Range;
      if (Range < kTopValue)
      {
        Range <<= 8;
        ShiftLow();
      }
    }
  }

  void EncodeBit(UInt32 size0, UInt32 numTotalBits, UInt32 symbol)
  {
    UInt32 newBound = (Range >> numTotalBits) * size0;
    if (symbol == 0)
      Range = newBound;
    else
    {
      Low += newBound;
      Range -= newBound;
    }
    while (Range < kTopValue)
    {
      Range <<= 8;
      ShiftLow();
    }
  }

  UInt64 GetProcessedSize() {  return Stream.GetProcessedSize() + _ffNum; }
};

class CDecoder
{
public:
  CInBuffer Stream;
  UInt32 Range;
  UInt32 Code;
  void Normalize()
  {
    while (Range < kTopValue)
    {
      Code = (Code << 8) | Stream.ReadByte();
      Range <<= 8;
    }
  }
  
  void Init(ISequentialInStream *stream)
  {
    Stream.Init(stream);
    Code = 0;
    Range = 0xFFFFFFFF;
    for(int i = 0; i < 5; i++)
      Code = (Code << 8) | Stream.ReadByte();
  }

  // void ReleaseStream() { Stream.ReleaseStream(); }

  UInt32 GetThreshold(UInt32 total)
  {
    return (Code) / ( Range /= total);
  }

  void Decode(UInt32 start, UInt32 size)
  {
    Code -= start * Range;
    Range *= size;
    Normalize();
  }

  /*
  UInt32 DecodeDirectBitsDiv(UInt32 numTotalBits)
  {
    Range >>= numTotalBits;
    UInt32 threshold = Code / Range;
    Code -= threshold * Range;
    
    Normalize();
    return threshold;
  }

  UInt32 DecodeDirectBitsDiv2(UInt32 numTotalBits)
  {
    if (numTotalBits <= kNumBottomBits)
      return DecodeDirectBitsDiv(numTotalBits);
    UInt32 result = DecodeDirectBitsDiv(numTotalBits - kNumBottomBits) << kNumBottomBits;
    return (result | DecodeDirectBitsDiv(kNumBottomBits));
  }
  */

  UInt32 DecodeDirectBits(UInt32 numTotalBits)
  {
    UInt32 range = Range;
    UInt32 code = Code;        
    UInt32 result = 0;
    for (UInt32 i = numTotalBits; i > 0; i--)
    {
      range >>= 1;
      /*
      result <<= 1;
      if (code >= range)
      {
        code -= range;
        result |= 1;
      }
      */
      UInt32 t = (code - range) >> 31;
      code -= range & (t - 1);
      // range = rangeTmp + ((range & 1) & (1 - t));
      result = (result << 1) | (1 - t);

      if (range < kTopValue)
      {
        code = (code << 8) | Stream.ReadByte();
        range <<= 8; 
      }
    }
    Range = range;
    Code = code;
    return result;
  }

  UInt32 DecodeBit(UInt32 size0, UInt32 numTotalBits)
  {
    UInt32 newBound = (Range >> numTotalBits) * size0;
    UInt32 symbol;
    if (Code < newBound)
    {
      symbol = 0;
      Range = newBound;
    }
    else
    {
      symbol = 1;
      Code -= newBound;
      Range -= newBound;
    }
    Normalize();
    return symbol;
  }

  UInt64 GetProcessedSize() {return Stream.GetProcessedSize(); }
};

}}

#endif
