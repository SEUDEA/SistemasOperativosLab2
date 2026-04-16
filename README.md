# Laboratorio 2 – API de Procesos: wish (Wisconsin Shell)
**Universidad de Antioquia** | Facultad de Ingeniería | Ingeniería de Sistemas
Sistemas Operativos – 2026

---

## (a) Integrantes

| Nombre completo | Correo | N° de documento |
|-----------------|--------|-----------------|
| Mateo Aguirre Duque | mateo.aguirre@udea.edu.co | CC 1152472114 |
| Adelaida Rodríguez García | adelaida.rodriguez@udea.edu.co | CC 1033377815 |

---

## (b) Documentación de funciones

### `print_error()`
Imprime el único mensaje de error del shell (`"An error has occurred\n"`) directamente a `stderr` usando `write()`. Se usa en **todos** los casos de error sin excepción.

---

### `init_path()`
Inicializa el arreglo global `search_path` con un único directorio: `/bin`. Es llamada una sola vez al inicio de `main()`.

```c
search_path[0] = strdup("/bin");
path_count = 1;
```

---

### `find_executable(char *name)`
Recorre `search_path` buscando el ejecutable llamado `name`. Para cada directorio construye la ruta completa y la verifica con `access(ruta, X_OK)`.

- **Retorna:** puntero a un buffer estático con la ruta completa si lo encuentra.
- **Retorna:** `NULL` si no está en ningún directorio del path.

```
search_path = {"/bin", "/usr/bin"}
name        = "ls"
→ prueba access("/bin/ls", X_OK)   → existe → retorna "/bin/ls"
```

---

### `run_line(char *line)`
Es la función central de ejecución. Recibe la línea completa (ya sin el `\n`) y hace todo el trabajo en 4 pasos:

**Paso 1 – Dividir por `&`:**
Usa `strsep(&line, "&")` para separar la línea en segmentos. Cada segmento es un comando independiente.

**Paso 2 – Parsear cada segmento:**
Para cada segmento:
- Cuenta cuántos `>` tiene. Si hay más de 1 → error.
- Si hay un `>`, parte el segmento en dos: parte del comando y parte del archivo.
- Tokeniza la parte del comando con `strsep(&seg, " \t")` para llenar el arreglo `args[i]`.
- Tokeniza la parte del archivo: debe haber exactamente 1 token → es el `redir[i]`.

**Paso 3 – Lanzar todos en paralelo:**
Para cada comando hace `fork()`. El proceso **hijo** configura la redirección con `dup2()` si aplica, y llama `execv()`. El proceso **padre** solo guarda el `pid` en un arreglo y **no espera**.

**Paso 4 – Esperar a todos:**
Después de lanzar **todos** los hijos, el padre hace `waitpid()` sobre cada `pid` guardado. Esto garantiza que el shell solo devuelve el prompt cuando todos los comandos paralelos terminaron.

---

### `builtin_exit(int argc, char *argv[])`
Implementa el comando integrado `exit`. Llama `exit(0)`. Si se pasan argumentos (`argc > 1`) imprime error y **no** sale.

---

### `builtin_cd(int argc, char *argv[])`
Implementa el comando integrado `cd`. Llama `chdir(argv[1])`. Requiere exactamente 1 argumento; con 0 o más de 1 imprime error.

---

### `builtin_route(int argc, char *argv[])`
Implementa el comando integrado `route`. **Sobreescribe** el search path completo con los directorios dados como argumentos. Si no se pasan argumentos (`argc == 1`) el path queda vacío y no se puede ejecutar ningún comando externo.

```
route /bin /usr/bin  →  search_path = {"/bin", "/usr/bin"}
route                →  search_path = {}  (path vacío)
```

---

### `main(int argc, char *argv[])`
Punto de entrada del shell. Hace:
1. Valida que se invoque con 0 o 1 argumento (si son más → error + `exit(1)`).
2. Abre el archivo batch si se dio uno; si falla → error + `exit(1)`.
3. Llama `init_path()`.
4. Entra al loop principal:
   - Imprime `wish> ` solo en modo interactivo.
   - Lee la línea con `getline()`. Si retorna `-1` (EOF) → `exit(0)`.
   - Detecta el primer token para saber si es un built-in.
   - Si es built-in: parsea argumentos y llama la función correspondiente.
   - Si no: llama `run_line()`.

---

## (c) Problemas durante el desarrollo y soluciones

### Problema 1: `strsep` modifica el string original
`strsep` inserta caracteres `\0` en el string que recibe. Si se usa directamente sobre el buffer de `getline`, los tokens de un comando apuntan a ese buffer y pueden corromperse al parsear el siguiente.

**Solución:** Antes de parsear, se hace `strdup(line)` para trabajar sobre una copia independiente. Esa copia se libera con `free()` al terminar.

---

### Problema 2: Operadores `>` y `&` sin espacios
El parser no puede simplemente tokenizar por espacios, porque `ls>file` o `ls&echo` son entradas válidas.

**Solución:** Antes de tokenizar por espacios, se divide explícitamente por el carácter `&` (para comandos paralelos) y se busca el carácter `>` con `strchr()` para la redirección. Así el operador funciona con o sin espacios alrededor.

---

### Problema 3: Redirección redirige solo stdout pero no stderr
La guía pide redirigir **los dos** (`stdout` y `stderr`) al mismo archivo.

**Solución:** En el hijo se hace `dup2(fd, STDOUT_FILENO)` y también `dup2(fd, STDERR_FILENO)`.

---

### Problema 4: Comandos paralelos — `wait` en el momento incorrecto
Si se llama `wait()` inmediatamente después de cada `fork()`, los comandos se ejecutan **en secuencia**, no en paralelo.

**Solución:** Se guardan todos los `pid` en un arreglo. Primero se lanzan **todos** los hijos sin esperar, y solo después se hace `waitpid()` sobre cada uno.

---

### Problema 5: El prompt aparecía en modo batch
En modo batch no se debe imprimir `wish> `.

**Solución:** La variable `interactive` (1 si no se pasó archivo, 0 si sí) controla si se imprime el prompt.

---

## (d) Pruebas realizadas

### 1. Comando básico sin argumentos
```bash
echo "ls" | ./wish
# Resultado esperado: lista de archivos del directorio actual
```

### 2. Comando con argumentos
```bash
echo "ls -la /tmp" | ./wish
# Resultado esperado: listado detallado de /tmp
```

### 3. Built-in `cd`
```bash
printf "cd /tmp\nls\n" | ./wish
# Resultado esperado: primero cambia a /tmp, luego lista /tmp
```

### 4. Built-in `route` — agregar directorios
```bash
printf "route /bin /usr/bin\necho hola\n" | ./wish
# Resultado esperado: "hola" (echo está en /bin o /usr/bin)
```

### 5. Built-in `route` — path vacío
```bash
printf "route\nls\n" | ./wish
# Resultado esperado: "An error has occurred" (no puede encontrar ls)
```

### 6. Redirección básica
```bash
echo "ls > /tmp/salida.txt" | ./wish
cat /tmp/salida.txt
# Resultado esperado: contenido del directorio en el archivo
```

### 7. Redirección sin espacios
```bash
echo "ls>/tmp/sinespacio.txt" | ./wish
cat /tmp/sinespacio.txt
# Resultado esperado: funciona igual que con espacios
```

### 8. Comandos paralelos con `&`
```bash
echo "ls & echo hola" | ./wish
# Resultado esperado: salida de ls y "hola" intercaladas (paralelo)
```

### 9. Paralelo sin espacios
```bash
echo "ls&echo hola" | ./wish
# Resultado esperado: igual que el anterior
```

### 10. Modo batch
```bash
cat > /tmp/prueba.txt << 'EOF'
ls
echo hola mundo
cd /tmp
ls
EOF
./wish /tmp/prueba.txt
# Resultado esperado: ejecuta los 4 comandos en secuencia, sin prompt
```

### 11. Error — múltiples `>`
```bash
echo "ls > a > b" | ./wish
# Resultado esperado: "An error has occurred"
```

### 12. Error — múltiples archivos después de `>`
```bash
echo "ls > archivo1 archivo2" | ./wish
# Resultado esperado: "An error has occurred"
```

### 13. Error — `exit` con argumento
```bash
echo "exit foo" | ./wish
# Resultado esperado: "An error has occurred", shell continúa
```

### 14. Error — `cd` sin argumento
```bash
echo "cd" | ./wish
# Resultado esperado: "An error has occurred"
```

### 15. Error — demasiados argumentos al invocar wish
```bash
./wish arg1 arg2
# Resultado esperado: "An error has occurred", exit code 1
echo $?   # debe imprimir 1
```

### 16. Error — batch file inexistente
```bash
./wish /tmp/noexiste.txt
# Resultado esperado: "An error has occurred", exit code 1
```

### 17. Whitespace variado
```bash
printf "  ls   -la  \n" | ./wish
# Resultado esperado: funciona igual que "ls -la"
```

### 18. EOF en modo interactivo (Ctrl+D)
```bash
echo "" | ./wish
# Resultado esperado: sale con código 0
```

---

## (e) Video de sustentación

[Enlace al video – pendiente de publicar]

---

## (f) Manifiesto de transparencia — uso de IA generativa

Durante el desarrollo de esta práctica se utilizó **Claude (Anthropic)** puntualmente como herramienta de consulta y para apoyar la redacción de la documentación. A continuación se detallan los usos específicos:

- **Documentación del código:** Una vez el código estuvo funcionando, se usó Claude para ayudar a redactar de forma clara los comentarios de las funciones y la estructura de este README. El contenido técnico fue revisado y ajustado por el grupo.

- **Consulta sobre `strsep`:** Durante el parser surgió la duda de cómo `strsep` modifica el puntero original al insertar `\0`. Se le preguntó a Claude cómo funciona internamente y por qué es distinto a `strtok`. La solución de hacer `strdup` antes de llamarlo la aplicamos nosotros una vez entendimos el problema.

- **Consulta sobre `dup2`:** No teníamos claro si redirigir solo `stdout` era suficiente o si también había que redirigir `stderr`. Consultamos cómo funciona `dup2` con los file descriptors estándar y con eso pudimos escribir el código de redirección nosotros mismos.

- **Orientación en la recuperación de procesos paralelos:** Tuvimos un bug donde los comandos lanzados con `&` no se esperaban correctamente y el shell devolvía el prompt antes de que terminaran. Le preguntamos a Claude cómo se puede rastrear múltiples procesos hijos para luego esperarlos, y nos explicó la idea de guardar los `pid` en un arreglo. La implementación con el arreglo `pids[]` y el loop de `waitpid()` la escribimos a partir de esa explicación.

- **Consulta sobre `access()`:** No conocíamos esta llamada al sistema. Se consultó qué hace `access(path, X_OK)` y por qué es más adecuada que intentar abrir el archivo para verificar si es ejecutable.

- **Diseño y ejecución de pruebas:** Una vez el shell estuvo implementado, se usó Claude para definir un conjunto completo de casos de prueba que cubrieran todos los requisitos del laboratorio: comandos básicos, built-ins, redirección, operadores sin espacios, whitespace variado, comandos paralelos, modo batch y manejo de errores. Claude también ejecutó los casos de prueba y ayudó a interpretar los resultados para confirmar que el comportamiento era el esperado según la guía.

El diseño general del shell, la lógica del loop principal, el manejo de los built-ins y la mayoría del código fueron desarrollados directamente por los integrantes del grupo basándose en la guía del laboratorio y el material del curso.
