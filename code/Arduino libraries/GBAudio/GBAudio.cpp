/******************************************************************************
 * GBAudio.cpp
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

#include <gameBadgePico.h>
#include "GBAudio.h"

GBAudio::GBAudio()
{
    int kLUT = 0;
    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 8; j++) {
            NESTriangleLUT[kLUT++] = NESTriangleRefTable[i];
        }
    }
}

void GBAudio::ProcessWaveforms()
{   
    volatile byte s1 = 0, s2 = 0, tri = 0;
    volatile word noise = 0;

    for (int i = 0; i < 16; i++) 
    {
        // Make sure that we only process loaded tracks that are playing
        if (!tracks[i].loaded || !tracks[i].playing) continue;

        s1 += tracks[i].Sqr1.synthesize();
        s2 += tracks[i].Sqr2.synthesize();
        tri += tracks[i].Triangle.synthesize() * 3;
        noise += tracks[i].Noise.synthesize() * 2;
    }
    pwm_set_freq_duty(PULSE1, SAMPLE_RATE, NESDACPulseLUT[s1] * masterVolume);
    pwm_set_freq_duty(PULSE2, SAMPLE_RATE, NESDACPulseLUT[s2] * masterVolume);
    pwm_set_freq_duty(TRIANGLE, SAMPLE_RATE, NESDACTriNoiseDMCLUT[tri + noise] * masterVolume);
    pwm_set_freq_duty(NOISE, SAMPLE_RATE, NESDACTriNoiseDMCLUT[tri + noise] * masterVolume);
    
}

void GBAudio::ServiceTracks()
{
    for (int i = 0; i < 16; i++)
    {
        if (!tracks[i].loaded) continue;            // Make sure the track is loaded first

        if (tracks[i].playing) {
            // Check if we're at the end of the track, and either stop the track or loop it
            if (tracks[i].AtTrackEnd()) {
                if (tracks[i].loop)
                    tracks[i].Rewind();     // Rewind the track
                else {
                    tracks[i].Reset();      // Reset the trck
                    continue;               // Skip to the next track
                }
            }

            byte control = tracks[i].ReadNextByte();
            bool sqr1FFlag = (control & 1);             // Bit 0 = square 1 note present
            bool sqr2FFlag = (control & 2);             // Bit 1 = square 2 note present 
            bool triangleFFlag = (control & 4);         // Bit 2 = triangle note present
            bool sqr1ParamFlag = (control & 8);         // Bit 3 = square 1 parameter present
            bool sqr2ParamFlag = (control & 16);        // Bit 4 = square 2 parameter present
            bool noiseParamFlag = (control & 32);       // Bit 5 = noise note present
            bool triangleActiveFlag = (control & 64);   // Bit 6 = ?
            bool extendedFlag = (control & 128);        // Bit 7 = Extended noise + DMC data present

            if (sqr1FFlag) {
                tracks[i].Sqr1.phaseDelta = tracks[i].ReadNextWord() * PHASE_1HZ;
            }
            if (sqr2FFlag) {
                tracks[i].Sqr2.phaseDelta = tracks[i].ReadNextWord() * PHASE_1HZ;
            }
            if (triangleFFlag) {
                tracks[i].Triangle.phaseDelta = tracks[i].ReadNextWord() * PHASE_1HZ;
                if (tracks[i].Triangle.phaseDelta > 10000) {
                    tracks[i].Triangle.phaseDelta = 0;
                }
            }
            if (sqr1ParamFlag) {
                byte data = tracks[i].ReadNextByte();               // Next byte is square 1 flag
                tracks[i].Sqr1.volume = data & 0x0F;                // Lower 4 bits is volume
                tracks[i].Sqr1.duty = pulseDutyTable[data >> 4];    // Upper 4 bits is duty cycle
            }
            if (sqr2ParamFlag) {
                byte data = tracks[i].ReadNextByte();               // Next byte is square 2 flag
                tracks[i].Sqr2.volume = data & 0x0F;                // Lower 4 bits is volume
                tracks[i].Sqr2.duty = pulseDutyTable[data >> 4];    // Upper 4 bits is duty cycle
            }
            if (noiseParamFlag) {
                byte data = tracks[i].ReadNextByte();                                           // Read the next byte as a noise parameter
                tracks[i].Noise.volume = data & 0x0F;                                           // Lower 4 bits is volume
                tracks[i].Noise.bitClockDelta = NESLFSRFrequencyTable[data >> 4] * PHASE_1HZ;   // Upper 4 bits is lookup in noise freq table
            }
            tracks[i].Triangle.Gate(triangleActiveFlag); 
            if (extendedFlag) {                             // If there is extended data
                byte data = tracks[i].ReadNextByte();       // Read the next byte as extended data
                tracks[i].Noise.shortMode = (data >> 7);    // Bit 7 = noise mode
                bool dmcFlag = ((data & 64) >> 6);          // Bit 6 = DMC flag
                if (dmcFlag) tracks[i].curPos += 2;         // We're not implementing DMC (yet), so if the DMC flag is set, just advance the pointer past it  
            }
        }
    }
}

uint8_t GBAudio::AddTrack(const byte *track, uint8_t num)
{
    tracks[num].Triangle.LUT = NESTriangleLUT;
    tracks[num].Load(track);
    return num;                             // Return the slot number to the user
}

void GBAudio::PlayTrack(uint8_t trackNum, bool doloop)
{
    if (!tracks[trackNum].loaded) return;

    tracks[trackNum].loop = doloop;
    tracks[trackNum].Reset();
    tracks[trackNum].playing = true;
}

void GBAudio::PlayTrack(uint8_t trackNum)
{
    if (!tracks[trackNum].loaded) return;
    tracks[trackNum].loop = false;
    tracks[trackNum].Reset();
    tracks[trackNum].playing = true;
}

void GBAudio::StopTrack(uint8_t num)
{
    if (tracks[num].loaded && tracks[num].playing)
    {
        tracks[num].Reset();
    }
}

void GBAudio::PauseTrack(uint8_t num)
{
    if (!tracks[num].loaded) return;

    tracks[num].playing = !tracks[num].playing;
}

uint8_t GBAudio::GetVolume()
{
    return masterVolume;
}

void GBAudio::SetVolume(uint8_t vol)
{
    masterVolume = vol > 9 ? 9 : vol;
}

void GBAudioTrack::Load(const byte* track)
{
    data = track;                           // Set the data stream
    curPos = 0;                             // Reset current position
    length = ReadNextWord();                // Read length from the header
    loopPoint = ReadNextWord();             // Read loop point from the header
    curPos++;                               // Skip byte 4 (region data)
    uint8_t dmcSamples = ReadNextByte();    // Next byte is # DMC samples

    // We're not using the DPCM track, so here we skip passed all the DMC samples
    uint16_t currentFileOffset = 6 + dmcSamples * 2;
    for (int i = 0; i < dmcSamples; i++) {
        uint16_t dmcLengths =  ReadWord(6 + i * 2);
        currentFileOffset += dmcLengths;
    }

    base = currentFileOffset;   // Set the address of the start of the actual audio data
    curPos = base;              // Set the current position to the base location
    loaded = true;              // We've loaded data now in this slot
}

byte GBAudioTrack::ReadByte(uint8_t offset = 0)
{
    return data[curPos + offset];
}

word GBAudioTrack::ReadWord(uint8_t offset = 0)
{
    return ReadByte() * 256 + ReadByte(1);
}

byte GBAudioTrack::ReadNextByte()
{
    byte b = ReadByte();
    curPos++;
    return b;
}

word GBAudioTrack::ReadNextWord()
{
    return ReadNextByte() * 256 + ReadNextByte();
}

void GBAudioTrack::Reset()
{
    curPos = base;
    playing = false;
}

void GBAudioTrack::Rewind()
{
    curPos = base + loopPoint;
}

bool GBAudioTrack::AtTrackEnd()
{
    return curPos - base >= length ? true : false;
}