# Licence et distribution

Le code original de CyCLAPBridge (bibliothèque C++20, Studio, CLI, adaptateurs
et modèles de wrappers) est sous MIT, selon le fichier LICENSE à la racine.
AAX est exclu du produit. Aucun SDK AAX n'est nécessaire ou distribué.

## VST3

Le projet utilise le SDK VST3 **3.8.0 build 66**, révision
`9fad9770f2ae8542ab1a548a68c1ad1ac690abe0`, et ses sous-modules épinglés.
Leur licence est MIT. Cette version permet de conserver MIT pour le code de
CyCLAPBridge et des wrappers ; elle n'impose ni GPL ni publication du code source.
Les notices de copyright et les textes MIT doivent accompagner les copies
distribuées contenant ces composants.

Références :

- [Licence du SDK épinglé](https://github.com/steinbergmedia/vst3sdk/blob/9fad9770f2ae8542ab1a548a68c1ad1ac690abe0/LICENSE.txt)
- [FAQ officielle Steinberg](https://steinbergmedia.github.io/vst3_dev_portal/pages/FAQ/Licensing.html)

Cette conclusion concerne ces révisions ; elle ne s'étend pas aux anciens SDK
ou aux bibliothèques ajoutées ultérieurement. Les marques et logos VST ne sont
pas concédés par la licence MIT du code.

## Dépendances

MIT couvre le code original du projet. Les composants tiers gardent leurs
licences et copyrights, sans changement de licence :

| Composants | Licence |
| --- | --- |
| CLAP, clap-wrapper, VST3 et fmt | MIT |
| AudioUnitSDK et ysfx | Apache-2.0 |
| LV2, Lilv, Serd, Sord, Sratom et Zix | ISC |
| WDL et autres composants embarqués de ysfx | Zlib, MIT, ISC ou termes propres conservés |
| Parseur EEL2 généré | GPL-2.0-or-later avec Bison-exception-2.2 |

Le parseur conserve explicitement l'exception Bison : un programme plus vaste
qui contient ce squelette peut être distribué sous les termes de son choix,
sauf s'il est lui-même un générateur de parseurs réutilisant le squelette.
CyCLAPBridge est un hôte audio ; cette exception permet son utilisation ici sans
imposer la GPL au programme complet. Le parseur conserve sa propre notice.

Les textes complets, y compris les notices des composants embarqués de ysfx,
sont conservés dans `third_party/licenses` et les arbres upstream. THIRD_PARTY.md
donne les révisions et le manifeste de provenance demeure intact. Conserver les
notices de licence, copyright et attribution applicables lors de la distribution
des sources, de la bibliothèque statique ou de binaires qui l'embarquent.
L'installation fournit ces notices dans `share/CyCLAPBridge/licenses`, y compris
avec `CYCLAPBRIDGE_INSTALL_BACKEND=OFF` pour une intégration DAW sans les sources
des SDK de conversion.

## Wrappers et plugins sélectionnés

Les projets générés copient les notices du moteur et des SDK dans `licenses` ;
la compilation les place dans `CyCLAPBridge-licenses` à côté du plugin ou dans
ses ressources. Conserver ce dossier avec le wrapper et son payload.

Un wrapper adapte une API autour du plugin ou du script original. Il ne change
pas sa licence et ne donne pas le droit de le redistribuer. Les conditions de
l'éditeur continuent de s'appliquer au payload, aux ressources et aux scripts
JSFX. Un payload GPL peut donc ajouter des obligations à sa distribution ;
un payload propriétaire conserve ses restrictions. Le projet ne remplace pas
ces conditions par MIT.
