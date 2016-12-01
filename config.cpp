// ===========================================================================
// Операции с конфигурационным файлом зондирования.
// ===========================================================================
#include "config.h"

parusConfig::parusConfig(void) :
	_fullFileName (std::string(CONFIG_DEFAULT_FILE_NAME)),
	_whitespaces (std::string(" \t\f\v\n\r")),
    _modules_count (0),
    _ptModules (nullptr)
{
	loadConf(_fullFileName);
}
     
parusConfig::~parusConfig(void){
	std::cout << "Выполнен деструктор конфигурационного файла." << std::endl;
	if(_modules_count) delete [] _ptModules;
}

// Является ли файл конфигурационным файлом для ионограммы.
bool parusConfig::isValidConf(std::string fullName){
	bool key = false;

	// Откроем файл для чтения.
	std::ifstream fin(fullName.c_str());
    if(!fin)
        throw std::runtime_error("Error: Не могу открыть конфигурационный файл " + fullName);

    std::string line;
	while(getline(fin, line)){
		// trim from end of string (right)
		line.erase(line.find_last_not_of(_whitespaces) + 1);
		if(line[0] != '#' && line.size()) // пропуск комментариев и пустых строк
		{
			std::size_t pos = line.find("SERIAL");
			if (pos != std::string::npos && pos == 0){ // тег найден
				key = true;
				break; // заканчиваем цикл после нахождения первой значащей строки
			}
		}			
	}
    fin.close();
    
	return key;
}

// Загружаем конфигурационный файл по полному пути.
void parusConfig::loadConf(std::string fullName){
	int i = 0; // счетчик значений
	bool key = false;

	if(isValidConf(fullName)){
		// Откроем файл для чтения.
		std::ifstream fin(fullName.c_str());
		if(!fin)
			throw std::runtime_error("Error: Не могу открыть конфигурационный файл " + fullName);

		std::string line;
		while(!key) {
			getline(fin, line);
			// trim from end of string (right)
			line.erase(line.find_last_not_of(_whitespaces) + 1);
			if(line[0] != '#' && line.size()) { // пропуск комментариев и пустых строк
				i++;
				switch(i) {									
				case 1: // пропускаем тег принадлежности
					break;
				case 2: // версия формата
					_ver = getValueFromString(line);
					break;
				case 3: // шаг по высоте, м
					_height_step = getValueFromString(line);
					break;
				case 4: // количество отсчётов высоты, не более 4096
					_height_count = getValueFromString(line);
					break;
				case 5: // импульсов зондирования на каждой частоте
					_pulse_count = getValueFromString(line);
					break;
				case 6: // ослабление (аттенюатор) 1/0 = вкл/выкл
					_attenuation = getValueFromString(line);
					break;
				case 7: // усиление (g = value/6, 6дБ = приращение в 4 раза по мощности)
					_gain = getValueFromString(line);
					break;
				case 8: // частота зондирующих импульсов, Гц
					_pulse_frq = getValueFromString(line);
					break;
				case 9: // длительность зондирующих импульсов, мкс
					_pulse_duration = getValueFromString(line);
					break;
				case 10: // начальная высота, км (всё, что ниже при обработке отбрасывается)
					_height_min = getValueFromString(line);
					break;
				case 11: // конечная высота, км (всё, что выше при обработке отбрасывается)
					_height_max = getValueFromString(line);
					key = true; // заканчиваем сканирование
					break;
				}
			}                        
		}
		getModules(fin); // войдем в сканирование модулей
		fin.close();
	}
	else
		throw std::runtime_error("Error: Это не конфигурационный файл для ионограмм." + fullName);
}

// Определяет количество модулей зондирования. 
void parusConfig::getModulesCount(std::ifstream &fin){
	_modules_count = 0;
	std::string line;
	while(getline(fin, line)) {          
		// trim from end of string (right)
		line.erase(line.find_last_not_of(_whitespaces) + 1);
		if(line.size() && line[0] != '#') { // пропуск комментариев и пустых строк
			std::size_t pos = line.find("%%%");
			if (pos != std::string::npos && pos == 0) // тег найден
				_modules_count++;                            
		}
	}
	fin.clear();
	fin.seekg(0, fin.beg);
}

// Извлекает модули зондирования.    
void parusConfig::getModules(std::ifstream &fin){
	getModulesCount(fin);
	_ptModules = new myModule [_modules_count];

	// Считываем параметры в модулях.
	int i = 0;
	std::string line;
	while(getline(fin, line)) {
		// trim from end of string (right)
		line.erase(line.find_last_not_of(_whitespaces) + 1);
		if(line.size() && line[0] != '#') { // пропуск комментариев и пустых строк
			std::size_t pos = line.find("%%%");
			if (pos != std::string::npos && pos == 0) { // тег найден
				_ptModules[i] = getCurrentModule(fin);
				i++;
			}
		}
	}    

}

// Извлекает текущий модуль зондирования.    
myModule parusConfig::getCurrentModule(std::ifstream &fin){
	bool key = false;
	myModule out;
	int i = 0;

	std::string line;
	while(!key) {
		getline(fin, line);
		// trim from end of string (right)
		line.erase(line.find_last_not_of(_whitespaces) + 1);
		if(line.size() && line[0] != '#' ) { // пропуск комментариев и пустых строк
			i++;
			switch(i) {									
			case 1: // частота модуля, кГц
				out.frq = getValueFromString(line);
				key = true;
				break;
			//case 2: // конечная частота модуля, кГц
			//	out.fend = getValueFromString(line);
			//	break;
			//case 3: // шаг по частоте ионограммы, кГц
			//	out.fstep = getValueFromString(line);
			//	break;
			//case 4: // импульсов зондирования на каждой частоте
			//	out.pulse_count = getValueFromString(line);
			//	break;
			//case 5: // ослабление (аттенюатор) 1/0 = вкл/выкл
			//	out.attenuation = getValueFromString(line);
			//	break;
			//case 6: // усиление (g = value/6, 6дБ = приращение в 4 раза по мощности)
			//	out.gain = getValueFromString(line);
			//	break;
			//case 7: // частота зондирующих импульсов, Гц
			//	out.pulse_frq = getValueFromString(line);
			//	break;
			//case 8: // длительность зондирующих импульсов, мкс
			//	out.pulse_duration = getValueFromString(line);
			//	key = true;
			//	break;
			}
		}					
	}    

	return out;
}

// Определение суммарного количества частот зондирования по всем модулям.
//unsigned parusConfig::getCount_freq(void){
//    unsigned count = 0;
//    for(unsigned i = 0; i < modules_count; i++)
//        {
//            unsigned fcount = static_cast<unsigned>((ptModulesArray[i].fend - ptModulesArray[i].fbeg)/ptModulesArray[i].fstep + 1);
//            count += fcount;
//        }
//
//    return count;
//}

// Получение целого значения из строки.
unsigned parusConfig::getValueFromString(std::string line){
	return std::stoi(line,nullptr);
}