@echo off
echo Generating C++ code...

cd ..
REM Create a temporary config file for C++ generation only
(
echo {
echo   "protocolFile": "Demo/test_protocol.pdl",
echo   "languages": [
echo     {
echo       "name": "cpp",
echo       "enabled": true,
echo       "namespace": "test.protocol",
echo       "runtimePath": "Src/C++Core",
echo       "OutputDirectory": "Demo/cpp",
echo     }
echo   ]
echo }
) > temp-config.json

dotnet run --project Src/GeneratorApp -- temp-config.json

REM Clean up temporary config file
del temp-config.json

pause