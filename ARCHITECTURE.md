# Architecture


## Schéma
![Architecture](./ClasseUML.jpeg)

## Résumé:
eraid.c est le coeur du projet. Il est relié à des fichiers comme errai_util, str_util, task et timing_t qui l'aide dans ses fonctions les plus complexes. Il communique avec tadmor, le côté utilisateur du projet à l'aide de erraid_req et de tubes à l'aide du fichier tube_util.