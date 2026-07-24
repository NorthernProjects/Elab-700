# E-Lab 700

Application de microscopie numérique pour une salle de classe (élèves de 10 à
12 ans), pensée comme une alternative plus simple et plus moderne à ToupView,
tout en restant du niveau d'un logiciel professionnel.

- **Plateformes :** Windows 11, macOS (11 Big Sur et plus récent)
- **Matériel visé :** microscope trinoculaire OMAX + caméra USB OMAX/ToupTek
  (identifiant caméra observé dans ToupView : `SCMOS05000KPA`, résolution
  1280x1024 — à confirmer sur votre poste, voir plus bas)

## Architecture

```
src/
  core/
    CameraTypes.h        types de données partagés (CameraFrame, CameraDeviceInfo, ...)
    CameraBackend.h       interface abstraite (contrat que TOUT backend doit respecter)
    NullCameraBackend.*    backend "aucune caméra" — ne simule jamais d'image
    DirectShowCameraBackend.*  backend réel : capture via OpenCV/DirectShow
                           (cv::CAP_DSHOW), voir "Caméra du microscope" plus bas
    CameraManager.*        possède le backend actif, détecte/ouvre la caméra
    CaptureManager.*       enregistrement photo (OpenCV imwrite) et vidéo (OpenCV VideoWriter)
    GalleryModel.*         liste les fichiers déjà enregistrés
    AppSettings.*          préférences persistantes (QSettings) : dossier, PIN, verrouillage
  ui/
    MainWindow.*           fenêtre principale élève (vidéo plein écran + barre du bas)
    VideoView.*             affichage du flux vidéo (ou message "aucune caméra")
    BottomBar.*             Photo / Vidéo / Auto / Galerie / Plein écran
    TopStatusBar.*          connexion / résolution / fps / bouton discret mode professeur
    TeacherPanel.*          réglages avancés, protégés par code PIN
    GalleryDialog.*         galerie locale des captures
    IdleScreen.*            écran de veille (logo) après inactivité
```

### Pourquoi une interface `CameraBackend` ?

L'UI et `CaptureManager` ne connaissent **que** `CameraBackend`, jamais un SDK
particulier. Pour ajouter demain une caméra UVC générique ou une carte de
capture HDMI, il suffit d'écrire une nouvelle classe qui implémente
`CameraBackend` (voir `NullCameraBackend` comme exemple minimal) — aucune
modification de l'UI n'est nécessaire.

Aucun backend ne doit jamais fabriquer d'image : si aucune caméra réelle n'est
ouverte, `isOpen()` doit renvoyer `false` et aucun signal `frameReady` ne doit
être émis. C'est ce qui permet à `VideoView` d'afficher honnêtement "Aucune
caméra détectée" plutôt qu'un flux simulé.

## Prérequis pour compiler

- CMake ≥ 3.21
- Un compilateur C++17 (MSVC 2022 sur Windows, ou MinGW)
- Qt 6 (modules Widgets, Multimedia, Concurrent) — installable via le
  Qt Online Installer ou `winget install Qt.Qt` selon votre configuration
- OpenCV ≥ 4.x (modules core, imgcodecs, imgproc, videoio)

Rien d'autre n'est nécessaire pour compiler : aucun SDK propriétaire, aucune
DLL tierce à fournir au build. Le backend caméra réel passe uniquement par
OpenCV (voir la section suivante).

## Caméra du microscope : pourquoi pas le SDK ToupCam ?

Le microscope embarque une caméra `SCMOS05000KPA` (ToupTek/OMAX) livrée avec
un SDK propriétaire (ToupCam) et son viewer (ToupView). En développant cette
appli, on a testé le SDK ToupCam directement (`Toupcam_EnumV2`/`Toupcam_Open`,
chargés dynamiquement depuis `toupcam.dll` pour ne rien committer de
propriétaire) : la DLL se charge bien, mais son énumération renvoie
systématiquement 0 caméra sur ce matériel/pilote, alors que ToupView affichait
un vrai flux au même moment. Explication trouvée après investigation : une
fois le pilote (`toupcam.sys`, fourni par OMAX/ToupTek) installé, Windows
expose cette caméra comme un périphérique **DirectShow standard** (même
classe PnP "Camera" qu'une webcam intégrée), pas via le protocole propriétaire
que `Toupcam_EnumV2` attend. Un test isolé avec `cv::VideoCapture(index,
cv::CAP_DSHOW)` a confirmé qu'elle répond parfaitement comme une webcam UVC
classique (résolution native 2592×1944 acceptée).

`DirectShowCameraBackend` s'appuie donc uniquement sur OpenCV/DirectShow,
déjà une dépendance du projet — plus simple et plus fiable ici que de deviner
l'ABI exacte d'un SDK propriétaire. Pour activer la vraie caméra :

1. **Installer le pilote Windows de la caméra** au moins une fois (sinon
   Windows ne reconnaît même pas le périphérique USB) : le plus simple est
   d'installer ToupView (CD/clé USB du microscope OMAX, ou support
   OMAX/ToupTek avec la référence `SCMOS05000KPA`), qui pose le pilote. On
   peut aussi passer uniquement par le Gestionnaire de périphériques →
   Mettre à jour le pilote → pointer vers le dossier `drivers/x64` du CD/clé.
2. **Après l'installation du pilote, débrancher/rebrancher physiquement le
   câble USB** de la caméra (une simple réinstallation depuis le Gestionnaire
   de périphériques ne suffit pas toujours à faire réapparaître le
   périphérique en état "OK" — vérifiable dans le Gestionnaire de
   périphériques, ou avec `Get-PnpDevice` en PowerShell).
3. **Vérifier.** Une fois le pilote actif et l'app relancée, le message
   "Aucune caméra détectée" doit disparaître et le flux vidéo s'afficher.
   Si une autre application (ToupView, un autre logiciel de capture) a la
   caméra ouverte en même temps, fermez-la d'abord : l'accès est exclusif.

`DirectShowCameraBackend.cpp` documente en commentaire pourquoi certains
réglages (exposition auto, etc.) restent "au mieux" : OpenCV n'expose pas de
façon portable les plages natives de chaque propriété DirectShow.

## Compiler (Windows, sans le SDK, pour tester l'UI immédiatement)

```powershell
cmake -S . -B build
cmake --build build --config Release
build\Release\E-Lab700.exe
```

L'application démarre, affiche "Aucune caméra détectée" (comportement
attendu et honnête, aucune image simulée), et tous les boutons/l'interface
professeur restent utilisables pour la démonstration et les tests d'UI.

## Fonctionnalités de la version 1

- Détection automatique de la caméra connectée (aucune configuration élève)
- Flux vidéo en direct, plein écran quasi total
- Message clair si aucune caméra n'est détectée (jamais d'image simulée)
- Photo, Vidéo, Plein écran, Auto (exposition + balance des blancs), galerie
  locale des captures
- Zoom numérique +/- (boutons discrets en bas à gauche) : recadre et zoome
  l'image capturée à la demande. Ne change pas le champ de vision réel de la
  caméra (fixé par l'optique/l'adaptateur C-mount du microscope — le port
  caméra d'un trinoculaire a presque toujours un champ plus étroit que
  l'oculaire, ce n'est pas ajustable en logiciel)
- Réglage de luminosité et mode professeur (exposition, gain, balance des
  blancs, résolution, dossier d'enregistrement, verrouillage du mode élève,
  changement du code PIN, délai de mise en veille)
- Balance des blancs "Auto" calibrée à la demande : vise une zone neutre/
  claire au moment du clic, mesure les couleurs de cette image précise et
  applique une correction fixe ensuite (plus fiable que la propriété
  DirectShow `CAP_PROP_AUTO_WB`, très inégale selon les pilotes)
- Écran de veille : après une période d'inactivité (réglable, 5 min par
  défaut, désactivable), un écran noir avec le logo affiché en fondu remplace
  le flux vidéo ; tout mouvement de souris, clic ou touche le referme
- Logo de l'établissement dans la barre du haut et comme icône de
  l'application (voir `resources/logo_ruelle_avenir.png` /
  `resources/logo_icon.png` — à remplacer si l'identité visuelle change)
- Écran de démarrage animé (`SplashScreen`) : symboles scientifiques qui
  dérivent en fond, logo qui bat comme un cœur pendant le chargement, durée
  fixe et volontairement indépendante de l'état de `MainWindow` (une version
  précédente essayait de synchroniser les deux et a fini par cacher une
  fenêtre modale derrière ce splash toujours au premier plan, gelant l'appli
  sans rien afficher — ne pas réintroduire ce couplage)
- Connexion classe/groupe via le bouton "Se connecter" dans la barre du
  haut (pas de fenêtre bloquante au démarrage) : chaque groupe a son propre
  sous-dossier de captures/galerie, protégé par un mot de passe **partagé
  par classe** (pas une vraie authentification par élève, juste de quoi
  éviter qu'un groupe se retrouve dans la galerie d'un autre par erreur).
  Configuré par l'enseignant dans Mode professeur → "Gérer les classes et
  groupes..." (jusqu'à 6 groupes par classe, avec courriel et mot de passe).
  Si aucune classe n'est configurée, le bouton affiche un message l'indiquant
- Galerie → "Envoyer par courriel..." (photos uniquement) : ouvre le client
  de messagerie par défaut (Outlook, etc. via MAPI) avec le courriel de
  l'enseignant de la classe active et la photo déjà en pièce jointe ;
  l'utilisateur clique lui-même sur Envoyer dans son client
- Affichage noir et blanc en option (Mode professeur → Affichage)
- Interface tactile et souris, gros boutons, mode sombre futuriste, aucun
  menu technique visible sur l'écran principal

## Code PIN du mode professeur

Par défaut `1234`, stocké haché (SHA-256) via `QSettings`. Changeable depuis
le mode professeur lui-même (section "Sécurité et veille") — nul besoin de
toucher au code.

## Installeur Windows (distribution sur d'autres postes)

Pour installer l'appli sur n'importe quel PC de la classe comme un vrai
logiciel Windows (raccourci menu Démarrer, icône Bureau optionnelle,
désinstalleur), un script [Inno Setup](https://jrsoftware.org/isinfo.php)
est fourni dans `installer/MicroscopeLab.iss`.

1. Compiler l'appli en Release (voir plus haut) et s'assurer que
   `build/Release/` contient bien `E-Lab700.exe` + toutes les DLL
   Qt/OpenCV (déployées via `windeployqt` + copie de `opencv_world*.dll`,
   voir la section Compiler).
2. Installer Inno Setup (`winget install JRSoftware.InnoSetup`, aucun droit
   admin requis).
3. Générer l'installeur :
   ```powershell
   & "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" installer\MicroscopeLab.iss
   ```
   Le résultat (`E-Lab700-Setup-<version>.exe`) est écrit dans `dist/`.
4. Distribuer ce seul fichier. Il s'installe **sans droits administrateur**
   (`PrivilegesRequired=lowest` dans le script) directement dans le profil de
   l'utilisateur (`%LocalAppData%\Programs\E-Lab 700`) — ça fonctionne
   donc "tel quel" sur n'importe quel poste de la classe, même sans compte
   admin/IT, exactement comme demandé.

Ce fichier `.exe` d'installation n'est pas committé dans le dépôt (c'est un
artefact de build, comme `build/`) : il se régénère à partir du code source
avec les étapes ci-dessus.

## Un seul exécutable, trois éditions (au choix, à l'exécution)

Depuis la version 2.0, il n'y a plus qu'UN exécutable et UN installeur. Au
premier démarrage, l'application demande quelle édition utiliser — toutes
gratuites, changeables à tout moment dans les réglages ("Édition du
logiciel") :

- **Scolaire (salle de classe)** : classes et groupes avec galeries
  séparées, code PIN professeur, verrouillage élève, minuteur d'observation,
  envoi des photos à l'enseignant, schéma du microscope et glossaire.
- **Grand public** : simple et complet — photo/vidéo/time-lapse, galerie,
  zoom, analyse (comptage/mesures), schéma et glossaire.
- **Laboratoire** : analyse, format photo TIFF/PNG/JPG, métadonnées de
  capture (.txt), étalonnage de l'échelle.

Le socle est commun (dont l'outil Analyse, disponible partout) ; chaque
édition active ses options par défaut, et la section "Fonctionnalités
supplémentaires" des réglages permet d'emprunter à une autre édition
(ex. activer le minuteur scolaire dans l'édition laboratoire). Le nom du
microscope affiché dans la barre du haut est libre (réglages → "Nom du
microscope").

Implémentation : `AppSettings::appMode()` + drapeaux `features/*` (runtime,
persistés via QSettings) — les anciennes options CMake `E_LAB_PUBLIC_BUILD`
et `E_LAB_INDIVIDUAL_BUILD` n'existent plus. Les réglages d'une ancienne
installation "école" (organisation `RuelleDeLAvenir`) sont migrés
automatiquement au premier lancement vers `ELab700Community` (roster de
classes, PIN...), sans écraser quoi que ce soit d'existant.

```powershell
cmake -S . -B build
cmake --build build --config Release
& "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" installer\MicroscopeLab.iss
```

(`installer/MicroscopeLabPublic.iss` et `MicroscopeLabIndividual.iss` sont
les anciens scripts des variantes séparées, conservés pour référence.)

Distribué sous licence MIT (voir `LICENSE`) — libre à vous de le partager, le
modifier, l'adapter à votre propre microscope.

## Compiler pour macOS

Le code est cross-platform (`UvcCameraBackend` utilise DirectShow sur Windows
et AVFoundation sur macOS via OpenCV ; `MailSender_mac.mm` remplace le MAPI
Windows par `NSSharingService`), mais **produire les .dmg nécessite de
compiler sur un vrai Mac** — ni CMake ni Qt ne se cross-compilent
correctement pour macOS depuis Windows. Deux façons de l'obtenir :

### Automatiquement, via GitHub Actions (recommandé)

Le fichier `.github/workflows/macos-build.yml` construit les **4 .dmg**
(École × Open Source, Intel × Apple Silicon) sur de vraies machines Mac
prêtées gratuitement par GitHub, à chaque push sur `main` ou à la demande
(onglet *Actions* → *Build macOS installers* → *Run workflow*) :

**Note sur le build Intel (x86_64)** : les 4 jobs tournent tous sur des
runners Apple Silicon (`macos-14`) — les runners Intel hébergés par GitHub
(`macos-13`) sont restés bloqués en file d'attente indéfiniment lors du
premier essai (GitHub réduit fortement cette capacité depuis qu'Apple ne
vend plus de Mac Intel). Le build x86_64 est donc obtenu par
**cross-compilation depuis le runner arm64** : un second Homebrew "Intel"
est installé sous Rosetta 2 dans `/usr/local`, et `CMAKE_OSX_ARCHITECTURES`
force Clang à produire du code x86_64. Si Homebrew ne trouve pas de bottle
précompilée x86_64 pour Qt/OpenCV sur l'image macOS du runner, il les
recompile depuis les sources — dans ce cas le job peut prendre 1 à 3 heures
au lieu de quelques minutes (à surveiller sur les premières exécutions).

1. Créer un dépôt GitHub (public de préférence — les minutes macOS des
   runners gratuits sont illimitées sur un dépôt public, mais comptées avec
   un facteur ×10 sur un dépôt privé).
2. Pousser ce projet dedans (`git remote add origin <url>` puis
   `git push -u origin main`).
3. Onglet **Actions** du dépôt : le workflow se lance automatiquement ;
   les 4 `.dmg` apparaissent en bas de la page d'exécution, section
   *Artifacts*, une fois terminé (15-25 min).

**Important — à valider avant diffusion** : ce pipeline a été écrit et câblé
depuis Windows, sans aucun Mac pour le tester. La première exécution est donc
le premier vrai test — vérifiez qu'un `.dmg` s'installe et fonctionne
réellement sur un Mac avant de le partager. Sans compte développeur Apple
payant (99 $/an), l'app n'est signée qu'en *ad-hoc* : au premier lancement,
macOS affichera "développeur non identifié" — clic droit sur l'app → *Ouvrir*
(une seule fois) débloque l'application définitivement.

### Manuellement, sur un Mac

```bash
brew install qt@6 opencv dylibbundler
iconutil -c icns resources/mac_iconset_public.iconset -o resources/AppIcon_public.icns   # ou _ruelle avec -DE_LAB_SCHOOL_BRANDING=ON
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)" -DOpenCV_DIR="$(brew --prefix opencv)/lib/cmake/opencv4"
cmake --build build --config Release
"$(brew --prefix qt@6)/bin/macdeployqt" build/Release/E-Lab700.app
dylibbundler -od -b -x build/Release/E-Lab700.app/Contents/MacOS/E-Lab700 \
  -d build/Release/E-Lab700.app/Contents/libs -p @executable_path/../libs
codesign --force --deep --sign - build/Release/E-Lab700.app
```

Puis glisser `E-Lab700.app` dans un `.dmg` (ou directement dans
`/Applications`) — voir le détail exact des étapes dans
`.github/workflows/macos-build.yml`.

## Limites connues / suite prévue

- Le pipeline macOS (voir ci-dessus) n'a encore jamais tourné sur un vrai Mac
  au moment de l'écriture — premier passage à valider.
- Un backend caméra UVC générique (webcam standard) ou carte de capture HDMI
  peut être ajouté en implémentant `CameraBackend`, sans toucher à l'UI.
- Pas de tests automatisés pour l'instant (l'essentiel de la logique métier
  — `CaptureManager`, `GalleryModel`, `AppSettings` — est découplé de Qt
  Widgets et testable unitairement si besoin).
