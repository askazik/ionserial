// ===========================================================================
// ������������ ���� ��� ��������� ��������������� ����� ������ � ���������.
// ===========================================================================
#ifndef __IONSERIAL_H__
#define __IONSERIAL_H__

#include <windows.h>
#include <stdexcept>
#include <clocale>
#include <iostream>
#include <fstream>
#include <climits>

// ��������� � ������ ���������� ������� daqdrv.dll
#include "daqdef.h"
#pragma comment(lib, "daqdrv.lib") // for Microsoft Visual C++
#include "m14x3mDf.h"

// ���������� ������������ �����
#include "config.h"
#include "parus.h"

void SetPriorityClass(void);

#endif // __IONSERIAL_H__