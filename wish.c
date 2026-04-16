#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

/* Mensaje de error unico para todo el programa */
char error_message[30] = "An error has occurred\n";

/* Path de busqueda: arreglo de strings y su conteo */
char *search_path[64];
int   path_count = 0;

/* Imprime el mensaje de error a stderr */
void print_error() {
    write(STDERR_FILENO, error_message, strlen(error_message));
}

/* Inicializa el path con solo /bin como dice la guia */
void init_path() {
    search_path[0] = strdup("/bin");
    path_count = 1;
}

/*
 * Busca el ejecutable recorriendo search_path con access().
 * Retorna la ruta completa si lo encuentra, NULL si no.
 */
char *find_executable(char *name) {
    static char full_path[1024];
    int i;
    for (i = 0; i < path_count; i++) {
        snprintf(full_path, sizeof(full_path), "%s/%s", search_path[i], name);
        if (access(full_path, X_OK) == 0)
            return full_path;
    }
    return NULL;
}

/*
 * Ejecuta una linea que puede tener varios comandos separados por &.
 * Lanza todos en paralelo y luego espera a que terminen.
 */
void run_line(char *line) {
    /* Arrays para guardar los datos de cada comando */
    char *args[64][128];   /* args[i] = argv del comando i  */
    int   argc[64];        /* argc[i] = nro de args         */
    char *redir[64];       /* redir[i] = archivo '>' o NULL */
    int   num_cmds = 0;
    int   i, j;

    /* Inicializar */
    for (i = 0; i < 64; i++) {
        argc[i] = 0;
        redir[i] = NULL;
    }

    /* --- Paso 1: dividir por '&' --- */
    char *segments[64];
    int   num_segments = 0;
    char *tok = NULL;

    tok = strsep(&line, "&");
    while (tok != NULL) {
        segments[num_segments++] = tok;
        tok = strsep(&line, "&");
    }

    /* --- Paso 2: parsear cada segmento --- */
    for (i = 0; i < num_segments; i++) {
        char *seg = segments[i];
        char *token;

        /* Contar cuantos '>' tiene este segmento */
        int redir_count = 0;
        char *p;
        for (p = seg; *p != '\0'; p++) {
            if (*p == '>') redir_count++;
        }
        if (redir_count > 1) {
            print_error();
            continue;
        }

        /* Si hay un '>', partir el segmento ahi */
        char *file_part = NULL;
        if (redir_count == 1) {
            char *redir_pos = strchr(seg, '>');
            *redir_pos = '\0';
            file_part = redir_pos + 1;
        }

        /* Tokenizar la parte del comando (espacios y tabs) */
        int cmd_idx = num_cmds;
        token = strsep(&seg, " \t");
        while (token != NULL) {
            if (strlen(token) > 0) {
                args[cmd_idx][argc[cmd_idx]] = token;
                argc[cmd_idx]++;
            }
            token = strsep(&seg, " \t");
        }
        args[cmd_idx][argc[cmd_idx]] = NULL;

        /* Tokenizar la parte del archivo si hay redireccion */
        if (file_part != NULL) {
            int   file_count = 0;
            char *file_token = NULL;
            token = strsep(&file_part, " \t");
            while (token != NULL) {
                if (strlen(token) > 0) {
                    file_count++;
                    if (file_count == 1) file_token = token;
                }
                token = strsep(&file_part, " \t");
            }
            if (file_count != 1) {
                print_error();
                argc[cmd_idx] = 0;
                continue;
            }
            if (argc[cmd_idx] == 0) {
                print_error();
                continue;
            }
            redir[cmd_idx] = file_token;
        }

        if (argc[cmd_idx] > 0)
            num_cmds++;
    }

    if (num_cmds == 0) return;

    /* --- Paso 3: lanzar todos los procesos en paralelo --- */
    pid_t pids[64];
    int launched = 0;

    for (i = 0; i < num_cmds; i++) {
        char *exe = find_executable(args[i][0]);
        if (exe == NULL) {
            print_error();
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            print_error();
            continue;
        }

        if (pid == 0) {
            /* Proceso hijo: configurar redireccion si aplica */
            if (redir[i] != NULL) {
                int fd = open(redir[i], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    print_error();
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
            execv(exe, (char * const *)args[i]);
            /* Si execv retorna es porque hubo un error */
            print_error();
            exit(1);
        }

        /* Proceso padre: guardar pid, NO esperar todavia */
        pids[launched] = pid;
        launched++;
    }

    /* --- Paso 4: esperar a que terminen todos los hijos --- */
    for (j = 0; j < launched; j++) {
        waitpid(pids[j], NULL, 0);
    }
}

/* ── Built-in: exit ─────────────────────────────────────── */
void builtin_exit(int argc, char *argv[]) {
    if (argc > 1) {
        print_error();
        return;
    }
    exit(0);
}

/* ── Built-in: cd ───────────────────────────────────────── */
void builtin_cd(int argc, char *argv[]) {
    if (argc != 2) {
        print_error();
        return;
    }
    if (chdir(argv[1]) != 0)
        print_error();
}

/* ── Built-in: route ────────────────────────────────────── */
void builtin_route(int argc, char *argv[]) {
    int i;
    /* Liberar paths anteriores */
    for (i = 0; i < path_count; i++) {
        free(search_path[i]);
        search_path[i] = NULL;
    }
    path_count = 0;

    /* Agregar los nuevos paths (pueden ser 0) */
    for (i = 1; i < argc && path_count < 64; i++) {
        search_path[path_count] = strdup(argv[i]);
        path_count++;
    }
}

/* ────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    /* Solo acepta 0 o 1 argumento */
    if (argc > 2) {
        print_error();
        exit(1);
    }

    FILE *input = stdin;
    int interactive = (argc == 1);

    /* Modo batch: abrir el archivo */
    if (!interactive) {
        input = fopen(argv[1], "r");
        if (input == NULL) {
            print_error();
            exit(1);
        }
    }

    init_path();

    char  *line = NULL;
    size_t len  = 0;

    while (1) {
        if (interactive) {
            printf("wish> ");
            fflush(stdout);
        }

        /* getline retorna -1 en EOF */
        if (getline(&line, &len, input) == -1)
            exit(0);

        /* Hacer una copia de la linea para parsear
           (strsep modifica el string original) */
        char *line_copy = strdup(line);

        /* Quitar el newline */
        line_copy[strcspn(line_copy, "\n")] = '\0';

        /* Tokenizar para saber el primer token (comando) */
        char *check = strdup(line_copy);
        char *check_ptr = check;
        char *first_token = NULL;

        /* Saltar espacios/tabs para llegar al primer token */
        char *t = strsep(&check_ptr, " \t\n");
        while (t != NULL) {
            if (strlen(t) > 0) {
                first_token = t;
                break;
            }
            t = strsep(&check_ptr, " \t\n");
        }

        /* Linea vacia: ignorar */
        if (first_token == NULL) {
            free(line_copy);
            free(check);
            continue;
        }

        /* Verificar si es built-in */
        if (strcmp(first_token, "exit") == 0 ||
            strcmp(first_token, "cd")   == 0 ||
            strcmp(first_token, "route") == 0) {

            /* Parsear los argumentos del built-in */
            char *builtin_args[128];
            int   builtin_argc = 0;
            char *ba_line = line_copy;
            char *ba_tok = strsep(&ba_line, " \t\n");
            while (ba_tok != NULL) {
                if (strlen(ba_tok) > 0) {
                    builtin_args[builtin_argc] = ba_tok;
                    builtin_argc++;
                }
                ba_tok = strsep(&ba_line, " \t\n");
            }
            builtin_args[builtin_argc] = NULL;

            if (strcmp(first_token, "exit") == 0)
                builtin_exit(builtin_argc, builtin_args);
            else if (strcmp(first_token, "cd") == 0)
                builtin_cd(builtin_argc, builtin_args);
            else if (strcmp(first_token, "route") == 0)
                builtin_route(builtin_argc, builtin_args);

        } else {
            /* Comando externo: pasar a run_line */
            run_line(line_copy);
        }

        free(line_copy);
        free(check);
    }

    free(line);
    if (!interactive) fclose(input);
    return 0;
}
