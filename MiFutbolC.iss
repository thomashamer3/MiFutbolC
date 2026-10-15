; ================================
; MiFutbolC - Instalador Oficial
; Autor: Thomas Hamer
; ================================

#define MyAppName "MiFutbolC"
#define MyAppVersion "4.5.1"
#define MyAppPublisher "Thomas Hamer"
#define MyAppURL "https://github.com/thomashamer3/MiFutbolC"
#define MyAppExeName "MiFutbolC.exe"

[Setup]
AppId={{B8F7E2E3-AD8C-5D9F-C8DA-234567890BCD}
AppName={#MyAppName}
AppVersion={#MyAppVersion}

AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

DefaultDirName={localappdata}\Programs\{#MyAppName}
DefaultGroupName={#MyAppName}

UninstallDisplayIcon={app}\MiFutbolC.ico
LicenseFile=LICENSE

PrivilegesRequired=lowest
DisableProgramGroupPage=yes

OutputDir=installer
OutputBaseFilename=MiFutbolC_Setup
SetupIconFile=MiFutbolC.ico

Compression=lzma2
SolidCompression=yes
InternalCompressLevel=max

WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64os
ArchitecturesAllowed=x64compatible

; Cerrar la app si está en ejecución antes de reemplazar archivos,
; pero no reabrirla automáticamente al terminar.
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "Accesos directos"

; ================================
; ARCHIVOS
; ================================

[Files]
Source: "MiFutbolC.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "MiFutbolC.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "Manual_Usuario_MiFutbolC.pdf"; DestDir: "{app}"; Flags: ignoreversion
Source: "README.pdf"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "langs\*"; DestDir: "{app}\langs"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; ================================
; DIRECTORIOS
; ================================

[Dirs]
; La carpeta de datos del usuario (bases .db y users.db) NUNCA se borra al
; desinstalar para no perder partidos, camisetas, perfiles ni estadísticas.
Name: "{localappdata}\MiFutbolC\data"; Flags: uninsneveruninstall

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

Filename: "{app}\Manual_Usuario_MiFutbolC.pdf"; Description: "Abrir manual de usuario"; Flags: postinstall shellexec skipifsilent

Filename: "{app}\{#MyAppExeName}"; Description: "Ejecutar MiFutbolC"; Flags: nowait postinstall skipifsilent

; ================================
; LIMPIEZA AL DESINSTALAR
; ================================
; El bloque [UninstallDelete] se eliminó a propósito: la carpeta
; "{localappdata}\MiFutbolC" contiene la base de datos del usuario y no debe
; borrarse al desinstalar. Solo se elimina lo instalado en {app}.
