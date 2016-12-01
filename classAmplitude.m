classdef classAmplitude < memmapfile
    %classAmplitude - доступ к большому файлу данных амплитуд.
    %   Detailed explanation goes here
    
    properties (GetAccess = private, SetAccess = private)
        fileProperties
    end
        
    properties (GetAccess = public, SetAccess = private)
        version
        frqs
        heights
        date
        dt
    end
    properties
        position
    end
    
    methods
        %% Class constructor
        function this = classAmplitude(fileName, varargin)
            % Конструктор класса доступа к файлу измерений.
            
            % Verify input arguments
            p = inputParser;
            isfrq = @(str) exist('str','var') & ischar(str) & strcmp(str(end-3:end),'.frq'); 
            p.addRequired('fileName', isfrq);
            p.parse(fileName, varargin{:});
            
            % Object Initialization
            % Call superclass constructor before accessing object
            % You cannot conditionalize this statement
            this = this@memmapfile(p.Results.fileName);
         
            % Post Initialization
            % Any code, including access to object
            this.fileProperties = this.getHeader();
            this.version = this.fileProperties.ver;
            this.frqs = this.fileProperties.frqs;
            this.heights = this.getRealHeights();
            this.date = datenum(this.fileProperties.year + 1900, ...
                                this.fileProperties.mon, ...
                                this.fileProperties.mday, ...
                                this.fileProperties.hour, ...
                                this.fileProperties.min, ...
                                this.fileProperties.sec);
            this.dt = 1. / this.fileProperties.pulse_frq;
            
            % Параметры обращения к данным.
            this.offset = this.fileProperties.data_begin_position;
        end
        
        %% Disp overload
        function disp(obj)
            disp@memmapfile(obj);

            str{1}=sprintf('Version: (%d)', obj.version);
            str{2}=sprintf('Frequensies, kHz: (%s)', sprintf('%d, ',obj.frqs));
            str{3}=sprintf('Heights, km: (%d : %d : %d)', obj.heights(1), obj.heights(2)-obj.heights(1), obj.heights(end));
            str{4}=sprintf('Date: (%s) UT', datestr(obj.date));
            str{5}=sprintf('dt: (%.3f), s', obj.dt);
            str = sprintf('%s\n',str{:});
            fprintf(str)
        end

        %% Get version of the data file.
        function value = get.version(this)
            % Получаем версию файла данных.
            value = this.version;
        end
        
        %% Get frequensies of the data file.
        function value = get.frqs(this)
            % Получаем частоты зондирования.
            value = this.frqs;
        end
        
        %% Get heights of the data file.
        function value = get.heights(this)
            % Получаем высоты зондирования.
            value = this.heights;
        end
        
        %% Get date of the data file.
        function value = get.date(this)
            % Получаем дату зондирования в числовом формате.
            value = this.date;
        end
        
        %% Get time step of the data file.
        function value = get.dt(this)
            % Получаем время между импульсами зондирования.
            value = this.dt;
        end
    end
    
    methods (Access = private, Sealed)
        %% Get real heights file version.
        function heights = getRealHeights(this)
            % Определение действующих высот отражения согласно версии файла
            % данных.
            
            heights = this.fileProperties.height_min : ...
                        this.fileProperties.height_step / 1000. : ...
                        this.fileProperties.height_max;
            switch this.version
                case 0
                    % интервал высот
                case 1
                    % все возможные высоты
                    heights = (0 : ...
                        this.fileProperties.height_step : ...
                        (this.fileProperties.count_height - 1) * this.fileProperties.height_step) / 1000.;
                case 2
                    % интервал высот
                otherwise
                    % интервал высот
            end
            
        end
        
        %% Read the file header.
        function properties = getHeader(this)
            % Чтение заголовка файла данных.
            %
            % struct dataHeader { 	    // === Заголовок файла данных ===
            %   unsigned ver; // номер версии
            %   struct tm time_sound; // GMT время получения зондирования
            %   unsigned height_min; // начальная высота, км (всё, что ниже при обработке отбрасывается)
            % 	unsigned height_max; // конечная высота, км (всё, что выше при обработке отбрасывается)
            %   unsigned height_step; // шаг по высоте, м (реальный шаг, вычисленный по частоте АЦП)
            %   unsigned count_height; // число высот (размер исходного буфера АЦП при зондировании, fifo нашего АЦП 4Кб. Т.е. не больше 1024 отсчётов для двух квадратурных каналов)
            %   unsigned count_modules; // количество модулей/частот зондирования
            % 	unsigned pulse_frq; // частота зондирующих импульсов, Гц
            % };
            %
            % struct tm
            % Member	Type	Meaning	Range
            % tm_sec	int	seconds after the minute	0-60*
            % tm_min	int	minutes after the hour	0-59
            % tm_hour	int	hours since midnight	0-23
            % tm_mday	int	day of the month	1-31
            % tm_mon	int	months since January	0-11
            % tm_year	int	years since 1900
            % tm_wday	int	days since Sunday	0-6
            % tm_yday	int	days since January 1	0-365
            % tm_isdst	int	Daylight Saving Time flag
            % =========================================================================
            
            % 1. Открываем файл на чтение.
            [fid, message] = fopen(this.filename, 'r');
            if ( fid == -1)
                error(message);
            end
            
            % 2. Извлекаем параметры эксперимента.
            properties.ver = fread(fid,1,'uint32');
            properties.sec = fread(fid,1,'int32');
            properties.min = fread(fid,1,'int32');
            properties.hour = fread(fid,1,'int32');
            properties.mday = fread(fid,1,'int32');
            properties.mon = fread(fid,1,'int32');
            properties.year = fread(fid,1,'int32');
            properties.wday = fread(fid,1,'int32');
            properties.yday = fread(fid,1,'int32');
            properties.isdst = fread(fid,1,'int32');
            properties.height_min = fread(fid,1,'uint32');
            properties.height_max = fread(fid,1,'uint32');
            properties.height_step = fread(fid,1,'uint32');
            properties.count_height = fread(fid,1,'uint32');
            properties.count_modules = fread(fid,1,'uint32');
            properties.pulse_frq = fread(fid,1,'uint32');
            
            % последовательное чтение частот зондирования
            for i=1:properties.count_modules
                properties.frqs(i) = fread(fid,1,'uint32');
            end
            
            properties.data_begin_position = ftell(fid);
            
            % 3. Закрываем файл данных.
            if ~isempty(fid)
                fclose(fid);
            end
        end
        
    end
end

