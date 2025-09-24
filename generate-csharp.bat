@echo off
echo Generating C# code...

REM Create a temporary config file for C# generation only
(
echo {
echo   "protocolFile": "Protocols/model_sync_service.pdl",
echo   "outputDirectory": "../Assets/Scripts/Gen/Protocol",
echo   "languages": [
echo     {
echo       "name": "csharp",
echo       "enabled": true,
echo       "namespace": "BitRPC.Protocol",
echo       "runtimePath": "Src"
echo     }
echo   ]
echo }
) > temp-config.json

dotnet run --project Src/GeneratorApp -- temp-config.json

REM Clean up temporary config file
del temp-config.json

pause