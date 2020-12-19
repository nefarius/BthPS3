using System.Windows;
using System.Windows.Threading;
using WpfBindingErrors;

namespace BthPS3CfgUI
{
    /// <summary>
    ///     Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        public App()
        {
            DispatcherUnhandledException += delegate(object sender, DispatcherUnhandledExceptionEventArgs args)
            {
                if (args.Exception is BindingException)
                {
                    MessageBox.Show("BthPS3 filter driver access failed. Are the drivers installed?",
                        "Filter driver not found", MessageBoxButton.OK,
                        MessageBoxImage.Error);

                    Shutdown(-1);
                    return;
                }

                MessageBox.Show(args.Exception.Message, "That looks like an error", MessageBoxButton.OK,
                    MessageBoxImage.Error);

                Shutdown(-1);
            };
        }

        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);

            BindingExceptionThrower.Attach();
        }
    }
}