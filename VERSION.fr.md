# CyCLAPBridge — Notes de version

[README](README.fr.md) · [English](VERSION.md)

**Bibliothèque, Studio et CLI : 0.2.0 · EN DÉVELOPPEMENT**

## Implémentation actuelle

- Bibliothèque statique C++20 autonome, Studio Windows natif et CLI de conversion.
- Hébergement et wrappers CLAP/VST3/LV2/JSFX, traitement float32, événements représentables, latence et état.
- Éditeurs Windows embarqués CLAP, VST3 HWND, LV2 WindowsUI et JSFX `@gfx`, y compris dans les façades CLAP.
- Hébergement AUv2, sources d’éditeur Cocoa et projets d’export AUv2/AUv3 ; validation Apple reportée.
- Manuel anglais/français hors ligne et accès à l’aide dans Studio.

## Limites connues

- Cible native : Windows x64. Les compilations macOS et Linux ne sont pas validées ; les tests macOS sont reportés par le propriétaire.
- AAX est exclu. JSFX est réservé à l’entrée. AUv3 n’importe pas de fichiers arbitraires ; les exports demandent macOS, Xcode et la signature.
- L’API commune ne promet pas float64, SysEx, MIDI 2, modulation par voix ou traduction complète du transport.
- Les UI LV2 Atom/patch, l’adaptation Gtk/Qt, le dépôt de fichiers JSFX et les éditeurs VST3/LV2/JSFX hors Windows restent hors de l’intégration actuelle.
- Les DAW fournissent la supervision des processus, l’IPC et l’isolation de sécurité. Le code étranger conserve son comportement temps réel.
- Studio utilise des libellés français. Aucun binaire ni liste de compatibilité de production n’est publié.

Ces notes ne maintiennent pas de liste détaillée de bugs confirmés. Consulter le [suivi des problèmes](https://github.com/MrMybal/CyCLAPBridge/issues) ; l’accès au dépôt peut être nécessaire. Cela ne signifie pas qu’il n’existe aucun bug.
