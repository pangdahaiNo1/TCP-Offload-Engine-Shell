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
#include "hash_table.hpp"

const ap_uint<kMaxAddressBits> tabulation_table[kNumTables][2][kMaxKeySize] = {
#include "tabulation_table.txt"
};

static HTEntry<kKeySize, kValueSize> cuckooTables[kNumTables][kTableSize];

void CalculateHashes(ap_uint<kKeySize> key, ap_uint<kTableAddressBits> hashes[kNumTables]) {
#pragma HLS ARRAY_PARTITION variable = hashes complete dim = 1

  for (int i = 0; i < kNumTables; i++) {
#pragma HLS UNROLL

    ap_uint<kTableAddressBits> hash = 0;
    for (int k = 0; k < kKeySize; k++) {
#pragma HLS UNROLL
      ap_uint<kMaxAddressBits> random_value = tabulation_table[i][key[k]][k];
      hash ^= random_value(kTableAddressBits - 1, 0);
    }
    hashes[i] = hash;
  }
}

template <int K, int V>
HTLookupResp<K, V> LookUp(HTLookupReq<K> request) {
#pragma HLS        INLINE

  HTEntry<K, V> current_entries[kNumTables];
#pragma HLS ARRAY_PARTITION variable = current_entries complete
  ap_uint<kTableAddressBits> hashes[kNumTables];
  HTLookupResp<K, V>         response;
  response.key    = request.key;
  response.source = request.source;
  response.hit    = false;

  CalculateHashes(request.key, hashes);
  // Look for matching key
  int slot = -1;
  for (int i = 0; i < kNumTables; i++) {
#pragma HLS UNROLL
    current_entries[i] = cuckooTables[i][hashes[i]];
    if (current_entries[i].valid && current_entries[i].key == request.key) {
      slot = i;
    }
  }
  // Check if key found
  if (slot != -1) {
    response.value = current_entries[slot].value;
    response.hit   = true;
  }

  return response;
}

template <int K, int V>
HTUpdateResp<K, V> Insert(HTUpdateReq<K, V> request, ap_uint<16> &reg_insert_fail_count) {
#pragma HLS        INLINE

  HTEntry<K, V> current_entries[kNumTables];
#pragma HLS ARRAY_PARTITION variable = current_entries complete
  ap_uint<kTableAddressBits> hashes[kNumTables];
  static ap_uint<8>          victimIdx = 0;
  static ap_uint<1>          victimBit = 0;
  HTUpdateResp<K, V>         response;
  response.op                          = request.op;
  response.key                         = request.key;
  response.value                       = request.value;
  response.source                      = request.source;
  response.success                     = false;
  static uint16_t insertFailureCounter = 0;

  reg_insert_fail_count = insertFailureCounter;

  HTEntry<K, V> currentEntry(request.key, request.value);
  victimIdx = 0;
// Try multiple times
insertLoop:
  for (int j = 0; j < kMaxTrials; j++) {
    CalculateHashes(currentEntry.key, hashes);
    // Look for free slot
    int slot = -1;
    for (int i = 0; i < kNumTables; i++) {
#pragma HLS UNROLL
      current_entries[i] = cuckooTables[i][hashes[i]];
      if (!current_entries[i].valid) {
        slot = i;
      }
    }

    // If free slot
    if (slot != -1) {
      current_entries[slot] = currentEntry;
      response.success      = true;
    } else {
      // Evict existing entry and try to re-insert
      int           victimPos = (hashes[victimIdx] % (kNumTables - 1)) + victimBit;
      HTEntry<K, V> victimEntry =
          current_entries[victimPos];  // cuckooTables[victimPos][hashes[victimPos]];
      current_entries[victimPos] = currentEntry;
      currentEntry               = victimEntry;
      victimIdx++;
      if (victimIdx == kNumTables)
        victimIdx = 0;
    }
    // Write current_entries back
    for (int i = 0; i < kNumTables; i++) {
#pragma HLS UNROLL
      cuckooTables[i][hashes[i]] = current_entries[i];
    }

    victimBit++;
    if (response.success)
      break;
  }  // for
  if (!response.success) {
    std::cout << "REACHED MAX TRIALS: " << request.key << " " << currentEntry.key << std::endl;
    insertFailureCounter++;
  }
  return response;
}

template <int K, int V>
HTUpdateResp<K, V> remove(HTUpdateReq<K, V> request) {
#pragma HLS        INLINE

  HTEntry<K, V> current_entries[kNumTables];
#pragma HLS ARRAY_PARTITION variable = current_entries complete
  ap_uint<kTableAddressBits> hashes[kNumTables];
  HTUpdateResp<K, V>         response;
  response.op      = request.op;
  response.key     = request.key;
  response.value   = request.value;
  response.source  = request.source;
  response.success = false;

  CalculateHashes(request.key, hashes);
  // Look for matching key
  for (int i = 0; i < kNumTables; i++) {
#pragma HLS UNROLL
    current_entries[i] = cuckooTables[i][hashes[i]];
    if (current_entries[i].valid && current_entries[i].key == request.key) {
      current_entries[i].valid = false;
      response.success         = true;
    }
    cuckooTables[i][hashes[i]] = current_entries[i];
  }

  return response;
}

template <int K, int V>
void HashTable(hls::stream<HTLookupReq<K> > &    s_axis_lup_req,
               hls::stream<HTUpdateReq<K, V> > & s_axis_upd_req,
               hls::stream<HTLookupResp<K, V> > &m_axis_lup_rsp,
               hls::stream<HTUpdateResp<K, V> > &m_axis_upd_rsp,
               ap_uint<16> &                     reg_insert_fail_count)

{
#pragma HLS INLINE

// Global arrays
#pragma HLS ARRAY_PARTITION variable = tabulation_table complete dim = 1
#pragma HLS RESOURCE variable = cuckooTables core = RAM_2P_BRAM
#pragma HLS ARRAY_PARTITION variable = cuckooTables complete dim = 1

  if (!s_axis_lup_req.empty()) {
    HTLookupReq<K>     request  = s_axis_lup_req.read();
    HTLookupResp<K, V> response = LookUp<K, V>(request);
    m_axis_lup_rsp.write(response);
  } else if (!s_axis_upd_req.empty()) {
    HTUpdateReq<K, V> request = s_axis_upd_req.read();
    if (request.op == KV_INSERT) {
      HTUpdateResp<K, V> response = Insert<K, V>(request, reg_insert_fail_count);
      m_axis_upd_rsp.write(response);
    } else  // DELETE
    {
      HTUpdateResp<K, V> response = remove<K, V>(request);
      m_axis_upd_rsp.write(response);
    }
  }
}

void hash_table_top(hls::stream<HTLookupReq<kKeySize> > &             s_axis_lup_req,
                    hls::stream<HTUpdateReq<kKeySize, kValueSize> > & s_axis_upd_req,
                    hls::stream<HTLookupResp<kKeySize, kValueSize> > &m_axis_lup_rsp,
                    hls::stream<HTUpdateResp<kKeySize, kValueSize> > &m_axis_upd_rsp,
                    ap_uint<16> &                                     reg_insert_fail_count) {
#pragma HLS INTERFACE ap_ctrl_none port = return
#pragma HLS INTERFACE ap_stable    port = reg_insert_fail_count

#pragma HLS INTERFACE axis register port = s_axis_lup_req
#pragma HLS INTERFACE axis register port = s_axis_upd_req
#pragma HLS INTERFACE axis register port = m_axis_lup_rsp
#pragma HLS INTERFACE axis register port = m_axis_upd_rsp
#pragma HLS DATA_PACK variable           = s_axis_lup_req
#pragma HLS DATA_PACK variable           = s_axis_upd_req
#pragma HLS DATA_PACK variable           = m_axis_lup_rsp
#pragma HLS DATA_PACK variable           = m_axis_upd_rsp

  HashTable<kKeySize, kValueSize>(
      s_axis_lup_req, s_axis_upd_req, m_axis_lup_rsp, m_axis_upd_rsp, reg_insert_fail_count);
}
