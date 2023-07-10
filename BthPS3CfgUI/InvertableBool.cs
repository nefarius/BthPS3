namespace BthPS3CfgUI;

public class InvertableBool
{
    public InvertableBool(bool b)
    {
        Value = b;
    }

    public bool Value { get; }

    public bool Invert => !Value;

    public static implicit operator InvertableBool(bool b)
    {
        return new InvertableBool(b);
    }

    public static implicit operator bool(InvertableBool b)
    {
        return b.Value;
    }
}