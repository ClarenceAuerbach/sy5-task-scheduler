# Architecture

## Schéma

![Architecture](./ClasseUML.jpeg)

## Explication

À la suite de `"./erraid"`, selon les options, `RUN_DIR` et `PIPES_DIR` sont mis à jour, et le parcours de l'arborescence est effectué.

Le processus crée un fils qui gère les requêtes de **tadmor**, et dispose d'un tube anonyme pour envoyer deux types de messages à son père :

- le message `"q"` indique au père de terminer (en libérant la mémoire),
- le message `"c"` indique au père qu'il y a eu un changement dans l'arborescence et qu'il faut mettre à jour `task_array`, en reparcourant l'arborescence.
- le message `"w"` demande au père le temps avant la prochaine éxecution et les tâches comprisent dans cette éxecution.

La double-flèche entre **erraid_req** et **tadmor** représente la communication par tube nommé, `req_pipe` et `rep_pipe`.

Les autres flèches indiquent seulement les liens de nécessité, `A -> B` exprime que **A repose sur les fonctions de B**.  
Enfin, **str_util.c** est en réalité relié à tous les fichiers.

---

## Récapitulatif

- **erraid** :  
  Contient la boucle principale et exécute les tâches

- **erraid_req** :  
  Effectue les requêtes, répond, et communique avec erraid

- **task** :  
  Contient le parcours et toutes les fonctions qui modifient `task_array` ou l'arborescence

- **timing** :  
  Contient les fonctions qui calcul les prochaines exécutions des taches

- **tadmor** :  
  Écris la requête en suivant le protocole et affiche la réponse

- **tube_util** :  
  Contient des fonctions nécessaires à la communication entre tadmor et erraid,  
  les fonctions d'écriture et de lecture,  
  la mis en commun de ces fonctions assure la non-modification des données,  
  et les fonctions de conversions bitmap_string

- **str_util** :  
  Contient les strings dynamiques et buffer dynamique  
  (string sans `'\0'` à la fin) utilisé pour ne pas deviner certaines tailles maximum

- **erraid_util** :  
  Contient `print_exc`, `print_tasks` et d'autre fonctions de debug
