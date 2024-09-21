#nullable enable
using System;
using System.Linq;

using WixToolset.Dtf.WindowsInstaller;

namespace Nefarius.BthPS3.Setup.Util;

public static class SessionExtensions
{
    public static bool IsFeatureEnabledPartial(this Session session, string partialName)
    {
        FeatureInfo? featureInfo =
            session.Features.SingleOrDefault(f => f.Name.Contains(partialName, StringComparison.OrdinalIgnoreCase));

        if (featureInfo is null)
        {
            return false;
        }

        return featureInfo.RequestState != InstallState.Unknown;
    }
}