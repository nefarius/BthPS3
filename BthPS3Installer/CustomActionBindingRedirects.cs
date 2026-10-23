#nullable enable

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Xml.Linq;

namespace Nefarius.BthPS3.Setup;

/// <summary>
///     Checks the DTF host config against the assemblies copied to the installer output.
///     MakeSfxCA embeds <c>CustomActions.config</c>, so a redirect that still names the
///     previous package version fails when a custom action loads that assembly.
/// </summary>
internal static class CustomActionBindingRedirects
{
    private static readonly XNamespace BindingNamespace = "urn:schemas-microsoft-com:asm.v1";

    internal readonly struct Redirect
    {
        internal Redirect(
            string name,
            string? publicKeyToken,
            Version newVersion,
            Version oldStart,
            Version oldEnd)
        {
            Name = name;
            PublicKeyToken = publicKeyToken;
            NewVersion = newVersion;
            OldStart = oldStart;
            OldEnd = oldEnd;
        }

        internal string Name { get; }

        internal string? PublicKeyToken { get; }

        internal Version NewVersion { get; }

        internal Version OldStart { get; }

        internal Version OldEnd { get; }
    }

    internal static int AssertMatch(string configPath, string outputDirectory)
    {
        if (!File.Exists(configPath))
        {
            throw new InvalidOperationException(
                $"Custom-action binding redirect config was not found: {configPath}");
        }

        if (!Directory.Exists(outputDirectory))
        {
            throw new InvalidOperationException(
                $"Custom-action output directory was not found: {outputDirectory}");
        }

        List<Redirect> redirects = Read(configPath).ToList();
        if (redirects.Count == 0)
        {
            throw new InvalidOperationException(
                $"Custom-action config '{configPath}' does not contain any binding redirects.");
        }

        List<string> errors = new();
        foreach (Redirect redirect in redirects)
        {
            string? assemblyPath = FindAssembly(outputDirectory, redirect.Name);
            if (assemblyPath is null)
            {
                errors.Add(
                    $"'{redirect.Name}' is redirected in '{configPath}' but neither " +
                    $"'{redirect.Name}.dll' nor '{redirect.Name}.exe' was found in '{outputDirectory}'.");
                continue;
            }

            string? mismatch = DescribeMismatch(redirect, AssemblyName.GetAssemblyName(assemblyPath));
            if (mismatch is not null)
            {
                errors.Add(mismatch);
            }
        }

        if (errors.Count > 0)
        {
            throw new InvalidOperationException(
                "CustomActions.config binding redirects do not match the assemblies this build ships. " +
                "Update BthPS3Installer/CustomActions.config; a stale redirect causes FileLoadException " +
                "in managed custom actions." + Environment.NewLine +
                string.Join(Environment.NewLine, errors));
        }

        return redirects.Count;
    }

    internal static IEnumerable<Redirect> Read(string configPath)
    {
        XDocument document = XDocument.Load(configPath);
        foreach (XElement dependent in document.Descendants(BindingNamespace + "dependentAssembly"))
        {
            XElement? identity = dependent.Element(BindingNamespace + "assemblyIdentity");
            XElement? redirect = dependent.Element(BindingNamespace + "bindingRedirect");
            if (identity is null || redirect is null)
            {
                throw new InvalidOperationException(
                    $"Custom-action config '{configPath}' has a dependentAssembly without an identity and redirect.");
            }

            string assemblyName = (string?)identity.Attribute("name") ?? "";
            if (string.IsNullOrWhiteSpace(assemblyName))
            {
                throw new InvalidOperationException(
                    $"Custom-action config '{configPath}' has an assemblyIdentity without a name.");
            }
            string? newVersionText = (string?)redirect.Attribute("newVersion");
            string? oldVersionText = (string?)redirect.Attribute("oldVersion");
            Version newVersion = ParseVersion(newVersionText, assemblyName, "newVersion");
            (Version oldStart, Version oldEnd) = ParseRange(oldVersionText, assemblyName);
            if (oldStart > oldEnd)
            {
                throw new InvalidOperationException(
                    $"'{assemblyName}' oldVersion '{oldVersionText}' starts after it ends.");
            }

            string? token = (string?)identity.Attribute("publicKeyToken");
            if (string.IsNullOrWhiteSpace(token) ||
                string.Equals(token, "null", StringComparison.OrdinalIgnoreCase))
            {
                token = null;
            }

            yield return new Redirect(assemblyName, token, newVersion, oldStart, oldEnd);
        }
    }

    /// <summary>
    ///     Returns an actionable error when <paramref name="actual"/> is outside the redirect,
    ///     or null when the shipped assembly is the version the config selects.
    /// </summary>
    internal static string? DescribeMismatch(Redirect redirect, AssemblyName actual)
    {
        if (string.IsNullOrWhiteSpace(actual.Name) ||
            !actual.Name.Equals(redirect.Name, StringComparison.OrdinalIgnoreCase))
        {
            return $"Assembly '{actual.Name}' does not match binding redirect '{redirect.Name}'.";
        }

        if (redirect.PublicKeyToken is not null)
        {
            string actualToken = FormatToken(actual.GetPublicKeyToken()) ?? "null";
            if (!actualToken.Equals(redirect.PublicKeyToken, StringComparison.OrdinalIgnoreCase))
            {
                return
                    $"'{redirect.Name}' publicKeyToken is '{actualToken}', but CustomActions.config expects '{redirect.PublicKeyToken}'.";
            }
        }

        if (actual.Version is null)
        {
            return $"'{redirect.Name}' has no assembly version to compare with CustomActions.config.";
        }

        List<string> problems = new();
        if (actual.Version != redirect.NewVersion)
        {
            problems.Add(
                $"newVersion is {redirect.NewVersion} but the shipped assembly is {actual.Version}");
        }

        if (actual.Version < redirect.OldStart || actual.Version > redirect.OldEnd)
        {
            problems.Add(
                $"oldVersion {redirect.OldStart}-{redirect.OldEnd} does not include shipped assembly {actual.Version}");
        }

        if (problems.Count == 0)
        {
            return null;
        }

        return $"'{redirect.Name}': {string.Join("; ", problems)}. Update BthPS3Installer/CustomActions.config.";
    }

    private static string? FindAssembly(string outputDirectory, string name)
    {
        foreach (string extension in new[] { ".dll", ".exe" })
        {
            string candidate = Path.Combine(outputDirectory, name + extension);
            if (File.Exists(candidate))
            {
                return candidate;
            }
        }

        return null;
    }

    private static Version ParseVersion(string? text, string assemblyName, string attribute)
    {
        string value = text ?? "";
        if (string.IsNullOrWhiteSpace(value) ||
            !Version.TryParse(value.Trim(), out Version? version) ||
            version is null)
        {
            throw new InvalidOperationException(
                $"'{assemblyName}' has an invalid {attribute} '{text}' in CustomActions.config.");
        }

        return version;
    }

    private static (Version Start, Version End) ParseRange(string? oldVersion, string assemblyName)
    {
        string range = oldVersion ?? "";
        if (string.IsNullOrWhiteSpace(range))
        {
            throw new InvalidOperationException(
                $"'{assemblyName}' is missing oldVersion in CustomActions.config.");
        }

        string[] parts = range.Split('-');
        if (parts.Length == 1)
        {
            Version single = ParseVersion(parts[0], assemblyName, "oldVersion");
            return (single, single);
        }

        if (parts.Length == 2)
        {
            return (
                ParseVersion(parts[0], assemblyName, "oldVersion"),
                ParseVersion(parts[1], assemblyName, "oldVersion"));
        }

        throw new InvalidOperationException(
            $"'{assemblyName}' has an invalid oldVersion '{oldVersion}' in CustomActions.config.");
    }

    private static string? FormatToken(byte[]? token)
    {
        if (token is null || token.Length == 0)
        {
            return null;
        }

        return string.Concat(token.Select(part => part.ToString("x2")));
    }
}
