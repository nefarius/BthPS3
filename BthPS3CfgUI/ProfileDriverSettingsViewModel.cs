using System;
using System.ComponentModel;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;
using System.Windows.Threading;

using Microsoft.Win32;

using Nefarius.BthPS3.Shared;

namespace BthPS3CfgUI;

[SuppressMessage("ReSharper", "UnusedMember.Global")]
[SuppressMessage("ReSharper", "InconsistentNaming")]
public class ProfileDriverSettingsViewModel : INotifyPropertyChanged
{
    private readonly RegistryKey _bthPs3ServiceParameters;
    // ReSharper disable once PrivateFieldCanBeConvertedToLocalVariable
    private readonly DispatcherTimer _dispatcherTimer;

    public ProfileDriverSettingsViewModel()
    {
        _bthPs3ServiceParameters =
            Registry.LocalMachine.OpenSubKey(
                "SYSTEM\\CurrentControlSet\\Services\\BthPS3\\Parameters", true);

        if (_bthPs3ServiceParameters == null)
        {
            throw new Exception("BthPS3 registry path not found. Are the drivers installed?");
        }

        if (BluetoothHelper.IsBluetoothRadioAvailable)
        {
            //
            // Periodically refresh patch state value
            // 
            _dispatcherTimer = new DispatcherTimer { Interval = new TimeSpan(0, 0, 1) };
            _dispatcherTimer.Tick += DispatcherTimerOnTick;
            _dispatcherTimer.Start();
        }
    }

    public InvertableBool IsBluetoothRadioAvailable => BluetoothHelper.IsBluetoothRadioAvailable;

    #region PSM patch
    
    public bool IsPSMPatchEnabled
    {
        get => FilterDriver.IsFilterEnabled;
        set => FilterDriver.IsFilterEnabled = value;
    }

    #endregion

    public event PropertyChangedEventHandler PropertyChanged;

    private void DispatcherTimerOnTick(object sender, EventArgs e)
    {
        //
        // Force UI to re-evaluate patch status value
        // 
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs("IsPSMPatchEnabled"));
    }

    private void SetBool(string valueName, bool value)
    {
        _bthPs3ServiceParameters.SetValue(valueName, value ? 1 : 0, RegistryValueKind.DWord);
    }

    private bool GetBool(string valueName, bool defaultValue)
    {
        return int.Parse(_bthPs3ServiceParameters.GetValue(valueName, defaultValue ? 1 : 0).ToString()) > 0;
    }

    private void SetUInt(string valueName, uint value)
    {
        _bthPs3ServiceParameters.SetValue(valueName, value, RegistryValueKind.DWord);
    }

    private uint GetUInt(string valueName, uint defaultValue)
    {
        return uint.Parse(_bthPs3ServiceParameters.GetValue(valueName, defaultValue).ToString());
    }

    protected virtual void OnPropertyChanged([CallerMemberName] string propertyName = null)
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }

    #region Profile Driver

    public bool IsSIXAXISSupported
    {
        get => GetBool("IsSIXAXISSupported", true);
        set => SetBool("IsSIXAXISSupported", value);
    }

    public bool IsNAVIGATIONSupported
    {
        get => GetBool("IsNAVIGATIONSupported", true);
        set => SetBool("IsNAVIGATIONSupported", value);
    }

    public bool IsMOTIONSupported
    {
        get => GetBool("IsMOTIONSupported", false);
        set => SetBool("IsMOTIONSupported", value);
    }

    public bool IsWIRELESSSupported
    {
        get => GetBool("IsWIRELESSSupported", false);
        set => SetBool("IsWIRELESSSupported", value);
    }

    public bool AutoEnableFilter
    {
        get => GetBool("AutoEnableFilter", true);
        set => SetBool("AutoEnableFilter", value);
    }

    public uint AutoEnableFilterDelay
    {
        get => GetUInt("AutoEnableFilterDelay", 10);
        set => SetUInt("AutoEnableFilterDelay", value);
    }

    public bool AutoDisableFilter
    {
        get => GetBool("AutoDisableFilter", true);
        set => SetBool("AutoDisableFilter", value);
    }

    #endregion

    #region Danger Zone

    public bool RawPDO
    {
        get => GetBool("RawPDO", true);
        set => SetBool("RawPDO", value);
    }

    public bool HidePDO
    {
        get => GetBool("HidePDO", false);
        set => SetBool("HidePDO", value);
    }

    public bool AdminOnlyPDO
    {
        get => GetBool("AdminOnlyPDO", false);
        set => SetBool("AdminOnlyPDO", value);
    }

    public bool ExclusivePDO
    {
        get => GetBool("ExclusivePDO", true);
        set => SetBool("ExclusivePDO", value);
    }

    public uint ChildIdleTimeout
    {
        get => GetUInt("ChildIdleTimeout", 10000);
        set => SetUInt("ChildIdleTimeout", value);
    }

    #endregion
}