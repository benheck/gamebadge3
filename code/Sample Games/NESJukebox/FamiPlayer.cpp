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
    for (int i = 0; i < NUM_TRACKS; i++) {
        if (tracks[i].playing) {                                            // The track is currently playing...
            int noteBufIdx = tracks[i].curBufCtr;                           // Put the current buffer index in a local variable... just for code readability
            tracks[i].wasPlaying = true;                                    // Indicate that the track has already started playing
            uint16_t note, volume, freq, duty;                              // Local vars

            // Play Pulse Channel 1
            note = tracks[i].chPulse1[noteBufIdx] >> 4;                     // Cleave the volume bits, result is the note index
            volume = tracks[i].chPulse1[noteBufIdx] & 0x000F;               // Grab the volume bits
            freq = noteFreq[note];                                          // Convert the note index to the correct frequency
            duty = volume * 3 + (volume > 0 ? 5 : 0);                       // Set the volume by adjusting the duty cycle
            pwm_set_freq_duty(PULSE1, freq, duty);
            
            // Play Pulse Channel 2
            note = tracks[i].chPulse2[noteBufIdx] >> 4;
            volume = tracks[i].chPulse2[noteBufIdx] & 0x000F;
            freq = noteFreq[note];
            duty = volume * 3 + (volume > 0 ? 5 : 0);
            pwm_set_freq_duty(PULSE2, freq, duty);
            
            // Play Triangle Channel
            note = tracks[i].chTriangle[noteBufIdx] >> 4;
            triangleNote = noteFreq[note];
            
            // Play Noise Channel
            note = tracks[i].chNoise[noteBufIdx] >> 4;
            volume = tracks[i].chNoise[noteBufIdx] & 0x000F;
            noiseVolume = volume;
            noiseNote = noiseMap[note];
            
            tracks[i].curBufCtr++;                                          // Increment the counter for the current buffer
            tracks[i].curNote++;                                            // Increment the note counter for the entire track
            if (tracks[i].curNote == tracks[i].numNotes)                    // We've reached the end of the track
                if (!tracks[i].loop)                                        // Loop is not enabled
                    tracks[i].playing = false;                              // So stop playing the track
                else {                                                      // Loop is enabled, so restart the track
                    tracks[i].curBufCtr = 0;                                // Reset the counter for the current buffer
                    tracks[i].curNote = 0;                                  // Reset the note counter for the entire track
                    tracks[i].bufferIndex = 0;                              // Reset the buffer counter
                    FillBuffer(&tracks[i]);                                 // Refill the buffer with the beginning of the track
                }
            else if (tracks[i].curBufCtr == BUF_SZ) {                       // We've reached the end of the buffer                  
                tracks[i].curBufCtr = 0;                                    // Reset the counter for the current buffer
                FillBuffer(&tracks[i]);                                     // And fill the buffer back up
            }
        } 
        else if (tracks[i].wasPlaying) {                                    // The track just finished playing
            tracks[i].wasPlaying = false;                                   // Reset the track history trigger
            pwm_set_freq_duty(PULSE1, 0, 0);                                // Make sure Pulse 1 channel is stopped
            pwm_set_freq_duty(PULSE2, 0, 0);                                // Make sure Pulse 2 channel is stopped
            triangleNote = 0;                                               // Make sure Triangle channel is stopped
            noiseNote = 0;                                                  // Make sure Noise channel is stopped
            tracks[i].curBufCtr = 0;                                        // Reset the counter for the current buffer
            tracks[i].curNote = 0;                                          // Reset the note counter for the entire track
            tracks[i].bufferIndex = 0;                                      // Reset the buffer counter
        }
    }
}

uint16_t FamiPlayer::AddTrack(const char *filename, uint8_t slot, bool loop)
{
    FatFile audioFile;
    if (!audioFile.open(filename, O_RDONLY)) return 0;

    // Make sure we don't overflow the track array
    if (slot > NUM_TRACKS - 1) return 0;

    tracks[slot].loop = loop;
    tracks[slot].fileName = filename;
    tracks[slot].curNote = 0;
    tracks[slot].curBufCtr = 0;
    tracks[slot].bufferIndex = 0;

    // Read the number of notes (first 4 bytes) and close the file
    audioFile.read(&tracks[slot].numNotes, 4);
    audioFile.close();

    return 1;
}


void FamiPlayer::PlayTrack(int trackNum)
{
    if (trackNum >= NUM_TRACKS) return;
    
    tracks[trackNum].curNote = 0;
    tracks[trackNum].curBufCtr = 0;
    tracks[trackNum].bufferIndex = 0;

    FillBuffer(&tracks[trackNum]);
    tracks[trackNum].playing = true;
}

void FamiPlayer::StopTrack(int trackNum)
{
    if (trackNum >= NUM_TRACKS) return;

    tracks[trackNum].playing = false;
    tracks[trackNum].curBufCtr = 0;
}

bool FamiPlayer::IsPlaying(int trackNum)
{
    if (trackNum >= NUM_TRACKS) return false;

    return tracks[trackNum].playing;
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
    adc_init();                     // Initialzie the ADC
    adc_gpio_init(28);              // Initialize GPIO 28 (ADC2)
    adc_select_input(2);            // Use channel 2 which corresponds to GP28
    return adc_read();              // Read ADC value (randomness)
}

uint8_t FamiPlayer::get_noise()
{
  uint32_t r = seed & 0x00000001;
  seed >>= 1;
  r ^= seed & 0x00000001;
  seed |= r << 31;
  return r;
}

void FamiPlayer::FillBuffer(FamiTrack* pF)
{
    FatFile audioFile;
    if (!audioFile.open(pF->fileName, O_RDONLY)) return;

    int bufferLeft = 0;                                                 // Assume we're going to fill the whole buffer
    int readlen = BUF_SZ * 2;                                           // Default to reading the buffer size * 2 (16-bit uint)
    if (pF->numNotes < BUF_SZ) {                                        // We're almost at the end of the file
        readlen = pF->numNotes * 2;                                     // So only read the part that's left, not the whole buffer
        bufferLeft = BUF_SZ - pF->numNotes;                             // Amount of buffer left that needs to be zeroed out 
    }    

    audioFile.seekSet(pF->bufferIndex + HEAD_SZ);                       // Skip ahead past the file header
    audioFile.read(pF->chPulse1, readlen);                              // Fill the Pulse 1 channel buffer                 

    audioFile.seekSet(pF->numNotes * 2 + pF->bufferIndex + HEAD_SZ);    // Jump to the start of Pulse 2
    audioFile.read(pF->chPulse2, readlen);                              // Fill the Pulse 2 channel buffer

    audioFile.seekSet(pF->numNotes * 4 + pF->bufferIndex + HEAD_SZ);    // Jump to the start of Triangle
    audioFile.read(pF->chTriangle, readlen);                            // Fill the Triangle channel buffer

    audioFile.seekSet(pF->numNotes * 6 + pF->bufferIndex + HEAD_SZ);    // Jump to the start of Noise
    audioFile.read(pF->chNoise, readlen);                               // Fill the Noise channel buffer

    audioFile.close();                                                  // Close the audio file

    for (int i = 0; i < bufferLeft; i++) {                              // We've run out of file to buffer, so zero out remainder of buffer
        int e = BUF_SZ - i - 1;                                         // Count back from the end of the buffer
        pF->chPulse1[e] = 0;                                            // Zero out Pulse 1 channel buffer
        pF->chPulse2[e] = 0;                                            // Zero out Pulse 2 channel buffer
        pF->chTriangle[e] = 0;                                          // Zero out Triangle channel buffer
        pF->chNoise[e] = 0;                                             // Zero out Noise channel buffer
    }
    pF->bufferIndex = pF->bufferIndex + readlen;                        // Advance the file buffer index
}