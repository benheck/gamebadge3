#include "FamiPlayer.h"
#include <gameBadgePico.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

FamiPlayer::FamiPlayer() 
{
    seed = get_seed();
}

FamiPlayer::~FamiPlayer() 
{

}

void FamiPlayer::serviceTracks()
{   
    for (int i = 0; i < tracks.size(); i++)
    {
        if (tracks[i].playing)
        {
            int noteCtr = tracks[i].noteCounter;
            tracks[i].wasPlaying = true;

            if (tracks[i].chPulse1 != nullptr)                              // There's data on the PULSE1 channel
            {
                uint16_t note = tracks[i].chPulse1[noteCtr] >> 4;           // Cleave the volume bits, result is the note index
                uint16_t volume = tracks[i].chPulse1[noteCtr] & 0x000F;     // Grab the volume bits
                uint16_t freq = noteFreq[note];                             // Convert the note index to the correct frequency
                uint8_t dutyCycle = volume * 3 + (volume > 0 ? 5 : 0);          // Set the volume by adjusting the duty cycle
                pwm_set_freq_duty(PULSE1, freq, dutyCycle);
            }
            if (tracks[i].chPulse2 != nullptr)                              // There's data on the PULSE2 channel
            {
                uint16_t note = tracks[i].chPulse2[noteCtr] >> 4;
                uint16_t volume = tracks[i].chPulse2[noteCtr] & 0x000F;
                uint16_t freq = noteFreq[note];
                uint8_t dutyCycle = volume * 3 + (volume > 0 ? 5 : 0);
                pwm_set_freq_duty(PULSE2, freq, dutyCycle);
            }
            if (tracks[i].chTriangle != nullptr)                            // There's data on the TRIANGLE channel
            {
                uint16_t note = tracks[i].chTriangle[noteCtr] >> 4;
                triangleNote = noteFreq[note];
            }
            if (tracks[i].chNoise != nullptr)                               // There's data on the NOISE channel
            {
                uint16_t note = tracks[i].chNoise[noteCtr] >> 4;
                uint16_t volume = tracks[i].chNoise[noteCtr] & 0x000F;
                noiseVolume = volume;
                noiseNote = noiseMap[note];
            }
            tracks[i].noteCounter = (tracks[i].noteCounter + 1) % tracks[i].numNotes;
            if (tracks[i].noteCounter == 0 && !tracks[i].loop) tracks[i].playing = false;
        } else if (tracks[i].wasPlaying) {
            tracks[i].wasPlaying = false;
            pwm_set_freq_duty(PULSE1, 0, 0);
            pwm_set_freq_duty(PULSE2, 0, 0);
            triangleNote = 0;
            noiseNote = 0;
        }
    }
}

uint16_t FamiPlayer::AddTrack(uint16_t *chPulse1, uint16_t *chPulse2, uint16_t *chTriangle, uint16_t *chNoise, int size, bool loop)
{
    FamiTrack f;
    f.chPulse1 = chPulse1;
    f.chPulse2 = chPulse2;
    f.chTriangle = chTriangle;
    f.chNoise = chNoise;

    f.numNotes = size;                          // Number of notes in the track
    f.loop = loop;                              // Loop track?
    tracks.push_back(f);                        // Add the track to the track list

    return tracks.size() - 1;
}


void FamiPlayer::PlayTrack(int trackNum)
{
    tracks[trackNum].playing = true;
}

void FamiPlayer::StopTrack(int trackNum)
{
    tracks[trackNum].playing = false;
    tracks[trackNum].noteCounter = 0;
}

void FamiPlayer::ProcessWave()
{
    if (triangleNote > 0)                                                   // We need to play a triangle note
    {
        if (triangleNotePrev == 0) {                                        // First cycle of note being played
            triangleFreqCounter = 125000/triangleNote/100;                  // Set the frequency counter w/ 100 step resolution
            triangleNotePrev = triangleNote;                                // Set so we skip this next time
        }
        
        if (triangleFreqCounter-- == 0)                                     // Frequency counter has rolled over
        {
            uint8_t triDuty = triangleMap[(triangleIndex++)%100];           // Grab the duty cycle from the waveform map
            uint8_t triDutyAdjusted = (triDuty*triangleVolume)/100;         // Adjust duty cycle for volume compensation
            pwm_set_freq_duty(TRIANGLE, triangleNote, triDutyAdjusted);     // Play note
            triangleFreqCounter = 125000/triangleNote/100;                  // Reset the frequency counter
        }
    } else {
        triangleNotePrev = 0;                                               // Make sure we reset the frequency counter when a note plays again
        pwm_set_freq_duty(TRIANGLE, 0, 0);                                  // Stop playing any sound
    }

    if (noiseNote > 0)                                                      // We need to play some noise
    {
        if (noiseNotePrev == 0) {                                           // First cycle of noise note being played
            noiseFreqCounter = 125000/noiseNote;                            // Set the frequency counter of the noise generator
            noiseNotePrev = noiseNote;                                      // Set this so we skip this next time
        }

        if (noiseFreqCounter-- == 0)                                        // Frequency counter has rolled over
        {
            if (get_noise()) {                                              // If we generate non-zero noise
                pwm_set_freq_duty(NOISE, noiseNote, noiseVolume);           // Play it
            } else {
                pwm_set_freq_duty(NOISE, 0, 0);                             // Otherwise, mute the channel
            }
            noiseFreqCounter = 125000/noiseNote;                            // Reset the noise frequency counter
        }
    }
}

uint16_t FamiPlayer::get_seed() {
    adc_init();
    adc_gpio_init(28);
    adc_select_input(4); // Use channel 4 which corresponds to GP28

    return adc_read();
}

uint8_t FamiPlayer::get_noise()
{
  uint32_t r = seed & 0x00000001;
  seed >>= 1;
  r ^= seed & 0x00000001;
  seed |= r << 31;
  return r;
}
