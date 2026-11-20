---
id: studio
title: Créer un wrapper
description: Choisir une source et livrer un plugin compilé dans un autre format.
section: Manuel
parent: overview
order: 10
sourceRevision: 8
---

```image
{"id":"studio-interface","src":"assets/studio-fr.png","alt":"Formulaire Studio avec sélection des formats source et cible","controls":"minimal"}
```

## Choisir l’entrée {#input}

1. Ouvrir Studio et sélectionner le format source à gauche et le format cible à droite.
2. Utiliser **Fichier** pour un fichier de plugin ou une source JSFX, ou **Bundle** pour un dossier LV2/VST3/AU.
3. Choisir un nouveau dossier de destination et saisir le nom du wrapper. Les dossiers existants sont refusés pour éviter d’écraser des données.
4. Conserver le dossier `Resources` de l’application à côté de l’exécutable ; les composants de conversion sont inclus.

La conversion Apple demande macOS, et AUv3 demande Xcode. Ces cibles sont indisponibles dans le Studio Windows/Linux. La validation Apple reste reportée. Consulter [Formats](doc:formats) avant de choisir une direction.

## Inclure les ressources {#resources}

Le dossier facultatif de ressources/imports doit contenir l’entrée sélectionnée et ses fichiers associés. Leurs chemins relatifs sont conservés. Cela sert aux imports JSFX, aux images et aux ressources de plugins. Choisir une destination extérieure à ce dossier source. Les banques, les dépendances externes et les autorisations restent sous la responsabilité de l’éditeur du plugin.

## Générer et compiler {#build}

Choisir **Convertir**. Studio prépare ses composants, réalise la conversion en arrière-plan et livre le plugin compilé, son payload et ses notices dans le dossier de destination. Consulter le journal de conversion en cas d’échec. L’application est déjà compilée ; la conversion des plugins demande encore CMake 3.27+ et un compilateur C++20 natif. Sous Windows, installer Visual Studio 2022 avec Développement Desktop en C++ et un SDK Windows. Les emplacements CMake courants sont détectés automatiquement.

Pour une sortie LV2, la conversion lance un outil de métadonnées qui charge le plugin. Cet outil n’est pas une sandbox de sécurité.

Après réussite, **Ouvrir le résultat** ouvre le dossier des artefacts. Garder le wrapper, son `CyCLAPBridge-payload` et ses `CyCLAPBridge-licenses` ensemble. Rien n’est installé automatiquement dans les dossiers de plugins d’un DAW. Suivre la procédure normale de découverte du DAW pour installer le résultat complet.

## Choisir un plugin dans un module {#selection}

Studio utilise par défaut le premier plugin compatible d’un module multiple. Saisir son ID CLAP, son URI LV2 ou son ID de classe VST3 dans **ID du plugin (facultatif)** sous **Options avancées** pour sélectionner un plugin précis. La [CLI](doc:cli#selection) propose l’option équivalente `--plugin-id` ainsi que des options supplémentaires de bundle Apple.

## Langue, thème et outils {#settings}

Choisir l’anglais ou le français et un thème en haut. Les préférences sont conservées dans le dossier de configuration de l’utilisateur du système. Le navigateur de fichiers commun utilise les racines locales ; les chemins complets peuvent aussi être collés dans les champs. Les options avancées sélectionnent l’ID du plugin et permettent d’adapter CMake, le générateur, l’architecture et la configuration au besoin.

## Assistants dans Studio {#assistants}

1. Ouvrir **Tchat**, puis **Fournisseurs**. Sélectionner **Démo Markdown (sans IA)** et cliquer sur **Connecter / Nouveau tchat** pour essayer le panneau localement.
2. Pour une conversation avec une IA, utiliser **Détecter les fournisseurs** pour repérer les CLI installées, les clés API configurées et les services locaux démarrés, ou choisir un fournisseur manuellement. Sélectionner la connexion et se connecter. La réponse s’affiche au fil de sa génération et **Arrêter** interrompt la réponse en cours. Un fournisseur CLI utilise le programme installé et déjà authentifié. Un fournisseur API utilise la clé de la variable d’environnement indiquée, si elle est nécessaire. Pour un moteur local installé, utiliser l’adresse de son service. Si Studio propose une liste de modèles, choisir un modèle et se reconnecter.
3. Avec un fournisseur compatible, laisser **Donner à ce tchat accès aux outils de Studio** coché lors de la connexion. Demander une inspection ou une conversion et donner les chemins utiles. Vérifier la demande dans Studio, puis choisir **Exécuter la demande** ou **Refuser**. N’approuver que les fichiers de confiance, car l’inspection et certaines conversions exécutent le code du plugin.

Pour un assistant externe sur la même machine, ouvrir **Assistants externes (MCP)**, choisir un profil et démarrer le serveur. **Copier la connexion MCP** fournit l’adresse locale. Pour un profil à jeton, la copie contient aussi l’en-tête Bearer.

| Profil | Jeton Bearer | Confirmation des inspections et conversions |
| --- | --- | --- |
| **Free** (par défaut) | Non | Non |
| **Local contrôlé** | Non | Oui |
| **Jeton seul** | Oui | Non |
| **Protégé** | Oui | Oui |

Le serveur est arrêté au démarrage de Studio. L’arrêter avant de changer de profil. Studio mémorise le choix dans `Studio.ini` sous `mcp_access_profile`. Les préférences sans profil utilisent Free. Le serveur écoute sur `127.0.0.1` pour les processus de la même machine. Garder les jetons privés et recopier la connexion après un redémarrage.

Cocher **Enregistrer l’historique sur cet ordinateur** avant de se connecter pour enregistrer chaque tour sous un nouvel ID. Pour reprendre la conversation, sélectionner le même fournisseur, saisir cet ID et choisir **Restaurer le tchat enregistré**.

| Outil | Fonction accessible à l’assistant |
| --- | --- |
| `list_formats` | Lire les formats d’inspection et les directions de conversion disponibles sur cette plateforme. |
| `inspect_plugin` | Demander les métadonnées et le nombre de bus audio et de paramètres d’un fichier, d’un bundle ou d’un script JSFX. |
| `convert_plugin` | Demander un wrapper compilé. Fournir les formats source et cible, un chemin d’entrée absolu et un nouveau dossier de sortie absolu. Les champs facultatifs sélectionnent un ID de plugin, un nom de wrapper, un dossier de ressources ou un identifiant de bundle Apple. |
| `get_job` | Suivre une demande par ID et lire son résultat, son erreur ou le journal de compilation. |

L’inspection et la conversion renvoient un ID dès la demande. Une conversion réussie livre les mêmes fichiers que **Convertir** dans Studio, avec le wrapper, le payload et ses notices. Le fournisseur et la connexion MCP restent en mémoire pendant la session de Studio.

### Périmètre des outils MCP {#assistant-scope}

Le tchat intégré et les deux profils avec confirmation affichent l’entrée, la destination et les options. Choisir **Exécuter la demande** pour les valider. Les autres profils mettent automatiquement les demandes externes en file. Tous les profils valident les arguments des outils. **Refuser** laisse l’entrée et la destination intactes. Une conversion lancée suit le parcours habituel de Studio. Les conversions en cours ne proposent ni annulation ni reprise. Studio garde en mémoire les huit derniers résultats de demandes d’assistants pendant la session.

La sélection de fichiers, l’ouverture du résultat dans l’explorateur et l’apparence restent des actions de l’interface. Les outils prennent des chemins explicites et renvoient le dossier de sortie au client. Studio reçoit les appels MCP pour ses propres fonctions audio. L’inspection et la conversion sont autonomes et ne nécessitent pas l’importation d’outils d’autres applications.

Le catalogue complet des quatre outils de Studio est transmis à l’assistant. Les clients MCP peuvent mettre leurs schémas en cache pendant 30 secondes. L’état d’un job est relu à chaque appel. Studio conserve l’historique complet et ne configure pas de fenêtre de contexte pour le modèle. Les journaux de conversion bornés et le catalogue fixe ne nécessitent ni stockage de ressources, ni recherche de fichiers, ni abonnement aux changements, ni journal de performance.
