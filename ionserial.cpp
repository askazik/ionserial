// ===========================================================================
// Программа многочастотного съёма данных с ионозонда.
// ===========================================================================
#include "ionserial.h"

int main(void){
	setlocale(LC_ALL,"Russian"); // настройка локали на вывод сообщений по-русски
	SetPriorityClass(); // назначаем приоритет процесса

    // ===========================================================================================
    // 1. Читаем файл конфигурации.
    // ===========================================================================================
    parusConfig conf; // используем имя файла конфигурации по умолчанию
    std::cout << "Используем конфигурационный файл: <" << conf.getFileName() << ">." << std::endl;
	std::cout << "Частоты зондирования:" << std::endl;
	for(int i=0; i < conf.getModulesCount(); i++)
		std::cout << i << ": " << conf.getFreq(i) << " кГц" << std::endl;
	std::cout << "Измерения начаты." << std::endl;
	std::cout << "Если не слышно жужжания - произошёл сбой. Можно закрывать это окно." << std::endl;

    // ===========================================================================================
    // 2. Конфигурирование и запуск сеанса.
    // ===========================================================================================
	int RetStatus = 0;
	try	{
		// Подготовка аппаратуры к зондированию.
		// Открытие выходнго файла данных и запись заголовка.
		parusWork *work = new parusWork(conf);

		DWORD msTimeout = 4;
		int curFrqNum = 0; // номер текущей частоты
		unsigned short curFrq; // текущая частота зондирования, кГц
		int counter = 32000; // максимальное число импульсов от генератора

		work->startGenerator(counter); // Запуск генератора импульсов.
		while(counter){ // обрабатываем импульсы генератора
			curFrq = conf.getFreq(curFrqNum); // получим текущую частоту зондирования
			work->adjustSounding(curFrq); // настроим синтезатор
			work->ASYNC_TRANSFER(); // запустим АЦП

			// Цикл проверки до появления результатов в буфере.
			// READ_BUFISCOMPLETE - сбоит на частоте 47 Гц
			while(work->READ_ISCOMPLETE(msTimeout) == NULL);

			// Остановим АЦП
			work->READ_ABORTIO();					

			// Запишем данные в файл.
			switch(conf.getVersion()){
				case 0: // выборка только двух отражений из измеренных данных
					work->fillUnits(); // копирование данных из аппаратного буфера в локальные хранилища для последующей обработки
					work->saveDirtyLine();
					break;
				case 1: // сохранение всех данных без обработки - для калибровки
					work->saveFullData();
					break;
				case 2: // упаковка данных - существенное уменьшение размера файла
					work->fillUnits(); // копирование данных из аппаратного буфера в локальные хранилища для последующей обработки
					work->saveTheresholdLine();
					break;
			}

			counter--; // приступаем к обработке следующего импульса
			curFrqNum = (curFrqNum == conf.getModulesCount()-1) ? 0 : curFrqNum++; // переключаем частоту
		}
		delete work;
	}
	catch(std::exception &e)
	{
		std::cerr << std::endl;
		std::cerr << "Сообщение: " << e.what() << std::endl;
		std::cerr << "Тип      : " << typeid(e).name() << std::endl;
		RetStatus = -1;
	}
	Beep( 1500, 300 );

	return RetStatus;
}

void SetPriorityClass(void){
	HANDLE procHandle = GetCurrentProcess();
	DWORD priorityClass = GetPriorityClass(procHandle);

	if (!SetPriorityClass(procHandle, HIGH_PRIORITY_CLASS))
		std::cerr << "SetPriorityClass" << std::endl;

	priorityClass = GetPriorityClass(procHandle);
	std::cerr << "Priority Class is set to : ";
	switch(priorityClass)
	{
	case HIGH_PRIORITY_CLASS:
		std::cerr << "HIGH_PRIORITY_CLASS\r\n";
		break;
	case IDLE_PRIORITY_CLASS:
		std::cerr << "IDLE_PRIORITY_CLASS\r\n";
		break;
	case NORMAL_PRIORITY_CLASS:
		std::cerr << "NORMAL_PRIORITY_CLASS\r\n";
		break;
	case REALTIME_PRIORITY_CLASS:
		std::cerr << "REALTIME_PRIORITY_CLASS\r\n";
		break;
	default:
		std::cerr <<"Unknown priority class\r\n";
	}
}