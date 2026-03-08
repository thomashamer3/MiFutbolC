; ================================
; MiFutbolC - Instalador Oficial
; Autor: Thomas Hamer
; ================================

#define MyAppName "MiFutbolC"
#define MyAppVersion "3.8"
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

; --- Runtime DLLs required by MiFutbolC.exe (verified via objdump) ---
Source: "bin\Debug\libhpdf.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "bin\Debug\libpng16-16.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "bin\Debug\zlib1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "bin\Debug\libqrencode.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "bin\Debug\libncursesw6.dll"; DestDir: "{app}"; Flags: ignoreversion

; The following DLLs are *not* directly required by MiFutbolC.exe and are removed
; from the installer to reduce package size:
; - libwinpthread-1.dll
; - libintl-8.dll
; - libgcc_s_seh-1.dll
; - libstdc++-6.dll
; - comctl32.dll
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
Name: "{autoprograms}\README"; Filename: "{app}\README.pdf"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; IconFilename: "{app}\MiFutbolC.ico"

; ================================
; POST INSTALACIÓN
; ================================
[Run]
Filename: "{app}\Manual_Usuario_MiFutbolC.pdf"; Description: "Abrir manual de usuario"; Flags: postinstall shellexec
Filename: "{app}\{#MyAppExeName}"; Description: "Ejecutar MiFutbolC"; Flags: nowait postinstall skipifsilent
