// ===========================================================================
// �������� � ���������������� ������ ������������.
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
	std::cout << "�������� ���������� ����������������� �����." << std::endl;
	if(_modules_count) delete [] _ptModules;
}

// �������� �� ���� ���������������� ������ ��� ����������.
bool parusConfig::isValidConf(std::string fullName){
	bool key = false;

	// ������� ���� ��� ������.
	std::ifstream fin(fullName.c_str());
    if(!fin)
        throw std::runtime_error("Error: �� ���� ������� ���������������� ���� " + fullName);

    std::string line;
	while(getline(fin, line)){
		// trim from end of string (right)
		line.erase(line.find_last_not_of(_whitespaces) + 1);
		if(line[0] != '#' && line.size()) // ������� ������������ � ������ �����
		{
			std::size_t pos = line.find("SERIAL");
			if (pos != std::string::npos && pos == 0){ // ��� ������
				key = true;
				break; // ����������� ���� ����� ���������� ������ �������� ������
			}
		}			
	}
    fin.close();
    
	return key;
}

// ��������� ���������������� ���� �� ������� ����.
void parusConfig::loadConf(std::string fullName){
	int i = 0; // ������� ��������
	bool key = false;

	if(isValidConf(fullName)){
		// ������� ���� ��� ������.
		std::ifstream fin(fullName.c_str());
		if(!fin)
			throw std::runtime_error("Error: �� ���� ������� ���������������� ���� " + fullName);

		std::string line;
		while(!key) {
			getline(fin, line);
			// trim from end of string (right)
			line.erase(line.find_last_not_of(_whitespaces) + 1);
			if(line[0] != '#' && line.size()) { // ������� ������������ � ������ �����
				i++;
				switch(i) {									
				case 1: // ���������� ��� ��������������
					break;
				case 2: // ������ �������
					_ver = getValueFromString(line);
					break;
				case 3: // ��� �� ������, �
					_height_step = getValueFromString(line);
					break;
				case 4: // ���������� �������� ������, �� ����� 4096
					_height_count = getValueFromString(line);
					break;
				case 5: // ��������� ������������ �� ������ �������
					_pulse_count = getValueFromString(line);
					break;
				case 6: // ���������� (����������) 1/0 = ���/����
					_attenuation = getValueFromString(line);
					break;
				case 7: // �������� (g = value/6, 6�� = ���������� � 4 ���� �� ��������)
					_gain = getValueFromString(line);
					break;
				case 8: // ������� ����������� ���������, ��
					_pulse_frq = getValueFromString(line);
					break;
				case 9: // ������������ ����������� ���������, ���
					_pulse_duration = getValueFromString(line);
					break;
				case 10: // ��������� ������, �� (��, ��� ���� ��� ��������� �������������)
					_height_min = getValueFromString(line);
					break;
				case 11: // �������� ������, �� (��, ��� ���� ��� ��������� �������������)
					_height_max = getValueFromString(line);
					key = true; // ����������� ������������
					break;
				}
			}                        
		}
		getModules(fin); // ������ � ������������ �������
		fin.close();
	}
	else
		throw std::runtime_error("Error: ��� �� ���������������� ���� ��� ���������." + fullName);
}

// ���������� ���������� ������� ������������. 
void parusConfig::getModulesCount(std::ifstream &fin){
	_modules_count = 0;
	std::string line;
	while(getline(fin, line)) {          
		// trim from end of string (right)
		line.erase(line.find_last_not_of(_whitespaces) + 1);
		if(line.size() && line[0] != '#') { // ������� ������������ � ������ �����
			std::size_t pos = line.find("%%%");
			if (pos != std::string::npos && pos == 0) // ��� ������
				_modules_count++;                            
		}
	}
	fin.clear();
	fin.seekg(0, fin.beg);
}

// ��������� ������ ������������.    
void parusConfig::getModules(std::ifstream &fin){
	getModulesCount(fin);
	_ptModules = new myModule [_modules_count];

	// ��������� ��������� � �������.
	int i = 0;
	std::string line;
	while(getline(fin, line)) {
		// trim from end of string (right)
		line.erase(line.find_last_not_of(_whitespaces) + 1);
		if(line.size() && line[0] != '#') { // ������� ������������ � ������ �����
			std::size_t pos = line.find("%%%");
			if (pos != std::string::npos && pos == 0) { // ��� ������
				_ptModules[i] = getCurrentModule(fin);
				i++;
			}
		}
	}    

}

// ��������� ������� ������ ������������.    
myModule parusConfig::getCurrentModule(std::ifstream &fin){
	bool key = false;
	myModule out;
	int i = 0;

	std::string line;
	while(!key) {
		getline(fin, line);
		// trim from end of string (right)
		line.erase(line.find_last_not_of(_whitespaces) + 1);
		if(line.size() && line[0] != '#' ) { // ������� ������������ � ������ �����
			i++;
			switch(i) {									
			case 1: // ������� ������, ���
				out.frq = getValueFromString(line);
				key = true;
				break;
			//case 2: // �������� ������� ������, ���
			//	out.fend = getValueFromString(line);
			//	break;
			//case 3: // ��� �� ������� ����������, ���
			//	out.fstep = getValueFromString(line);
			//	break;
			//case 4: // ��������� ������������ �� ������ �������
			//	out.pulse_count = getValueFromString(line);
			//	break;
			//case 5: // ���������� (����������) 1/0 = ���/����
			//	out.attenuation = getValueFromString(line);
			//	break;
			//case 6: // �������� (g = value/6, 6�� = ���������� � 4 ���� �� ��������)
			//	out.gain = getValueFromString(line);
			//	break;
			//case 7: // ������� ����������� ���������, ��
			//	out.pulse_frq = getValueFromString(line);
			//	break;
			//case 8: // ������������ ����������� ���������, ���
			//	out.pulse_duration = getValueFromString(line);
			//	key = true;
			//	break;
			}
		}					
	}    

	return out;
}

// ����������� ���������� ���������� ������ ������������ �� ���� �������.
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

// ��������� ������ �������� �� ������.
unsigned parusConfig::getValueFromString(std::string line){
	return std::stoi(line,nullptr);
}