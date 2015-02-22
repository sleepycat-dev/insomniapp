#include "plugin.h"

// abstract base class for RackAFX filters
class Cconvolution
{
public:
	// RackAFX Plug-In API Member Methods:
	// The followung 5 methods must be impelemented for a meaningful Plug-In
	//
	// 1. One Time Initialization
	Cconvolution();

	// 2. One Time Destruction
	~Cconvolution(void);

	// --- IR buffers
	float* m_pLeftIR;
	float* m_pRightIR;

	// --- input buffers
	float* m_pBufferLeft;
	float* m_pBufferRight;

	int m_nConvolutionLength;

	// --- helper function for IRs
	bool openIRFile(CWaveData* pWaveData);

	// read index for delay lines (input x buffers)
	int m_nReadIndexDL;

	// read index for impulse response buffers
	int m_nReadIndexH;

	// write index for input x buffer
	int m_nWriteIndex;

	float processAudio(float fIn);
	void prepareForPlay();
};
