/******************************************************************************
 * GBAudio.h
 * Ken St. Cyr
 * Web: whatskenmaking.com
 * Twitter: @kenstcyr
 * 
 * This library borrows from and is insipired by the work of some talented 
 * developers. In particular, the waveform generation code is largely a port 
 * of the Prentendo project (https://github.com/vlad-the-compiler/pretendo) 
 * with modifications to work with the gameBadge3 MCU and design.
 * 
 * For more information on how to use this library, please see the gameBadge3
 * GitHub repo (https://github.com/benheck/gamebadge3)
******************************************************************************/

#pragma once

#define PULSE1      6                           // GPIO for the Pulse Wave Channel #1
#define PULSE2      8                           // GPIO for the Pulse Wave Channel #2
#define TRIANGLE    10                          // GPIO for the Triangle Wave Channel
#define NOISE       12                          // GPIO for the Noise Channel
#define SAMPLE_RATE 31250
#define PHASE_1HZ   2.097152
#define WAVE_TIMER  -32
#define AUDIO_TIMER -16666

class PulseWave {
    public:
        volatile union {
            uint16_t accumulator;
            struct __attribute__((packed)) {
                byte accuLowByte;
                byte value;
            };
        } phase = { 0 };
        volatile uint16_t phaseDelta = 0;
        volatile byte duty = 128;
        volatile byte volume = 0;

        inline byte synthesize() {
            phase.accumulator += phaseDelta;
            return (phase.value < duty) * volume;
        }
};

class GatedLUTWave {  
    public:
        byte* LUT;
        volatile union {
            uint16_t accumulator;
            struct __attribute__((packed)) {
                byte accuLowByte;
                byte value;
            };
        } phase = { 0 };
        volatile uint16_t phaseDelta = 0;
        volatile bool gate = false;
        volatile byte sample = 0;

        void Gate(bool setGate) {
            // Reset accumulator to sample level 0 when gating to prevent aliasing
            if ((setGate) && (!gate)) {
                phase.accumulator = 32768;
            }
            gate = setGate;
        }

        inline byte synthesize() {
            if (gate) {
                sample = LUT[phase.value];
                phase.accumulator += phaseDelta;
            } else {
                // Fall off to prevent aliasing
                if (sample > 0) {
                    sample--;
                }
            }
            return sample;
        }
};

class LFSR {
    public:
        volatile union {
            uint32_t accumulator;
            struct __attribute__((packed)) {
                uint16_t accuLow;
                uint16_t value;
            };
        } bitClock = { 0 };
        volatile uint16_t cmpBitClock = 0;
        volatile uint32_t bitClockDelta = 0;
        volatile bool shortMode = false;
        volatile uint16_t seed = 1;
        volatile byte feedback;
        volatile bool output;
        volatile byte volume = 0;

        inline byte synthesize() {
            bitClock.accumulator += bitClockDelta;
            if (bitClock.value != cmpBitClock) {
                output = seed & 1;
                if (shortMode) {
                    feedback = output ^ ((seed & 64) >> 6);
                } else {
                    feedback = output ^ ((seed & 2) >> 1);
                }
                seed >>= 1;
                seed |= (feedback << 14);
                cmpBitClock = bitClock.value;
            }
            return output * volume;
        }
};

class GBAudioTrack {
    public:
        byte ReadByte(uint8_t);                 // Read the next 8-bit value from the audio data at the current position
        word ReadWord(uint8_t);                 // Read the next 16-bit value from the audio data at the current position
        byte ReadNextByte();                    // Read the next 8-bit value from the data stream, and advance the pointer
        word ReadNextWord();                    // Read the next 16-bit value from the data stream, and advance the pointer
        bool AtTrackEnd();                      // Indicate if the pointer is at the end of the track
        void Rewind();                          // Go back to the track's loop point
        void Reset();                           // Reset the track to start playing from the beginning
        void Load(const byte*);                 // Load the audio data into the track

        bool playing = false;                   // Indicates if the track is currently playing
        bool loop = false;                      // If true, track repeats when finished

        uint16_t length;                        // Length of the audio data in the track (not including header)
        uint16_t loopPoint;                     // Data position where the track should loop to
        const byte *data;                       // Audio track data
        uint16_t base;                          // Address offset of where the audio data starts
        uint16_t curPos;                        // Current position of the music data being read
        bool loaded = false;                    // Indicates whether track data has been loaded in this slot

        PulseWave Sqr1, Sqr2;
        GatedLUTWave Triangle;
        LFSR Noise;
};

class GBAudio {
    public:
        GBAudio();
        void ProcessWaveforms();
        void ServiceTracks();
        void PlayTrack(uint8_t, bool);
        void PlayTrack(uint8_t);
        void StopTrack(uint8_t);
        void PauseTrack(uint8_t);
        uint8_t AddTrack(const byte *track, uint8_t slotNum);
        void SetVolume(uint8_t);
        uint8_t GetVolume();

        byte masterVolume = 4;
        byte pulseDutyTable[4] = { 32, 64, 128, 192 };
        byte NESTriangleLUT[256];
        byte NESTriangleRefTable[32] = { 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 };
        byte NESDACPulseLUT[31] = { 0, 3, 6, 9, 11, 14, 17, 19, 22, 24, 27, 29, 31, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 57, 59, 61, 62, 64, 66 };
        byte NESDACTriNoiseDMCLUT[203] = { 0, 2, 3, 5, 7, 8, 10, 12, 13, 15, 16, 18, 20, 21, 23, 24, 26, 27, 29, 30, 32, 33, 35, 36, 37, 39, 40, 42, 43, 44, 46, 47, 49, 50, 51, 52, 54, 55, 56, 58, 59, 60, 61, 63, 64, 65, 66, 68, 69, 70, 71, 72, 73, 75, 76, 77, 78, 79, 80, 81, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 115, 116, 117, 118, 119, 120, 121, 122, 122, 123, 124, 125, 126, 127, 127, 128, 129, 130, 131, 132, 132, 133, 134, 135, 136, 136, 137, 138, 139, 139, 140, 141, 142, 142, 143, 144, 145, 145, 146, 147, 148, 148, 149, 150, 150, 151, 152, 152, 153, 154, 155, 155, 156, 157, 157, 158, 159, 159, 160, 160, 161, 162, 162, 163, 164, 164, 165, 166, 166, 167, 167, 168, 169, 169, 170, 170, 171, 172, 172, 173, 173, 174, 175, 175, 176, 176, 177, 177, 178, 179, 179, 180, 180, 181, 181, 182, 182, 183, 184, 184, 185, 185, 186, 186, 187, 187, 188, 188, 189, 189 };
        uint32_t NESLFSRFrequencyTable[16] = { 447443, 223722, 111861, 55930, 27965, 18643, 13983, 11186, 8860, 7046, 4710, 3523, 2349, 1762, 880, 440 };       

        GBAudioTrack tracks[16];
};