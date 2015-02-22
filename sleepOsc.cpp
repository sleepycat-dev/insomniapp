#include "sleepOsc.h"

void CSleepOsc::init(int nSampleRate, float fFreq, UINT uOsc)
{
	m_WTOsc.setSampleRate(nSampleRate);
	m_WTOsc.m_fFrequency_Hz = fFreq;
	m_WTOsc.m_uPolarity = 0;
	m_WTOsc.reset();
	m_WTOsc.prepareForPlay();
	m_WTOsc.cookFrequency();

	m_SampleOsc.m_bSingleCycleSample = false;
	m_SampleOsc.m_bPitchlessSample = true;
	m_SampleOsc.m_uLoopMode = 1; //1 for loop
}

void CSleepOsc::prepareForPlay()
{
	m_SampleOsc.reset();
	m_SampleOsc.update();
}

void CSleepOsc::setSamplePath(char * cSamplePath)
{
	m_cFilePath = cSamplePath;
	m_SampleOsc.initWithFilePath(m_cFilePath);
}

void CSleepOsc::setFreq(float fFreq)
{
	m_WTOsc.m_fFrequency_Hz = fFreq;
	m_WTOsc.cookFrequency();
}

char* CSleepOsc::concatStrings(char* pString1, char* pString2)
{
	int n = strlen(pString1);
	int m = strlen(pString2);

	char* p = new char[n+m+1];
	strcpy(p, pString1);

	return strncat(p, pString2, m);
}

void CSleepOsc::setOscType(UINT oscType)
{
	m_uOscType = oscType;
	
	switch (oscType)
	{
		case white:
		{
			char * temp = concatStrings(m_cFilePath, "\\white.wav");
			m_SampleOsc.initWithFilePath(temp);
			break;	
		}
		case pink:
		{
			char * temp = concatStrings(m_cFilePath, "\\pink.wav");
			m_SampleOsc.initWithFilePath(temp);
			break;
		}
		case car:
		{
			char * temp = concatStrings(m_cFilePath, "\\car.wav");
			m_SampleOsc.initWithFilePath(temp);
			break;
		}
		case rain:
		{
			char * temp = concatStrings(m_cFilePath, "\\rain.wav");
			m_SampleOsc.initWithFilePath(temp);
			break;
		}
		case ocean:
		{
			char * temp = concatStrings(m_cFilePath, "\\ocean.wav");
			m_SampleOsc.initWithFilePath(temp);
			break;
		}
		case vinyl:
		{
			char * temp = concatStrings(m_cFilePath, "\\vinyl.wav");
			m_SampleOsc.initWithFilePath(temp);
			break;
		}
		case tape:
		{
			char * temp = concatStrings(m_cFilePath, "\\tape.wav");
			m_SampleOsc.initWithFilePath(temp);
			break;
		}
		case male:
		{
			char * temp = concatStrings(m_cFilePath, "\\male.wav");
			m_SampleOsc.initWithFilePath(temp);
			break;
		}
		case female:
		{
			char * temp = concatStrings(m_cFilePath, "\\female.wav");
			m_SampleOsc.initWithFilePath(temp);
			break;
		}
		case user:
		{
			HWND myWindow = GetForegroundWindow();
			char buffer[MAX_PATH] = "";
			OPENFILENAMEA ofn = {0};  // note:  OPENFILENAMEA, not OPENFILENAME
			// the 'A' at the end specifies we want the 'char' version and not the 'TCHAR' version
			// if you want the 'wchar_t' version, you want to use OPENFILENAMEW instead

			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = myWindow; // not entirely necessary if you don't have a window
			ofn.lpstrFilter = "AudioFiles\0*.wav\0All Files\0*.*\0";
			ofn.nFilterIndex = 1; // for some reason this is 1-based not zero-based.  grrrrr

			ofn.Flags = OFN_FILEMUSTEXIST;  // only allow the user to open files that actually exist

			// the most important bits:
			ofn.lpstrFile = buffer;
			ofn.nMaxFile = MAX_PATH;  // size of our 'buffer' buffer


			// Now that we've prepped the struct, actually open the dialog:
			//  the below function call actually opens the "File Open" dialog box, and returns
			//  once the user exits out of it
			if( !GetOpenFileNameA( &ofn ) ) // <- again note the 'A' at the end, signifying the 'char' version
			{
			  // code reaches here if the user hit 'Cancel'
			}
			else
			{
				// code reaches here if the user hit 'OK'.  The full path of the file
				  //  they selected is now stored in 'buffer'
				m_SampleOsc.initWithFilePath(buffer);
			}
			break;
		}
		default:
		{
			char * temp = concatStrings(m_cFilePath, "\\pink.wav");
			m_SampleOsc.initWithFilePath(temp);
			break;
		}
	}
}

void CSleepOsc::doOscillate(float & fIn, float & fQIn)
{
	if(m_uOscType == sine)
		m_WTOsc.doOscillate(&fIn, &fQIn);
	else
	{
		double fTemp = (double)fIn;
		fIn = (float)m_SampleOsc.doOscillate(&fTemp);
	}
}
