classdef classAmplitude < handle
    %classAmplitude - доступ к большому файлу данных амплитуд.
    %   Detailed explanation goes here
    
    properties (GetAccess = public, SetAccess = private)
        fileName
    end
    properties
        position
    end
    
    methods
        function obj = classAmplitude(fileName, varargin)
            p = inputParser;
            isfrq = @(str) exist('str','var') & ischar(str) & strcmp(str(end-3:end),'.frq'); 
            p.addRequired('fileName', isfrq);
            p.parse(fileName, varargin{:});
            obj.fileName = p.Results.fileName;
        end
    end
    
end

