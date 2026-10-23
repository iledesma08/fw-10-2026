# Trabajo Práctico N°2 : Firmware y Arquitectura

### Completen la tabla:

Usando el test ``test_fw_status.c`` como base completen la siguiente tabla, donde en cada iteración tienen que modificar el FW, el compilador o el HW.

Asegurense que en ``fw/Makefile`` estemos compilado el fw que queremos. Ejecuten un ``make clean`` antes de pasar a la siguiente línea de la tabla.

| Caso (test_fw_status.c) | Flag march (fw/Makefile) | Config de RTL (top.v) | (*)Tamaño de FW | Tamaño .bin (stat) | Ciclos totales | (**)Uso de celdas | Accesos a memoria (datos / instr) |
|---|---|---|---|---|---|---|---|
| Original | rv32i | original | 0x388 | 977 B | 13.876 | 3,6 M | 894 / 3.893 |
| Aumentar iteraciones x10 | rv32i | original | 0x388 | 977 B | 210.132 | 3,6 M | 8.414 / 59.105 |
| Anterior | rv32ic | original | 0x2a4 | 749 B | 12 (TRAP) | 3,6 M | 0 / 4 |
| Anterior | rv32ic | COMPRESSED_ISA(1) | 0x2a4 | 749 B | 214.367 | 3,7 M | 8.414 / 52.872 |
| Agregar multiplicaciones | rv32ic | anterior | 0x2d4 | 797 B | 238.167 | 3,7 M | 8.414 / 59.472 |
| Anterior | rv32imc | anterior | 0x1ee | 565 B | 1.506 (TRAP) | 3,7 M | 103 / 348 |
| Anterior | rv32imc | ENABLE_MUL(1) + ENABLE_DIV(1) | 0x1ee | 565 B | 149.376 | 3,9 M | 7.214 / 18.985 |
| Anterior | rv32imc | ENABLE_FAST_MUL(1) + ENABLE_DIV(1) | 0x1ee | 565 B | 128.976 | 3,9 M | 7.214 / 18.985 |

(*) utilizar dirección de la última instrucción

(**) estimación usando tamaño de compilación de verilator: ``du obj_dir/``. Mide bytes en disco, no celdas ni área de síntesis.

### Observaciones

El tamaño del firmware cambia segun lo que el compilador puede usar. Se ve en la ultima instruccion: en rv32i y rv32ic cae en `__modsi3` porque el linker deja ahi las rutinas de libgcc; En rv32imc cae en `main` porque esas rutinas ya no hacen falta.

- Con rv32ic aparecieron instrucciones comprimidas de 16 bits y el binario se achico.
- Con rv32imc GCC pudo usar `mul` y `div` de hardware y desaparecieron las rutinas de libgcc (`__mulsi3`, `__udivsi3`, etc.), por eso el binario quedo todavia mas chico: no es solo por las instrucciones comprimidas, tambien porque desaparecen las rutinas de software.

Para entender los ciclos hay que mirar la parte del codigo que se ejecuta muchas veces o concentra la mayor parte del tiempo (que tengo entendido que se le dice hot path, capaz me equivoco). En este TP identifique dos lugares: el for de `test_fw_status.c`, que repite 200 veces el bloque de sumas o multiplicaciones, y `print_dec`, que por cada digito del resultado hace /10 y %10 y despues manda los caracteres por UART. La multiplicacion esta en el for y la division aparece en `print_dec`. En hardware real el UART tambien pesa porque logicamente espera la transmision, pero en la simulacion el modelo puede ser mas rapido.

Con eso se entiende por que los ciclos no bajan siempre. Pasar a rv32ic achica el codigo y baja el fetch de instrucciones, pero el tiempo total queda igual o apenas peor: las instrucciones comprimidas ahorran memoria, no necesariamente ciclos. En cambio, cuando el cambio toca una operacion del hot path, si se nota: en rv32ic las multiplicaciones se resuelven con `__mulsi3`, una rutina de software que se ejecuta en cada vuelta del for, y por eso esa fila es la mas lenta. Al pasar a rv32imc con `ENABLE_MUL`, GCC usa `mul` de hardware y el tiempo baja bastante; tambien bajan los accesos a datos porque desaparece la rutina. Con `ENABLE_FAST_MUL` los ciclos bajan todavia mas, pero los accesos a memoria quedan iguales: la mejora viene de la latencia del multiplicador, no de menos lecturas o escrituras.

Hay configuraciones que no andan, son las que piden instrucciones que el RTL no tiene. Por eso para los casos 7 y 8 puse `ENABLE_DIV(1)`:

- rv32ic con `COMPRESSED_ISA=0` trapea con la primera instruccion comprimida.
- rv32imc con `ENABLE_MUL=0` trapea en el primer `mul`. Ademas, rv32imc con MUL pero sin DIV se cuelga en `remu`: `print_dec` hace /10 y %10, GCC usa `divu` y `remu`, y el core deriva esas instrucciones al PCPI, pero no hay divisor que responda.

Detras de esos casos hay una misma regla: el `-march` decide que instrucciones puede emitir GCC y los parametros del RTL deciden cuales puede ejecutar. Si se cambia uno sin el otro, el programa trapea o se cuelga.

El testbench original tampoco conecta trap, asi que sin arreglar eso los traps se ven como que el programa cuelga (lo arregle agregando `assign trap = u_top.trap;` en el testbench).

Conclusion: rv32i es la opcion mas simple y compatible; rv32ic sirve si el tamaño de codigo es critico, pero no esperaria que baje el tiempo; rv32imc con MUL y DIV conviene si hay multiplicaciones o divisiones en el hot path, porque ahi el hardware extra se paga con menos ciclos; `ENABLE_FAST_MUL` mejora la latencia a costa de mas hardware.
