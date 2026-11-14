# Éditeurs de plugins

CyCLAPBridge fournit une API d'éditeur embarqué sans SDK audio ni toolkit GUI
dans les en-têtes publics. Le DAW possède la fenêtre parente et choisit le
processus qui héberge le plugin. Toutes les opérations d'éditeur et
`pumpMainThread()` s'exécutent sur le thread créateur, qui est le thread UI.
Sous macOS, ce thread doit aussi être le thread principal Cocoa.

| Backend | Éditeur | Périmètre |
| --- | --- | --- |
| CLAP | Extension clap.gui du plugin | Relais de l'API native, GUI embarquée |
| VST3 | IPlugView / IPlugFrame, HWND | Windows ; taille, contraintes, DPI si le plugin accepte cette extension |
| LV2 | ui:WindowsUI | Windows ; contrôle float, URID, instance/data access, port map, touch, resize et idle |
| JSFX | Section @gfx exécutée par ysfx/WDL/LICE | Windows ; bitmap BGRA, souris, clavier et menus natifs |
| AU | Fabrique AUCocoaUIBase / NSView | Sources Apple intégrées ; compilation et tests macOS reportés |

`editorSupported()` retourne false si le plugin ne fournit pas d'éditeur natif
compatible. Un JSFX sans @gfx n'a pas d'éditeur natif ; le DAW peut toujours
afficher ses sliders. Les UI LV2 Gtk/Qt ne sont pas adaptées par cette API.
Les messages Atom/patch des UI LV2, leurs dialogues complexes, le dépôt de
fichiers JSFX et les GUI VST3/LV2/JSFX hors Windows restent hors des chemins
validés. Un plugin peut conserver ses propres dépendances GUI externes.

Exemple dans le thread UI d'un DAW Windows, après création de HostedPlugin :

```cpp
cybridge::EditorCallbacks callbacks;
callbacks.context = myDawEditor;
callbacks.requestResize = acceptNativeSize; // bool(void*, EditorSize) noexcept
callbacks.parameterEdit = recordAutomation; // void(void*, id, value, gesture) noexcept

if (plugin.editorSupported(cybridge::WindowApi::win32) &&
    plugin.createEditor(cybridge::WindowApi::win32, callbacks)) {
    auto size = plugin.editorSize();
    // Le DAW dimensionne son parent et l'attache sur le même thread.
    if (plugin.setEditorParent({cybridge::WindowApi::win32, nativeParent}))
        plugin.showEditor(true);
    else
        plugin.destroyEditor();
}
// Depuis le timer UI du DAW, environ toutes les 33 ms :
plugin.pumpMainThread();
// Avant de détruire la fenêtre parente et le plugin :
plugin.destroyEditor();
```

Les callbacks sont synchrones sur le thread UI et ne doivent pas lever
d'exception. Leur contexte reste valide jusqu'au retour de destroyEditor().
Le callback de taille retourne false si le DAW refuse la nouvelle dimension.
Les tailles sont limitées à 8192 × 8192 ; les dimensions Cocoa sont logiques.
L'ouverture et la fermeture sont possibles pendant le traitement audio ; les
changements de préparation et la sauvegarde/restauration restent des opérations
de contrôle avec l'audio arrêté. Un éditeur LV2 est reconnecté si prepare()
recrée son instance DSP.

Les valeurs modifiées par l'éditeur sont coalescées par paramètre et injectées
au début du prochain bloc audio. Les événements du DAW conservent leur ordre
et leurs offsets ; une automation du DAW à l'offset zéro prend le pas sur la
valeur GUI du même bloc. Le chemin de fusion utilise un stockage préparé et
des atomiques lock-free. Les événements de geste en sortie de CLAP sont exposés
comme EventKind::gestureBegin/gestureEnd ; ils ne constituent pas des entrées
audio représentables par l'API commune.

L'état opaque de HostedPlugin inclut les valeurs GUI encore en attente, avec
un indicateur distinct du contenu natif du plugin. Les caches déjà consommés
ne remplacent pas l'état natif lors du rechargement. Les anciennes enveloppes
de façade CLAP et les états backend bruts restent lisibles.

La façade CLAP générée demande CLAP_EXT_TIMER_SUPPORT à l'hôte pour son éditeur,
puis relaie les appels GUI et les gestes d'automation. La file de gestes contient
1023 entrées utilisables ; un débordement demande un rescan des valeurs à l'hôte.
Les valeurs audio restent coalescées même lorsque cette file est saturée.
La bibliothèque CLAP directe distribue les timers depuis pumpMainThread().
Lorsqu'une instance CLAP est activée, le retour flushRequested/processRequested
demande au DAW de planifier le travail audio ; le pump UI n'exécute pas flush()
sur ce thread à la place du propriétaire audio.

Le rendu @gfx est du code de script étranger : il conserve son comportement,
ses allocations, ses accès aux fichiers et sa synchronisation. La garantie
du chemin préparé de l'adaptateur ne s'étend pas au script ni au plugin.

Références des API :

- [CLAP gui.h épinglé](https://github.com/free-audio/clap/blob/195b42a004144fab0b3cf95e9c067187d15365b7/include/clap/ext/gui.h)
- [IPlugView Steinberg](https://steinbergmedia.github.io/vst3_doc/base/classSteinberg_1_1IPlugView.html)
- [LV2 User Interfaces](https://lv2plug.in/ns/extensions/ui)
- [Vue native Audio Unit Apple](https://developer.apple.com/library/archive/documentation/MusicAudio/Conceptual/AudioUnitProgrammingGuide/TheAudioUnitView/TheAudioUnitView.html)
