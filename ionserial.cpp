// ===========================================================================
// ��������� ��������������� ����� ������ � ���������.
// ===========================================================================
#include "ionserial.h"

int main(void){
	setlocale(LC_ALL,"Russian"); // ��������� ������ �� ����� ��������� ��-������
	SetPriorityClass(); // ��������� ��������� ��������

    // ===========================================================================================
    // 1. ������ ���� ������������.
    // ===========================================================================================
    parusConfig conf; // ���������� ��� ����� ������������ �� ���������
    std::cout << "���������� ���������������� ����: <" << conf.getFileName() << ">." << std::endl;
	std::cout << "������� ������������:" << std::endl;
	for(int i=0; i < conf.getModulesCount(); i++)
		std::cout << i << ": " << conf.getFreq(i) << " ���" << std::endl;
	std::cout << "��������� ������." << std::endl;
	std::cout << "���� �� ������ �������� - ��������� ����. ����� ��������� ��� ����." << std::endl;

    // ===========================================================================================
    // 2. ���������������� � ������ ������.
    // ===========================================================================================
	int RetStatus = 0;
	try	{
		// ���������� ���������� � ������������.
		// �������� �������� ����� ������ � ������ ���������.
		parusWork *work = new parusWork(conf);

		DWORD msTimeout = 4;
		int curFrqNum = 0; // ����� ������� �������
		unsigned short curFrq; // ������� ������� ������������, ���
		int counter = 32000; // ������������ ����� ��������� �� ����������

		work->startGenerator(counter); // ������ ���������� ���������.
		while(counter){ // ������������ �������� ����������
			curFrq = conf.getFreq(curFrqNum); // ������� ������� ������� ������������
			work->adjustSounding(curFrq); // �������� ����������
			work->ASYNC_TRANSFER(); // �������� ���

			// ���� �������� �� ��������� ����������� � ������.
			// READ_BUFISCOMPLETE - ����� �� ������� 47 ��
			while(work->READ_ISCOMPLETE(msTimeout) == NULL);

			// ��������� ���
			work->READ_ABORTIO();					

			// ������� ������ � ����.
			switch(conf.getVersion()){
				case 0: // ������� ������ ���� ��������� �� ���������� ������
					work->fillUnits(); // ����������� ������ �� ����������� ������ � ��������� ��������� ��� ����������� ���������
					work->saveDirtyLine();
					break;
				case 1: // ���������� ���� ������ ��� ��������� - ��� ����������
					work->saveFullData();
					break;
				case 2: // �������� ������ - ������������ ���������� ������� �����
					work->fillUnits(); // ����������� ������ �� ����������� ������ � ��������� ��������� ��� ����������� ���������
					work->saveTheresholdLine();
					break;
			}

			counter--; // ���������� � ��������� ���������� ��������
			curFrqNum = (curFrqNum == conf.getModulesCount()-1) ? 0 : curFrqNum++; // ����������� �������
		}
		delete work;
	}
	catch(std::exception &e)
	{
		std::cerr << std::endl;
		std::cerr << "���������: " << e.what() << std::endl;
		std::cerr << "���      : " << typeid(e).name() << std::endl;
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