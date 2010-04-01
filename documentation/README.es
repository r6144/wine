1. INTRODUCCI�N

Wine es un programa que permite la ejecuci�n de programas de Microsoft Windows
(incluyendo ejecutables de DOS, Windows 3.x y Win32) sobre Unix. Consiste en un
programa cargador que carga y ejecuta un binario de Microsoft Windows, y una
librer�a (llamada Winelib) que implementa las llamadas a la API de Windows
usando sus equivalentes Unix o X11. La librer�a puede tambi�n utilizarse para
portar c�digo Win32 a ejecutables Unix nativos.

Wine es software libre, publicado bajo la licencia GNU LGPL; vea el fichero
LICENSE para los detalles.

2. INICIO R�PIDO

Cuando compile desde el c�digo fuente, se recomienda utilizar el Instalador de
Wine para contruir e instalar Wine. Desde el directorio superior del c�digo de
Wine (el cual contiene este fichero), ejecute:

./tools/wineinstall

Ejecute programas con "wine [opciones] programa". Para m�s informaci�n y
resoluci�n de problemas, lea el resto de este fichero, la p�gina de manual de
Wine  y, espec�ficamente, la numerosa informaci�n que se encuentra en 
http://www.winehq.org.

3. REQUISITOS

Para compilar y ejecutar Wine, deber� tener uno de los siguientes:

  Linux versi�n 2.0.36 o superior
  FreeBSD 5.3 o superior
  Solaris x86 2.5 o superior
  NetBSD-current

Ya que Wine requiere soporte de hilos a nivel de n�cleo para ejecutarse, s�lo
se soportan los sistemas operativos arriba mencionados.
Otros sistemas operativos que soportan hilos de n�cleo pueden ser soportados en
el futuro.

Informaci�n de Linux:
  A pesar de que Linux 2.2.x deber�a funcionar todav�a y Linux 2.0.x a�n podr�a
  funcionar (versiones antiguas de 2.0.x ten�an problemas relacionados con los
  hilos), es mejor tener un n�cleo actual como los 2.4.x.

Informaci�n de FreeBSD:
  Wine deber�a construirse sobre FreeBSD 4.x y FreeBSD 5.x, pero versiones
  anteriores a FreeBSD 5.3 generalmente no funcionar�n adecuadamente.

  M�s informaci�n se puede encontrar en el �rbol de portes de FreeBSD en
  <ftp://ftp.freebsd.org/pub/FreeBSD/ports/ports/emulators/wine/>.

Informaci�n de Solaris:
  Lo m�s probable es que necesite construir con el conjunto de herramientas GNU
  (gcc, gas, etc.). Advertencia: el instalar gas *no* asegura que sea utilizado
  por gcc. Se dice que recompilar gcc tras la instalaci�n de gas o enlazar
  simb�licamente cc, as y ld a las herramientas gnu es necesario.

Informaci�n de NetBSD:
  Aseg�rese de que posee las opciones USER_LDT, SYSVSHM, SYSVSEM, y SYSVMSG
  activadas en su n�cleo.



Sistemas de ficheros soportados:
  Wine deber�a ejecutarse en la mayor�a de los sistemas de ficheros. Sin
  embargo, Wine no conseguir� iniciarse si umsdos es utilizado para el
  directorio /tmp. Unos cuantos problemas de compatibilidad se han reportado
  tambi�n al utilizar ficheros accedidos a trav�s de Samba. Adem�s, como de
  momento NTFS s�lo puede ser utilizado con seguridad con acceso de s�lo
  lectura, recomendamos no utilizar NTFS, ya que los programas Windows
  necesitan acceso de escritura en casi cualquier sitio. En el caso de ficheros
  NTFS, c�pielos a una localizaci�n escribible.

Requisitos b�sicos:
  Necesita tener los ficheros de inclusi�n de desarrollo de X11 instalados
  (llamados xlib6g-dev en Debian y XFree86-devel en RedHat).

Requisitos de herramienta de construcci�n:
  Sobre sistemas x86 se requiere gcc >= 2.7.2.
  Versiones m�s antiguas que la 2.7.2.3 pueden tener problemas cuando ciertos
  ficheros sean compilados con optimizaci�n, a menudo debido a problemas con el
  manejo de ficheros de cabecera. pgcc actualmente no funciona con Wine. La
  causa de este problema se desconoce.

  Por supuesto tambi�n necesita "make" (preferiblemente GNU make).

  Tambi�n necesita flex versi�n 2.5 o superior y bison. Si est� utilizando
  RedHat o Debian, instale los paquetes flex y bison.

Librer�as de soporte adicionales:
  Si desea soporte de impresi�n CUPS, por favor instale los paquetes cups y
  cups-devel.

4. COMPILACI�N

En el caso de que elija no utilizar wineinstall, ejecute los siguientes
comandos para construir Wine:

./configure
make depend
make

Esto construir� el programa "wine" y numerosas librer�as/binarios de soporte.
El programa "wine" cargar� y ejecutar� ejecutables de Windows.
La librer�a "libwine" ("Winelib") puede utilizarse para compilar y enlazar
c�digo fuente de Windows bajo Unix.

Para ver las opciones de configuraci�n para la compilaci�n, haga ./configure
--help.

Para actualizar a nueva versi�n usando un fichero de parches, primero haga cd
al directorio superior de la versi�n (el que contiene este fichero README).
Entonces haga un "make clean", y parchee la versi�n con:

    gunzip -c fichero-parche | patch -p1

donde "fichero-parche" es el nombre del fichero de parches (algo como
Wine-aammdd.diff.gz). Entonces puede volver a ejecutar "./configure", y luego
"make depend && make".

5. CONFIGURACI�N

Una vez que Wine ha sido construido correctamente, puede hacer "make install";
esto instalar� el ejecutable de wine, la p�gina de manual de Wine, y otros
cuantos ficheros necesarios.

No olvide desinstalar antes cualquier instalaci�n anterior de Wine conflictiva.
Intente "dpkg -r wine" o "rpm -e wine" o "make uninstall" antes de instalar.

Vea la zona de Soporte en http://www.winehq.org/ para consejos de
configuraci�n.

En el caso de que tenga problemas de carga de librer�as (p. ej. "Error while
loading shared libraries: libntdll.so"), aseg�rese de a�adir la ruta de las
librer�as a /etc/ld.so.conf y ejecutar ldconfig como root.

6. EJECUTANDO PROGRAMAS

Cuando invoque Wine, puede especificar la ruta completa al ejecutable, o s�lo
el nombre del fichero.

Por ejemplo: para ejecutar el Solitario:

  wine sol                   (usando la ruta de b�squeda indicada en el fichero
  wine sol.exe                de configuraci�n para encontrar el fichero)

  wine c:\\windows\\sol.exe  (usando la sintaxis de nombre de fichero de DOS)

  wine /usr/windows/sol.exe  (usando la sintaxis de nombre de fichero de Unix)

  wine sol.exe /parametro1 -parametro2 parametro3
                             (llamando al programa con par�metros)

Nota: la ruta del fichero tambi�n se a�adir� a la ruta cuando se proporcione un
      nombre completo en la l�nea de comandos.

Wine todav�a no est� completo, por lo que algunos programas pueden fallar. Si
configura winedbg correctamente de acuerdo con documentation/debugger.sgml,
entrar� en un depurador para que pueda investigar y corregir el problema.
Para m�s informaci�n sobre c�mo hacer esto, por favor lea el fichero
documentation/debugging.sgml.

Deber�a hacer copia de seguridad de todos sus ficheros importantes a los d�
acceso desde Wine, o utilizar una copia especial para Wine de ellos, ya que ha
habido algunos casos de usuarios reportando corrupci�n de ficheros. NO ejecute
Explorer, por lo tanto, si no posee una copia de seguridad adecuada, ya que
renombra/corrompe a veces algunos directorios. Tampoco otras aplicaciones MS
como p. ej. Messenger son seguras, ya que lanzan de alg�n modo Explorer. Esta
corrupci�n particular (!$!$!$!$.pfr) puede corregirse al menos parcialmente
utilizando http://home.nexgo.de/andi.mohr/download/decorrupt_explorer

7. OBTENIENDO M�S INFORMACI�N

WWW:    Una gran cantidad de informaci�n sobre Wine est� disponible en WineHQ
        en http://www.winehq.org/ : varias gu�as de Wine, base de datos de
        aplicaciones, registro de bugs. Este es probablemente el mejor punto de
        partida.

FAQ:    La FAQ de Wine se encuentra en http://www.winehq.org/FAQ

Usenet: Puede discutir sobre temas relacionados con Wine y obtener ayuda en
        comp.emulators.ms-windows.wine.

Bugs:   Reporte bugs al Bugzilla de Wine en http://bugs.winehq.org
        Por favor, busque en la base de datos de bugzilla para comprobar si su
        problema ya se encuentra antes de enviar un informe de bug. Puede
        tambi�n enviar informes de bugs a comp.emulators.ms-windows.wine.

IRC:    Hay disponoble ayuda online en el canal #WineHQ de irc.freenode.net.

CVS:    El �rbol actual de desarrollo de Wine est� disponible a trav�s de CVS.
        Vaya a http://www.winehq.org/cvs para m�s informaci�n.

Listas de correo:
        Hay varias listas de correo para desarrolladores de Wine; vea
        http://www.winehq.org/forums para m�s informaci�n.

Si a�ade algo, o corrige alg�n bug, por favor env�e un parche (en formato
'diff -u') a la lista wine-patches@winehq.org para su inclusi�n en la siguiente
versi�n.

--
Alexandre Julliard
julliard@winehq.org
