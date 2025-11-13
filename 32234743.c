#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_S_REGISTERS 8
#define MAX_T_REGISTERS 10
#define MAX_LINES 100

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "r");
    if (file == NULL) {
        perror("파일을 열수 없음");
        return 1;
    }

    char lines[MAX_LINES][100]; // jmp, bne, beq를 위한 라인수 저장배열
    int lineCount = 0;
    while (fgets(lines[lineCount], sizeof(lines[lineCount]), file)) {
        lineCount++;
    }
    fclose(file);

    int s_registers[MAX_S_REGISTERS] = {0}; // s 레지스터 저장배열
    int t_registers[MAX_T_REGISTERS] = {0}; // t 레지스터 저장배열
    int r0 = 0; // r0 레지스터

    // 라인 별 한줄씩 명령어 확인
    for (int currentLine = 0; currentLine < lineCount; currentLine++) {
        // printf("currentLine %d\n", currentLine);
        char *line = lines[currentLine];
        char op[10], dst[10], src1[10], src2[10];
        int value, index1, index2, jumpLine;
        // 명령어가 LW인 경우
        if (sscanf(line, "%s %s %x", op, dst, &value) == 3 && strcmp(op, "LW") == 0) {
            int dstIndex = dst[1] - '0';
            if (dst[0] == 's' && dstIndex >= 0 && dstIndex < MAX_S_REGISTERS) { //해당 s레지스터 위치에 값 저장
                s_registers[dstIndex] = value;
                printf("Loaded %d to s%d\n", value, dstIndex);
            }
            continue;
        }
        // 명령어가 JMP인 경우
        if (sscanf(line, "%s %x", op, &jumpLine) == 2 && strcmp(op, "JMP") == 0) {
            if (jumpLine > 0 && jumpLine <= lineCount) {
                currentLine = jumpLine - 2; 
                // 읽어드린 줄번호의 0기반 인덱스 이전번호로 이동(continue시 currentLine++을 수행하기 때문)
            } else {
                printf("Invalid jump line %d\n", jumpLine);
                break; // 점프 범위가 벗어나면 프로그램 종료
            }
            continue;
        }
        // 명령어가 BNE, BEQ인 경우
        if (sscanf(line, "%s %s %s %x", op, src1, src2, &jumpLine) == 4 && (strcmp(op, "BEQ") == 0 || strcmp(op, "BNE") == 0)) {
            index1 = src1[0] == 's' ? s_registers[src1[1] - '0'] : (src1[0] == 't' ? t_registers[src1[1] - '0'] : 0);
            index2 = src2[0] == 's' ? s_registers[src2[1] - '0'] : (src2[0] == 't' ? t_registers[src2[1] - '0'] : 0);
            // 읽어드린 레지스터가 s인 경우, t 인경우, zero인 경우 저장했던 해당 위치의 값을 index1과 index2에 받아옴

            if (strcmp(op, "BEQ") == 0 && index1 == index2) {   //BEQ의 경우 -> 값이 동일한 경우
                currentLine = jumpLine - 2; // 읽어드린 줄번호의 0기반 인덱스 이전번호로 이동
                printf("%s: %s(%d) == %s(%d) and Jumped line %d\n", op, src1, index1, src2, index2, jumpLine);
                continue;
            } else if (strcmp(op, "BNE") == 0 && index1 != index2) {    //BNE의 경우 -> 값이 동일하지 않은 경우
                currentLine = jumpLine - 2; //읽어드린 줄번호의 0기반 인덱스 이전번호로 이동
                printf("%s: %s(%d) != %s(%d) and Jumped line %d\n", op, src1, index1, src2, index2, jumpLine);
                continue;
            }
            printf("%s: %s(%d) and %s(%d)\n", op, src1, index1, src2, index2);

        // BNE, BEQ 이외의 명령어인 경우 ADD, SUB, MUL, DIV, SLT 명령어 처리
        } else if (sscanf(line, "%s %s %s %s", op, dst, src1, src2) == 4) {
            int *dest = (dst[0] == 't') ? &t_registers[dst[1] - '0'] : (dst[0] == 's' ? &s_registers[dst[1] - '0'] : &r0);
            // 계산 처리후 저장할 레지스터 t인경우, s인경우, r0인 경우
            index1 = src1[0] == 's' ? s_registers[src1[1] - '0'] : (src1[0] == 't' ? t_registers[src1[1] - '0'] : 0);
            index2 = src2[0] == 's' ? s_registers[src2[1] - '0'] : (src2[0] == 't' ? t_registers[src2[1] - '0'] : 0);
            // 읽어드린 레지스터가 s인 경우, t 인경우, zero인 경우 저장했던 해당 위치의 값을 index1과 index2에 받아옴
        
            if (strcmp(op, "ADD") == 0) {           // ADD의 경우 덧셈
                *dest = index1 + index2;
            } else if (strcmp(op, "SUB") == 0) {    // SUB의 경우 뺄셈
                *dest = index1 - index2;
            } else if (strcmp(op, "MUL") == 0) {    // MUL의 경우 곱셈
                *dest = index1 * index2;
            } else if (strcmp(op, "DIV") == 0) {    // DIV의 경우 나눗셈
                if (index2 != 0) { 
                    *dest = index1 / index2;
                } else {    //분모가 0이 아닌 경우 예외 처리
                    printf("Cannot divide by zero.\n");
                    continue;
                }
            } else if (strcmp(op, "SLT") == 0) {    // SLT의 경우, index1 <index2 이면 dst에 1, 아니면 0 저장
                *dest = (index1 < index2) ? 1 : 0;
            }
            printf("%sed %s(%d) and %s(%d) and changed %s to %d\n", op, src1, index1, src2, index2, dst, *dest);
        } else if (strcmp(line, "NOP") == 0) {      // NOP의 경우
            printf("No operation\n");
        }
    }
    // 최종 결과 값 출력
    printf("Final result: r0 = %d\n", r0);
    return 0;
}
