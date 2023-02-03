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

#include <map>
#include <random>

using namespace std;

std::default_random_engine              gen;
std::uniform_int_distribution<uint64_t> distr(0, std::numeric_limits<uint64_t>::max());

uint64_t getRandomKey() { return distr(gen); }

uint64_t getRandomTuple() { return 0; }

int main() {
  hls::stream<HTLookupReq<kKeySize> >              s_axis_lup_req;
  hls::stream<HTUpdateReq<kKeySize, kValueSize> >  s_axis_upd_req;
  hls::stream<HTLookupResp<kKeySize, kValueSize> > m_axis_lup_rsp;
  hls::stream<HTUpdateResp<kKeySize, kValueSize> > m_axis_upd_rsp;
  ap_uint<16>                                      reg_insert_fail_count;

  std::map<uint64_t, uint16_t> expected_kv;
  uint32_t                     num_elements = (kNumTables - 1) * kTableSize;
  std::cout << "Testing with " << num_elements
            << ", total number of entries: " << kNumTables * kTableSize
            << ", load factor: " << (double)(kNumTables - 1) / (double)kNumTables << std::endl;

  // insert elements
  int i = 0;
  while (i < num_elements) {
    uint64_t new_key   = getRandomKey();
    uint16_t new_value = i;

    // Check if key unique
    if (expected_kv.find(new_key) == expected_kv.end()) {
      s_axis_upd_req.write(HTUpdateReq<kKeySize, kValueSize>(KV_INSERT, new_key, new_value, SRC_A));
      expected_kv[new_key] = new_value;
      i++;
    }
  }

  bool success = true;
  int  count   = 0;
  // Execute Inserts
  int success_counter = 0;
  while (count < num_elements + 100) {
    HashTable(
        s_axis_lup_req, s_axis_upd_req, m_axis_lup_rsp, m_axis_upd_rsp, reg_insert_fail_count);
    if (!m_axis_upd_rsp.empty()) {
      HTUpdateResp<kKeySize, kValueSize> response = m_axis_upd_rsp.read();
      if (!response.success) {
        success = false;
        std::cerr << "[ERROR] insert failed" << std::endl;
      } else {
        success_counter++;
      }
    }
    count++;
  }
  if (success_counter != num_elements) {
    success = false;
    std::cerr << "[ERROR] not all elements inserted successfully, expected: " << num_elements
              << ", received: " << success_counter << std::endl;
  } else {
    std::cout << "[INFO] All elements inserted successfully, expected: " << num_elements << endl;
  }

  // Issue lookups
  std::map<uint64_t, uint16_t>::const_iterator it;
  for (it = expected_kv.begin(); it != expected_kv.end(); ++it) {
    uint64_t key = it->first;
    s_axis_lup_req.write(HTLookupReq<kKeySize>(key, SRC_A));
  }

  // Execute lookups
  count             = 0;
  int lookupCounter = 0;
  it                = expected_kv.begin();
  while (count < num_elements + 100) {
    HashTable(
        s_axis_lup_req, s_axis_upd_req, m_axis_lup_rsp, m_axis_upd_rsp, reg_insert_fail_count);
    if (!m_axis_lup_rsp.empty()) {
      HTLookupResp<kKeySize, kValueSize> response = m_axis_lup_rsp.read();
      if (!response.hit) {
        success = false;
        std::cerr << "[ERROR] LookUp did not return hit" << std::endl;
      } else {
        if (response.key == it->first && response.value == it->second) {
          lookupCounter++;
        } else {
          success = false;
          std::cerr << "[ERROR] LookUp returned wrong key or value" << std::endl;
        }
      }
      it++;
    }
    count++;
  }
  if (lookupCounter != num_elements) {
    success = false;
    std::cerr << "[ERROR] not all lookups successfully, expected: " << num_elements
              << ", received: " << lookupCounter << std::endl;
  } else {
    std::cout << "[INFO] All lookups successfully, expected: " << num_elements << std::endl;
  }

  // Deleting half of the keys
  bool     even            = true;
  uint32_t expectedDeletes = 0;
  for (it = expected_kv.begin(); it != expected_kv.end(); ++it) {
    uint64_t key = it->first;
    if (even) {
      s_axis_upd_req.write(HTUpdateReq<kKeySize, kValueSize>(KV_DELETE, key, SRC_A, SRC_A));
      expectedDeletes++;
    }
    even = !even;
  }

  // Execute deletes
  int deleteCounter = 0;
  count             = 0;
  while (count < expectedDeletes + 100) {
    HashTable(
        s_axis_lup_req, s_axis_upd_req, m_axis_lup_rsp, m_axis_upd_rsp, reg_insert_fail_count);
    if (!m_axis_upd_rsp.empty()) {
      HTUpdateResp<kKeySize, kValueSize> response = m_axis_upd_rsp.read();
      if (!response.success) {
        success = false;
        std::cerr << "[ERROR] delete failed" << std::endl;
      } else {
        deleteCounter++;
      }
    }
    count++;
  }
  if (deleteCounter != expectedDeletes) {
    success = false;
    std::cerr << "[ERROR] not all elements deletes successfully, expected: " << num_elements
              << ", received: " << success_counter << std::endl;
  } else {
    std::cout << "[INFO] All elements deletes successfully, expected: " << num_elements
              << std::endl;
  }

  // Run lookups again to make sure elements got deleted
  for (it = expected_kv.begin(); it != expected_kv.end(); ++it) {
    uint64_t key = it->first;
    s_axis_lup_req.write(HTLookupReq<kKeySize>(key, SRC_A));
  }

  // Execute lookups
  count         = 0;
  lookupCounter = 0;
  it            = expected_kv.begin();
  even          = true;
  while (count < num_elements + 100) {
    HashTable(
        s_axis_lup_req, s_axis_upd_req, m_axis_lup_rsp, m_axis_upd_rsp, reg_insert_fail_count);
    if (!m_axis_lup_rsp.empty()) {
      HTLookupResp<kKeySize, kValueSize> response = m_axis_lup_rsp.read();
      if (even) {
        if (response.hit) {
          success = false;
          std::cerr << "[ERROR] LookUp returned deleted item" << std::endl;
        }
      } else  //!even
      {
        if (!response.hit) {
          success = false;
          std::cerr << "[ERROR] LookUp did not return hit" << std::endl;
        } else {
          if (response.key == it->first && response.value == it->second) {
            lookupCounter++;
          } else {
            success = false;
            std::cerr << "[ERROR] LookUp returned wrong key or value" << std::endl;
          }
        }
      }
      it++;
      even = !even;
    }
    count++;
  }
  if (lookupCounter != (num_elements - expectedDeletes)) {
    success = false;
    std::cerr << "[ERROR] not all lookups successfully, expected: " << num_elements
              << ", received: " << lookupCounter << std::endl;
  } else {
    std::cout << "[INFO] All lookups successfully, expected: " << num_elements << std::endl;
  }

  std::cout << "reg_insert_fail_count: " << reg_insert_fail_count << std::endl;
  return (success) ? 0 : -1;
}