// ====================================================================================
// Класс работы с аппаратурой
// ====================================================================================
#include "parus.h"

parusWork::parusWork(parusConfig &conf):
	_PLDFileName("p14x3m31.hex"),
	_DriverName("m14x3m.dll"),
	_DeviceName("ADM214x3M on AMBPCI"),
	_C(2.9979e8),
	_hFile(NULL)
{
	_g = conf.getGain()/6;	// ??? 6дБ = приращение в 4 раза по мощности
	if(_g > 7) _g = 7;

	_att = static_cast<char>(conf.getAttenuation());
	_fsync = conf.getPulse_frq();

	_height_step = conf.getHeightStep(); // шаг по высоте, м
    _height_count = conf.getHeightCount(); // количество высот (реально измеренных)
	// Первое отражение.
	_height_min = 1000 * conf.getHeightMin(); // начальная высота, м (всё, что ниже при обработке отбрасывается)
	_height_max = 1000 * conf.getHeightMax(); // конечная высота, м (всё, что выше при обработке отбрасывается)
	// Второе отражение.
	unsigned DH = _height_max - _height_min;
	unsigned h_min2 = _height_min * 2;
	unsigned h_max2 = h_min2 + DH;

	// Открываем файл выходных данных
	openDataFile(conf);

	// Распределение памяти для отражений
	_unit1 = allocateUnit(_height_step, _height_count, _height_min, _height_max);
	_unit2 = allocateUnit(_height_step, _height_count, h_min2, h_max2);

	// Проверка на корректность параметров зондирования.
    // Количество отсчетов = размер буфера FIFO АЦП в 32-разрядных словах.
	unsigned count = conf.getHeightCount(); 
	std::string stmp = std::to_string(static_cast<unsigned long long>(count));
	if(count < 64 && count >= 4096) // 4096 - сбоит!!! 16К = 4096*4б
		throw std::out_of_range("Размер буфера АЦП должен быть больше 64 и меньше 4096. <" + stmp + ">");
	else
		if(count & (count - 1))
			throw std::out_of_range("Размер буфера АЦП должен быть степенью 2. <" + stmp + ">");

	// Открываем устройство, используя глобальные переменные, установленные заранее.
	_DrvPars = initADC(count);
	_DAQ = DAQ_open(_DriverName.c_str(), &_DrvPars); // NULL 
	if(!_DAQ)
		throw std::runtime_error("Не могу открыть модуль сбора данных.");

	// Запрос памяти для работы АЦП.
	// Два сырых 16-битных чередующихся канала на одной частоте (count - количество 32-разрядных слов).
	buf_size = static_cast<unsigned long>(count * sizeof(unsigned long)); // размер буфера строки в байтах
	_fullBuf = new unsigned long [count];

	// блоки TBLENTRY плотно упакованы вслед за нулевым из DAQ_ASYNCREQDATA
	int ReqDataSize = sizeof(DAQ_ASYNCREQDATA); // используем толко один буфер!!!
	_ReqData = (DAQ_ASYNCREQDATA *)new char[ReqDataSize];
	
	// Подготовка к запросу буферов для АЦП.
	_ReqData->BufCnt = 1; // используем только один буфер
	TBLENTRY *pTbl = &_ReqData->Tbl[0]; // установим счётчик на нулевой TBLENTRY
	pTbl[0].Addr = NULL; // непрерывная область размера Tbl[i].Size запрашивается из системной памяти
	pTbl[0].Size = buf_size;

	// Request buffers of memory
	_RetStatus = DAQ_ioctl(_DAQ, DAQ_ioctlREAD_MEM_REQUEST, _ReqData); 
	if(_RetStatus != ERROR_SUCCESS)
		throw(DAQ_GetErrorMessage(_DAQ, _RetStatus));

	char *buffer = (char* )_ReqData->Tbl[0].Addr;
	memset( buffer, '*', buf_size );

	// Задаём частоту дискретизации АЦП, Гц.
	double Frq = _C/(2.*conf.getHeightStep());
	// Внести пожелания о частоте дискретизации АЦП.
	// Возвращает частоту дискретизации, реально установленную для АЦП.
	DAQ_ioctl(_DAQ, DAQ_ioctlSETRATE, &Frq);
	conf.setHeightStep(_C/(2.*Frq)); // реальный шаг по высоте, м

	// Настройка параметров асинхронного режима.
	_xferData.UseIrq = 1;	// 1 - использовать прерывания
	_xferData.Dir = 1;		// Направление обмена (1 - ввод, 2 - вывод данных)
	// Для непрерывной передачи данных по замкнутому циклу необходимо установить автоинициализацию обмена.
	_xferData.AutoInit = 0;	// 1 - автоинициализация обмена

	// Открываем LPT1 порт для записи через драйвер GiveIO.sys
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

	// Закрываем порт LPT
	CloseHandle(_LPTPort);

	// Закрываем выходной файл данных
	closeDataFile();
}

// Подготовка запуска АЦП.
M214x3M_DRVPARS parusWork::initADC(unsigned int count){
	M214x3M_DRVPARS DrvPars;
    
	DrvPars.Pars = M214x3M_Params; // Устанавливаем значения по умолчанию для субмодуля ADM214x1M			
	DrvPars.Carrier.Pars = AMB_Params; // Устанавливаем значения для базового модуля AMBPCI
	DrvPars.Carrier.Pars.AdcFifoSize = count; // размер буфера FIFO АЦП в 32-разрядных словах;
	// если ЦАП не установлен, то DacFifoSize должен быть равен 0,
	// иначе тесты на асинхронный вывод приведут к зависанию компьютера
	DrvPars.Carrier.Pars.DacFifoSize = 0; 

	// Включаем по одному входу в обоих каналах.
	DrvPars.Carrier.Pars.ChanMask = 257; // Если бит равен 1, то соответствующий вход включен, иначе - отключен.
	DrvPars.Pars.Gain[0].OnOff = 1;
	DrvPars.Pars.Gain[8].OnOff = 1;

	// Установка регистра режимов стартовой синхронизации АЦП
	DrvPars.Carrier.Pars.Start.Start = 1; // запуск по сигналу компаратора 0
	DrvPars.Carrier.Pars.Start.Src = 0;	// на компаратор 0 подается сигнал внешнего старта, на компаратор 1 подается сигнал с разъема X4
	DrvPars.Carrier.Pars.Start.Cmp0Inv = 1;	// 1 соответствует инверсии сигнала с выхода компаратора 0
	DrvPars.Carrier.Pars.Start.Cmp1Inv = 0;	// 1 соответствует инверсии сигнала с выхода компаратора 1
	DrvPars.Carrier.Pars.Start.Pretrig = 0;	// 1 соответствует режиму претриггера
	DrvPars.Carrier.Pars.Start.Trig.On = 1;	// 1 соответствует триггерному режиму запуска/остановки АЦП
	DrvPars.Carrier.Pars.Start.Trig.Stop = 0;// Режим остановки АЦП в триггерном режиме запуска: 0 - программная остановка
	DrvPars.Carrier.Pars.Start.Thr[0] = 1; // Пороговые значения в Вольтах для компараторов 0 и 1
	DrvPars.Carrier.Pars.Start.Thr[1] = 1;

	DrvPars.Carrier.HalPars = HAL_Params; // Устанавливаем значения по умолчанию для Слоя Аппаратных Абстракций (HAL)
										  // VendorID, DeviceID, InstanceDevice, LatencyTimer

	strcpy_s(DrvPars.Carrier.PldFileName, _PLDFileName.c_str()); // Используем старую версию  PLD файла

    return DrvPars;
}

// Инициирует процесс обмена данными между устройством и областью выделенной памяти, 
// после чего возвращает управление программе, вызвавшей данную функцию.
// При использовании прерываний перепрограммирование контроллера ПДП происходит по прерываниям. 
// В противном случае используется периодический программный опрос статуса контроллера ПДП, 
// в результате процесс непрерывной передачи данных существенно замедляется.
void parusWork::ASYNC_TRANSFER(void){
	DAQ_ioctl(_DAQ, DAQ_ioctlASYNC_TRANSFER, &_xferData);
}

// Служит для проверки завершения асинхронного ввода данных в буфер. 
// Функция возвращает ненулевое значение в случае завершения ввода, ноль – в противном случае
int parusWork::READ_BUFISCOMPLETE(unsigned long msTimeout){
	return DAQ_ioctl(_DAQ, DAQ_ioctlREAD_BUFISCOMPLETE, &msTimeout);
}

// Позволяет узнать текущее состояние процесса асинхронного ввода данных. 
// Функция возвращает номер текущего буфера (нумеруются с нуля), размер введенных 
// в текущий буфер на момент запроса данных (в байтах), число произведенных автоинициализаций.
void parusWork::READ_GETIOSTATE(void){
	DAQ_ioctl(_DAQ, DAQ_ioctlREAD_GETIOSTATE, &_curState);
}

// Позволяет прервать процесс асинхронного ввода данных. 
// Функция возвращает номер текущего (на момент завершения процесса) 
// буфера (нумеруются с нуля), размер введенных в текущий буфер на момент 
// завершения процесса данных (в байтах), число произведенных автоинициализаций.
void parusWork::READ_ABORTIO(void){
	DAQ_ioctl(_DAQ, DAQ_ioctlREAD_ABORTIO, &_curState);
}

// Служит для проверки завершения ввода данных, инициированного функцией начала 
// обмена данными (DAQ_ioctlASYNC_TRANSFER). Функция возвращает ненулевое значение 
// в случае завершения ввода, ноль – в противном случае.
int parusWork::READ_ISCOMPLETE(unsigned long msTimeout){
	return DAQ_ioctl(_DAQ, DAQ_ioctlREAD_ISCOMPLETE, &msTimeout);
}

// Программируем синтезатор приемника
void parusWork::adjustSounding(unsigned int curFrq){
// curFrq -  частота, Гц
// g - усиление
// att - ослабление (вкл/выкл)
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
	
	// Промежуточная частота и характеристики мощности.
	ag = _att + sb*2 + _g*16 + 128;
    _outp(Address, ag);

	// Запись шага по частоте в синтезатор
	for ( i = 2; i >= 0;  i-- ) { // Код шага частоты делителя синтезатора
		j = ( 0x1 & (r>>i) ) * 2 + 4;
		j1 = j + 1;
		jk = (j^0X0B)&0x0F;
		_outp( Address+2, jk );
		jk1 = (j1^0x0B)&0x0F;
		_outp(Address+2, jk1);
		_outp(Address+2, jk1);
		_outp(Address+2, jk);
	}

	// Запись кода частоты в синтезатор
	for ( i = 15; i >= 0; i-- )  { // Код частоты синтезатора
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

// Программируем PIC на вывод синхроимпульсов сканирования ионограммы
// Число строк, передаваемое в PIC-контроллер, можно задать с запасом на сбои.
void parusWork::startGenerator(unsigned int nPulses){
// nPulses -  количество импульсов генератора
// fsound - частота следования строк, Гц
	double			fosc = 5e6;
	unsigned char	cdat[8] = {103, 159, 205, 218, 144, 1, 0, 0}; // 50 Гц, 398 строк (Вообще-то 400 строк. 2 запасные?)
	unsigned int	pimp_n, nimp_n, lnumb;
	unsigned long	NumberOfBytesWritten;
	
	lnumb = nPulses;

	// ==========================================================================================
	// Магические вычисления Геннадия Ивановича
	pimp_n = (unsigned int)(fosc/(4.*(double)_fsync));		// Циклов PIC на период	
	nimp_n = 0x1000d - pimp_n;								// PIC_TMR1       Fimp_min = 19 Hz
	cdat[0] = nimp_n%256;
	cdat[1] = nimp_n/256;
	cdat[2] = 0xCD;
	cdat[3] = 0xDA;
	cdat[4] = lnumb%256;
	cdat[5] = lnumb/256;
	cdat[6] = 0; // не используется
	cdat[7] = 0; // не используется

	// Завершение магических вычислений (Частота следования не менее 19 Гц.)
	// ==========================================================================================
 

	// ==========================================================================================
	// Запуск синхроимпульсов
	// ==========================================================================================
	// Передача параметров в кристалл PIC16F870
	HANDLE	_COMPort = initCOM2();
	BOOL fSuccess = WriteFile(
		_COMPort,		// описатель сом порта
		&cdat,		// Указатель на буфер, содержащий данные, которые будут записаны в файл. 
		8,// Число байтов, которые будут записаны в файл.
		&NumberOfBytesWritten,//  Указатель на переменную, которая получает число записанных байтов
		NULL // Указатель на структуру OVERLAPPED
	);
    if (!fSuccess){
		// Handle the error.
		throw std::runtime_error("Ошибка передачи параметров в кристалл PIC16F870.");
	}
	
	// Закрываем порт
	CloseHandle(_COMPort);
}

// Для передачи параметров в кристалл PIC16F870
HANDLE parusWork::initCOM2(void){
	// Открываем порт для записи
	HANDLE _COMPort = CreateFile(TEXT("COM2"), 
							GENERIC_READ | GENERIC_WRITE, 
							0, 
							NULL, 
							OPEN_EXISTING, 
							0, 
							NULL);
	if (_COMPort == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Error! Невозможно открыть COM порт!");
	}
 
	// Build on the current configuration, and skip setting the size
	// of the input and output buffers with SetupComm.
	DCB dcb;
	BOOL fSuccess;

	// Проверка
	fSuccess = GetCommState(_COMPort, &dcb);
	if (!fSuccess){
		// Handle the error.
        throw std::runtime_error("Error! GetCommState failed with error!");
	}

	// Подготовка
	dcb.BaudRate = CBR_19200;     // set the baud rate
	dcb.ByteSize = 8;             // data size, xmit, and rcv
	dcb.Parity = NOPARITY;        // no parity bit
	dcb.StopBits = TWOSTOPBITS;   // two stop bit

	// Заполнение
	fSuccess = SetCommState(_COMPort, &dcb);
	if (!fSuccess){
		// Handle the error.
        throw std::runtime_error("Error! SetCommState failed with error!");
	}

	// Проверка
	fSuccess = GetCommState(_COMPort, &dcb);
	if (!fSuccess){
		// Handle the error.
		throw std::runtime_error("Error! GetCommState failed with error!");
	}

	return _COMPort;
}

// Для конфигурирования синтезатора.
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

// Открываем файл ионограммы для записи и вносим в него заголовок.
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

    // Заполнение заголовка файла.
    dataHeader header;
    header.ver = conf.getVersion();
    header.time_sound = newtime;
    header.height_min = conf.getHeightMin(); // начальная высота сохранённых данных, м
	header.height_max = conf.getHeightMax(); // конечная высота сохранённых данных, м
    header.height_step = conf.getHeightStep(); // шаг по высоте, м
    header.count_height = conf.getHeightCount(); // число высот (реально измеренных)
	header.pulse_frq = conf.getPulse_frq(); // частота зондирующих импульсов, Гц
    header.count_modules = conf.getModulesCount(); // количество модулей зондирования

    // Определение имя файла данных.
    std::stringstream name;
    name << std::setfill('0');
    name << std::setw(4);
    name << newtime.tm_year+1900 << std::setw(2);
    name << newtime.tm_mon+1 << std::setw(2) 
		<< newtime.tm_mday << std::setw(2) 
		<< newtime.tm_hour << std::setw(2) 
		<< newtime.tm_min << std::setw(2) << newtime.tm_sec;
    name << ".frq";

    // Попытаемся открыть файл.
	_hFile = CreateFile(name.str().c_str(),		// name of the write
                       GENERIC_WRITE,			// open for writing
                       0,						// do not share
                       NULL,					// default security
                       CREATE_ALWAYS,			// create new file always
                       FILE_ATTRIBUTE_NORMAL,	// normal file
                       NULL);					// no attr. template
    if(_hFile == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Ошибка открытия файла данных на запись.");

    DWORD bytes;              // счётчик записи в файл
    BOOL Retval;
    Retval = WriteFile(_hFile,	// писать в файл
              &header,			// адрес буфера: что писать
              sizeof(header),	// сколько писать
              &bytes,			// адрес DWORD'a: на выходе - сколько записано
              0);				// A pointer to an OVERLAPPED structure.
	for(unsigned i=0; i<header.count_modules; i++){ // последовательно пишем частоты зондирования
		unsigned f = conf.getFreq(i);
		Retval = WriteFile(_hFile,	// писать в файл
              &f,					// адрес буфера: что писать
              sizeof(f),			// сколько писать
              &bytes,				// адрес DWORD'a: на выходе - сколько записано
              0);					// A pointer to an OVERLAPPED structure.
	}
    if (!Retval)
        throw std::runtime_error("Ошибка записи заголовка файла данных.");    
}

// Заполнение локальных буферов данных для первого и второго отражения данными из АЦП.
void parusWork::fillUnits(void){	
	// В буффере АЦП сохранены два сырых 16-битных чередующихся канала на одной частоте (count - количество 32-разрядных слов).
	TBLENTRY *pTbl = &_ReqData->Tbl[0];
	memcpy(_fullBuf, pTbl[0].Addr, pTbl[0].Size); // копируем весь аппаратный буфер

	unsigned long ul_Size = sizeof(unsigned long);
	// Первое отражение
	_unit1.Frq = _curFrq;
	memcpy(_unit1.Addr, _fullBuf + _unit1.iBeg, ul_Size * _unit1.Size); // копируем часть аппаратного буфера

	// Второе отражение
	_unit2.Frq = _curFrq;
	memcpy(_unit2.Addr, _fullBuf + _unit2.iBeg, ul_Size * _unit2.Size); // копируем часть аппаратного буфера
}

void parusWork::saveDirtyUnit(const dataUnit &unit){
	//unsigned nunsigned = sizeof(unsigned);
	//unsigned n = 3*nunsigned + unit.Size * sizeof(unsigned long);
	//char *tmpArray = new char [n];

	// Упаковка в временный массив
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
		throw std::runtime_error("Не могу сохранить блок данных в файл.");
}

void parusWork::saveFullData(void){
	// В буффере АЦП сохранены два сырых 16-битных чередующихся канала на одной частоте (count - количество 32-разрядных слов).
	TBLENTRY *pTbl = &_ReqData->Tbl[0];
	memcpy(_fullBuf, pTbl[0].Addr, pTbl[0].Size); // копируем весь аппаратный буфер

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
		throw std::runtime_error("Не могу сохранить блок данных в файл.");
}

// В файл сохраняется линия данных без обработки и упаковки (как есть).
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

// Распределение блока данных для одного отражения.
dataUnit parusWork::allocateUnit(unsigned height_step, unsigned height_count, unsigned height_min, unsigned height_max){
	dataUnit	unit;

	unsigned i_beg = static_cast<unsigned> (ceil(static_cast<float>(height_min)/height_step));
	unsigned i_end = static_cast<unsigned> (floor(static_cast<float>(height_max)/height_step));
	if(i_end >= height_count) // ограничение размера выборки сверху
		i_end = height_count-1;
	if(i_beg < 0 || i_end < 0 || i_beg > i_end)
		throw std::out_of_range("Неправильные значения для начальной/конечной высот обработки данных в конфигурационном файле.");

	// два сырых 16-битных чередующихся канала на одной частоте (count - количество 32-разрядных слов)
	unit.Size = i_end - i_beg + 1; // размер массива
	unit.Addr = new unsigned long [unit.Size]; // адрес массива данных
    unit.HeightBeg = i_beg * height_step; // начальная высота, м (всё, что ниже при обработке отбрасывается)
	unit.HeightEnd = i_end * height_step; // конечная высота, м (всё, что выше при обработке отбрасывается)
    unit.iBeg = i_beg; // начальный номер
	unit.iEnd = i_end; // конечный номер

	return unit;
}
