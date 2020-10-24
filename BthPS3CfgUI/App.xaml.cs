using System.Windows;
using System.Windows.Threading;

namespace BthPS3CfgUI
{
    /// <summary>
    ///     Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        public App()
        {
            this.DispatcherUnhandledException += delegate(object sender, DispatcherUnhandledExceptionEventArgs args)
                {
                    MessageBox.Show(args.Exception.InnerException.Message, "That looks like an error", MessageBoxButton.OK,
                        MessageBoxImage.Error);
                };
        }
    }
}