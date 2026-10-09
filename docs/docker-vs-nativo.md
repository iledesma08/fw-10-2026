# Docker vs instalación nativa: comparación

**Veredicto:** todo el flujo del curso (compilar FW, simular con Verilator,
ver waveforms) se puede hacer con la imagen Docker **sin compilar nada** y sin
perder nada respecto a la instalación nativa. La imagen ya fue verificada de
punta a punta (ver [Evidencia](#evidencia)).

Imagen publicada: `ghcr.io/iledesma08/fw-env:latest` (`linux/amd64`, 1.25 GB).

## Tabla comparativa

| Aspecto | Instalación nativa (readme original) | Docker (`Dockerfile` del repo) |
|---|---|---|
| Tiempo total setup | 1–2 hs típico; **8+ hs reportado en el curso** compilando `riscv-gnu-toolchain` | `docker pull` (~2 min) o `docker build` (~1 min de descargas). Cero compilación |
| Disco | Clon del toolchain solo: ~6.6 GB + objetos de build (~10–20 GB en total) | Imagen: **1.25 GB** |
| Permisos | Requiere `sudo` (instala en `/opt/riscv`, `apt upgrade` del sistema, compila como root con `sudo make`) | Solo acceso al daemon Docker; nada se instala en el host |
| Reproducibilidad | Depende del Ubuntu de cada uno: 22.04 instala Verilator **4.038**, 24.04 instala **5.020** | Todos usan exactamente la misma imagen: Ubuntu 24.04 + Verilator 5.020 + `gcc-riscv64-unknown-elf` 13.2.0 |
| Toolchain RISC-V | Se compila de fuentes (`./configure --prefix=/opt/riscv --enable-multilib` + `make -jX`) | Precompilado de Ubuntu (`gcc-riscv64-unknown-elf`), con symlinks de compatibilidad en `/opt/riscv/bin`. Soporta `rv32i/ilp32` igual que el compilado |
| WSL | Requiere `.wslconfig` afinado (RAM/swap/CPU) o la compilación cuelga la máquina | Solo Docker Desktop; la compilación pesada no existe |
| `make build_project_config` | Corre en el host | Idem, adentro del container (genera `fw/config.h`, `rtl/config.vh`, `rtl/register_file.v`) |
| `make -C fw` | Usa `/opt/riscv/bin/...` fijo | `fw/Makefile` detecta `/opt/riscv` si existe y si no usa el del `PATH`: anda en ambos mundos |
| `make sim` | Verilator del host | Verilator 5.020 del container (requirió el fix de 1 línea en `rtl/adc.sv`, compatible con Verilator 4 también) |
| `make wave` (gtkwave GUI) | Abre el viewer automáticamente | El `.vcd` se genera en el container y **se abre con gtkwave en el host** (única diferencia real del flujo, ver abajo) |
| Limpieza (`make clean`) | `rm` acotado a `obj_dir`, `*.vcd`, `*.elf/bin/hex` | Idem, sobre los archivos montados |

## Qué pasos del readme reemplaza Docker

Se **saltean por completo** (el container ya trae el resultado):

- `sudo apt update && sudo apt upgrade -y`
- `sudo apt install -y verilator gtkwave python3 python3-pip`
- Todas las dependencias de compilación del toolchain (`autoconf`, `bison`, `flex`, ...)
- `cd submodules/riscv-gnu-toolchain` + `git submodule update --init` (~6.6 GB de descarga)
- `./configure --prefix=/opt/riscv --enable-multilib`
- `sudo make -j4/-j8` + `sudo make clean` (la compilación de horas)
- El `.wslconfig` afinado para compilar (solo sigue haciendo falta Docker Desktop en WSL/Windows)

Se **siguen corriendo igual**, pero adentro del container con el repo montado:

```terminal
docker run --rm -it -v "$(pwd)":/work -w /work fw-env make build_project_config
docker run --rm -it -v "$(pwd)":/work -w /work fw-env make -C fw
docker run --rm -it -v "$(pwd)":/work -w /work fw-env make sim
docker run --rm -it -v "$(pwd)":/work -w /work fw-env make clean
```

## Evidencia

Verificado con la imagen publicada (ver checklist en el `readme.md`):

- `riscv64-unknown-elf-gcc --version` → `13.2.0`, resuelve `rv32i/ilp32` con su `libgcc` multilib.
- `fw/current.elf` → `ELF 32-bit LSB executable, UCB RISC-V`.
- `make sim` → exit `0`, `REGISTER_FW_STATUS = 000000ff`, `Verilog $finish`, `wave.vcd` válido, ningún `%Error`.
- Los `%Warning` de Verilator (ancho de puertos, pines sin conectar del PicoRV32, etc.) salen igual en nativo con Verilator 5: son ruido preexistente, no algo que Docker agregue.

## Diferencias honestas (y por qué no perjudican)

1. **gtkwave corre en el host, no en el container.** `make wave` adentro del container
   necesita display; el flujo Docker es: simular adentro, abrir el `.vcd` afuera.
   Mismo waveform, un comando distinto. No se pierde información.
2. **Verilator 5 en vez de 4.** La imagen trae 5.020 (el de Ubuntu 24.04) mientras que
   quien instala en 22.04 obtiene 4.038. La única incompatibilidad encontrada en el
   repo (`sample_mem[i] <= '0` en `rtl/adc.sv`) se corrigió de forma compatible con
   ambas versiones. Si aparece otra diferencia 4-vs-5, se verá como `%Error` puntual,
   no como degradación silenciosa.
3. **Imagen solo `linux/amd64`.** En Mac ARM corre emulada (simulación más lenta) y
   en WSL/Windows pide Docker Desktop. Aun así sigue siendo más fácil que compilar
   el toolchain en esos entornos (en macOS el build nativo ni siquiera es directo).
   Multi-arquitectura (`buildx` para `arm64`) queda como mejora pendiente.
4. **El fork puede desactualizarse.** La cátedra actualiza el repo original con cada TP;
   este fork debe mergear `upstream/master` periódicamente. El `Dockerfile` está
   desacoplado del contenido del curso (solo instala herramientas), así que en general
   sobrevive a los updates sin cambios; si cambian las herramientas, se rebuildea y
   re-pushea la imagen.

## Mantenimiento

- Cambios de herramientas → editar `Dockerfile`, `docker build -t fw-env .`,
  verificar con la checklist del readme, `docker tag` + `docker push` a GHCR.
- La imagen es pública: cualquier compañero hace `docker pull` sin compilar nada.
