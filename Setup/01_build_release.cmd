@echo off
@setlocal

set MYDIR=%~dp0
pushd "%MYDIR%"

where msbuild > nul 2>&1 && (
    echo Starting build

    rem https://github.com/microsoft/DMF
    msbuild ..\DMF\Dmf.sln /p:Configuration=Release /p:Platform=x64 -verbosity:minimal
    msbuild ..\DMF\Dmf.sln /p:Configuration=Debug /p:Platform=x64 -verbosity:minimal
    msbuild ..\DMF\Dmf.sln /p:Configuration=Release /p:Platform=ARM64 -verbosity:minimal
    msbuild ..\DMF\Dmf.sln /p:Configuration=Debug /p:Platform=ARM64 -verbosity:minimal

    rem https://git.nefarius.at/nefarius/Domito
    msbuild ..\Domito\Domito.sln /p:Configuration=Release /p:Platform=x64 -verbosity:minimal
    msbuild ..\Domito\Domito.sln /p:Configuration=Debug /p:Platform=x64 -verbosity:minimal
    msbuild ..\Domito\Domito.sln /p:Configuration=Release /p:Platform=ARM64 -verbosity:minimal
    msbuild ..\Domito\Domito.sln /p:Configuration=Debug /p:Platform=ARM64 -verbosity:minimal
    
    rem filter driver
    msbuild /p:Configuration=Release /p:Platform=x64 ..\BthPS3PSM\BthPS3PSM.vcxproj -verbosity:minimal
    msbuild /p:Configuration=Release /p:Platform=ARM64 ..\BthPS3PSM\BthPS3PSM.vcxproj -verbosity:minimal

    rem profile driver
    msbuild /p:Configuration=Release /p:Platform=x64 ..\BthPS3\BthPS3.vcxproj -verbosity:minimal
    msbuild /p:Configuration=Release /p:Platform=ARM64 ..\BthPS3\BthPS3.vcxproj -verbosity:minimal

) || (
    echo MSBuild not found, please make sure you called the "Setup-VS" snippet before!
)

popd
endlocal