#ifndef SMALL_ECO_DESTROYED_SOUND_SAMPLES_HPP
#define SMALL_ECO_DESTROYED_SOUND_SAMPLES_HPP

#include <stdint.h>

enum class SoundSampleName
{
    None = 0,
    Explosion,
    Explosion2,
    Explosion3,
    Explosion4,

    Pick,
    Pick2,

    Shot,
    Shot2,
    Shot3,

    Count,

    ExplosionCount = 4,
    PickCount = 2,
    ShotCount = 3,

};

void playSoundSample(SoundSampleName name);

inline SoundSampleName normalizeSampleNameIndex(SoundSampleName base, SoundSampleName count, uint32_t index)
{
    return SoundSampleName(uint32_t(base) + index % uint32_t(count));
}

inline void playExplosionSound(uint32_t index)
{
    playSoundSample(normalizeSampleNameIndex(SoundSampleName::Explosion, SoundSampleName::ExplosionCount, index));
}

inline void playShotSound(uint32_t index)
{
    playSoundSample(normalizeSampleNameIndex(SoundSampleName::Shot, SoundSampleName::ShotCount, index));
}

inline void playPickSound(uint32_t index)
{
    playSoundSample(normalizeSampleNameIndex(SoundSampleName::Pick, SoundSampleName::PickCount, index));
}

#endif //SMALL_ECO_DESTROYED_SOUND_SAMPLES_HPP
