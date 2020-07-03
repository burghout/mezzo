@echo off
md "%~dp0\output"

call "C:\mezzo\branches\busmezzo\standalone\release\mezzo_s" masterfile.mezzo 2 42 > output.txt

for /f "tokens=2 delims==" %%a in ('wmic OS Get localdatetime /value') do set "dt=%%a"
set "YYYY=%dt:~0,4%" & set "MM=%dt:~4,2%" & set "DD=%dt:~6,2%"
set "HH=%dt:~8,2%" & set "Min=%dt:~10,2%" & set "Sec=%dt:~12,2%"

set "fullstamp=%YYYY%-%MM%-%DD%_%HH%.%Min%.%Sec%"

md "%~dp0\output_%fullstamp%"

move "%~dp0\o_*" "%~dp0\output_%fullstamp%\."
move "%~dp0\output" "%~dp0\output_%fullstamp%\"
copy "%~dp0\*.mezzo" "%~dp0\output_%fullstamp%\."
copy "%~dp0\output.txt" "%~dp0\output_%fullstamp%\."

if "%1" == "gui" (
call "C:\mezzo\branches\busmezzo\gui\release\mezzo_gui" masterfile.mezzo
)
