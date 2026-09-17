// Copyright 2026 colinaudergon.
//
// Author: colinaudergon
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <cstddef>

#include "sample_perc_0.h"
#include "sample_perc_1.h"
#include "sample_perc_2.h"
#include "sample_perc_3.h"
#include "sample_perc_4.h"
#include "sample_perc_5.h"
#include "sample_perc_6.h"
#include "sample_perc_7.h"
#include "sample_perc_8.h"
#include "sample_perc_9.h"
#include "sample_perc_10.h"
#include "sample_perc_11.h"

struct sample
{
    const int16_t *samples;
    size_t lenght;
};

constexpr sample kPerc_0{
    .samples = sample_perc_0,
    .lenght = sample_perc_0_length};

constexpr sample kPerc_1{
    .samples = sample_perc_1,
    .lenght = sample_perc_1_length};

constexpr sample kPerc_2{
    .samples = sample_perc_2,
    .lenght = sample_perc_2_length};

constexpr sample kPerc_3{
    .samples = sample_perc_3,
    .lenght = sample_perc_3_length};

constexpr sample kPerc_4{
    .samples = sample_perc_4,
    .lenght = sample_perc_4_length};

constexpr sample kPerc_5{
    .samples = sample_perc_5,
    .lenght = sample_perc_5_length};

constexpr sample kPerc_6{
    .samples = sample_perc_6,
    .lenght = sample_perc_6_length};

constexpr sample kPerc_7{
    .samples = sample_perc_7,
    .lenght = sample_perc_7_length};

constexpr sample kPerc_8{
    .samples = sample_perc_8,
    .lenght = sample_perc_8_length};

constexpr sample kPerc_9{
    .samples = sample_perc_9,
    .lenght = sample_perc_9_length};

constexpr sample kPerc_10{
    .samples = sample_perc_10,
    .lenght = sample_perc_10_length};

constexpr sample kPerc_11{
    .samples = sample_perc_11,
    .lenght = sample_perc_11_length};

constexpr size_t kNsamples = 12;

constexpr sample kSamples[kNsamples] = {
    kPerc_0,
    kPerc_1,
    kPerc_2,
    kPerc_3,
    kPerc_4,
    kPerc_5,
    kPerc_6,
    kPerc_7,
    kPerc_8,
    kPerc_9,
    kPerc_10,
    kPerc_11};