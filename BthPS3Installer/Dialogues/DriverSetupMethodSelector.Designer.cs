namespace Nefarius.BthPS3.Setup.Dialogues;

partial class DriverSetupMethodSelector
{
    /// <summary>
    /// Required designer variable.
    /// </summary>
    private System.ComponentModel.IContainer components = null;

    /// <summary>
    /// Clean up any resources being used.
    /// </summary>
    /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
    protected override void Dispose(bool disposing)
    {
        if (disposing && (components != null))
        {
            components.Dispose();
        }
        base.Dispose(disposing);
    }

    #region Windows Form Designer generated code

    /// <summary>
    /// Required method for Designer support - do not modify
    /// the contents of this method with the code editor.
    /// </summary>
    private void InitializeComponent()
    {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(DriverSetupMethodSelector));
            this.banner = new System.Windows.Forms.PictureBox();
            this.topPanel = new System.Windows.Forms.Panel();
            this.summaryLabel = new System.Windows.Forms.Label();
            this.headingLabel = new System.Windows.Forms.Label();
            this.bottomPanel = new System.Windows.Forms.Panel();
            this.back = new System.Windows.Forms.Button();
            this.next = new System.Windows.Forms.Button();
            this.cancel = new System.Windows.Forms.Button();
            this.rbModern = new System.Windows.Forms.RadioButton();
            this.rbLegacy = new System.Windows.Forms.RadioButton();
            this.infoLabel = new System.Windows.Forms.Label();
            ((System.ComponentModel.ISupportInitialize)(this.banner)).BeginInit();
            this.topPanel.SuspendLayout();
            this.bottomPanel.SuspendLayout();
            this.SuspendLayout();
            // 
            // banner
            // 
            this.banner.BackColor = System.Drawing.Color.White;
            this.banner.Dock = System.Windows.Forms.DockStyle.Fill;
            this.banner.Location = new System.Drawing.Point(0, 0);
            this.banner.Name = "banner";
            this.banner.Size = new System.Drawing.Size(501, 59);
            this.banner.TabIndex = 0;
            this.banner.TabStop = false;
            // 
            // topPanel
            // 
            this.topPanel.BackColor = System.Drawing.SystemColors.Control;
            this.topPanel.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.topPanel.Controls.Add(this.summaryLabel);
            this.topPanel.Controls.Add(this.headingLabel);
            this.topPanel.Controls.Add(this.banner);
            this.topPanel.Location = new System.Drawing.Point(-5, -5);
            this.topPanel.Name = "topPanel";
            this.topPanel.Size = new System.Drawing.Size(503, 61);
            this.topPanel.TabIndex = 10;
            // 
            // summaryLabel
            // 
            this.summaryLabel.AutoSize = true;
            this.summaryLabel.BackColor = System.Drawing.Color.White;
            this.summaryLabel.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.summaryLabel.Location = new System.Drawing.Point(30, 31);
            this.summaryLabel.Name = "summaryLabel";
            this.summaryLabel.Size = new System.Drawing.Size(324, 13);
            this.summaryLabel.TabIndex = 1;
            this.summaryLabel.Text = "Select the desired driver installation logic to fix compatibility issues";
            // 
            // headingLabel
            // 
            this.headingLabel.AutoSize = true;
            this.headingLabel.BackColor = System.Drawing.Color.White;
            this.headingLabel.Font = new System.Drawing.Font("Tahoma", 9F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.headingLabel.Location = new System.Drawing.Point(16, 8);
            this.headingLabel.Name = "headingLabel";
            this.headingLabel.Size = new System.Drawing.Size(196, 14);
            this.headingLabel.TabIndex = 1;
            this.headingLabel.Text = "Driver Setup Method Selection";
            // 
            // bottomPanel
            // 
            this.bottomPanel.BackColor = System.Drawing.SystemColors.Control;
            this.bottomPanel.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.bottomPanel.Controls.Add(this.back);
            this.bottomPanel.Controls.Add(this.next);
            this.bottomPanel.Controls.Add(this.cancel);
            this.bottomPanel.Location = new System.Drawing.Point(-3, 308);
            this.bottomPanel.Name = "bottomPanel";
            this.bottomPanel.Size = new System.Drawing.Size(503, 57);
            this.bottomPanel.TabIndex = 9;
            // 
            // back
            // 
            this.back.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.back.Location = new System.Drawing.Point(227, 12);
            this.back.Name = "back";
            this.back.Size = new System.Drawing.Size(75, 23);
            this.back.TabIndex = 0;
            this.back.Text = "[WixUIBack]";
            this.back.UseVisualStyleBackColor = true;
            this.back.Click += new System.EventHandler(this.back_Click);
            // 
            // next
            // 
            this.next.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.next.Location = new System.Drawing.Point(308, 12);
            this.next.Name = "next";
            this.next.Size = new System.Drawing.Size(75, 23);
            this.next.TabIndex = 0;
            this.next.Text = "Install";
            this.next.UseVisualStyleBackColor = true;
            this.next.Click += new System.EventHandler(this.next_Click);
            // 
            // cancel
            // 
            this.cancel.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.cancel.Location = new System.Drawing.Point(404, 12);
            this.cancel.Name = "cancel";
            this.cancel.Size = new System.Drawing.Size(75, 23);
            this.cancel.TabIndex = 0;
            this.cancel.Text = "[WixUICancel]";
            this.cancel.UseVisualStyleBackColor = true;
            this.cancel.Click += new System.EventHandler(this.cancel_Click);
            // 
            // rbModern
            // 
            this.rbModern.AutoSize = true;
            this.rbModern.Checked = true;
            this.rbModern.Location = new System.Drawing.Point(29, 191);
            this.rbModern.Name = "rbModern";
            this.rbModern.Size = new System.Drawing.Size(244, 17);
            this.rbModern.TabIndex = 11;
            this.rbModern.TabStop = true;
            this.rbModern.Text = "Modern hot-plug method (no reboot required)";
            this.rbModern.UseVisualStyleBackColor = true;
            // 
            // rbLegacy
            // 
            this.rbLegacy.AutoSize = true;
            this.rbLegacy.Location = new System.Drawing.Point(29, 214);
            this.rbLegacy.Name = "rbLegacy";
            this.rbLegacy.Size = new System.Drawing.Size(236, 17);
            this.rbLegacy.TabIndex = 12;
            this.rbLegacy.Text = "Legacy sequential method (reboot required)";
            this.rbLegacy.UseVisualStyleBackColor = true;
            // 
            // infoLabel
            // 
            this.infoLabel.Location = new System.Drawing.Point(29, 87);
            this.infoLabel.Name = "infoLabel";
            this.infoLabel.Size = new System.Drawing.Size(448, 101);
            this.infoLabel.TabIndex = 13;
            this.infoLabel.Text = resources.GetString("infoLabel.Text");
            // 
            // DriverSetupMethodSelector
            // 
            this.ClientSize = new System.Drawing.Size(494, 361);
            this.Controls.Add(this.infoLabel);
            this.Controls.Add(this.rbLegacy);
            this.Controls.Add(this.rbModern);
            this.Controls.Add(this.topPanel);
            this.Controls.Add(this.bottomPanel);
            this.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.Name = "DriverSetupMethodSelector";
            this.Text = "[UserNameDlg_Title]";
            this.Load += new System.EventHandler(this.dialog_Load);
            ((System.ComponentModel.ISupportInitialize)(this.banner)).EndInit();
            this.topPanel.ResumeLayout(false);
            this.topPanel.PerformLayout();
            this.bottomPanel.ResumeLayout(false);
            this.ResumeLayout(false);
            this.PerformLayout();

    }

    #endregion

    private System.Windows.Forms.PictureBox banner;
    private System.Windows.Forms.Panel topPanel;
    private System.Windows.Forms.Label summaryLabel;
    private System.Windows.Forms.Label headingLabel;
    private System.Windows.Forms.Panel bottomPanel;
    private System.Windows.Forms.Button back;
    private System.Windows.Forms.Button next;
    private System.Windows.Forms.Button cancel;
    private System.Windows.Forms.RadioButton rbModern;
    private System.Windows.Forms.RadioButton rbLegacy;
    private System.Windows.Forms.Label infoLabel;
}