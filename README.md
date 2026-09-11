# Mavra

Mavra is a simple, no-nonsense wrapper around the [Microsoft Macro Assembler (MASM)](https://en.wikipedia.org/wiki/Microsoft_Macro_Assembler).

MASM is notoriously cumbersome to use outside of Visual Studio. You can't simply invoke it from the command line like an ordinary program: you first need to launch the [Developer Command Prompt](https://learn.microsoft.com/en-us/visualstudio/ide/reference/command-prompt-powershell?view=visualstudio), or manually run scripts buried deep within the Visual Studio installation to configure the required environment variables. And those variables only apply to the terminal session in which the setup was performed.

The other official option is to use MASM through Visual Studio itself, which defeats the purpose of having a command-line assembler in the first place.

**Mavra is the alternative solution.** It lets you use MASM from any terminal, anywhere on Windows, without having to configure a special environment first.

## Installation

Mavra requires:

* [MinGW](https://www.mingw-w64.org/downloads/#mingw-w64-builds) 16.2.0
* [vswhere](https://github.com/microsoft/vswhere) 3.17

Place `vswhere` either alongside the Mavra binaries or in a directory listed in your `PATH`. After that, you can start using Mavra.

## Build

```powershell
git clone https://github.com/tudor14x/mavra
cd mavra
.\make all
```

Build with:

* `x64` — 64-bit binaries
* `x86` — 32-bit binaries
* `all` — both architectures

## Usage

Mavra provides five executables (or three when compiled for x86):

`ml`, `ml64`, `link`, `link64`, and `masm`.

`ml`, `ml64`, and `link` wrap the MASM toolchain and automatically provide the required library paths. They also include additional Mavra-specific options, documented under `/help`.

`masm` behaves similarly to `ml`, but can also compile in 64-bit mode using the `/x64` argument.

In short: if you know how to use MASM, you already know how to use Mavra.

> **Note:** The binaries provided are **not** drop-in replacements for the original MASM binaries. They just have the same name for convenience.

## Configuring SDKs

When linking a MASM program, the linker normally relies on library directories provided by the Visual Studio and Windows SDK installations. Mavra handles this automatically by scanning your system for Windows Kits and Visual Studio installations and, by default, selecting the first compatible versions it finds.

To see the SDKs installed on your system, run:

```powershell
ml /listsdks
```

For example:

```text
Windows SDK installations:
[1] -- C:\Program Files (x86)\Windows Kits\10\ -- 10.0.26100.0

Visual Studio installations:
[1] -- C:\Program Files\Microsoft Visual Studio\18\Community -- Visual Studio Community 2026 -- 18.9.12120.119
        MSVC installations:
        [1] -- 14.51.36231
```

You can then select specific SDK and toolchain versions using the following case-insensitive options:

```text
/ListSDKs
/SetWinSDK:<int>
/SetVS:<int>
/SetMSVC:<int>
```

For example:

```powershell
masm /setwinsdk:1 /setmsvc:1 /setvs:1
```

```text
selected Windows Kit 10.0.26100.0.
selected MSVC 14.51.36231 from Visual Studio Community 2026 -- 18.9.12120.119.
selected Visual Studio Community 2026 -- 18.9.12120.119.
```

> **Note:** Each Visual Studio installation includes its own MSVC toolchain.

## Namesake

The name comes from Mavra Fominishna, a character mentioned in passing in Dostoevsky's *The Brothers Karamazov*.

It's also a portmanteau of **MASM** and **Wrapper**.

## License

This project is licensed under the [MIT License](https://github.com/tudor14x/mavra/blob/main/LICENSE).
