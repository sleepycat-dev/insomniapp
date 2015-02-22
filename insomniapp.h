/*
	RackAFX(TM)
	Applications Programming Interface
	Derived Class Object Definition
	Copyright(c) Tritone Systems Inc. 2006-2012

	Your plug-in must implement the constructor,
	destructor and virtual Plug-In API Functions below.
*/

#pragma once

// base class
#include "plugin.h"
#include "sleepOsc.h"
#include "convolution.h"

class Cinsomniapp : public CPlugIn
{
public:
	// RackAFX Plug-In API Member Methods:
	// The followung 5 methods must be impelemented for a meaningful Plug-In
	//
	// 1. One Time Initialization
	Cinsomniapp();

	// 2. One Time Destruction
	virtual ~Cinsomniapp(void);

	// 3. The Prepare For Play Function is called just before audio streams
	virtual bool __stdcall prepareForPlay();

	// 4. processAudioFrame() processes an audio input to create an audio output
	virtual bool __stdcall processAudioFrame(float* pInputBuffer, float* pOutputBuffer, UINT uNumInputChannels, UINT uNumOutputChannels);

	// 5. userInterfaceChange() occurs when the user moves a control.
	virtual bool __stdcall userInterfaceChange(int nControlIndex);


	// OPTIONAL ADVANCED METHODS ------------------------------------------------------------------------------------------------
	// These are more advanced; see the website for more details
	//
	// 6. initialize() is called once just after creation; if you need to use Plug-In -> Host methods
	//				   such as sendUpdateGUI(), you must do them here and NOT in the constructor
	virtual bool __stdcall initialize();

	// 7. joystickControlChange() occurs when the user moves a control.
	virtual bool __stdcall joystickControlChange(float fControlA, float fControlB, float fControlC, float fControlD, float fACMix, float fBDMix);

	// 8. process buffers instead of Frames:
	// NOTE: set m_bWantBuffers = true to use this function
	virtual bool __stdcall processRackAFXAudioBuffer(float* pInputBuffer, float* pOutputBuffer, UINT uNumInputChannels, UINT uNumOutputChannels, UINT uBufferSize);

	// 9. rocess buffers instead of Frames:
	// NOTE: set m_bWantVSTBuffers = true to use this function
	virtual bool __stdcall processVSTAudioBuffer(float** inBuffer, float** outBuffer, UINT uNumChannels, int inFramesToProcess);

	// 10. MIDI Note On Event
	virtual bool __stdcall midiNoteOn(UINT uChannel, UINT uMIDINote, UINT uVelocity);

	// 11. MIDI Note Off Event
	virtual bool __stdcall midiNoteOff(UINT uChannel, UINT uMIDINote, UINT uVelocity, bool bAllNotesOff);


	// 12. MIDI Modulation Wheel uModValue = 0 -> 127
	virtual bool __stdcall midiModWheel(UINT uChannel, UINT uModValue);

	// 13. MIDI Pitch Bend
	//					nActualPitchBendValue = -8192 -> 8191, 0 is center, corresponding to the 14-bit MIDI value
	//					fNormalizedPitchBendValue = -1.0 -> +1.0, 0 is at center by using only -8191 -> +8191
	virtual bool __stdcall midiPitchBend(UINT uChannel, int nActualPitchBendValue, float fNormalizedPitchBendValue);

	// 14. MIDI Timing Clock (Sunk to BPM) function called once per clock
	virtual bool __stdcall midiClock();


	// 15. all MIDI messages -
	// NOTE: set m_bWantAllMIDIMessages true to get everything else (other than note on/off)
	virtual bool __stdcall midiMessage(unsigned char cChannel, unsigned char cStatus, unsigned char cData1, unsigned char cData2);

	// 16. initUI() is called only once from the constructor; you do not need to write or call it. Do NOT modify this function
	virtual bool __stdcall initUI();



	// Add your code here: ----------------------------------------------------------- //
	//data
	int m_nSet;
	float m_fLeftGain;
	float m_fRightGain;
	float m_fCutoff;
	float m_fVolumeDB;
	//HRTF wav file addresses
	//0 = left, 1 = middle, 2 = right
	char * m_cHRTF[3];

	//objects
	//LPF
	CBiQuad m_LPF;
	//convolution objects
	Cconvolution m_ConvL;
	Cconvolution m_ConvR;
	//signal oscillators
	CSleepOsc m_LeftOsc;
	CSleepOsc m_RightOsc;
	CWaveData test;
	//pan LFO
	CWaveTable m_LFO1;

	//functions
	void doDCA();
	void assignFiles();
	void doArraya(float &fInL, float &fInR);
	void calculateLPFCoeffs(float fCutoffFreq, float fQ);
	void cookVolume();
	// END OF USER CODE -------------------------------------------------------------- //


	// ADDED BY RACKAFX -- DO NOT EDIT THIS CODE!!! ----------------------------------- //
	//  **--0x07FD--**

	float m_fLFORate;
	float m_fOsc1Pitch;
	float m_fOsc2Pitch;
	UINT m_uOscSourceL;
	enum{sine,white,pink,car,rain,ocean,tape,vinyl,male,female,user};
	UINT m_uOscSourceR;
	float m_fBrightness;
	float m_fDarken;
	float m_fLFODepth;
	float m_fVolume;
	float m_fOsc1Level;
	float m_fOsc2Level;
	UINT m_uMono;
	enum{SWITCH_OFF,SWITCH_ON};
	UINT m_uStart;
	UINT m_uAutoPan;
	UINT m_uBinaural;

	// **--0x1A7F--**
	// ------------------------------------------------------------------------------- //

};
