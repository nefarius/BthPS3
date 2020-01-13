using System.ComponentModel;
using System.Runtime.CompilerServices;
using BthPS3CfgUI.Annotations;
using Microsoft.Win32;

namespace BthPS3CfgUI
{
    public class ProfileDriverSettingsViewModel : INotifyPropertyChanged
    {
        private RegistryKey _bthPs3ServiceParameters;

        public ProfileDriverSettingsViewModel()
        {
            _bthPs3ServiceParameters =
                Registry.LocalMachine.OpenSubKey(
                    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\BthPS3\\Parameters", true);
        }

        public bool IsSIXAXISSupported
        {
            get => bool.Parse(_bthPs3ServiceParameters.GetValue("IsSIXAXISSupported", true).ToString());
            set => _bthPs3ServiceParameters.SetValue("IsSIXAXISSupported", value, RegistryValueKind.DWord);
        }

        public bool IsNAVIGATIONSupported
        {
            get => bool.Parse(_bthPs3ServiceParameters.GetValue("IsNAVIGATIONSupported", true).ToString());
            set => _bthPs3ServiceParameters.SetValue("IsNAVIGATIONSupported", value, RegistryValueKind.DWord);
        }

        public bool IsMOTIONSupported
        {
            get => bool.Parse(_bthPs3ServiceParameters.GetValue("IsMOTIONSupported", true).ToString());
            set => _bthPs3ServiceParameters.SetValue("IsMOTIONSupported", value, RegistryValueKind.DWord);
        }

        public bool IsWIRELESSSupported
        {
            get => bool.Parse(_bthPs3ServiceParameters.GetValue("IsWIRELESSSupported", true).ToString());
            set => _bthPs3ServiceParameters.SetValue("IsWIRELESSSupported", value, RegistryValueKind.DWord);
        }

        public event PropertyChangedEventHandler PropertyChanged;

        [NotifyPropertyChangedInvocator]
        protected virtual void OnPropertyChanged([CallerMemberName] string propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
}