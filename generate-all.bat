@echo off
echo Generating all code...

dotnet run --project Src/GeneratorApp -- generator-config.json

pause