/*========== convert MC6809 to MC68000 source utility ==========*/
/*
 * Version 1.2
 * Systems Engg, E. Kilbride, Scotland
 * Motorola Inc. Copyright 1986
 * 
 * 1.1 : set lenopc after codes2 match (ln610)
 * 1.2 : include command line param test
 *
 * 1.2.1: Converted from Pascal xlate09.pas to C11
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* Constants */
#define VERSION        "1.2.2"
#define VDATE          "2026-07-06"


#define IMAGE_LEN      256  /* input line length */
#define LABEL_LEN      8    /* label length */
#define OPCODE_LEN     5    /* opcode length */
#define MAX_MACRO      256  /* max no. of macro names allowed */
#define CHAR_LEN       256  /* general character length */
#define INST_NUM       256  /* no. of entries in codes file */
#define MAX_OPRLN      6    /* max number of output line entries */

/* Type definitions */
typedef enum { WARNING, ERROR } ErrorType;

typedef struct {
    char image[IMAGE_LEN + 1];      /* input image */
    char lbl[LABEL_LEN + 1];        /* label field */
    char opc[CHAR_LEN + 1];         /* opcode field */
    char opr[CHAR_LEN + 1];         /* operand */
    char opr2[CHAR_LEN + 1];        /* operand no.2 */
    char comment[IMAGE_LEN + 1];    /* comment */
    int lines;                       /* total input lines read */
    int comm_in;                     /* input comment line count */
    int reg;                         /* inherent register number */
    int errors;                      /* diagnostic count - errors */
    int warnings;                    /* diagnostic count - warnings */
    int linesout;                    /* code lines into output file */
    int comm_out;                    /* output comment line count */
    bool passthis;                   /* pass this record thru unaltered */
    bool passflag;                   /* pass flag for general pass */
    bool indirect;                   /* indirect address mode flag */
    bool pcr;                        /* pcr addr mode flag */
    bool direct;                     /* direct address mode flag */
    bool problem;                    /* fatal error flag */
    bool xflag;                      /* indicates type of indirection */
    bool interleave;                 /* 6809 source interleave flag */
    char t;
    bool X, Y, U, S, A, B, DP, CC, PC, D;  /* partial register set */
    bool DPR, DPW;                   /* DP reg read and write flags */
    bool label_flag;                 /* prevents multiple label prints */
    char index;
    char first;
    char t2[CHAR_LEN + 1];           /* temp */
    int  posi;
    int  i;
    int  temp;
    int  temp2;
    char opcode[INST_NUM + 1][OPCODE_LEN + 1];
    char opcode2[INST_NUM + 1][OPCODE_LEN + 1];
    char expres[INST_NUM + 1][CHAR_LEN + 1];
    char expres2[INST_NUM + 1][CHAR_LEN + 1];
    char oprln[MAX_OPRLN][CHAR_LEN + 1];
    char macr_name[MAX_MACRO + 1][LABEL_LEN + 1];
    int  count;                       /* macro name array pointer */
    bool macro;                      /* macro flag */
    int  lenopr;
    char regnum;
    char siz[OPCODE_LEN + 1];
    char opc1t[CHAR_LEN + 1];
    int  pos2;
    char last1;
    int  lenopc;
    int  z;
    char delim;
    char optab[OPCODE_LEN + 1];
    int  lenoptab;
    char tempop[OPCODE_LEN + 1];
    bool match;
    bool match2;
    char strg1[CHAR_LEN + 1];
    char strg2[CHAR_LEN + 1];
    char strg3[CHAR_LEN + 1];
    int  auto_inc;                    /* auto increment/decrement counter */
    char esc;
} State;

FILE *infile, *outfile, *errorfile, *codes, *codes2, *stubxref;

/* Function prototypes */
void replace(State *s, const char *strg1, const char *strg2, int z);
void create_line(State *s, char delim, int z);
void make_list(State *s, int temp);
void diagnostic(State *s, ErrorType severity, const char *message);
void create_file(State *s);
void check_comment(State *s, int y);
void convert(State *s);
bool getstmt(State *s);
void init_state(State *s);
void cleanup(void);

/* Utility functions */

/* Find position of substring in string (like Pascal pos) */
int str_pos(const char *str, const char *substr) {
    char *p = strstr(str, substr);
    
    if (p == NULL)
        return 0;

    return (int)(p - str) + 1;
}

/* Replace first occurrence of strg1 with strg2 in oprln[z] */
void replace(State *s, const char *strg1, const char *strg2, int z) {
    int pos = str_pos(s->oprln[z], strg1);

    if (pos != 0) {
        pos--;  /* Convert to 0-based index */
        int len1 = strlen(strg1);
        char temp[CHAR_LEN + 1] = {0};
        
        strncpy(temp, s->oprln[z], pos);
        strcat(temp, strg2);
        strcat(temp, s->oprln[z] + pos + len1);
        strcpy(s->oprln[z], temp);
        
        s->pos2 = pos;
    } else {
        s->pos2 = 0;  /* Not found — always reset, matching Pascal pos() behaviour */
    }
}

/* Delete n characters at position pos in string */
void str_delete(char *str, int pos, int n) {
    if (pos < 0 || pos >= (int)strlen(str))
        return;
    memmove(str + pos, str + pos + n, strlen(str) + 1 - pos - n);
}

/* Insert string at position pos */
void str_insert(char *str, const char *substr, int pos) {
    if (pos < 0 || pos > (int)strlen(str))
        return;

    char temp[CHAR_LEN + 1];

    strncpy(temp, str, pos);
    temp[pos] = '\0';

    strcat(temp, substr);
    strcat(temp, str + pos);
    strcpy(str, temp);
}

/* Copy substring of str starting at pos with length len */
char* str_copy(char *dest, const char *src, int pos, int len) {
    if (pos < 0 || pos >= (int)strlen(src))
        return dest;

    if (pos + len > (int)strlen(src))
        len = strlen(src) - pos;

    strncpy(dest, src + pos, len);
    dest[len] = '\0';

    return dest;
}

void create_line(State *s, char delim, int z) {
    fprintf(stderr, "DEBUG create_line: z=%d, oprln[%d][0]='%c', copying?\n", 
            z, z, s->oprln[z][0]);
    int i = strlen(s->oprln[z]);
    char delim_str[2] = { delim, '\0' };

    replace(s, delim_str, " ", z);
    fprintf(stderr, "  After replace: pos2=%d, i=%d\n", s->pos2, i);

    if (s->pos2 != i && s->pos2 != 0) {
        fprintf(stderr, "  SPLITTING LINE\n");
        int z_idx = 5;

        while (z_idx > z && strlen(s->oprln[z_idx]) == 0)
            z_idx--;
        
        while (z_idx > z) {
            strcpy(s->oprln[z_idx + 1], s->oprln[z_idx]);
            z_idx--;
        }
        
        str_copy(s->oprln[z + 1], s->oprln[z], s->pos2 + 1,
                strlen(s->oprln[z]) - s->pos2);
        str_copy(s->oprln[z], s->oprln[z], 0, s->pos2);
    }
}

void make_list(State *s, int temp) {
    replace(s, "o", "", temp);
    strcpy(s->t2, s->oprln[temp]);
    
    if (s->S)  str_insert(s->t2, "/A6", s->pos2);
    if (s->U)  str_insert(s->t2, "/A5", s->pos2);
    if (s->DP) str_insert(s->t2, "/A4", s->pos2);
    if (s->Y)  str_insert(s->t2, "/A1", s->pos2);
    if (s->X)  str_insert(s->t2, "/A0", s->pos2);
    if (s->B)  str_insert(s->t2, "/D1", s->pos2);
    if (s->A)  str_insert(s->t2, "/D0", s->pos2);
    if (s->D)  str_insert(s->t2, "/D0/D1", s->pos2);
    
    strcpy(s->oprln[temp], s->t2);
    replace(s, "/", "", temp);
}

void diagnostic(State *s, ErrorType severity, const char *message) {
    if (severity == WARNING) {
        fprintf(outfile, "** WARNING **%22s%s\n", "", message);
        s->comm_out++;
        s->warnings++;
    } else {
        fprintf(outfile, "** ERROR ** %s\n", message);
        fprintf(outfile, " * %s\n", s->image);
        s->comm_out += 2;
        fprintf(errorfile, "%6d%10d     %s\n", s->lines, s->linesout + s->comm_out, s->image);
        fprintf(errorfile, "%31s%s\n", "", message);
        s->problem = true;
        s->errors++;
    }
}

void create_file(State *s) {
    s->z = 0;
    s->label_flag = true;
    
    if (s->interleave) {
        fprintf(outfile, "\n");
        fprintf(outfile, "* %s%*s%s\n", s->opc, (int)(8 - strlen(s->opc)), "", s->opr2);
        s->comm_out += 2;
    }
    
    while (strlen(s->oprln[s->z]) > 0 && s->z <= 5) {
        /* DEBUG: Check if this is our problematic LEA */
        if (strcmp(s->opc1t, "LEA") == 0 && (s->index == 'X' || s->index == 'Y')) {
            fprintf(stderr, "DEBUG create_file z=%d: oprln[%d]='%s' (len=%lu)\n", 
                    s->z, s->z, s->oprln[s->z], strlen(s->oprln[s->z]));
        }
        
        /* Only call create_line for non-warning lines (those not starting with '*') */
        if (s->oprln[s->z][0] != '*') {
            create_line(s, ';', s->z);
        }
        char regnum_str[2] = { s->regnum, '\0' };
        replace(s, "\\", regnum_str, s->z);
        replace(s, "o", s->opr, s->z);
        strcpy(s->t2, s->oprln[s->z]);
        
        if (s->t2[0] == '*') {
            diagnostic(s, WARNING, s->t2);
        } else {
            int pos_dot = str_pos(s->oprln[s->z], ".");

            if (pos_dot > 0 && pos_dot < (int)strlen(s->oprln[s->z])) {
                if (s->oprln[s->z][pos_dot] == ' ')
                    replace(s, ".", s->siz, s->z);
            }
            
            if (s->label_flag) {
                fprintf(outfile, "%s%*s", s->lbl, (int)(10 - strlen(s->lbl)), "");
            } else {
                fprintf(outfile, "%10s", "");
            }
            
            fprintf(outfile, "%s", s->oprln[s->z]);
            int temp_len = strlen(s->oprln[s->z]);
            
            if (temp_len < 20) {
                fprintf(outfile, "%*s", (int)(22 - temp_len), "");
                s->temp = 33;
            } else {
                fprintf(outfile, "%3s", "");
                s->temp = temp_len + 14;
            }
            
            if (s->label_flag && strlen(s->comment) > 0) {
                int comment_space = 81 - s->temp;
                if ((int)strlen(s->comment) < comment_space) {
                    fprintf(outfile, "%s", s->comment);
                } else {
                    fprintf(outfile, "%.*s\n", comment_space - 1, s->comment);
                    int remaining = strlen(s->comment) - comment_space + 1;
                    fprintf(outfile, "*%31s%.*s", "", remaining,
                            s->comment + comment_space - 1);
                }
                s->label_flag = false;
            }
            
            fprintf(outfile, "\n");
            s->linesout++;
        }
        s->z++;
    }
}

void check_comment(State *s, int y) {
    if (str_pos(s->oprln[y], "o") == 0) {
        char temp[CHAR_LEN + 1];
        snprintf(temp, sizeof(temp), "%s %s", s->opr, s->comment);
        strcpy(s->comment, temp);
        strcpy(s->opr, "");
        strcpy(s->opr2, "");
    }
}

void convert(State *s) {
    for (s->z = 0; s->z <= 5; s->z++)
        strcpy(s->oprln[s->z], "");
    
    if (s->passthis) {
        fprintf(outfile, "%s\n", s->image);
        s->comm_out++;
        s->comm_in++;
        return;
    }
    
    if (s->problem)
        return;
    
    s->last1 = s->opc[strlen(s->opc) - 1];
    s->macro = false;
    strcpy(s->siz, ".B");
    s->temp = 0;
    
    if (s->match2) {
        strcpy(s->oprln[0], s->expres2[s->posi - 1]);
        check_comment(s, s->temp);
        create_file(s);
    } else {
        s->posi = 0;
        s->match = false;
        
        while (!s->match && s->posi <= INST_NUM && strlen(s->opcode[s->posi]) > 0) {
            strcpy(s->optab, s->opcode[s->posi]);
            s->lenoptab = strlen(s->optab);
            
            if (s->optab[0] == '*') {
                if (s->lenopc == s->lenoptab) {
                    str_copy(s->tempop, s->optab, 1, s->lenoptab - 1);
                    if (strcmp(s->tempop, s->opc1t) == 0) {
                        s->match = true;
                        if (s->last1 == 'A' || s->last1 == 'B')
                            strcpy(s->siz, ".B");
                        else
                            strcpy(s->siz, ".W");
                        
                        switch (s->last1) {
                            case 'A': case 'X': s->regnum = '0'; break;
                            case 'B': case 'Y': s->regnum = '1'; break;
                            case 'U': s->regnum = '5'; break;
                            case 'S': s->regnum = '6'; break;
                            case 'D': 
                                s->regnum = '2';
                                s->D = true;
                                break;
                            default:
                                diagnostic(s, ERROR, "Invalid implied register");
                        }
                    }
                }
            } else {
                if (strcmp(s->opc, s->optab) == 0) {
                    s->match = true;
                } else {
                    s->z = 0;
                    while (!s->match && s->z <= MAX_MACRO && strlen(s->macr_name[s->z]) > 0) {
                        if (strcmp(s->opc, s->macr_name[s->z]) == 0) {
                            s->macro = true;
                            s->match = true;
                        } else {
                            s->z++;
                        }
                    }
                }
            }
            s->posi++;
        }
        
        if (s->problem)
            return;
        
        if (s->match) {
            switch (s->index) {
                case 'X': s->t = '0'; break;
                case 'Y': s->t = '1'; break;
                case 'U': s->t = '5'; break;
                case 'S': s->t = '6'; break;
                default: s->t = '0';
            }
            
            if (s->D && strcmp(s->opc1t, "PSH") != 0 && strcmp(s->opc1t, "PUL") != 0) {
                strcpy(s->oprln[s->temp], "BSR ..DIN");
                s->temp++;
            }
            
            if (s->DPR) {
                strcpy(s->oprln[s->temp], "BSR ..DPR;* CCR MODIFIED *");
                s->temp++;
            }
            
            if (s->auto_inc < 0 && s->auto_inc > -3) {
                if ((((strcmp(s->siz, ".W") == 0) && (s->auto_inc == -2)) ||
                     ((strcmp(s->siz, ".B") == 0) && (s->auto_inc == -1))) &&
                    strcmp(s->opc1t, "LEA") != 0 && s->opc[0] != 'J') {
                    char temp[CHAR_LEN + 1];
                    snprintf(temp, sizeof(temp), "-%s", s->opr);
                    strcpy(s->opr, temp);
                } else {
                    s->z = s->auto_inc + 3;
                    char temp[CHAR_LEN + 1];
                    switch (s->z) {
                        case 1:
                            snprintf(temp, sizeof(temp), "SUBQ.L #2,A%c", s->t);
                            break;
                        case 2:
                            snprintf(temp, sizeof(temp), "SUBQ.L #1,A%c", s->t);
                            break;
                    }
                    strcpy(s->oprln[s->temp], temp);
                    s->temp++;
                }
            }
            
            if (s->macro) {
                char temp[CHAR_LEN + 1];
                snprintf(temp, sizeof(temp), "%s o ;", s->opc);
                strcpy(s->oprln[s->temp], temp);
            } else {
                strcpy(s->oprln[s->temp], s->expres[s->posi - 1]);
            }
            
            check_comment(s, s->temp);
            
            if (strcmp(s->opc, "END") == 0) {
                if (strcmp(s->opr, "") == 0) {
                    strcpy(s->oprln[s->temp], "END");
                } else {
                    strcpy(s->lbl, "..START");
                }
            }
            
            s->pos2 = str_pos(s->oprln[s->temp], "p");
            if (s->pos2 != 0) {
                str_delete(s->oprln[s->temp], s->pos2 - 1, 1);
                s->pos2 = str_pos(s->opr, "(PC)");
                if (s->pos2 != 0 && !s->indirect) {
                    str_delete(s->opr, s->pos2 - 1, 4);
                    char temp[CHAR_LEN + 1];
                    snprintf(temp, sizeof(temp), "%s(A2)", s->opr);
                    strcpy(s->opr, temp);
                    
                    char line[CHAR_LEN + 1];
                    snprintf(line, sizeof(line), "LEA.L 0(PC),A2;%s", s->oprln[s->temp]);
                    strcpy(s->oprln[s->temp], line);
                    s->pcr = true;
                }
            }
            
            if (s->direct) {
                s->pos2 = str_pos(s->opr, "(");
                if (s->pos2 != 0) {
                    diagnostic(s, WARNING, "Mixed addressing mode - indexed assumed");
                } else {
                    char temp[CHAR_LEN + 1];
                    snprintf(temp, sizeof(temp), "%s(A4)", s->opr);
                    strcpy(s->opr, temp);
                }
            }
            
            if (strcmp(s->opc1t, "PSH") == 0) {
                if (!s->CC)
                    replace(s, "MOVE.W SR,-(A\\);", "", s->temp);
                if (!s->PC)
                    replace(s, "LEA.L 0(PC),A3;MOVE.L A3,-(A\\);", "", s->temp);
                make_list(s, s->temp);
                if (s->pos2 == 0)
                    replace(s, "MOVEM.L ,-(A\\)", "", s->temp);
            }
            
            if (strcmp(s->opc1t, "PUL") == 0) {
                if (!s->CC)
                    replace(s, "MOVE.W (A\\)+,SR;", "", s->temp);
                if (!s->PC)
                    replace(s, "MOVE.L (A\\)+,A3;JMP (A3);", "", s->temp);
                make_list(s, s->temp);
                if (s->pos2 == 0)
                    replace(s, "MOVEM.L (A\\)+,;", "", s->temp);
            }
            
            if (s->indirect) {
                strcpy(s->oprln[s->temp + 1], s->oprln[s->temp]);
                strcpy(s->oprln[s->temp], "MOVE.L o,A2");
                s->temp++;
                replace(s, "o", "(A2)", s->temp);
                replace(s, "o", "(A2)", s->temp);
            }
            
            if (s->optab[0] == '*') {
                create_line(s, '^', s->temp);
                if (s->pos2 != 0) {
                    switch (s->last1) {
                        case 'X': case 'Y': case 'S': case 'U':
                            strcpy(s->oprln[s->temp], s->oprln[s->temp + 1]);
                            break;
                        default:
                            break;
                    }
                    strcpy(s->oprln[s->temp + 1], "");
                }
            }
            
            s->temp++;
            
            if (s->auto_inc > 0 && s->auto_inc < 3) {
                if ((((strcmp(s->siz, ".W") == 0) && (s->auto_inc == 2)) ||
                     ((strcmp(s->siz, ".B") == 0) && (s->auto_inc == 1))) &&
                    strcmp(s->opc1t, "LEA") != 0 && s->opc[0] != 'J') {
                    char temp[CHAR_LEN + 1];
                    snprintf(temp, sizeof(temp), "%s+", s->opr);
                    strcpy(s->opr, temp);
                } else {
                    char temp[CHAR_LEN + 1];
                    switch (s->auto_inc) {
                        case 1:
                            snprintf(temp, sizeof(temp), "ADDQ.L #1,A%c", s->t);
                            break;
                        case 2:
                            snprintf(temp, sizeof(temp), "ADDQ.L #2,A%c", s->t);
                            break;
                    }
                    strcpy(s->oprln[s->temp], temp);
                }
                
                if (strcmp(s->opc, "JSR") == 0) {
                    char temp[CHAR_LEN + 1];
                    snprintf(temp, sizeof(temp), "%s;BSR  ..JSR", s->oprln[s->temp]);
                    strcpy(s->oprln[s->temp], temp);
                    int semi_pos = str_pos(s->oprln[s->temp - 1], ";");
                    if (semi_pos > 0)
                        s->oprln[s->temp - 1][semi_pos - 1] = '\0';
                }
                
                if (strcmp(s->opc, "JMP") == 0) {
                    strcpy(s->oprln[s->temp - 1], s->oprln[s->temp]);
                    strcpy(s->oprln[s->temp], "JMP o ;");
                    char temp[CHAR_LEN + 1];
                    snprintf(temp, sizeof(temp), "-%c%s", s->t, s->opr);
                    strcpy(s->opr, temp);
                }
                
                s->temp++;
            }
            
            /* Remove this block - we'll add the warning AFTER create_file */
            /*
            if ((strcmp(s->opc1t, "LEA") == 0) && 
                ((s->index == 'X') || (s->index == 'Y'))) {
                fprintf(stderr, "DEBUG: Adding Z-BIT to oprln[%d]\n", s->temp);
                strcpy(s->oprln[s->temp], "* Z-BIT NOT MODIFIED *");
                s->temp++;
            }
            */
            
            if (s->D && strcmp(s->opc1t, "PSH") != 0 && strcmp(s->opc1t, "PUL") != 0)
                strcpy(s->oprln[s->temp], "BSR ..DOUT");
            
            if (s->DPW)
                strcpy(s->oprln[s->temp], "BSR ..DPW");
            
            /* DEBUG: Check oprln contents before create_file */
            if (strcmp(s->opc1t, "LEA") == 0 && (s->index == 'X' || s->index == 'Y')) {
                fprintf(stderr, "DEBUG LEA: pos2=%d\n", s->pos2);
                for (int dbg = 0; dbg <= 5; dbg++) {
                    if (strlen(s->oprln[dbg]) > 0)
                        fprintf(stderr, "  oprln[%d]='%s'\n", dbg, s->oprln[dbg]);
                }
            }
            
            create_file(s);
            
            /* Add Z-BIT warning AFTER create_file to avoid shifting issues */
            if ((strcmp(s->opc1t, "LEA") == 0) && 
                ((s->index == 'X') || (s->index == 'Y'))) {
                diagnostic(s, WARNING, "* Z-BIT NOT MODIFIED *");
            }
        } else {
            diagnostic(s, ERROR, "Unable to translate");
        }
    }
}

bool getstmt(State *s) {
    int i, j;
    
    strcpy(s->lbl, "");
    strcpy(s->opc, "");
    strcpy(s->opr, "");
    strcpy(s->opr2, "");
    strcpy(s->comment, "");
    s->passthis = false;
    s->problem = false;
    s->indirect = false;
    s->direct = false;
    s->match = false;
    s->match2 = false;
    s->pcr = false;
    s->A = s->B = s->DP = s->CC = s->X = s->Y = s->U = s->S = s->PC = s->D = false;
    s->DPR = s->DPW = false;
    i = 0;
    s->auto_inc = 0;
    s->index = 'A';
    
    if (fgets(s->image, sizeof(s->image), infile) == NULL)
        return false;
    
    s->image[strcspn(s->image, "\r\n")] = '\0';  /* Remove newline and CR */
    s->lines++;
    
    if ((s->lines % 10) == 0) {
        printf("%8d%15d%15d%16d\n", s->lines - s->comm_in, s->linesout, 
               s->errors, s->warnings);
    }
    
    /* Ignore line numbers if present */
    if (strlen(s->image) > 0 && isdigit(s->image[0])) {
        while (i < (int)strlen(s->image) && s->image[i] != ' ')
            i++;
        memmove(s->image, s->image + i, strlen(s->image) - i + 1);
        i = 0;
    }
    
    /* Check special bypass mode indicators */
    if (strcmp(s->image, "**PASS") == 0)
        s->passflag = true;
    if (strcmp(s->image, "**PASSOFF") == 0)
        s->passflag = false;
    if (s->passflag)
        s->passthis = true;
    
    /* Skip empty or comment statements */
    if (strlen(s->image) == 0 || s->image[0] == '*') {
        s->passthis = true;
        return true;
    }
    
    /* Read label */
    if (s->image[0] != ' ') {
        i = 0;
        while (i < (int)strlen(s->image) && s->image[i] != ' ')
            i++;
        strncpy(s->lbl, s->image, i);
        s->lbl[i] = '\0';
    }
    
    /* Skip to next non-blank */
    while (i < (int)strlen(s->image) && s->image[i] == ' ')
        i++;
    
    /* Read opcode */
    j = i;
    while (i < (int)strlen(s->image) && s->image[i] != ' ')
        i++;
    strncpy(s->opc, s->image + j, i - j);
    s->opc[i - j] = '\0';
    
    /* Convert to uppercase for matching */
    for (char *p = s->opc; *p; p++)
        *p = toupper(*p);
    
    if (strcmp(s->opc, "MACR") == 0) {
        strcpy(s->macr_name[s->count], s->lbl);
        if (s->count < MAX_MACRO)
            s->count++;
        else
            diagnostic(s, ERROR, "Too many macro names");
    }
    
    /* Strip off unnecessary "L" in "LBxx" instructions */
    if (strlen(s->opc) >= 2 && s->opc[0] == 'L' && s->opc[1] == 'B')
        str_delete(s->opc, 0, 1);
    
    /* Read operand */
    while (i < (int)strlen(s->image) && s->image[i] == ' ')
        i++;
    
    if (i >= (int)strlen(s->image)) {
        goto end_of_operand;
    }
    
    j = i;
    while (i < (int)strlen(s->image) && s->image[i] != ' ')
        i++;
    
    if (strcmp(s->opc, "FCC") == 0) {
        char t = s->image[j];
        int pos2 = j + 1;
        while (pos2 < (int)strlen(s->image) && s->image[pos2] != t)
            pos2++;
        if (pos2 < (int)strlen(s->image))
            i = pos2 + 1;
    }
    
    strncpy(s->opr, s->image + j, i - j);
    s->opr[i - j] = '\0';
    strcpy(s->opr2, s->opr);
    
    /* Remove redundant force extended ">" character */
    if ((s->opr[0] == '>') || (s->opr[0] == '<' && strlen(s->opr) > 1 && s->opr[1] == '['))
        str_delete(s->opr, 0, 1);
    
    /* Skip blanks for comment */
    while (i < (int)strlen(s->image) && s->image[i] == ' ')
        i++;
    
    if (i < (int)strlen(s->image))
        strcpy(s->comment, s->image + i);
    
    s->indirect = (s->opr[0] == '[');
    if (s->indirect) {
        int pos = str_pos(s->opr, "[");
        if (pos > 0)
            str_delete(s->opr, pos - 1, 1);
        pos = str_pos(s->opr, "]");
        if (pos > 0)
            str_delete(s->opr, pos - 1, 1);
    }
    
end_of_operand:
    
    if (strcmp(s->opc, "NAM") == 0) {
        strcpy(s->lbl, s->opr);
        strcpy(s->opr, "");
    }
    
    /* Check for opcode in data base 2 */
    s->posi = 0;
    s->match2 = false;

    while (!s->match2 && s->posi <= INST_NUM && strlen(s->opcode2[s->posi]) > 0) {
        if (strcmp(s->opc, s->opcode2[s->posi]) == 0) {
            s->match2 = true;
            s->lenopc = strlen(s->opc);
        }
        s->posi++;
    }
    
    if (!s->match2) {
        s->lenopc = strlen(s->opc);
        str_copy(s->opc1t, s->opc, 0, s->lenopc - 1);
        
        if (strcmp(s->opc1t, "PSH") == 0 || strcmp(s->opc1t, "PUL") == 0) {
            int pos2 = 0;

            while (pos2 < (int)strlen(s->opr)) {
                char t = s->opr[pos2];

                switch (t) {
                    case 'A': s->A = true; break;
                    case 'B': s->B = true; break;
                    case 'X': s->X = true; break;
                    case 'Y': s->Y = true; break;
                    case 'U': s->U = true; break;
                    case 'S': s->S = true; break;
                    case 'D':
                        if (pos2 + 1 < (int)strlen(s->opr) && s->opr[pos2 + 1] == 'P')
                            s->DP = true;
                        else
                            s->D = true;
                        break;
                    case 'C': s->CC = true; break;
                    case 'P': s->PC = true; break;
                    default:
                        diagnostic(s, ERROR, "Invalid PSH/PUL register");
                }

                int comma = str_pos(s->opr, ",");

                if (comma > 0) {
                    str_delete(s->opr, comma - 1, 1);
                    pos2 = comma - 1;
                } else {
                    break;
                }
            }
        }
        
        /* Detect direct addr mode */
        s->direct = (s->opr[0] == '<');

        if (s->direct)
            str_delete(s->opr, 0, 1);
        
        /* Add preceding "," for single register operands */
        if (strlen(s->opr) == 1 && strcmp(s->opc1t, "PUL") != 0 && 
            strcmp(s->opc1t, "PSH") != 0) {
            if (s->opr[0] == 'X' || s->opr[0] == 'Y' || 
                s->opr[0] == 'U' || s->opr[0] == 'S') {
                char temp[CHAR_LEN + 1];
                snprintf(temp, sizeof(temp), ",%s", s->opr);
                strcpy(s->opr, temp);
                diagnostic(s, WARNING, "Indexed addr mode assumed in next inst.");
            }
        }
        
        /* Convert indexed addressing */
        int comma = str_pos(s->opr, ",");

        if (comma > 0) {
            s->direct = false;
            
            while (comma > 0 && s->opr[comma] == '-') {
                s->auto_inc--;
                str_delete(s->opr, comma, 1);
                comma = str_pos(s->opr, ",");
            }
            
            /* Ensure string has space for checking */
            if (strlen(s->opr) < CHAR_LEN - 2)
                strcat(s->opr, " ");
            
            for (int z = 2; z <= 3 && comma + z <= (int)strlen(s->opr); z++) {
                if (s->opr[comma + z - 1] == '+')
                    s->auto_inc++;
            }
            
            if (comma == 1 && s->auto_inc == 0) {
                char temp[CHAR_LEN + 1];
                snprintf(temp, sizeof(temp), "0%s", s->opr);
                strcpy(s->opr, temp);
                comma++;
            }
            
            s->index = s->opr[comma];
            char t2[CHAR_LEN + 1] = {0};
            
            switch (s->index) {
                case 'X': strcpy(t2, "(A0)"); break;
                case 'Y': strcpy(t2, "(A1)"); break;
                case 'U': strcpy(t2, "(A5)"); break;
                case 'S': strcpy(t2, "(A6)"); break;
                case 'P':
                    if (strcmp(s->opc, "EXG") == 0 || strcmp(s->opc, "TFR") == 0)
                        diagnostic(s, ERROR, "PC not supported");
                    strcpy(t2, "(PC)");
                    break;
                case 'A': strcpy(t2, "(D0)"); break;
                case 'B': strcpy(t2, "(D1)"); break;
                case 'D':
                    if (comma + 1 < (int)strlen(s->opr) && s->opr[comma + 1] == 'P') {
                        if (strcmp(s->opc, "EXG") == 0)
                            s->DPR = true;
                        strcpy(t2, "(D3)");
                        s->DPW = true;
                    } else {
                        strcpy(t2, "(D2)");
                        s->D = true;
                    }
                    break;
                case 'C':
                    if (strcmp(s->opc, "EXG") == 0)
                        diagnostic(s, ERROR, "CCR not supported");
                    strcpy(t2, "(CCR)");
                    break;
                default:
                    diagnostic(s, ERROR, "Invalid second (index) register");
            }
            
            char temp[CHAR_LEN + 1];
            strncpy(temp, s->opr, comma - 1);
            temp[comma - 1] = '\0';
            strcat(temp, t2);
            strcpy(s->opr, temp);
        }
        
        /* Handle EXG and TFR instructions */
        /* Use original comma position (Pascal variable j, 1-based).        */
        /* The indexed-addr section consumed the comma, so str_pos would     */
        /* return 0 on the modified opr — we must use the saved value.       */
        if (strcmp(s->opc, "EXG") == 0 || strcmp(s->opc, "TFR") == 0) {
            switch (s->opr[0]) {
                case 'A': str_insert(s->opr, "D0,", comma - 1); break;
                case 'B': str_insert(s->opr, "D1,", comma - 1); break;
                case 'D':
                    if (strlen(s->opr) > 1 && s->opr[1] == 'P') {
                        str_insert(s->opr, "D3,", comma - 1);
                        s->DPR = true;
                        if (strcmp(s->opc, "EXG") == 0)
                            s->DPW = true;
                    } else {
                        if (strcmp(s->opc, "EXG") == 0) {
                            /* Pascal: delete at j (the '('), then at j+2 (the ')') */
                            str_delete(s->opr, comma - 1, 1);
                            int rp = str_pos(s->opr, ")");
                            if (rp > 0)
                                str_delete(s->opr, rp - 1, 1);
                            strcat(s->opr, ",(D2)");
                        } else {
                            str_insert(s->opr, "D2,", comma - 1);
                        }
                        s->D = true;
                    }
                    break;
                case 'C':
                    str_insert(s->opr, "SR,", comma - 1);
                    if (strcmp(s->opc, "EXG") == 0)
                        diagnostic(s, ERROR, "CCR not supported");
                    break;
                case 'X': str_insert(s->opr, "A0,", comma - 1); break;
                case 'Y': str_insert(s->opr, "A1,", comma - 1); break;
                case 'U': str_insert(s->opr, "A5,", comma - 1); break;
                case 'S': str_insert(s->opr, "A6,", comma - 1); break;
                default:
                    diagnostic(s, ERROR, "Invalid first register");
            }
            
            int paren = str_pos(s->opr, "(");
            if (paren > 0)
                str_delete(s->opr, paren - 1, 1);
            paren = str_pos(s->opr, ")");
            if (paren > 0)
                str_delete(s->opr, paren - 1, 1);
            
            /* Pascal: delete(opr, 1, j-1) — strip the original first register */
            str_delete(s->opr, 0, comma - 1);
        } else {
            /* Accumulator-offset addressing: same original comma position check */
            if (comma == 2) {
                if (s->opr[0] == 'A' || s->opr[0] == 'B' || s->opr[0] == 'D') {
                    switch (s->opr[0]) {
                        /* Pascal: insert(',D0.W',opr,5) — insert before closing ')' */
                        case 'A': str_insert(s->opr, ",D0.W", strlen(s->opr) - 1); break;
                        case 'B': str_insert(s->opr, ",D1.W", strlen(s->opr) - 1); break;
                        case 'D': 
                            str_insert(s->opr, ",D2.L", strlen(s->opr) - 1);
                            s->D = true;
                            break;
                    }
                    str_delete(s->opr, 0, 1);
                }
            }
        }
        
        /* Convert 6800 type string to 68000 string */
        int quote_pos = str_pos(s->opr, "\"");
        if (quote_pos > 0)
            s->opr[quote_pos - 1] = '\'';
        
        s->lenopr = strlen(s->opr);
        if (s->lenopr > 0 && s->opr[s->lenopr - 1] == '"')
            s->opr[s->lenopr - 1] = '\'';
        
        quote_pos = str_pos(s->opr, "'");
        if (quote_pos > 0 && s->opr[quote_pos - 1] == '\'' && 
            s->lenopr > 0 && s->opr[s->lenopr - 1] != '\'') {
            strcat(s->opr, "'");
        }
    }
    return true;
}

void init_state(State *s) {
    memset(s, 0, sizeof(*s));
    strcpy(s->siz, ".B");
    s->index = 'A';
}

void cleanup(void) {
    if (infile) fclose(infile);
    if (outfile) fclose(outfile);
    if (errorfile) fclose(errorfile);
    if (codes) fclose(codes);
    if (codes2) fclose(codes2);
    if (stubxref) fclose(stubxref);
}

int main(int argc, char *argv[]) {
    State state;

    atexit(cleanup);
    
    if (argc < 3) {
        fprintf(stderr, "Error : source and/or destination filename missing from command line\n");
        return 1;
    }
    
    infile = fopen(argv[1], "r");
    if (!infile) {
        fprintf(stderr, "Error : source file not found\n");
        return 1;
    }
    
    outfile = fopen(argv[2], "w");
    if (!outfile) {
        fprintf(stderr, "Error : cannot open destination file\n");
        return 1;
    }
    
    errorfile = fopen("error.txt", "w");
    codes     = fopen("codes.dbb", "r");
    codes2    = fopen("codes2.dbb", "r");
    stubxref  = fopen("stubxref.dbb", "r");
    
    if (!codes || !codes2 || !stubxref) {
        fprintf(stderr, "Error : database file(s) missing\n");
        return 1;
    }
    
    init_state(&state);
    
    printf("M6809 to M68000 Source Code Translator     Version %s (%2)\n", VERSION, VDATE);
    printf("Systems Engg, E. Kilbride, Scotland\n");
    printf("Motorola Inc. Copyright 1986\n\n");
    printf("Reading database ........\n");
    
    /* Read translation database 1 */
    int posi = 0;
    char line[CHAR_LEN + 1];

    while (posi <= INST_NUM && fgets(line, sizeof(line), codes)) {
        line[strcspn(line, "\r\n")] = '\0';
        strcpy(state.opcode[posi], line);

        if (fgets(line, sizeof(line), codes)) {
            line[strcspn(line, "\r\n")] = '\0';
            strcpy(state.expres[posi], line);
        }
        posi++;
    }
    
    /* Read translation database 2 */
    posi = 0;
    while (posi <= INST_NUM && fgets(line, sizeof(line), codes2)) {
        line[strcspn(line, "\r\n")] = '\0';
        strcpy(state.opcode2[posi], line);

        if (fgets(line, sizeof(line), codes2)) {
            line[strcspn(line, "\r\n")] = '\0';
            strcpy(state.expres2[posi], line);
        }
        posi++;
    }
    
    /* Check for command line option 'I' (interleave) */
    if (argc >= 4 && toupper(argv[3][0]) == 'I')
        state.interleave = true;
    
    printf("Code in%15sCode out%15sErrors%15sWarnings\n", "", "", "");
    
    /* Output stub references */
    fprintf(outfile, "*++       ******   STUB EXTERNAL REFERENCES  ******\n");
    fprintf(outfile, "\n");
    
    while (fgets(line, sizeof(line), stubxref)) {
        line[strcspn(line, "\r\n")] = '\0';
        fprintf(outfile, "%10s%s\n", "", line);
        state.linesout++;
    }
    
    /* Output error file header */
    fprintf(errorfile, "%20s*********  Translation Error List  ***********\n", "");
    fprintf(errorfile, "\n");
    fprintf(errorfile, "File in : File out\n");
    
    /* Process input file */
    while (getstmt(&state)) {
        convert(&state);
    }
    
    printf("%8d%15d%15d%16d\n", state.lines - state.comm_in, 
           state.linesout, state.errors, state.warnings);
    
    return 0;
}
