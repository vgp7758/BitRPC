@echo off
echo Generating Python code...

REM Create a temporary config file for Python generation only
(
echo {
echo   "protocolFile": "Protocols/scene3d.pdl",
echo   "outputDirectory": "Generated",
echo   "languages": [
echo     {
echo       "name": "python",
echo       "enabled": true,
echo       "namespace": "example.protocol",
echo       "runtimePath": "Src/Python"
echo     }
echo   ]
echo }
) > temp-config.json

dotnet run --project Src/GeneratorApp -- temp-config.json

REM Clean up temporary config file
del temp-config.json

pause