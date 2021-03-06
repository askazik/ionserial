// ====================================================================================
// ����� ������ � �����������
// ====================================================================================
#include "parus.h"

parusWork::parusWork(parusConfig &conf):
	_PLDFileName("p14x3m31.hex"),
	_DriverName("m14x3m.dll"),
	_DeviceName("ADM214x3M on AMBPCI"),
	_C(2.9979e8),
	_hFile(NULL)
{
	_g = conf.getGain()/6;	// ??? 6�� = ���������� � 4 ���� �� ��������
	if(_g > 7) _g = 7;

	_att = static_cast<char>(conf.getAttenuation());
	_fsync = conf.getPulse_frq();

	_height_step = conf.getHeightStep(); // ��� �� ������, �
    _height_count = conf.getHeightCount(); // ���������� ����� (������� ����������)
	// ������ ���������.
	_height_min = 1000 * conf.getHeightMin(); // ��������� ������, � (��, ��� ���� ��� ��������� �������������)
	_height_max = 1000 * conf.getHeightMax(); // �������� ������, � (��, ��� ���� ��� ��������� �������������)
	// ������ ���������.
	unsigned DH = _height_max - _height_min;
	unsigned h_min2 = _height_min * 2;
	unsigned h_max2 = h_min2 + DH;

	// ��������� ���� �������� ������
	openDataFile(conf);

	// ������������� ������ ��� ���������
	_unit1 = allocateUnit(_height_step, _height_count, _height_min, _height_max);
	_unit2 = allocateUnit(_height_step, _height_count, h_min2, h_max2);

	// �������� �� ������������ ���������� ������������.
    // ���������� �������� = ������ ������ FIFO ��� � 32-��������� ������.
	unsigned count = conf.getHeightCount(); 
	std::string stmp = std::to_string(static_cast<unsigned long long>(count));
	if(count < 64 && count >= 4096) // 4096 - �����!!! 16� = 4096*4�
		throw std::out_of_range("������ ������ ��� ������ ���� ������ 64 � ������ 4096. <" + stmp + ">");
	else
		if(count & (count - 1))
			throw std::out_of_range("������ ������ ��� ������ ���� �������� 2. <" + stmp + ">");

	// ��������� ����������, ��������� ���������� ����������, ������������� �������.
	_DrvPars = initADC(count);
	_DAQ = DAQ_open(_DriverName.c_str(), &_DrvPars); // NULL 
	if(!_DAQ)
		throw std::runtime_error("�� ���� ������� ������ ����� ������.");

	// ������ ������ ��� ������ ���.
	// ��� ����� 16-������ ������������ ������ �� ����� ������� (count - ���������� 32-��������� ����).
	buf_size = static_cast<unsigned long>(count * sizeof(unsigned long)); // ������ ������ ������ � ������
	_fullBuf = new unsigned long [count];

	// ����� TBLENTRY ������ ��������� ����� �� ������� �� DAQ_ASYNCREQDATA
	int ReqDataSize = sizeof(DAQ_ASYNCREQDATA); // ���������� ����� ���� �����!!!
	_ReqData = (DAQ_ASYNCREQDATA *)new char[ReqDataSize];
	
	// ���������� � ������� ������� ��� ���.
	_ReqData->BufCnt = 1; // ���������� ������ ���� �����
	TBLENTRY *pTbl = &_ReqData->Tbl[0]; // ��������� ������� �� ������� TBLENTRY
	pTbl[0].Addr = NULL; // ����������� ������� ������� Tbl[i].Size ������������� �� ��������� ������
	pTbl[0].Size = buf_size;

	// Request buffers of memory
	_RetStatus = DAQ_ioctl(_DAQ, DAQ_ioctlREAD_MEM_REQUEST, _ReqData); 
	if(_RetStatus != ERROR_SUCCESS)
		throw(DAQ_GetErrorMessage(_DAQ, _RetStatus));

	char *buffer = (char* )_ReqData->Tbl[0].Addr;
	memset( buffer, '*', buf_size );

	// ����� ������� ������������� ���, ��.
	double Frq = _C/(2.*conf.getHeightStep());
	// ������ ��������� � ������� ������������� ���.
	// ���������� ������� �������������, ������� ������������� ��� ���.
	DAQ_ioctl(_DAQ, DAQ_ioctlSETRATE, &Frq);
	conf.setHeightStep(_C/(2.*Frq)); // �������� ��� �� ������, �

	// ��������� ���������� ������������ ������.
	_xferData.UseIrq = 1;	// 1 - ������������ ����������
	_xferData.Dir = 1;		// ����������� ������ (1 - ����, 2 - ����� ������)
	// ��� ����������� �������� ������ �� ���������� ����� ���������� ���������� ����������������� ������.
	_xferData.AutoInit = 0;	// 1 - ����������������� ������

	// ��������� LPT1 ���� ��� ������ ����� ������� GiveIO.sys
	initLPT1();
}

parusWork::~parusWork(void){
	// Free buffers of memory
	delete _unit1.Addr;
	delete _unit2.Addr;
	delete _fullBuf;

	if(_DAQ)
		DAQ_ioctl(_DAQ, DAQ_ioctlREAD_MEM_RELEASE, NULL);
	_RetStatus = DAQ_close(_DAQ);
	if(_RetStatus != ERROR_SUCCESS)
		throw(DAQ_GetErrorMessage(_DAQ, _RetStatus));

	// ��������� ���� LPT
	CloseHandle(_LPTPort);

	// ��������� �������� ���� ������
	closeDataFile();
}

// ���������� ������� ���.
M214x3M_DRVPARS parusWork::initADC(unsigned int count){
	M214x3M_DRVPARS DrvPars;
    
	DrvPars.Pars = M214x3M_Params; // ������������� �������� �� ��������� ��� ��������� ADM214x1M			
	DrvPars.Carrier.Pars = AMB_Params; // ������������� �������� ��� �������� ������ AMBPCI
	DrvPars.Carrier.Pars.AdcFifoSize = count; // ������ ������ FIFO ��� � 32-��������� ������;
	// ���� ��� �� ����������, �� DacFifoSize ������ ���� ����� 0,
	// ����� ����� �� ����������� ����� �������� � ��������� ����������
	DrvPars.Carrier.Pars.DacFifoSize = 0; 

	// �������� �� ������ ����� � ����� �������.
	DrvPars.Carrier.Pars.ChanMask = 257; // ���� ��� ����� 1, �� ��������������� ���� �������, ����� - ��������.
	DrvPars.Pars.Gain[0].OnOff = 1;
	DrvPars.Pars.Gain[8].OnOff = 1;

	// ��������� �������� ������� ��������� ������������� ���
	DrvPars.Carrier.Pars.Start.Start = 1; // ������ �� ������� ����������� 0
	DrvPars.Carrier.Pars.Start.Src = 0;	// �� ���������� 0 �������� ������ �������� ������, �� ���������� 1 �������� ������ � ������� X4
	DrvPars.Carrier.Pars.Start.Cmp0Inv = 1;	// 1 ������������� �������� ������� � ������ ����������� 0
	DrvPars.Carrier.Pars.Start.Cmp1Inv = 0;	// 1 ������������� �������� ������� � ������ ����������� 1
	DrvPars.Carrier.Pars.Start.Pretrig = 0;	// 1 ������������� ������ �����������
	DrvPars.Carrier.Pars.Start.Trig.On = 1;	// 1 ������������� ����������� ������ �������/��������� ���
	DrvPars.Carrier.Pars.Start.Trig.Stop = 0;// ����� ��������� ��� � ���������� ������ �������: 0 - ����������� ���������
	DrvPars.Carrier.Pars.Start.Thr[0] = 1; // ��������� �������� � ������� ��� ������������ 0 � 1
	DrvPars.Carrier.Pars.Start.Thr[1] = 1;

	DrvPars.Carrier.HalPars = HAL_Params; // ������������� �������� �� ��������� ��� ���� ���������� ���������� (HAL)
										  // VendorID, DeviceID, InstanceDevice, LatencyTimer

	strcpy_s(DrvPars.Carrier.PldFileName, _PLDFileName.c_str()); // ���������� ������ ������  PLD �����

    return DrvPars;
}

// ���������� ������� ������ ������� ����� ����������� � �������� ���������� ������, 
// ����� ���� ���������� ���������� ���������, ��������� ������ �������.
// ��� ������������� ���������� �������������������� ����������� ��� ���������� �� �����������. 
// � ��������� ������ ������������ ������������� ����������� ����� ������� ����������� ���, 
// � ���������� ������� ����������� �������� ������ ����������� �����������.
void parusWork::ASYNC_TRANSFER(void){
	DAQ_ioctl(_DAQ, DAQ_ioctlASYNC_TRANSFER, &_xferData);
}

// ������ ��� �������� ���������� ������������ ����� ������ � �����. 
// ������� ���������� ��������� �������� � ������ ���������� �����, ���� � � ��������� ������
int parusWork::READ_BUFISCOMPLETE(unsigned long msTimeout){
	return DAQ_ioctl(_DAQ, DAQ_ioctlREAD_BUFISCOMPLETE, &msTimeout);
}

// ��������� ������ ������� ��������� �������� ������������ ����� ������. 
// ������� ���������� ����� �������� ������ (���������� � ����), ������ ��������� 
// � ������� ����� �� ������ ������� ������ (� ������), ����� ������������� �����������������.
void parusWork::READ_GETIOSTATE(void){
	DAQ_ioctl(_DAQ, DAQ_ioctlREAD_GETIOSTATE, &_curState);
}

// ��������� �������� ������� ������������ ����� ������. 
// ������� ���������� ����� �������� (�� ������ ���������� ��������) 
// ������ (���������� � ����), ������ ��������� � ������� ����� �� ������ 
// ���������� �������� ������ (� ������), ����� ������������� �����������������.
void parusWork::READ_ABORTIO(void){
	DAQ_ioctl(_DAQ, DAQ_ioctlREAD_ABORTIO, &_curState);
}

// ������ ��� �������� ���������� ����� ������, ��������������� �������� ������ 
// ������ ������� (DAQ_ioctlASYNC_TRANSFER). ������� ���������� ��������� �������� 
// � ������ ���������� �����, ���� � � ��������� ������.
int parusWork::READ_ISCOMPLETE(unsigned long msTimeout){
	return DAQ_ioctl(_DAQ, DAQ_ioctlREAD_ISCOMPLETE, &msTimeout);
}

// ������������� ���������� ���������
void parusWork::adjustSounding(unsigned int curFrq){
// curFrq -  �������, ��
// g - ��������
// att - ���������� (���/����)
	char			ag, sb, j, jk, jk1, j1, j2, jk2;
	int				i;
	double			step = 2.5;
	unsigned int	fp = 63000, nf, r = 4;
	unsigned int	s_gr[9] = {0, 3555, 8675, 13795, 18915, 24035, 29155, 34280, 39395};
	int				Address=888;

	_curFrq = curFrq;

	for ( i = 8; i > 0; )
		if ( curFrq > s_gr[--i] ){
			sb = i;
			break;
		}
	nf =(unsigned int)( ((double)curFrq + (double)fp) / step );
	
	// ������������� ������� � �������������� ��������.
	ag = _att + sb*2 + _g*16 + 128;
    _outp(Address, ag);

	// ������ ���� �� ������� � ����������
	for ( i = 2; i >= 0;  i-- ) { // ��� ���� ������� �������� �����������
		j = ( 0x1 & (r>>i) ) * 2 + 4;
		j1 = j + 1;
		jk = (j^0X0B)&0x0F;
		_outp( Address+2, jk );
		jk1 = (j1^0x0B)&0x0F;
		_outp(Address+2, jk1);
		_outp(Address+2, jk1);
		_outp(Address+2, jk);
	}

	// ������ ���� ������� � ����������
	for ( i = 15; i >= 0; i-- )  { // ��� ������� �����������
		j = ( 0x1 & (nf>>i) ) * 2 + 4;
		j1 = j2 = j + 1;
		if ( i == 0) 
			j2 -= 4;
		jk = (j^0X0B)&0X0F;
		_outp( Address+2, jk);
		jk1 = (j1^0X0B)&0X0F;
		_outp( Address+2, jk1);
		jk2 = (j2^0X0B)&0X0F;
		_outp( Address+2, jk2);
		_outp( Address+2, jk);
	}
}

// ������������� PIC �� ����� ��������������� ������������ ����������
// ����� �����, ������������ � PIC-����������, ����� ������ � ������� �� ����.
void parusWork::startGenerator(unsigned int nPulses){
// nPulses -  ���������� ��������� ����������
// fsound - ������� ���������� �����, ��
	double			fosc = 5e6;
	unsigned char	cdat[8] = {103, 159, 205, 218, 144, 1, 0, 0}; // 50 ��, 398 ����� (������-�� 400 �����. 2 ��������?)
	unsigned int	pimp_n, nimp_n, lnumb;
	unsigned long	NumberOfBytesWritten;
	
	lnumb = nPulses;

	// ==========================================================================================
	// ���������� ���������� �������� ���������
	pimp_n = (unsigned int)(fosc/(4.*(double)_fsync));		// ������ PIC �� ������	
	nimp_n = 0x1000d - pimp_n;								// PIC_TMR1       Fimp_min = 19 Hz
	cdat[0] = nimp_n%256;
	cdat[1] = nimp_n/256;
	cdat[2] = 0xCD;
	cdat[3] = 0xDA;
	cdat[4] = lnumb%256;
	cdat[5] = lnumb/256;
	cdat[6] = 0; // �� ������������
	cdat[7] = 0; // �� ������������

	// ���������� ���������� ���������� (������� ���������� �� ����� 19 ��.)
	// ==========================================================================================
 

	// ==========================================================================================
	// ������ ���������������
	// ==========================================================================================
	// �������� ���������� � �������� PIC16F870
	HANDLE	_COMPort = initCOM2();
	BOOL fSuccess = WriteFile(
		_COMPort,		// ��������� ��� �����
		&cdat,		// ��������� �� �����, ���������� ������, ������� ����� �������� � ����. 
		8,// ����� ������, ������� ����� �������� � ����.
		&NumberOfBytesWritten,//  ��������� �� ����������, ������� �������� ����� ���������� ������
		NULL // ��������� �� ��������� OVERLAPPED
	);
    if (!fSuccess){
		// Handle the error.
		throw std::runtime_error("������ �������� ���������� � �������� PIC16F870.");
	}
	
	// ��������� ����
	CloseHandle(_COMPort);
}

// ��� �������� ���������� � �������� PIC16F870
HANDLE parusWork::initCOM2(void){
	// ��������� ���� ��� ������
	HANDLE _COMPort = CreateFile(TEXT("COM2"), 
							GENERIC_READ | GENERIC_WRITE, 
							0, 
							NULL, 
							OPEN_EXISTING, 
							0, 
							NULL);
	if (_COMPort == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Error! ���������� ������� COM ����!");
	}
 
	// Build on the current configuration, and skip setting the size
	// of the input and output buffers with SetupComm.
	DCB dcb;
	BOOL fSuccess;

	// ��������
	fSuccess = GetCommState(_COMPort, &dcb);
	if (!fSuccess){
		// Handle the error.
        throw std::runtime_error("Error! GetCommState failed with error!");
	}

	// ����������
	dcb.BaudRate = CBR_19200;     // set the baud rate
	dcb.ByteSize = 8;             // data size, xmit, and rcv
	dcb.Parity = NOPARITY;        // no parity bit
	dcb.StopBits = TWOSTOPBITS;   // two stop bit

	// ����������
	fSuccess = SetCommState(_COMPort, &dcb);
	if (!fSuccess){
		// Handle the error.
        throw std::runtime_error("Error! SetCommState failed with error!");
	}

	// ��������
	fSuccess = GetCommState(_COMPort, &dcb);
	if (!fSuccess){
		// Handle the error.
		throw std::runtime_error("Error! GetCommState failed with error!");
	}

	return _COMPort;
}

// ��� ���������������� �����������.
void parusWork::initLPT1(void){
	_LPTPort = CreateFile(TEXT("\\\\.\\giveio"),
                         GENERIC_READ,
                         0,
                         NULL,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL,
                         NULL);
	if (_LPTPort==INVALID_HANDLE_VALUE){
        throw std::runtime_error("Error! Can't open driver GIVEIO.SYS! Press any key to exit...");
	}
}

// ��������� ���� ���������� ��� ������ � ������ � ���� ���������.
void parusWork::openDataFile(parusConfig &conf){
	// Obtain coordinated universal time (!!!! UTC !!!!!):
    // ==================================================================================================
    // The value returned generally represents the number of seconds since 00:00 hours, Jan 1, 1970 UTC
    // (i.e., the current unix timestamp). Although libraries may use a different representation of time:
    // Portable programs should not use the value returned by this function directly, but always rely on
    // calls to other elements of the standard library to translate them to portable types (such as
    // localtime, gmtime or difftime).
    // ==================================================================================================
	time_t ltime;
    time(&ltime);
	struct tm newtime;
	
	gmtime_s(&newtime, &ltime);

    // ���������� ��������� �����.
    dataHeader header;
    header.ver = conf.getVersion();
    header.time_sound = newtime;
    header.height_min = conf.getHeightMin(); // ��������� ������ ����������� ������, �
	header.height_max = conf.getHeightMax(); // �������� ������ ����������� ������, �
    header.height_step = conf.getHeightStep(); // ��� �� ������, �
    header.count_height = conf.getHeightCount(); // ����� ����� (������� ����������)
	header.pulse_frq = conf.getPulse_frq(); // ������� ����������� ���������, ��
    header.count_modules = conf.getModulesCount(); // ���������� ������� ������������

    // ����������� ��� ����� ������.
    std::stringstream name;
    name << std::setfill('0');
    name << std::setw(4);
    name << newtime.tm_year+1900 << std::setw(2);
    name << newtime.tm_mon+1 << std::setw(2) 
		<< newtime.tm_mday << std::setw(2) 
		<< newtime.tm_hour << std::setw(2) 
		<< newtime.tm_min << std::setw(2) << newtime.tm_sec;
    name << ".frq";

    // ���������� ������� ����.
	_hFile = CreateFile(name.str().c_str(),		// name of the write
                       GENERIC_WRITE,			// open for writing
                       0,						// do not share
                       NULL,					// default security
                       CREATE_ALWAYS,			// create new file always
                       FILE_ATTRIBUTE_NORMAL,	// normal file
                       NULL);					// no attr. template
    if(_hFile == INVALID_HANDLE_VALUE)
        throw std::runtime_error("������ �������� ����� ������ �� ������.");

    DWORD bytes;              // ������� ������ � ����
    BOOL Retval;
    Retval = WriteFile(_hFile,	// ������ � ����
              &header,			// ����� ������: ��� ������
              sizeof(header),	// ������� ������
              &bytes,			// ����� DWORD'a: �� ������ - ������� ��������
              0);				// A pointer to an OVERLAPPED structure.
	for(unsigned i=0; i<header.count_modules; i++){ // ��������������� ����� ������� ������������
		unsigned f = conf.getFreq(i);
		Retval = WriteFile(_hFile,	// ������ � ����
              &f,					// ����� ������: ��� ������
              sizeof(f),			// ������� ������
              &bytes,				// ����� DWORD'a: �� ������ - ������� ��������
              0);					// A pointer to an OVERLAPPED structure.
	}
    if (!Retval)
        throw std::runtime_error("������ ������ ��������� ����� ������.");    
}

// ���������� ��������� ������� ������ ��� ������� � ������� ��������� ������� �� ���.
void parusWork::fillUnits(void){	
	// � ������� ��� ��������� ��� ����� 16-������ ������������ ������ �� ����� ������� (count - ���������� 32-��������� ����).
	TBLENTRY *pTbl = &_ReqData->Tbl[0];
	memcpy(_fullBuf, pTbl[0].Addr, pTbl[0].Size); // �������� ���� ���������� �����

	unsigned long ul_Size = sizeof(unsigned long);
	// ������ ���������
	_unit1.Frq = _curFrq;
	memcpy(_unit1.Addr, _fullBuf + _unit1.iBeg, ul_Size * _unit1.Size); // �������� ����� ����������� ������

	// ������ ���������
	_unit2.Frq = _curFrq;
	memcpy(_unit2.Addr, _fullBuf + _unit2.iBeg, ul_Size * _unit2.Size); // �������� ����� ����������� ������
}

void parusWork::saveDirtyUnit(const dataUnit &unit){
	//unsigned nunsigned = sizeof(unsigned);
	//unsigned n = 3*nunsigned + unit.Size * sizeof(unsigned long);
	//char *tmpArray = new char [n];

	// �������� � ��������� ������
	//memcpy(tmpArray, &(unit.Frq), nunsigned);
	//memcpy(tmpArray+nunsigned, &(unit.HeightBeg), nunsigned);
	//memcpy(tmpArray+2*nunsigned, &(unit.Size), nunsigned);
	//memcpy(tmpArray+3*nunsigned, unit.Addr, unit.Size * sizeof(unsigned long));

	// Writing data from buffers into file (unsigned long = unsigned int)
	BOOL	bErrorFlag = FALSE;
	DWORD	dwBytesWritten = 0;	
	bErrorFlag = WriteFile( 
		_hFile,				// open file handle
		unit.Addr,			// start of data to write
		unit.Size * sizeof(unsigned long),					// number of bytes to write
		&dwBytesWritten,	// number of bytes that were written
		NULL);				// no overlapped structure
	if (!bErrorFlag) 
		throw std::runtime_error("�� ���� ��������� ���� ������ � ����.");
}

void parusWork::saveFullData(void){
	// � ������� ��� ��������� ��� ����� 16-������ ������������ ������ �� ����� ������� (count - ���������� 32-��������� ����).
	TBLENTRY *pTbl = &_ReqData->Tbl[0];
	memcpy(_fullBuf, pTbl[0].Addr, pTbl[0].Size); // �������� ���� ���������� �����

	// Writing data from buffer into file (unsigned long = unsigned int)
	BOOL	bErrorFlag = FALSE;
	DWORD	dwBytesWritten = 0;	
	bErrorFlag = WriteFile( 
		_hFile,				// open file handle
		_fullBuf,			// start of data to write
		buf_size,			// number of bytes to write
		&dwBytesWritten,	// number of bytes that were written
		NULL);				// no overlapped structure
	if (!bErrorFlag) 
		throw std::runtime_error("�� ���� ��������� ���� ������ � ����.");
}

// � ���� ����������� ����� ������ ��� ��������� � �������� (��� ����).
void parusWork::saveDirtyLine(void){
	saveDirtyUnit(_unit1);
	saveDirtyUnit(_unit2);
}

void parusWork::saveTheresholdLine(void){

}

void parusWork::closeDataFile(void){
	if(_hFile)
		CloseHandle(_hFile);
}

// ������������� ����� ������ ��� ������ ���������.
dataUnit parusWork::allocateUnit(unsigned height_step, unsigned height_count, unsigned height_min, unsigned height_max){
	dataUnit	unit;

	unsigned i_beg = static_cast<unsigned> (ceil(static_cast<float>(height_min)/height_step));
	unsigned i_end = static_cast<unsigned> (floor(static_cast<float>(height_max)/height_step));
	if(i_end >= height_count) // ����������� ������� ������� ������
		i_end = height_count-1;
	if(i_beg < 0 || i_end < 0 || i_beg > i_end)
		throw std::out_of_range("������������ �������� ��� ���������/�������� ����� ��������� ������ � ���������������� �����.");

	// ��� ����� 16-������ ������������ ������ �� ����� ������� (count - ���������� 32-��������� ����)
	unit.Size = i_end - i_beg + 1; // ������ �������
	unit.Addr = new unsigned long [unit.Size]; // ����� ������� ������
    unit.HeightBeg = i_beg * height_step; // ��������� ������, � (��, ��� ���� ��� ��������� �������������)
	unit.HeightEnd = i_end * height_step; // �������� ������, � (��, ��� ���� ��� ��������� �������������)
    unit.iBeg = i_beg; // ��������� �����
	unit.iEnd = i_end; // �������� �����

	return unit;
}
