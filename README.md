# QuickInstaller
#### QuickInstaller is simple ps vita tool which let you install vpk via qcma.
## Requirements
#### Windows & .NET Framework 4.5 or above. (OS X and Linux are not officialy supported yet. feel free to test via mono.)
#### QCMA https://codestation.github.io/qcma/
## Instructions
#### Create a directory named 'whatever'.
#### Extract downloaded zip file into 'whatever'.
#### Create a directory named 'vpk' in 'whatever'.
#### Put vpks into 'vpk' or extract to each directories. VPK files are not changed, extracted directories will be removed.
#### Run QuickInstallerPC.exe. ('work' and 'mp4' directories are automatically purged.)
#### Copy every files in 'mp4' to VITA via qcma.
#### Install QuickInstaller.vpk on your VITA. (you don't need to this everytime.)
#### Run QuickInstaller.
#### Wait for "Done." is showing up. (or can be failed. :P)
## Trouble shooting
#### Failed to convert vpk to mp4 - just extract it and put contents into new directory (for example, extract Hastune_Miku_Project_Diva_F.vpk into vpk/hmpd/VPK_CONTENT_HERE)
#### Failed to copy mp4 files - remove every directories in ux0:video and try again.
#### Failed to install package. - content might not be vpk or broken. (for example, mai dumps cannot be installed directly yet.)
