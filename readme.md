# Entorno de Trabajo

## Docker (recomendado):

No hace falta compilar el toolchain (~2hs): la imagen ya trae `gcc-riscv64-unknown-elf`, `verilator`, `gtkwave` y `python3` (ver `Dockerfile`).

Descargar la imagen prearmada (recomendado, no compila ni buildea nada):

```terminal
docker pull ghcr.io/iledesma08/fw-env
docker tag ghcr.io/iledesma08/fw-env fw-env
```

Alternativa: buildearla localmente con ```docker build -t fw-env .```.

Compilar FW y simular con el repo montado (sin copiar nada a la imagen). Con Docker se saltean todos los pasos de "Instalar dependencias" y "Buildear toolchain":

```terminal
docker run --rm -it -v "$(pwd)":/work -w /work fw-env make build_project_config
docker run --rm -it -v "$(pwd)":/work -w /work fw-env make -C fw
docker run --rm -it -v "$(pwd)":/work -w /work fw-env make sim
```

Alternativa para trabajar y revisar logs con comodidad: abrir una shell adentro del container (equivale a la shell de la máquina) y correr los `make` ahí:

```terminal
docker run --rm -it -v "$(pwd)":/work -w /work fw-env bash
```

Adentro del container:

```terminal
make build_project_config
make -C fw
make sim
```

El `.vcd` generado se abre con `gtkwave` en el host. Para no dejar archivos generados como root, agregar ```--user "$(id -u):$(id -g)"``` a cada `docker run`.

### Verificación (todo en Docker):

```terminal
docker run --rm fw-env riscv64-unknown-elf-gcc --version
docker run --rm -v "$(pwd)":/work -w /work fw-env make clean_sim
docker run --rm -v "$(pwd)":/work -w /work fw-env make build_project_config
docker run --rm -v "$(pwd)":/work -w /work fw-env make -C fw
docker run --rm -v "$(pwd)":/work -w /work fw-env make sim
file fw/current.elf
```

Resultados esperados: `13.2.0`; se generan `fw/config.h`, `rtl/config.vh`, `rtl/register_file.v` y `fw/fw.hex` (una palabra hex por línea); `make sim` termina con ```REGISTER_FW_STATUS = 000000ff``` y ```Verilog $finish```, exit code 0 y sin ningún ```%Error``` (los ```%Warning``` son normales); `file` dice `ELF 32-bit LSB executable, UCB RISC-V`. La etapa de `g++` tarda varios minutos y parece colgada: dejarla terminar, no cortar con Ctrl+C.

## Ubuntu:
```terminal
git clone https://github.com/curso-firmware-marvell/fw-10-2026
cd fw-10-2026
git submodule update --init
```

### Actualizar
```sudo apt update && sudo apt upgrade -y```

### Instalar dependencias:
```sudo apt install -y verilator gtkwave python3 python3-pip```

### Buildear toolchain (riscv repo)
```terminal
sudo apt-get install autoconf automake autotools-dev curl python3-tomli libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev ninja-build git cmake libglib2.0-dev libslirp-dev libncurses-dev
cd submodules/riscv-gnu-toolchain
git submodule update --init
./configure --prefix=/opt/riscv --enable-multilib
```

#### Si usan WSL:

Crear un archivo .wslconfig (docs: https://learn.microsoft.com/es-mx/windows/wsl/wsl-config) en C:\Usuarios\\%usuario%\\.wslconfig con parámetros igual a la mitad de los disponibles en la computadora (por ejemplo, para 8 núcleos y 16GB de RAM), más al menos 8GB de swap (espacio en disco que se ocupará en C:).

```
[wsl2]
memory=8GB
processors=4
swap=16GB
```

Luego de crear el archivo, reiniciar la instancia WSL desde powershell (wsl --shutdown). Para el siguiente comando, modificar el "4" con la cantidad de procesadores asignados a WSL (también se puede checkear con el comando nproc).

```terminal
sudo make -j4
sudo make clean
```

#### Si no usan WSL:

```terminal
sudo make -j8
sudo make clean
```

## Compilación de FW:

Ir a directorio de FW.

```terminal
cd fw
make clean
make
```


## Simulación de RTL:

Opciones:

```terminal
make
make clean
make fw
make sim
make wave
make build_project_config
```
