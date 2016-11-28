// ===========================================================================
// Конвертация файлов амплитуд по алгоритму ИПГ.
// ===========================================================================
#include "encode.h"

// Упаковка строки ионограммы по алгоритму ИЗМИРАН-ИПГ.
// data - сырая строка
// _Count - число отсчетов АЦП
// _HeightStep - шаг по высоте, м
// _Frq - частота зондирования, кГц
// hFile - дескриптор выходного файла
void saveLine(BYTE *data, unsigned size, unsigned _HeightStep, unsigned short _Frq, HANDLE hFile)
{
    // Выделим буфер под упакованную строку. Считаем, что упакованная строка не больше исходной.
	unsigned n = size;
	unsigned char *tmpArray = new unsigned char [n];
    unsigned char *tmpLine = new unsigned char [n];
	unsigned char *tmpAmplitude = new unsigned char [n];
    
    unsigned j;
    unsigned char *dataLine = data;
    unsigned char thereshold;
    SignalResponse tmpSignalResponse;
	unsigned char countOutliers;
	unsigned short countFrq = 0; // количество байт в упакованном частотном массиве

	// Определим уровень наличия выбросов.
	FrequencyData tmpFrequencyData;
	thereshold = getThereshold(dataLine, n);            
	tmpFrequencyData.frequency = _Frq;
	tmpFrequencyData.threshold_o = thereshold;
	tmpFrequencyData.threshold_x = 0;
	tmpFrequencyData.count_o = 0; // может изменяться
	tmpFrequencyData.count_x = 0; // Антенна у нас одна о- и х- компоненты объединены.

	unsigned short countLine = 0; // количество байт в упакованной строке
	countOutliers = 0; // счетчик количества выбросов в текущей строке
	j = 0;
	while(j < n) // Находим выбросы
	{
		if(dataLine[j] > thereshold) // Выброс найден - обрабатываем его.
		{
			tmpSignalResponse.height_begin = static_cast<unsigned long>(floor(1.0 * j * _HeightStep));
			tmpSignalResponse.count_samples = 1;
			tmpAmplitude[tmpSignalResponse.count_samples-1] = dataLine[j];
			j++; // переход к следующему элементу
			while(dataLine[j] > thereshold && j < n)
			{
				tmpSignalResponse.count_samples++;
				tmpAmplitude[tmpSignalResponse.count_samples-1] = dataLine[j];
				j++; // переход к следующему элементу
			}
			countOutliers++; // прирастим количество выбросов в строке

			// Собираем упакованные выбросы
			// Заголовок выброса.
			unsigned short nn = sizeof(SignalResponse);
			memcpy(tmpLine+countLine, &tmpSignalResponse, nn);
			countLine += nn;
			// Данные выброса.
			nn = tmpSignalResponse.count_samples*sizeof(unsigned char);
			memcpy(tmpLine+countLine, tmpAmplitude, nn);
			countLine += nn;
		}
		else
			j++; // тупо двигаемся дальше
	}

	// Данные для текущей частоты помещаем в буфер.
	// Заголовки частот пишутся всегда, даже если выбросы отсутствуют.
	tmpFrequencyData.count_o = countOutliers;
	// Заголовок частоты.
	unsigned nFrequencyData = sizeof(FrequencyData);
	memcpy(tmpArray, &tmpFrequencyData, nFrequencyData);
	// сохраняем ранее сформированную цепочку выбросов
	if(countLine)
		memcpy(tmpArray+nFrequencyData, tmpLine, countLine);

	// Writing data from buffers into file (unsigned long = unsigned int)
	BOOL	bErrorFlag = FALSE;
	DWORD	dwBytesWritten = 0;	
	bErrorFlag = WriteFile( 
		hFile,									// open file handle
		reinterpret_cast<char*>(tmpArray),		// start of data to write
		countLine + nFrequencyData,				// number of bytes to write
		&dwBytesWritten,						// number of bytes that were written
		NULL);									// no overlapped structure
	if (!bErrorFlag) 
		throw std::runtime_error("Не могу сохранить строку ионограммы в файл.");
	
    delete [] tmpLine;
	delete [] tmpAmplitude;
	delete [] tmpArray;
} 

// сравнение целых
int comp(const void *i, const void *j)
{
    return *(unsigned char *)i - *(unsigned char *)j;
}

// Возвращает уровень более которого наблюдается выброс.
unsigned char getThereshold(unsigned char *arr, unsigned n)
{
    unsigned char *sortData = new unsigned char [n]; // буферный массив для сортировки
    unsigned char Q1, Q3, dQ;
    unsigned short maxLim = 0;

    // 1. Упорядочим данные по возрастанию.
    memcpy(sortData, arr, n);
    qsort(sortData, n, sizeof(unsigned char), comp);
    // 2. Квартили (n - чётное, степень двойки!!!)
    Q1 = min(sortData[n/4],sortData[n/4-1]);
    Q3 = max(sortData[3*n/4],sortData[3*n/4-1]);
    // 3. Межквартильный диапазон
    dQ = Q3 - Q1;
    // 4. Верхняя граница выбросов.
    maxLim = Q3 + 3 * dQ;    

    delete [] sortData;

    return (maxLim>=255)? 255 : static_cast<unsigned char>(maxLim);
}

// Преобразование "сырой" квадратурной строки в строку значений амплитуд с усечением данных.
unsigned char* loadLine(unsigned long *data, int n)
{
	unsigned char *cdata = NULL;

	// Обработка сырых данных.
	cdata = new unsigned char [n];
	int re, im, abstmp;
    for(int i = 0; i < n; i++)
		{
            // Используем двухканальную интерпретацию через анонимную структуру
            union {
                unsigned long word;     // 4-байтное слово двухканального АЦП
                adcTwoChannels twoCh;  // двухканальные (квадратурные) данные
            };
            
			// Разбиение на квадратуры. 
			// Значимы только старшие 14 бит. Младшие 2 бит - технологическая окраска.
            word = data[i];
            re = static_cast<int>(twoCh.re.value) >> 2;
            im = static_cast<int>(twoCh.im.value) >> 2;

            // Объединим квадратурную информацию в одну амплитуду.
			abstmp = static_cast<int>(floor(sqrt(re*re + im*im*1.0)));

			// Усечение данных до размера 8 бит (на 2 бита уже сместились ранее).
			cdata[i] = static_cast<unsigned char>(abstmp >> 6);
		}            
    
    return cdata;
}

// Преобразование "сырой" квадратурной строки в строку значений амплитуд без усечения данных.
unsigned int* loadIonogramLine(unsigned long *data, int n)
{
	unsigned int *idata = NULL;

	// Обработка сырых данных.
	idata = new unsigned int [n];
	int re, im, abstmp;
    for(int i = 0; i < n; i++)
		{
            // Используем двухканальную интерпретацию через анонимную структуру
            union {
                unsigned long word;     // 4-байтное слово двухканального АЦП
                adcTwoChannels twoCh;  // двухканальные (квадратурные) данные
            };
            
			// Разбиение на квадратуры. 
			// Значимы только старшие 14 бит. Младшие 2 бит - технологическая окраска.
            word = data[i];
            re = static_cast<int>(twoCh.re.value) >> 2;
            im = static_cast<int>(twoCh.im.value) >> 2;

            // Объединим квадратурную информацию в одну амплитуду.
			abstmp = static_cast<int>(floor(sqrt(re*re + im*im*1.0)));
		}            
    
    return idata;
}

// Суммирование последовательных линий ионограммы.
void sumIonogramLine(unsigned int *dataDestination, unsigned int *dataSource, int n)
{
    for(int i = 0; i < n; i++)
		dataDestination[i] = dataDestination[i]/2 + dataSource[i]/2;
}

// Усечение данных до char (сдвиг на 6 бит).
unsigned char* shiftLine6bits(unsigned int *data, int n)
{
	unsigned char *cdata = NULL;

	// Обработка сырых данных.
	cdata = new unsigned char [n];
    for(int i = 0; i < n; i++)
		// Усечение данных до размера 8 бит (на 2 бита уже сместились ранее).
		cdata[i] = static_cast<unsigned char>(data[i] >> 6);
     
	return cdata;
}