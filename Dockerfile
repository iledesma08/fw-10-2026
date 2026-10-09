# Entorno de desarrollo fw-10-2026.
# Evita compilar riscv-gnu-toolchain (~2hs): usa el cross-compiler
# precompilado de Ubuntu (bare-metal, 32 y 64 bits).
#
# Build una sola vez:
#   docker build -t fw-env .
#
# Uso con el repo montado (no hace falta copiar el codigo a la imagen):
#   docker run --rm -it -v "$(pwd)":/work -w /work fw-env make build_project_config
#   docker run --rm -it -v "$(pwd)":/work -w /work fw-env make -C fw
#   docker run --rm -it -v "$(pwd)":/work -w /work fw-env make sim
#
# El .vcd generado se abre en el host con gtkwave.
# Para no dejar archivos generados como root, agregar:
#   --user "$(id -u):$(id -g)"

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    make git python3 \
    g++ \
    verilator gtkwave \
    bsdextrautils \
    gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf \
    && rm -rf /var/lib/apt/lists/*

# Compatibilidad: fw/Makefile historicamente esperaba /opt/riscv/bin/...
# (con el ?= del Makefile ya no es obligatorio, pero no rompe nada existente).
RUN mkdir -p /opt/riscv/bin && \
    ln -sf /usr/bin/riscv64-unknown-elf-gcc /opt/riscv/bin/riscv64-unknown-elf-gcc && \
    ln -sf /usr/bin/riscv64-unknown-elf-objdump /opt/riscv/bin/riscv64-unknown-elf-objdump && \
    ln -sf /usr/bin/riscv64-unknown-elf-objcopy /opt/riscv/bin/riscv64-unknown-elf-objcopy

WORKDIR /work
