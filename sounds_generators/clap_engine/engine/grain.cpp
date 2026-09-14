#include "grain.h"

namespace peaks
{

    void Grain::Activate(const GrainSource::Essence &essence)
    {
        source_.Init(essence);
        is_active_ = true;
    }

    int16_t Grain::Synthesize()
    {
        int16_t sample = source_.Synthesize();
        if (source_.Done())
        {
            is_active_ = false;
        }
        return sample;
    }

    bool Grain::IsGrainActive()
    {
        return is_active_;
    }

} // namespace peaks