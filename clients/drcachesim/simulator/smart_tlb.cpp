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

#define compute_set(b) ((b) >> assoc_bits_)

static inline bool
is_srrip_leader(int set)
{
    int offset = set & 0x1f;
    int constituency = (set >> 5) & 0x1f;
    return offset == constituency;
}

static inline bool
is_brrip_leader(int set)
{
    int offset = (~set) & 0x1f;
    int constituency = (set >> 5) & 0x1f;
    return offset == constituency;
}

void
smart_tlb_t::access_update(int block_idx, int way)
{
    if (!_miss) {
        // set RRPV to 0
        get_caching_device_block(block_idx, way).counter_ = NEARIMM_RRPV;
        return;
    }

    _miss = false;
}

int
smart_tlb_t::replace_which_way(int block_idx)
{
    _miss = true;

    // The base caching device class only implements LFU.
    // A subclass can override this and access_update() to implement
    // some other scheme.
    int max_way = 0;
    int max_rrpv = 0;
    for (int way = 0; way < associativity_; ++way) {
        auto block = get_caching_device_block(block_idx, way);

        // extra space is available, use it
        if (block.tag_ == TAG_INVALID) {
            // set a _distant_ rrpv value for incoming way
            get_caching_device_block(block_idx, max_way).counter_ = LONG_RRPV;
            return way;
        }
        
        // find left-most cache block with max RRPV value
        else if (block.counter_ > max_rrpv) {
            max_rrpv = block.counter_;
            max_way = way;
        }
    }

    // update RRPV values
    for (int way = 0; way < associativity_; ++way) {
        auto block = get_caching_device_block(block_idx, way);
        block.counter_ += (DISTANT_RRPV - max_rrpv);
    }


    int set = compute_set(block_idx);

    bool srrip = is_srrip_leader(set);
    bool brrip = is_brrip_leader(set);

    assert(!(srrip && brrip));

    // update _psel
    if (brrip && (_psel != 0x1FF)) _psel++;
    if (srrip && (_psel != 0x200)) _psel--;

    bool leader = srrip || brrip;
    bool follower = !leader;
    int psel_msb = (_psel >> 9) & 0b1;

    // brrip insertion policy
    if ((leader && brrip) || (follower && psel_msb)) {
        int val = rand() % 32;
        get_caching_device_block(block_idx, max_way).counter_ = val ? DISTANT_RRPV : LONG_RRPV;
    }

    // srrip insertion policy
    if ((leader && srrip) || (follower && !psel_msb)) {
        // set a _distant_ rrpv value for incoming way
        get_caching_device_block(block_idx, max_way).counter_ = DISTANT_RRPV;
    }

    // return line to replace
    return max_way;
}
