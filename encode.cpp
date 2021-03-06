// ===========================================================================
// ����������� ������ �������� �� ��������� ���.
// ===========================================================================
#include "encode.h"

// �������� ������ ���������� �� ��������� �������-���.
// data - ����� ������
// _Count - ����� �������� ���
// _HeightStep - ��� �� ������, �
// _Frq - ������� ������������, ���
// hFile - ���������� ��������� �����
void saveLine(BYTE *data, unsigned size, unsigned _HeightStep, unsigned short _Frq, HANDLE hFile)
{
    // ������� ����� ��� ����������� ������. �������, ��� ����������� ������ �� ������ ��������.
	unsigned n = size;
	unsigned char *tmpArray = new unsigned char [n];
    unsigned char *tmpLine = new unsigned char [n];
	unsigned char *tmpAmplitude = new unsigned char [n];
    
    unsigned j;
    unsigned char *dataLine = data;
    unsigned char thereshold;
    SignalResponse tmpSignalResponse;
	unsigned char countOutliers;
	unsigned short countFrq = 0; // ���������� ���� � ����������� ��������� �������

	// ��������� ������� ������� ��������.
	FrequencyData tmpFrequencyData;
	thereshold = getThereshold(dataLine, n);            
	tmpFrequencyData.frequency = _Frq;
	tmpFrequencyData.threshold_o = thereshold;
	tmpFrequencyData.threshold_x = 0;
	tmpFrequencyData.count_o = 0; // ����� ����������
	tmpFrequencyData.count_x = 0; // ������� � ��� ���� �- � �- ���������� ����������.

	unsigned short countLine = 0; // ���������� ���� � ����������� ������
	countOutliers = 0; // ������� ���������� �������� � ������� ������
	j = 0;
	while(j < n) // ������� �������
	{
		if(dataLine[j] > thereshold) // ������ ������ - ������������ ���.
		{
			tmpSignalResponse.height_begin = static_cast<unsigned long>(floor(1.0 * j * _HeightStep));
			tmpSignalResponse.count_samples = 1;
			tmpAmplitude[tmpSignalResponse.count_samples-1] = dataLine[j];
			j++; // ������� � ���������� ��������
			while(dataLine[j] > thereshold && j < n)
			{
				tmpSignalResponse.count_samples++;
				tmpAmplitude[tmpSignalResponse.count_samples-1] = dataLine[j];
				j++; // ������� � ���������� ��������
			}
			countOutliers++; // ��������� ���������� �������� � ������

			// �������� ����������� �������
			// ��������� �������.
			unsigned short nn = sizeof(SignalResponse);
			memcpy(tmpLine+countLine, &tmpSignalResponse, nn);
			countLine += nn;
			// ������ �������.
			nn = tmpSignalResponse.count_samples*sizeof(unsigned char);
			memcpy(tmpLine+countLine, tmpAmplitude, nn);
			countLine += nn;
		}
		else
			j++; // ���� ��������� ������
	}

	// ������ ��� ������� ������� �������� � �����.
	// ��������� ������ ������� ������, ���� ���� ������� �����������.
	tmpFrequencyData.count_o = countOutliers;
	// ��������� �������.
	unsigned nFrequencyData = sizeof(FrequencyData);
	memcpy(tmpArray, &tmpFrequencyData, nFrequencyData);
	// ��������� ����� �������������� ������� ��������
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
		throw std::runtime_error("�� ���� ��������� ������ ���������� � ����.");
	
    delete [] tmpLine;
	delete [] tmpAmplitude;
	delete [] tmpArray;
} 

// ��������� �����
int comp(const void *i, const void *j)
{
    return *(unsigned char *)i - *(unsigned char *)j;
}

// ���������� ������� ����� �������� ����������� ������.
unsigned char getThereshold(unsigned char *arr, unsigned n)
{
    unsigned char *sortData = new unsigned char [n]; // �������� ������ ��� ����������
    unsigned char Q1, Q3, dQ;
    unsigned short maxLim = 0;

    // 1. ���������� ������ �� �����������.
    memcpy(sortData, arr, n);
    qsort(sortData, n, sizeof(unsigned char), comp);
    // 2. �������� (n - ������, ������� ������!!!)
    Q1 = min(sortData[n/4],sortData[n/4-1]);
    Q3 = max(sortData[3*n/4],sortData[3*n/4-1]);
    // 3. �������������� ��������
    dQ = Q3 - Q1;
    // 4. ������� ������� ��������.
    maxLim = Q3 + 3 * dQ;    

    delete [] sortData;

    return (maxLim>=255)? 255 : static_cast<unsigned char>(maxLim);
}

// �������������� "�����" ������������ ������ � ������ �������� �������� � ��������� ������.
unsigned char* loadLine(unsigned long *data, int n)
{
	unsigned char *cdata = NULL;

	// ��������� ����� ������.
	cdata = new unsigned char [n];
	int re, im, abstmp;
    for(int i = 0; i < n; i++)
		{
            // ���������� ������������� ������������� ����� ��������� ���������
            union {
                unsigned long word;     // 4-������� ����� �������������� ���
                adcTwoChannels twoCh;  // ������������� (������������) ������
            };
            
			// ��������� �� ����������. 
			// ������� ������ ������� 14 ���. ������� 2 ��� - ��������������� �������.
            word = data[i];
            re = static_cast<int>(twoCh.re.value) >> 2;
            im = static_cast<int>(twoCh.im.value) >> 2;

            // ��������� ������������ ���������� � ���� ���������.
			abstmp = static_cast<int>(floor(sqrt(re*re + im*im*1.0)));

			// �������� ������ �� ������� 8 ��� (�� 2 ���� ��� ���������� �����).
			cdata[i] = static_cast<unsigned char>(abstmp >> 6);
		}            
    
    return cdata;
}

// �������������� "�����" ������������ ������ � ������ �������� �������� ��� �������� ������.
unsigned int* loadIonogramLine(unsigned long *data, int n)
{
	unsigned int *idata = NULL;

	// ��������� ����� ������.
	idata = new unsigned int [n];
	int re, im, abstmp;
    for(int i = 0; i < n; i++)
		{
            // ���������� ������������� ������������� ����� ��������� ���������
            union {
                unsigned long word;     // 4-������� ����� �������������� ���
                adcTwoChannels twoCh;  // ������������� (������������) ������
            };
            
			// ��������� �� ����������. 
			// ������� ������ ������� 14 ���. ������� 2 ��� - ��������������� �������.
            word = data[i];
            re = static_cast<int>(twoCh.re.value) >> 2;
            im = static_cast<int>(twoCh.im.value) >> 2;

            // ��������� ������������ ���������� � ���� ���������.
			abstmp = static_cast<int>(floor(sqrt(re*re + im*im*1.0)));
		}            
    
    return idata;
}

// ������������ ���������������� ����� ����������.
void sumIonogramLine(unsigned int *dataDestination, unsigned int *dataSource, int n)
{
    for(int i = 0; i < n; i++)
		dataDestination[i] = dataDestination[i]/2 + dataSource[i]/2;
}

// �������� ������ �� char (����� �� 6 ���).
unsigned char* shiftLine6bits(unsigned int *data, int n)
{
	unsigned char *cdata = NULL;

	// ��������� ����� ������.
	cdata = new unsigned char [n];
    for(int i = 0; i < n; i++)
		// �������� ������ �� ������� 8 ��� (�� 2 ���� ��� ���������� �����).
		cdata[i] = static_cast<unsigned char>(data[i] >> 6);
     
	return cdata;
}