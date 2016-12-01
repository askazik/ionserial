// ===========================================================================
// Заголовочный файл для работы кодированием данных зондирования.
// ===========================================================================
#ifndef __ENCODE_H__
#define __ENCODE_H__

#include <fstream>
#include <cmath>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <windows.h>
#include <ctime>

#include "config.h"

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Помним, что структуры в памяти выравниваются в зависимости
// от компилятора, разрядности и архитектуры системы. 
// Оптимальный выход в использовании машинно независимых форматов
// данных. Например HDF, CDF и.т.п.
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

#pragma pack(push, 1)

struct adcChannel {
    unsigned short b0: 1;  // Устанавливается в 1 в первом отсчете блока для каждого канала АЦП (т.е. после старта АЦП или после каждого старта в старт-стопном режиме).
    unsigned short b1: 1;  // Устанавливается в 1 в отсчете, который соответствует последнему опрашиваемому входу (по порядку расположения номеров входов в памяти мультиплексора) для каждого канала АЦП.    
    short value : 14; // данные из одного канала АЦП, 14-разрядное слово
};

struct adcTwoChannels {
    adcChannel re; // условно первая квадратура
    adcChannel im; // условно вторая квадратура
};

// Формат до 2015 г.
struct ionHeaderOld { 	        // === Заголовок файла измерений ===   
	unsigned char st;		// Station  <Ростов = 3>     > 1
	unsigned int f_beg;     // ORIGN FREQUENCY     	     > 2
	unsigned int f_step;    // STEP FREQUENCY            > 4
	unsigned int f_end;     // Final frequency           > 6
	double		 dh;		// Height step, m            > 8
	unsigned int scale;  	// Scale type & lines        >10
							// Format: <S_ppppppppppppp>
							// S=0 -> line  S=1 -> log
	unsigned char   sec;   	// Seconds                   >12
	unsigned char  	min;   	// Minutes                   >13
	unsigned char  	hour; 	// Hours                     >14
	unsigned char  	day;	// Data                      >15
	unsigned char	mon;	// Month                     >16
	unsigned char	year; 	// Year                      >17
	unsigned char 	fr;    	// Rept_freq (Hz)            >18
	unsigned char	an;    	// Ant_type & polarisation flag  >19
							//  & Doppler flag & number of pulses
							// Format: <AAPDnnnn>
	unsigned char	ka;    	// Attenuation               >20
					        // Flags of configuration:
	unsigned char	p_len;	// Pulse length (n*50 mks)   >21
	unsigned char	power;	// Transmitter power n kWt   >22
	unsigned char	ch_r;  	//                       >23
    unsigned char	ver;   	// Version of program    >24
	unsigned int 	yday;	// Day of year (0 - 365; January 1 = 0) >25
	unsigned int    wday;	// Day of week (0 - 6; Sunday = 0)      >27
	unsigned int    ks;     //                       >29
	unsigned int    count;  //                       >31
};

struct ionHeaderNew1 { 	    // === Заголовок файла ионограмм ===   
	char project_name[16];	// Название проекта (PARUS)
	char format_version[8]; // Версия формата (ION1, ION2,...)
	
	unsigned char   sec;   	// Seconds   
	unsigned char  	min;   	// Minutes 
	unsigned char  	hour; 	// Hours 
	unsigned char  	day;	// Data 
	unsigned char	mon;	// Month
	unsigned char	year; 	// Year
	
	unsigned char 	imp_frq;// частота посылки зондирующего импульса, Гц (20...100)	
	unsigned char	imp_duration;// длительность зондирующих импульсов, мкс
	unsigned char	imp_count;// зондирующих импульсов на каждой частоте
	unsigned char	att;    	// ослабление (аттенюатор)
	unsigned char	power;	// усиление

	unsigned short f_beg;   // начальная частота, кГц
	unsigned short f_step;  // шаг по частоте, кГц
	unsigned short f_end;   // конечная частота, кГц

	unsigned short  count;  // количество отсчётов высоты
	double dh;              // шаг по высоте, км
};

struct ionHeaderNew2 { 	    // === Заголовок файла ионограмм ===
    unsigned ver; // номер версии
    struct tm time_sound; // GMT время получения ионограммы
    unsigned height_min; // начальная высота, м
    unsigned height_step; // шаг по высоте, м
    unsigned count_height; // число высот
    unsigned switch_frequency; // частота переключения антенн ионозонда 
	unsigned freq_min; // начальная частота, кГц (первого модуля)
	unsigned freq_max; // конечная частота, кГц (последнего модуля)   
	unsigned count_freq; // число частот во всех модулях
    unsigned count_modules; // количество модулей зондирования

    // Начальная инициализация структуры.
    ionHeaderNew2(void)
    {
        ver = 2;
        height_min = 0; // Это не означает, что зондирование от поверхности. Есть задержки!!!
        height_step = 0;
        count_height = 0;
        switch_frequency = 0;
        freq_min = 0;
        freq_max = 0;    
        count_freq = 0;
        count_modules = 0;
    }
};

struct dataHeader { 	    // === Заголовок файла данных ===
    unsigned ver; // номер версии
    struct tm time_sound; // GMT время получения ионограммы
    unsigned height_min; // начальная высота, м
    unsigned height_step; // шаг по высоте, м
    unsigned count_height; // число высот
    unsigned count_modules; // количество модулей зондирования

    // Начальная инициализация структуры.
    dataHeader(void)
    {
        ver = 0;
        height_min = 0; // Это не означает, что зондирование от поверхности. Есть задержки!!!
        height_step = 0;
        count_height = 0;
        count_modules = 0;
    }
};

struct ionPackedData { // Упакованные данные ионограммы.
	unsigned size; // Размер упакованной ионограммы в байтах.
	unsigned char *ptr;   // Указатель на блок данных упакованной ионограммы.
};

/* =====================================================================  */
/* Родные структуры данных ИПГ-ИЗМИРАН */
/* =====================================================================  */
/* Каждая строка начинается с заголовка следующей структуры */
struct FrequencyData {
    unsigned short frequency; //!< Частота зондирования, [кГц].
    unsigned short gain_control; // !< Значение ослабления входного аттенюатора дБ.
    unsigned short pulse_time; //!< Время зондирования на одной частоте, [мс].
    unsigned char pulse_length; //!< Длительность зондирующего импульса, [мкc].
    unsigned char band; //!< Полоса сигнала, [кГц].
    unsigned char type; //!< Вид модуляции (0 - гладкий импульс, 1 - ФКМ).
    unsigned char threshold_o; //!< Порог амплитуды обыкновенной волны, ниже которого отклики не будут записываться в файл, [Дб/ед. АЦП].
    unsigned char threshold_x; //!< Порог амплитуды необыкновенной волны, ниже которого отклики не будут записываться в файл, [Дб/ед. АЦП].
    unsigned char count_o; //!< Число сигналов компоненты O.
    unsigned char count_x; //!< Число сигналов компоненты X.
};

/* Сначала следуют FrequencyData::count_o структур SignalResponse, описывающих обыкновенную волну. */
/* Cразу же после перечисления всех SignalResponse  и SignalSample  для обыкновенных откликов следуют FrequencyData::count_x */
/* структур SignalResponse, описывающих необыкновенные отклики с массивом структур SignalSample после каждой из них. Величины */
/* FrequencyData::count_o и FrequencyData::count_x могут быть равны нулю, тогда соответствующие данные отсутствуют. */
struct SignalResponse {
    unsigned long height_begin; //!< начальная высота, [м]
    unsigned short count_samples; //!< Число дискретов
};

/* =====================================================================  */

#pragma pack(pop)

// Декларация внутренних функций

int ReadADCIntoFile(int daq);
unsigned char* loadLine(unsigned long *data, int n); // усечение данных до char
unsigned int* loadIonogramLine(unsigned long *data, int n); // без усечения данных до char
void sumIonogramLine(unsigned int *dataDestination, unsigned int *dataSource, int n); // суммирование
unsigned char* shiftLine6bits(unsigned int *data, int n); // усечение данных до char

int comp(const void *i, const void *j);
unsigned char getThereshold(unsigned char *arr, unsigned n);
void saveLine(unsigned char *data, unsigned size, unsigned _HeightStep, unsigned short curFrq, HANDLE hFile);


#endif // __ENCODE_H__
