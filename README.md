# QuickInstaller
QuickInstaller is simple ps vita tool which let you install vpk via qcma.

## Requirements ##
- Windows & .NET Framework 4.5 or above. (OS X and Linux are not officialy supported yet. feel free to test via mono.)
- QCMA https://codestation.github.io/qcma/

## Instructions ##
- Create a directory named WHATEVER.
- Extract QuickInstaller to WHATEVER.
- Turn on VitaShell.
- Copy and Install QuickInstaller.vpk on your ps vita.
- Create a directory named 'vpk' in WHATEVER.
- Put vpks into 'vpk' directory.
- Run QuickInstallerPC.exe
- Turn on QCMA.
- Open settings and set video folder as WHATEVER/mp4.
- Plug cable and connect.
- Run cma app and copy mp4 files to ps vita.
- Run QuickInstaller app.
- Wait for 'done.' is showing up. (or fail)

## Known Issues ##
- Mai dumps are not supported yet.
- Can be slow if vpk contains a lot of small files.

## Trouble shooting ##
- Failed to convert vpk to mp4: extract manually and put directory into 'vpk' instead of vpk file.
- Failed to copy mp4 files: remove every directories in ux0:video and try again.
- Failed to install package: content might be broken.
