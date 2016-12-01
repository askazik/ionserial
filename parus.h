// ===========================================================================
// Заголовочный файл для работы с аппаратурой.
// ===========================================================================
#ifndef __PARUS_H__
#define __PARUS_H__

#include <iomanip>
#include <cstring>
#include <sstream>
#include <ctime>
#include <stdexcept>
#include <conio.h>

// Для ускорения сохранения в файл используются функции Windows API.
#include <windows.h>
// Заголовочные файлы для работы с модулем АЦП.
#include "daqdef.h"
#include "m14x3mDf.h"
// Заголовочные файлы для работы с данными.
#include "config.h"

struct dataHeader { 	    // === Заголовок файла данных ===
    unsigned ver; // номер версии
    struct tm time_sound; // GMT время получения зондирования
    unsigned height_min; // начальная высота, м (всё, что ниже при обработке отбрасывается)
	unsigned height_max; // конечная высота, м (всё, что выше при обработке отбрасывается)
    unsigned height_step; // шаг по высоте, м (реальный шаг, вычисленный по частоте АЦП)
    unsigned count_height; // число высот (размер исходного буфера АЦП при зондировании, fifo нашего АЦП 4Кб. Т.е. не больше 1024 отсчётов для двух квадратурных каналов)
    unsigned count_modules; // количество модулей/частот зондирования
	unsigned pulse_frq; // частота зондирующих импульсов, Гц
};

struct dataUnit { 	    // === данные интервала высот ===
    unsigned long	*Addr; // адрес массива данных
	unsigned		Size; // размер массива
    unsigned		HeightBeg; // начальная высота, м (всё, что ниже при обработке отбрасывается)
	unsigned		HeightEnd; // конечная высота, м (всё, что выше при обработке отбрасывается)
    unsigned		iBeg; // начальный номер
	unsigned		iEnd; // конечный номер  
	unsigned		Frq; // частота зондирования
};

// Класс работы с аппаратурой Паруса.
class parusWork {
	// Глобальные описания
	std::string _PLDFileName;
	std::string _DriverName;
	std::string _DeviceName;
	double _C; // скорость света в вакууме
	HANDLE _hFile; // выходной файл данных

	unsigned int _g;
	char _att;
	unsigned int _fsync;
	unsigned int _curFrq;
    
	unsigned _height_step; // шаг по высоте, м
    unsigned _height_count; // количество высот
	unsigned _height_min; // начальная высота, км (всё, что ниже при обработке отбрасывается)
	unsigned _height_max; // конечная высота, км (всё, что выше при обработке отбрасывается)

	int _RetStatus;
	M214x3M_DRVPARS _DrvPars;
	int _DAQ; // дескриптор устройства (если равен нулю, то значит произошла ошибка)
	DAQ_ASYNCREQDATA *_ReqData; // цепочка буферов АЦП
	DAQ_ASYNCXFERDATA _xferData; // параметры асинхронного режима
	DAQ_ASYNCXFERSTATE _curState;

	unsigned long *_fullBuf;
	unsigned long	buf_size;

	BYTE *getBuffer(void){return (BYTE*)(&_ReqData->Tbl[0].Addr);};
	DWORD getBufferSize(void){return (DWORD)(&_ReqData->Tbl[0].Size);};

	HANDLE	_LPTPort; // для конфигурирования синтезатора
	HANDLE initCOM2(void);
	void initLPT1(void);

	// Работа с файлами выходных данных
	dataUnit	_unit1, _unit2; // блоки данных отражений
	dataUnit allocateUnit(unsigned height_step, unsigned height_count, unsigned height_min, unsigned height_max); // распределение блока данных для одного отражения
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

	// Работа с файлами выходных данных
	void fillUnits(void);
	void saveDirtyLine(void);
	void saveFullData(void);
	void saveTheresholdLine(void);
};

#endif // __PARUS_H__