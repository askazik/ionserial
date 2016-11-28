// ===========================================================================
// Заголовочный файл для работы с конфигурационными файлами.
// ===========================================================================
#ifndef __CONFIG_H__
#define __CONFIG_H__

#define CONFIG_DEFAULT_FILE_NAME "default.conf"

#include <stdexcept>
#include <string>
#include <iostream>
#include <fstream>

// Параметры зондирования модуля. Версия 0.
struct myModule {  
    unsigned frq;   // частота модуля, кГц
};

// Класс доступа к конфигурационному файлу зондирования.
class parusConfig {
    std::string _fullFileName;
	std::string _whitespaces;    
    
    unsigned _ver; // номер версии
    unsigned _height_step; // шаг по высоте, м
    unsigned _height_count; // количество высот
    unsigned _modules_count; // количество модулей/частот зондирования
    unsigned _pulse_count; // импульсов зондирования на каждой частоте
    unsigned _attenuation; // ослабление (аттенюатор) 1/0 = вкл/выкл
    unsigned _gain;	// усиление (g = value/6, 6дБ = приращение в 4 раза по мощности)
    unsigned _pulse_frq; // частота зондирующих импульсов, Гц
    unsigned _pulse_duration; // длительность зондирующих импульсов, мкс
	unsigned _height_min; // начальная высота, км (всё, что ниже при обработке отбрасывается)
	unsigned _height_max; // конечная высота, км (всё, что выше при обработке отбрасывается)

    myModule	*_ptModules; // указатель на массив модулей

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

	unsigned getAttenuation(void){return _attenuation;} // ослабление (аттенюатор) 1/0 = вкл/выкл
	unsigned getGain(void){return _gain;}	// усиление (g = value/6, 6дБ = приращение в 4 раза по мощности)
	unsigned getPulse_frq(void){return _pulse_frq;} // частота зондирующих импульсов, Гц
	unsigned getPulse_duration(void){return _pulse_duration;} // длительность зондирующих импульсов, мкс

	unsigned getHeightMin(void){return _height_min;} // начальная высота, км (всё, что ниже при обработке отбрасывается)
	unsigned getHeightMax(void){return _height_max;} // конечная высота, км (всё, что выше при обработке отбрасывается)
};

#endif // __CONFIG_H__
