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

void
smart_tlb_t::access_update(int block_idx, int _way)
{
    // old LRU stack position
    int old = get_caching_device_block(block_idx, _way).counter_;
    get_caching_device_block(block_idx, _way).counter_ = MRU;

    for (int way = 0; way < associativity_; ++way) {
        if (way == _way) continue;

        auto block = get_caching_device_block(block_idx, way);
        if (block.counter_ < old) block.counter_++;
    }

    _miss = false;
}

int
smart_tlb_t::replace_which_way(int block_idx)
{
    _miss = true;

    // check if the current set is not yet full
    for (int way = 0; way < associativity_; ++way) {
        if (get_caching_device_block(block_idx, way).tag_ == TAG_INVALID) {
            return way;
        }
    }

    for (int way = 0; way < associativity_; ++way) {
        // find the LRU block
        if (get_caching_device_block(block_idx, way).counter_ == LRU)
            return way;
    }

    // should never get here
    assert(false);
    return -1;
}
