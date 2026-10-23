using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;

static class CustomActionBindingRedirectTests
{
    public static void Run()
    {
        TestShippedAssemblyMatchesRedirect();
        TestMismatchedNewVersionFails();
        TestRangeThatExcludesShippedVersionFails();
        TestMissingAssemblyFails();
        TestPublicKeyTokenMismatchFails();
        TestRepositoryRedirectsCoverTheirTargetVersion();
    }

    static void TestShippedAssemblyMatchesRedirect()
    {
        using TempAssemblyScope scope = new();
        WriteConfig(scope.ConfigPath, scope.Identity, scope.Identity.Version, scope.Identity.Version);
        int count = Nefarius.BthPS3.Setup.CustomActionBindingRedirects.AssertMatch(
            scope.ConfigPath,
            scope.Directory);
        if (count != 1)
        {
            throw new InvalidOperationException($"FAIL matching redirect: expected 1 redirect, got {count}.");
        }
    }

    static void TestMismatchedNewVersionFails()
    {
        using TempAssemblyScope scope = new();
        Version stale = new(9, 9, 9, 9);
        WriteConfig(scope.ConfigPath, scope.Identity, stale, scope.Identity.Version);
        AssertThrows(
            () => Nefarius.BthPS3.Setup.CustomActionBindingRedirects.AssertMatch(scope.ConfigPath, scope.Directory),
            "stale newVersion",
            scope.Identity.Version.ToString(),
            stale.ToString());
    }

    static void TestRangeThatExcludesShippedVersionFails()
    {
        using TempAssemblyScope scope = new();
        Version rangeEnd = new(0, 0, 0, 1);
        if (scope.Identity.Version <= rangeEnd)
        {
            throw new InvalidOperationException(
                $"FAIL oldVersion fixture: test assembly {scope.Identity.Version} is inside the excluded range.");
        }

        WriteConfig(scope.ConfigPath, scope.Identity, scope.Identity.Version, rangeEnd);
        AssertThrows(
            () => Nefarius.BthPS3.Setup.CustomActionBindingRedirects.AssertMatch(scope.ConfigPath, scope.Directory),
            "oldVersion excludes shipped assembly",
            "does not include shipped assembly");
    }

    static void TestMissingAssemblyFails()
    {
        using TempAssemblyScope scope = new();
        File.Delete(scope.AssemblyPath);
        WriteConfig(scope.ConfigPath, scope.Identity, scope.Identity.Version, scope.Identity.Version);
        AssertThrows(
            () => Nefarius.BthPS3.Setup.CustomActionBindingRedirects.AssertMatch(scope.ConfigPath, scope.Directory),
            "missing shipped assembly",
            "was found");
    }

    static void TestPublicKeyTokenMismatchFails()
    {
        AssemblyName actual = new("Example, Version=1.2.3.4, Culture=neutral, PublicKeyToken=0123456789abcdef");
        Nefarius.BthPS3.Setup.CustomActionBindingRedirects.Redirect redirect = new(
            actual.Name,
            "cc7b13ffcd2ddd51",
            actual.Version,
            new Version(0, 0, 0, 0),
            actual.Version);
        string mismatch = Nefarius.BthPS3.Setup.CustomActionBindingRedirects.DescribeMismatch(redirect, actual);
        if (mismatch is null || !mismatch.Contains("publicKeyToken"))
        {
            throw new InvalidOperationException(
                $"FAIL publicKeyToken mismatch: expected an identity error, got '{mismatch}'.");
        }
    }

    static void TestRepositoryRedirectsCoverTheirTargetVersion()
    {
        string configPath = FindRepositoryConfig();
        var redirects = Nefarius.BthPS3.Setup.CustomActionBindingRedirects.Read(configPath).ToList();
        foreach (string name in new[] { "Microsoft.Bcl.AsyncInterfaces", "WixSharp" })
        {
            Nefarius.BthPS3.Setup.CustomActionBindingRedirects.Redirect redirect = redirects.Single(candidate =>
                candidate.Name.Equals(name, StringComparison.OrdinalIgnoreCase));
            if (redirect.NewVersion < redirect.OldStart || redirect.NewVersion > redirect.OldEnd)
            {
                throw new InvalidOperationException(
                    $"FAIL {name}: newVersion {redirect.NewVersion} is outside oldVersion " +
                    $"{redirect.OldStart}-{redirect.OldEnd}.");
            }
        }
    }

    static void WriteConfig(string path, AssemblyName identity, Version newVersion, Version oldEnd)
    {
        string token = FormatToken(identity.GetPublicKeyToken());
        string tokenAttribute = string.IsNullOrEmpty(token) ? "" : $" publicKeyToken=\"{token}\"";
        string xml = $"""
            <?xml version="1.0" encoding="utf-8"?>
            <configuration>
              <runtime>
                <assemblyBinding xmlns="urn:schemas-microsoft-com:asm.v1">
                  <dependentAssembly>
                    <assemblyIdentity name="{identity.Name}" culture="neutral"{tokenAttribute} />
                    <bindingRedirect oldVersion="0.0.0.0-{oldEnd}" newVersion="{newVersion}" />
                  </dependentAssembly>
                </assemblyBinding>
              </runtime>
            </configuration>
            """;
        File.WriteAllText(path, xml, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
    }

    static string FormatToken(byte[] token)
    {
        if (token is null || token.Length == 0)
        {
            return "";
        }

        return string.Concat(token.Select(part => part.ToString("x2")));
    }

    static string FindRepositoryConfig()
    {
        DirectoryInfo directory = new(AppContext.BaseDirectory);
        while (directory is not null)
        {
            string candidate = Path.Combine(directory.FullName, "BthPS3Installer", "CustomActions.config");
            if (File.Exists(candidate))
            {
                return candidate;
            }

            directory = directory.Parent;
        }

        throw new InvalidOperationException("CustomActions.config was not found above the test output.");
    }

    static void AssertThrows(Action action, string name, params string[] requiredText)
    {
        try
        {
            action();
        }
        catch (InvalidOperationException exception)
        {
            foreach (string text in requiredText)
            {
                if (!exception.Message.Contains(text))
                {
                    throw new InvalidOperationException(
                        $"FAIL {name}: error did not mention '{text}'.{Environment.NewLine}{exception.Message}");
                }
            }

            return;
        }

        throw new InvalidOperationException($"FAIL {name}: expected an exception");
    }

    sealed class TempAssemblyScope : IDisposable
    {
        public TempAssemblyScope()
        {
            Directory = System.IO.Directory.CreateTempSubdirectory("bthps3-binding-").FullName;
            SourcePath = typeof(CustomActionBindingRedirectTests).Assembly.Location;
            if (string.IsNullOrEmpty(SourcePath))
            {
                throw new InvalidOperationException("Test assembly location is empty.");
            }

            Identity = AssemblyName.GetAssemblyName(SourcePath);
            AssemblyPath = Path.Combine(Directory, Identity.Name + ".dll");
            ConfigPath = Path.Combine(Directory, "CustomActions.config");
            File.Copy(SourcePath, AssemblyPath);
        }

        public string Directory { get; }

        public string SourcePath { get; }

        public string AssemblyPath { get; }

        public string ConfigPath { get; }

        public AssemblyName Identity { get; }

        public void Dispose()
        {
            if (System.IO.Directory.Exists(Directory))
            {
                System.IO.Directory.Delete(Directory, recursive: true);
            }
        }
    }
}
