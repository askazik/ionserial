// ===========================================================================
// Заголовочный файл для программы многочастотного съёма данных с ионозонда.
// ===========================================================================
#ifndef __IONSERIAL_H__
#define __IONSERIAL_H__

#include <windows.h>
#include <stdexcept>
#include <clocale>
#include <iostream>
#include <fstream>
#include <climits>

// Включение в проект библиотеки импорта daqdrv.dll
#include "daqdef.h"
#pragma comment(lib, "daqdrv.lib") // for Microsoft Visual C++
#include "m14x3mDf.h"

// Внутренние заголовочные файлы
#include "config.h"
#include "parus.h"

void SetPriorityClass(void);

#endif // __IONSERIAL_H__