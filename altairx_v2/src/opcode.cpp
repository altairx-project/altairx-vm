#include "opcode.hpp"

std::string AxOpcode::to_string() const
{
    switch(operation())
    {
    case AX_EXE_ALU_MOVEIX:
        return "AX_EXE_ALU_MOVEIX";

    case AX_EXE_ALU_MOVEI:
        return "AX_EXE_ALU_MOVEI";

    case AX_EXE_ALU_EXT:
        return "AX_EXE_ALU_EXT";

    case AX_EXE_ALU_INS:
        return "AX_EXE_ALU_INS";

    case AX_EXE_ALU_MAX:
        return "AX_EXE_ALU_MAX";

    case AX_EXE_ALU_UMAX:
        return "AX_EXE_ALU_UMAX";

    case AX_EXE_ALU_MIN:
        return "AX_EXE_ALU_MIN";

    case AX_EXE_ALU_UMIN:
        return "AX_EXE_ALU_UMIN";

    case AX_EXE_ALU_ADDS:
        return "AX_EXE_ALU_ADDS";

    case AX_EXE_ALU_SUBS:
        return "AX_EXE_ALU_SUBS";

    case AX_EXE_ALU_RTL:
        return "AX_EXE_ALU_RTL";

    case AX_EXE_ALU_RTR:
        return "AX_EXE_ALU_RTR";

    case AX_EXE_ALU_CMP:
        return "AX_EXE_ALU_CMP";

    case AX_EXE_ALU_BIT:
        return "AX_EXE_ALU_BIT";

    case AX_EXE_ALU_TEST:
        return "AX_EXE_ALU_TEST";

    case AX_EXE_ALU_TESTFR:
        return "AX_EXE_ALU_TESTFR";

    case AX_EXE_ALU_ADD:
        return "AX_EXE_ALU_ADD";

    case AX_EXE_ALU_SUB:
        return "AX_EXE_ALU_SUB";

    case AX_EXE_ALU_XOR:
        return "AX_EXE_ALU_XOR";

    case AX_EXE_ALU_OR:
        return "AX_EXE_ALU_OR";

    case AX_EXE_ALU_AND:
        return "AX_EXE_ALU_AND";

    case AX_EXE_ALU_LSL:
        return "AX_EXE_ALU_LSL";

    case AX_EXE_ALU_ASR:
        return "AX_EXE_ALU_ASR";

    case AX_EXE_ALU_LSR:
        return "AX_EXE_ALU_LSR";

    case AX_EXE_ALU_SE:
        return "AX_EXE_ALU_SE";

    case AX_EXE_ALU_SEN:
        return "AX_EXE_ALU_SEN";

    case AX_EXE_ALU_SLTS:
        return "AX_EXE_ALU_SLTS";

    case AX_EXE_ALU_SLTU:
        return "AX_EXE_ALU_SLTU";

    case AX_EXE_ALU_SAND:
        return "AX_EXE_ALU_SAND";

    case AX_EXE_ALU_SBIT:
        return "AX_EXE_ALU_SBIT";

    case AX_EXE_ALU_CMOVEN:
        return "AX_EXE_ALU_CMOVEN";

    case AX_EXE_ALU_CMOVE:
        return "AX_EXE_ALU_CMOVE";

    case AX_EXE_LSU_LD:
        return "AX_EXE_LSU_LD";

    case AX_EXE_LSU_LDS:
        return "AX_EXE_LSU_LDS";

    case AX_EXE_LSU_FLD:
        return "AX_EXE_LSU_FLD";

    case AX_EXE_LSU_EMPTY0:
        return "AX_EXE_LSU_EMPTY";
    case AX_EXE_LSU_ST:
        return "AX_EXE_LSU_ST";
    case AX_EXE_LSU_EMPTY1:
        return "AX_EXE_LSU_EMPTY";
    case AX_EXE_LSU_FST:
        return "AX_EXE_LSU_FST";
    case AX_EXE_LSU_EMPTY2:
        return "AX_EXE_LSU_EMPTY";
    case AX_EXE_LSU_LDI:
        return "AX_EXE_LSU_LDI";
    case AX_EXE_LSU_LDIS:
        return "AX_EXE_LSU_LDIS";
    case AX_EXE_LSU_FLDI:
        return "AX_EXE_LSU_FLDI";
    case AX_EXE_LSU_EMPTY3:
        return "AX_EXE_LSU_EMPTY";
    case AX_EXE_LSU_STI:
        return "AX_EXE_LSU_STI";
    case AX_EXE_LSU_EMPTY4:
        return "AX_EXE_LSU_EMPTY";
    case AX_EXE_LSU_FSTI:
        return "AX_EXE_LSU_FSTI";
    case AX_EXE_LSU_EMPTY5:
        return "AX_EXE_LSU_EMPTY";
    case AX_EXE_FPU_FADD:
        return "AX_EXE_FPU_FADD";
    case AX_EXE_FPU_FSUB:
        return "AX_EXE_FPU_FSUB";
    case AX_EXE_FPU_FMUL:
        return "AX_EXE_FPU_FMUL";
    case AX_EXE_FPU_FNMUL:
        return "AX_EXE_FPU_FNMUL";
    case AX_EXE_FPU_HTOF:
        return "AX_EXE_FPU_HTOF";
    case AX_EXE_FPU_FTOH:
        return "AX_EXE_FPU_FTOH";
    case AX_EXE_FPU_ITOF:
        return "AX_EXE_FPU_ITOF";
    case AX_EXE_FPU_FTOI:
        return "AX_EXE_FPU_FTOI";
    case AX_EXE_FPU_FNEG:
        return "AX_EXE_FPU_FNEG";
    case AX_EXE_FPU_FABS:
        return "AX_EXE_FPU_FABS";
    case AX_EXE_FPU_FMIN:
        return "AX_EXE_FPU_FMIN";
    case AX_EXE_FPU_FMAX:
        return "AX_EXE_FPU_FMAX";
    case AX_EXE_FPU_FMOVE:
        return "AX_EXE_FPU_FMOVE";
    case AX_EXE_FPU_FCMP:
        return "AX_EXE_FPU_FCMP";
    case AX_EXE_FPU_FMOVEI:
        return "AX_EXE_FPU_FMOVEI";
    case AX_EXE_FPU_FCMPI:
        return "AX_EXE_FPU_FCMPI";
    case AX_EXE_EFU_FDIV:
        return "AX_EXE_EFU_FDIV";
    case AX_EXE_EFU_FATAN2:
        return "AX_EXE_EFU_FATAN";
    case AX_EXE_EFU_FSQRT:
        return "AX_EXE_EFU_FSQRT";
    case AX_EXE_EFU_FSIN:
        return "AX_EXE_EFU_FSIN";
    case AX_EXE_EFU_FATAN:
        return "AX_EXE_EFU_FATAN";
    case AX_EXE_EFU_FEXP:
        return "AX_EXE_EFU_FEXP";
    case AX_EXE_EFU_ISQRT:
        return "AX_EXE_EFU_ISQRT";
    case AX_EXE_EFU_EMOVEFROM:
        return "AX_EXE_EFU_EMOVEFROM";
    case AX_EXE_EFU_DDIV:
        return "AX_EXE_EFU_DDIV";
    case AX_EXE_EFU_DATAN2:
        return "AX_EXE_EFU_DATAN";
    case AX_EXE_EFU_DSQRT:
        return "AX_EXE_EFU_DSQRT";
    case AX_EXE_EFU_DSIN:
        return "AX_EXE_EFU_DSIN";
    case AX_EXE_EFU_DATAN:
        return "AX_EXE_EFU_DATAN";
    case AX_EXE_EFU_DEXP:
        return "AX_EXE_EFU_DEXP";
    case AX_EXE_EFU_ISQRT2:
        return "AX_EXE_EFU_ISQRT";
    case AX_EXE_EFU_EMOVETO:
        return "AX_EXE_EFU_EMOVETO";
    case AX_EXE_MDU_DIV:
        return "AX_EXE_MDU_DIV";
    case AX_EXE_MDU_DIVU:
        return "AX_EXE_MDU_DIVU";
    case AX_EXE_MDU_MUL:
        return "AX_EXE_MDU_MUL";
    case AX_EXE_MDU_MULU:
        return "AX_EXE_MDU_MULU";
    case AX_EXE_MDU_GETMD:
        return "AX_EXE_MDU_GETMD";
    case AX_EXE_MDU_SETMD:
        return "AX_EXE_MDU_SETMD";
    case AX_EXE_MDU_EMPTY0:
        return "AX_EXE_MDU_EMPTY";
    case AX_EXE_MDU_EMPTY1:
        return "AX_EXE_MDU_EMPTY";
    case AX_EXE_MDU_EMPTY2:
        return "AX_EXE_MDU_EMPTY";
    case AX_EXE_MDU_EMPTY3:
        return "AX_EXE_MDU_EMPTY";
    case AX_EXE_MDU_EMPTY4:
        return "AX_EXE_MDU_EMPTY";
    case AX_EXE_MDU_EMPTY5:
        return "AX_EXE_MDU_EMPTY";
    case AX_EXE_MDU_EMPTY6:
        return "AX_EXE_MDU_EMPTY";
    case AX_EXE_MDU_EMPTY7:
        return "AX_EXE_MDU_EMPTY";
    case AX_EXE_MDU_EMPTY8:
        return "AX_EXE_MDU_EMPTY";
    case AX_EXE_MDU_EMPTY9:
        return "AX_EXE_MDU_EMPTY";
    case AX_EXE_BRU_BEQ:
        return "AX_EXE_BRU_BEQ";
    case AX_EXE_BRU_BNE:
        return "AX_EXE_BRU_BNE";
    case AX_EXE_BRU_BLT:
        return "AX_EXE_BRU_BLT";
    case AX_EXE_BRU_BGE:
        return "AX_EXE_BRU_BGE";
    case AX_EXE_BRU_BLTU:
        return "AX_EXE_BRU_BLTU";
    case AX_EXE_BRU_BGEU:
        return "AX_EXE_BRU_BGEU";
    case AX_EXE_BRU_EMPTY0:
        return "AX_EXE_BRU_EMPTY";
    case AX_EXE_BRU_EMPTY1:
        return "AX_EXE_BRU_EMPTY";
    case AX_EXE_BRU_BRA:
        return "AX_EXE_BRU_BRA";
    case AX_EXE_BRU_CALLR:
        return "AX_EXE_BRU_CALLR";
    case AX_EXE_BRU_JUMP:
        return "AX_EXE_BRU_JUMP";
    case AX_EXE_BRU_CALL:
        return "AX_EXE_BRU_CALL";
    case AX_EXE_BRU_EMPTY2:
        return "AX_EXE_BRU_EMPTY";
    case AX_EXE_BRU_INDIRECTCALLR:
        return "AX_EXE_BRU_INDIRECTCALLR";
    case AX_EXE_BRU_EMPTY3:
        return "AX_EXE_BRU_EMPTY";
    case AX_EXE_BRU_INDIRECTCALL:
        return "AX_EXE_BRU_INDIRECTCALL";
    //case AX_EXE_CU_GETIR:
    //    return "AX_EXE_CU_GETIR";
    //case AX_EXE_CU_SETFR:
    //    return "AX_EXE_CU_SETFR";
    //case AX_EXE_CU_EMPTY0:
    //    return "AX_EXE_CU_EMPTY";
    //case AX_EXE_CU_MMU:
    //    return "AX_EXE_CU_MMU";
    //case AX_EXE_CU_EMPTY1:
    //    return "AX_EXE_CU_EMPTY";
    //case AX_EXE_CU_EMPTY2:
    //    return "AX_EXE_CU_EMPTY";
    //case AX_EXE_CU_EMPTY3:
    //    return "AX_EXE_CU_EMPTY";
    //case AX_EXE_CU_EMPTY4:
    //    return "AX_EXE_CU_EMPTY";
    //case AX_EXE_CU_EMPTY5:
    //    return "AX_EXE_CU_EMPTY";
    //case AX_EXE_CU_EMPTY6:
    //    return "AX_EXE_CU_EMPTY";
    //case AX_EXE_CU_EMPTY7:
    //    return "AX_EXE_CU_EMPTY";
    //case AX_EXE_CU_EMPTY8:
    //    return "AX_EXE_CU_EMPTY";
    //case AX_EXE_CU_EMPTY9:
    //    return "AX_EXE_CU_EMPTY";
    //case AX_EXE_CU_SYNC:
    //    return "AX_EXE_CU_SYNC";
    //case AX_EXE_CU_SYSCALL:
    //    return "AX_EXE_CU_SYSCALL";
    //case AX_EXE_CU_RETI:
    //    return "AX_EXE_CU_RETI";
    //case AX_EXE_VU_VECTOR2:
    //    return "AX_EXE_VU_VECTOR";
    //case AX_EXE_VU_VECTOR4:
    //    return "AX_EXE_VU_VECTOR";
    //case AX_EXE_VU_VECTOR8:
    //    return "AX_EXE_VU_VECTOR";
    //case AX_EXE_VU_INV:
    //    return "AX_EXE_VU_INV";
    //case AX_EXE_VU_VECTOR2X2:
    //    return "AX_EXE_VU_VECTOR";
    //case AX_EXE_VU_VECTOR4X2:
    //    return "AX_EXE_VU_VECTOR";
    //case AX_EXE_VU_VECTOR8X2:
    //    return "AX_EXE_VU_VECTOR";
    //case AX_EXE_VU_INVX2:
    //    return "AX_EXE_VU_INVX";
    default:

        return "";
    }
}
