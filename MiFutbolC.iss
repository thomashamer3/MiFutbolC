; ================================
; MiFutbolC - Instalador Oficial
; Autor: Thomas Hamer
; ================================

#define MyAppName "MiFutbolC"
#define MyAppVersion "4.6.1"
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

; ================================
; PROTECCIÓN DE DATOS DEL USUARIO
; ================================

[Code]
const
  // Hasta la 4.3.1 el AppId tenía una llave de más, así que Windows la registraba
  // como otro programa, y su desinstalador borraba localappdata\MiFutbolC entero.
  ClaveInstalacionVieja =
    'Software\Microsoft\Windows\CurrentVersion\Uninstall\{B8F7E2E3-AD8C-5D9F-C8DA-234567890BCD}}_is1';

var
  BorrarDatosAlDesinstalar: Boolean;

function CarpetaDatos: String;
begin
  Result := ExpandConstant('{localappdata}\MiFutbolC\data');
end;

{ Copia las bases (*.db, *.db-wal, *.db-shm, users.db) a
  Documentos\MiFutbolC\Backups\<motivo>_<fecha>. Devuelve la carpeta, o '' si no
  había nada que copiar o falló. }
function CopiarDatosABackup(const Motivo: String): String;
var
  Busqueda: TFindRec;
  Destino: String;
  Copiados: Integer;
begin
  Result := '';
  if not DirExists(CarpetaDatos) then
    Exit;

  Destino := ExpandConstant('{userdocs}\MiFutbolC\Backups\') + Motivo + '_' +
             GetDateTimeString('yyyymmdd_hhnnss', #0, #0);
  if not ForceDirectories(Destino) then
    Exit;

  Copiados := 0;
  if FindFirst(CarpetaDatos + '\*.db*', Busqueda) then
  try
    repeat
      if (Busqueda.Attributes and FILE_ATTRIBUTE_DIRECTORY) = 0 then
        if CopyFile(CarpetaDatos + '\' + Busqueda.Name, Destino + '\' + Busqueda.Name, False) then
          Copiados := Copiados + 1;
    until not FindNext(Busqueda);
  finally
    FindClose(Busqueda);
  end;

  if Copiados > 0 then
    Result := Destino
  else
    RemoveDir(Destino);
end;

{ Instalación: si sigue registrada la versión vieja, copiar los datos, avisar y
  ofrecer quitarla de "Aplicaciones instaladas" junto con su desinstalador. }
procedure CurStepChanged(CurStep: TSetupStep);
var
  Version, Desinstalador, Copia: String;
begin
  if (CurStep <> ssPostInstall) or not RegKeyExists(HKCU, ClaveInstalacionVieja) then
    Exit;

  Copia := CopiarDatosABackup('instalacion');
  if WizardSilent then
    Exit;

  if not RegQueryStringValue(HKCU, ClaveInstalacionVieja, 'DisplayVersion', Version) then
    Version := 'anterior';
  if not RegQueryStringValue(HKCU, ClaveInstalacionVieja, 'UninstallString', Desinstalador) then
    Desinstalador := '';
  Desinstalador := RemoveQuotes(Desinstalador);

  if MsgBox('Se detectó otra instalación de MiFutbolC (versión ' + Version + ').' + #13#13 +
            'ATENCIÓN: el desinstalador de esa versión BORRA todos tus datos (partidos, ' +
            'camisetas, estadísticas), también los de esta versión.' + #13#13 +
            '¿Quitarla de "Aplicaciones instaladas" para que no se pueda desinstalar por error?' + #13 +
            'Tus datos y esta versión no se tocan.',
            mbConfirmation, MB_YESNO) = IDYES then
  begin
    RegDeleteKeyIncludingSubkeys(HKCU, ClaveInstalacionVieja);
    { Comparten carpeta: nunca borrar el desinstalador de esta versión. }
    if (Desinstalador <> '') and
       (CompareText(Desinstalador, ExpandConstant('{uninstallexe}')) <> 0) then
    begin
      DeleteFile(Desinstalador);
      DeleteFile(ChangeFileExt(Desinstalador, '.dat'));
    end;
  end;

  if Copia <> '' then
    MsgBox('Se guardó una copia de seguridad de tus datos en:' + #13 + Copia, mbInformation, MB_OK);
end;

{ Desinstalación: copia de seguridad siempre; los datos solo se borran si el
  usuario lo pide y lo confirma dos veces (por defecto, No). }
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Copia, Mensaje: String;
begin
  if CurUninstallStep = usUninstall then
  begin
    Copia := CopiarDatosABackup('desinstalacion');
    if UninstallSilent then
      Exit;

    Mensaje := 'Tus datos (partidos, camisetas, estadísticas) NO se borran: quedan en' + #13 +
               CarpetaDatos + #13 + 'y vuelven a aparecer si reinstalás MiFutbolC.';
    if Copia <> '' then
      Mensaje := Mensaje + #13#13 + 'Además se guardó una copia de seguridad en:' + #13 + Copia;
    Mensaje := Mensaje + #13#13 + '¿Querés BORRAR también tus datos de esta PC? (No recomendado)';

    if MsgBox(Mensaje, mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
      BorrarDatosAlDesinstalar :=
        MsgBox('Se borrarán todos tus datos de MiFutbolC de esta PC.' + #13 +
               'La copia de seguridad en Documentos se conserva.' + #13#13 + '¿Confirmás?',
               mbError, MB_YESNO or MB_DEFBUTTON2) = IDYES;
  end
  else if (CurUninstallStep = usPostUninstall) and BorrarDatosAlDesinstalar then
    DelTree(CarpetaDatos, True, True, True);
end;
