# Réseau du client : ce qu'il envoie, ce qu'il attend

Couvre le commit `265fc26` (*chore: restart Salah-eddine23's base*) jusqu'à `main` (`9067067`).
Code source : [ClientNetworkManager.hpp](../include/client/network/ClientNetworkManager.hpp),
[Protocol.hpp](../include/common/network/Protocol.hpp),
[ProtocolExtension.hpp](../include/client/network/ProtocolExtension.hpp),
[ServerNetworkManager.hpp](../include/server/ServerNetworkManager.hpp).
Le détail des règles de jeu proposées est dans [protocol-extension.md](protocol-extension.md).

## Format commun

- UDP, octets bruts copiés avec `memcpy` (little-endian sur x86), aucun padding.
- Chaque paquet commence par un header de **9 octets** :

| Champ       | Type | Valeur |
|-------------|------|--------|
| magic       | u16  | `0x5254` (sinon le paquet est ignoré) |
| type        | u8   | voir les tableaux ci-dessous |
| sequence    | u32  | compteur incrémenté à chaque envoi |
| payloadSize | u16  | taille du payload qui suit |

- `NetworkId` = `u32`, `float` = `f32`, `bool` = 1 octet.

## Ce que le client envoie (client → serveur)

| Paquet | Type | Payload | Quand |
|--------|------|---------|-------|
| `ConnectReq` | `0x01` | `difficulty u8`, `skin u8`, `previousNetId u32` (voir note) | Une seule fois, au lancement de la partie. **Pas de renvoi** si la réponse se perd. |
| `PlayerInput` | `0x20` | `netId u32`, `up`, `down`, `left`, `right`, `shoot` (5 × bool) = 9 octets | **À chaque frame**, dès que le `ConnectAck` est reçu. |
| `Disconnect` | `0x03` | `netId u32` | Quand on quitte la scène de jeu (si connecté). |

Note sur `ConnectReq` :

- `difficulty` : 0 = easy, 1 = normal, 2 = hard, `0xFF` = pas de préférence.
- `skin` : index du vaisseau dans `[skins] list` de `client.cfg`, `0xFF` = aucun.
- `previousNetId` : l'id reçu dans la partie précédente. Il est envoyé seulement quand on relance une partie (retour au menu après un game over), pour garder le même numéro de joueur.
- Le payload fait 0, 1, 2 ou 6 octets. Un champ n'est présent que si celui d'après l'est aussi (`0xFF` sert alors de bouche-trou).

Note sur `PlayerInput` :

- Si le joueur local est mort, il envoie tout à `false` (pour rester connecté).
- Le client **coupe lui-même** une direction qui pousserait le vaisseau hors de l'écran, parce que le serveur ne borne pas les positions.

Le client n'envoie jamais de `Ping`.

## Ce que le client attend (serveur → client)

### Minimum pour que le client marche

| Paquet | Type | Payload | Effet côté client |
|--------|------|---------|-------------------|
| `ConnectAck` | `0x02` | `netId u32` [+ `difficulty u8`] | Le client passe en "connecté" et commence à envoyer ses inputs. Sans lui, rien ne se passe et le message de timeout s'affiche après `connect_timeout` (5 s). |
| `SpawnPlayer` | `0x10` | `netId u32`, `posX f32`, `posY f32` [+ `skin u8`, `0xFF` = aucun] | Crée un joueur. Si `netId` est celui du `ConnectAck`, c'est le joueur local. |
| `GameStateSync` | `0x30` | `count u16`, puis `count` × (`netId u32`, `posX`, `posY`, `velX`, `velY` en f32) = 20 octets par entité | Met à jour la position cible (interpolée). `velX`/`velY` sont lus mais pas utilisés. |
| `DestroyEntity` | `0x11` | `netId u32` | Supprime l'entité. |

### Optionnel (gameplay complet)

| Paquet | Type | Payload | Effet côté client |
|--------|------|---------|-------------------|
| `SpawnEntity` | `0x12` | `netId u32`, `type u8`, `posX f32`, `posY f32` | Crée un ennemi, un missile, un bonus ou un boss (types ci-dessous). |
| `HealthUpdate` | `0x13` | `netId u32`, `health u8`, `maxHealth u8` | Barre de vie, flash rouge, écran "DESTROYED" si `health = 0` pour le joueur local. Pour un boss, met à jour sa barre de vie. |
| `AugmentUpdate` | `0x14` | `netId u32`, `augment u8`, `level u8`, `remaining f32` | Affiche les bonus actifs. `remaining = 0` : fini, négatif : permanent. |
| `ScoreUpdate` | `0x15` | `netId u32`, `score u32` | Affiche "SCORE 0000000" si `netId` est le joueur local. Les scores des autres sont ignorés. |

Types d'entité (`[entity_types]` de `client.cfg`) :

| Id | Entité |
|----|--------|
| 0 | Joueur (réservé à `SpawnPlayer`) |
| 1 | Bydos |
| 2 | Missile joueur |
| 3 | Missile ennemi |
| 4 / 5 / 6 / 7 | Bonus : double shot / triple shot / heal / damage |
| 8 | Missile joueur boosté |
| 9 | Boss "Green Mecha" : barre de boss, message d'alerte et musique de boss. Sa mort (`DestroyEntity`) affiche "destroyed". |
| 10 | Tir du boss |

Augments : 1 = double shot, 2 = triple shot, 3 = damage up.

## Ce que le serveur doit respecter

1. **Répondre depuis le même port** que celui où le client envoie. Le client jette tout paquet dont le port source n'est pas celui du serveur.
2. **Envoyer `ConnectAck` avant tout le reste** : sans lui, le client n'envoie pas d'inputs.
3. **Au `ConnectReq`, envoyer au nouveau client un `SpawnPlayer` pour chaque joueur déjà là, lui compris**, et prévenir les autres joueurs avec un `SpawnPlayer` du nouveau.
4. **Envoyer `SpawnEntity` avant le premier `GameStateSync`** qui contient cette entité. Sinon, le client crée l'entité inconnue en tant que *joueur*.
5. **La `sequence` des `GameStateSync` doit toujours augmenter** pour un même client. Un sync dont la séquence n'est pas plus grande que la précédente est ignoré.
6. **Lire le bool `shoot`** à la fin de `PlayerInput` : le client ne crée jamais de missile lui-même, c'est le serveur qui décide.
7. **Timeout** : le client envoie des inputs à chaque frame, donc ils servent de signe de vie. Un timeout de quelques secondes sans paquet suffit (3 s dans le serveur actuel).
8. Le client ne vérifie pas `payloadSize` : il lit les champs dans l'ordre. Les champs optionnels (`difficulty` dans `ConnectAck`, `skin` dans `SpawnPlayer`) sont lus seulement s'il reste des octets dans le paquet.
9. **Numéro de joueur** : le client calcule "Player N" ainsi : `(netId - first_player_id) % 4 + 1`, où `first_player_id` vient de `[game]` dans `client.cfg`. Les ids de joueurs doivent donc commencer à `first_player_id`.

## Ce qui a changé depuis `265fc26`

Côté client :

- `ConnectReq` peut contenir `previousNetId` (payload de 6 octets).
- Nouveau paquet `ScoreUpdate` (`0x15`).
- Nouveaux types d'entité : 9 (boss) et 10 (tir du boss).
- Le client bloque les inputs qui sortent de l'écran.
- Après un game over, la touche `menu` (Entrée) ramène au titre. Une nouvelle partie envoie un nouveau `ConnectReq`.
- `Protocol.hpp` ne déclare plus `Disconnect` deux fois, donc ça compile.

Côté serveur (`df2784d`, `c8cf272`) :

- Il lit `difficulty`, `skin`, `previousNetId` et `shoot`.
- Il envoie `ConnectAck` avec la difficulté, `SpawnPlayer` avec le skin, `SpawnEntity` et `HealthUpdate`.
- Il fait apparaître des Bydos, des tirs ennemis (visés en hard), des missiles joueur (1 toutes les 0,25 s) et des bonus. Il gère les collisions et la mort (le joueur est retiré au bout de 1,5 s).
- Un `ConnectReq` qui vient d'un endpoint déjà connecté renvoie le `ConnectAck` et le monde, sans créer de nouveau joueur.
- Les ids de joueurs vont de 1000 à 1999. Les autres entités commencent à 1.

## Ce qui manque encore côté serveur

| Manque | Conséquence côté client |
|--------|-------------------------|
| `AugmentUpdate` jamais envoyé, et aucune collision joueur / bonus | Les bonus traversent l'écran sans pouvoir être ramassés. |
| `ScoreUpdate` jamais envoyé | Le score reste à 0. |
| Pas de boss (types 9 et 10) ni de missile boosté (type 8) | Rien ne s'affiche pour eux. |
| Positions des joueurs non bornées | Le client compense en coupant les inputs au bord de l'écran. |
| Ids de joueurs à partir de 1000 alors que `first_player_id = 1` dans `client.cfg` | Le premier joueur s'affiche "Player 4" au lieu de "Player 1". Il faut mettre `first_player_id = 1000` dans `client.cfg`, ou faire partir les ids du serveur à 1. |
