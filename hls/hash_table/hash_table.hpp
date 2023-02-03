/************************************************
Copyright (c) 2019, Systems Group, ETH Zurich.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
************************************************/
#pragma once
#include "toe/toe_config.hpp"

#include <ap_int.h>
#include <hls_stream.h>
#include <math.h>

#ifndef __SYNTHESIS__
#include <iostream>
#endif

const uint32_t kMaxNumberOfEntries = TCP_MAX_SESSIONS;

// Copied from hlslib by Johannes de Fine Licht
// https://github.com/definelicht/hlslib/blob/master/include/hlslib/xilinx/Utility.h
constexpr unsigned long ConstLog2(unsigned long val) {
  return val == 1 ? 0 : 1 + ConstLog2(val >> 1);
}

const uint32_t kMaxKeySize     = (MULTI_IP_ADDR) ? 96 : 64;
const uint32_t kMaxAddressBits = 16;

const uint32_t kNumTables        = 9;
const uint32_t kTableAddressBits = ConstLog2(kMaxNumberOfEntries / (kNumTables - 1));
const uint32_t kTableSize        = (1 << kTableAddressBits);

const uint32_t kKeySize   = (MULTI_IP_ADDR) ? 96 : 64;
const uint32_t kValueSize = 16;
const uint32_t kMaxTrials = 12;

typedef enum { KV_INSERT, KV_DELETE } KVOperation;
typedef enum { SRC_A, SRC_B } KVOpeationSource;
// The hash table can easily support kNumTables-1 * kTableSize
// for kNumTables = 9 -> this equals to a load factor of 0.88

template <int K>
struct HTLookupReq {
  ap_uint<K>       key;
  KVOpeationSource source;
  HTLookupReq<K>() {}
  HTLookupReq<K>(ap_uint<K> key, KVOpeationSource source) : key(key), source(source) {}
};

template <int K, int V>
struct HTLookupResp {
  ap_uint<K>       key;
  ap_uint<V>       value;
  bool             hit;
  KVOpeationSource source;
};

template <int K, int V>
struct HTUpdateReq {
  KVOperation      op;
  ap_uint<K>       key;
  ap_uint<V>       value;
  KVOpeationSource source;
  HTUpdateReq<K, V>() {}
  HTUpdateReq<K, V>(KVOperation op, ap_uint<K> key, ap_uint<V> value, KVOpeationSource source)
      : op(op), key(key), value(value), source(source) {}
};

template <int K, int V>
struct HTUpdateResp {
  KVOperation      op;
  ap_uint<K>       key;
  ap_uint<V>       value;
  bool             success;
  KVOpeationSource source;
};

template <int K, int V>
struct HTEntry {
  ap_uint<K> key;
  ap_uint<V> value;
  bool       valid;
  HTEntry<K, V>() {}
  HTEntry<K, V>(ap_uint<K> key, ap_uint<V> value) : key(key), value(value), valid(true) {}
};

template <int K, int V>
void HashTable(hls::stream<HTLookupReq<K> >     &s_axis_lup_req,
               hls::stream<HTUpdateReq<K, V> >  &s_axis_upd_req,
               hls::stream<HTLookupResp<K, V> > &m_axis_lup_rsp,
               hls::stream<HTUpdateResp<K, V> > &m_axis_upd_rsp,
               ap_uint<16>                      &regInsertFailureCount);
