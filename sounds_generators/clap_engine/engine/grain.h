#pragma once
#include "../../../lib/mu_stmlib.h"
#include "grain_source.h"

namespace peaks
{

    class Grain{
        public:
        void Activate(const GrainSource::Essence &essence);
        int16_t Synthesize();
        bool IsGrainActive();
        private:

        
        GrainSource source_;
        bool is_active_{false};
        
    };
}// namespace peaks