
#include "Convolution.h"


/* constructor()
	You can initialize variables here.
	You can also allocate memory here as long is it does not
	require the plugin to be fully instantiated. If so, allocate in init()

*/
Cconvolution::Cconvolution()
{
	// Finish initializations here
	// reset all indices
	m_nReadIndexDL = 0;
	m_nReadIndexH = 0;
	m_nWriteIndex = 0;
	m_nConvolutionLength = 0;

	// --- clear out
	m_pLeftIR = NULL;
	m_pRightIR = NULL;

	// --- input buffers
	m_pBufferLeft = NULL;
	m_pBufferRight = NULL;

	// --- one way to use the CWaveData object is to initialize it at construction
	//     the file LPF2sir.wav was placed in my C:\ root directory
	//     this is a highly resonant LPF IR; easy to verify working
	//     SEE userInterfaceChange() for the second way to use this object
	//CWaveData* pWaveData = new CWaveData("C:\\LPF2sirQ.wav");
}

bool Cconvolution::openIRFile(CWaveData* pWaveData)
{
	if(!pWaveData->m_bWaveLoaded)
	{
		m_nConvolutionLength = 0;
		return false;
	}

	m_nConvolutionLength = (int)((float)pWaveData->m_uSampleCount/(float)pWaveData->m_uNumChannels);

	if(m_pLeftIR) delete [] m_pLeftIR;
	if(m_pRightIR) delete [] m_pRightIR;
	if(m_pBufferLeft) delete [] m_pBufferLeft;
	if(m_pBufferRight) delete [] m_pBufferRight;

	m_pLeftIR = new float[m_nConvolutionLength];
	m_pRightIR = new float[m_nConvolutionLength];
	m_pBufferLeft = new float[m_nConvolutionLength];
	m_pBufferRight = new float[m_nConvolutionLength];

	memset(m_pBufferLeft, 0, m_nConvolutionLength*sizeof(float));
	memset(m_pBufferRight, 0, m_nConvolutionLength*sizeof(float));
	memset(m_pLeftIR, 0, m_nConvolutionLength*sizeof(float));
	memset(m_pRightIR, 0, m_nConvolutionLength*sizeof(float));

	// --- for mono, just copy CWaveData buffer into our IR buffers
	if(pWaveData->m_uNumChannels == 1)
	{
		for(int i=0; i<pWaveData->m_uSampleCount; i++)
		{
			m_pLeftIR[i] = pWaveData->m_pWaveBuffer[i];
			m_pRightIR[i] = m_pLeftIR[i];
		}
	}

	return true;
}

/* destructor()
	Destroy variables allocated in the contructor()

*/
Cconvolution::~Cconvolution(void)
{
	if(m_pLeftIR) delete [] m_pLeftIR;
	if(m_pRightIR) delete [] m_pRightIR;
	if(m_pBufferLeft) delete [] m_pBufferLeft;
	if(m_pBufferRight) delete [] m_pBufferRight;
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
void Cconvolution::prepareForPlay()
{
	// Add your code here:
	memset(m_pBufferLeft, 0, m_nConvolutionLength*sizeof(float));
	memset(m_pBufferRight, 0, m_nConvolutionLength*sizeof(float));

	// reset all indices
	m_nReadIndexDL = 0;
	m_nReadIndexH = 0;
	m_nWriteIndex = 0;
}

float Cconvolution::processAudio(float fIn)
{
	if(m_nConvolutionLength <= 0)
	{
		// pass thru
		return fIn;
	}	
	
	// write x(n) -- now have x(n) -> x(n-1023)
	m_pBufferLeft[m_nWriteIndex] = fIn; 

	// reset: read index for Delay Line -> write index
	m_nReadIndexDL = m_nWriteIndex;

	// reset: read index for IR - > top (0)
	m_nReadIndexH = 0;

	// accumulator
	float yn_accum = 0;	

	// convolve:
	for(int i=0; i<m_nConvolutionLength; i++)
	{
		// do the sum of products
		yn_accum += m_pBufferLeft[m_nReadIndexDL]*m_pLeftIR[m_nReadIndexH];

		// advance the IR index
		m_nReadIndexH++;

		// decrement the Delay Line index
		m_nReadIndexDL--;

		// check for wrap of delay line (no need to check IR buffer)
		if(m_nReadIndexDL < 0)
			m_nReadIndexDL = m_nConvolutionLength -1;
	}

	// incremnent the pointers and wrap if necessary
	m_nWriteIndex++;
	if(m_nWriteIndex >= m_nConvolutionLength)
		m_nWriteIndex = 0;

	return yn_accum;
}

/*bool __stdcall Cconvolution::userInterfaceChange(int nControlIndex)
{
	// decode the control index, or delete the switch and use brute force calls
	switch(nControlIndex)
	{
		case 50:
		{
			// --- the other way to initialize is to let the user select an IR
			CWaveData* pWaveData = new CWaveData(NULL);
		
			if(pWaveData)
			{
				if(pWaveData->initWithUserWAVFile())
				{
					// --- use helper
					if(openIRFile(pWaveData))
						MessageBox ( NULL , "Opened IR file!" , "Message" , MB_OK);
					else
						MessageBox ( NULL , "Failed to open IR file!" , "Message" , MB_OK);

					// --- don't need anymore
					delete pWaveData;
				}
			}

			break;
		}

		default:
			break;
	}

	return true;
}*/
