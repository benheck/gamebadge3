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

//BJH MATH PASS 3-26-23

GBAudio::GBAudio()
{
    int kLUT = 0;
    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 8; j++) {
            NESTriangleLUT[kLUT++] = NESTriangleRefTable[i];
        }
    }

    for (uint8_t i = 0; i < CHANNELS; i++) playStack[i] = 0x80;  // Set up the play stack
}

void GBAudio::ProcessWaveforms()				//Called really fast (64us minimum)
{   
	gpio_put(15, 1);
	
    byte s1 = 0, s2 = 0, tri = 0;
    word noise = 0, dpcm = 0;

    for (uint8_t c = 0; c < CHANNELS; c++)
    {
        uint8_t i = playStack[c];
        if (i & 0x80) continue;

        // Make sure that we only process loaded tracks that are playing
        if (!tracks[i].playing) continue;

        s1 += tracks[i].Sqr1.synthesize();
        s2 += tracks[i].Sqr2.synthesize();
        tri += tracks[i].Triangle.synthesize() << 2;
        noise += tracks[i].Noise.synthesize() << 1;
        dpcm += tracks[i].DPCM.synthesize();
    }
	
	//Special PWM function that uses a constant FREQ for far less math BJH 3-26-23
    pwm_set_freq_music(0, NESDACPulseLUT[s1] * masterVolume);
    pwm_set_freq_music(1, NESDACPulseLUT[s2] * masterVolume);
    pwm_set_freq_music(2, NESDACTriNoiseDMCLUT[tri + noise] * masterVolume);
    pwm_set_freq_music(3, NESDACTriNoiseDMCLUT[dpcm] * masterVolume);	
	
	gpio_put(15, 0);
}

void GBAudio::ServiceTracks()
{
	//gpio_put(15, 1);
	
    for (uint8_t c = 0; c < CHANNELS; c++)
    {
        uint8_t i = playStack[c];
        if (i & 0x80) continue;

        // Check if the track is playing, in case it's paused
        if (tracks[i].playing) {
            // Check if we're at the end of the track, and either stop the track or loop it
            if (tracks[i].AtTrackEnd()) {
                if (tracks[i].loop)
                    tracks[i].Rewind();     // Rewind the track
                else {
                    tracks[i].queued = 0x80;
                    tracks[i].Reset();
                    playStack[c] = 0x80;
                    continue;               // Skip to the next track
                }
            }

            volatile byte control = tracks[i].ReadNextByte();      

			if (control == 0) {			//Null line? Don't processs bits
				continue;
			}
			
			// Bit 0 = square 1 note present
            if (control & 0x01) {
                tracks[i].Sqr1.phaseDelta = tracks[i].ReadNextWord() * PHASE_1HZ;
            }

			// Bit 1 = square 2 note present
            if (control & 0x02) {
                tracks[i].Sqr2.phaseDelta = tracks[i].ReadNextWord() * PHASE_1HZ;
            }
			
            // Bit 2 = triangle note present
            if (control & 0x04) {
                tracks[i].Triangle.phaseDelta = tracks[i].ReadNextWord() * PHASE_1HZ;
                if (tracks[i].Triangle.phaseDelta > 10000) {
                    tracks[i].Triangle.phaseDelta = 0;
                }
            }

			// Bit 3 = square 1 parameter present
            if (control & 0x08) {
                byte data = tracks[i].ReadNextByte();               // Next byte is square 1 flag
                tracks[i].Sqr1.volume = data & 0x0F;                // Lower 4 bits is volume
                tracks[i].Sqr1.duty = pulseDutyTable[data >> 4];    // Upper 4 bits is duty cycle
            }
			
            // Bit 4 = square 2 parameter present
            if (control & 0x10) {
                byte data = tracks[i].ReadNextByte();               // Next byte is square 2 flag
                tracks[i].Sqr2.volume = data & 0x0F;                // Lower 4 bits is volume
                tracks[i].Sqr2.duty = pulseDutyTable[data >> 4];    // Upper 4 bits is duty cycle
            }

			// Bit 5 = noise note present
            if (control & 0x20) {
                byte data = tracks[i].ReadNextByte();                                           // Read the next byte as a noise parameter
                tracks[i].Noise.volume = data & 0x0F;                                           // Lower 4 bits is volume
                tracks[i].Noise.bitClockDelta = NESLFSRFrequencyTable[data >> 4] * PHASE_1HZ;   // Upper 4 bits is lookup in noise freq table
            }

			// Bit 6 = ?
            tracks[i].Triangle.Gate(control & 0x40); 
			
			// Bit 7 = Extended noise + DMC data present
            if (control & 0x80) {                           // If there is extended data
                byte data = tracks[i].ReadNextByte();       // Read the next byte as extended data
                tracks[i].Noise.shortMode = (data >> 7);    // Bit 7 = noise mode
                bool dmcFlag = ((data & 64) >> 6);          // Bit 6 = DMC flag
                bool dmcKillFlag = ((data & 32) >> 5);      // Bit 5 = DMC Kill Flag
                if (dmcFlag) {                              // If the DMC flag is set...  
                    data =tracks[i].ReadNextByte();         //   Read in next byte as DMC data
                    byte dmcIndex = data & 0x0F;            //   Lower 4 bits is DMC index
                    byte fDMCIndex = data >> 4;             //   Upper 4 bits is DMC frequency index?
                    data = tracks[i].ReadNextByte();        //   Read in next byte
                    byte dmcSeed = data & 127;              //   Lower 7 bits is the DMC seed
                    bool dmcLoop = data >> 7;               //   Upper bit indicates DMC looping
                    byte* dmcAddr = (byte*)(tracks[i].data + tracks[i].dmcOffsets[dmcIndex]);         //   Set the DMC address start
                    tracks[i].DPCM.play(dmcAddr, tracks[i].dmcLengths[dmcIndex], fDMCIndex, dmcSeed, dmcLoop);  //   Play DMC
                }
                if (dmcKillFlag) {                          // Should we kill the DMC?
                    tracks[i].DPCM.kill();                  // Kill the DMC
                }
            }
        }
    }
	
	//gpio_put(15, 0);
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
    if (!(tracks[trackNum].queued & 0x80)) { 
        tracks[trackNum].curPos = tracks[trackNum].base; 
        return; 
    }

    tracks[trackNum].loop = doloop;
    tracks[trackNum].Reset();

    for (uint8_t i = 0; i < CHANNELS; i++) {
        if (playStack[i] & 0x80) {
            playStack[i] = trackNum;
            tracks[trackNum].queued = i;
            tracks[trackNum].playing = true;
            return;
        }
    }
}

void GBAudio::PlayTrack(uint8_t trackNum)
{
    if (!tracks[trackNum].loaded) return;
    if (!(tracks[trackNum].queued & 0x80)) { 
        tracks[trackNum].curPos = tracks[trackNum].base; 
        return;
    }

    tracks[trackNum].loop = false;
    tracks[trackNum].Reset();

    for (uint8_t i = 0; i < CHANNELS; i++) {
        if (playStack[i] & 0x80) {
            playStack[i] = trackNum;
            tracks[trackNum].queued = i;
            tracks[trackNum].playing = true;
            return;
        }
    }
}

void GBAudio::StopTrack(uint8_t num)
{   
    uint8_t i = tracks[num].queued;
    if (i & 0x80) return;
    
    playStack[i] = 0x80;
    tracks[num].queued = 0x80;
    tracks[num].Reset();
}

void GBAudio::PauseTrack(uint8_t num)
{
    tracks[num].playing = !tracks[num].playing;
}

bool GBAudio::IsTrackPlaying(uint8_t num) {
	
	return tracks[num].playing;
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
    length = ReadWord(0);                   // Read length from the header
    loopPoint = ReadWord(2);                // Read loop point from the header
    dmcSamples = ReadByte(5);               // Next byte is # DMC samples

    // skip past the DMC samples and set the position as the current file offset. Adding 6 to skip over the 6 bytes of header we just read in
    uint16_t currentFileOffset = 6 + (dmcSamples << 1);

    // If there are DMC Samples to be read, read those in and adjust the current file offsets accordingly
    for (int i = 0; i < dmcSamples; i++) {
        dmcLengths[i] =  ReadWord(6 + (i << 1));
        dmcOffsets[i] = currentFileOffset;
        currentFileOffset += dmcLengths[i];
    }

    base = currentFileOffset;   // Set the address of the start of the actual audio data
    curPos = base;              // Set the current position to the base location
    loaded = true;              // We've loaded data now in this slot
}

byte GBAudioTrack::ReadByte(uint16_t offset = 0)
{
    return data[curPos + offset];
}

word GBAudioTrack::ReadWord(uint16_t offset = 0)
{
    return (ReadByte(offset) << 8) | ReadByte(offset + 1);
}

byte GBAudioTrack::ReadNextByte()
{
    byte b = ReadByte();
    curPos++;
    return b;
}

word GBAudioTrack::ReadNextWord()
{
    return (ReadNextByte() << 8) | ReadNextByte();
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