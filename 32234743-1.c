#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MEM_SIZE 1024 * 1024 // 1 MB 메모리 크기
#define REG_COUNT 32         // 레지스터 개수

// 메모리와 레지스터 배열 선언
uint32_t memory[MEM_SIZE];
uint32_t registers[REG_COUNT];
uint32_t pc = 0;
uint32_t cycle = 0;
uint32_t instr_count[3] = {0}; // R-type, I-type, J-type 명령어 카운트

// 명령어 유형 정의
typedef enum {
    R_TYPE,
    I_TYPE,
    J_TYPE,
    UNKNOWN_TYPE
} InstrType;

// 디코딩된 명령어 구조체 정의
typedef struct {
    InstrType type;
    uint32_t raw;
    uint32_t opcode;
    uint32_t rs;
    uint32_t rt;
    uint32_t rd;
    uint32_t shamt;
    uint32_t funct;
    uint32_t imm;
    uint32_t address;
    char inst_str[32];
} DecodedInstr;

// 메모리를 바이너리 파일로부터 로드하는 함수
void load_memory(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("파일 열기 오류");
        exit(EXIT_FAILURE);
    }
    fread(memory, sizeof(uint32_t), MEM_SIZE, file);
    fclose(file);
}

// 명령어를 가져오는 함수
DecodedInstr fetch() {
    DecodedInstr instr = {0};
    if (pc >= MEM_SIZE * 4) {
        fprintf(stderr, "PC가 메모리 범위를 벗어났습니다: 0x%08x\n", pc);
        instr.raw = 0xFFFFFFFF; // 유효하지 않은 명령어
        return instr;
    }
    instr.raw = memory[pc / 4];
    pc += 4;
    return instr;
}

// 명령어를 디코드하는 함수
DecodedInstr decode(DecodedInstr instr) {
    if (instr.raw == 0xFFFFFFFF) {
        instr.type = UNKNOWN_TYPE;
        return instr;
    }

    instr.opcode = (instr.raw >> 26) & 0x3F;
    switch (instr.opcode) {
        case 0x00: { // R-type 명령어
            instr.type = R_TYPE;
            instr.rs = (instr.raw >> 21) & 0x1F;
            instr.rt = (instr.raw >> 16) & 0x1F;
            instr.rd = (instr.raw >> 11) & 0x1F;
            instr.shamt = (instr.raw >> 6) & 0x1F;
            instr.funct = instr.raw & 0x3F;
            snprintf(instr.inst_str, sizeof(instr.inst_str), "R, funct: %02x", instr.funct);
            instr_count[R_TYPE]++;
            break;
        }
        case 0x02: // J-type (J)
        case 0x03: // J-type (JAL)
            instr.type = J_TYPE;
            instr.address = instr.raw & 0x3FFFFFF;
            snprintf(instr.inst_str, sizeof(instr.inst_str), "J, address: %08x", instr.address);
            instr_count[J_TYPE]++;
            break;
        default: { // I-type 명령어
            instr.type = I_TYPE;
            instr.rs = (instr.raw >> 21) & 0x1F;
            instr.rt = (instr.raw >> 16) & 0x1F;
            instr.imm = instr.raw & 0xFFFF;
            snprintf(instr.inst_str, sizeof(instr.inst_str), "I, imm: %04x", instr.imm);
            instr_count[I_TYPE]++;
            break;
        }
    }
    return instr;
}

// 명령어를 실행하는 함수
void execute(DecodedInstr instr, uint32_t *alu_result, char *execute_info) {
    if (instr.type == UNKNOWN_TYPE) return;

    switch (instr.type) {
        case R_TYPE:
            switch (instr.funct) {
                case 0x20: // ADD
                    *alu_result = registers[instr.rs] + registers[instr.rt];
                    snprintf(execute_info, 256, "ALU result: 0x%08x", *alu_result);
                    snprintf(instr.inst_str, sizeof(instr.inst_str), "add %s, %s, %s", "rd", "rs", "rt");
                    break;
                case 0x08: // JR
                    pc = registers[instr.rs];
                    snprintf(execute_info, 256, "JR to 0x%08x", pc);
                    snprintf(instr.inst_str, sizeof(instr.inst_str), "jr %s", "rs");
                    break;
                // 다른 R-type 명령어 처리 추가
                default:
                    snprintf(instr.inst_str, sizeof(instr.inst_str), "Unknown R-type funct: %02x", instr.funct);
                    break;
            }
            break;
        case I_TYPE:
            switch (instr.opcode) {
                case 0x08: // ADDI
                    *alu_result = registers[instr.rs] + (int16_t)instr.imm;
                    snprintf(execute_info, 256, "ALU result: 0x%08x", *alu_result);
                    snprintf(instr.inst_str, sizeof(instr.inst_str), "addi %s, %s, %04x", "rt", "rs", instr.imm);
                    break;
                // 다른 I-type 명령어 처리 추가
                default:
                    snprintf(instr.inst_str, sizeof(instr.inst_str), "Unknown I-type opcode: %02x", instr.opcode);
                    break;
            }
            break;
        case J_TYPE:
            switch (instr.opcode) {
                case 0x02: // J
                    pc = (instr.address << 2);
                    snprintf(execute_info, 256, "Jump to 0x%08x", pc);
                    snprintf(instr.inst_str, sizeof(instr.inst_str), "j %08x", instr.address);
                    break;
                case 0x03: // JAL
                    registers[31] = pc;
                    pc = (instr.address << 2);
                    snprintf(execute_info, 256, "Jump and Link to 0x%08x", pc);
                    snprintf(instr.inst_str, sizeof(instr.inst_str), "jal %08x", instr.address);
                    break;
                default:
                    snprintf(instr.inst_str, sizeof(instr.inst_str), "Unknown J-type opcode: %02x", instr.opcode);
                    break;
            }
            break;
        default:
            snprintf(instr.inst_str, sizeof(instr.inst_str), "Unknown type: %02x", instr.opcode);
            break;
    }
}

// 메모리 액세스를 처리하는 함수
void memory_access(DecodedInstr instr, uint32_t *alu_result, char *memory_access_info) {
    if (instr.type == UNKNOWN_TYPE) return;

    // 메모리 액세스는 I-type 명령어에서만 발생
    if (instr.type == I_TYPE) {
        switch (instr.opcode) {
            case 0x23: // LW
                if (*alu_result >= MEM_SIZE * 4) {
                    fprintf(stderr, "메모리 로드 주소가 범위를 벗어났습니다: 0x%08x\n", *alu_result);
                    return;
                }
                *alu_result = memory[*alu_result / 4];
                snprintf(memory_access_info, 256, "Load, Address: 0x%08x, Value: 0x%08x", *alu_result, registers[instr.rt]);
                snprintf(instr.inst_str, sizeof(instr.inst_str), "lw %s, %04x(%s)", "rt", instr.imm, "rs");
                break;
            case 0x2B: // SW
                if (*alu_result >= MEM_SIZE * 4) {
                    fprintf(stderr, "메모리 저장 주소가 범위를 벗어났습니다: 0x%08x\n", *alu_result);
                    return;
                }
                memory[*alu_result / 4] = registers[instr.rt];
                snprintf(memory_access_info, 256, "Store, Address: 0x%08x, Value: 0x%08x", *alu_result, registers[instr.rt]);
                snprintf(instr.inst_str, sizeof(instr.inst_str), "sw %s, %04x(%s)", "rt", instr.imm, "rs");
                break;
            default:
                snprintf(instr.inst_str, sizeof(instr.inst_str), "Unknown memory access opcode: %02x", instr.opcode);
                break;
        }
    } else {
        snprintf(memory_access_info, 256, "Pass");
    }
}

// 결과를 레지스터 파일에 쓰는 함수
void write_back(DecodedInstr instr, uint32_t alu_result, char *write_back_info) {
    if (instr.type == UNKNOWN_TYPE) return;

    if (instr.type == R_TYPE && instr.funct != 0x08) {
        registers[instr.rd] = alu_result;
        snprintf(write_back_info, 256, "Target: %s, Value: 0x%08x / newPC: 0x%08x", "rd", alu_result, pc);
    } else if (instr.type == I_TYPE) {
        registers[instr.rt] = alu_result;
        snprintf(write_back_info, 256, "Target: %s, Value: 0x%08x / newPC: 0x%08x", "rt", alu_result, pc);
    } else {
        snprintf(write_back_info, 256, "newPC: 0x%08x", pc);
    }
}

// 각 단계마다 상태를 출력하는 함수
void print_stage_info(uint32_t cycle, DecodedInstr instr, const char *fetch_info, const char *decode_info, const char *execute_info, const char *memory_access_info, const char *write_back_info) {
    printf("%08x> Cycle: %d\n", instr.raw, cycle);
    printf("[Instruction Fetch] %s\n", fetch_info);
    printf("[Instruction Decode] %s\n", decode_info);
    printf("[Execute] %s\n", execute_info);
    printf("[Memory Access] %s\n", memory_access_info);
    printf("[Write Back] %s\n", write_back_info);
}

// 에뮬레이터를 실행하는 함수
void run_emulator(const char *filename) {
    load_memory(filename);

    // 초기 레지스터 값 설정
    registers[29] = 0x1000000; // 스택 포인터 설정
    registers[31] = 0xFFFFFFFF; // 리턴 어드레스 설정

    while (pc != 0xFFFFFFFF) {
        cycle++;
        DecodedInstr instr = fetch();
        char fetch_info[256];
        snprintf(fetch_info, sizeof(fetch_info), "0x%08x (PC=0x%08x)", instr.raw, pc - 4);

        // 명령어가 0x00000000인 경우 프로그램을 종료하도록 설정
        if (instr.raw == 0x00000000) {
            printf("NOP 명령어를 만났습니다. 프로그램을 종료합니다.\n");
            break;
        }

        instr = decode(instr);
        char decode_info[256];
        snprintf(decode_info, sizeof(decode_info), "Type: %s, Inst: %s\nopcode: %02x, rs: %02x (%08x), rt: %02x (%08x), rd: %02x (%08x), shamt: %02x, funct: %02x\nRegDst: x, RegWrite: x, ALUSrc: x, PCSrc: x, MemRead: x, MemWrite: x, MemtoReg: x, ALUOp: xx",
                 (instr.type == R_TYPE ? "R" : (instr.type == I_TYPE ? "I" : "J")),
                 instr.inst_str,
                 instr.opcode, instr.rs, registers[instr.rs], instr.rt, registers[instr.rt], instr.rd, registers[instr.rd],
                 instr.shamt, instr.funct);

        uint32_t alu_result = 0;
        char execute_info[256];
        execute(instr, &alu_result, execute_info);

        char memory_access_info[256];
        memory_access(instr, &alu_result, memory_access_info);

        char write_back_info[256];
        write_back(instr, alu_result, write_back_info);

        print_stage_info(cycle, instr, fetch_info, decode_info, execute_info, memory_access_info, write_back_info);

        // PC 범위 체크
        if (pc >= MEM_SIZE * 4) {
            printf("PC가 메모리 범위를 벗어났습니다: 0x%08x\n", pc);
            break;
        }

        // JR $RA 명령어를 만나고 $RA가 0xFFFFFFFF이면 프로그램을 종료
        if (instr.opcode == 0x00 && instr.funct == 0x08 && registers[instr.rs] == 0xFFFFFFFF) {
            break;
        }
    }

    printf("%08x> Final Result\n", pc);
    printf("Cycles: %d, R-type instructions: %d, I-type instructions: %d, J-type instructions: %d\n",
           cycle, instr_count[R_TYPE], instr_count[I_TYPE], instr_count[J_TYPE]);
    printf("Return value (v0): 0x%x\n", registers[2]); // v0 레지스터의 값
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <binary_file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    run_emulator(argv[1]);
    return EXIT_SUCCESS;
}
