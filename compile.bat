@echo off
path c:\BCPP31\bin;%path%
bcc -ml -IC:\BCPP31\INCLUDE -LC:\BCPP31\LIB -eJUMANJI.EXE JUMANJI.CPP > compile.log
exit
