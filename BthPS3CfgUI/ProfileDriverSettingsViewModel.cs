using System.ComponentModel;
using System.Runtime.CompilerServices;
using BthPS3CfgUI.Annotations;
using Microsoft.Win32;

namespace BthPS3CfgUI
{
    public class ProfileDriverSettingsViewModel : INotifyPropertyChanged
    {
        private readonly RegistryKey _bthPs3ServiceParameters;

        public ProfileDriverSettingsViewModel()
        {
            _bthPs3ServiceParameters =
                Registry.LocalMachine.OpenSubKey(
                    "SYSTEM\\CurrentControlSet\\Services\\BthPS3\\Parameters", true);
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

        #region Profile Driver

        [UsedImplicitly]
        public bool IsSIXAXISSupported
        {
            get => GetBool("IsSIXAXISSupported", true);
            set => SetBool("IsSIXAXISSupported", value);
        }

        [UsedImplicitly]
        public bool IsNAVIGATIONSupported
        {
            get => GetBool("IsNAVIGATIONSupported", true);
            set => SetBool("IsNAVIGATIONSupported", value);
        }

        [UsedImplicitly]
        public bool IsMOTIONSupported
        {
            get => GetBool("IsMOTIONSupported", false);
            set => SetBool("IsMOTIONSupported", value);
        }

        [UsedImplicitly]
        public bool IsWIRELESSSupported
        {
            get => GetBool("IsWIRELESSSupported", false);
            set => SetBool("IsWIRELESSSupported", value);
        }

        [UsedImplicitly]
        public bool AutoEnableFilter
        {
            get => GetBool("AutoEnableFilter", true);
            set => SetBool("AutoEnableFilter", value);
        }

        [UsedImplicitly]
        public uint AutoEnableFilterDelay
        {
            get => GetUInt("AutoEnableFilterDelay", 10);
            set => SetUInt("AutoEnableFilterDelay", value);
        }

        [UsedImplicitly]
        public bool AutoDisableFilter
        {
            get => GetBool("AutoDisableFilter", true);
            set => SetBool("AutoDisableFilter", value);
        }

        #endregion

        #region Danger Zone

        [UsedImplicitly]
        public bool RawPDO
        {
            get => GetBool("RawPDO", true);
            set => SetBool("RawPDO", value);
        }

        [UsedImplicitly]
        public bool HidePDO
        {
            get => GetBool("HidePDO", false);
            set => SetBool("HidePDO", value);
        }

        [UsedImplicitly]
        public bool AdminOnlyPDO
        {
            get => GetBool("AdminOnlyPDO", false);
            set => SetBool("AdminOnlyPDO", value);
        }

        [UsedImplicitly]
        public bool ExclusivePDO
        {
            get => GetBool("ExclusivePDO", true);
            set => SetBool("ExclusivePDO", value);
        }

        [UsedImplicitly]
        public uint ChildIdleTimeout
        {
            get => GetUInt("ChildIdleTimeout", 10000);
            set => SetUInt("ChildIdleTimeout", value);
        }

        #endregion

        public event PropertyChangedEventHandler PropertyChanged;

        [NotifyPropertyChangedInvocator]
        protected virtual void OnPropertyChanged([CallerMemberName] string propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
}