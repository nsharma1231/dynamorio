/* **********************************************************
 * Copyright (c) 2015-2021 Google, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "smart_tlb.h"
#include "../common/utils.h"
#include <assert.h>
#include <stdlib.h>
#include <iostream>
#include <unordered_set>

#ifdef MYDEBUG
#define HERE(str)  \
    std::cout << "[LOG] reached code location " << __FILE__ << ":" << __LINE__  << " (" << str << ")\n"
#else
#define HERE(str)
#endif

void
smart_tlb_t::access_update(int block_idx, int _way)
{
    HERE("entered access_update");
#ifdef MYDEBUG
    if (_way < 0 || _way >= associativity_)
        std::cout << "[LOG] _way is out of bounds" << std::endl;
#endif

    // old LRU stack position
    int old = _miss ? associativity_ : get_caching_device_block(block_idx, _way).counter_;
    get_caching_device_block(block_idx, _way).counter_ = MRU;

    for (int way = 0; way < associativity_; ++way) {
        if (way == _way) continue;

        auto block = get_caching_device_block(block_idx, way);
        if (block.tag_ == TAG_INVALID) continue;
        
        if (block.counter_ < old) block.counter_++;
    }

    _miss = false;
    HERE("exiting access_update");
}


int
smart_tlb_t::replace_which_way(int line_idx)
{
    // We implement LRU by picking the slot with the largest counter value.
    // because i hate life
    int max_counter = 0;
    int max_way = 0;
    for (int way = 0; way < associativity_; ++way) {
        if (get_caching_device_block(line_idx, way).tag_ == TAG_INVALID) {
            max_way = way;
            break;
        }
        if (get_caching_device_block(line_idx, way).counter_ > max_counter) {
            max_counter = get_caching_device_block(line_idx, way).counter_;
            max_way = way;
        }
    }
    return max_way;
}
