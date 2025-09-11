@echo off
echo Installing Newtonsoft.Json package...
dotnet add Src/GeneratorApp/GeneratorApp.csproj package Newtonsoft.Json
echo Package installation complete.
pause