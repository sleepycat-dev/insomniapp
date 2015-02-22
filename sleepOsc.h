//oscillator superclass for sleep plugin
#include "plugin.h"
#include "SampleOscillator.h"

class CSleepOsc
{
	//POD
	UINT m_uOscType;
	enum{sine,white,pink,car,rain,ocean,tape,vinyl,male,female,user};
	char * m_cFilePath;
	char * m_cUserPath;

	//objects
	//sample oscillators
	CSampleOscillator m_SampleOsc;
	//WTOscs
	CWaveTable m_WTOsc;

	//functions
public:
	char * concatStrings(char* pString1, char* pString2);
	void init(int nSampleRate = 44100, float fFreq = 10.0, UINT osc = 0);
	void setSamplePath(char * cSamplePath);
	void setFreq(float fFreq);
	void setOscType(UINT oscType);
	void prepareForPlay();
	void doOscillate(float & fIn, float & fQIn);
};
