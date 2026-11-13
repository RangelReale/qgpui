; Inno Setup script for the QGPUI installer. build-installer.ps1 runs it; it passes:
;   AppVersion  the version, such as 1.0.0
;   StageDir    the folder that `cmake --install` filled
;   OutputDir   where to write qgpui-<version>-setup.exe

#ifndef AppVersion
  #error Pass /DAppVersion=<version>
#endif
#ifndef StageDir
  #error Pass /DStageDir=<folder>
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif

[Setup]
; Never change AppId: Windows uses it to find an installed QGPUI to upgrade.
AppId={{6C942AF0-0362-4415-A076-EB576B3A2210}
AppName=QGPUI
AppVersion={#AppVersion}
AppVerName=QGPUI {#AppVersion}
AppPublisher=Rangel Reale
AppPublisherURL=https://github.com/RangelReale/qgpui
AppSupportURL=https://github.com/RangelReale/qgpui/issues
AppUpdatesURL=https://github.com/RangelReale/qgpui/releases
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\QGPUI
DisableProgramGroupPage=yes
; Install for the current user without asking for administrator rights; the user can still
; choose an install for all users.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=qgpui-{#AppVersion}-setup
SetupIconFile=..\..\apps\qgpui\icons\qgpui.ico
UninstallDisplayIcon={app}\qgpui.exe
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
; QGPUI and the adb server it starts keep files in {app} open; ask to close them.
CloseApplications=yes

[Tasks]
Name: desktopicon; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\QGPUI"; Filename: "{app}\qgpui.exe"
Name: "{autodesktop}\QGPUI"; Filename: "{app}\qgpui.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\qgpui.exe"; Description: "{cm:LaunchProgram,QGPUI}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; Stop the adb server that QGPUI started from {app}, so that its files can be removed.
Filename: "{app}\platform-tools\adb.exe"; Parameters: "kill-server"; Flags: runhidden skipifdoesntexist; RunOnceId: "KillAdb"
