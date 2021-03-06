// ===========================================================================
// ������������ ���� ��� ������ � ����������������� �������.
// ===========================================================================
#ifndef __CONFIG_H__
#define __CONFIG_H__

#define CONFIG_DEFAULT_FILE_NAME "default.conf"

#include <stdexcept>
#include <string>
#include <iostream>
#include <fstream>

// ��������� ������������ ������. ������ 0.
struct myModule {  
    unsigned frq;   // ������� ������, ���
};

// ����� ������� � ����������������� ����� ������������.
class parusConfig {
    std::string _fullFileName;
	std::string _whitespaces;    
    
    unsigned _ver; // ����� ������
    unsigned _height_step; // ��� �� ������, �
    unsigned _height_count; // ���������� �����
    unsigned _modules_count; // ���������� �������/������ ������������
    unsigned _pulse_count; // ��������� ������������ �� ������ �������
    unsigned _attenuation; // ���������� (����������) 1/0 = ���/����
    unsigned _gain;	// �������� (g = value/6, 6�� = ���������� � 4 ���� �� ��������)
    unsigned _pulse_frq; // ������� ����������� ���������, ��
    unsigned _pulse_duration; // ������������ ����������� ���������, ���
	unsigned _height_min; // ��������� ������, �� (��, ��� ���� ��� ��������� �������������)
	unsigned _height_max; // �������� ������, �� (��, ��� ���� ��� ��������� �������������)

    myModule	*_ptModules; // ��������� �� ������ �������

	bool isValidConf(std::string fullName = std::string(CONFIG_DEFAULT_FILE_NAME));
	unsigned getValueFromString(std::string line);
    void getModulesCount(std::ifstream &fin);
	void getModules(std::ifstream &fin);
    myModule getCurrentModule(std::ifstream &fin);
public:
    parusConfig(void);
	~parusConfig(void);

	void loadConf(std::string fullName);
    std::string getFileName(void){return _fullFileName;}
	int getModulesCount(void){return _modules_count;}
	unsigned getHeightStep(void){return _height_step;}
    void setHeightStep(double value){_height_step = static_cast<unsigned>(value);}
	unsigned getHeightCount(void){return _height_count;}
	unsigned getVersion(void){return _ver;}
    unsigned getFreq(int num){return _ptModules[num].frq;}

	unsigned getAttenuation(void){return _attenuation;} // ���������� (����������) 1/0 = ���/����
	unsigned getGain(void){return _gain;}	// �������� (g = value/6, 6�� = ���������� � 4 ���� �� ��������)
	unsigned getPulse_frq(void){return _pulse_frq;} // ������� ����������� ���������, ��
	unsigned getPulse_duration(void){return _pulse_duration;} // ������������ ����������� ���������, ���

	unsigned getHeightMin(void){return _height_min;} // ��������� ������, �� (��, ��� ���� ��� ��������� �������������)
	unsigned getHeightMax(void){return _height_max;} // �������� ������, �� (��, ��� ���� ��� ��������� �������������)
};

#endif // __CONFIG_H__
