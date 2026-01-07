; ================================
; MiFutbolC - Instalador Oficial
; Autor: Thomas Hamer
; ================================

#define MyAppName "MiFutbolC"
#define MyAppVersion "4.0"
#define MyAppPublisher "Thomas Hamer"
#define MyAppURL "https://github.com/thomashamer3/MiFutbolC"
#define MyAppExeName "MiFutbolC.exe"

[Setup]
AppId={{B8F7E2E3-AD8C-5D9F-C8DA-234567890BCD}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
LicenseFile=LICENSE
UninstallDisplayIcon={app}\MiFutbolC.ico

DefaultDirName={localappdata}\Programs\{#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest

OutputDir=installer
OutputBaseFilename=MiFutbolC_Setup
SetupIconFile=MiFutbolC.ico
Compression=lzma
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "Accesos directos"

; ================================
; ARCHIVOS DE LA APLICACIÓN
; ================================
[Files]
Source: "MiFutbolC.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "MiFutbolC.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "Manual_Usuario_MiFutbolC.pdf"; DestDir: "{app}"; Flags: ignoreversion
Source: "README.pdf"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE"; DestDir: "{app}"; Flags: ignoreversion

; ================================
; DIRECTORIOS DE DATOS (CLAVE)
; ================================
[Dirs]
Name: "{localappdata}\MiFutbolC\data"

; ================================
; ACCESOS DIRECTOS
; ================================
[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\MiFutbolC.ico"
Name: "{autoprograms}\Manual de Usuario"; Filename: "{app}\Manual_Usuario_MiFutbolC.pdf"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; IconFilename: "{app}\MiFutbolC.ico"

; ================================
; POST INSTALACIÓN
; ================================
[Run]
Filename: "{app}\Manual_Usuario_MiFutbolC.pdf"; Description: "Abrir manual de usuario"; Flags: postinstall shellexec
Filename: "{app}\{#MyAppExeName}"; Description: "Ejecutar MiFutbolC"; Flags: nowait postinstall skipifsilent
