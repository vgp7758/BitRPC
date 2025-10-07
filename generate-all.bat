@echo off
echo Generating all code...

dotnet run --project Src/GeneratorApp/GeneratorApp.csproj -- generator-config.json

pause