using System.Windows;
using System.Windows.Media.Imaging;

using WixSharp;
using WixSharp.UI.Forms;
using WixSharp.UI.WPF;

namespace Nefarius.BthPS3.Setup.Dialogs;

/// <summary>
///     The standard LicenceDialog.
/// </summary>
/// <seealso cref="WixSharp.UI.WPF.WpfDialog" />
/// <seealso cref="System.Windows.Markup.IComponentConnector" />
/// <seealso cref="WixSharp.IWpfDialog" />
public partial class DriverSetupMethodSelectorDialog : WpfDialog, IWpfDialog
{
    private DriverSetupMethodSelectorModel _model;

    /// <summary>
    ///     Initializes a new instance of the <see cref="LicenceDialog" /> class.
    /// </summary>
    public DriverSetupMethodSelectorDialog()
    {
        InitializeComponent();
    }

    /// <summary>
    ///     This method is invoked by WixSHarp runtime when the custom dialog content is internally fully initialized.
    ///     This is a convenient place to do further initialization activities (e.g. localization).
    /// </summary>
    public void Init()
    {
        DataContext = _model = new DriverSetupMethodSelectorModel { Host = ManagedFormHost };
    }

    private void GoPrev_Click(object sender, RoutedEventArgs e)
    {
        _model.GoPrev();
    }

    private void GoNext_Click(object sender, RoutedEventArgs e)
    {
        _model.GoNext();
    }
}

/// <summary>
///     ViewModel for <see cref="DriverSetupMethodSelectorDialog" />.
/// </summary>
internal class DriverSetupMethodSelectorModel : NotifyPropertyChangedBase
{
    private ManagedForm _host;
    private ISession Session => Host?.Runtime.Session;
    private IManagedUIShell Shell => Host?.Shell;

    public ManagedForm Host
    {
        get => _host;
        set
        {
            _host = value;

            NotifyOfPropertyChange(nameof(Banner));
            NotifyOfPropertyChange(nameof(UseModern));
            NotifyOfPropertyChange(nameof(CanGoNext));
        }
    }

    public BitmapImage Banner => Session?.GetResourceBitmap("WixSharpUI_Bmp_Banner").ToImageSource() ??
                                 Session?.GetResourceBitmap("WixUI_Bmp_Banner").ToImageSource();

    public bool UseModern
    {
        get => Session?[CustomProperties.UseModern] == bool.TrueString;
        set
        {
            if (Host != null)
                Session[CustomProperties.UseModern] = value.ToString();

            NotifyOfPropertyChange(nameof(UseModern));
            NotifyOfPropertyChange(nameof(CanGoNext));
        }
    }

    public bool CanGoNext
        => true;

    public void GoPrev()
    {
        Shell?.GoPrev();
    }

    public void GoNext()
    {
        Shell?.GoNext();
    }
}