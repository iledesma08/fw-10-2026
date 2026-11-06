# TP4 — arranque, validación y comprobación del firmware

Resultados obtenidos ejecutando las pruebas con ayuda del agente; no se afirma que el estudiante haya realizado las prácticas o demostrado comprensión. Fuentes basadas en b88caf8b846d7b9cb8753ac58af167b7dc13487f, rama tp4-aprendizaje-guiado, con la implementación desarrollada en esta rama.

## Entorno y reproducción

Docker local fw-env:latest: GCC 13.2.0, ABI ilp32, instrucciones rv32i; Verilator 5.020. Los comandos siguientes se ejecutaron desde /work, la raíz del repositorio montado. Desde el host:

```sh
cd /media/data_partition/Facultad/Marvell/firmware/fw-10-2026
docker run --rm -it --user "$(id -u):$(id -g)" -v "$PWD":/work -w /work fw-env:latest bash
```

Las simulaciones completas se ejecutaron con `VER_FLAGS="--binary -Wno-fatal -f filelist.f"` para no generar grandes waveforms. Esto cambia la instrumentación de trazas, no el firmware ni la configuración del core. No se modificaron project_config.def, fw_image.h, startup.S, linker.ld ni los C protegidos print.c/test_fw_status.c.

## Hito 1 — Reconocimiento del arranque original

### 1. Primera instrucción, ubicación y orden de enlace

```sh
make -C fw STANDALONE=1 APP_SRC="print.c test_fw_status.c"
riscv64-unknown-elf-objdump -d fw/build/standalone.elf
riscv64-unknown-elf-nm -n fw/build/standalone.elf
```

Se observó la primera palabra `00040117`: en dirección 0x0 se desensambla como `auipc sp,0x40`. La instrucción siguiente conserva el valor mediante `mv sp,sp`; juntas implementan `la sp,_stack_top`, con _stack_top=0x40000. En 0x8 hay `jal 0x1e8 <main>` y en 0xc un salto a sí mismo.

_start queda en cero porque FLASH empieza en cero, el script reúne .text y la receta pasa startup.S antes de print.c. ENTRY(_start) declara la entrada ELF, pero no ubica automáticamente el símbolo ni modifica el reset físico.

Se enlazó además una variante temporal con print.c antes de startup.S, sin modificar las fuentes: uart0_send.part.0 quedó en 0x0 y _start en 0x1d8. El ELF declaró entrada 0x1d8. El core, sin embargo, seguiría empezando en 0x0: ejecutaría una rutina distinta, sin haber preparado el entorno. Se verificó el enlace de esa variante, no se presentó como una ejecución exitosa.

### 2. Secciones huérfanas y globales

```sh
make -C fw STANDALONE=1 APP_SRC="print.c test_fw_status.c" LDFLAGS="-Wl,--gc-sections -Wl,--orphan-handling=warn"
riscv64-unknown-elf-objdump -t fw/build/standalone.elf
```

Se reconstruyó desde un estado sin el ELF standalone antes de activar los avisos. El enlace con linker.ld informó .sbss, .riscv.attributes, .comment, .debug_line, .debug_line_str, .debug_info, .debug_abbrev, .debug_aranges, .debug_str, .debug_loclists, .debug_rnglists y .debug_frame. También avisó .rela.text ubicada en .rela.dyn durante el enlace; no todo aviso intermedio se conserva como sección final. Los avisos posteriores de bl.ld/app.ld pertenecen a otros programas y no se mezclan con esta respuesta.

Las globales quedaron en .sbss: int_2=0x20000, int_1=0x20004 e int_0=0x20008. La sección ocupa doce bytes y es NOBITS. El patrón .bss* no captura .sbss porque esta última comienza por .s. La letra B de nm por sí sola no da el nombre preciso de sección; objdump -t sí.

### 3. Comparación con «De 0 a main»

El arranque original prepara sp y llama a main, pero no copia valores inicializados desde ROM a RAM ni limpia .bss/.sbss. El testbench carga memory_i, no realiza esas tareas sobre memory_d. En esta configuración del simulador, el estado inicial de RAM puede ocultar la falta de limpieza. El test_fw_status no compara resultados numéricos de sus globales ni exige valores iniciales no nulos: alcanza a escribir 0xFF aunque no pruebe el entorno completo de C.

La simulación standalone terminó con 0xFF en 13876 ciclos. La UART del test original mostró repeticiones/cortes al imprimir índices, que no se corrigieron modificando los archivos protegidos. La observación de 0xFF demuestra la finalización declarada por ese test; no demuestra inicialización general correcta. El caso del hito 6 discrimina esa diferencia.

## Hitos 2–5 — Implementación realizada

- BL: cinco saltos de cuatro bytes, primer vector en 0x0 e IRQ defensiva en 0x10; .option norvc mantiene offsets. bl_reset_handler carga la pila y llama a C.
- app.ld ubica seis campos de header primero en 0x1000, Reset_Handler en 0x1018, y las familias pequeñas de datos junto a .data/.bss. Publica límites de copia/limpieza y calcula tamaño según bytes cargables. Reserva 4096 bytes mínimos de pila como decisión de implementación; no mide su máximo consumo.
- Startup: carga sp, copia palabras entre límites alineados, limpia ceros y llama a main. Ambos bucles aceptan rangos vacíos. Si main retorna sin 0xFF ejecuta ebreak; si ya declaró éxito queda en un bucle seguro.
- bl_jump recibe stack en a0 y entry en a1, carga sp, limpia ra y salta mediante jr sin enlace al BL. La UART anuncia BL y entrada antes de ejecutar el FW.
