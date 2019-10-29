@echo off
@setlocal

makecab /f .\BthPS3.ddf

signtool sign /v /ac "C:\Program Files (x86)\Windows Kits\10\CrossCertificates\DigiCert_High_Assurance_EV_Root_CA.crt" /n "Nefarius Software Solutions e.U." /tr http://timestamp.digicert.com /fd sha256 /td sha256 *.cab

endlocal
