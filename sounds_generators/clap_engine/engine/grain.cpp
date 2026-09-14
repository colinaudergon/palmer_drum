#include "grain.h"

namespace peaks
{

    void Grain::Activate(const GrainSource::Essence &source_essence,
                          const Envelope::Essence &envelope_essence)
    {
        source_.Init(source_essence);
        envelope_.Init(envelope_essence);
        is_active_ = true;
    }

    int16_t Grain::Synthesize()
    {
        int16_t sample = source_.Synthesize();
        uint32_t gain = envelope_.Synthesize();
        int16_t enveloped = static_cast<int16_t>(
            (static_cast<int32_t>(sample) * static_cast<int32_t>(gain)) >> 16);

        if (source_.Done() || envelope_.Done())
        {
            is_active_ = false;
        }
        return enveloped;
    }

    bool Grain::IsGrainActive()
    {
        return is_active_;
    }

} // namespace peaks