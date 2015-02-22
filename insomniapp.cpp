/*
	RackAFX(TM)
	Applications Programming Interface
	Derived Class Object Implementation
*/


#include "insomniapp.h"


/* constructor()
	You can initialize variables here.
	You can also allocate memory here as long is it does not
	require the plugin to be fully instantiated. If so, allocate in init()

*/
Cinsomniapp::Cinsomniapp()
{
	// Added by RackAFX - DO NOT REMOVE
	//
	// initUI() for GUI controls: this must be called before initializing/using any GUI variables
	initUI();
	// END initUI()

	// built in initialization
	m_PlugInName = "insomniapp";

	// Default to Stereo Operation:
	// Change this if you want to support more/less channels
	m_uMaxInputChannels = 2;
	m_uMaxOutputChannels = 2;

	// use of MIDI controllers to adjust sliders/knobs
	m_bEnableMIDIControl = true;		// by default this is enabled

	// custom GUI stuff
	m_bLinkGUIRowsAndButtons = false;	// change this if you want to force-link

	// DO NOT CHANGE let RackAFX change it for you; use Edit Project to alter
	m_bUseCustomVSTGUI = true;

	// for a user (not RackAFX) generated GUI - advanced you must compile your own resources
	// DO NOT CHANGE let RackAFX change it for you; use Edit Project to alter
	m_bUserCustomGUI = false;

	// output only - SYNTH - plugin DO NOT CHANGE let RackAFX change it for you; use Edit Project to alter
	m_bOutputOnlyPlugIn = true;

	// change to true if you want all MIDI messages
	m_bWantAllMIDIMessages = false;

	// un-comment this for VST/AU Buffer-style processing
	// m_bWantVSTBuffers = true;

	// Finish initializations here
	m_fCutoff = 18000;

	char * cWavPath = addStrings(getMyDLLDirectory(),"\\sleep");
	m_LeftOsc.setSamplePath(cWavPath);
	m_RightOsc.setSamplePath(cWavPath);
	m_LeftOsc.init();
	m_RightOsc.init();
	m_LFO1.m_uPolarity = 0;
	m_LFO1.setSampleRate(m_nSampleRate);

	m_cHRTF[0] = m_LeftOsc.concatStrings(cWavPath, "\\HRTF\\HRTF_LEFT"); //left
	m_cHRTF[1] = m_LeftOsc.concatStrings(cWavPath, "\\HRTF\\HRTF_CENTER"); //middle
	m_cHRTF[2] = m_LeftOsc.concatStrings(cWavPath, "\\HRTF\\HRTF_RIGHT"); //right

	//delete cWavPath;
}


/* destructor()
	Destroy variables allocated in the contructor()

*/
Cinsomniapp::~Cinsomniapp(void)
{
	//delete [] m_cHRTF;
}

/*
initialize()
	Called by the client after creation; the parent window handle is now valid
	so you can use the Plug-In -> Host functions here (eg sendUpdateUI())
	See the website www.willpirkle.com for more details
*/
bool __stdcall Cinsomniapp::initialize()
{
	// Add your code here

	return true;
}

void Cinsomniapp::calculateLPFCoeffs(float fCutoffFreq, float fQ)
{
	float theta_c = 2.0*pi*fCutoffFreq/(float)m_nSampleRate;
	float d = 1.0/fQ;

	float fBetaNumerator = 1.0 - ((d/2.0)*sin(theta_c));
	float fBetaDenominator = 1.0 + ((d/2.0)*sin(theta_c));

	float fBeta = 0.5*(fBetaNumerator/fBetaDenominator);

	float fGamma = (0.5 + fBeta)*cos(theta_c);

	float fAlpha = (0.5 + fBeta - fGamma)/2.0;

	m_LPF.m_f_a0 = fAlpha;
	m_LPF.m_f_a1 = 2.0*fAlpha;
	m_LPF.m_f_a2 = fAlpha;
	m_LPF.m_f_b1 = -2.0*fGamma;
	m_LPF.m_f_b2 = 2.0*fBeta;
}

void Cinsomniapp::doDCA()
{
	float fLFOVal = 0.0;
	float fLFOValQ = 0.0;

	m_LFO1.doOscillate(&fLFOVal, &fLFOValQ);
	m_fLeftGain = abs(fLFOVal*m_fLFODepth);
	m_fRightGain = abs(fLFOValQ*m_fLFODepth);
}

void Cinsomniapp::cookVolume()
{m_fVolumeDB = pow(10.0, m_fVolume/20.0);}

void Cinsomniapp::doArraya(float & fInL, float & fInR)
{
	fInL = (m_fBrightness/100.0)*((3.0*fInL)/2.0)*(1 - (fInL*fInL)/3.0) + (1.0 - (m_fBrightness/100.0))*fInL;
	fInR = (m_fBrightness/100.0)*((3.0*fInR)/2.0)*(1 - (fInR*fInR)/3.0) + (1.0 - (m_fBrightness/100.0))*fInR;
}

/* prepareForPlay()
	Called by the client after Play() is initiated but before audio streams

	You can perform buffer flushes and per-run intializations.
	You can check the following variables and use them if needed:

	m_nNumWAVEChannels;
	m_nSampleRate;
	m_nBitDepth;

	NOTE: the above values are only valid during prepareForPlay() and
		  processAudioFrame() because the user might change to another wave file,
		  or use the sound card, oscillators, or impulse response mechanisms

    NOTE: if you allocte memory in this function, destroy it in ::destroy() above
*/
bool __stdcall Cinsomniapp::prepareForPlay()
{
	// Add your code here:
	//LPF
	m_LPF.flushDelays();
	calculateLPFCoeffs(m_fCutoff*((100.0 - m_fDarken)/100.0), 0.707);

	//convolution
	m_ConvL.openIRFile(&CWaveData(m_cHRTF[0]));
	m_ConvL.prepareForPlay();
	m_ConvR.openIRFile(&CWaveData(m_cHRTF[2]));
	m_ConvR.prepareForPlay();

	//LFO
	m_LFO1.prepareForPlay();
	m_LFO1.reset();
	m_LFO1.m_fFrequency_Hz = m_fLFORate;
	m_LFO1.cookFrequency();

	//source oscillators
	m_LeftOsc.setOscType(m_uOscSourceL);
	m_LeftOsc.prepareForPlay();
	m_RightOsc.setOscType(m_uOscSourceR);
	m_RightOsc.prepareForPlay();
	m_LeftOsc.setFreq(m_fOsc1Pitch);
	m_RightOsc.setFreq(m_fOsc2Pitch);

	return true;
}

void Cinsomniapp::assignFiles()
{
	if(m_uAutoPan)
	{
		if(m_fLeftGain > 0.3)
		{
			if(m_nSet != 0)
			{
				m_ConvL.openIRFile(&CWaveData(m_cHRTF[0]));
				m_ConvR.openIRFile(&CWaveData(m_cHRTF[0]));
				m_nSet = 0;
			}
		}
		if(m_fLeftGain <= 0.3 && m_fRightGain <= 0.3)
		{
			if(m_nSet != 1)
			{
				m_ConvL.openIRFile(&CWaveData(m_cHRTF[1]));
				m_ConvR.openIRFile(&CWaveData(m_cHRTF[1]));
				m_nSet = 1;
			}
		}
		if(m_fRightGain > 0.3)
		{
			if(m_nSet != 2)
			{
				m_ConvL.openIRFile(&CWaveData(m_cHRTF[2]));
				m_ConvR.openIRFile(&CWaveData(m_cHRTF[2]));
				m_nSet = 2;
			}
		}
	}
	else
	{
		if(m_nSet != 4)
		{
			m_ConvL.openIRFile(&CWaveData(m_cHRTF[0]));
			m_ConvR.openIRFile(&CWaveData(m_cHRTF[2]));
			m_nSet = 4;			
		}
	}
}

/* processAudioFrame

// ALL VALUES IN AND OUT ON THE RANGE OF -1.0 TO + 1.0

LEFT INPUT = pInputBuffer[0];
RIGHT INPUT = pInputBuffer[1]

LEFT OUTPUT = pInputBuffer[0]
RIGHT OUTPUT = pOutputBuffer[1]

*/
bool __stdcall Cinsomniapp::processAudioFrame(float* pInputBuffer, float* pOutputBuffer, UINT uNumInputChannels, UINT uNumOutputChannels)
{
	// output = input -- change this for meaningful processing
	//
	// Do LEFT (MONO) Channel; there is always at least one input/one output
	// (INSERT Effect)
	
	if(m_uStart)
	{
		float fOutL = 0.0; 
		float fOutQL = 0.0;
		float fOutR = 0.0; 
		float fOutQR = 0.0;

		float outL = 0.0, outR = 0.0;

		if(m_uAutoPan)
			doDCA();
		else
		{
			m_fLeftGain = 1.0;
			m_fRightGain = 1.0;
		}

		m_LeftOsc.doOscillate(fOutL, fOutQL);
		m_RightOsc.doOscillate(fOutR, fOutQR);

		fOutL *= m_fOsc1Level/100.0;
		fOutQL *= m_fOsc1Level/100.0;
		fOutR *= m_fOsc2Level/100.0;
		fOutQR *= m_fOsc2Level/100.0;

		if(m_uMono)
		{
			outL = ((fOutL + fOutR)/2.0)*m_fLeftGain;
			outR = ((fOutL + fOutR)/2.0)*m_fRightGain;
		}
		else
		{
			outL = fOutL*m_fLeftGain;
			outR = fOutR*m_fRightGain;
		}

		if(m_uBinaural)
		{
			assignFiles();
			outL = m_ConvL.processAudio(outL);
			outR = m_ConvR.processAudio(outR);
		}

		doArraya(outL, outR);
		outL = m_LPF.doBiQuad(outL);
		outR = m_LPF.doBiQuad(outR);
		pOutputBuffer[0] = outL*m_fVolumeDB;

		// Mono-In, Stereo-Out (AUX Effect)
		if(uNumInputChannels == 1 && uNumOutputChannels == 2)
			pOutputBuffer[1] = outL*m_fVolumeDB;

		// Stereo-In, Stereo-Out (INSERT Effect)
		if(uNumInputChannels == 2 && uNumOutputChannels == 2)
			pOutputBuffer[1] = outR*m_fVolumeDB;
	}
	else
	{
		pOutputBuffer[0] = 0;
		pOutputBuffer[1] = 0;
	}

	return true;
}


/* ADDED BY RACKAFX -- DO NOT EDIT THIS CODE!!! ----------------------------------- //
   	**--0x2983--**

	Variable Name                    Index
-----------------------------------------------
	m_fLFORate                        0
	m_fOsc1Pitch                      1
	m_fOsc2Pitch                      2
	m_uOscSourceL                     3
	m_uOscSourceR                     4
	m_fBrightness                     5
	m_fDarken                         6
	m_fLFODepth                       7
	m_fVolume                         8
	m_fOsc1Level                      9
	m_fOsc2Level                      10
	m_uMono                           45
	m_uStart                          46
	m_uAutoPan                        47
	m_uBinaural                       48

	Assignable Buttons               Index
-----------------------------------------------
	B1                                50
	B2                                51
	B3                                52

-----------------------------------------------
Joystick Drop List Boxes          Index
-----------------------------------------------
	 Drop List A                     60
	 Drop List B                     61
	 Drop List C                     62
	 Drop List D                     63

-----------------------------------------------

	**--0xFFDD--**
// ------------------------------------------------------------------------------- */
// Add your UI Handler code here ------------------------------------------------- //
//
bool __stdcall Cinsomniapp::userInterfaceChange(int nControlIndex)
{
	// decode the control index, or delete the switch and use brute force calls
	switch(nControlIndex)
	{
		case 0:
		{
			m_LFO1.m_fFrequency_Hz = m_fLFORate;
			m_LFO1.cookFrequency();
			break;
		}

		case 1:
		{
			m_LeftOsc.setFreq(m_fOsc1Pitch);
			break;
		}

		case 2:
		{
			m_RightOsc.setFreq(m_fOsc2Pitch);
			break;
		}

		case 3:
		{
			m_LeftOsc.setOscType(m_uOscSourceL);
			break;
		}

		case 4:
		{
			m_RightOsc.setOscType(m_uOscSourceR);
			break;
		}

		case 6:
		{
			calculateLPFCoeffs(m_fCutoff*((100.0 - m_fDarken)/100.0), 0.707);
			break;
		}

		default:
			break;
	}

	return true;
}


/* joystickControlChange

	Indicates the user moved the joystick point; the variables are the relative mixes
	of each axis; the values will add up to 1.0

			B
			|
		A -	x -	C
			|
			D

	The point in the very center (x) would be:
	fControlA = 0.25
	fControlB = 0.25
	fControlC = 0.25
	fControlD = 0.25

	AC Mix = projection on X Axis (0 -> 1)
	BD Mix = projection on Y Axis (0 -> 1)
*/
bool __stdcall Cinsomniapp::joystickControlChange(float fControlA, float fControlB, float fControlC, float fControlD, float fACMix, float fBDMix)
{
	// add your code here

	return true;
}



/* processAudioBuffer

	// ALL VALUES IN AND OUT ON THE RANGE OF -1.0 TO + 1.0

	The I/O buffers are interleaved depending on the number of channels. If uNumChannels = 2, then the
	buffer is L/R/L/R/L/R etc...

	if uNumChannels = 6 then the buffer is L/R/C/Sub/BL/BR etc...

	It is up to you to decode and de-interleave the data.

	To use this function set m_bWantBuffers = true in your constructor.

	******************************
	********* IMPORTANT! *********
	******************************
	If you are going to ultimately make this a VST Compatible Plug-In and you want to process
	buffers, you need to override the NEXT function below:

	processVSTAudioBuffer()


	This function (processRackAFXAudioBuffer) is not supported in the VST wrapper because
	the VST buffer sizes no maximum value. This would require the use of dynamic buffering
	in the callback which is not acceptable for performance!
*/
bool __stdcall Cinsomniapp::processRackAFXAudioBuffer(float* pInputBuffer, float* pOutputBuffer,
													   UINT uNumInputChannels, UINT uNumOutputChannels,
													   UINT uBufferSize)
{

	for(UINT i=0; i<uBufferSize; i++)
	{
		// pass through code
		pOutputBuffer[i] = pInputBuffer[i];
	}


	return true;
}



/* processVSTAudioBuffer

	// ALL VALUES IN AND OUT ON THE RANGE OF -1.0 TO + 1.0

	NOTE: You do not have to implement this function if you don't want to; the processAudioFrame()
	will still work; however this using function will be more CPU efficient for your plug-in, and will
	override processAudioFrame().

	To use this function set m_bWantVSTBuffers = true in your constructor.

	The VST input and output buffers are pointers-to-pointers. The pp buffers are the same depth as uNumChannels, so
	if uNumChannels = 2, then ppInputs would contain two pointers,

		inBuffer[0] = a pointer to the LEFT buffer of data
		inBuffer[1] = a pointer to the RIGHT buffer of data

	Similarly, outBuffer would have 2 pointers, one for left and one for right.

	For 5.1 audio you would get 6 pointers in each buffer.

*/
bool __stdcall Cinsomniapp::processVSTAudioBuffer(float** inBuffer, float** outBuffer, UINT uNumChannels, int inFramesToProcess)
{
	// PASS Through example
	// MONO First
	float* pInputL  = inBuffer[0];
	float* pOutputL = outBuffer[0];
	float* pInputR  = NULL;
	float* pOutputR = NULL;

	// if STEREO,
	if(inBuffer[1])
		pInputR = inBuffer[1];

	if(outBuffer[1])
		pOutputR = outBuffer[1];

	// Process audio by de-referencing ptrs
	// this is siple pass through code
	while (--inFramesToProcess >= 0)
	{
		// Left channel processing
		*pOutputL = *pInputL;

		// If there is a right channel
		if(pInputR)
			*pOutputR = *pInputR;

		// advance pointers
		pInputL++;
		pOutputL++;
		if(pInputR) pInputR++;
		if(pOutputR) pOutputR++;
	}
	// all OK
	return true;
}

bool __stdcall Cinsomniapp::midiNoteOn(UINT uChannel, UINT uMIDINote, UINT uVelocity)
{
	return true;
}

bool __stdcall Cinsomniapp::midiNoteOff(UINT uChannel, UINT uMIDINote, UINT uVelocity, bool bAllNotesOff)
{
	return true;
}

// uModValue = 0->127
bool __stdcall Cinsomniapp::midiModWheel(UINT uChannel, UINT uModValue)
{
	return true;
}

// nActualPitchBendValue 		= -8192 -> +8191, 0 at center
// fNormalizedPitchBendValue 	= -1.0  -> +1.0,  0 at center
bool __stdcall Cinsomniapp::midiPitchBend(UINT uChannel, int nActualPitchBendValue, float fNormalizedPitchBendValue)
{
	return true;
}

// MIDI Clock
// http://home.roadrunner.com/~jgglatt/tech/midispec/clock.htm
/* There are 24 MIDI Clocks in every quarter note. (12 MIDI Clocks in an eighth note, 6 MIDI Clocks in a 16th, etc).
   Therefore, when a slave device counts down the receipt of 24 MIDI Clock messages, it knows that one quarter note
   has passed. When the slave counts off another 24 MIDI Clock messages, it knows that another quarter note has passed.
   Etc. Of course, the rate that the master sends these messages is based upon the master's tempo.

   For example, for a tempo of 120 BPM (ie, there are 120 quarter notes in every minute), the master sends a MIDI clock
   every 20833 microseconds. (ie, There are 1,000,000 microseconds in a second. Therefore, there are 60,000,000
   microseconds in a minute. At a tempo of 120 BPM, there are 120 quarter notes per minute. There are 24 MIDI clocks
   in each quarter note. Therefore, there should be 24 * 120 MIDI Clocks per minute.
   So, each MIDI Clock is sent at a rate of 60,000,000/(24 * 120) microseconds).
*/
bool __stdcall Cinsomniapp::midiClock()
{

	return true;
}

// any midi message other than note on, note off, pitchbend, mod wheel or clock
bool __stdcall Cinsomniapp::midiMessage(unsigned char cChannel, unsigned char cStatus, unsigned char
						   				  cData1, unsigned char cData2)
{
	return true;
}


// DO NOT DELETE THIS FUNCTION --------------------------------------------------- //
bool __stdcall Cinsomniapp::initUI()
{
	// ADDED BY RACKAFX -- DO NOT EDIT THIS CODE!!! ------------------------------ //
	if(m_UIControlList.count() > 0)
		return true;

// **--0xDEA7--**

	m_fLFORate = 0.100000;
	CUICtrl* ui0 = new CUICtrl;
	ui0->uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
	ui0->uControlId = 0;
	ui0->bLogSlider = false;
	ui0->bExpSlider = false;
	ui0->fUserDisplayDataLoLimit = 0.010000;
	ui0->fUserDisplayDataHiLimit = 1.000000;
	ui0->uUserDataType = floatData;
	ui0->fInitUserIntValue = 0;
	ui0->fInitUserFloatValue = 0.100000;
	ui0->fInitUserDoubleValue = 0;
	ui0->fInitUserUINTValue = 0;
	ui0->m_pUserCookedIntData = NULL;
	ui0->m_pUserCookedFloatData = &m_fLFORate;
	ui0->m_pUserCookedDoubleData = NULL;
	ui0->m_pUserCookedUINTData = NULL;
	ui0->cControlUnits = "Hz                                                              ";
	ui0->cVariableName = "m_fLFORate";
	ui0->cEnumeratedList = "SEL1,SEL2,SEL3";
	ui0->dPresetData[0] = 0.100000;ui0->dPresetData[1] = 0.000000;ui0->dPresetData[2] = 0.000000;ui0->dPresetData[3] = 0.000000;ui0->dPresetData[4] = 0.000000;ui0->dPresetData[5] = 0.000000;ui0->dPresetData[6] = 0.000000;ui0->dPresetData[7] = 0.000000;ui0->dPresetData[8] = 0.000000;ui0->dPresetData[9] = 0.000000;ui0->dPresetData[10] = 0.000000;ui0->dPresetData[11] = 0.000000;ui0->dPresetData[12] = 0.000000;ui0->dPresetData[13] = 0.000000;ui0->dPresetData[14] = 0.000000;ui0->dPresetData[15] = 0.000000;
	ui0->cControlName = "Rate";
	ui0->bOwnerControl = false;
	ui0->bMIDIControl = false;
	ui0->uMIDIControlCommand = 176;
	ui0->uMIDIControlName = 3;
	ui0->uMIDIControlChannel = 0;
	ui0->nGUIRow = -1;
	ui0->nGUIColumn = -1;
	ui0->uControlTheme[0] = 0; ui0->uControlTheme[1] = 0; ui0->uControlTheme[2] = 0; ui0->uControlTheme[3] = 0; ui0->uControlTheme[4] = 0; ui0->uControlTheme[5] = 0; ui0->uControlTheme[6] = 0; ui0->uControlTheme[7] = 0; ui0->uControlTheme[8] = 0; ui0->uControlTheme[9] = 0; ui0->uControlTheme[10] = 0; ui0->uControlTheme[11] = 0; ui0->uControlTheme[12] = 0; ui0->uControlTheme[13] = 0; ui0->uControlTheme[14] = 0; ui0->uControlTheme[15] = 0; ui0->uControlTheme[16] = 0; ui0->uControlTheme[17] = 0; ui0->uControlTheme[18] = 0; ui0->uControlTheme[19] = 0; ui0->uControlTheme[20] = 0; ui0->uControlTheme[21] = 0; ui0->uControlTheme[22] = 0; ui0->uControlTheme[23] = 0; ui0->uControlTheme[24] = 0; ui0->uControlTheme[25] = 0; ui0->uControlTheme[26] = 0; ui0->uControlTheme[27] = 1; ui0->uControlTheme[28] = 0; ui0->uControlTheme[29] = 0; ui0->uControlTheme[30] = 0; ui0->uControlTheme[31] = 0; 
	ui0->uFluxCapControl[0] = 0; ui0->uFluxCapControl[1] = 0; ui0->uFluxCapControl[2] = 0; ui0->uFluxCapControl[3] = 0; ui0->uFluxCapControl[4] = 0; ui0->uFluxCapControl[5] = 0; ui0->uFluxCapControl[6] = 0; ui0->uFluxCapControl[7] = 0; ui0->uFluxCapControl[8] = 0; ui0->uFluxCapControl[9] = 0; ui0->uFluxCapControl[10] = 0; ui0->uFluxCapControl[11] = 0; ui0->uFluxCapControl[12] = 0; ui0->uFluxCapControl[13] = 0; ui0->uFluxCapControl[14] = 0; ui0->uFluxCapControl[15] = 0; ui0->uFluxCapControl[16] = 0; ui0->uFluxCapControl[17] = 0; ui0->uFluxCapControl[18] = 0; ui0->uFluxCapControl[19] = 0; ui0->uFluxCapControl[20] = 0; ui0->uFluxCapControl[21] = 0; ui0->uFluxCapControl[22] = 0; ui0->uFluxCapControl[23] = 0; ui0->uFluxCapControl[24] = 0; ui0->uFluxCapControl[25] = 0; ui0->uFluxCapControl[26] = 0; ui0->uFluxCapControl[27] = 0; ui0->uFluxCapControl[28] = 0; ui0->uFluxCapControl[29] = 0; ui0->uFluxCapControl[30] = 0; ui0->uFluxCapControl[31] = 0; ui0->uFluxCapControl[32] = 0; ui0->uFluxCapControl[33] = 0; ui0->uFluxCapControl[34] = 0; ui0->uFluxCapControl[35] = 0; ui0->uFluxCapControl[36] = 0; ui0->uFluxCapControl[37] = 0; ui0->uFluxCapControl[38] = 0; ui0->uFluxCapControl[39] = 0; ui0->uFluxCapControl[40] = 0; ui0->uFluxCapControl[41] = 0; ui0->uFluxCapControl[42] = 0; ui0->uFluxCapControl[43] = 0; ui0->uFluxCapControl[44] = 0; ui0->uFluxCapControl[45] = 0; ui0->uFluxCapControl[46] = 0; ui0->uFluxCapControl[47] = 0; ui0->uFluxCapControl[48] = 0; ui0->uFluxCapControl[49] = 0; ui0->uFluxCapControl[50] = 0; ui0->uFluxCapControl[51] = 0; ui0->uFluxCapControl[52] = 0; ui0->uFluxCapControl[53] = 0; ui0->uFluxCapControl[54] = 0; ui0->uFluxCapControl[55] = 0; ui0->uFluxCapControl[56] = 0; ui0->uFluxCapControl[57] = 0; ui0->uFluxCapControl[58] = 0; ui0->uFluxCapControl[59] = 0; ui0->uFluxCapControl[60] = 0; ui0->uFluxCapControl[61] = 0; ui0->uFluxCapControl[62] = 0; ui0->uFluxCapControl[63] = 0; 
	ui0->fFluxCapData[0] = 0.000000; ui0->fFluxCapData[1] = 0.000000; ui0->fFluxCapData[2] = 0.000000; ui0->fFluxCapData[3] = 0.000000; ui0->fFluxCapData[4] = 0.000000; ui0->fFluxCapData[5] = 0.000000; ui0->fFluxCapData[6] = 0.000000; ui0->fFluxCapData[7] = 0.000000; ui0->fFluxCapData[8] = 0.000000; ui0->fFluxCapData[9] = 0.000000; ui0->fFluxCapData[10] = 0.000000; ui0->fFluxCapData[11] = 0.000000; ui0->fFluxCapData[12] = 0.000000; ui0->fFluxCapData[13] = 0.000000; ui0->fFluxCapData[14] = 0.000000; ui0->fFluxCapData[15] = 0.000000; ui0->fFluxCapData[16] = 0.000000; ui0->fFluxCapData[17] = 0.000000; ui0->fFluxCapData[18] = 0.000000; ui0->fFluxCapData[19] = 0.000000; ui0->fFluxCapData[20] = 0.000000; ui0->fFluxCapData[21] = 0.000000; ui0->fFluxCapData[22] = 0.000000; ui0->fFluxCapData[23] = 0.000000; ui0->fFluxCapData[24] = 0.000000; ui0->fFluxCapData[25] = 0.000000; ui0->fFluxCapData[26] = 0.000000; ui0->fFluxCapData[27] = 0.000000; ui0->fFluxCapData[28] = 0.000000; ui0->fFluxCapData[29] = 0.000000; ui0->fFluxCapData[30] = 0.000000; ui0->fFluxCapData[31] = 0.000000; ui0->fFluxCapData[32] = 0.000000; ui0->fFluxCapData[33] = 0.000000; ui0->fFluxCapData[34] = 0.000000; ui0->fFluxCapData[35] = 0.000000; ui0->fFluxCapData[36] = 0.000000; ui0->fFluxCapData[37] = 0.000000; ui0->fFluxCapData[38] = 0.000000; ui0->fFluxCapData[39] = 0.000000; ui0->fFluxCapData[40] = 0.000000; ui0->fFluxCapData[41] = 0.000000; ui0->fFluxCapData[42] = 0.000000; ui0->fFluxCapData[43] = 0.000000; ui0->fFluxCapData[44] = 0.000000; ui0->fFluxCapData[45] = 0.000000; ui0->fFluxCapData[46] = 0.000000; ui0->fFluxCapData[47] = 0.000000; ui0->fFluxCapData[48] = 0.000000; ui0->fFluxCapData[49] = 0.000000; ui0->fFluxCapData[50] = 0.000000; ui0->fFluxCapData[51] = 0.000000; ui0->fFluxCapData[52] = 0.000000; ui0->fFluxCapData[53] = 0.000000; ui0->fFluxCapData[54] = 0.000000; ui0->fFluxCapData[55] = 0.000000; ui0->fFluxCapData[56] = 0.000000; ui0->fFluxCapData[57] = 0.000000; ui0->fFluxCapData[58] = 0.000000; ui0->fFluxCapData[59] = 0.000000; ui0->fFluxCapData[60] = 0.000000; ui0->fFluxCapData[61] = 0.000000; ui0->fFluxCapData[62] = 0.000000; ui0->fFluxCapData[63] = 0.000000; 
	m_UIControlList.append(*ui0);
	delete ui0;


	m_fOsc1Pitch = 50.000000;
	CUICtrl* ui1 = new CUICtrl;
	ui1->uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
	ui1->uControlId = 1;
	ui1->bLogSlider = false;
	ui1->bExpSlider = false;
	ui1->fUserDisplayDataLoLimit = 1.000000;
	ui1->fUserDisplayDataHiLimit = 150.000000;
	ui1->uUserDataType = floatData;
	ui1->fInitUserIntValue = 0;
	ui1->fInitUserFloatValue = 50.000000;
	ui1->fInitUserDoubleValue = 0;
	ui1->fInitUserUINTValue = 0;
	ui1->m_pUserCookedIntData = NULL;
	ui1->m_pUserCookedFloatData = &m_fOsc1Pitch;
	ui1->m_pUserCookedDoubleData = NULL;
	ui1->m_pUserCookedUINTData = NULL;
	ui1->cControlUnits = "Hz                                                              ";
	ui1->cVariableName = "m_fOsc1Pitch";
	ui1->cEnumeratedList = "SEL1,SEL2,SEL3";
	ui1->dPresetData[0] = 50.000000;ui1->dPresetData[1] = 0.000000;ui1->dPresetData[2] = 0.000000;ui1->dPresetData[3] = 0.000000;ui1->dPresetData[4] = 0.000000;ui1->dPresetData[5] = 0.000000;ui1->dPresetData[6] = 0.000000;ui1->dPresetData[7] = 0.000000;ui1->dPresetData[8] = 0.000000;ui1->dPresetData[9] = 0.000000;ui1->dPresetData[10] = 0.000000;ui1->dPresetData[11] = 0.000000;ui1->dPresetData[12] = 0.000000;ui1->dPresetData[13] = 0.000000;ui1->dPresetData[14] = 0.000000;ui1->dPresetData[15] = 0.000000;
	ui1->cControlName = "Freq 1";
	ui1->bOwnerControl = false;
	ui1->bMIDIControl = false;
	ui1->uMIDIControlCommand = 176;
	ui1->uMIDIControlName = 3;
	ui1->uMIDIControlChannel = 0;
	ui1->nGUIRow = -1;
	ui1->nGUIColumn = -1;
	ui1->uControlTheme[0] = 0; ui1->uControlTheme[1] = 0; ui1->uControlTheme[2] = 0; ui1->uControlTheme[3] = 0; ui1->uControlTheme[4] = 0; ui1->uControlTheme[5] = 0; ui1->uControlTheme[6] = 0; ui1->uControlTheme[7] = 0; ui1->uControlTheme[8] = 0; ui1->uControlTheme[9] = 0; ui1->uControlTheme[10] = 0; ui1->uControlTheme[11] = 0; ui1->uControlTheme[12] = 0; ui1->uControlTheme[13] = 0; ui1->uControlTheme[14] = 0; ui1->uControlTheme[15] = 0; ui1->uControlTheme[16] = 0; ui1->uControlTheme[17] = 0; ui1->uControlTheme[18] = 0; ui1->uControlTheme[19] = 0; ui1->uControlTheme[20] = 0; ui1->uControlTheme[21] = 0; ui1->uControlTheme[22] = 0; ui1->uControlTheme[23] = 0; ui1->uControlTheme[24] = 0; ui1->uControlTheme[25] = 0; ui1->uControlTheme[26] = 0; ui1->uControlTheme[27] = 1; ui1->uControlTheme[28] = 0; ui1->uControlTheme[29] = 0; ui1->uControlTheme[30] = 0; ui1->uControlTheme[31] = 0; 
	ui1->uFluxCapControl[0] = 0; ui1->uFluxCapControl[1] = 0; ui1->uFluxCapControl[2] = 0; ui1->uFluxCapControl[3] = 0; ui1->uFluxCapControl[4] = 0; ui1->uFluxCapControl[5] = 0; ui1->uFluxCapControl[6] = 0; ui1->uFluxCapControl[7] = 0; ui1->uFluxCapControl[8] = 0; ui1->uFluxCapControl[9] = 0; ui1->uFluxCapControl[10] = 0; ui1->uFluxCapControl[11] = 0; ui1->uFluxCapControl[12] = 0; ui1->uFluxCapControl[13] = 0; ui1->uFluxCapControl[14] = 0; ui1->uFluxCapControl[15] = 0; ui1->uFluxCapControl[16] = 0; ui1->uFluxCapControl[17] = 0; ui1->uFluxCapControl[18] = 0; ui1->uFluxCapControl[19] = 0; ui1->uFluxCapControl[20] = 0; ui1->uFluxCapControl[21] = 0; ui1->uFluxCapControl[22] = 0; ui1->uFluxCapControl[23] = 0; ui1->uFluxCapControl[24] = 0; ui1->uFluxCapControl[25] = 0; ui1->uFluxCapControl[26] = 0; ui1->uFluxCapControl[27] = 0; ui1->uFluxCapControl[28] = 0; ui1->uFluxCapControl[29] = 0; ui1->uFluxCapControl[30] = 0; ui1->uFluxCapControl[31] = 0; ui1->uFluxCapControl[32] = 0; ui1->uFluxCapControl[33] = 0; ui1->uFluxCapControl[34] = 0; ui1->uFluxCapControl[35] = 0; ui1->uFluxCapControl[36] = 0; ui1->uFluxCapControl[37] = 0; ui1->uFluxCapControl[38] = 0; ui1->uFluxCapControl[39] = 0; ui1->uFluxCapControl[40] = 0; ui1->uFluxCapControl[41] = 0; ui1->uFluxCapControl[42] = 0; ui1->uFluxCapControl[43] = 0; ui1->uFluxCapControl[44] = 0; ui1->uFluxCapControl[45] = 0; ui1->uFluxCapControl[46] = 0; ui1->uFluxCapControl[47] = 0; ui1->uFluxCapControl[48] = 0; ui1->uFluxCapControl[49] = 0; ui1->uFluxCapControl[50] = 0; ui1->uFluxCapControl[51] = 0; ui1->uFluxCapControl[52] = 0; ui1->uFluxCapControl[53] = 0; ui1->uFluxCapControl[54] = 0; ui1->uFluxCapControl[55] = 0; ui1->uFluxCapControl[56] = 0; ui1->uFluxCapControl[57] = 0; ui1->uFluxCapControl[58] = 0; ui1->uFluxCapControl[59] = 0; ui1->uFluxCapControl[60] = 0; ui1->uFluxCapControl[61] = 0; ui1->uFluxCapControl[62] = 0; ui1->uFluxCapControl[63] = 0; 
	ui1->fFluxCapData[0] = 0.000000; ui1->fFluxCapData[1] = 0.000000; ui1->fFluxCapData[2] = 0.000000; ui1->fFluxCapData[3] = 0.000000; ui1->fFluxCapData[4] = 0.000000; ui1->fFluxCapData[5] = 0.000000; ui1->fFluxCapData[6] = 0.000000; ui1->fFluxCapData[7] = 0.000000; ui1->fFluxCapData[8] = 0.000000; ui1->fFluxCapData[9] = 0.000000; ui1->fFluxCapData[10] = 0.000000; ui1->fFluxCapData[11] = 0.000000; ui1->fFluxCapData[12] = 0.000000; ui1->fFluxCapData[13] = 0.000000; ui1->fFluxCapData[14] = 0.000000; ui1->fFluxCapData[15] = 0.000000; ui1->fFluxCapData[16] = 0.000000; ui1->fFluxCapData[17] = 0.000000; ui1->fFluxCapData[18] = 0.000000; ui1->fFluxCapData[19] = 0.000000; ui1->fFluxCapData[20] = 0.000000; ui1->fFluxCapData[21] = 0.000000; ui1->fFluxCapData[22] = 0.000000; ui1->fFluxCapData[23] = 0.000000; ui1->fFluxCapData[24] = 0.000000; ui1->fFluxCapData[25] = 0.000000; ui1->fFluxCapData[26] = 0.000000; ui1->fFluxCapData[27] = 0.000000; ui1->fFluxCapData[28] = 0.000000; ui1->fFluxCapData[29] = 0.000000; ui1->fFluxCapData[30] = 0.000000; ui1->fFluxCapData[31] = 0.000000; ui1->fFluxCapData[32] = 0.000000; ui1->fFluxCapData[33] = 0.000000; ui1->fFluxCapData[34] = 0.000000; ui1->fFluxCapData[35] = 0.000000; ui1->fFluxCapData[36] = 0.000000; ui1->fFluxCapData[37] = 0.000000; ui1->fFluxCapData[38] = 0.000000; ui1->fFluxCapData[39] = 0.000000; ui1->fFluxCapData[40] = 0.000000; ui1->fFluxCapData[41] = 0.000000; ui1->fFluxCapData[42] = 0.000000; ui1->fFluxCapData[43] = 0.000000; ui1->fFluxCapData[44] = 0.000000; ui1->fFluxCapData[45] = 0.000000; ui1->fFluxCapData[46] = 0.000000; ui1->fFluxCapData[47] = 0.000000; ui1->fFluxCapData[48] = 0.000000; ui1->fFluxCapData[49] = 0.000000; ui1->fFluxCapData[50] = 0.000000; ui1->fFluxCapData[51] = 0.000000; ui1->fFluxCapData[52] = 0.000000; ui1->fFluxCapData[53] = 0.000000; ui1->fFluxCapData[54] = 0.000000; ui1->fFluxCapData[55] = 0.000000; ui1->fFluxCapData[56] = 0.000000; ui1->fFluxCapData[57] = 0.000000; ui1->fFluxCapData[58] = 0.000000; ui1->fFluxCapData[59] = 0.000000; ui1->fFluxCapData[60] = 0.000000; ui1->fFluxCapData[61] = 0.000000; ui1->fFluxCapData[62] = 0.000000; ui1->fFluxCapData[63] = 0.000000; 
	m_UIControlList.append(*ui1);
	delete ui1;


	m_fOsc2Pitch = 55.000000;
	CUICtrl* ui2 = new CUICtrl;
	ui2->uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
	ui2->uControlId = 2;
	ui2->bLogSlider = false;
	ui2->bExpSlider = false;
	ui2->fUserDisplayDataLoLimit = 0.100000;
	ui2->fUserDisplayDataHiLimit = 150.000000;
	ui2->uUserDataType = floatData;
	ui2->fInitUserIntValue = 0;
	ui2->fInitUserFloatValue = 55.000000;
	ui2->fInitUserDoubleValue = 0;
	ui2->fInitUserUINTValue = 0;
	ui2->m_pUserCookedIntData = NULL;
	ui2->m_pUserCookedFloatData = &m_fOsc2Pitch;
	ui2->m_pUserCookedDoubleData = NULL;
	ui2->m_pUserCookedUINTData = NULL;
	ui2->cControlUnits = "hz                                                              ";
	ui2->cVariableName = "m_fOsc2Pitch";
	ui2->cEnumeratedList = "SEL1,SEL2,SEL3";
	ui2->dPresetData[0] = 55.000000;ui2->dPresetData[1] = 0.000000;ui2->dPresetData[2] = 0.000000;ui2->dPresetData[3] = 0.000000;ui2->dPresetData[4] = 0.000000;ui2->dPresetData[5] = 0.000000;ui2->dPresetData[6] = 0.000000;ui2->dPresetData[7] = 0.000000;ui2->dPresetData[8] = 0.000000;ui2->dPresetData[9] = 0.000000;ui2->dPresetData[10] = 0.000000;ui2->dPresetData[11] = 0.000000;ui2->dPresetData[12] = 0.000000;ui2->dPresetData[13] = 0.000000;ui2->dPresetData[14] = 0.000000;ui2->dPresetData[15] = 0.000000;
	ui2->cControlName = "Freq 2";
	ui2->bOwnerControl = false;
	ui2->bMIDIControl = false;
	ui2->uMIDIControlCommand = 176;
	ui2->uMIDIControlName = 3;
	ui2->uMIDIControlChannel = 0;
	ui2->nGUIRow = -1;
	ui2->nGUIColumn = -1;
	ui2->uControlTheme[0] = 0; ui2->uControlTheme[1] = 0; ui2->uControlTheme[2] = 0; ui2->uControlTheme[3] = 0; ui2->uControlTheme[4] = 0; ui2->uControlTheme[5] = 0; ui2->uControlTheme[6] = 0; ui2->uControlTheme[7] = 0; ui2->uControlTheme[8] = 0; ui2->uControlTheme[9] = 0; ui2->uControlTheme[10] = 0; ui2->uControlTheme[11] = 0; ui2->uControlTheme[12] = 0; ui2->uControlTheme[13] = 0; ui2->uControlTheme[14] = 0; ui2->uControlTheme[15] = 0; ui2->uControlTheme[16] = 0; ui2->uControlTheme[17] = 0; ui2->uControlTheme[18] = 0; ui2->uControlTheme[19] = 0; ui2->uControlTheme[20] = 0; ui2->uControlTheme[21] = 0; ui2->uControlTheme[22] = 0; ui2->uControlTheme[23] = 0; ui2->uControlTheme[24] = 0; ui2->uControlTheme[25] = 0; ui2->uControlTheme[26] = 0; ui2->uControlTheme[27] = 1; ui2->uControlTheme[28] = 0; ui2->uControlTheme[29] = 0; ui2->uControlTheme[30] = 0; ui2->uControlTheme[31] = 0; 
	ui2->uFluxCapControl[0] = 0; ui2->uFluxCapControl[1] = 0; ui2->uFluxCapControl[2] = 0; ui2->uFluxCapControl[3] = 0; ui2->uFluxCapControl[4] = 0; ui2->uFluxCapControl[5] = 0; ui2->uFluxCapControl[6] = 0; ui2->uFluxCapControl[7] = 0; ui2->uFluxCapControl[8] = 0; ui2->uFluxCapControl[9] = 0; ui2->uFluxCapControl[10] = 0; ui2->uFluxCapControl[11] = 0; ui2->uFluxCapControl[12] = 0; ui2->uFluxCapControl[13] = 0; ui2->uFluxCapControl[14] = 0; ui2->uFluxCapControl[15] = 0; ui2->uFluxCapControl[16] = 0; ui2->uFluxCapControl[17] = 0; ui2->uFluxCapControl[18] = 0; ui2->uFluxCapControl[19] = 0; ui2->uFluxCapControl[20] = 0; ui2->uFluxCapControl[21] = 0; ui2->uFluxCapControl[22] = 0; ui2->uFluxCapControl[23] = 0; ui2->uFluxCapControl[24] = 0; ui2->uFluxCapControl[25] = 0; ui2->uFluxCapControl[26] = 0; ui2->uFluxCapControl[27] = 0; ui2->uFluxCapControl[28] = 0; ui2->uFluxCapControl[29] = 0; ui2->uFluxCapControl[30] = 0; ui2->uFluxCapControl[31] = 0; ui2->uFluxCapControl[32] = 0; ui2->uFluxCapControl[33] = 0; ui2->uFluxCapControl[34] = 0; ui2->uFluxCapControl[35] = 0; ui2->uFluxCapControl[36] = 0; ui2->uFluxCapControl[37] = 0; ui2->uFluxCapControl[38] = 0; ui2->uFluxCapControl[39] = 0; ui2->uFluxCapControl[40] = 0; ui2->uFluxCapControl[41] = 0; ui2->uFluxCapControl[42] = 0; ui2->uFluxCapControl[43] = 0; ui2->uFluxCapControl[44] = 0; ui2->uFluxCapControl[45] = 0; ui2->uFluxCapControl[46] = 0; ui2->uFluxCapControl[47] = 0; ui2->uFluxCapControl[48] = 0; ui2->uFluxCapControl[49] = 0; ui2->uFluxCapControl[50] = 0; ui2->uFluxCapControl[51] = 0; ui2->uFluxCapControl[52] = 0; ui2->uFluxCapControl[53] = 0; ui2->uFluxCapControl[54] = 0; ui2->uFluxCapControl[55] = 0; ui2->uFluxCapControl[56] = 0; ui2->uFluxCapControl[57] = 0; ui2->uFluxCapControl[58] = 0; ui2->uFluxCapControl[59] = 0; ui2->uFluxCapControl[60] = 0; ui2->uFluxCapControl[61] = 0; ui2->uFluxCapControl[62] = 0; ui2->uFluxCapControl[63] = 0; 
	ui2->fFluxCapData[0] = 0.000000; ui2->fFluxCapData[1] = 0.000000; ui2->fFluxCapData[2] = 0.000000; ui2->fFluxCapData[3] = 0.000000; ui2->fFluxCapData[4] = 0.000000; ui2->fFluxCapData[5] = 0.000000; ui2->fFluxCapData[6] = 0.000000; ui2->fFluxCapData[7] = 0.000000; ui2->fFluxCapData[8] = 0.000000; ui2->fFluxCapData[9] = 0.000000; ui2->fFluxCapData[10] = 0.000000; ui2->fFluxCapData[11] = 0.000000; ui2->fFluxCapData[12] = 0.000000; ui2->fFluxCapData[13] = 0.000000; ui2->fFluxCapData[14] = 0.000000; ui2->fFluxCapData[15] = 0.000000; ui2->fFluxCapData[16] = 0.000000; ui2->fFluxCapData[17] = 0.000000; ui2->fFluxCapData[18] = 0.000000; ui2->fFluxCapData[19] = 0.000000; ui2->fFluxCapData[20] = 0.000000; ui2->fFluxCapData[21] = 0.000000; ui2->fFluxCapData[22] = 0.000000; ui2->fFluxCapData[23] = 0.000000; ui2->fFluxCapData[24] = 0.000000; ui2->fFluxCapData[25] = 0.000000; ui2->fFluxCapData[26] = 0.000000; ui2->fFluxCapData[27] = 0.000000; ui2->fFluxCapData[28] = 0.000000; ui2->fFluxCapData[29] = 0.000000; ui2->fFluxCapData[30] = 0.000000; ui2->fFluxCapData[31] = 0.000000; ui2->fFluxCapData[32] = 0.000000; ui2->fFluxCapData[33] = 0.000000; ui2->fFluxCapData[34] = 0.000000; ui2->fFluxCapData[35] = 0.000000; ui2->fFluxCapData[36] = 0.000000; ui2->fFluxCapData[37] = 0.000000; ui2->fFluxCapData[38] = 0.000000; ui2->fFluxCapData[39] = 0.000000; ui2->fFluxCapData[40] = 0.000000; ui2->fFluxCapData[41] = 0.000000; ui2->fFluxCapData[42] = 0.000000; ui2->fFluxCapData[43] = 0.000000; ui2->fFluxCapData[44] = 0.000000; ui2->fFluxCapData[45] = 0.000000; ui2->fFluxCapData[46] = 0.000000; ui2->fFluxCapData[47] = 0.000000; ui2->fFluxCapData[48] = 0.000000; ui2->fFluxCapData[49] = 0.000000; ui2->fFluxCapData[50] = 0.000000; ui2->fFluxCapData[51] = 0.000000; ui2->fFluxCapData[52] = 0.000000; ui2->fFluxCapData[53] = 0.000000; ui2->fFluxCapData[54] = 0.000000; ui2->fFluxCapData[55] = 0.000000; ui2->fFluxCapData[56] = 0.000000; ui2->fFluxCapData[57] = 0.000000; ui2->fFluxCapData[58] = 0.000000; ui2->fFluxCapData[59] = 0.000000; ui2->fFluxCapData[60] = 0.000000; ui2->fFluxCapData[61] = 0.000000; ui2->fFluxCapData[62] = 0.000000; ui2->fFluxCapData[63] = 0.000000; 
	m_UIControlList.append(*ui2);
	delete ui2;


	m_uOscSourceL = 0;
	CUICtrl* ui3 = new CUICtrl;
	ui3->uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
	ui3->uControlId = 3;
	ui3->bLogSlider = false;
	ui3->bExpSlider = false;
	ui3->fUserDisplayDataLoLimit = 0.000000;
	ui3->fUserDisplayDataHiLimit = 10.000000;
	ui3->uUserDataType = UINTData;
	ui3->fInitUserIntValue = 0;
	ui3->fInitUserFloatValue = 0;
	ui3->fInitUserDoubleValue = 0;
	ui3->fInitUserUINTValue = 0.000000;
	ui3->m_pUserCookedIntData = NULL;
	ui3->m_pUserCookedFloatData = NULL;
	ui3->m_pUserCookedDoubleData = NULL;
	ui3->m_pUserCookedUINTData = &m_uOscSourceL;
	ui3->cControlUnits = "                                                                ";
	ui3->cVariableName = "m_uOscSourceL";
	ui3->cEnumeratedList = "sine,white,pink,car,rain,ocean,tape,vinyl,male,female,user";
	ui3->dPresetData[0] = 0.000000;ui3->dPresetData[1] = 0.000000;ui3->dPresetData[2] = 0.000000;ui3->dPresetData[3] = 0.000000;ui3->dPresetData[4] = 0.000000;ui3->dPresetData[5] = 0.000000;ui3->dPresetData[6] = 0.000000;ui3->dPresetData[7] = 0.000000;ui3->dPresetData[8] = 0.000000;ui3->dPresetData[9] = 0.000000;ui3->dPresetData[10] = 0.000000;ui3->dPresetData[11] = 0.000000;ui3->dPresetData[12] = 0.000000;ui3->dPresetData[13] = 0.000000;ui3->dPresetData[14] = 0.000000;ui3->dPresetData[15] = 0.000000;
	ui3->cControlName = "Osc L Source";
	ui3->bOwnerControl = false;
	ui3->bMIDIControl = false;
	ui3->uMIDIControlCommand = 176;
	ui3->uMIDIControlName = 3;
	ui3->uMIDIControlChannel = 0;
	ui3->nGUIRow = -1;
	ui3->nGUIColumn = -1;
	ui3->uControlTheme[0] = 0; ui3->uControlTheme[1] = 0; ui3->uControlTheme[2] = 0; ui3->uControlTheme[3] = 0; ui3->uControlTheme[4] = 0; ui3->uControlTheme[5] = 0; ui3->uControlTheme[6] = 0; ui3->uControlTheme[7] = 0; ui3->uControlTheme[8] = 0; ui3->uControlTheme[9] = 0; ui3->uControlTheme[10] = 0; ui3->uControlTheme[11] = 0; ui3->uControlTheme[12] = 0; ui3->uControlTheme[13] = 0; ui3->uControlTheme[14] = 0; ui3->uControlTheme[15] = 0; ui3->uControlTheme[16] = 0; ui3->uControlTheme[17] = 0; ui3->uControlTheme[18] = 0; ui3->uControlTheme[19] = 0; ui3->uControlTheme[20] = 0; ui3->uControlTheme[21] = 0; ui3->uControlTheme[22] = 0; ui3->uControlTheme[23] = 0; ui3->uControlTheme[24] = 0; ui3->uControlTheme[25] = 0; ui3->uControlTheme[26] = 0; ui3->uControlTheme[27] = 1; ui3->uControlTheme[28] = 0; ui3->uControlTheme[29] = 0; ui3->uControlTheme[30] = 0; ui3->uControlTheme[31] = 0; 
	ui3->uFluxCapControl[0] = 0; ui3->uFluxCapControl[1] = 0; ui3->uFluxCapControl[2] = 0; ui3->uFluxCapControl[3] = 0; ui3->uFluxCapControl[4] = 0; ui3->uFluxCapControl[5] = 0; ui3->uFluxCapControl[6] = 0; ui3->uFluxCapControl[7] = 0; ui3->uFluxCapControl[8] = 0; ui3->uFluxCapControl[9] = 0; ui3->uFluxCapControl[10] = 0; ui3->uFluxCapControl[11] = 0; ui3->uFluxCapControl[12] = 0; ui3->uFluxCapControl[13] = 0; ui3->uFluxCapControl[14] = 0; ui3->uFluxCapControl[15] = 0; ui3->uFluxCapControl[16] = 0; ui3->uFluxCapControl[17] = 0; ui3->uFluxCapControl[18] = 0; ui3->uFluxCapControl[19] = 0; ui3->uFluxCapControl[20] = 0; ui3->uFluxCapControl[21] = 0; ui3->uFluxCapControl[22] = 0; ui3->uFluxCapControl[23] = 0; ui3->uFluxCapControl[24] = 0; ui3->uFluxCapControl[25] = 0; ui3->uFluxCapControl[26] = 0; ui3->uFluxCapControl[27] = 0; ui3->uFluxCapControl[28] = 0; ui3->uFluxCapControl[29] = 0; ui3->uFluxCapControl[30] = 0; ui3->uFluxCapControl[31] = 0; ui3->uFluxCapControl[32] = 0; ui3->uFluxCapControl[33] = 0; ui3->uFluxCapControl[34] = 0; ui3->uFluxCapControl[35] = 0; ui3->uFluxCapControl[36] = 0; ui3->uFluxCapControl[37] = 0; ui3->uFluxCapControl[38] = 0; ui3->uFluxCapControl[39] = 0; ui3->uFluxCapControl[40] = 0; ui3->uFluxCapControl[41] = 0; ui3->uFluxCapControl[42] = 0; ui3->uFluxCapControl[43] = 0; ui3->uFluxCapControl[44] = 0; ui3->uFluxCapControl[45] = 0; ui3->uFluxCapControl[46] = 0; ui3->uFluxCapControl[47] = 0; ui3->uFluxCapControl[48] = 0; ui3->uFluxCapControl[49] = 0; ui3->uFluxCapControl[50] = 0; ui3->uFluxCapControl[51] = 0; ui3->uFluxCapControl[52] = 0; ui3->uFluxCapControl[53] = 0; ui3->uFluxCapControl[54] = 0; ui3->uFluxCapControl[55] = 0; ui3->uFluxCapControl[56] = 0; ui3->uFluxCapControl[57] = 0; ui3->uFluxCapControl[58] = 0; ui3->uFluxCapControl[59] = 0; ui3->uFluxCapControl[60] = 0; ui3->uFluxCapControl[61] = 0; ui3->uFluxCapControl[62] = 0; ui3->uFluxCapControl[63] = 0; 
	ui3->fFluxCapData[0] = 0.000000; ui3->fFluxCapData[1] = 0.000000; ui3->fFluxCapData[2] = 0.000000; ui3->fFluxCapData[3] = 0.000000; ui3->fFluxCapData[4] = 0.000000; ui3->fFluxCapData[5] = 0.000000; ui3->fFluxCapData[6] = 0.000000; ui3->fFluxCapData[7] = 0.000000; ui3->fFluxCapData[8] = 0.000000; ui3->fFluxCapData[9] = 0.000000; ui3->fFluxCapData[10] = 0.000000; ui3->fFluxCapData[11] = 0.000000; ui3->fFluxCapData[12] = 0.000000; ui3->fFluxCapData[13] = 0.000000; ui3->fFluxCapData[14] = 0.000000; ui3->fFluxCapData[15] = 0.000000; ui3->fFluxCapData[16] = 0.000000; ui3->fFluxCapData[17] = 0.000000; ui3->fFluxCapData[18] = 0.000000; ui3->fFluxCapData[19] = 0.000000; ui3->fFluxCapData[20] = 0.000000; ui3->fFluxCapData[21] = 0.000000; ui3->fFluxCapData[22] = 0.000000; ui3->fFluxCapData[23] = 0.000000; ui3->fFluxCapData[24] = 0.000000; ui3->fFluxCapData[25] = 0.000000; ui3->fFluxCapData[26] = 0.000000; ui3->fFluxCapData[27] = 0.000000; ui3->fFluxCapData[28] = 0.000000; ui3->fFluxCapData[29] = 0.000000; ui3->fFluxCapData[30] = 0.000000; ui3->fFluxCapData[31] = 0.000000; ui3->fFluxCapData[32] = 0.000000; ui3->fFluxCapData[33] = 0.000000; ui3->fFluxCapData[34] = 0.000000; ui3->fFluxCapData[35] = 0.000000; ui3->fFluxCapData[36] = 0.000000; ui3->fFluxCapData[37] = 0.000000; ui3->fFluxCapData[38] = 0.000000; ui3->fFluxCapData[39] = 0.000000; ui3->fFluxCapData[40] = 0.000000; ui3->fFluxCapData[41] = 0.000000; ui3->fFluxCapData[42] = 0.000000; ui3->fFluxCapData[43] = 0.000000; ui3->fFluxCapData[44] = 0.000000; ui3->fFluxCapData[45] = 0.000000; ui3->fFluxCapData[46] = 0.000000; ui3->fFluxCapData[47] = 0.000000; ui3->fFluxCapData[48] = 0.000000; ui3->fFluxCapData[49] = 0.000000; ui3->fFluxCapData[50] = 0.000000; ui3->fFluxCapData[51] = 0.000000; ui3->fFluxCapData[52] = 0.000000; ui3->fFluxCapData[53] = 0.000000; ui3->fFluxCapData[54] = 0.000000; ui3->fFluxCapData[55] = 0.000000; ui3->fFluxCapData[56] = 0.000000; ui3->fFluxCapData[57] = 0.000000; ui3->fFluxCapData[58] = 0.000000; ui3->fFluxCapData[59] = 0.000000; ui3->fFluxCapData[60] = 0.000000; ui3->fFluxCapData[61] = 0.000000; ui3->fFluxCapData[62] = 0.000000; ui3->fFluxCapData[63] = 0.000000; 
	m_UIControlList.append(*ui3);
	delete ui3;


	m_uOscSourceR = 0;
	CUICtrl* ui4 = new CUICtrl;
	ui4->uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
	ui4->uControlId = 4;
	ui4->bLogSlider = false;
	ui4->bExpSlider = false;
	ui4->fUserDisplayDataLoLimit = 0.000000;
	ui4->fUserDisplayDataHiLimit = 10.000000;
	ui4->uUserDataType = UINTData;
	ui4->fInitUserIntValue = 0;
	ui4->fInitUserFloatValue = 0;
	ui4->fInitUserDoubleValue = 0;
	ui4->fInitUserUINTValue = 0.000000;
	ui4->m_pUserCookedIntData = NULL;
	ui4->m_pUserCookedFloatData = NULL;
	ui4->m_pUserCookedDoubleData = NULL;
	ui4->m_pUserCookedUINTData = &m_uOscSourceR;
	ui4->cControlUnits = "                                                                ";
	ui4->cVariableName = "m_uOscSourceR";
	ui4->cEnumeratedList = "sine,white,pink,car,rain,ocean,tape,vinyl,male,female,user";
	ui4->dPresetData[0] = 0.000000;ui4->dPresetData[1] = 0.000000;ui4->dPresetData[2] = 0.000000;ui4->dPresetData[3] = 0.000000;ui4->dPresetData[4] = 0.000000;ui4->dPresetData[5] = 0.000000;ui4->dPresetData[6] = 0.000000;ui4->dPresetData[7] = 0.000000;ui4->dPresetData[8] = 0.000000;ui4->dPresetData[9] = 0.000000;ui4->dPresetData[10] = 0.000000;ui4->dPresetData[11] = 0.000000;ui4->dPresetData[12] = 0.000000;ui4->dPresetData[13] = 0.000000;ui4->dPresetData[14] = 0.000000;ui4->dPresetData[15] = 0.000000;
	ui4->cControlName = "Osc R Source";
	ui4->bOwnerControl = false;
	ui4->bMIDIControl = false;
	ui4->uMIDIControlCommand = 176;
	ui4->uMIDIControlName = 3;
	ui4->uMIDIControlChannel = 0;
	ui4->nGUIRow = -1;
	ui4->nGUIColumn = -1;
	ui4->uControlTheme[0] = 0; ui4->uControlTheme[1] = 0; ui4->uControlTheme[2] = 0; ui4->uControlTheme[3] = 0; ui4->uControlTheme[4] = 0; ui4->uControlTheme[5] = 0; ui4->uControlTheme[6] = 0; ui4->uControlTheme[7] = 0; ui4->uControlTheme[8] = 0; ui4->uControlTheme[9] = 0; ui4->uControlTheme[10] = 0; ui4->uControlTheme[11] = 0; ui4->uControlTheme[12] = 0; ui4->uControlTheme[13] = 0; ui4->uControlTheme[14] = 0; ui4->uControlTheme[15] = 0; ui4->uControlTheme[16] = 0; ui4->uControlTheme[17] = 0; ui4->uControlTheme[18] = 0; ui4->uControlTheme[19] = 0; ui4->uControlTheme[20] = 0; ui4->uControlTheme[21] = 0; ui4->uControlTheme[22] = 0; ui4->uControlTheme[23] = 0; ui4->uControlTheme[24] = 0; ui4->uControlTheme[25] = 0; ui4->uControlTheme[26] = 0; ui4->uControlTheme[27] = 1; ui4->uControlTheme[28] = 0; ui4->uControlTheme[29] = 0; ui4->uControlTheme[30] = 0; ui4->uControlTheme[31] = 0; 
	ui4->uFluxCapControl[0] = 0; ui4->uFluxCapControl[1] = 0; ui4->uFluxCapControl[2] = 0; ui4->uFluxCapControl[3] = 0; ui4->uFluxCapControl[4] = 0; ui4->uFluxCapControl[5] = 0; ui4->uFluxCapControl[6] = 0; ui4->uFluxCapControl[7] = 0; ui4->uFluxCapControl[8] = 0; ui4->uFluxCapControl[9] = 0; ui4->uFluxCapControl[10] = 0; ui4->uFluxCapControl[11] = 0; ui4->uFluxCapControl[12] = 0; ui4->uFluxCapControl[13] = 0; ui4->uFluxCapControl[14] = 0; ui4->uFluxCapControl[15] = 0; ui4->uFluxCapControl[16] = 0; ui4->uFluxCapControl[17] = 0; ui4->uFluxCapControl[18] = 0; ui4->uFluxCapControl[19] = 0; ui4->uFluxCapControl[20] = 0; ui4->uFluxCapControl[21] = 0; ui4->uFluxCapControl[22] = 0; ui4->uFluxCapControl[23] = 0; ui4->uFluxCapControl[24] = 0; ui4->uFluxCapControl[25] = 0; ui4->uFluxCapControl[26] = 0; ui4->uFluxCapControl[27] = 0; ui4->uFluxCapControl[28] = 0; ui4->uFluxCapControl[29] = 0; ui4->uFluxCapControl[30] = 0; ui4->uFluxCapControl[31] = 0; ui4->uFluxCapControl[32] = 0; ui4->uFluxCapControl[33] = 0; ui4->uFluxCapControl[34] = 0; ui4->uFluxCapControl[35] = 0; ui4->uFluxCapControl[36] = 0; ui4->uFluxCapControl[37] = 0; ui4->uFluxCapControl[38] = 0; ui4->uFluxCapControl[39] = 0; ui4->uFluxCapControl[40] = 0; ui4->uFluxCapControl[41] = 0; ui4->uFluxCapControl[42] = 0; ui4->uFluxCapControl[43] = 0; ui4->uFluxCapControl[44] = 0; ui4->uFluxCapControl[45] = 0; ui4->uFluxCapControl[46] = 0; ui4->uFluxCapControl[47] = 0; ui4->uFluxCapControl[48] = 0; ui4->uFluxCapControl[49] = 0; ui4->uFluxCapControl[50] = 0; ui4->uFluxCapControl[51] = 0; ui4->uFluxCapControl[52] = 0; ui4->uFluxCapControl[53] = 0; ui4->uFluxCapControl[54] = 0; ui4->uFluxCapControl[55] = 0; ui4->uFluxCapControl[56] = 0; ui4->uFluxCapControl[57] = 0; ui4->uFluxCapControl[58] = 0; ui4->uFluxCapControl[59] = 0; ui4->uFluxCapControl[60] = 0; ui4->uFluxCapControl[61] = 0; ui4->uFluxCapControl[62] = 0; ui4->uFluxCapControl[63] = 0; 
	ui4->fFluxCapData[0] = 0.000000; ui4->fFluxCapData[1] = 0.000000; ui4->fFluxCapData[2] = 0.000000; ui4->fFluxCapData[3] = 0.000000; ui4->fFluxCapData[4] = 0.000000; ui4->fFluxCapData[5] = 0.000000; ui4->fFluxCapData[6] = 0.000000; ui4->fFluxCapData[7] = 0.000000; ui4->fFluxCapData[8] = 0.000000; ui4->fFluxCapData[9] = 0.000000; ui4->fFluxCapData[10] = 0.000000; ui4->fFluxCapData[11] = 0.000000; ui4->fFluxCapData[12] = 0.000000; ui4->fFluxCapData[13] = 0.000000; ui4->fFluxCapData[14] = 0.000000; ui4->fFluxCapData[15] = 0.000000; ui4->fFluxCapData[16] = 0.000000; ui4->fFluxCapData[17] = 0.000000; ui4->fFluxCapData[18] = 0.000000; ui4->fFluxCapData[19] = 0.000000; ui4->fFluxCapData[20] = 0.000000; ui4->fFluxCapData[21] = 0.000000; ui4->fFluxCapData[22] = 0.000000; ui4->fFluxCapData[23] = 0.000000; ui4->fFluxCapData[24] = 0.000000; ui4->fFluxCapData[25] = 0.000000; ui4->fFluxCapData[26] = 0.000000; ui4->fFluxCapData[27] = 0.000000; ui4->fFluxCapData[28] = 0.000000; ui4->fFluxCapData[29] = 0.000000; ui4->fFluxCapData[30] = 0.000000; ui4->fFluxCapData[31] = 0.000000; ui4->fFluxCapData[32] = 0.000000; ui4->fFluxCapData[33] = 0.000000; ui4->fFluxCapData[34] = 0.000000; ui4->fFluxCapData[35] = 0.000000; ui4->fFluxCapData[36] = 0.000000; ui4->fFluxCapData[37] = 0.000000; ui4->fFluxCapData[38] = 0.000000; ui4->fFluxCapData[39] = 0.000000; ui4->fFluxCapData[40] = 0.000000; ui4->fFluxCapData[41] = 0.000000; ui4->fFluxCapData[42] = 0.000000; ui4->fFluxCapData[43] = 0.000000; ui4->fFluxCapData[44] = 0.000000; ui4->fFluxCapData[45] = 0.000000; ui4->fFluxCapData[46] = 0.000000; ui4->fFluxCapData[47] = 0.000000; ui4->fFluxCapData[48] = 0.000000; ui4->fFluxCapData[49] = 0.000000; ui4->fFluxCapData[50] = 0.000000; ui4->fFluxCapData[51] = 0.000000; ui4->fFluxCapData[52] = 0.000000; ui4->fFluxCapData[53] = 0.000000; ui4->fFluxCapData[54] = 0.000000; ui4->fFluxCapData[55] = 0.000000; ui4->fFluxCapData[56] = 0.000000; ui4->fFluxCapData[57] = 0.000000; ui4->fFluxCapData[58] = 0.000000; ui4->fFluxCapData[59] = 0.000000; ui4->fFluxCapData[60] = 0.000000; ui4->fFluxCapData[61] = 0.000000; ui4->fFluxCapData[62] = 0.000000; ui4->fFluxCapData[63] = 0.000000; 
	m_UIControlList.append(*ui4);
	delete ui4;


	m_fBrightness = 0.000000;
	CUICtrl* ui5 = new CUICtrl;
	ui5->uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
	ui5->uControlId = 5;
	ui5->bLogSlider = false;
	ui5->bExpSlider = false;
	ui5->fUserDisplayDataLoLimit = 0.000000;
	ui5->fUserDisplayDataHiLimit = 100.000000;
	ui5->uUserDataType = floatData;
	ui5->fInitUserIntValue = 0;
	ui5->fInitUserFloatValue = 0.000000;
	ui5->fInitUserDoubleValue = 0;
	ui5->fInitUserUINTValue = 0;
	ui5->m_pUserCookedIntData = NULL;
	ui5->m_pUserCookedFloatData = &m_fBrightness;
	ui5->m_pUserCookedDoubleData = NULL;
	ui5->m_pUserCookedUINTData = NULL;
	ui5->cControlUnits = "Units                                                           ";
	ui5->cVariableName = "m_fBrightness";
	ui5->cEnumeratedList = "SEL1,SEL2,SEL3";
	ui5->dPresetData[0] = 0.000000;ui5->dPresetData[1] = 0.000000;ui5->dPresetData[2] = 0.000000;ui5->dPresetData[3] = 0.000000;ui5->dPresetData[4] = 0.000000;ui5->dPresetData[5] = 0.000000;ui5->dPresetData[6] = 0.000000;ui5->dPresetData[7] = 0.000000;ui5->dPresetData[8] = 0.000000;ui5->dPresetData[9] = 0.000000;ui5->dPresetData[10] = 0.000000;ui5->dPresetData[11] = 0.000000;ui5->dPresetData[12] = 0.000000;ui5->dPresetData[13] = 0.000000;ui5->dPresetData[14] = 0.000000;ui5->dPresetData[15] = 0.000000;
	ui5->cControlName = "Brighten";
	ui5->bOwnerControl = false;
	ui5->bMIDIControl = false;
	ui5->uMIDIControlCommand = 176;
	ui5->uMIDIControlName = 3;
	ui5->uMIDIControlChannel = 0;
	ui5->nGUIRow = -1;
	ui5->nGUIColumn = -1;
	ui5->uControlTheme[0] = 0; ui5->uControlTheme[1] = 0; ui5->uControlTheme[2] = 0; ui5->uControlTheme[3] = 0; ui5->uControlTheme[4] = 0; ui5->uControlTheme[5] = 0; ui5->uControlTheme[6] = 0; ui5->uControlTheme[7] = 0; ui5->uControlTheme[8] = 0; ui5->uControlTheme[9] = 0; ui5->uControlTheme[10] = 0; ui5->uControlTheme[11] = 0; ui5->uControlTheme[12] = 0; ui5->uControlTheme[13] = 0; ui5->uControlTheme[14] = 0; ui5->uControlTheme[15] = 0; ui5->uControlTheme[16] = 0; ui5->uControlTheme[17] = 0; ui5->uControlTheme[18] = 0; ui5->uControlTheme[19] = 0; ui5->uControlTheme[20] = 0; ui5->uControlTheme[21] = 0; ui5->uControlTheme[22] = 0; ui5->uControlTheme[23] = 0; ui5->uControlTheme[24] = 0; ui5->uControlTheme[25] = 0; ui5->uControlTheme[26] = 0; ui5->uControlTheme[27] = 1; ui5->uControlTheme[28] = 0; ui5->uControlTheme[29] = 0; ui5->uControlTheme[30] = 0; ui5->uControlTheme[31] = 0; 
	ui5->uFluxCapControl[0] = 0; ui5->uFluxCapControl[1] = 0; ui5->uFluxCapControl[2] = 0; ui5->uFluxCapControl[3] = 0; ui5->uFluxCapControl[4] = 0; ui5->uFluxCapControl[5] = 0; ui5->uFluxCapControl[6] = 0; ui5->uFluxCapControl[7] = 0; ui5->uFluxCapControl[8] = 0; ui5->uFluxCapControl[9] = 0; ui5->uFluxCapControl[10] = 0; ui5->uFluxCapControl[11] = 0; ui5->uFluxCapControl[12] = 0; ui5->uFluxCapControl[13] = 0; ui5->uFluxCapControl[14] = 0; ui5->uFluxCapControl[15] = 0; ui5->uFluxCapControl[16] = 0; ui5->uFluxCapControl[17] = 0; ui5->uFluxCapControl[18] = 0; ui5->uFluxCapControl[19] = 0; ui5->uFluxCapControl[20] = 0; ui5->uFluxCapControl[21] = 0; ui5->uFluxCapControl[22] = 0; ui5->uFluxCapControl[23] = 0; ui5->uFluxCapControl[24] = 0; ui5->uFluxCapControl[25] = 0; ui5->uFluxCapControl[26] = 0; ui5->uFluxCapControl[27] = 0; ui5->uFluxCapControl[28] = 0; ui5->uFluxCapControl[29] = 0; ui5->uFluxCapControl[30] = 0; ui5->uFluxCapControl[31] = 0; ui5->uFluxCapControl[32] = 0; ui5->uFluxCapControl[33] = 0; ui5->uFluxCapControl[34] = 0; ui5->uFluxCapControl[35] = 0; ui5->uFluxCapControl[36] = 0; ui5->uFluxCapControl[37] = 0; ui5->uFluxCapControl[38] = 0; ui5->uFluxCapControl[39] = 0; ui5->uFluxCapControl[40] = 0; ui5->uFluxCapControl[41] = 0; ui5->uFluxCapControl[42] = 0; ui5->uFluxCapControl[43] = 0; ui5->uFluxCapControl[44] = 0; ui5->uFluxCapControl[45] = 0; ui5->uFluxCapControl[46] = 0; ui5->uFluxCapControl[47] = 0; ui5->uFluxCapControl[48] = 0; ui5->uFluxCapControl[49] = 0; ui5->uFluxCapControl[50] = 0; ui5->uFluxCapControl[51] = 0; ui5->uFluxCapControl[52] = 0; ui5->uFluxCapControl[53] = 0; ui5->uFluxCapControl[54] = 0; ui5->uFluxCapControl[55] = 0; ui5->uFluxCapControl[56] = 0; ui5->uFluxCapControl[57] = 0; ui5->uFluxCapControl[58] = 0; ui5->uFluxCapControl[59] = 0; ui5->uFluxCapControl[60] = 0; ui5->uFluxCapControl[61] = 0; ui5->uFluxCapControl[62] = 0; ui5->uFluxCapControl[63] = 0; 
	ui5->fFluxCapData[0] = 0.000000; ui5->fFluxCapData[1] = 0.000000; ui5->fFluxCapData[2] = 0.000000; ui5->fFluxCapData[3] = 0.000000; ui5->fFluxCapData[4] = 0.000000; ui5->fFluxCapData[5] = 0.000000; ui5->fFluxCapData[6] = 0.000000; ui5->fFluxCapData[7] = 0.000000; ui5->fFluxCapData[8] = 0.000000; ui5->fFluxCapData[9] = 0.000000; ui5->fFluxCapData[10] = 0.000000; ui5->fFluxCapData[11] = 0.000000; ui5->fFluxCapData[12] = 0.000000; ui5->fFluxCapData[13] = 0.000000; ui5->fFluxCapData[14] = 0.000000; ui5->fFluxCapData[15] = 0.000000; ui5->fFluxCapData[16] = 0.000000; ui5->fFluxCapData[17] = 0.000000; ui5->fFluxCapData[18] = 0.000000; ui5->fFluxCapData[19] = 0.000000; ui5->fFluxCapData[20] = 0.000000; ui5->fFluxCapData[21] = 0.000000; ui5->fFluxCapData[22] = 0.000000; ui5->fFluxCapData[23] = 0.000000; ui5->fFluxCapData[24] = 0.000000; ui5->fFluxCapData[25] = 0.000000; ui5->fFluxCapData[26] = 0.000000; ui5->fFluxCapData[27] = 0.000000; ui5->fFluxCapData[28] = 0.000000; ui5->fFluxCapData[29] = 0.000000; ui5->fFluxCapData[30] = 0.000000; ui5->fFluxCapData[31] = 0.000000; ui5->fFluxCapData[32] = 0.000000; ui5->fFluxCapData[33] = 0.000000; ui5->fFluxCapData[34] = 0.000000; ui5->fFluxCapData[35] = 0.000000; ui5->fFluxCapData[36] = 0.000000; ui5->fFluxCapData[37] = 0.000000; ui5->fFluxCapData[38] = 0.000000; ui5->fFluxCapData[39] = 0.000000; ui5->fFluxCapData[40] = 0.000000; ui5->fFluxCapData[41] = 0.000000; ui5->fFluxCapData[42] = 0.000000; ui5->fFluxCapData[43] = 0.000000; ui5->fFluxCapData[44] = 0.000000; ui5->fFluxCapData[45] = 0.000000; ui5->fFluxCapData[46] = 0.000000; ui5->fFluxCapData[47] = 0.000000; ui5->fFluxCapData[48] = 0.000000; ui5->fFluxCapData[49] = 0.000000; ui5->fFluxCapData[50] = 0.000000; ui5->fFluxCapData[51] = 0.000000; ui5->fFluxCapData[52] = 0.000000; ui5->fFluxCapData[53] = 0.000000; ui5->fFluxCapData[54] = 0.000000; ui5->fFluxCapData[55] = 0.000000; ui5->fFluxCapData[56] = 0.000000; ui5->fFluxCapData[57] = 0.000000; ui5->fFluxCapData[58] = 0.000000; ui5->fFluxCapData[59] = 0.000000; ui5->fFluxCapData[60] = 0.000000; ui5->fFluxCapData[61] = 0.000000; ui5->fFluxCapData[62] = 0.000000; ui5->fFluxCapData[63] = 0.000000; 
	m_UIControlList.append(*ui5);
	delete ui5;


	m_fDarken = 1.000000;
	CUICtrl* ui6 = new CUICtrl;
	ui6->uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
	ui6->uControlId = 6;
	ui6->bLogSlider = true;
	ui6->bExpSlider = false;
	ui6->fUserDisplayDataLoLimit = 1.000000;
	ui6->fUserDisplayDataHiLimit = 99.000000;
	ui6->uUserDataType = floatData;
	ui6->fInitUserIntValue = 0;
	ui6->fInitUserFloatValue = 1.000000;
	ui6->fInitUserDoubleValue = 0;
	ui6->fInitUserUINTValue = 0;
	ui6->m_pUserCookedIntData = NULL;
	ui6->m_pUserCookedFloatData = &m_fDarken;
	ui6->m_pUserCookedDoubleData = NULL;
	ui6->m_pUserCookedUINTData = NULL;
	ui6->cControlUnits = "Units                                                           ";
	ui6->cVariableName = "m_fDarken";
	ui6->cEnumeratedList = "SEL1,SEL2,SEL3";
	ui6->dPresetData[0] = 1.000000;ui6->dPresetData[1] = 0.000000;ui6->dPresetData[2] = 0.000000;ui6->dPresetData[3] = 0.000000;ui6->dPresetData[4] = 0.000000;ui6->dPresetData[5] = 0.000000;ui6->dPresetData[6] = 0.000000;ui6->dPresetData[7] = 0.000000;ui6->dPresetData[8] = 0.000000;ui6->dPresetData[9] = 0.000000;ui6->dPresetData[10] = 0.000000;ui6->dPresetData[11] = 0.000000;ui6->dPresetData[12] = 0.000000;ui6->dPresetData[13] = 0.000000;ui6->dPresetData[14] = 0.000000;ui6->dPresetData[15] = 0.000000;
	ui6->cControlName = "Darken";
	ui6->bOwnerControl = false;
	ui6->bMIDIControl = false;
	ui6->uMIDIControlCommand = 176;
	ui6->uMIDIControlName = 3;
	ui6->uMIDIControlChannel = 0;
	ui6->nGUIRow = -1;
	ui6->nGUIColumn = -1;
	ui6->uControlTheme[0] = 0; ui6->uControlTheme[1] = 0; ui6->uControlTheme[2] = 0; ui6->uControlTheme[3] = 0; ui6->uControlTheme[4] = 0; ui6->uControlTheme[5] = 0; ui6->uControlTheme[6] = 0; ui6->uControlTheme[7] = 0; ui6->uControlTheme[8] = 0; ui6->uControlTheme[9] = 0; ui6->uControlTheme[10] = 0; ui6->uControlTheme[11] = 0; ui6->uControlTheme[12] = 0; ui6->uControlTheme[13] = 0; ui6->uControlTheme[14] = 0; ui6->uControlTheme[15] = 0; ui6->uControlTheme[16] = 0; ui6->uControlTheme[17] = 0; ui6->uControlTheme[18] = 0; ui6->uControlTheme[19] = 0; ui6->uControlTheme[20] = 0; ui6->uControlTheme[21] = 0; ui6->uControlTheme[22] = 0; ui6->uControlTheme[23] = 0; ui6->uControlTheme[24] = 0; ui6->uControlTheme[25] = 0; ui6->uControlTheme[26] = 0; ui6->uControlTheme[27] = 1; ui6->uControlTheme[28] = 0; ui6->uControlTheme[29] = 0; ui6->uControlTheme[30] = 0; ui6->uControlTheme[31] = 0; 
	ui6->uFluxCapControl[0] = 0; ui6->uFluxCapControl[1] = 0; ui6->uFluxCapControl[2] = 0; ui6->uFluxCapControl[3] = 0; ui6->uFluxCapControl[4] = 0; ui6->uFluxCapControl[5] = 0; ui6->uFluxCapControl[6] = 0; ui6->uFluxCapControl[7] = 0; ui6->uFluxCapControl[8] = 0; ui6->uFluxCapControl[9] = 0; ui6->uFluxCapControl[10] = 0; ui6->uFluxCapControl[11] = 0; ui6->uFluxCapControl[12] = 0; ui6->uFluxCapControl[13] = 0; ui6->uFluxCapControl[14] = 0; ui6->uFluxCapControl[15] = 0; ui6->uFluxCapControl[16] = 0; ui6->uFluxCapControl[17] = 0; ui6->uFluxCapControl[18] = 0; ui6->uFluxCapControl[19] = 0; ui6->uFluxCapControl[20] = 0; ui6->uFluxCapControl[21] = 0; ui6->uFluxCapControl[22] = 0; ui6->uFluxCapControl[23] = 0; ui6->uFluxCapControl[24] = 0; ui6->uFluxCapControl[25] = 0; ui6->uFluxCapControl[26] = 0; ui6->uFluxCapControl[27] = 0; ui6->uFluxCapControl[28] = 0; ui6->uFluxCapControl[29] = 0; ui6->uFluxCapControl[30] = 0; ui6->uFluxCapControl[31] = 0; ui6->uFluxCapControl[32] = 0; ui6->uFluxCapControl[33] = 0; ui6->uFluxCapControl[34] = 0; ui6->uFluxCapControl[35] = 0; ui6->uFluxCapControl[36] = 0; ui6->uFluxCapControl[37] = 0; ui6->uFluxCapControl[38] = 0; ui6->uFluxCapControl[39] = 0; ui6->uFluxCapControl[40] = 0; ui6->uFluxCapControl[41] = 0; ui6->uFluxCapControl[42] = 0; ui6->uFluxCapControl[43] = 0; ui6->uFluxCapControl[44] = 0; ui6->uFluxCapControl[45] = 0; ui6->uFluxCapControl[46] = 0; ui6->uFluxCapControl[47] = 0; ui6->uFluxCapControl[48] = 0; ui6->uFluxCapControl[49] = 0; ui6->uFluxCapControl[50] = 0; ui6->uFluxCapControl[51] = 0; ui6->uFluxCapControl[52] = 0; ui6->uFluxCapControl[53] = 0; ui6->uFluxCapControl[54] = 0; ui6->uFluxCapControl[55] = 0; ui6->uFluxCapControl[56] = 0; ui6->uFluxCapControl[57] = 0; ui6->uFluxCapControl[58] = 0; ui6->uFluxCapControl[59] = 0; ui6->uFluxCapControl[60] = 0; ui6->uFluxCapControl[61] = 0; ui6->uFluxCapControl[62] = 0; ui6->uFluxCapControl[63] = 0; 
	ui6->fFluxCapData[0] = 0.000000; ui6->fFluxCapData[1] = 0.000000; ui6->fFluxCapData[2] = 0.000000; ui6->fFluxCapData[3] = 0.000000; ui6->fFluxCapData[4] = 0.000000; ui6->fFluxCapData[5] = 0.000000; ui6->fFluxCapData[6] = 0.000000; ui6->fFluxCapData[7] = 0.000000; ui6->fFluxCapData[8] = 0.000000; ui6->fFluxCapData[9] = 0.000000; ui6->fFluxCapData[10] = 0.000000; ui6->fFluxCapData[11] = 0.000000; ui6->fFluxCapData[12] = 0.000000; ui6->fFluxCapData[13] = 0.000000; ui6->fFluxCapData[14] = 0.000000; ui6->fFluxCapData[15] = 0.000000; ui6->fFluxCapData[16] = 0.000000; ui6->fFluxCapData[17] = 0.000000; ui6->fFluxCapData[18] = 0.000000; ui6->fFluxCapData[19] = 0.000000; ui6->fFluxCapData[20] = 0.000000; ui6->fFluxCapData[21] = 0.000000; ui6->fFluxCapData[22] = 0.000000; ui6->fFluxCapData[23] = 0.000000; ui6->fFluxCapData[24] = 0.000000; ui6->fFluxCapData[25] = 0.000000; ui6->fFluxCapData[26] = 0.000000; ui6->fFluxCapData[27] = 0.000000; ui6->fFluxCapData[28] = 0.000000; ui6->fFluxCapData[29] = 0.000000; ui6->fFluxCapData[30] = 0.000000; ui6->fFluxCapData[31] = 0.000000; ui6->fFluxCapData[32] = 0.000000; ui6->fFluxCapData[33] = 0.000000; ui6->fFluxCapData[34] = 0.000000; ui6->fFluxCapData[35] = 0.000000; ui6->fFluxCapData[36] = 0.000000; ui6->fFluxCapData[37] = 0.000000; ui6->fFluxCapData[38] = 0.000000; ui6->fFluxCapData[39] = 0.000000; ui6->fFluxCapData[40] = 0.000000; ui6->fFluxCapData[41] = 0.000000; ui6->fFluxCapData[42] = 0.000000; ui6->fFluxCapData[43] = 0.000000; ui6->fFluxCapData[44] = 0.000000; ui6->fFluxCapData[45] = 0.000000; ui6->fFluxCapData[46] = 0.000000; ui6->fFluxCapData[47] = 0.000000; ui6->fFluxCapData[48] = 0.000000; ui6->fFluxCapData[49] = 0.000000; ui6->fFluxCapData[50] = 0.000000; ui6->fFluxCapData[51] = 0.000000; ui6->fFluxCapData[52] = 0.000000; ui6->fFluxCapData[53] = 0.000000; ui6->fFluxCapData[54] = 0.000000; ui6->fFluxCapData[55] = 0.000000; ui6->fFluxCapData[56] = 0.000000; ui6->fFluxCapData[57] = 0.000000; ui6->fFluxCapData[58] = 0.000000; ui6->fFluxCapData[59] = 0.000000; ui6->fFluxCapData[60] = 0.000000; ui6->fFluxCapData[61] = 0.000000; ui6->fFluxCapData[62] = 0.000000; ui6->fFluxCapData[63] = 0.000000; 
	m_UIControlList.append(*ui6);
	delete ui6;


	m_fLFODepth = 0.990000;
	CUICtrl* ui7 = new CUICtrl;
	ui7->uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
	ui7->uControlId = 7;
	ui7->bLogSlider = false;
	ui7->bExpSlider = false;
	ui7->fUserDisplayDataLoLimit = 0.000000;
	ui7->fUserDisplayDataHiLimit = 1.000000;
	ui7->uUserDataType = floatData;
	ui7->fInitUserIntValue = 0;
	ui7->fInitUserFloatValue = 0.990000;
	ui7->fInitUserDoubleValue = 0;
	ui7->fInitUserUINTValue = 0;
	ui7->m_pUserCookedIntData = NULL;
	ui7->m_pUserCookedFloatData = &m_fLFODepth;
	ui7->m_pUserCookedDoubleData = NULL;
	ui7->m_pUserCookedUINTData = NULL;
	ui7->cControlUnits = "Units                                                           ";
	ui7->cVariableName = "m_fLFODepth";
	ui7->cEnumeratedList = "SEL1,SEL2,SEL3";
	ui7->dPresetData[0] = 0.990000;ui7->dPresetData[1] = 0.000000;ui7->dPresetData[2] = 0.000000;ui7->dPresetData[3] = 0.000000;ui7->dPresetData[4] = 0.000000;ui7->dPresetData[5] = 0.000000;ui7->dPresetData[6] = 0.000000;ui7->dPresetData[7] = 0.000000;ui7->dPresetData[8] = 0.000000;ui7->dPresetData[9] = 0.000000;ui7->dPresetData[10] = 0.000000;ui7->dPresetData[11] = 0.000000;ui7->dPresetData[12] = 0.000000;ui7->dPresetData[13] = 0.000000;ui7->dPresetData[14] = 0.000000;ui7->dPresetData[15] = 0.000000;
	ui7->cControlName = "Depth";
	ui7->bOwnerControl = false;
	ui7->bMIDIControl = false;
	ui7->uMIDIControlCommand = 176;
	ui7->uMIDIControlName = 3;
	ui7->uMIDIControlChannel = 0;
	ui7->nGUIRow = -1;
	ui7->nGUIColumn = -1;
	ui7->uControlTheme[0] = 0; ui7->uControlTheme[1] = 0; ui7->uControlTheme[2] = 0; ui7->uControlTheme[3] = 0; ui7->uControlTheme[4] = 0; ui7->uControlTheme[5] = 0; ui7->uControlTheme[6] = 0; ui7->uControlTheme[7] = 0; ui7->uControlTheme[8] = 0; ui7->uControlTheme[9] = 0; ui7->uControlTheme[10] = 0; ui7->uControlTheme[11] = 0; ui7->uControlTheme[12] = 0; ui7->uControlTheme[13] = 0; ui7->uControlTheme[14] = 0; ui7->uControlTheme[15] = 0; ui7->uControlTheme[16] = 0; ui7->uControlTheme[17] = 0; ui7->uControlTheme[18] = 0; ui7->uControlTheme[19] = 0; ui7->uControlTheme[20] = 0; ui7->uControlTheme[21] = 0; ui7->uControlTheme[22] = 0; ui7->uControlTheme[23] = 0; ui7->uControlTheme[24] = 0; ui7->uControlTheme[25] = 0; ui7->uControlTheme[26] = 0; ui7->uControlTheme[27] = 1; ui7->uControlTheme[28] = 0; ui7->uControlTheme[29] = 0; ui7->uControlTheme[30] = 0; ui7->uControlTheme[31] = 0; 
	ui7->uFluxCapControl[0] = 0; ui7->uFluxCapControl[1] = 0; ui7->uFluxCapControl[2] = 0; ui7->uFluxCapControl[3] = 0; ui7->uFluxCapControl[4] = 0; ui7->uFluxCapControl[5] = 0; ui7->uFluxCapControl[6] = 0; ui7->uFluxCapControl[7] = 0; ui7->uFluxCapControl[8] = 0; ui7->uFluxCapControl[9] = 0; ui7->uFluxCapControl[10] = 0; ui7->uFluxCapControl[11] = 0; ui7->uFluxCapControl[12] = 0; ui7->uFluxCapControl[13] = 0; ui7->uFluxCapControl[14] = 0; ui7->uFluxCapControl[15] = 0; ui7->uFluxCapControl[16] = 0; ui7->uFluxCapControl[17] = 0; ui7->uFluxCapControl[18] = 0; ui7->uFluxCapControl[19] = 0; ui7->uFluxCapControl[20] = 0; ui7->uFluxCapControl[21] = 0; ui7->uFluxCapControl[22] = 0; ui7->uFluxCapControl[23] = 0; ui7->uFluxCapControl[24] = 0; ui7->uFluxCapControl[25] = 0; ui7->uFluxCapControl[26] = 0; ui7->uFluxCapControl[27] = 0; ui7->uFluxCapControl[28] = 0; ui7->uFluxCapControl[29] = 0; ui7->uFluxCapControl[30] = 0; ui7->uFluxCapControl[31] = 0; ui7->uFluxCapControl[32] = 0; ui7->uFluxCapControl[33] = 0; ui7->uFluxCapControl[34] = 0; ui7->uFluxCapControl[35] = 0; ui7->uFluxCapControl[36] = 0; ui7->uFluxCapControl[37] = 0; ui7->uFluxCapControl[38] = 0; ui7->uFluxCapControl[39] = 0; ui7->uFluxCapControl[40] = 0; ui7->uFluxCapControl[41] = 0; ui7->uFluxCapControl[42] = 0; ui7->uFluxCapControl[43] = 0; ui7->uFluxCapControl[44] = 0; ui7->uFluxCapControl[45] = 0; ui7->uFluxCapControl[46] = 0; ui7->uFluxCapControl[47] = 0; ui7->uFluxCapControl[48] = 0; ui7->uFluxCapControl[49] = 0; ui7->uFluxCapControl[50] = 0; ui7->uFluxCapControl[51] = 0; ui7->uFluxCapControl[52] = 0; ui7->uFluxCapControl[53] = 0; ui7->uFluxCapControl[54] = 0; ui7->uFluxCapControl[55] = 0; ui7->uFluxCapControl[56] = 0; ui7->uFluxCapControl[57] = 0; ui7->uFluxCapControl[58] = 0; ui7->uFluxCapControl[59] = 0; ui7->uFluxCapControl[60] = 0; ui7->uFluxCapControl[61] = 0; ui7->uFluxCapControl[62] = 0; ui7->uFluxCapControl[63] = 0; 
	ui7->fFluxCapData[0] = 0.000000; ui7->fFluxCapData[1] = 0.000000; ui7->fFluxCapData[2] = 0.000000; ui7->fFluxCapData[3] = 0.000000; ui7->fFluxCapData[4] = 0.000000; ui7->fFluxCapData[5] = 0.000000; ui7->fFluxCapData[6] = 0.000000; ui7->fFluxCapData[7] = 0.000000; ui7->fFluxCapData[8] = 0.000000; ui7->fFluxCapData[9] = 0.000000; ui7->fFluxCapData[10] = 0.000000; ui7->fFluxCapData[11] = 0.000000; ui7->fFluxCapData[12] = 0.000000; ui7->fFluxCapData[13] = 0.000000; ui7->fFluxCapData[14] = 0.000000; ui7->fFluxCapData[15] = 0.000000; ui7->fFluxCapData[16] = 0.000000; ui7->fFluxCapData[17] = 0.000000; ui7->fFluxCapData[18] = 0.000000; ui7->fFluxCapData[19] = 0.000000; ui7->fFluxCapData[20] = 0.000000; ui7->fFluxCapData[21] = 0.000000; ui7->fFluxCapData[22] = 0.000000; ui7->fFluxCapData[23] = 0.000000; ui7->fFluxCapData[24] = 0.000000; ui7->fFluxCapData[25] = 0.000000; ui7->fFluxCapData[26] = 0.000000; ui7->fFluxCapData[27] = 0.000000; ui7->fFluxCapData[28] = 0.000000; ui7->fFluxCapData[29] = 0.000000; ui7->fFluxCapData[30] = 0.000000; ui7->fFluxCapData[31] = 0.000000; ui7->fFluxCapData[32] = 0.000000; ui7->fFluxCapData[33] = 0.000000; ui7->fFluxCapData[34] = 0.000000; ui7->fFluxCapData[35] = 0.000000; ui7->fFluxCapData[36] = 0.000000; ui7->fFluxCapData[37] = 0.000000; ui7->fFluxCapData[38] = 0.000000; ui7->fFluxCapData[39] = 0.000000; ui7->fFluxCapData[40] = 0.000000; ui7->fFluxCapData[41] = 0.000000; ui7->fFluxCapData[42] = 0.000000; ui7->fFluxCapData[43] = 0.000000; ui7->fFluxCapData[44] = 0.000000; ui7->fFluxCapData[45] = 0.000000; ui7->fFluxCapData[46] = 0.000000; ui7->fFluxCapData[47] = 0.000000; ui7->fFluxCapData[48] = 0.000000; ui7->fFluxCapData[49] = 0.000000; ui7->fFluxCapData[50] = 0.000000; ui7->fFluxCapData[51] = 0.000000; ui7->fFluxCapData[52] = 0.000000; ui7->fFluxCapData[53] = 0.000000; ui7->fFluxCapData[54] = 0.000000; ui7->fFluxCapData[55] = 0.000000; ui7->fFluxCapData[56] = 0.000000; ui7->fFluxCapData[57] = 0.000000; ui7->fFluxCapData[58] = 0.000000; ui7->fFluxCapData[59] = 0.000000; ui7->fFluxCapData[60] = 0.000000; ui7->fFluxCapData[61] = 0.000000; ui7->fFluxCapData[62] = 0.000000; ui7->fFluxCapData[63] = 0.000000; 
	m_UIControlList.append(*ui7);
	delete ui7;


	m_fVolume = 0.000000;
	CUICtrl* ui8 = new CUICtrl;
	ui8->uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
	ui8->uControlId = 8;
	ui8->bLogSlider = false;
	ui8->bExpSlider = false;
	ui8->fUserDisplayDataLoLimit = -60.000000;
	ui8->fUserDisplayDataHiLimit = 6.000000;
	ui8->uUserDataType = floatData;
	ui8->fInitUserIntValue = 0;
	ui8->fInitUserFloatValue = 0.000000;
	ui8->fInitUserDoubleValue = 0;
	ui8->fInitUserUINTValue = 0;
	ui8->m_pUserCookedIntData = NULL;
	ui8->m_pUserCookedFloatData = &m_fVolume;
	ui8->m_pUserCookedDoubleData = NULL;
	ui8->m_pUserCookedUINTData = NULL;
	ui8->cControlUnits = "Units                                                           ";
	ui8->cVariableName = "m_fVolume";
	ui8->cEnumeratedList = "SEL1,SEL2,SEL3";
	ui8->dPresetData[0] = 0.000000;ui8->dPresetData[1] = 0.000000;ui8->dPresetData[2] = 0.000000;ui8->dPresetData[3] = 0.000000;ui8->dPresetData[4] = 0.000000;ui8->dPresetData[5] = 0.000000;ui8->dPresetData[6] = 0.000000;ui8->dPresetData[7] = 0.000000;ui8->dPresetData[8] = 0.000000;ui8->dPresetData[9] = 0.000000;ui8->dPresetData[10] = 0.000000;ui8->dPresetData[11] = 0.000000;ui8->dPresetData[12] = 0.000000;ui8->dPresetData[13] = 0.000000;ui8->dPresetData[14] = 0.000000;ui8->dPresetData[15] = 0.000000;
	ui8->cControlName = "Volume";
	ui8->bOwnerControl = false;
	ui8->bMIDIControl = false;
	ui8->uMIDIControlCommand = 176;
	ui8->uMIDIControlName = 3;
	ui8->uMIDIControlChannel = 0;
	ui8->nGUIRow = -1;
	ui8->nGUIColumn = -1;
	ui8->uControlTheme[0] = 0; ui8->uControlTheme[1] = 0; ui8->uControlTheme[2] = 0; ui8->uControlTheme[3] = 0; ui8->uControlTheme[4] = 0; ui8->uControlTheme[5] = 0; ui8->uControlTheme[6] = 0; ui8->uControlTheme[7] = 0; ui8->uControlTheme[8] = 0; ui8->uControlTheme[9] = 0; ui8->uControlTheme[10] = 0; ui8->uControlTheme[11] = 0; ui8->uControlTheme[12] = 0; ui8->uControlTheme[13] = 0; ui8->uControlTheme[14] = 0; ui8->uControlTheme[15] = 0; ui8->uControlTheme[16] = 0; ui8->uControlTheme[17] = 0; ui8->uControlTheme[18] = 0; ui8->uControlTheme[19] = 0; ui8->uControlTheme[20] = 0; ui8->uControlTheme[21] = 0; ui8->uControlTheme[22] = 0; ui8->uControlTheme[23] = 0; ui8->uControlTheme[24] = 0; ui8->uControlTheme[25] = 0; ui8->uControlTheme[26] = 0; ui8->uControlTheme[27] = 1; ui8->uControlTheme[28] = 0; ui8->uControlTheme[29] = 0; ui8->uControlTheme[30] = 0; ui8->uControlTheme[31] = 0; 
	ui8->uFluxCapControl[0] = 0; ui8->uFluxCapControl[1] = 0; ui8->uFluxCapControl[2] = 0; ui8->uFluxCapControl[3] = 0; ui8->uFluxCapControl[4] = 0; ui8->uFluxCapControl[5] = 0; ui8->uFluxCapControl[6] = 0; ui8->uFluxCapControl[7] = 0; ui8->uFluxCapControl[8] = 0; ui8->uFluxCapControl[9] = 0; ui8->uFluxCapControl[10] = 0; ui8->uFluxCapControl[11] = 0; ui8->uFluxCapControl[12] = 0; ui8->uFluxCapControl[13] = 0; ui8->uFluxCapControl[14] = 0; ui8->uFluxCapControl[15] = 0; ui8->uFluxCapControl[16] = 0; ui8->uFluxCapControl[17] = 0; ui8->uFluxCapControl[18] = 0; ui8->uFluxCapControl[19] = 0; ui8->uFluxCapControl[20] = 0; ui8->uFluxCapControl[21] = 0; ui8->uFluxCapControl[22] = 0; ui8->uFluxCapControl[23] = 0; ui8->uFluxCapControl[24] = 0; ui8->uFluxCapControl[25] = 0; ui8->uFluxCapControl[26] = 0; ui8->uFluxCapControl[27] = 0; ui8->uFluxCapControl[28] = 0; ui8->uFluxCapControl[29] = 0; ui8->uFluxCapControl[30] = 0; ui8->uFluxCapControl[31] = 0; ui8->uFluxCapControl[32] = 0; ui8->uFluxCapControl[33] = 0; ui8->uFluxCapControl[34] = 0; ui8->uFluxCapControl[35] = 0; ui8->uFluxCapControl[36] = 0; ui8->uFluxCapControl[37] = 0; ui8->uFluxCapControl[38] = 0; ui8->uFluxCapControl[39] = 0; ui8->uFluxCapControl[40] = 0; ui8->uFluxCapControl[41] = 0; ui8->uFluxCapControl[42] = 0; ui8->uFluxCapControl[43] = 0; ui8->uFluxCapControl[44] = 0; ui8->uFluxCapControl[45] = 0; ui8->uFluxCapControl[46] = 0; ui8->uFluxCapControl[47] = 0; ui8->uFluxCapControl[48] = 0; ui8->uFluxCapControl[49] = 0; ui8->uFluxCapControl[50] = 0; ui8->uFluxCapControl[51] = 0; ui8->uFluxCapControl[52] = 0; ui8->uFluxCapControl[53] = 0; ui8->uFluxCapControl[54] = 0; ui8->uFluxCapControl[55] = 0; ui8->uFluxCapControl[56] = 0; ui8->uFluxCapControl[57] = 0; ui8->uFluxCapControl[58] = 0; ui8->uFluxCapControl[59] = 0; ui8->uFluxCapControl[60] = 0; ui8->uFluxCapControl[61] = 0; ui8->uFluxCapControl[62] = 0; ui8->uFluxCapControl[63] = 0; 
	ui8->fFluxCapData[0] = 0.000000; ui8->fFluxCapData[1] = 0.000000; ui8->fFluxCapData[2] = 0.000000; ui8->fFluxCapData[3] = 0.000000; ui8->fFluxCapData[4] = 0.000000; ui8->fFluxCapData[5] = 0.000000; ui8->fFluxCapData[6] = 0.000000; ui8->fFluxCapData[7] = 0.000000; ui8->fFluxCapData[8] = 0.000000; ui8->fFluxCapData[9] = 0.000000; ui8->fFluxCapData[10] = 0.000000; ui8->fFluxCapData[11] = 0.000000; ui8->fFluxCapData[12] = 0.000000; ui8->fFluxCapData[13] = 0.000000; ui8->fFluxCapData[14] = 0.000000; ui8->fFluxCapData[15] = 0.000000; ui8->fFluxCapData[16] = 0.000000; ui8->fFluxCapData[17] = 0.000000; ui8->fFluxCapData[18] = 0.000000; ui8->fFluxCapData[19] = 0.000000; ui8->fFluxCapData[20] = 0.000000; ui8->fFluxCapData[21] = 0.000000; ui8->fFluxCapData[22] = 0.000000; ui8->fFluxCapData[23] = 0.000000; ui8->fFluxCapData[24] = 0.000000; ui8->fFluxCapData[25] = 0.000000; ui8->fFluxCapData[26] = 0.000000; ui8->fFluxCapData[27] = 0.000000; ui8->fFluxCapData[28] = 0.000000; ui8->fFluxCapData[29] = 0.000000; ui8->fFluxCapData[30] = 0.000000; ui8->fFluxCapData[31] = 0.000000; ui8->fFluxCapData[32] = 0.000000; ui8->fFluxCapData[33] = 0.000000; ui8->fFluxCapData[34] = 0.000000; ui8->fFluxCapData[35] = 0.000000; ui8->fFluxCapData[36] = 0.000000; ui8->fFluxCapData[37] = 0.000000; ui8->fFluxCapData[38] = 0.000000; ui8->fFluxCapData[39] = 0.000000; ui8->fFluxCapData[40] = 0.000000; ui8->fFluxCapData[41] = 0.000000; ui8->fFluxCapData[42] = 0.000000; ui8->fFluxCapData[43] = 0.000000; ui8->fFluxCapData[44] = 0.000000; ui8->fFluxCapData[45] = 0.000000; ui8->fFluxCapData[46] = 0.000000; ui8->fFluxCapData[47] = 0.000000; ui8->fFluxCapData[48] = 0.000000; ui8->fFluxCapData[49] = 0.000000; ui8->fFluxCapData[50] = 0.000000; ui8->fFluxCapData[51] = 0.000000; ui8->fFluxCapData[52] = 0.000000; ui8->fFluxCapData[53] = 0.000000; ui8->fFluxCapData[54] = 0.000000; ui8->fFluxCapData[55] = 0.000000; ui8->fFluxCapData[56] = 0.000000; ui8->fFluxCapData[57] = 0.000000; ui8->fFluxCapData[58] = 0.000000; ui8->fFluxCapData[59] = 0.000000; ui8->fFluxCapData[60] = 0.000000; ui8->fFluxCapData[61] = 0.000000; ui8->fFluxCapData[62] = 0.000000; ui8->fFluxCapData[63] = 0.000000; 
	m_UIControlList.append(*ui8);
	delete ui8;


	m_fOsc1Level = 100.000000;
	CUICtrl* ui9 = new CUICtrl;
	ui9->uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
	ui9->uControlId = 9;
	ui9->bLogSlider = false;
	ui9->bExpSlider = false;
	ui9->fUserDisplayDataLoLimit = 0.000000;
	ui9->fUserDisplayDataHiLimit = 100.000000;
	ui9->uUserDataType = floatData;
	ui9->fInitUserIntValue = 0;
	ui9->fInitUserFloatValue = 100.000000;
	ui9->fInitUserDoubleValue = 0;
	ui9->fInitUserUINTValue = 0;
	ui9->m_pUserCookedIntData = NULL;
	ui9->m_pUserCookedFloatData = &m_fOsc1Level;
	ui9->m_pUserCookedDoubleData = NULL;
	ui9->m_pUserCookedUINTData = NULL;
	ui9->cControlUnits = "Units                                                           ";
	ui9->cVariableName = "m_fOsc1Level";
	ui9->cEnumeratedList = "SEL1,SEL2,SEL3";
	ui9->dPresetData[0] = 100.000000;ui9->dPresetData[1] = 0.000000;ui9->dPresetData[2] = 0.000000;ui9->dPresetData[3] = 0.000000;ui9->dPresetData[4] = 0.000000;ui9->dPresetData[5] = 0.000000;ui9->dPresetData[6] = 0.000000;ui9->dPresetData[7] = 0.000000;ui9->dPresetData[8] = 0.000000;ui9->dPresetData[9] = 0.000000;ui9->dPresetData[10] = 0.000000;ui9->dPresetData[11] = 0.000000;ui9->dPresetData[12] = 0.000000;ui9->dPresetData[13] = 0.000000;ui9->dPresetData[14] = 0.000000;ui9->dPresetData[15] = 0.000000;
	ui9->cControlName = "Osc1 Volume";
	ui9->bOwnerControl = false;
	ui9->bMIDIControl = false;
	ui9->uMIDIControlCommand = 176;
	ui9->uMIDIControlName = 3;
	ui9->uMIDIControlChannel = 0;
	ui9->nGUIRow = -1;
	ui9->nGUIColumn = -1;
	ui9->uControlTheme[0] = 0; ui9->uControlTheme[1] = 0; ui9->uControlTheme[2] = 0; ui9->uControlTheme[3] = 0; ui9->uControlTheme[4] = 0; ui9->uControlTheme[5] = 0; ui9->uControlTheme[6] = 0; ui9->uControlTheme[7] = 0; ui9->uControlTheme[8] = 0; ui9->uControlTheme[9] = 0; ui9->uControlTheme[10] = 0; ui9->uControlTheme[11] = 0; ui9->uControlTheme[12] = 0; ui9->uControlTheme[13] = 0; ui9->uControlTheme[14] = 0; ui9->uControlTheme[15] = 0; ui9->uControlTheme[16] = 0; ui9->uControlTheme[17] = 0; ui9->uControlTheme[18] = 0; ui9->uControlTheme[19] = 0; ui9->uControlTheme[20] = 0; ui9->uControlTheme[21] = 0; ui9->uControlTheme[22] = 0; ui9->uControlTheme[23] = 0; ui9->uControlTheme[24] = 0; ui9->uControlTheme[25] = 0; ui9->uControlTheme[26] = 0; ui9->uControlTheme[27] = 1; ui9->uControlTheme[28] = 0; ui9->uControlTheme[29] = 0; ui9->uControlTheme[30] = 0; ui9->uControlTheme[31] = 0; 
	ui9->uFluxCapControl[0] = 0; ui9->uFluxCapControl[1] = 0; ui9->uFluxCapControl[2] = 0; ui9->uFluxCapControl[3] = 0; ui9->uFluxCapControl[4] = 0; ui9->uFluxCapControl[5] = 0; ui9->uFluxCapControl[6] = 0; ui9->uFluxCapControl[7] = 0; ui9->uFluxCapControl[8] = 0; ui9->uFluxCapControl[9] = 0; ui9->uFluxCapControl[10] = 0; ui9->uFluxCapControl[11] = 0; ui9->uFluxCapControl[12] = 0; ui9->uFluxCapControl[13] = 0; ui9->uFluxCapControl[14] = 0; ui9->uFluxCapControl[15] = 0; ui9->uFluxCapControl[16] = 0; ui9->uFluxCapControl[17] = 0; ui9->uFluxCapControl[18] = 0; ui9->uFluxCapControl[19] = 0; ui9->uFluxCapControl[20] = 0; ui9->uFluxCapControl[21] = 0; ui9->uFluxCapControl[22] = 0; ui9->uFluxCapControl[23] = 0; ui9->uFluxCapControl[24] = 0; ui9->uFluxCapControl[25] = 0; ui9->uFluxCapControl[26] = 0; ui9->uFluxCapControl[27] = 0; ui9->uFluxCapControl[28] = 0; ui9->uFluxCapControl[29] = 0; ui9->uFluxCapControl[30] = 0; ui9->uFluxCapControl[31] = 0; ui9->uFluxCapControl[32] = 0; ui9->uFluxCapControl[33] = 0; ui9->uFluxCapControl[34] = 0; ui9->uFluxCapControl[35] = 0; ui9->uFluxCapControl[36] = 0; ui9->uFluxCapControl[37] = 0; ui9->uFluxCapControl[38] = 0; ui9->uFluxCapControl[39] = 0; ui9->uFluxCapControl[40] = 0; ui9->uFluxCapControl[41] = 0; ui9->uFluxCapControl[42] = 0; ui9->uFluxCapControl[43] = 0; ui9->uFluxCapControl[44] = 0; ui9->uFluxCapControl[45] = 0; ui9->uFluxCapControl[46] = 0; ui9->uFluxCapControl[47] = 0; ui9->uFluxCapControl[48] = 0; ui9->uFluxCapControl[49] = 0; ui9->uFluxCapControl[50] = 0; ui9->uFluxCapControl[51] = 0; ui9->uFluxCapControl[52] = 0; ui9->uFluxCapControl[53] = 0; ui9->uFluxCapControl[54] = 0; ui9->uFluxCapControl[55] = 0; ui9->uFluxCapControl[56] = 0; ui9->uFluxCapControl[57] = 0; ui9->uFluxCapControl[58] = 0; ui9->uFluxCapControl[59] = 0; ui9->uFluxCapControl[60] = 0; ui9->uFluxCapControl[61] = 0; ui9->uFluxCapControl[62] = 0; ui9->uFluxCapControl[63] = 0; 
	ui9->fFluxCapData[0] = 0.000000; ui9->fFluxCapData[1] = 0.000000; ui9->fFluxCapData[2] = 0.000000; ui9->fFluxCapData[3] = 0.000000; ui9->fFluxCapData[4] = 0.000000; ui9->fFluxCapData[5] = 0.000000; ui9->fFluxCapData[6] = 0.000000; ui9->fFluxCapData[7] = 0.000000; ui9->fFluxCapData[8] = 0.000000; ui9->fFluxCapData[9] = 0.000000; ui9->fFluxCapData[10] = 0.000000; ui9->fFluxCapData[11] = 0.000000; ui9->fFluxCapData[12] = 0.000000; ui9->fFluxCapData[13] = 0.000000; ui9->fFluxCapData[14] = 0.000000; ui9->fFluxCapData[15] = 0.000000; ui9->fFluxCapData[16] = 0.000000; ui9->fFluxCapData[17] = 0.000000; ui9->fFluxCapData[18] = 0.000000; ui9->fFluxCapData[19] = 0.000000; ui9->fFluxCapData[20] = 0.000000; ui9->fFluxCapData[21] = 0.000000; ui9->fFluxCapData[22] = 0.000000; ui9->fFluxCapData[23] = 0.000000; ui9->fFluxCapData[24] = 0.000000; ui9->fFluxCapData[25] = 0.000000; ui9->fFluxCapData[26] = 0.000000; ui9->fFluxCapData[27] = 0.000000; ui9->fFluxCapData[28] = 0.000000; ui9->fFluxCapData[29] = 0.000000; ui9->fFluxCapData[30] = 0.000000; ui9->fFluxCapData[31] = 0.000000; ui9->fFluxCapData[32] = 0.000000; ui9->fFluxCapData[33] = 0.000000; ui9->fFluxCapData[34] = 0.000000; ui9->fFluxCapData[35] = 0.000000; ui9->fFluxCapData[36] = 0.000000; ui9->fFluxCapData[37] = 0.000000; ui9->fFluxCapData[38] = 0.000000; ui9->fFluxCapData[39] = 0.000000; ui9->fFluxCapData[40] = 0.000000; ui9->fFluxCapData[41] = 0.000000; ui9->fFluxCapData[42] = 0.000000; ui9->fFluxCapData[43] = 0.000000; ui9->fFluxCapData[44] = 0.000000; ui9->fFluxCapData[45] = 0.000000; ui9->fFluxCapData[46] = 0.000000; ui9->fFluxCapData[47] = 0.000000; ui9->fFluxCapData[48] = 0.000000; ui9->fFluxCapData[49] = 0.000000; ui9->fFluxCapData[50] = 0.000000; ui9->fFluxCapData[51] = 0.000000; ui9->fFluxCapData[52] = 0.000000; ui9->fFluxCapData[53] = 0.000000; ui9->fFluxCapData[54] = 0.000000; ui9->fFluxCapData[55] = 0.000000; ui9->fFluxCapData[56] = 0.000000; ui9->fFluxCapData[57] = 0.000000; ui9->fFluxCapData[58] = 0.000000; ui9->fFluxCapData[59] = 0.000000; ui9->fFluxCapData[60] = 0.000000; ui9->fFluxCapData[61] = 0.000000; ui9->fFluxCapData[62] = 0.000000; ui9->fFluxCapData[63] = 0.000000; 
	m_UIControlList.append(*ui9);
	delete ui9;


	m_fOsc2Level = 100.000000;
	CUICtrl* ui10 = new CUICtrl;
	ui10->uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
	ui10->uControlId = 10;
	ui10->bLogSlider = false;
	ui10->bExpSlider = false;
	ui10->fUserDisplayDataLoLimit = 0.000000;
	ui10->fUserDisplayDataHiLimit = 100.000000;
	ui10->uUserDataType = floatData;
	ui10->fInitUserIntValue = 0;
	ui10->fInitUserFloatValue = 100.000000;
	ui10->fInitUserDoubleValue = 0;
	ui10->fInitUserUINTValue = 0;
	ui10->m_pUserCookedIntData = NULL;
	ui10->m_pUserCookedFloatData = &m_fOsc2Level;
	ui10->m_pUserCookedDoubleData = NULL;
	ui10->m_pUserCookedUINTData = NULL;
	ui10->cControlUnits = "Units                                                           ";
	ui10->cVariableName = "m_fOsc2Level";
	ui10->cEnumeratedList = "SEL1,SEL2,SEL3";
	ui10->dPresetData[0] = 100.000000;ui10->dPresetData[1] = 0.000000;ui10->dPresetData[2] = 0.000000;ui10->dPresetData[3] = 0.000000;ui10->dPresetData[4] = 0.000000;ui10->dPresetData[5] = 0.000000;ui10->dPresetData[6] = 0.000000;ui10->dPresetData[7] = 0.000000;ui10->dPresetData[8] = 0.000000;ui10->dPresetData[9] = 0.000000;ui10->dPresetData[10] = 0.000000;ui10->dPresetData[11] = 0.000000;ui10->dPresetData[12] = 0.000000;ui10->dPresetData[13] = 0.000000;ui10->dPresetData[14] = 0.000000;ui10->dPresetData[15] = 0.000000;
	ui10->cControlName = "Osc 2 Level";
	ui10->bOwnerControl = false;
	ui10->bMIDIControl = false;
	ui10->uMIDIControlCommand = 176;
	ui10->uMIDIControlName = 3;
	ui10->uMIDIControlChannel = 0;
	ui10->nGUIRow = -1;
	ui10->nGUIColumn = -1;
	ui10->uControlTheme[0] = 0; ui10->uControlTheme[1] = 0; ui10->uControlTheme[2] = 0; ui10->uControlTheme[3] = 0; ui10->uControlTheme[4] = 0; ui10->uControlTheme[5] = 0; ui10->uControlTheme[6] = 0; ui10->uControlTheme[7] = 0; ui10->uControlTheme[8] = 0; ui10->uControlTheme[9] = 0; ui10->uControlTheme[10] = 0; ui10->uControlTheme[11] = 0; ui10->uControlTheme[12] = 0; ui10->uControlTheme[13] = 0; ui10->uControlTheme[14] = 0; ui10->uControlTheme[15] = 0; ui10->uControlTheme[16] = 0; ui10->uControlTheme[17] = 0; ui10->uControlTheme[18] = 0; ui10->uControlTheme[19] = 0; ui10->uControlTheme[20] = 0; ui10->uControlTheme[21] = 0; ui10->uControlTheme[22] = 0; ui10->uControlTheme[23] = 0; ui10->uControlTheme[24] = 0; ui10->uControlTheme[25] = 0; ui10->uControlTheme[26] = 0; ui10->uControlTheme[27] = 1; ui10->uControlTheme[28] = 0; ui10->uControlTheme[29] = 0; ui10->uControlTheme[30] = 0; ui10->uControlTheme[31] = 0; 
	ui10->uFluxCapControl[0] = 0; ui10->uFluxCapControl[1] = 0; ui10->uFluxCapControl[2] = 0; ui10->uFluxCapControl[3] = 0; ui10->uFluxCapControl[4] = 0; ui10->uFluxCapControl[5] = 0; ui10->uFluxCapControl[6] = 0; ui10->uFluxCapControl[7] = 0; ui10->uFluxCapControl[8] = 0; ui10->uFluxCapControl[9] = 0; ui10->uFluxCapControl[10] = 0; ui10->uFluxCapControl[11] = 0; ui10->uFluxCapControl[12] = 0; ui10->uFluxCapControl[13] = 0; ui10->uFluxCapControl[14] = 0; ui10->uFluxCapControl[15] = 0; ui10->uFluxCapControl[16] = 0; ui10->uFluxCapControl[17] = 0; ui10->uFluxCapControl[18] = 0; ui10->uFluxCapControl[19] = 0; ui10->uFluxCapControl[20] = 0; ui10->uFluxCapControl[21] = 0; ui10->uFluxCapControl[22] = 0; ui10->uFluxCapControl[23] = 0; ui10->uFluxCapControl[24] = 0; ui10->uFluxCapControl[25] = 0; ui10->uFluxCapControl[26] = 0; ui10->uFluxCapControl[27] = 0; ui10->uFluxCapControl[28] = 0; ui10->uFluxCapControl[29] = 0; ui10->uFluxCapControl[30] = 0; ui10->uFluxCapControl[31] = 0; ui10->uFluxCapControl[32] = 0; ui10->uFluxCapControl[33] = 0; ui10->uFluxCapControl[34] = 0; ui10->uFluxCapControl[35] = 0; ui10->uFluxCapControl[36] = 0; ui10->uFluxCapControl[37] = 0; ui10->uFluxCapControl[38] = 0; ui10->uFluxCapControl[39] = 0; ui10->uFluxCapControl[40] = 0; ui10->uFluxCapControl[41] = 0; ui10->uFluxCapControl[42] = 0; ui10->uFluxCapControl[43] = 0; ui10->uFluxCapControl[44] = 0; ui10->uFluxCapControl[45] = 0; ui10->uFluxCapControl[46] = 0; ui10->uFluxCapControl[47] = 0; ui10->uFluxCapControl[48] = 0; ui10->uFluxCapControl[49] = 0; ui10->uFluxCapControl[50] = 0; ui10->uFluxCapControl[51] = 0; ui10->uFluxCapControl[52] = 0; ui10->uFluxCapControl[53] = 0; ui10->uFluxCapControl[54] = 0; ui10->uFluxCapControl[55] = 0; ui10->uFluxCapControl[56] = 0; ui10->uFluxCapControl[57] = 0; ui10->uFluxCapControl[58] = 0; ui10->uFluxCapControl[59] = 0; ui10->uFluxCapControl[60] = 0; ui10->uFluxCapControl[61] = 0; ui10->uFluxCapControl[62] = 0; ui10->uFluxCapControl[63] = 0; 
	ui10->fFluxCapData[0] = 0.000000; ui10->fFluxCapData[1] = 0.000000; ui10->fFluxCapData[2] = 0.000000; ui10->fFluxCapData[3] = 0.000000; ui10->fFluxCapData[4] = 0.000000; ui10->fFluxCapData[5] = 0.000000; ui10->fFluxCapData[6] = 0.000000; ui10->fFluxCapData[7] = 0.000000; ui10->fFluxCapData[8] = 0.000000; ui10->fFluxCapData[9] = 0.000000; ui10->fFluxCapData[10] = 0.000000; ui10->fFluxCapData[11] = 0.000000; ui10->fFluxCapData[12] = 0.000000; ui10->fFluxCapData[13] = 0.000000; ui10->fFluxCapData[14] = 0.000000; ui10->fFluxCapData[15] = 0.000000; ui10->fFluxCapData[16] = 0.000000; ui10->fFluxCapData[17] = 0.000000; ui10->fFluxCapData[18] = 0.000000; ui10->fFluxCapData[19] = 0.000000; ui10->fFluxCapData[20] = 0.000000; ui10->fFluxCapData[21] = 0.000000; ui10->fFluxCapData[22] = 0.000000; ui10->fFluxCapData[23] = 0.000000; ui10->fFluxCapData[24] = 0.000000; ui10->fFluxCapData[25] = 0.000000; ui10->fFluxCapData[26] = 0.000000; ui10->fFluxCapData[27] = 0.000000; ui10->fFluxCapData[28] = 0.000000; ui10->fFluxCapData[29] = 0.000000; ui10->fFluxCapData[30] = 0.000000; ui10->fFluxCapData[31] = 0.000000; ui10->fFluxCapData[32] = 0.000000; ui10->fFluxCapData[33] = 0.000000; ui10->fFluxCapData[34] = 0.000000; ui10->fFluxCapData[35] = 0.000000; ui10->fFluxCapData[36] = 0.000000; ui10->fFluxCapData[37] = 0.000000; ui10->fFluxCapData[38] = 0.000000; ui10->fFluxCapData[39] = 0.000000; ui10->fFluxCapData[40] = 0.000000; ui10->fFluxCapData[41] = 0.000000; ui10->fFluxCapData[42] = 0.000000; ui10->fFluxCapData[43] = 0.000000; ui10->fFluxCapData[44] = 0.000000; ui10->fFluxCapData[45] = 0.000000; ui10->fFluxCapData[46] = 0.000000; ui10->fFluxCapData[47] = 0.000000; ui10->fFluxCapData[48] = 0.000000; ui10->fFluxCapData[49] = 0.000000; ui10->fFluxCapData[50] = 0.000000; ui10->fFluxCapData[51] = 0.000000; ui10->fFluxCapData[52] = 0.000000; ui10->fFluxCapData[53] = 0.000000; ui10->fFluxCapData[54] = 0.000000; ui10->fFluxCapData[55] = 0.000000; ui10->fFluxCapData[56] = 0.000000; ui10->fFluxCapData[57] = 0.000000; ui10->fFluxCapData[58] = 0.000000; ui10->fFluxCapData[59] = 0.000000; ui10->fFluxCapData[60] = 0.000000; ui10->fFluxCapData[61] = 0.000000; ui10->fFluxCapData[62] = 0.000000; ui10->fFluxCapData[63] = 0.000000; 
	m_UIControlList.append(*ui10);
	delete ui10;


	m_uMono = 0;
	CUICtrl* ui11 = new CUICtrl;
	ui11->uControlType = FILTER_CONTROL_RADIO_SWITCH_VARIABLE;
	ui11->uControlId = 45;
	ui11->bLogSlider = false;
	ui11->bExpSlider = false;
	ui11->fUserDisplayDataLoLimit = 0.000000;
	ui11->fUserDisplayDataHiLimit = 1.000000;
	ui11->uUserDataType = UINTData;
	ui11->fInitUserIntValue = 0;
	ui11->fInitUserFloatValue = 0;
	ui11->fInitUserDoubleValue = 0;
	ui11->fInitUserUINTValue = 0.000000;
	ui11->m_pUserCookedIntData = NULL;
	ui11->m_pUserCookedFloatData = NULL;
	ui11->m_pUserCookedDoubleData = NULL;
	ui11->m_pUserCookedUINTData = &m_uMono;
	ui11->cControlUnits = "";
	ui11->cVariableName = "m_uMono";
	ui11->cEnumeratedList = "SWITCH_OFF,SWITCH_ON";
	ui11->dPresetData[0] = 0.000000;ui11->dPresetData[1] = 0.000000;ui11->dPresetData[2] = 0.000000;ui11->dPresetData[3] = 0.000000;ui11->dPresetData[4] = 0.000000;ui11->dPresetData[5] = 0.000000;ui11->dPresetData[6] = 0.000000;ui11->dPresetData[7] = 0.000000;ui11->dPresetData[8] = 0.000000;ui11->dPresetData[9] = 0.000000;ui11->dPresetData[10] = 0.000000;ui11->dPresetData[11] = 0.000000;ui11->dPresetData[12] = 0.000000;ui11->dPresetData[13] = 0.000000;ui11->dPresetData[14] = 0.000000;ui11->dPresetData[15] = 0.000000;
	ui11->cControlName = "mono";
	ui11->bOwnerControl = false;
	ui11->bMIDIControl = false;
	ui11->uMIDIControlCommand = 176;
	ui11->uMIDIControlName = 3;
	ui11->uMIDIControlChannel = 0;
	ui11->nGUIRow = -1;
	ui11->nGUIColumn = -1;
	ui11->uControlTheme[0] = 0; ui11->uControlTheme[1] = 0; ui11->uControlTheme[2] = 0; ui11->uControlTheme[3] = 8355711; ui11->uControlTheme[4] = 139; ui11->uControlTheme[5] = 0; ui11->uControlTheme[6] = 0; ui11->uControlTheme[7] = 0; ui11->uControlTheme[8] = 0; ui11->uControlTheme[9] = 0; ui11->uControlTheme[10] = 0; ui11->uControlTheme[11] = 0; ui11->uControlTheme[12] = 0; ui11->uControlTheme[13] = 0; ui11->uControlTheme[14] = 0; ui11->uControlTheme[15] = 0; ui11->uControlTheme[16] = 0; ui11->uControlTheme[17] = 0; ui11->uControlTheme[18] = 0; ui11->uControlTheme[19] = 0; ui11->uControlTheme[20] = 0; ui11->uControlTheme[21] = 0; ui11->uControlTheme[22] = 0; ui11->uControlTheme[23] = 0; ui11->uControlTheme[24] = 0; ui11->uControlTheme[25] = 0; ui11->uControlTheme[26] = 0; ui11->uControlTheme[27] = 0; ui11->uControlTheme[28] = 0; ui11->uControlTheme[29] = 0; ui11->uControlTheme[30] = 0; ui11->uControlTheme[31] = 0; 
	ui11->uFluxCapControl[0] = 0; ui11->uFluxCapControl[1] = 0; ui11->uFluxCapControl[2] = 0; ui11->uFluxCapControl[3] = 0; ui11->uFluxCapControl[4] = 0; ui11->uFluxCapControl[5] = 0; ui11->uFluxCapControl[6] = 0; ui11->uFluxCapControl[7] = 0; ui11->uFluxCapControl[8] = 0; ui11->uFluxCapControl[9] = 0; ui11->uFluxCapControl[10] = 0; ui11->uFluxCapControl[11] = 0; ui11->uFluxCapControl[12] = 0; ui11->uFluxCapControl[13] = 0; ui11->uFluxCapControl[14] = 0; ui11->uFluxCapControl[15] = 0; ui11->uFluxCapControl[16] = 0; ui11->uFluxCapControl[17] = 0; ui11->uFluxCapControl[18] = 0; ui11->uFluxCapControl[19] = 0; ui11->uFluxCapControl[20] = 0; ui11->uFluxCapControl[21] = 0; ui11->uFluxCapControl[22] = 0; ui11->uFluxCapControl[23] = 0; ui11->uFluxCapControl[24] = 0; ui11->uFluxCapControl[25] = 0; ui11->uFluxCapControl[26] = 0; ui11->uFluxCapControl[27] = 0; ui11->uFluxCapControl[28] = 0; ui11->uFluxCapControl[29] = 0; ui11->uFluxCapControl[30] = 0; ui11->uFluxCapControl[31] = 0; ui11->uFluxCapControl[32] = 0; ui11->uFluxCapControl[33] = 0; ui11->uFluxCapControl[34] = 0; ui11->uFluxCapControl[35] = 0; ui11->uFluxCapControl[36] = 0; ui11->uFluxCapControl[37] = 0; ui11->uFluxCapControl[38] = 0; ui11->uFluxCapControl[39] = 0; ui11->uFluxCapControl[40] = 0; ui11->uFluxCapControl[41] = 0; ui11->uFluxCapControl[42] = 0; ui11->uFluxCapControl[43] = 0; ui11->uFluxCapControl[44] = 0; ui11->uFluxCapControl[45] = 0; ui11->uFluxCapControl[46] = 0; ui11->uFluxCapControl[47] = 0; ui11->uFluxCapControl[48] = 0; ui11->uFluxCapControl[49] = 0; ui11->uFluxCapControl[50] = 0; ui11->uFluxCapControl[51] = 0; ui11->uFluxCapControl[52] = 0; ui11->uFluxCapControl[53] = 0; ui11->uFluxCapControl[54] = 0; ui11->uFluxCapControl[55] = 0; ui11->uFluxCapControl[56] = 0; ui11->uFluxCapControl[57] = 0; ui11->uFluxCapControl[58] = 0; ui11->uFluxCapControl[59] = 0; ui11->uFluxCapControl[60] = 0; ui11->uFluxCapControl[61] = 0; ui11->uFluxCapControl[62] = 0; ui11->uFluxCapControl[63] = 0; 
	ui11->fFluxCapData[0] = 0.000000; ui11->fFluxCapData[1] = 0.000000; ui11->fFluxCapData[2] = 0.000000; ui11->fFluxCapData[3] = 0.000000; ui11->fFluxCapData[4] = 0.000000; ui11->fFluxCapData[5] = 0.000000; ui11->fFluxCapData[6] = 0.000000; ui11->fFluxCapData[7] = 0.000000; ui11->fFluxCapData[8] = 0.000000; ui11->fFluxCapData[9] = 0.000000; ui11->fFluxCapData[10] = 0.000000; ui11->fFluxCapData[11] = 0.000000; ui11->fFluxCapData[12] = 0.000000; ui11->fFluxCapData[13] = 0.000000; ui11->fFluxCapData[14] = 0.000000; ui11->fFluxCapData[15] = 0.000000; ui11->fFluxCapData[16] = 0.000000; ui11->fFluxCapData[17] = 0.000000; ui11->fFluxCapData[18] = 0.000000; ui11->fFluxCapData[19] = 0.000000; ui11->fFluxCapData[20] = 0.000000; ui11->fFluxCapData[21] = 0.000000; ui11->fFluxCapData[22] = 0.000000; ui11->fFluxCapData[23] = 0.000000; ui11->fFluxCapData[24] = 0.000000; ui11->fFluxCapData[25] = 0.000000; ui11->fFluxCapData[26] = 0.000000; ui11->fFluxCapData[27] = 0.000000; ui11->fFluxCapData[28] = 0.000000; ui11->fFluxCapData[29] = 0.000000; ui11->fFluxCapData[30] = 0.000000; ui11->fFluxCapData[31] = 0.000000; ui11->fFluxCapData[32] = 0.000000; ui11->fFluxCapData[33] = 0.000000; ui11->fFluxCapData[34] = 0.000000; ui11->fFluxCapData[35] = 0.000000; ui11->fFluxCapData[36] = 0.000000; ui11->fFluxCapData[37] = 0.000000; ui11->fFluxCapData[38] = 0.000000; ui11->fFluxCapData[39] = 0.000000; ui11->fFluxCapData[40] = 0.000000; ui11->fFluxCapData[41] = 0.000000; ui11->fFluxCapData[42] = 0.000000; ui11->fFluxCapData[43] = 0.000000; ui11->fFluxCapData[44] = 0.000000; ui11->fFluxCapData[45] = 0.000000; ui11->fFluxCapData[46] = 0.000000; ui11->fFluxCapData[47] = 0.000000; ui11->fFluxCapData[48] = 0.000000; ui11->fFluxCapData[49] = 0.000000; ui11->fFluxCapData[50] = 0.000000; ui11->fFluxCapData[51] = 0.000000; ui11->fFluxCapData[52] = 0.000000; ui11->fFluxCapData[53] = 0.000000; ui11->fFluxCapData[54] = 0.000000; ui11->fFluxCapData[55] = 0.000000; ui11->fFluxCapData[56] = 0.000000; ui11->fFluxCapData[57] = 0.000000; ui11->fFluxCapData[58] = 0.000000; ui11->fFluxCapData[59] = 0.000000; ui11->fFluxCapData[60] = 0.000000; ui11->fFluxCapData[61] = 0.000000; ui11->fFluxCapData[62] = 0.000000; ui11->fFluxCapData[63] = 0.000000; 
	m_UIControlList.append(*ui11);
	delete ui11;


	m_uStart = 0;
	CUICtrl* ui12 = new CUICtrl;
	ui12->uControlType = FILTER_CONTROL_RADIO_SWITCH_VARIABLE;
	ui12->uControlId = 46;
	ui12->bLogSlider = false;
	ui12->bExpSlider = false;
	ui12->fUserDisplayDataLoLimit = 0.000000;
	ui12->fUserDisplayDataHiLimit = 1.000000;
	ui12->uUserDataType = UINTData;
	ui12->fInitUserIntValue = 0;
	ui12->fInitUserFloatValue = 0;
	ui12->fInitUserDoubleValue = 0;
	ui12->fInitUserUINTValue = 0.000000;
	ui12->m_pUserCookedIntData = NULL;
	ui12->m_pUserCookedFloatData = NULL;
	ui12->m_pUserCookedDoubleData = NULL;
	ui12->m_pUserCookedUINTData = &m_uStart;
	ui12->cControlUnits = "";
	ui12->cVariableName = "m_uStart";
	ui12->cEnumeratedList = "SWITCH_OFF,SWITCH_ON";
	ui12->dPresetData[0] = 0.000000;ui12->dPresetData[1] = 0.000000;ui12->dPresetData[2] = 0.000000;ui12->dPresetData[3] = 0.000000;ui12->dPresetData[4] = 0.000000;ui12->dPresetData[5] = 0.000000;ui12->dPresetData[6] = 0.000000;ui12->dPresetData[7] = 0.000000;ui12->dPresetData[8] = 0.000000;ui12->dPresetData[9] = 0.000000;ui12->dPresetData[10] = 0.000000;ui12->dPresetData[11] = 0.000000;ui12->dPresetData[12] = 0.000000;ui12->dPresetData[13] = 0.000000;ui12->dPresetData[14] = 0.000000;ui12->dPresetData[15] = 0.000000;
	ui12->cControlName = "Start";
	ui12->bOwnerControl = false;
	ui12->bMIDIControl = false;
	ui12->uMIDIControlCommand = 176;
	ui12->uMIDIControlName = 3;
	ui12->uMIDIControlChannel = 0;
	ui12->nGUIRow = -1;
	ui12->nGUIColumn = -1;
	ui12->uControlTheme[0] = 0; ui12->uControlTheme[1] = 0; ui12->uControlTheme[2] = 0; ui12->uControlTheme[3] = 8355711; ui12->uControlTheme[4] = 139; ui12->uControlTheme[5] = 0; ui12->uControlTheme[6] = 0; ui12->uControlTheme[7] = 0; ui12->uControlTheme[8] = 0; ui12->uControlTheme[9] = 0; ui12->uControlTheme[10] = 0; ui12->uControlTheme[11] = 0; ui12->uControlTheme[12] = 0; ui12->uControlTheme[13] = 0; ui12->uControlTheme[14] = 0; ui12->uControlTheme[15] = 0; ui12->uControlTheme[16] = 0; ui12->uControlTheme[17] = 0; ui12->uControlTheme[18] = 0; ui12->uControlTheme[19] = 0; ui12->uControlTheme[20] = 0; ui12->uControlTheme[21] = 0; ui12->uControlTheme[22] = 0; ui12->uControlTheme[23] = 0; ui12->uControlTheme[24] = 0; ui12->uControlTheme[25] = 0; ui12->uControlTheme[26] = 0; ui12->uControlTheme[27] = 0; ui12->uControlTheme[28] = 0; ui12->uControlTheme[29] = 0; ui12->uControlTheme[30] = 0; ui12->uControlTheme[31] = 0; 
	ui12->uFluxCapControl[0] = 0; ui12->uFluxCapControl[1] = 0; ui12->uFluxCapControl[2] = 0; ui12->uFluxCapControl[3] = 0; ui12->uFluxCapControl[4] = 0; ui12->uFluxCapControl[5] = 0; ui12->uFluxCapControl[6] = 0; ui12->uFluxCapControl[7] = 0; ui12->uFluxCapControl[8] = 0; ui12->uFluxCapControl[9] = 0; ui12->uFluxCapControl[10] = 0; ui12->uFluxCapControl[11] = 0; ui12->uFluxCapControl[12] = 0; ui12->uFluxCapControl[13] = 0; ui12->uFluxCapControl[14] = 0; ui12->uFluxCapControl[15] = 0; ui12->uFluxCapControl[16] = 0; ui12->uFluxCapControl[17] = 0; ui12->uFluxCapControl[18] = 0; ui12->uFluxCapControl[19] = 0; ui12->uFluxCapControl[20] = 0; ui12->uFluxCapControl[21] = 0; ui12->uFluxCapControl[22] = 0; ui12->uFluxCapControl[23] = 0; ui12->uFluxCapControl[24] = 0; ui12->uFluxCapControl[25] = 0; ui12->uFluxCapControl[26] = 0; ui12->uFluxCapControl[27] = 0; ui12->uFluxCapControl[28] = 0; ui12->uFluxCapControl[29] = 0; ui12->uFluxCapControl[30] = 0; ui12->uFluxCapControl[31] = 0; ui12->uFluxCapControl[32] = 0; ui12->uFluxCapControl[33] = 0; ui12->uFluxCapControl[34] = 0; ui12->uFluxCapControl[35] = 0; ui12->uFluxCapControl[36] = 0; ui12->uFluxCapControl[37] = 0; ui12->uFluxCapControl[38] = 0; ui12->uFluxCapControl[39] = 0; ui12->uFluxCapControl[40] = 0; ui12->uFluxCapControl[41] = 0; ui12->uFluxCapControl[42] = 0; ui12->uFluxCapControl[43] = 0; ui12->uFluxCapControl[44] = 0; ui12->uFluxCapControl[45] = 0; ui12->uFluxCapControl[46] = 0; ui12->uFluxCapControl[47] = 0; ui12->uFluxCapControl[48] = 0; ui12->uFluxCapControl[49] = 0; ui12->uFluxCapControl[50] = 0; ui12->uFluxCapControl[51] = 0; ui12->uFluxCapControl[52] = 0; ui12->uFluxCapControl[53] = 0; ui12->uFluxCapControl[54] = 0; ui12->uFluxCapControl[55] = 0; ui12->uFluxCapControl[56] = 0; ui12->uFluxCapControl[57] = 0; ui12->uFluxCapControl[58] = 0; ui12->uFluxCapControl[59] = 0; ui12->uFluxCapControl[60] = 0; ui12->uFluxCapControl[61] = 0; ui12->uFluxCapControl[62] = 0; ui12->uFluxCapControl[63] = 0; 
	ui12->fFluxCapData[0] = 0.000000; ui12->fFluxCapData[1] = 0.000000; ui12->fFluxCapData[2] = 0.000000; ui12->fFluxCapData[3] = 0.000000; ui12->fFluxCapData[4] = 0.000000; ui12->fFluxCapData[5] = 0.000000; ui12->fFluxCapData[6] = 0.000000; ui12->fFluxCapData[7] = 0.000000; ui12->fFluxCapData[8] = 0.000000; ui12->fFluxCapData[9] = 0.000000; ui12->fFluxCapData[10] = 0.000000; ui12->fFluxCapData[11] = 0.000000; ui12->fFluxCapData[12] = 0.000000; ui12->fFluxCapData[13] = 0.000000; ui12->fFluxCapData[14] = 0.000000; ui12->fFluxCapData[15] = 0.000000; ui12->fFluxCapData[16] = 0.000000; ui12->fFluxCapData[17] = 0.000000; ui12->fFluxCapData[18] = 0.000000; ui12->fFluxCapData[19] = 0.000000; ui12->fFluxCapData[20] = 0.000000; ui12->fFluxCapData[21] = 0.000000; ui12->fFluxCapData[22] = 0.000000; ui12->fFluxCapData[23] = 0.000000; ui12->fFluxCapData[24] = 0.000000; ui12->fFluxCapData[25] = 0.000000; ui12->fFluxCapData[26] = 0.000000; ui12->fFluxCapData[27] = 0.000000; ui12->fFluxCapData[28] = 0.000000; ui12->fFluxCapData[29] = 0.000000; ui12->fFluxCapData[30] = 0.000000; ui12->fFluxCapData[31] = 0.000000; ui12->fFluxCapData[32] = 0.000000; ui12->fFluxCapData[33] = 0.000000; ui12->fFluxCapData[34] = 0.000000; ui12->fFluxCapData[35] = 0.000000; ui12->fFluxCapData[36] = 0.000000; ui12->fFluxCapData[37] = 0.000000; ui12->fFluxCapData[38] = 0.000000; ui12->fFluxCapData[39] = 0.000000; ui12->fFluxCapData[40] = 0.000000; ui12->fFluxCapData[41] = 0.000000; ui12->fFluxCapData[42] = 0.000000; ui12->fFluxCapData[43] = 0.000000; ui12->fFluxCapData[44] = 0.000000; ui12->fFluxCapData[45] = 0.000000; ui12->fFluxCapData[46] = 0.000000; ui12->fFluxCapData[47] = 0.000000; ui12->fFluxCapData[48] = 0.000000; ui12->fFluxCapData[49] = 0.000000; ui12->fFluxCapData[50] = 0.000000; ui12->fFluxCapData[51] = 0.000000; ui12->fFluxCapData[52] = 0.000000; ui12->fFluxCapData[53] = 0.000000; ui12->fFluxCapData[54] = 0.000000; ui12->fFluxCapData[55] = 0.000000; ui12->fFluxCapData[56] = 0.000000; ui12->fFluxCapData[57] = 0.000000; ui12->fFluxCapData[58] = 0.000000; ui12->fFluxCapData[59] = 0.000000; ui12->fFluxCapData[60] = 0.000000; ui12->fFluxCapData[61] = 0.000000; ui12->fFluxCapData[62] = 0.000000; ui12->fFluxCapData[63] = 0.000000; 
	m_UIControlList.append(*ui12);
	delete ui12;


	m_uAutoPan = 0;
	CUICtrl* ui13 = new CUICtrl;
	ui13->uControlType = FILTER_CONTROL_RADIO_SWITCH_VARIABLE;
	ui13->uControlId = 47;
	ui13->bLogSlider = false;
	ui13->bExpSlider = false;
	ui13->fUserDisplayDataLoLimit = 0.000000;
	ui13->fUserDisplayDataHiLimit = 1.000000;
	ui13->uUserDataType = UINTData;
	ui13->fInitUserIntValue = 0;
	ui13->fInitUserFloatValue = 0;
	ui13->fInitUserDoubleValue = 0;
	ui13->fInitUserUINTValue = 0.000000;
	ui13->m_pUserCookedIntData = NULL;
	ui13->m_pUserCookedFloatData = NULL;
	ui13->m_pUserCookedDoubleData = NULL;
	ui13->m_pUserCookedUINTData = &m_uAutoPan;
	ui13->cControlUnits = "";
	ui13->cVariableName = "m_uAutoPan";
	ui13->cEnumeratedList = "SWITCH_OFF,SWITCH_ON";
	ui13->dPresetData[0] = 0.000000;ui13->dPresetData[1] = 0.000000;ui13->dPresetData[2] = 0.000000;ui13->dPresetData[3] = 0.000000;ui13->dPresetData[4] = 0.000000;ui13->dPresetData[5] = 0.000000;ui13->dPresetData[6] = 0.000000;ui13->dPresetData[7] = 0.000000;ui13->dPresetData[8] = 0.000000;ui13->dPresetData[9] = 0.000000;ui13->dPresetData[10] = 0.000000;ui13->dPresetData[11] = 0.000000;ui13->dPresetData[12] = 0.000000;ui13->dPresetData[13] = 0.000000;ui13->dPresetData[14] = 0.000000;ui13->dPresetData[15] = 0.000000;
	ui13->cControlName = "autopan";
	ui13->bOwnerControl = false;
	ui13->bMIDIControl = false;
	ui13->uMIDIControlCommand = 176;
	ui13->uMIDIControlName = 3;
	ui13->uMIDIControlChannel = 0;
	ui13->nGUIRow = -1;
	ui13->nGUIColumn = -1;
	ui13->uControlTheme[0] = 0; ui13->uControlTheme[1] = 0; ui13->uControlTheme[2] = 0; ui13->uControlTheme[3] = 8355711; ui13->uControlTheme[4] = 139; ui13->uControlTheme[5] = 0; ui13->uControlTheme[6] = 0; ui13->uControlTheme[7] = 0; ui13->uControlTheme[8] = 0; ui13->uControlTheme[9] = 0; ui13->uControlTheme[10] = 0; ui13->uControlTheme[11] = 0; ui13->uControlTheme[12] = 0; ui13->uControlTheme[13] = 0; ui13->uControlTheme[14] = 0; ui13->uControlTheme[15] = 0; ui13->uControlTheme[16] = 0; ui13->uControlTheme[17] = 0; ui13->uControlTheme[18] = 0; ui13->uControlTheme[19] = 0; ui13->uControlTheme[20] = 0; ui13->uControlTheme[21] = 0; ui13->uControlTheme[22] = 0; ui13->uControlTheme[23] = 0; ui13->uControlTheme[24] = 0; ui13->uControlTheme[25] = 0; ui13->uControlTheme[26] = 0; ui13->uControlTheme[27] = 0; ui13->uControlTheme[28] = 0; ui13->uControlTheme[29] = 0; ui13->uControlTheme[30] = 0; ui13->uControlTheme[31] = 0; 
	ui13->uFluxCapControl[0] = 0; ui13->uFluxCapControl[1] = 0; ui13->uFluxCapControl[2] = 0; ui13->uFluxCapControl[3] = 0; ui13->uFluxCapControl[4] = 0; ui13->uFluxCapControl[5] = 0; ui13->uFluxCapControl[6] = 0; ui13->uFluxCapControl[7] = 0; ui13->uFluxCapControl[8] = 0; ui13->uFluxCapControl[9] = 0; ui13->uFluxCapControl[10] = 0; ui13->uFluxCapControl[11] = 0; ui13->uFluxCapControl[12] = 0; ui13->uFluxCapControl[13] = 0; ui13->uFluxCapControl[14] = 0; ui13->uFluxCapControl[15] = 0; ui13->uFluxCapControl[16] = 0; ui13->uFluxCapControl[17] = 0; ui13->uFluxCapControl[18] = 0; ui13->uFluxCapControl[19] = 0; ui13->uFluxCapControl[20] = 0; ui13->uFluxCapControl[21] = 0; ui13->uFluxCapControl[22] = 0; ui13->uFluxCapControl[23] = 0; ui13->uFluxCapControl[24] = 0; ui13->uFluxCapControl[25] = 0; ui13->uFluxCapControl[26] = 0; ui13->uFluxCapControl[27] = 0; ui13->uFluxCapControl[28] = 0; ui13->uFluxCapControl[29] = 0; ui13->uFluxCapControl[30] = 0; ui13->uFluxCapControl[31] = 0; ui13->uFluxCapControl[32] = 0; ui13->uFluxCapControl[33] = 0; ui13->uFluxCapControl[34] = 0; ui13->uFluxCapControl[35] = 0; ui13->uFluxCapControl[36] = 0; ui13->uFluxCapControl[37] = 0; ui13->uFluxCapControl[38] = 0; ui13->uFluxCapControl[39] = 0; ui13->uFluxCapControl[40] = 0; ui13->uFluxCapControl[41] = 0; ui13->uFluxCapControl[42] = 0; ui13->uFluxCapControl[43] = 0; ui13->uFluxCapControl[44] = 0; ui13->uFluxCapControl[45] = 0; ui13->uFluxCapControl[46] = 0; ui13->uFluxCapControl[47] = 0; ui13->uFluxCapControl[48] = 0; ui13->uFluxCapControl[49] = 0; ui13->uFluxCapControl[50] = 0; ui13->uFluxCapControl[51] = 0; ui13->uFluxCapControl[52] = 0; ui13->uFluxCapControl[53] = 0; ui13->uFluxCapControl[54] = 0; ui13->uFluxCapControl[55] = 0; ui13->uFluxCapControl[56] = 0; ui13->uFluxCapControl[57] = 0; ui13->uFluxCapControl[58] = 0; ui13->uFluxCapControl[59] = 0; ui13->uFluxCapControl[60] = 0; ui13->uFluxCapControl[61] = 0; ui13->uFluxCapControl[62] = 0; ui13->uFluxCapControl[63] = 0; 
	ui13->fFluxCapData[0] = 0.000000; ui13->fFluxCapData[1] = 0.000000; ui13->fFluxCapData[2] = 0.000000; ui13->fFluxCapData[3] = 0.000000; ui13->fFluxCapData[4] = 0.000000; ui13->fFluxCapData[5] = 0.000000; ui13->fFluxCapData[6] = 0.000000; ui13->fFluxCapData[7] = 0.000000; ui13->fFluxCapData[8] = 0.000000; ui13->fFluxCapData[9] = 0.000000; ui13->fFluxCapData[10] = 0.000000; ui13->fFluxCapData[11] = 0.000000; ui13->fFluxCapData[12] = 0.000000; ui13->fFluxCapData[13] = 0.000000; ui13->fFluxCapData[14] = 0.000000; ui13->fFluxCapData[15] = 0.000000; ui13->fFluxCapData[16] = 0.000000; ui13->fFluxCapData[17] = 0.000000; ui13->fFluxCapData[18] = 0.000000; ui13->fFluxCapData[19] = 0.000000; ui13->fFluxCapData[20] = 0.000000; ui13->fFluxCapData[21] = 0.000000; ui13->fFluxCapData[22] = 0.000000; ui13->fFluxCapData[23] = 0.000000; ui13->fFluxCapData[24] = 0.000000; ui13->fFluxCapData[25] = 0.000000; ui13->fFluxCapData[26] = 0.000000; ui13->fFluxCapData[27] = 0.000000; ui13->fFluxCapData[28] = 0.000000; ui13->fFluxCapData[29] = 0.000000; ui13->fFluxCapData[30] = 0.000000; ui13->fFluxCapData[31] = 0.000000; ui13->fFluxCapData[32] = 0.000000; ui13->fFluxCapData[33] = 0.000000; ui13->fFluxCapData[34] = 0.000000; ui13->fFluxCapData[35] = 0.000000; ui13->fFluxCapData[36] = 0.000000; ui13->fFluxCapData[37] = 0.000000; ui13->fFluxCapData[38] = 0.000000; ui13->fFluxCapData[39] = 0.000000; ui13->fFluxCapData[40] = 0.000000; ui13->fFluxCapData[41] = 0.000000; ui13->fFluxCapData[42] = 0.000000; ui13->fFluxCapData[43] = 0.000000; ui13->fFluxCapData[44] = 0.000000; ui13->fFluxCapData[45] = 0.000000; ui13->fFluxCapData[46] = 0.000000; ui13->fFluxCapData[47] = 0.000000; ui13->fFluxCapData[48] = 0.000000; ui13->fFluxCapData[49] = 0.000000; ui13->fFluxCapData[50] = 0.000000; ui13->fFluxCapData[51] = 0.000000; ui13->fFluxCapData[52] = 0.000000; ui13->fFluxCapData[53] = 0.000000; ui13->fFluxCapData[54] = 0.000000; ui13->fFluxCapData[55] = 0.000000; ui13->fFluxCapData[56] = 0.000000; ui13->fFluxCapData[57] = 0.000000; ui13->fFluxCapData[58] = 0.000000; ui13->fFluxCapData[59] = 0.000000; ui13->fFluxCapData[60] = 0.000000; ui13->fFluxCapData[61] = 0.000000; ui13->fFluxCapData[62] = 0.000000; ui13->fFluxCapData[63] = 0.000000; 
	m_UIControlList.append(*ui13);
	delete ui13;


	m_uBinaural = 0;
	CUICtrl* ui14 = new CUICtrl;
	ui14->uControlType = FILTER_CONTROL_RADIO_SWITCH_VARIABLE;
	ui14->uControlId = 48;
	ui14->bLogSlider = false;
	ui14->bExpSlider = false;
	ui14->fUserDisplayDataLoLimit = 0.000000;
	ui14->fUserDisplayDataHiLimit = 1.000000;
	ui14->uUserDataType = UINTData;
	ui14->fInitUserIntValue = 0;
	ui14->fInitUserFloatValue = 0;
	ui14->fInitUserDoubleValue = 0;
	ui14->fInitUserUINTValue = 0.000000;
	ui14->m_pUserCookedIntData = NULL;
	ui14->m_pUserCookedFloatData = NULL;
	ui14->m_pUserCookedDoubleData = NULL;
	ui14->m_pUserCookedUINTData = &m_uBinaural;
	ui14->cControlUnits = "";
	ui14->cVariableName = "m_uBinaural";
	ui14->cEnumeratedList = "SWITCH_OFF,SWITCH_ON";
	ui14->dPresetData[0] = 0.000000;ui14->dPresetData[1] = 0.000000;ui14->dPresetData[2] = 0.000000;ui14->dPresetData[3] = 0.000000;ui14->dPresetData[4] = 0.000000;ui14->dPresetData[5] = 0.000000;ui14->dPresetData[6] = 0.000000;ui14->dPresetData[7] = 0.000000;ui14->dPresetData[8] = 0.000000;ui14->dPresetData[9] = 0.000000;ui14->dPresetData[10] = 0.000000;ui14->dPresetData[11] = 0.000000;ui14->dPresetData[12] = 0.000000;ui14->dPresetData[13] = 0.000000;ui14->dPresetData[14] = 0.000000;ui14->dPresetData[15] = 0.000000;
	ui14->cControlName = "Binaural";
	ui14->bOwnerControl = false;
	ui14->bMIDIControl = false;
	ui14->uMIDIControlCommand = 176;
	ui14->uMIDIControlName = 3;
	ui14->uMIDIControlChannel = 0;
	ui14->nGUIRow = -1;
	ui14->nGUIColumn = -1;
	ui14->uControlTheme[0] = 0; ui14->uControlTheme[1] = 0; ui14->uControlTheme[2] = 0; ui14->uControlTheme[3] = 8355711; ui14->uControlTheme[4] = 139; ui14->uControlTheme[5] = 0; ui14->uControlTheme[6] = 0; ui14->uControlTheme[7] = 0; ui14->uControlTheme[8] = 0; ui14->uControlTheme[9] = 0; ui14->uControlTheme[10] = 0; ui14->uControlTheme[11] = 0; ui14->uControlTheme[12] = 0; ui14->uControlTheme[13] = 0; ui14->uControlTheme[14] = 0; ui14->uControlTheme[15] = 0; ui14->uControlTheme[16] = 0; ui14->uControlTheme[17] = 0; ui14->uControlTheme[18] = 0; ui14->uControlTheme[19] = 0; ui14->uControlTheme[20] = 0; ui14->uControlTheme[21] = 0; ui14->uControlTheme[22] = 0; ui14->uControlTheme[23] = 0; ui14->uControlTheme[24] = 0; ui14->uControlTheme[25] = 0; ui14->uControlTheme[26] = 0; ui14->uControlTheme[27] = 0; ui14->uControlTheme[28] = 0; ui14->uControlTheme[29] = 0; ui14->uControlTheme[30] = 0; ui14->uControlTheme[31] = 0; 
	ui14->uFluxCapControl[0] = 0; ui14->uFluxCapControl[1] = 0; ui14->uFluxCapControl[2] = 0; ui14->uFluxCapControl[3] = 0; ui14->uFluxCapControl[4] = 0; ui14->uFluxCapControl[5] = 0; ui14->uFluxCapControl[6] = 0; ui14->uFluxCapControl[7] = 0; ui14->uFluxCapControl[8] = 0; ui14->uFluxCapControl[9] = 0; ui14->uFluxCapControl[10] = 0; ui14->uFluxCapControl[11] = 0; ui14->uFluxCapControl[12] = 0; ui14->uFluxCapControl[13] = 0; ui14->uFluxCapControl[14] = 0; ui14->uFluxCapControl[15] = 0; ui14->uFluxCapControl[16] = 0; ui14->uFluxCapControl[17] = 0; ui14->uFluxCapControl[18] = 0; ui14->uFluxCapControl[19] = 0; ui14->uFluxCapControl[20] = 0; ui14->uFluxCapControl[21] = 0; ui14->uFluxCapControl[22] = 0; ui14->uFluxCapControl[23] = 0; ui14->uFluxCapControl[24] = 0; ui14->uFluxCapControl[25] = 0; ui14->uFluxCapControl[26] = 0; ui14->uFluxCapControl[27] = 0; ui14->uFluxCapControl[28] = 0; ui14->uFluxCapControl[29] = 0; ui14->uFluxCapControl[30] = 0; ui14->uFluxCapControl[31] = 0; ui14->uFluxCapControl[32] = 0; ui14->uFluxCapControl[33] = 0; ui14->uFluxCapControl[34] = 0; ui14->uFluxCapControl[35] = 0; ui14->uFluxCapControl[36] = 0; ui14->uFluxCapControl[37] = 0; ui14->uFluxCapControl[38] = 0; ui14->uFluxCapControl[39] = 0; ui14->uFluxCapControl[40] = 0; ui14->uFluxCapControl[41] = 0; ui14->uFluxCapControl[42] = 0; ui14->uFluxCapControl[43] = 0; ui14->uFluxCapControl[44] = 0; ui14->uFluxCapControl[45] = 0; ui14->uFluxCapControl[46] = 0; ui14->uFluxCapControl[47] = 0; ui14->uFluxCapControl[48] = 0; ui14->uFluxCapControl[49] = 0; ui14->uFluxCapControl[50] = 0; ui14->uFluxCapControl[51] = 0; ui14->uFluxCapControl[52] = 0; ui14->uFluxCapControl[53] = 0; ui14->uFluxCapControl[54] = 0; ui14->uFluxCapControl[55] = 0; ui14->uFluxCapControl[56] = 0; ui14->uFluxCapControl[57] = 0; ui14->uFluxCapControl[58] = 0; ui14->uFluxCapControl[59] = 0; ui14->uFluxCapControl[60] = 0; ui14->uFluxCapControl[61] = 0; ui14->uFluxCapControl[62] = 0; ui14->uFluxCapControl[63] = 0; 
	ui14->fFluxCapData[0] = 0.000000; ui14->fFluxCapData[1] = 0.000000; ui14->fFluxCapData[2] = 0.000000; ui14->fFluxCapData[3] = 0.000000; ui14->fFluxCapData[4] = 0.000000; ui14->fFluxCapData[5] = 0.000000; ui14->fFluxCapData[6] = 0.000000; ui14->fFluxCapData[7] = 0.000000; ui14->fFluxCapData[8] = 0.000000; ui14->fFluxCapData[9] = 0.000000; ui14->fFluxCapData[10] = 0.000000; ui14->fFluxCapData[11] = 0.000000; ui14->fFluxCapData[12] = 0.000000; ui14->fFluxCapData[13] = 0.000000; ui14->fFluxCapData[14] = 0.000000; ui14->fFluxCapData[15] = 0.000000; ui14->fFluxCapData[16] = 0.000000; ui14->fFluxCapData[17] = 0.000000; ui14->fFluxCapData[18] = 0.000000; ui14->fFluxCapData[19] = 0.000000; ui14->fFluxCapData[20] = 0.000000; ui14->fFluxCapData[21] = 0.000000; ui14->fFluxCapData[22] = 0.000000; ui14->fFluxCapData[23] = 0.000000; ui14->fFluxCapData[24] = 0.000000; ui14->fFluxCapData[25] = 0.000000; ui14->fFluxCapData[26] = 0.000000; ui14->fFluxCapData[27] = 0.000000; ui14->fFluxCapData[28] = 0.000000; ui14->fFluxCapData[29] = 0.000000; ui14->fFluxCapData[30] = 0.000000; ui14->fFluxCapData[31] = 0.000000; ui14->fFluxCapData[32] = 0.000000; ui14->fFluxCapData[33] = 0.000000; ui14->fFluxCapData[34] = 0.000000; ui14->fFluxCapData[35] = 0.000000; ui14->fFluxCapData[36] = 0.000000; ui14->fFluxCapData[37] = 0.000000; ui14->fFluxCapData[38] = 0.000000; ui14->fFluxCapData[39] = 0.000000; ui14->fFluxCapData[40] = 0.000000; ui14->fFluxCapData[41] = 0.000000; ui14->fFluxCapData[42] = 0.000000; ui14->fFluxCapData[43] = 0.000000; ui14->fFluxCapData[44] = 0.000000; ui14->fFluxCapData[45] = 0.000000; ui14->fFluxCapData[46] = 0.000000; ui14->fFluxCapData[47] = 0.000000; ui14->fFluxCapData[48] = 0.000000; ui14->fFluxCapData[49] = 0.000000; ui14->fFluxCapData[50] = 0.000000; ui14->fFluxCapData[51] = 0.000000; ui14->fFluxCapData[52] = 0.000000; ui14->fFluxCapData[53] = 0.000000; ui14->fFluxCapData[54] = 0.000000; ui14->fFluxCapData[55] = 0.000000; ui14->fFluxCapData[56] = 0.000000; ui14->fFluxCapData[57] = 0.000000; ui14->fFluxCapData[58] = 0.000000; ui14->fFluxCapData[59] = 0.000000; ui14->fFluxCapData[60] = 0.000000; ui14->fFluxCapData[61] = 0.000000; ui14->fFluxCapData[62] = 0.000000; ui14->fFluxCapData[63] = 0.000000; 
	m_UIControlList.append(*ui14);
	delete ui14;


	m_uX_TrackPadIndex = -1;
	m_uY_TrackPadIndex = -1;

	m_AssignButton1Name = "B1";
	m_AssignButton2Name = "B2";
	m_AssignButton3Name = "B3";

	m_bLatchingAssignButton1 = false;
	m_bLatchingAssignButton2 = false;
	m_bLatchingAssignButton3 = false;

	m_nGUIType = -1;
	m_nGUIThemeID = -1;
	m_bUseCustomVSTGUI = true;

	m_uControlTheme[0] = 0; m_uControlTheme[1] = 0; m_uControlTheme[2] = 0; m_uControlTheme[3] = 0; m_uControlTheme[4] = 0; m_uControlTheme[5] = 0; m_uControlTheme[6] = 0; m_uControlTheme[7] = 0; m_uControlTheme[8] = 0; m_uControlTheme[9] = 0; m_uControlTheme[10] = 0; m_uControlTheme[11] = 0; m_uControlTheme[12] = 0; m_uControlTheme[13] = 0; m_uControlTheme[14] = 0; m_uControlTheme[15] = 0; m_uControlTheme[16] = 0; m_uControlTheme[17] = 0; m_uControlTheme[18] = 0; m_uControlTheme[19] = 0; m_uControlTheme[20] = 0; m_uControlTheme[21] = 0; m_uControlTheme[22] = 0; m_uControlTheme[23] = 0; m_uControlTheme[24] = 0; m_uControlTheme[25] = 0; m_uControlTheme[26] = 0; m_uControlTheme[27] = 0; m_uControlTheme[28] = 0; m_uControlTheme[29] = 0; m_uControlTheme[30] = 0; m_uControlTheme[31] = 0; m_uControlTheme[32] = 0; m_uControlTheme[33] = 0; m_uControlTheme[34] = 0; m_uControlTheme[35] = 0; m_uControlTheme[36] = 0; m_uControlTheme[37] = 0; m_uControlTheme[38] = 0; m_uControlTheme[39] = 0; m_uControlTheme[40] = 0; m_uControlTheme[41] = 0; m_uControlTheme[42] = 0; m_uControlTheme[43] = 0; m_uControlTheme[44] = 0; m_uControlTheme[45] = 0; m_uControlTheme[46] = 0; m_uControlTheme[47] = 0; m_uControlTheme[48] = 0; m_uControlTheme[49] = 0; m_uControlTheme[50] = 0; m_uControlTheme[51] = 0; m_uControlTheme[52] = 0; m_uControlTheme[53] = 0; m_uControlTheme[54] = 0; m_uControlTheme[55] = 0; m_uControlTheme[56] = 0; m_uControlTheme[57] = 0; m_uControlTheme[58] = 0; m_uControlTheme[59] = 0; m_uControlTheme[60] = 0; m_uControlTheme[61] = 0; m_uControlTheme[62] = 0; m_uControlTheme[63] = 0; 

	m_uPlugInEx[0] = 6514; m_uPlugInEx[1] = 422; m_uPlugInEx[2] = 0; m_uPlugInEx[3] = 0; m_uPlugInEx[4] = 0; m_uPlugInEx[5] = 0; m_uPlugInEx[6] = 0; m_uPlugInEx[7] = 0; m_uPlugInEx[8] = 0; m_uPlugInEx[9] = 0; m_uPlugInEx[10] = 0; m_uPlugInEx[11] = 0; m_uPlugInEx[12] = 0; m_uPlugInEx[13] = 0; m_uPlugInEx[14] = 0; m_uPlugInEx[15] = 0; m_uPlugInEx[16] = 0; m_uPlugInEx[17] = 0; m_uPlugInEx[18] = 0; m_uPlugInEx[19] = 0; m_uPlugInEx[20] = 0; m_uPlugInEx[21] = 0; m_uPlugInEx[22] = 0; m_uPlugInEx[23] = 0; m_uPlugInEx[24] = 0; m_uPlugInEx[25] = 0; m_uPlugInEx[26] = 0; m_uPlugInEx[27] = 0; m_uPlugInEx[28] = 0; m_uPlugInEx[29] = 0; m_uPlugInEx[30] = 0; m_uPlugInEx[31] = 0; m_uPlugInEx[32] = 0; m_uPlugInEx[33] = 0; m_uPlugInEx[34] = 0; m_uPlugInEx[35] = 0; m_uPlugInEx[36] = 0; m_uPlugInEx[37] = 0; m_uPlugInEx[38] = 0; m_uPlugInEx[39] = 0; m_uPlugInEx[40] = 0; m_uPlugInEx[41] = 0; m_uPlugInEx[42] = 0; m_uPlugInEx[43] = 0; m_uPlugInEx[44] = 0; m_uPlugInEx[45] = 0; m_uPlugInEx[46] = 0; m_uPlugInEx[47] = 0; m_uPlugInEx[48] = 0; m_uPlugInEx[49] = 0; m_uPlugInEx[50] = 0; m_uPlugInEx[51] = 0; m_uPlugInEx[52] = 0; m_uPlugInEx[53] = 0; m_uPlugInEx[54] = 0; m_uPlugInEx[55] = 0; m_uPlugInEx[56] = 0; m_uPlugInEx[57] = 0; m_uPlugInEx[58] = 0; m_uPlugInEx[59] = 0; m_uPlugInEx[60] = 0; m_uPlugInEx[61] = 0; m_uPlugInEx[62] = 0; m_uPlugInEx[63] = 0; 
	m_fPlugInEx[0] = 0.000000; m_fPlugInEx[1] = 0.000000; m_fPlugInEx[2] = 0.000000; m_fPlugInEx[3] = 0.000000; m_fPlugInEx[4] = 0.000000; m_fPlugInEx[5] = 0.000000; m_fPlugInEx[6] = 0.000000; m_fPlugInEx[7] = 0.000000; m_fPlugInEx[8] = 0.000000; m_fPlugInEx[9] = 0.000000; m_fPlugInEx[10] = 0.000000; m_fPlugInEx[11] = 0.000000; m_fPlugInEx[12] = 0.000000; m_fPlugInEx[13] = 0.000000; m_fPlugInEx[14] = 0.000000; m_fPlugInEx[15] = 0.000000; m_fPlugInEx[16] = 0.000000; m_fPlugInEx[17] = 0.000000; m_fPlugInEx[18] = 0.000000; m_fPlugInEx[19] = 0.000000; m_fPlugInEx[20] = 0.000000; m_fPlugInEx[21] = 0.000000; m_fPlugInEx[22] = 0.000000; m_fPlugInEx[23] = 0.000000; m_fPlugInEx[24] = 0.000000; m_fPlugInEx[25] = 0.000000; m_fPlugInEx[26] = 0.000000; m_fPlugInEx[27] = 0.000000; m_fPlugInEx[28] = 0.000000; m_fPlugInEx[29] = 0.000000; m_fPlugInEx[30] = 0.000000; m_fPlugInEx[31] = 0.000000; m_fPlugInEx[32] = 0.000000; m_fPlugInEx[33] = 0.000000; m_fPlugInEx[34] = 0.000000; m_fPlugInEx[35] = 0.000000; m_fPlugInEx[36] = 0.000000; m_fPlugInEx[37] = 0.000000; m_fPlugInEx[38] = 0.000000; m_fPlugInEx[39] = 0.000000; m_fPlugInEx[40] = 0.000000; m_fPlugInEx[41] = 0.000000; m_fPlugInEx[42] = 0.000000; m_fPlugInEx[43] = 0.000000; m_fPlugInEx[44] = 0.000000; m_fPlugInEx[45] = 0.000000; m_fPlugInEx[46] = 0.000000; m_fPlugInEx[47] = 0.000000; m_fPlugInEx[48] = 0.000000; m_fPlugInEx[49] = 0.000000; m_fPlugInEx[50] = 0.000000; m_fPlugInEx[51] = 0.000000; m_fPlugInEx[52] = 0.000000; m_fPlugInEx[53] = 0.000000; m_fPlugInEx[54] = 0.000000; m_fPlugInEx[55] = 0.000000; m_fPlugInEx[56] = 0.000000; m_fPlugInEx[57] = 0.000000; m_fPlugInEx[58] = 0.000000; m_fPlugInEx[59] = 0.000000; m_fPlugInEx[60] = 0.000000; m_fPlugInEx[61] = 0.000000; m_fPlugInEx[62] = 0.000000; m_fPlugInEx[63] = 0.000000; 

	m_TextLabels[0] = ""; m_TextLabels[1] = ""; m_TextLabels[2] = ""; m_TextLabels[3] = ""; m_TextLabels[4] = ""; m_TextLabels[5] = ""; m_TextLabels[6] = ""; m_TextLabels[7] = ""; m_TextLabels[8] = ""; m_TextLabels[9] = ""; m_TextLabels[10] = ""; m_TextLabels[11] = ""; m_TextLabels[12] = ""; m_TextLabels[13] = ""; m_TextLabels[14] = ""; m_TextLabels[15] = ""; m_TextLabels[16] = ""; m_TextLabels[17] = ""; m_TextLabels[18] = ""; m_TextLabels[19] = ""; m_TextLabels[20] = ""; m_TextLabels[21] = ""; m_TextLabels[22] = ""; m_TextLabels[23] = ""; m_TextLabels[24] = ""; m_TextLabels[25] = ""; m_TextLabels[26] = ""; m_TextLabels[27] = ""; m_TextLabels[28] = ""; m_TextLabels[29] = ""; m_TextLabels[30] = ""; m_TextLabels[31] = ""; m_TextLabels[32] = ""; m_TextLabels[33] = ""; m_TextLabels[34] = ""; m_TextLabels[35] = ""; m_TextLabels[36] = ""; m_TextLabels[37] = ""; m_TextLabels[38] = ""; m_TextLabels[39] = ""; m_TextLabels[40] = ""; m_TextLabels[41] = ""; m_TextLabels[42] = ""; m_TextLabels[43] = ""; m_TextLabels[44] = ""; m_TextLabels[45] = ""; m_TextLabels[46] = ""; m_TextLabels[47] = ""; m_TextLabels[48] = ""; m_TextLabels[49] = ""; m_TextLabels[50] = ""; m_TextLabels[51] = ""; m_TextLabels[52] = ""; m_TextLabels[53] = ""; m_TextLabels[54] = ""; m_TextLabels[55] = ""; m_TextLabels[56] = ""; m_TextLabels[57] = ""; m_TextLabels[58] = ""; m_TextLabels[59] = ""; m_TextLabels[60] = ""; m_TextLabels[61] = ""; m_TextLabels[62] = ""; m_TextLabels[63] = ""; 

	m_uLabelCX[0] = 0; m_uLabelCX[1] = 0; m_uLabelCX[2] = 0; m_uLabelCX[3] = 0; m_uLabelCX[4] = 0; m_uLabelCX[5] = 0; m_uLabelCX[6] = 0; m_uLabelCX[7] = 0; m_uLabelCX[8] = 0; m_uLabelCX[9] = 0; m_uLabelCX[10] = 0; m_uLabelCX[11] = 0; m_uLabelCX[12] = 0; m_uLabelCX[13] = 0; m_uLabelCX[14] = 0; m_uLabelCX[15] = 0; m_uLabelCX[16] = 0; m_uLabelCX[17] = 0; m_uLabelCX[18] = 0; m_uLabelCX[19] = 0; m_uLabelCX[20] = 0; m_uLabelCX[21] = 0; m_uLabelCX[22] = 0; m_uLabelCX[23] = 0; m_uLabelCX[24] = 0; m_uLabelCX[25] = 0; m_uLabelCX[26] = 0; m_uLabelCX[27] = 0; m_uLabelCX[28] = 0; m_uLabelCX[29] = 0; m_uLabelCX[30] = 0; m_uLabelCX[31] = 0; m_uLabelCX[32] = 0; m_uLabelCX[33] = 0; m_uLabelCX[34] = 0; m_uLabelCX[35] = 0; m_uLabelCX[36] = 0; m_uLabelCX[37] = 0; m_uLabelCX[38] = 0; m_uLabelCX[39] = 0; m_uLabelCX[40] = 0; m_uLabelCX[41] = 0; m_uLabelCX[42] = 0; m_uLabelCX[43] = 0; m_uLabelCX[44] = 0; m_uLabelCX[45] = 0; m_uLabelCX[46] = 0; m_uLabelCX[47] = 0; m_uLabelCX[48] = 0; m_uLabelCX[49] = 0; m_uLabelCX[50] = 0; m_uLabelCX[51] = 0; m_uLabelCX[52] = 0; m_uLabelCX[53] = 0; m_uLabelCX[54] = 0; m_uLabelCX[55] = 0; m_uLabelCX[56] = 0; m_uLabelCX[57] = 0; m_uLabelCX[58] = 0; m_uLabelCX[59] = 0; m_uLabelCX[60] = 0; m_uLabelCX[61] = 0; m_uLabelCX[62] = 0; m_uLabelCX[63] = 0; 
	m_uLabelCY[0] = 0; m_uLabelCY[1] = 0; m_uLabelCY[2] = 0; m_uLabelCY[3] = 0; m_uLabelCY[4] = 0; m_uLabelCY[5] = 0; m_uLabelCY[6] = 0; m_uLabelCY[7] = 0; m_uLabelCY[8] = 0; m_uLabelCY[9] = 0; m_uLabelCY[10] = 0; m_uLabelCY[11] = 0; m_uLabelCY[12] = 0; m_uLabelCY[13] = 0; m_uLabelCY[14] = 0; m_uLabelCY[15] = 0; m_uLabelCY[16] = 0; m_uLabelCY[17] = 0; m_uLabelCY[18] = 0; m_uLabelCY[19] = 0; m_uLabelCY[20] = 0; m_uLabelCY[21] = 0; m_uLabelCY[22] = 0; m_uLabelCY[23] = 0; m_uLabelCY[24] = 0; m_uLabelCY[25] = 0; m_uLabelCY[26] = 0; m_uLabelCY[27] = 0; m_uLabelCY[28] = 0; m_uLabelCY[29] = 0; m_uLabelCY[30] = 0; m_uLabelCY[31] = 0; m_uLabelCY[32] = 0; m_uLabelCY[33] = 0; m_uLabelCY[34] = 0; m_uLabelCY[35] = 0; m_uLabelCY[36] = 0; m_uLabelCY[37] = 0; m_uLabelCY[38] = 0; m_uLabelCY[39] = 0; m_uLabelCY[40] = 0; m_uLabelCY[41] = 0; m_uLabelCY[42] = 0; m_uLabelCY[43] = 0; m_uLabelCY[44] = 0; m_uLabelCY[45] = 0; m_uLabelCY[46] = 0; m_uLabelCY[47] = 0; m_uLabelCY[48] = 0; m_uLabelCY[49] = 0; m_uLabelCY[50] = 0; m_uLabelCY[51] = 0; m_uLabelCY[52] = 0; m_uLabelCY[53] = 0; m_uLabelCY[54] = 0; m_uLabelCY[55] = 0; m_uLabelCY[56] = 0; m_uLabelCY[57] = 0; m_uLabelCY[58] = 0; m_uLabelCY[59] = 0; m_uLabelCY[60] = 0; m_uLabelCY[61] = 0; m_uLabelCY[62] = 0; m_uLabelCY[63] = 0; 

	m_pVectorJSProgram[JS_PROG_INDEX(0,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(0,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(0,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(0,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(0,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(0,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(0,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(1,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(1,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(1,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(1,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(1,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(1,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(1,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(2,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(2,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(2,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(2,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(2,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(2,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(2,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(3,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(3,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(3,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(3,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(3,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(3,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(3,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(4,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(4,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(4,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(4,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(4,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(4,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(4,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(5,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(5,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(5,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(5,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(5,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(5,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(5,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(6,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(6,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(6,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(6,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(6,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(6,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(6,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(7,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(7,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(7,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(7,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(7,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(7,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(7,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(8,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(8,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(8,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(8,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(8,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(8,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(8,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(9,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(9,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(9,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(9,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(9,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(9,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(9,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(10,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(10,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(10,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(10,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(10,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(10,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(10,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(11,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(11,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(11,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(11,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(11,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(11,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(11,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(12,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(12,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(12,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(12,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(12,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(12,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(12,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(13,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(13,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(13,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(13,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(13,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(13,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(13,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(14,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(14,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(14,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(14,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(14,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(14,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(14,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(15,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(15,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(15,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(15,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(15,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(15,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(15,6)] = 0.0000;


	m_JS_XCtrl.cControlName = "MIDI JS X";
	m_JS_XCtrl.uControlId = 0;
	m_JS_XCtrl.bMIDIControl = false;
	m_JS_XCtrl.uMIDIControlCommand = 176;
	m_JS_XCtrl.uMIDIControlName = 16;
	m_JS_XCtrl.uMIDIControlChannel = 0;


	m_JS_YCtrl.cControlName = "MIDI JS Y";
	m_JS_YCtrl.uControlId = 0;
	m_JS_YCtrl.bMIDIControl = false;
	m_JS_YCtrl.uMIDIControlCommand = 176;
	m_JS_YCtrl.uMIDIControlName = 17;
	m_JS_YCtrl.uMIDIControlChannel = 0;


	float* pJSProg = NULL;
	m_PresetNames[0] = "Factory Preset";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[0] = pJSProg;

	m_PresetNames[1] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[1] = pJSProg;

	m_PresetNames[2] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[2] = pJSProg;

	m_PresetNames[3] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[3] = pJSProg;

	m_PresetNames[4] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[4] = pJSProg;

	m_PresetNames[5] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[5] = pJSProg;

	m_PresetNames[6] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[6] = pJSProg;

	m_PresetNames[7] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[7] = pJSProg;

	m_PresetNames[8] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[8] = pJSProg;

	m_PresetNames[9] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[9] = pJSProg;

	m_PresetNames[10] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[10] = pJSProg;

	m_PresetNames[11] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[11] = pJSProg;

	m_PresetNames[12] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[12] = pJSProg;

	m_PresetNames[13] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[13] = pJSProg;

	m_PresetNames[14] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[14] = pJSProg;

	m_PresetNames[15] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[15] = pJSProg;

	// Additional Preset Support (avanced)


	// **--0xEDA5--**
// ------------------------------------------------------------------------------- //

	return true;

}














































































































































































































































































































