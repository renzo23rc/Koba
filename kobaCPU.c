#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// Registros especiales basados en la especificación
typedef enum {
    REG_RD = 10,
    REG_SP = 12
} SpecialRegisters;

// Estructura de la CPU
typedef struct {
    unsigned int pc;
    unsigned int psw;
    uint16_t regs[16];
} CPU;

// Memorias
// program_memory de 64 posiciones
unsigned int program_memory[64];
// data_memory de 64KB (acceso por byte)
uint8_t data_memory[65536];

// Opcodes definidos
#define OP_ADD 0x0
#define OP_LW  0x2
#define OP_SW  0x3
#define OP_BEQ 0x4
#define OP_J   0xE

// Inicializar estado de la CPU
void init_cpu(CPU *cpu) {
    cpu->pc = 0;
    cpu->psw = 0;
    for (int i = 0; i < 16; i++) {
        cpu->regs[i] = 0;
    }
    // Inicializa la memoria (opcional pero buena práctica)
    for (int i = 0; i < 64; i++) program_memory[i] = 0;
    for (int i = 0; i < 65536; i++) data_memory[i] = 0;
}

// Ejecutar un ciclo de la CPU
void step(CPU *cpu) {
    if (cpu->pc >= 64) {
        printf("Error: PC supero los limites de memoria.\n");
        exit(1);
    }
    
    // FETCH: Obtener la instruccion de la program_memory y actualizar PC
    unsigned int instr = program_memory[cpu->pc];
    cpu->pc++;
    
    // DECODE: Extraer los nibbles (bloques de 4 bits)
    // El orden base propuesto (para operacion de 5 nibbles -> 20 bits):
    // Bits [16-19]: Opcode
    // Bits [12-15]: rd (Destino)
    // Bits [ 8-11]: rs (Fuente 1)
    // Bits [ 4- 7]: rt (Fuente 2)
    // Bits [ 0- 3]: Offset (ultimo nibble, util para saltos)
    
    unsigned int opcode = (instr >> 16) & 0xF;  // Shift right de 16 bits y & 1111 binario
    unsigned int rd     = (instr >> 12) & 0xF;
    unsigned int rs     = (instr >> 8) & 0xF;
    unsigned int rt     = (instr >> 4) & 0xF;
    unsigned int offset = instr & 0xF;         // No hace falta shift, solo enmascarar los ultimos 4 bits
    unsigned int j_addr = instr & 0xFFFF;      // Bits restantes despues del opcode (4 nibbles) para 'J'
    
    // EXECUTE
    switch (opcode) {
        case OP_ADD:
            cpu->regs[rd] = cpu->regs[rs] + cpu->regs[rt];
            printf("ADD: regs[%d] = regs[%d] (%d) + regs[%d] (%d) = %d\n",
                   rd, rs, cpu->regs[rs], rt, cpu->regs[rt], cpu->regs[rd]);
            break;
            
        case OP_LW:
            // Lee 2 bytes (16 bits) desde la data_memory en la posicion dada por rs.
            // Para lidiar con 2 bytes y guardarlos en un uint16_t unimos mediante OR bit a bit (Little Endian).
            cpu->regs[rd] = data_memory[cpu->regs[rs]] | 
                            (data_memory[cpu->regs[rs] + 1] << 8);
            printf("LW: regs[%d] <- data_memory[%d] (valor: %d)\n", rd, cpu->regs[rs], cpu->regs[rd]);
            break;
            
        case OP_SW:
            // Almacena los 16 bits del registro separandolos en 2 bytes
            data_memory[cpu->regs[rs]] = cpu->regs[rd] & 0xFF;                     // Byte menos significativo
            data_memory[cpu->regs[rs] + 1] = (cpu->regs[rd] >> 8) & 0xFF; // Byte mas significativo
            printf("SW: data_memory[%d] <- regs[%d] (valor: %d)\n", cpu->regs[rs], rd, cpu->regs[rd]);
            break;
            
        case OP_BEQ:
            printf("BEQ: regs[%d] == regs[%d]? (%d == %d) -> ", rd, rs, cpu->regs[rd], cpu->regs[rs]);
            if (cpu->regs[rd] == cpu->regs[rs]) {
                cpu->pc = offset; // Salto a la direccion indicada por el ultimo nibble
                printf("Salto a PC = %d!\n", cpu->pc);
            } else {
                printf("No salta.\n");
            }
            break;
            
        case OP_J:
            cpu->pc = j_addr;  // Salto incondicional 
            printf("J: Salto incondicional a PC = %d\n", cpu->pc);
            break;
            
        default:
            printf("Opcode %X no soportado.\n", opcode);
            break;
    }
}

int main(void) {
    CPU cpu;
    init_cpu(&cpu);
    
    // --- PROGRAMA DE PRUEBA --- //
    // Inicializar algunos valores para la suma
    cpu.regs[0] = 15;
    cpu.regs[1] = 27;
    // Base address arbitraria en data_memory
    cpu.regs[REG_SP] = 100;
    
    // Instruccion 0: ADD $rd, r0, r1 (Suma 15+27 y guarda en REG_RD(10))
    // Estructura: 0x0 | A(10) | 0(0) | 1(1) | 0(last nibble ignorado)
    // => 0x0A010
    program_memory[0] = 0x0A010;
    
    // Instruccion 1: SW en dir del REG_SP(12) guardando el contenido del REG_RD(10)
    // Estructura: 0x3 | A(10) | C(12) | 0 | 0
    // => 0x3AC00
    program_memory[1] = 0x3AC00;
    
    // Instruccion 2: LW en REG_2 desde dir REG_SP(12) 
    // Estructura: 0x2 | 2(2)  | C(12) | 0 | 0
    // => 0x22C00
    program_memory[2] = 0x22C00;
    
    // Instruccion 3: BEQ compara REG_2 (que deberia ser igual a REG_RD) y REG_RD.
    // Si son iguales salta al PC 5 (saltandose la inst 4)
    // Estructura: 0x4 | 2(2) | A(10) | 0 | 5(offset a saltar)
    // => 0x42A05
    program_memory[3] = 0x42A05;
    
    // Instruccion 4: Jump incondicional al 0 (Deberia saltarse, sino causara loop)
    // 0xE0000
    program_memory[4] = 0xE0000;
    
    // Instruccion 5: Add tonto de salida
    // 0x02220 (reg2 = reg2 + reg2)
    program_memory[5] = 0x02220;
    
    // Setear limite de finalización 
    program_memory[6] = 0xF0000; // Finalizar la simulación si es opcode desconocido
    
    printf("Iniciando Simulador...\n\n");
    // Simulamos un pequenio ciclo
    while (cpu.pc < 7) {
        printf("--- Ciclo PC=%d ---\n", cpu.pc);
        if (program_memory[cpu.pc] == 0xF0000) break;
        step(&cpu);
        printf("\n");
    }
    
    printf("Simulacion Terminada. Valor final en REG_RD: %d, valor en regs[2]: %d\n", 
           cpu.regs[REG_RD], cpu.regs[2]);
           
    return 0;
}
