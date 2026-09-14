#pragma once
#include "../../../lib/mu_stmlib.h"
#include "grain_source.h"
#include "envelope.h"

namespace peaks
{

    class Grain{
        public:
        void Activate(const GrainSource::Essence &source_essence,
                       const Envelope::Essence &envelope_essence);
        int16_t Synthesize();
        bool IsGrainActive();
        private:

        
        GrainSource source_;
        Envelope envelope_;
        bool is_active_{false};
        
    };
}// namespace peaks