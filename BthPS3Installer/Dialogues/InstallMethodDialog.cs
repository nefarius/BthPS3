using System;

using WixSharp;
using WixSharp.UI.Forms;

namespace Nefarius.BthPS3.Setup.Dialogues
{
    /// <summary>
    /// The standard InstallDir dialog
    /// </summary>
    public partial class InstallMethodDialog : ManagedForm, IManagedDialog  // change ManagedForm->Form if you want to show it in designer
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="InstallMethodDialog"/> class.
        /// </summary>
        public InstallMethodDialog()
        {
            InitializeComponent();
            label1.MakeTransparentOn(banner);
            label2.MakeTransparentOn(banner);
        }

        void InstallDirDialog_Load(object sender, EventArgs e)
        {
            banner.Image = Runtime.Session.GetResourceBitmap("WixUI_Bmp_Banner") ??
                           Runtime.Session.GetResourceBitmap("WixSharpUI_Bmp_Banner");

            if (banner.Image != null)
                ResetLayout();
        }

        void ResetLayout()
        {
            // The form controls are properly anchored and will be correctly resized on parent form
            // resizing. However the initial sizing by WinForm runtime doesn't a do good job with DPI
            // other than 96. Thus manual resizing is the only reliable option apart from going WPF.
            float ratio = (float)banner.Image.Width / (float)banner.Image.Height;
            topPanel.Height = (int)(banner.Width / ratio);
            topBorder.Top = topPanel.Height + 1;

            middlePanel.Top = topBorder.Bottom + 10;

            var upShift = (int)(next.Height * 2.3) - bottomPanel.Height;
            bottomPanel.Top -= upShift;
            bottomPanel.Height += upShift;
        }

        void back_Click(object sender, EventArgs e)
        {
            Shell.GoPrev();
        }

        void next_Click(object sender, EventArgs e)
        {
            Runtime.Session[CustomProperties.UseModern] = rbModern.Checked.ToString();
            Shell.GoNext();
        }

        void cancel_Click(object sender, EventArgs e)
        {
            Shell.Cancel();
        }
    }
}