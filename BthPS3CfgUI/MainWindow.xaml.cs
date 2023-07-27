using System.Diagnostics;
using System.Windows;
using System.Windows.Navigation;

using AdonisUI.Controls;

namespace BthPS3CfgUI;

/// <summary>
///     Interaction logic for MainWindow.xaml
/// </summary>
public partial class MainWindow : AdonisWindow
{
    public MainWindow()
    {
        InitializeComponent();

        DataContext = new ProfileDriverSettingsViewModel();
    }

    private void Hyperlink_OnRequestNavigate(object sender, RequestNavigateEventArgs e)
    {
        Process.Start(e.Uri.ToString());
    }

    private void Help_OnClick(object sender, RoutedEventArgs e)
    {
        Process.Start("https://docs.nefarius.at/projects/BthPS3/");
    }
}