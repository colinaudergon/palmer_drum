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
    const size_t n_bins;
    const AudioBin *bins;
};

constexpr sample kPerc_0{
    .samples = sample_perc_0,
    .lenght = sample_perc_0_length,
    .n_bins = sample_perc_0_num_bins,
    .bins = sample_perc_0_bins};

constexpr sample kPerc_1{
    .samples = sample_perc_1,
    .lenght = sample_perc_1_length,
    .n_bins = sample_perc_1_num_bins,
    .bins = sample_perc_1_bins};

constexpr sample kPerc_2{
    .samples = sample_perc_2,
    .lenght = sample_perc_2_length,
    .n_bins = sample_perc_2_num_bins,
    .bins = sample_perc_2_bins};

constexpr sample kPerc_3{
    .samples = sample_perc_3,
    .lenght = sample_perc_3_length,
    .n_bins = sample_perc_3_num_bins,
    .bins = sample_perc_3_bins};

constexpr sample kPerc_4{
    .samples = sample_perc_4,
    .lenght = sample_perc_4_length,
    .n_bins = sample_perc_4_num_bins,
    .bins = sample_perc_4_bins};

constexpr sample kPerc_5{
    .samples = sample_perc_5,
    .lenght = sample_perc_5_length,
    .n_bins = sample_perc_5_num_bins,
    .bins = sample_perc_5_bins};

constexpr sample kPerc_6{
    .samples = sample_perc_6,
    .lenght = sample_perc_6_length,
    .n_bins = sample_perc_6_num_bins,
    .bins = sample_perc_6_bins};

constexpr sample kPerc_7{
    .samples = sample_perc_7,
    .lenght = sample_perc_7_length,
    .n_bins = sample_perc_7_num_bins,
    .bins = sample_perc_7_bins};

constexpr sample kPerc_8{
    .samples = sample_perc_8,
    .lenght = sample_perc_8_length,
    .n_bins = sample_perc_8_num_bins,
    .bins = sample_perc_8_bins};

constexpr sample kPerc_9{
    .samples = sample_perc_9,
    .lenght = sample_perc_9_length,
    .n_bins = sample_perc_9_num_bins,
    .bins = sample_perc_9_bins};

constexpr sample kPerc_10{
    .samples = sample_perc_10,
    .lenght = sample_perc_10_length,
    .n_bins = sample_perc_10_num_bins,
    .bins = sample_perc_10_bins};

constexpr sample kPerc_11{
    .samples = sample_perc_11,
    .lenght = sample_perc_11_length,
    .n_bins = sample_perc_11_num_bins,
    .bins = sample_perc_11_bins};

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