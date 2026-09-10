# Mavra

Mavra is a simple no-nonsense wrapper around the [Microsoft Micro Assembler, or MASM](https://en.wikipedia.org/wiki/Microsoft_Macro_Assembler).
<br>
MASM is notorious for being unreasonably difficult to use, even in the simplest of use cases. You can't just call it through the CLI, like you would for any other program: you must first boot up the [Developer Command Prompt](https://learn.microsoft.com/en-us/visualstudio/ide/reference/command-prompt-powershell?view=visualstudio). Another option provided is to call scripts buried deep into the Visual Studio directory tree to set up the environment variables required for MASM, and you can only call it from the terminal session you've run the script in.  

The final official alternative to these two approaches is calling MASM through Visual Studio, which, to many, is a no-go, and understandably so, as no one should be forced to boot up an IDE to use an assembler.

Mavra is the alternative solution for Assembly Programming on Windows, letting you call MASM anywhere, anytime, no strings attached.

# Dependencies

[MinGW](https://www.mingw-w64.org/downloads/#mingw-w64-builds) 16.2.0  
[vswhere](https://github.com/microsoft/vswhere) 3.17

# Usage

Mavra provides five (or three, if you compile for x86) executables: `ml`, `ml64`, `link`, `link64`, and `masm`
<br><br>
`ml`, `ml64` and `link` wrap around the MASM toolchain, providing all the necessary library paths. There are also additional, Mavra-specific options included within `/help`.<br><br>
`masm` is functionally similar to `ml`, with the exception of being able to compile in 64-bit mode through the `/x64` argument.<br>

In short, if you know how to use MASM, you know how to use Mavra.

## Configuring SDKs

When you link a program with MASM, the linker fetches library directories behind the scenes so you can access the C runtime and other system libraries without much hassle. Mavra achieves the same thing, scanning your computer for Windows Kits and Visual Studio installations, then choosing, by default, to use the first ones it finds. 

To see the SDKs you have on your computer, call Mavra with the argument `/listsdks`.
<br>Of note: MSVC comes packed with each individual Visual Studio instance.
```
PS C:\Programming\mavra\bin> ml /listsdks
Windows SDK installations:
[1] -- C:\Program Files (x86)\Windows Kits\10\ -- 10.0.26100.0

Visual Studio installations:
[1] -- C:\Program Files\Microsoft Visual Studio\18\Community -- Visual Studio Community 2026 -- 18.9.12120.119
        MSVC installations:
        [1] -- 14.51.36231
```
<br>
You may then select the specific version of which SDK you want by use of the case-insensitive arguments here.

```
PS C:\Programming\mavra\bin> masm /help 

...
/ListSDKs List all of the SDKs (Windows SDK, Visual Studio, MSVC)
/SetWinSDK:<int> Sets the Windows SDK
/SetVS:<int> Sets the Visual Studio toolchain
/SetMSVC:<int> Sets the MSVC toolchain  
...
```
```
$ masm /setwinsdk:1 /setmsvc:1 /setvs:1
selected Windows Kit 10.0.26100.0.
selected MSVC 14.51.36231 from Visual Studio Community 2026 -- 18.9.12120.119.
selected Visual Studio Community 2026 -- 18.9.12120.119.
```

# Namesake
The name of the tool comes from Mavra Fominishna, a character mentioned in passing in Dostoevsky's "The Brothers Karamazov".   

It's also a portmanteau of "Masm" and "Wrapper".

# License
This project is licensed under the [MIT license](https://github.com/tudor14x/mavra/blob/main/LICENSE).