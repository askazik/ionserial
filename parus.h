// ===========================================================================
// ������������ ���� ��� ������ � �����������.
// ===========================================================================
#ifndef __PARUS_H__
#define __PARUS_H__

#include <iomanip>
#include <cstring>
#include <sstream>
#include <ctime>
#include <stdexcept>
#include <conio.h>

// ��� ��������� ���������� � ���� ������������ ������� Windows API.
#include <windows.h>
// ������������ ����� ��� ������ � ������� ���.
#include "daqdef.h"
#include "m14x3mDf.h"
// ������������ ����� ��� ������ � �������.
#include "config.h"

struct dataHeader { 	    // === ��������� ����� ������ ===
    unsigned ver; // ����� ������
    struct tm time_sound; // GMT ����� ��������� ������������
    unsigned height_min; // ��������� ������, � (��, ��� ���� ��� ��������� �������������)
	unsigned height_max; // �������� ������, � (��, ��� ���� ��� ��������� �������������)
    unsigned height_step; // ��� �� ������, � (�������� ���, ����������� �� ������� ���)
    unsigned count_height; // ����� ����� (������ ��������� ������ ��� ��� ������������, fifo ������ ��� 4��. �.�. �� ������ 1024 �������� ��� ���� ������������ �������)
    unsigned count_modules; // ���������� �������/������ ������������
	unsigned pulse_frq; // ������� ����������� ���������, ��
};

struct dataUnit { 	    // === ������ ��������� ����� ===
    unsigned long	*Addr; // ����� ������� ������
	unsigned		Size; // ������ �������
    unsigned		HeightBeg; // ��������� ������, � (��, ��� ���� ��� ��������� �������������)
	unsigned		HeightEnd; // �������� ������, � (��, ��� ���� ��� ��������� �������������)
    unsigned		iBeg; // ��������� �����
	unsigned		iEnd; // �������� �����  
	unsigned		Frq; // ������� ������������
};

// ����� ������ � ����������� ������.
class parusWork {
	// ���������� ��������
	std::string _PLDFileName;
	std::string _DriverName;
	std::string _DeviceName;
	double _C; // �������� ����� � �������
	HANDLE _hFile; // �������� ���� ������

	unsigned int _g;
	char _att;
	unsigned int _fsync;
	unsigned int _curFrq;
    
	unsigned _height_step; // ��� �� ������, �
    unsigned _height_count; // ���������� �����
	unsigned _height_min; // ��������� ������, �� (��, ��� ���� ��� ��������� �������������)
	unsigned _height_max; // �������� ������, �� (��, ��� ���� ��� ��������� �������������)

	int _RetStatus;
	M214x3M_DRVPARS _DrvPars;
	int _DAQ; // ���������� ���������� (���� ����� ����, �� ������ ��������� ������)
	DAQ_ASYNCREQDATA *_ReqData; // ������� ������� ���
	DAQ_ASYNCXFERDATA _xferData; // ��������� ������������ ������
	DAQ_ASYNCXFERSTATE _curState;

	unsigned long *_fullBuf;
	unsigned long	buf_size;

	BYTE *getBuffer(void){return (BYTE*)(&_ReqData->Tbl[0].Addr);};
	DWORD getBufferSize(void){return (DWORD)(&_ReqData->Tbl[0].Size);};

	HANDLE	_LPTPort; // ��� ���������������� �����������
	HANDLE initCOM2(void);
	void initLPT1(void);

	// ������ � ������� �������� ������
	dataUnit	_unit1, _unit2; // ����� ������ ���������
	dataUnit allocateUnit(unsigned height_step, unsigned height_count, unsigned height_min, unsigned height_max); // ������������� ����� ������ ��� ������ ���������
	void openDataFile(parusConfig &conf);
	void saveDirtyUnit(const dataUnit &unit);
	void closeDataFile(void);
public:
    parusWork(parusConfig &conf);
	~parusWork(void);

	M214x3M_DRVPARS initADC(unsigned int nHeights);

	void ASYNC_TRANSFER(void);
	int READ_BUFISCOMPLETE(unsigned long msTimeout);
	void READ_ABORTIO(void);
	void READ_GETIOSTATE(void);
	int READ_ISCOMPLETE(unsigned long msTimeout);

	void adjustSounding(unsigned int curFrq);
	void startGenerator(unsigned int nPulses);

	// ������ � ������� �������� ������
	void fillUnits(void);
	void saveDirtyLine(void);
	void saveFullData(void);
	void saveTheresholdLine(void);
};

#endif // __PARUS_H__