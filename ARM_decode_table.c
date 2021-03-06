/*
ARM_decode_table.c

Copyright (C) 2015 Juha Aaltonen

This file is part of standalone gdb stub for Raspberry Pi 2B.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// ARM instruction set decoder
// This implementation uses decoding table and a secondary
// decoding for multiplexed instruction encodings.
//
// Multiplexed instruction encodings are instruction encodings
// common to two or more instructions that cannot be told apart
// by masking and comparing the result to the data.
// They are often special cases - some field has a certain value.
//
// The decoding table is generated from a spreadsheet and edited
// mostly by scripts. The extra-field enum-names and decoder-function
// names are also defined in the spreadsheet.
// The outcome is in the files 'ARM_decode_table_data.h'
// ARM_decode_table_prototypes.h and ARM_decode_table_extras.h
//
// At this stage the function names and extra-field enum-values are
// just copied from the spreadsheet. The function names and the
// extra-values must match to those in the spreadsheet.

// the register context
#include "rpi2.h"
#include "ARM_decode_table.h"
#include "instr_util.h"
#include "log.h"

// Extra info to help in decoding.
// Especially useful for decoding multiplexed instructions
// (= different instructions that have same mask and data).
// These are read from a file generated from the
// spreadsheet. The comma from the last entry is removed
// manually.
// TODO: add "UNPREDICTABLE"-bits check: the (0)s and (1)s.
// At the moment they are ignored.
typedef enum
{
#include "ARM_decode_table_extras.h"
// added extras for multiplexed instructions
	arm_cdata_mov_r,
	arm_cdata_lsl_imm,
	arm_ret_mov_pc,
	arm_ret_lsl_imm,
	arm_cdata_rrx_r,
	arm_cdata_ror_imm,
	arm_vldste_vld1_mult,
	arm_vldste_vld2_mult,
	arm_vldste_vld3_mult,
	arm_vldste_vld4_mult,
	arm_vldste_vst1_mult,
	arm_vldste_vst2_mult,
	arm_vldste_vst3_mult,
	arm_vldste_vst4_mult,
	arm_extras_last
} ARM_decode_extra_t;

// Decoding function prototypes are read from a file generated from the
// spreadsheet.
#include "ARM_decode_table_prototypes.h"

// the decoding table type
typedef struct
{
	unsigned int data;
	unsigned int mask;
	ARM_decode_extra_t extra;
	instr_next_addr_t (*decoder)(unsigned int, ARM_decode_extra_t);
} ARM_dec_tbl_entry_t;

// the decoding table itself.
// (See the description in the beginning of this file.)
// The initializer contents are read from a file generated from the
// spreadsheet. The comma from the last entry is removed
// manually.
ARM_dec_tbl_entry_t ARM_decode_table[] =
{
#include "ARM_decode_table_data.h"
};

// set next address for linear execution
// the address is set to 0xffffffff and flag to INSTR_ADDR_ARM
// that indicates the main ARM decoding function to set the address
// right for linear execution.
static inline instr_next_addr_t set_addr_lin(void)
{
	instr_next_addr_t retval = {
			.flag = INSTR_ADDR_ARM,
			.address = 0xffffffff
	};
	return retval;
}

unsigned int get_decode_table()
{
	return (unsigned int)ARM_decode_table;
}
unsigned int get_decode_table_sz()
{
	return (unsigned int)sizeof(ARM_decode_table);
}

// decoder dispatcher - finds the decoding function and calls it
// TODO: Partition the table search (use bits 27 - 25 of the instruction.
// Maybe:
// if condition code is not 1 1 1 1 then divide by bits 27 - 25
// under which the groups: 0 0 0, 0 1 1, the rest
// if condition code is 1 1 1 1, divide first by bits 27 - 25 != 0 0 1
// bits 27 - 25 == 0 0 1 and under which by bit 23.
instr_next_addr_t ARM_decoder_dispatch(unsigned int instr)
{
	int instr_count = sizeof(ARM_decode_table);
	instr_next_addr_t retval;
	int i;

	instr_count /= sizeof(ARM_dec_tbl_entry_t);
	retval = set_undef_addr();

	for (i=0; i<instr_count; i++)
	{
		if ((instr & ARM_decode_table[i].mask) == ARM_decode_table[i].data)
		{
			LOG_PR_VAL("Decode table hit, i: ", (unsigned int)i);
			LOG_PR_VAL_CONT(" at addr: ", (unsigned int)(&ARM_decode_table[i]));
			LOG_PR_VAL_CONT(" instr: ", instr);
			LOG_PR_VAL_CONT(" mask: ", ARM_decode_table[i].mask);
			LOG_PR_VAL_CONT(" data: ", ARM_decode_table[i].data);
			LOG_PR_VAL_CONT(" call: ", (unsigned int)(ARM_decode_table[i].decoder));
			LOG_PR_VAL_CONT(" extra: ", (unsigned int)(ARM_decode_table[i].extra));
			LOG_NEWLINE();
			retval = ARM_decode_table[i].decoder(instr, ARM_decode_table[i].extra);
			break;
		}
	}
	return retval;
}

// the decoding functions

// Sub-dispatcher - handles the multiplexed instruction encodings
// TODO: finish when V-regs are available and handling of standards is
// more clear.
instr_next_addr_t arm_mux(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2; // scratchpads
	retval = set_undef_addr();

	switch (extra)
	{
	case arm_mux_vbic_vmvn:
		// Check cmode to see if it's VBIC (imm) or VMVN (imm)
		if (bitrng(instr, 11,9) != 7)
		{
			/* neither changes the program flow
			// either VBIC or VMVN
			if (bitrng(instr, 11, 10) == 3)
			{
				// VMVN
			}
			else if (bit(instr, 8))
			{
				// VBIC
			}
			else
			{
				// VMVN
			}
			*/
			retval = set_addr_lin();
		}
		// else UNDEFINED
		break;
	case arm_mux_wfe_wfi:
		// WFE,WFI
		if ((bitrng(instr, 7, 0) == 2) || (bitrng(instr, 7, 0) == 2))
		{
			// neither changes the program flow
			retval = set_addr_lin();
		}
		// else UNDEFINED
		break;
	case arm_mux_vshrn_q_imm:
		// VSHRN,VQSHR{U}N (imm)
		// if Vm<0> == '1' then UNDEFINED;
		if (bit(instr,0) == 0)
		{
			/* neither changes the program flow
			// bits 24 and 8 == 0 => VSHRN
			if ((bit(instr, 24) == 0) && (bit(instr, 8) == 0))
			{
				// VSHRN
			}
			else
			{
				// VQSHR{U}N
			}
			*/
			retval = set_addr_lin();
		}
		// else UNDEFINED
		break;
	case arm_mux_vrshrn_q_imm:
		// VRSHRN,VQRSHR{U}N (imm)
		// if Vm<0> == '1' then UNDEFINED;
		if (bit(instr,0) == 0)
		{
			/* neither changes the program flow
			// bits 24 and 8 == 0 => VRSHRN
			if ((bit(instr, 24) == 0) && (bit(instr, 8) == 0))
			{
				// VRSHRN
			}
			else
			{
				// VQRSHR{U}N
			}
			*/
			retval = set_addr_lin();
		}
		// else UNDEFINED
		break;
	case arm_mux_vshll_i_vmovl:
		// VSHLL(imm!=size,imm),VMOVL
		// if Vd<0> == '1' then UNDEFINED;
		if (bit(instr, 12) == 0)
		{
			// if imm3 == '000' then SEE "Related encodings";
			// if imm3 != '001' && imm3 != '010' && imm3 != '100' then SEE VSHLL;
			switch (bitrng(instr, 21, 19))
			{
			case 0:
				// UNDEFINED
				break;
			case 1:
			case 2:
			case 4:
				// VMOVL
				// doesn't change the program flow
				retval = set_addr_lin();
				break;
			default:
				// VSHLL
				// doesn't change the program flow
				retval = set_addr_lin();
				break;
			}
		}
		// else UNDEFINED
		break;
	case arm_mux_vorr_i_vmov_i:
		// VORR,VMOV,VSHR (imm)
		// bit 7=0 and bits 21-20=0 0 => not VSHR
		// bit 8=0 or bits 11-10=1 1 => VMOV
		// bit 5=0 and bit 8=1 and bits 11-10 != 1 1 => VORR
		// if Q == '1' && Vd<0> == '1' then UNDEFINED
		// Q = bit 6, Vd = bits 15-12
		if ((bit(instr, 6) == 1) && (bit(instr, 12) == 0))
		{
			if ((bit(instr,7) == 0) && (bitrng(instr, 21, 19) == 0))
			{
				// VORR/VMOV
				// if Q == '1' && Vd<0> == '1' then UNDEFINED
				// Q = bit 6, Vd = bits 15-12
				if ((bit(instr, 5) == 0) && (bit(instr, 8) == 1)
						&& (bitrng(instr, 11, 10) != 3))
				{
					// VORR
					// doesn't change the program flow
					retval = set_addr_lin();
				}
				else if ((bit(instr, 8) == 0) || (bitrng(instr, 11, 10) == 3))
				{
					// VMOV
					// doesn't change the program flow
					retval = set_addr_lin();
				}
				// else UNDEFINED
			}
			else
			{
				// VSHR
				// if Q == '1' && (Vd<0> == '1' || Vm<0> == '1') then UNDEFINED
				// Q = bit 6, Vd = bits 15-12, Vm = bits 3-0
				if ((bit(instr, 6) == 1) && (bit(instr, 0) == 0))
				{
					// doesn't change the program flow
					retval = set_addr_lin();
				}
				// else UNDEFINED
			}
		}
		// else UNDEFINED
		break;
	case arm_mux_lsl_i_mov:
	case arm_mux_lsl_i_mov_pc:
		// LSL(imm),MOV
		if (bitrng(instr, 11, 7) == 0)
		{
			// MOV reg
			if (extra == arm_mux_lsl_i_mov)
			{
				// MOV reg
				retval = arm_core_data_bit(instr, arm_cdata_mov_r);

			}
			else
			{
				// 'flags'-bit set?
				if (bit(instr, 20) == 1)
				{
					// return from exception
					// if CurrentModeIsHyp() then UNDEFINED;
					// elsif CurrentModeIsUserOrSystem() then UNPREDICTABLE
					// copy SPSR to CPSR (many state-related restrictions)
					// put result to PC
					retval = arm_core_data_bit(instr, arm_ret_mov_pc);
				}
				else
				{
					// MOV PC
					retval = arm_core_data_bit(instr, arm_cdata_mov_r);
				}
			}
		}
		else
		{
			// LSL imm
			if (extra == arm_mux_lsl_i_mov)
			{
				// LSL imm
				retval = arm_core_data_bit(instr, arm_cdata_lsl_imm);
			}
			else
			{
				// 'flags'-bit set?
				if (bit(instr, 20) == 1)
				{
					// return from exception
					retval = arm_core_data_bit(instr, arm_ret_lsl_imm);
				}
				else
				{
					// LSL imm PC
					retval = arm_core_data_bit(instr, arm_cdata_lsl_imm);
				}
			}
		}
		break;
	case arm_mux_ror_i_rrx:
		// ROR(imm),RRX
		// The PC-variants are not multiplexed
		if (bitrng(instr, 11, 7) == 0)
		{
			// RRX reg
			retval = arm_core_data_bit(instr, arm_cdata_rrx_r);
		}
		else
		{
			// ROR imm
			retval = arm_core_data_bit(instr, arm_cdata_ror_imm);
		}
		break;
	case arm_mux_vmovn_q:
		// VQMOV{U}N,VMOVN
		// VQMOV{U}N: if op (bits 7-6) == '00' then SEE VMOVN;
		// 		if size == '11' || Vm<0> == '1' then UNDEFINED;
		// VMOVN: if size (bits 19-18) == '11' then UNDEFINED;
		// 		if Vm<0> (bit 0)== '1' then UNDEFINED;
		if ((bitrng(instr, 19, 18) != 3) && (bit(instr, 0) == 0))
		{
			/* neither changes the program flow
			if (bitrng(instr, 7, 6) == 0)
			{
				// VMOVN
			}
			else
			{
				// VQMOV{U}N
			}
			*/
			retval = set_addr_lin();
		}
		// else UNDEFINED
		break;
	case arm_mux_msr_r_pr:
		// MSR (reg) priv
		// user-mode: write_nzcvq = (mask<1> == '1'); write_g = (mask<0> == '1');
		// if mask (bits 19,18) == 0 then UNPREDICTABLE;
		// if R = 1, UNPREDICTABLE
		// other modes:
		// if mask (bits 19-16) == 0 then UNPREDICTABLE;
		// if n == 15 then UNPREDICTABLE
		// R = bit 22
		// TODO: add check for T-bit, return thumb address if set (CPSR)
		if (bitrng(rpi2_reg_context.reg.cpsr, 4, 0) == 16) // user mode
		{
			retval = set_addr_lin();
			if (bit(instr, 22) == 1) // SPSR
			{
				retval = set_unpred_addr(retval);
			}
			else if (bitrng(instr, 19, 18) == 0)
			{
				retval = set_unpred_addr(retval);
			}
			else if (bitrng(instr, 3, 0) == 15)
			{
				retval = set_unpred_addr(retval);
			}
			// else set APSR-bits
		}
		else if (bitrng(rpi2_reg_context.reg.cpsr, 4, 0) == 31) // system mode
		{
			retval = set_addr_lin();
			// mode-to-be
			tmp1 = rpi2_reg_context.storage[bitrng(instr, 3, 0)] & 0x1f;
			if ((tmp1 != 16) && (tmp1 != 31)) // user/system
			{
				retval = set_unpred_addr(retval);
			}
			else if (bitrng(instr, 19, 16) == 0) // mask = 0
			{
				retval = set_unpred_addr(retval);
			}
			else if (bitrng(instr, 3, 0) == 15)
			{
				retval = set_unpred_addr(retval);
			}
			// else set CPSR/SPSR
		}
		else
		{
			// TODO: add all mode restrictions
			retval = set_addr_lin();
			if (bitrng(instr, 19, 16) == 0) // mask = 0
			{
				retval = set_unpred_addr(retval);
			}
			else if (bitrng(instr, 3, 0) == 15)
			{
				retval = set_unpred_addr(retval);
			}
			// else set CPSR/SPSR
		}
		break;
	case arm_mux_mrs_r_pr:
		// MRS (reg) priv
		// Rd=bits 11-8, if d == 15 then UNPREDICTABLE
		// user mode: copies APSR into Rd
		// MRS that accesses the SPSR is UNPREDICTABLE if executed in
		//   User or System mode
		// An MRS that is executed in User mode and accesses the CPSR
		// returns an UNKNOWN value for the CPSR.{E, A, I, F, M} fields.
		// R = bit 22
		// R[d] = CPSR AND '11111000 11111111 00000011 11011111';
		retval = set_addr_lin();
		tmp1 = bitrng(rpi2_reg_context.reg.cpsr, 4, 0) & 0x1f;
		if (tmp1 == 16) // user mode
		{
			// Rd = PC
			if (bitrng(instr, 11, 8) == 15)
			{
				if (bit(instr, 22) == 0) // CPSR
				{
					tmp2 = rpi2_reg_context.reg.cpsr & 0xf80f0000;
					retval.address = tmp2;
					retval.flag = INSTR_ADDR_ARM;
				}
				// UNPREDICTABLE anyway due to Rd = PC
				retval = set_unpred_addr(retval);
			}
			// else the program flow is not changed
		}
		else if (tmp1 == 31) // system mode
		{
			// Rd = PC
			if (bitrng(instr, 11, 8) == 15)
			{
				if (bit(instr, 22) == 0) // CPSR
				{
					tmp2 = rpi2_reg_context.reg.cpsr & 0xf8ff03df;
					retval.address = tmp2;
					retval.flag = INSTR_ADDR_ARM;
				}
				// UNPREDICTABLE anyway due to Rd = PC
				retval = set_unpred_addr(retval);
			}
			// else the program flow is not changed
		}
		else
		{
			// Rd = PC
			if (bitrng(instr, 11, 8) == 15)
			{
				if (bit(instr, 22) == 0) // CPSR
				{
					tmp2 = rpi2_reg_context.reg.cpsr & 0xf8ff03df;
					retval.address = tmp2;
					retval.flag = INSTR_ADDR_ARM;
				}
				else // SPSR
				{
					tmp2 = rpi2_reg_context.reg.spsr;
					retval.address = tmp2;
					retval.flag = INSTR_ADDR_ARM;
				}
				// UNPREDICTABLE anyway due to Rd = PC
				retval = set_unpred_addr(retval);
			}
			// else the program flow is not changed
		}
		break;
	case arm_mux_msr_i_pr_hints:
		// MSR(imm),NOP,YIELD
		retval = set_addr_lin();
		if (bitrng(instr, 19, 16) != 0) // msr mask
		{
			// MSR
			if (bitrng(rpi2_reg_context.reg.cpsr, 4, 0) == 16) // user mode
			{
				if (bit(instr, 22) == 1) // SPSR
				{
					retval = set_unpred_addr(retval);
				}
				// else set APSR-bits
			}
			else if (bitrng(rpi2_reg_context.reg.cpsr, 4, 0) == 31) // system mode
			{
				// mode-to-be
				tmp1 = rpi2_reg_context.storage[bitrng(instr, 3, 0)] & 0x1f;
				if ((tmp1 != 16) && (tmp1 != 31)) // user/system
				{
					retval = set_unpred_addr(retval);
				}
				if (bit(instr, 22) == 1) // SPSR
				{
					retval = set_unpred_addr(retval);
				}
				// else set CPSR/SPSR
			}
			// else set CPSR/SPSR - privileged mode
			// the program flow is not changed
		}
		else
		{
			// hints - bits 7-0: hint opcode
			switch (bitrng(instr, 7, 0))
			{
			case 0: // NOP
			case 1: // YIELD
			case 2: // WFE
			case 3: // WFI
			case 4: // SEV
				// the program flow is not changed
				break;
			default:
				if ((bitrng(instr, 7, 0) & 0xf0) != 0xf0)
				{
					retval = set_undef_addr();
				}
				// else DBG - the program flow is not changed
				break;
			}
		}
		break;
	case arm_mux_vorr_vmov_nm:
		// VORR,VMOV (reg) - same Rn, Rm
		// VORR: if N (bit 7) == M (bit 5) && Vn == Vm then SEE VMOV (register);
		// if Q == '1' && (Vd<0> == '1' || Vn<0> == '1' || Vm<0> == '1') then UNDEFINED;
		// Vd = bits 15-12, Vn = bits 19-16, Vm = bits 3-0
		// VMOV: if !Consistent(M) || !Consistent(Vm) then SEE VORR (register);
		// if Q (bit 6)== '1' && (Vd<0> == '1' || Vm<0> == '1') then UNDEFINED;
		// Vm = bits 19-16 and 3-0, M = bit 7 and bit 5
		if ((bit(instr,16) == 0) && (bit(instr,12) == 0) && (bit(instr,0) ==0))
		{
			/* neither changes the program flow
			if ((bit(instr, 7)== bit(instr, 5))
					&& (bitrng(instr, 19, 16) == bitrng(instr, 3, 0)))
			{
				// VMOV
			}
			else
			{
				// VORR
			}
			*/
			retval = set_addr_lin();
		}
		// else UNDEFINED
		break;
	case arm_mux_vst_type:
		// arm_vldste_vst1_mult
		// arm_vldste_vst2_mult
		// arm_vldste_vst3_mult
		// arm_vldste_vst4_mult
		switch (bitrng(instr, 11, 8)) // type
		{
		case 2:
		case 6:
		case 7:
		case 10:
			retval = arm_vfp_ldst_elem(instr, arm_vldste_vst1_mult);
			break;
		case 3:
		case 8:
		case 9:
			retval = arm_vfp_ldst_elem(instr, arm_vldste_vst2_mult);
			break;
		case 4:
		case 5:
			retval = arm_vfp_ldst_elem(instr, arm_vldste_vst3_mult);
			break;
		case 0:
		case 1:
			retval = arm_vfp_ldst_elem(instr, arm_vldste_vst4_mult);
			break;
		default:
			// UNDEFINED
			break;
		}
		break;
	case arm_mux_vld_type:
		// arm_vldste_vld1_mult
		// arm_vldste_vld2_mult
		// arm_vldste_vld3_mult
		// arm_vldste_vld4_mult
		switch (bitrng(instr, 11, 8))
		{
		case 2:
		case 6:
		case 7:
		case 10:
			retval = arm_vfp_ldst_elem(instr, arm_vldste_vld1_mult);
			break;
		case 3:
		case 8:
		case 9:
			retval = arm_vfp_ldst_elem(instr, arm_vldste_vld2_mult);
			break;
		case 4:
		case 5:
			retval = arm_vfp_ldst_elem(instr, arm_vldste_vld3_mult);
			break;
		case 0:
		case 1:
			retval = arm_vfp_ldst_elem(instr, arm_vldste_vld4_mult);
			break;
		default:
			// UNDEFINED
			break;
		}
		break;
	default:
		break;
	}
	// The above needs to be done to find out if the instruction is UNDEFINED or UNPREDICTABLE.
	// That's why we check the condition here
	if ((retval.flag & (~INSTR_ADDR_UNPRED)) == INSTR_ADDR_ARM)
	{
		// if the condition doesn't match
		if (!will_branch(instr))
		{
			retval = set_addr_lin();
		}
	}
	return retval;
}

instr_next_addr_t arm_branch(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;
	int baddr = 0;
	retval = set_undef_addr();
	if (will_branch(instr))
	{
		switch (extra)
		{
		case arm_bra_b_lbl:
		case arm_bra_bl_lbl:
			baddr = (int) rpi2_reg_context.reg.r15; // PC
			baddr += 8; // PC runs 2 words ahead
			baddr += (sx32(instr, 23, 0) << 2);
			retval = set_arm_addr((unsigned int) baddr);
			break;
		case arm_bra_blx_lbl:
			baddr = (int) rpi2_reg_context.reg.r15; // PC
			baddr += 8; // PC runs 2 words ahead
			baddr += (sx32(instr, 23, 0) << 2) | (bit(instr, 24) << 1);
			retval = set_thumb_addr((unsigned int) baddr);
			break;
		case arm_bra_bx_r:
		case arm_bra_blx_r:
		case arm_bra_bxj_r:
			// from Cortex-A7 mpcore trm:
			// "The BXJ instruction behaves as a BX instruction"
			// if bit 0 of the address is '1' -> switch to thumb-state.
			retval.address = rpi2_reg_context.storage[bitrng(instr, 3, 0)];
			if (bit(retval.address, 0))
				retval = set_thumb_addr(retval.address & ((~0) << 1));
			else if (bit(retval.address, 1) == 0)
				retval = set_arm_addr(retval.address & ((~0) << 2));
			else
			{
				retval = set_addr_lin();
				retval = set_unpred_addr(retval);
			}
			if (bitrng(instr, 3, 0) == 15) // PC
				retval = set_unpred_addr(retval);
			break;
		default:
			// shouldn't get here
			break;
		}
	}
	else
	{
		// No condition match - NOP
		retval = set_addr_lin();
	}
	return retval;
}

instr_next_addr_t arm_coproc(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;
	unsigned int tmp;
	retval = set_undef_addr();
	// coproc 15 = system control, 14 = debug
	// coproc 10, 11 = fp and vector
	// coproc 8, 9, 12, 13 = reserved => UNDEFINED
	// coproc 0 - 7 = vendor-specific => UNPREDICTABLE
	// if Rt or RT2 = PC or SP => UNPREDICTABLE
	// TODO: add checks for valid known coprocessor commands
	tmp = bitrng(instr, 11, 8); // coproc
	if (!((tmp == 8) || (tmp == 9) || (tmp == 12) || (tmp == 13)))
	{
		switch(extra)
		{
		case arm_cop_mcrr2:
		case arm_cop_mcrr:
			// if Rt or Rt2 is PC
			if ((bitrng(instr, 19, 16) == 15) || (bitrng(instr, 15, 12) == 15))
			{
				/*
				if ((bitrng(instr, 19, 16) == 15) && (bitrng(instr, 15, 12) == 15))
				{
					// Rt2 = Rt = PC
					// execute: Rt2 = Rt = R0 if valid

				}
				else
				{
					// execute : Rt2 = R0, Rt = R1 if valid

					// find new PC value
					if (bitrng(instr, 19, 16) == 15)
					{
						// Rt2 = PC
					}
					else
					{
						// Rt = PC
					}
				}
				*/
				// at the moment, assume (falsely) linear, but unpredictable
				retval = set_addr_lin();
				retval = set_unpred_addr(retval);
			}
			else
			{
				retval = set_addr_lin();
			}
			break;
		case arm_cop_mcr2:
		case arm_cop_mcr:
			retval = set_addr_lin();
			tmp = bitrng(instr, 15, 12);
			if (tmp == 15)
			{
				// at the moment, assume (falsely) linear, but unpredictable
				retval = set_unpred_addr(retval);
			}
			else if (tmp == 13)
			{
				retval = set_unpred_addr(retval);
			}
			// else no effect on program flow
			break;
		case arm_cop_ldc2:
		case arm_cop_ldc:
			// if P U D W = 0 0 0 0 => UNDEFINED
			if (bitrng(instr, 24, 21) != 0)
			{
				retval = set_addr_lin();
				retval = set_unpred_addr(retval);
			}
			break;
		case arm_cop_ldc2_pc:
		case arm_cop_ldc_pc:
			// if P U D W = 0 0 0 0 => UNDEFINED
			if (bitrng(instr, 24, 21) != 0)
			{
				retval = set_addr_lin();
				retval = set_unpred_addr(retval);
			}
			break;
		case arm_cop_mrrc2:
		case arm_cop_mrrc:
			retval = set_addr_lin();
			retval = set_unpred_addr(retval);
			break;
		case arm_cop_mrc2:
		case arm_cop_mrc:
			retval = set_addr_lin();
			retval = set_unpred_addr(retval);
			break;
		case arm_cop_stc2:
		case arm_cop_stc:
			// if P U D W = 0 0 0 0 => UNDEFINED
			if (bitrng(instr, 24, 21) != 0)
			{
				retval = set_addr_lin();
				retval = set_unpred_addr(retval);
			}
			break;
		case arm_cop_cdp2:
		case arm_cop_cdp:
			// coproc 101x => fp instr
			retval = set_addr_lin();
			retval = set_unpred_addr(retval);
			break;
		default:
			// shouldn't get here
			break;
		}
	}
	return retval;
}

instr_next_addr_t arm_core_data_div(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4;
	int stmp1, stmp2, stmp3;
	retval = set_undef_addr();

	tmp1 = bitrng(instr, 19, 16); // Rd
	tmp2 = bitrng(instr, 11, 8);  // Rm
	tmp3 = bitrng(instr, 3, 0);   // Rn
	if (tmp1 == 15) // Rd = PC
	{
		if (extra == arm_div_sdiv)
		{
			stmp2 = (int)rpi2_reg_context.storage[tmp2];
			stmp3 = (int)rpi2_reg_context.storage[tmp3];
			if (tmp2 == 15) stmp2 += 8; // PC runs 2 words ahead
			if (tmp3 == 15) stmp3 += 8; // PC runs 2 words ahead
			if (stmp2 == 0) // zero divisor
			{
				// division by zero
				// if no division by zero exception, return zero
				retval = set_arm_addr(0);
			}
			else
			{
				// round towards zero
				// check the result sign
				if (bit(stmp3, 31) == bit(stmp2, 31))
				{
					// result is positive
					// round towards zero = trunc
					stmp1 = stmp3/stmp2;
				}
				else
				{
					// result is negative
					stmp1 = (((stmp3 << 1)/stmp2) + 1) >> 1;
				}
				// if no division by zero exception, return zero
				retval = set_arm_addr((unsigned int) stmp1);
			}
		}
		else
		{
			tmp4 = 0;
			// arm_div_udiv
			if (tmp2 == 15)
			{
				tmp4 += 8; // PC runs 2 words ahead
			}
			tmp2 = rpi2_reg_context.storage[tmp2] + tmp4;
			if (tmp3 == 15)
			{
				tmp4 += 8; // PC runs 2 words ahead
			}
			tmp3 = rpi2_reg_context.storage[tmp3]+ tmp4;

			if (tmp2 == 0) // zero divisor
			{
				// division by zero
				// if no division by zero exception, return zero
				retval = set_arm_addr(0);
			}
			else
			{
				// round towards zero = trunc
				tmp1 = tmp3/tmp2;
			}
			retval = set_arm_addr(tmp1);
		}
	}
	else
	{
		// PC not involved
		retval = set_addr_lin();
		if ((tmp2 == 15) || (tmp3 == 15))
			retval = set_unpred_addr(retval); // Why?
	}
	return retval;
}

instr_next_addr_t arm_core_data_mac(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4;
	int stmp1, stmp2, stmp3;
	long long int ltmp;

	retval = set_undef_addr();

	// Rd = bits 19-16, Rm = 11-8, Rn = 3-0, Ra = 15-12
	// if d == 15 || n == 15 || m == 15 || a == 15 then UNPREDICTABLE;
	tmp3 = bitrng(instr, 19, 16);

	if (tmp3 == 15) // if Rd = PC
	{
		tmp1 = bitrng(instr, 11, 8); // Rm
		tmp2 = bitrng(instr, 3, 0); // Rn
		tmp4 = 0;
		if (tmp1 == 15) tmp4 = 8; // PC runs 4 words ahead
		tmp1 = rpi2_reg_context.storage[tmp1] + tmp4;
		tmp4 = 0;
		if (tmp2 == 15) tmp4 = 8; // PC runs 4 words ahead
		tmp2 = rpi2_reg_context.storage[tmp2] + tmp4;
		switch (extra)
		{
		case arm_cmac_mul:
			// multiply Rn and Rm and keep only the low 32 bits
		case arm_cmac_mla:
			// multiply Rn by Rm and add Ra; keep only the low 32 bits
		case arm_cmac_mls:
			// multiply Rn by Rm and subtract from Ra; keep only the low 32 bits
			// The low 32-bits are same whether signed or unsigned multiply
			ltmp = tmp1 * tmp2;
			tmp3 = (unsigned int)(ltmp & 0xffffffffL);
			if (extra == arm_cmac_mla)
			{
				tmp4 = bitrng(instr, 15, 12); // Ra
				tmp4 = rpi2_reg_context.storage[tmp4];
				tmp3 += tmp4;
			}
			else if (extra == arm_cmac_mls)
			{
				tmp4 = bitrng(instr, 15, 12); // Ra
				tmp4 = rpi2_reg_context.storage[tmp4];
				tmp3 = tmp4 - tmp3;
			}
			// else no accumulate
			retval = set_arm_addr(tmp3);
			retval = set_unpred_addr(retval);
			break;
		case arm_cmac_smulw:
			// signed multiply of Rn by 16 bits of Rm
		case arm_cmac_smlaw:
			// signed multiply of Rn by 16 bits of Rm and add Ra
			// only the top 32-bit of the 48-bit product is kept
			if (bit(instr, 6)) // which half of Rm
				stmp1 = (int)(short int)bitrng(tmp1, 31, 16);
			else
				stmp1 = (int)(short int)bitrng(tmp1, 15, 0);

			stmp2 = (int) tmp2;
			ltmp = stmp2 * stmp1;
			stmp3 = (int)((ltmp >> 16) & 0xffffffffL);
			if (extra == arm_cmac_smlaw)
			{
				tmp4 = bitrng(instr, 15, 12); // Ra
				tmp4 = rpi2_reg_context.storage[tmp4];
				stmp3 += (int)tmp4;
			}
			retval = set_arm_addr((unsigned int)stmp3);
			retval = set_unpred_addr(retval);
			break;
		case arm_cmac_smul:
			// signed multiply 16 bits of Rn by 16 bits of Rm
		case arm_cmac_smla:
			// signed multiply 16 bits of Rn by 16 bits of Rm and add Ra
			if (bit(instr, 6)) // which half of Rm
				stmp1 = (int)(short int)bitrng(tmp1, 31, 16);
			else
				stmp1 = (int)(short int)bitrng(tmp1, 15, 0);

			if (bit(instr, 5)) // which half of Rn
				stmp2 = (int)(short int)bitrng(tmp2, 31, 16);
			else
				stmp2 = (int)(short int)bitrng(tmp2, 15, 0);

			stmp3 = stmp2 * stmp1;
			if (extra == arm_cmac_smla)
			{
				tmp4 = bitrng(instr, 15, 12); // Ra
				tmp4 = rpi2_reg_context.storage[tmp4];
				stmp3 += (int)tmp4;
			}
			retval = set_arm_addr((unsigned int)stmp3);
			retval = set_unpred_addr(retval);
			break;
		case arm_cmac_smmul:
			// multiply Rn by Rm and keep only the top 32 bits
		case arm_cmac_smmla:
			// signed multiply Rn by Rm and add only the top 32 bits to Ra
			// exceptionally Ra can be PC without restrictions
			// if Ra == '1111' -> SMMUL
		case arm_cmac_smmls:
			// smmls is like smmla, but subtract from Ra
			ltmp = ((int)tmp1) * ((int)tmp2);
			if (bit(instr, 5)) ltmp += 0x80000000L; // round
			stmp3 = (int)((ltmp >> 32) & 0xffffffffL);
			if (extra == arm_cmac_smmla)
			{
				tmp4 = bitrng(instr, 15, 12); // Ra
				tmp4 = rpi2_reg_context.storage[tmp4];
				stmp3 += (int)tmp4;
			}
			else if (extra == arm_cmac_smmls)
			{
				tmp4 = bitrng(instr, 15, 12); // Ra
				tmp4 = rpi2_reg_context.storage[tmp4];
				stmp3 = (int)tmp4 - stmp3;
			}
			retval = set_arm_addr((unsigned int)stmp3);
			retval = set_unpred_addr(retval);
			break;
		case arm_cmac_smuad:
			// smuad: multiply upper and lower 16 bits (maybe Rm swapped)
			// and sum results
		case arm_cmac_smusd:
			// smusd is like smuad, but subtract (low - high)
		case arm_cmac_smlad:
			// smlad is like smuad, but sum is added to Ra
			// if Ra == '1111' -> SMUAD
		case arm_cmac_smlsd:
			// smlsd is like smusd but the difference is added to Ra
			// if Ra == '1111' -> SMUSD
			// M-bit = 5
			if (bit(instr, 5))
			{
				// swap Rm
				tmp3 = bitrng(tmp1, 31, 16);
				tmp3 |= bitrng(tmp1, 15, 0) << 16;
				tmp1 = tmp3;
			}
			// low 16 bits
			stmp1 = (int)(short int)(tmp1 & 0xffff);
			stmp2 = (int)(short int)(tmp2 & 0xffff);
			stmp3 = stmp1 * stmp2;
			// high 16 bits
			stmp1 = (int)(short int)((tmp1 >> 16) & 0xffff);
			stmp2 = (int)(short int)((tmp2 >> 16) & 0xffff);
			if ((extra == arm_cmac_smuad) || (extra == arm_cmac_smlad))
				stmp3 += stmp1 * stmp2;
			else
				stmp3 -= stmp1 * stmp2;

			if ((extra == arm_cmac_smlsd) || (extra == arm_cmac_smlad))
			{
				tmp4 = bitrng(instr, 15, 12); // Ra
				tmp4 = rpi2_reg_context.storage[tmp4];
				stmp3 += (int)tmp4;
			}
			retval = set_arm_addr((unsigned int) stmp3);
			retval = set_unpred_addr(retval);
			break;
		default:
			// shouldn't get here
			break;
		}
	}
	else
	{
		retval = set_addr_lin();
	}
	return retval;
}

instr_next_addr_t arm_core_data_macd(unsigned int instr, ARM_decode_extra_t extra)
{
	// Rm = bits 11-8, Rn=3-0, RdHi=19-16, RdLo=15-12, R/M/N=bit 5, M=bit 6
	// if dLo == 15 || dHi == 15 || n == 15 || m == 15 then UNPREDICTABLE;
	// if dHi == dLo then UNPREDICTABLE;
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4, tmp5;
	int stmp1, stmp2, stmp3;
	long long int ltmp;
	long long utmp;

	retval = set_undef_addr();

	tmp3 = bitrng(instr, 19, 16); // RdHi
	tmp4 = bitrng(instr, 15, 12); // RdLo

	if ((tmp3 == 15) || (tmp4 == 15))// if RdHi = PC or RdLo = PC
	{
		tmp1 = bitrng(instr, 11, 8); // Rm
		tmp2 = bitrng(instr, 3, 0); // Rn
		tmp5 = 0;
		if (tmp1 == 15) tmp5 = 8; // PC runs 2 words ahead
		tmp1 = rpi2_reg_context.storage[tmp1] + tmp5;
		tmp5 = 0;
		if (tmp2 == 15) tmp5 = 8; // PC runs 2 words ahead
		tmp2 = rpi2_reg_context.storage[tmp2] + tmp5;
		switch (extra)
		{
		case arm_cmac_smlal16:
			// signed multiply 16 bits of Rn by 16 bits of Rm and
			// add RdHi and RdLo as two 32-bit values to the result
			// then put the result to RdHi and RdLo as 64-bit value
			if (bit(instr, 6)) // which half of Rm
				stmp1 = (int)(short int)bitrng(tmp1, 31, 16);
			else
				stmp1 = (int)(short int)bitrng(tmp1, 15, 0);

			if (bit(instr, 5)) // which half of Rn
				stmp2 = (int)(short int)bitrng(tmp2, 31, 16);
			else
				stmp2 = (int)(short int)bitrng(tmp2, 15, 0);

			ltmp = (long long int)(((int)tmp1) * ((int)tmp2));
			stmp1 = (int) rpi2_reg_context.storage[tmp3];
			stmp2 = (int) rpi2_reg_context.storage[tmp4];
			ltmp += (long long int)(stmp1 + stmp2);
			if (tmp3 == 15) // if PC is RdHi
				tmp4 = (unsigned int)((ltmp >> 32) & 0xffffffff);
			else // PC is RdLo
				tmp4 = (unsigned int)(ltmp & 0xffffffff);
			break;
		case arm_cmac_smlal:
			// signed multiply Rn by Rm and add RdHi and RdLo as 64-bit value
			// result is 64 bit value in RdHi and RdLo
			ltmp = (long long int)(((int)tmp1) * ((int)tmp2));
			ltmp += ((long long int) tmp3) << 32;
			ltmp += (long long int)((int) tmp4);
			if (tmp3 == 15) // if PC is RdHi
				tmp4 = (unsigned int)((ltmp >> 32) & 0xffffffff);
			else // PC is RdLo
				tmp4 = (unsigned int)(ltmp & 0xffffffff);
			break;
		case arm_cmac_smull:
			// signed multiply Rn by Rm
			// result is 64 bit value in RdHi and RdLo
			ltmp = (long long int)(((int)tmp1) * ((int)tmp2));
			if (tmp3 == 15) // if PC is RdHi
				tmp4 = (unsigned int)((ltmp >> 32) & 0xffffffff);
			else // PC is RdLo
				tmp4 = (unsigned int)(ltmp & 0xffffffff);
			break;
		case arm_cmac_umaal:
			// signed multiply Rn by Rm and
			// add RdHi and RdLo as two 32-bit values to the result
			// then put the result to RdHi and RdLo as 64-bit value
			ltmp = (long long int)(((int)tmp1) * ((int)tmp2));
			stmp1 = (int) rpi2_reg_context.storage[tmp3];
			stmp2 = (int) rpi2_reg_context.storage[tmp4];
			ltmp += (long long int)(stmp1 + stmp2);
			if (tmp3 == 15) // if PC is RdHi
				tmp4 = (unsigned int)((ltmp >> 32) & 0xffffffff);
			else // PC is RdLo
				tmp4 = (unsigned int)(ltmp & 0xffffffff);
		break;
		case arm_cmac_umlal:
			// unsigned multiply Rn by Rm and add RdHi and RdLo as 64-bit value
			// result is 64 bit value in RdHi and RdLo
			utmp = (long long)(tmp1 * tmp2);
			utmp += ((long long)tmp3) << 32;
			utmp += (long long)tmp4;
			if (tmp3 == 15) // if PC is RdHi
				tmp4 = (unsigned int)((utmp >> 32) & 0xffffffff);
			else // PC is RdLo
				tmp4 = (unsigned int)(utmp & 0xffffffff);
			break;
		case arm_cmac_umull:
			// unsigned multiply Rn by Rm
			// result is 64 bit value in RdHi and RdLo
			utmp = (long long)(tmp1 * tmp2);
			if (tmp3 == 15) // if PC is RdHi
				tmp4 = (unsigned int)((utmp >> 32) & 0xffffffff);
			else // PC is RdLo
				tmp4 = (unsigned int)(utmp & 0xffffffff);
			break;
		case arm_cmac_smlald:
			// signed multiply upper and lower 16 bits (maybe Rm swapped)
			// and sum the products in RdHi and RdLo
			// result is 64 bit value in RdHi and RdLo
		case arm_cmac_smlsld:
			// smlsld is like smlald, but subtract (low - high)
			if (bit(instr, 5))
			{
				// swap Rm
				tmp3 = bitrng(tmp1, 31, 16);
				tmp3 |= bitrng(tmp1, 15, 0) << 16;
				tmp1 = tmp3;
				tmp3 = bitrng(instr, 19, 16); // restore tmp3
			}

			// low 16 bits
			stmp1 = (int)(short int)(tmp1 & 0xffff);
			stmp2 = (int)(short int)(tmp2 & 0xffff);
			stmp3 = stmp1 * stmp2;
			// high 16 bits
			stmp1 = (int)(short int)((tmp1 >> 16) & 0xffff);
			stmp2 = (int)(short int)((tmp2 >> 16) & 0xffff);
			if (extra == arm_cmac_smlald)
				stmp3 += stmp1 * stmp2;
			else
				stmp3 -= stmp1 * stmp2;
			stmp1 = (int)rpi2_reg_context.storage[tmp3];
			stmp2 = (int)rpi2_reg_context.storage[tmp4];
			ltmp = (long long int)stmp3;
			ltmp += ((long long int)stmp1) << 32;
			ltmp += (long long int)stmp2;
			if (tmp3 == 15) // if PC is RdHi
				tmp4 = (unsigned int)((ltmp >> 32) & 0xffffffff);
			else // PC is RdLo
				tmp4 = (unsigned int)(ltmp & 0xffffffff);
			break;
		default:
			// shouldn't get here
			break;
		}
		retval = set_arm_addr(tmp4);
		retval = set_unpred_addr(retval);
	}
	else
	{
		retval = set_addr_lin();
	}
	return retval;
}

instr_next_addr_t arm_core_data_misc(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4;
	unsigned short htmp1, htmp2;

	retval = set_undef_addr();

	switch (extra)
	{
	case arm_cmisc_movw:
		// move 16-bit immediate to Rd (upper word zeroed)
	case arm_cmisc_movt:
		// move 16-bit immediate to top word of Rd
		// Rd = bits 15-12, imm = bits 19-16 and 11-0
		// if d == 15 then UNPREDICTABLE;
		tmp1 = bitrng(instr, 15, 12); // Rd
		if (tmp1 == 15) // Rd = PC
		{
			tmp2 = bits(instr, 0x000f0fff); // immediate
			if (instr == arm_cmisc_movt)
			{
				tmp2 <<= 16;
				tmp1 = rpi2_reg_context.storage[tmp1];
				tmp1 &= 0x0000ffff;
				tmp2 |= tmp1;
			}
			retval = set_arm_addr(tmp2);
			retval = set_unpred_addr(retval);
		}
		else
		{
			retval = set_addr_lin();
		}
		break;
	case arm_cmisc_clz:
		// counts leading zeroes in Rm and places the number in Rd
		// Rd = bits 15-12, Rm = bits 3-0
		// if d == 15 || m == 15 then UNPREDICTABLE
		tmp1 = bitrng(instr, 15, 12); // Rd
		if (tmp1 == 15) // Rd = PC
		{
			tmp2 = bitrng(instr, 3, 0); // Rm
			tmp3 = 0;
			if (tmp2 == 15) tmp3 = 8; // PC runs 2 words ahead
			tmp2 = rpi2_reg_context.storage[tmp2] + tmp3;
			// this could be optimized
			for (tmp3=0; tmp3 < 32; tmp3++)
			{
				// if tmp3:th bit (from top) is set
				if (tmp2 & (1 << (31 - tmp3))) break;
			}
			retval = set_arm_addr(tmp3);
			retval = set_unpred_addr(retval);
		}
		else
		{
			retval = set_addr_lin();
		}
		break;
	case arm_cmisc_bfc:
		// clears a bit field in Rd (bits 15-12)
		// msb = bits 20-16, lsb = bits 11-7
		// if d == 15 then UNPREDICTABLE;
		tmp1 = bitrng(instr, 15, 12); // Rd
		if (tmp1 == 15) // Rd = PC
		{
			tmp2 = bitrng(instr, 20, 16); // msb
			tmp3 = bitrng(instr, 11, 7); // lsb
			// make the 'clear' mask
			tmp4 = (~0) << (tmp2 - tmp3 + 1);
			tmp4 = (~tmp4) << tmp3;
			tmp4 = ~tmp4;
			// clear bits
			tmp1 = rpi2_reg_context.storage[tmp1];
			tmp1 += 8; // PC runs 2 words ahead
			tmp1 &= tmp4;
			retval = set_arm_addr(tmp1);
			retval = set_unpred_addr(retval);
		}
		else
		{
			retval = set_addr_lin();
		}
		break;
	case arm_cmisc_bfi:
		// inserts (low)bits from Rn into bit field in Rd
		// Rd = bits 15-12, Rm = bits 3-0
		// msb = bits 20-16, lsb = bits 11-7
		// if d == 15 then UNPREDICTABLE
		tmp1 = bitrng(instr, 15, 12); // Rd
		if (tmp1 == 15) // Rd = PC
		{
			tmp2 = bitrng(instr, 20, 16); // msb
			tmp3 = bitrng(instr, 11, 7); // lsb
			// make the 'pick' mask
			tmp4 = (~0) << (tmp2 - tmp3 + 1);
			tmp4 = (~tmp4) << tmp3;
			// clear bits in Rd
			tmp1 = rpi2_reg_context.storage[tmp1];
			tmp1 += 8; // PC runs 2 words ahead
			tmp1 &= (~tmp4);
			// get bits from Rm
			tmp2 = bitrng(instr, 3, 0); // Rm
			if (tmp2 == 15)
			{
				tmp2 = rpi2_reg_context.storage[tmp2];
				tmp2 += 8; // PC runs 2 words ahead
			}
			else
			{
				tmp2 = rpi2_reg_context.storage[tmp2];
			}
			tmp2 &= tmp4;
			// insert bits
			tmp1 |= tmp2;
			retval = set_arm_addr(tmp1);
			retval = set_unpred_addr(retval);
		}
		else
		{
			retval = set_addr_lin();
		}
		break;
	case arm_cmisc_rbit:
		// reverse Rm bit order
		// if d == 15 || m == 15 then UNPREDICTABLE
		tmp1 = bitrng(instr, 15, 12); // Rd
		if (tmp1 == 15) // Rd = PC
		{
			tmp1 = bitrng(instr, 3, 0); // Rm
			if (tmp1 == 15)
			{
				tmp1 = rpi2_reg_context.storage[tmp1];
				tmp1 += 8;  // PC runs 2 words ahead
			}
			else
			{
				tmp1 = rpi2_reg_context.storage[tmp1];
			}
			// swap odd and even bits
			tmp1 = ((tmp1 & 0xaaaaaaaa) >> 1) | ((tmp1 & 0x55555555) << 1);
			// swap bit pairs
			tmp1 = ((tmp1 & 0xcccccccc) >> 2) | ((tmp1 & 0x33333333) << 2);
			// swap nibbles
			tmp1 = ((tmp1 & 0xf0f0f0f0) >> 4) | ((tmp1 & 0x0f0f0f0f) << 4);
			// swap bytes
			tmp1 = ((tmp1 & 0xff00ff00) >> 8) | ((tmp1 & 0x00ff00ff) << 8);
			// swap half words
			tmp1 = ((tmp1 & 0xffff0000) >> 16) | ((tmp1 & 0x0000ffff) << 16);

			retval = set_arm_addr(tmp1);
			retval = set_unpred_addr(retval);
		}
		else
		{
			retval = set_addr_lin();
		}
		break;
	case arm_cmisc_rev:
		// reverse Rm byte order
		// if d == 15 || m == 15 then UNPREDICTABLE
		tmp1 = bitrng(instr, 15, 12); // Rd
		if (tmp1 == 15) // Rd = PC
		{
			tmp1 = bitrng(instr, 3, 0); // Rm
			tmp2 = rpi2_reg_context.storage[tmp1]; // (Rm)
			if (tmp1 == 15) tmp2 += 8; // PC runs 2 words ahead
			tmp1 = (tmp2 & 0xff000000)  >> 24; // result lowest
			tmp1 |= (tmp2 & 0x00ff0000) >> 8;
			tmp1 |= (tmp2 & 0x0000ff00) << 8;
			tmp1 |= (tmp2 & 0x000000ff) << 24; // result highest
			retval = set_unpred_addr(retval);
		}
		else
		{
			retval = set_addr_lin();
		}
		break;

	case arm_cmisc_rev16:
		// reverse Rm half word byte orders
		// if d == 15 || m == 15 then UNPREDICTABLE
		tmp1 = bitrng(instr, 15, 12); // Rd
		if (tmp1 == 15) // Rd = PC
		{
			tmp1 = bitrng(instr, 3, 0); // Rm
			tmp2 = rpi2_reg_context.storage[tmp1]; // (Rm)
			if (tmp1 == 15) tmp2 += 8; // PC runs 2 words ahead
			tmp1 = (tmp2 & 0xff000000)  >> 8;
			tmp1 |= (tmp2 & 0x00ff0000) << 8; // result highest
			tmp1 |= (tmp2 & 0x0000ff00) >> 8; // result lowest
			tmp1 |= (tmp2 & 0x000000ff) << 8;
			retval = set_unpred_addr(retval);
		}
		else
		{
			retval = set_addr_lin();
		}
		break;
	case arm_cmisc_revsh:
		// Byte-Reverse Signed Halfword and sign-extend
		// if d == 15 || m == 15 then UNPREDICTABLE
		tmp1 = bitrng(instr, 15, 12); // Rd
		if (tmp1 == 15) // Rd = PC
		{
			tmp1 = bitrng(instr, 3, 0); // Rm
			tmp2 = rpi2_reg_context.storage[tmp1]; // (Rm)
			if (tmp1 == 15) tmp2 += 8; // PC runs 2 words ahead
			if (bit(tmp2, 7)) // the result will be negative
				tmp3 = (~0) << 16;
			else
				tmp3 = 0;
			tmp3 |= (tmp2 & 0x0000ff00) >> 8;
			tmp3 |= (tmp2 & 0x000000ff) << 8;

			retval = set_arm_addr(tmp3);
			retval = set_unpred_addr(retval);
		}
		else
		{
			retval = set_addr_lin();
		}
		break;
	case arm_cmisc_sbfx:
	case arm_cmisc_ubfx:
		// Signed Bit Field Extract
		// if d == 15 || n == 15 then UNPREDICTABLE
		// width minus 1 = bits 20-16, lsb = bits 11-7
		tmp1 = bitrng(instr, 15, 12); // Rd
		if (tmp1 == 15) // Rd = PC
		{
			tmp2 = bitrng(instr, 20, 16); // widhtminus1
			tmp3 = bitrng(instr, 11, 7); // lsb
			// make the 'pick' mask
			tmp4 = (~0) << (tmp2 + 1);
			tmp4 = (~tmp4) << tmp3;
			// get bits from Rm
			tmp1 = bitrng(instr, 3, 0); // Rn
			if (tmp1 == 15)
			{
				tmp1 = rpi2_reg_context.storage[tmp1];
				tmp1 += 8; // PC runs 2 words ahead
			}
			else
			{
				tmp1 = rpi2_reg_context.storage[tmp1];
			}
			tmp1 &= tmp4;
			// make it an unsigned number
			tmp1 >>= tmp3;
			if (extra == arm_cmisc_sbfx)
			{
				// if msb is '1'
				if (bit(tmp1, tmp2))
				{
					// sign-extend to signed
					tmp1 |= (~0)<<(tmp2 + 1);
				}
			}
			retval = set_arm_addr(tmp1);
			retval = set_unpred_addr(retval);
		}
		else
		{
			retval = set_addr_lin();
		}
		break;
	case arm_cmisc_sel:
		// select bytes by GE-flags
		// if d == 15 || n == 15 || m == 15 then UNPREDICTABLE
		tmp1 = bitrng(instr, 15, 12); // Rd
		if (tmp1 == 15) // Rd = PC
		{
			tmp3 = bitrng(instr, 19, 16); // Rn
			tmp1 = rpi2_reg_context.storage[tmp3]; // (Rn)
			if (tmp3 == 15) tmp1 += 8; // PC runs 2 words ahead
			tmp3 = bitrng(instr, 3, 0); // Rm
			tmp2 = rpi2_reg_context.storage[tmp3]; // (Rm)
			if (tmp3 == 15) tmp2 += 8; // PC runs 2 words ahead
			tmp3 = rpi2_reg_context.reg.cpsr;
			tmp4 = 0;
			// select Rn or Rm by the GE-bits
			tmp4 |= ((bit(tmp3,19)?tmp1:tmp2) & 0xff000000);
			tmp4 |= ((bit(tmp3,18)?tmp1:tmp2) & 0x00ff0000);
			tmp4 |= ((bit(tmp3,17)?tmp1:tmp2) & 0x0000ff00);
			tmp4 |= ((bit(tmp3,16)?tmp1:tmp2) & 0x000000ff);
			retval = set_arm_addr(tmp4);
			retval = set_unpred_addr(retval);
		}
		else
		{
			retval = set_addr_lin();
		}
		break;
	case arm_cmisc_usad8:
	case arm_cmisc_usada8:
		// calculate sum of absolute byte differences
		// if d == 15 || n == 15 || m == 15 then UNPREDICTABLE
		tmp1 = bitrng(instr, 15, 12); // Rd
		if (tmp1 == 15) // Rd = PC
		{
			tmp3 = bitrng(instr, 11, 8); // Rm
			tmp1 = rpi2_reg_context.storage[tmp3]; // (Rm)
			if (tmp3 == 15) tmp1 += 8; // PC runs 2 words ahead
			tmp3 = bitrng(instr, 3, 0); // Rn
			tmp2 = rpi2_reg_context.storage[tmp3]; // (Rn)
			if (tmp3 == 15) tmp2 += 8; // PC runs 2 words ahead
			// sum of absolute differences
			tmp3 = 0;
			for (tmp4 = 0; tmp4 < 4; tmp4++) // for each byte
			{
				htmp1 = (unsigned short)((tmp1 >> (8*tmp4)) & 0xff);
				htmp2 = (unsigned short)((tmp2 >> (8*tmp4)) & 0xff);
				if (htmp2 < htmp1) htmp1 -= htmp2;
				else htmp1 = htmp2 - htmp1;
				tmp3 += (unsigned int)htmp1;
			}

			if (extra == arm_cmisc_usada8)
			{
				// USADA8 (=USAD8 with Ra)
				tmp1 = rpi2_reg_context.storage[bitrng(instr, 15, 12)]; // (Ra)
				tmp3 += tmp1;
			}
			retval = set_arm_addr(tmp3);
			retval = set_unpred_addr(retval);
		}
		else
		{
			retval = set_addr_lin();
		}
		break;
	default:
		break;
	}
	return retval;
}

instr_next_addr_t arm_core_data_pack(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4, tmp5;
	int stmp1, stmp2;

	retval = set_undef_addr();

	switch (extra)
	{
	case arm_pack_pkh:
		// pack half words
		// if d == 15 || n == 15 || m == 15 then UNPREDICTABLE
		// Rn = bits 19-16, Rd = bits 15-12, Rm = bits 3-0
		// tb = bit 6, imm = bits 11-7
		tmp4 = bitrng(instr, 15, 12); // Rd
		if (tmp4 == 15) // Rd = PC
		{
			// operands
			tmp5 = bitrng(instr, 3, 0); // Rm
			tmp1 = rpi2_reg_context.storage[tmp5]; // (Rm)
			if (tmp5 == 15) tmp1 += 8;  // PC runs 2 words ahead
			tmp2 = bitrng(instr, 11, 7); // imm5
			tmp5 = bitrng(instr, 19, 16); // Rn
			tmp3 = rpi2_reg_context.storage[tmp5]; // (Rn)
			if (tmp5 == 15) tmp3 += 8;  // PC runs 2 words ahead
			if (bit(instr, 6)) // tb: 1=top from Rn, bottom from operand2
			{
				// PKHTB: shift=ASR (sign extended shift)
				// >> is ASR only for signed numbers
				stmp1 = (int)tmp1;
				stmp1 >>= tmp2;
				tmp1 = (unsigned int) stmp1;
				// Rd[31:16] = Rn[31:16]
				tmp4 = tmp3 & 0xffff0000;
				// Rd[15:0] = shifted_op[15:0]
				tmp4 |= tmp1 & 0xffff;
			}
			else // bt: 1=top from operand2, bottom from Rn
			{
				// PKHBT: shift=LSL
				tmp1 <<= tmp2;
				// Rd[31:16] = shifted_op[31:16]
				tmp4 = tmp1 & 0xffff0000;
				// Rd[15:0] = Rn[15:0]
				tmp4 |= tmp3 & 0xffff;
			}
			retval = set_arm_addr(tmp4);
			retval = set_unpred_addr(retval);
		}
		else
		{
			retval = set_addr_lin();
		}
		break;
	case arm_pack_sxtb:
		// sign-extend byte
	case arm_pack_uxtb:
		// zero-extend byte
	case arm_pack_sxtab:
		// sign-extend byte and add
	case arm_pack_uxtab:
		// zero-extend byte and add
	case arm_pack_sxtab16:
		// sign-extend dual byte and add
	case arm_pack_uxtab16:
		// zero-extend dual byte and add
	case arm_pack_sxtb16:
		// sign-extend dual byte
	case arm_pack_uxtb16:
		// zero-extend dual byte
	case arm_pack_sxth:
		// sign-extend half word
	case arm_pack_sxtah:
		// sign-extend half word and add
	case arm_pack_uxtah:
		// zero-extend half word and add
	case arm_pack_uxth:
		// zero-extend half word

		// if d == 15 || m == 15 then UNPREDICTABLE;
		// Rn = bits 19-16, Rd = bits 15-12, Rm = bits 3-0
		// Rm ror = bits 11-10 (0, 8, 16, or 24 bits)
		// bit 22: 1=unsigned, bit 21-20: 00=b16, 10=b, 11=h
		tmp4 = bitrng(instr, 15, 12); // Rd
		if (tmp4 == 15) // Rd = PC
		{
			tmp5 = bitrng(instr, 3, 0); // Rm
			tmp1 = rpi2_reg_context.storage[tmp5]; // (Rm)
			if (tmp5 == 15) tmp1 += 8; // PC runs 2 words ahead
			// Rotate Rm
			tmp1 = instr_util_rorb(tmp1, (int)bitrng(instr, 3, 0));

			// prepare extract mask
			switch (bitrng(instr, 21, 20))
			{
			case 0: // dual byte
				tmp2 = 0xff00ff00; // mask for dual-byte
				break;
			case 2: // single byte
				tmp2 = (~0) << 8; // mask for byte
				break;
			case 3:
				tmp2 = (~0) << 16; // mask for half word
				break;
			default:
				// shouldn't get here
				break;
			}

			tmp3 = 0; // clear extraction result

			if (bit(instr, 22)) // signed?
			{
				// prepare sign extension bits as needed
				if (bitrng(instr, 21, 20) == 0) // sxtb16 / sxtab16
				{
					if (bit(tmp1, 23)) // upper byte is negative
					{
						// sign extension bits for upper part
						tmp3 |= 0xff000000;
					}
					if (bit(tmp1, 7)) // lower byte is negative
					{
						// sign extension bits for lower part
						tmp3 |= 0x0000ff00;
					}
				}
				else
				{
					if ((tmp1 & (~tmp2)) & (tmp2 >> 1))
					{
						tmp3 = tmp2; // set sign-extension bits
					}
				}
			}
			tmp3 |= (tmp1 & (~tmp2)); // extract data

			// if Rn = PC then no Rn
			if (bitrng(instr, 19, 16) != 15)
			{
				tmp1 = rpi2_reg_context.storage[bitrng(instr, 19, 16)]; // Rn
				if (bit(instr, 22)) // signed
				{
					if (bitrng(instr, 21, 20) == 0) // sxtab16
					{
						// low half
						stmp1 = instr_util_shgetlo(tmp1);
						stmp1 += instr_util_shgetlo(tmp3 );
						// high half
						stmp2 = instr_util_shgethi(tmp1);
						stmp2 += instr_util_shgethi(tmp3 );

						tmp3 = instr_util_ustuffs16(stmp2, stmp1);;
					}
					else
					{
						stmp1 = (int)tmp3 + (int)tmp1;
						tmp3 = (unsigned int) tmp3;
					}
				}
				else // unsigned
				{
					if (bitrng(instr, 21, 20) == 0) // uxtab16
					{
						tmp4 = bitrng(tmp1, 31, 16) + bitrng(tmp3, 31, 16);
						tmp2 = bitrng(tmp1, 15, 0) + bitrng(tmp3, 15, 0);
						tmp3 = instr_util_ustuffu16(tmp4, tmp2);
					}
					else
					{
						tmp3 += tmp1;
					}
				}
			}
			retval = set_arm_addr(tmp3);
			retval = set_unpred_addr(retval);
		}
		else
		{
			retval = set_addr_lin();
		}
		break;
	default:
		// shouldn't get here
		break;
	}
	return retval;
}

instr_next_addr_t arm_core_data_par(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4, tmp5, tmp6;
	int stmp1, stmp2, stmp3, stmp4;

	retval = set_undef_addr();

	// parallel add&subtr (Rd: bits 15-12, Rn:19-16, Rm:3-0)
	// unify all these with execution (if Rd = PC)
	// bit 22=1: unsigned, bits 21,20: 00=undef, 01=basic, 10=Q and 11=H
	// bits 7-5 = op 0,1,2,3,4,7 (5,6 = UNDEFINED, but shouldn't come here)
	// if d == 15 || n == 15 || m == 15 then UNPREDICTABLE;
	if (bitrng(instr, 15, 12) == 15) // Rd = PC
	{
		tmp3 = bitrng(instr, 19, 16); // Rn
		tmp1 = rpi2_reg_context.storage[tmp3]; // (Rn)
		if (tmp3 == 15) tmp1 += 8; // PC runs 2 words ahead
		tmp3 = bitrng(instr, 3, 0); // Rm
		tmp2 = rpi2_reg_context.storage[tmp3]; // (Rm)
		if (tmp3 == 15) tmp2 += 8; // PC runs 2 words ahead
		switch (extra)
		{
		case arm_par_qadd16:
			stmp1 = instr_util_ssat(instr_util_shgethi(tmp1)
					+ instr_util_shgethi(tmp2), 16);
			stmp2 = instr_util_ssat(instr_util_shgetlo(tmp1)
					+ instr_util_shgetlo(tmp2), 16);
			tmp4 = instr_util_ustuffs16(stmp1, stmp2);
			break;
		case arm_par_qsub16:
			stmp1 = instr_util_ssat(instr_util_shgethi(tmp1)
					- instr_util_shgethi(tmp2), 16);
			stmp2 = instr_util_ssat(instr_util_shgetlo(tmp1)
					- instr_util_shgetlo(tmp2), 16);
			tmp4 = instr_util_ustuffs16(stmp1, stmp2);
			break;
		case arm_par_sadd16:
			stmp1 = instr_util_shgethi(tmp1) + instr_util_shgethi(tmp2);
			stmp2 = instr_util_shgetlo(tmp1) + instr_util_shgetlo(tmp2);
			tmp4 = instr_util_ustuffs16(stmp1, stmp2);
			break;
		case arm_par_ssub16:
			stmp1 = instr_util_shgethi(tmp1) - instr_util_shgethi(tmp2);
			stmp2 = instr_util_shgetlo(tmp1) - instr_util_shgetlo(tmp2);
			tmp4 = instr_util_ustuffs16(stmp1, stmp2);
			break;
		case arm_par_shadd16:
			stmp1 = instr_util_shgethi(tmp1) + instr_util_shgethi(tmp2);
			stmp2 = instr_util_shgetlo(tmp1) + instr_util_shgetlo(tmp2);
			tmp4 = instr_util_ustuffs16(stmp1 >> 1, stmp2 >> 1);
			break;
		case arm_par_shsub16:
			stmp1 = instr_util_shgethi(tmp1) + instr_util_shgethi(tmp2);
			stmp2 = instr_util_shgetlo(tmp1) + instr_util_shgetlo(tmp2);
			tmp4 = instr_util_ustuffs16(stmp1 >> 1, stmp2 >> 1);
			break;
		case arm_par_qadd8:
			stmp4 = instr_util_signx_byte((tmp1 >> 24) & 0xff);
			stmp4 = instr_util_ssat((stmp4
					+ instr_util_signx_byte((tmp2 >> 24) & 0xff)), 8);
			stmp3 = instr_util_signx_byte((tmp1 >> 16) & 0xff);
			stmp3 = instr_util_ssat((stmp3
					+ instr_util_signx_byte((tmp2 >> 16) & 0xff)), 8);
			stmp2 = instr_util_signx_byte((tmp1 >> 8) & 0xff);
			stmp2 = instr_util_ssat((stmp2
					+ instr_util_signx_byte((tmp2 >> 8) & 0xff)), 8);
			stmp1 = instr_util_signx_byte(tmp1 & 0xff);
			stmp1 = instr_util_ssat((stmp1
					+ instr_util_signx_byte(tmp2 & 0xff)), 8);
			tmp4 = instr_util_ustuffs8(stmp4, stmp3, stmp2, stmp1);
			break;
		case arm_par_qsub8:
			stmp4 = instr_util_signx_byte((tmp1 >> 24) & 0xff);
			stmp4 = instr_util_ssat((stmp4
					- instr_util_signx_byte((tmp2 >> 24) & 0xff)), 8);
			stmp3 = instr_util_signx_byte((tmp1 >> 16) & 0xff);
			stmp3 = instr_util_ssat((stmp3
					- instr_util_signx_byte((tmp2 >> 16) & 0xff)), 8);
			stmp2 = instr_util_signx_byte((tmp1 >> 8) & 0xff);
			stmp2 = instr_util_ssat((stmp2
					- instr_util_signx_byte((tmp2 >> 8) & 0xff)), 8);
			stmp1 = instr_util_signx_byte(tmp1 & 0xff);
			stmp1 = instr_util_ssat((stmp1
					- instr_util_signx_byte(tmp2 & 0xff)), 8);
			tmp4 = instr_util_ustuffs8(stmp4, stmp3, stmp2, stmp1);
			break;
		case arm_par_sadd8:
			stmp4 = instr_util_signx_byte((tmp1 >> 24) & 0xff);
			stmp4 += instr_util_signx_byte((tmp2 >> 24) & 0xff);

			stmp3 = instr_util_signx_byte((tmp1 >> 16) & 0xff);
			stmp3 += instr_util_signx_byte((tmp2 >> 16) & 0xff);

			stmp2 = instr_util_signx_byte((tmp1 >> 8) & 0xff);
			stmp2 += instr_util_signx_byte((tmp2 >> 8) & 0xff);

			stmp1 = instr_util_signx_byte(tmp1 & 0xff);
			stmp1 += instr_util_signx_byte(tmp2 & 0xff);

			tmp4 = instr_util_ustuffs8(stmp4, stmp3, stmp2, stmp1);
			break;
		case arm_par_shadd8:
			stmp4 = instr_util_signx_byte((tmp1 >> 24) & 0xff);
			stmp4 += instr_util_signx_byte((tmp2 >> 24) & 0xff);

			stmp3 = instr_util_signx_byte((tmp1 >> 16) & 0xff);
			stmp3 += instr_util_signx_byte((tmp2 >> 16) & 0xff);

			stmp2 = instr_util_signx_byte((tmp1 >> 8) & 0xff);
			stmp2 += instr_util_signx_byte((tmp2 >> 8) & 0xff);

			stmp1 = instr_util_signx_byte(tmp1 & 0xff);
			stmp1 += instr_util_signx_byte(tmp2 & 0xff);
			tmp4 = instr_util_ustuffs8(stmp4 >> 1, stmp3 >> 1, stmp2 >> 1, stmp1 >> 1);
			break;
		case arm_par_shsub8:
			stmp4 = instr_util_signx_byte((tmp1 >> 24) & 0xff);
			stmp4 -= instr_util_signx_byte((tmp2 >> 24) & 0xff);

			stmp3 = instr_util_signx_byte((tmp1 >> 16) & 0xff);
			stmp3 -= instr_util_signx_byte((tmp2 >> 16) & 0xff);

			stmp2 = instr_util_signx_byte((tmp1 >> 8) & 0xff);
			stmp2 -= instr_util_signx_byte((tmp2 >> 8) & 0xff);

			stmp1 = instr_util_signx_byte(tmp1 & 0xff);
			stmp1 -= instr_util_signx_byte(tmp2 & 0xff);
			tmp4 = instr_util_ustuffs8(stmp4 >> 1, stmp3 >> 1, stmp2 >> 1, stmp1 >> 1);
			break;
		case arm_par_ssub8:
			stmp4 = instr_util_signx_byte((tmp1 >> 24) & 0xff);
			stmp4 -= instr_util_signx_byte((tmp2 >> 24) & 0xff);

			stmp3 = instr_util_signx_byte((tmp1 >> 16) & 0xff);
			stmp3 -= instr_util_signx_byte((tmp2 >> 16) & 0xff);

			stmp2 = instr_util_signx_byte((tmp1 >> 8) & 0xff);
			stmp2 -= instr_util_signx_byte((tmp2 >> 8) & 0xff);

			stmp1 = instr_util_signx_byte(tmp1 & 0xff);
			stmp1 -= instr_util_signx_byte(tmp2 & 0xff);

			tmp4 = instr_util_ustuffs8(stmp4, stmp3, stmp2, stmp1);
			break;
		case arm_par_qasx:
			stmp1 = instr_util_ssat(instr_util_shgetlo(tmp1)
					+ instr_util_shgethi(tmp2), 16);
			stmp2 = instr_util_ssat(instr_util_shgethi(tmp1)
					- instr_util_shgetlo(tmp2), 16);
			tmp4 = instr_util_ustuffs16(stmp1, stmp2);
			break;
		case arm_par_qsax:
			stmp1 = instr_util_ssat(instr_util_shgethi(tmp1)
					+ instr_util_shgetlo(tmp2), 16);
			stmp2 = instr_util_ssat(instr_util_shgetlo(tmp1)
					- instr_util_shgethi(tmp2), 16);
			tmp4 = instr_util_ustuffs16(stmp2, stmp1);
			break;
		case arm_par_sasx:
			stmp1 = instr_util_shgetlo(tmp1) + instr_util_shgethi(tmp2);
			stmp2 = instr_util_shgethi(tmp1) - instr_util_shgetlo(tmp2);
			tmp4 = instr_util_ustuffs16(stmp1, stmp2);
			break;
		case arm_par_shasx:
			stmp1 = instr_util_shgetlo(tmp1) + instr_util_shgethi(tmp2);
			stmp2 = instr_util_shgethi(tmp1) - instr_util_shgetlo(tmp2);
			tmp4 = instr_util_ustuffs16(stmp1 >> 1, stmp2 >> 1);
			break;
		case arm_par_shsax:
			stmp1 = instr_util_shgethi(tmp1) + instr_util_shgetlo(tmp2);
			stmp2 = instr_util_shgetlo(tmp1) - instr_util_shgethi(tmp2);
			tmp4 = instr_util_ustuffs16(stmp1 >> 1, stmp2 >> 1);
			break;
		case arm_par_ssax:
			stmp1 = instr_util_shgethi(tmp1) + instr_util_shgetlo(tmp2);
			stmp2 = instr_util_shgetlo(tmp1) - instr_util_shgethi(tmp2);
			tmp4 = instr_util_ustuffs16(stmp2, stmp1);
			break;
		case arm_par_uadd16:
			tmp3 = ((tmp1 >> 16) & 0xffff) + ((tmp2 >> 16) & 0xffff);
			tmp2 = (tmp1 & 0xffff) + (tmp2 & 0xffff);
			tmp4 = instr_util_ustuffu16(tmp3, tmp2);
			break;
		case arm_par_uhadd16:
			tmp3 = ((tmp1 >> 16) & 0xffff) + ((tmp2 >> 16) & 0xffff);
			tmp2 = (tmp1 & 0xffff) + (tmp2 & 0xffff);
			tmp4 = instr_util_ustuffu16(tmp3 >> 1, tmp2 >> 1);
			break;
		case arm_par_uhsub16:
			tmp3 = ((tmp1 >> 16) & 0xffff) - ((tmp2 >> 16) & 0xffff);
			tmp2 = (tmp1 & 0xffff) - (tmp2 & 0xffff);
			tmp4 = instr_util_ustuffu16(tmp3 >> 1, tmp2 >> 1);
			break;
		case arm_par_uqadd16:
			tmp3 = instr_util_usat(((tmp1 >> 16) & 0xffff)
					+ ((tmp2 >> 16) & 0xffff), 16);
			tmp2 = instr_util_usat((tmp1 & 0xffff) + (tmp2 & 0xffff), 16);
			tmp4 = instr_util_ustuffu16(tmp3, tmp2);
			break;
		case arm_par_uqsub16:
			tmp3 = instr_util_usat(((tmp1 >> 16) & 0xffff)
					- ((tmp2 >> 16) & 0xffff), 16);
			tmp2 = instr_util_usat((tmp1 & 0xffff) - (tmp2 & 0xffff), 16);
			tmp4 = instr_util_ustuffu16(tmp3, tmp2);
			break;
		case arm_par_usub16:
			tmp3 = ((tmp1 >> 16) & 0xffff) - ((tmp2 >> 16) & 0xffff);
			tmp2 = (tmp1 & 0xffff) - (tmp2 & 0xffff);
			tmp4 = instr_util_ustuffu16(tmp3, tmp2);
			break;
		case arm_par_uadd8:
			tmp6 = ((tmp1 >> 24) & 0xff) + ((tmp2 >> 24) & 0xff);
			tmp5 = ((tmp1 >> 16) & 0xff) + ((tmp2 >> 16) & 0xff);
			tmp4 = ((tmp1 >> 8) & 0xff) + ((tmp2 >> 8) & 0xff);
			tmp3 = (tmp1 & 0xff) + (tmp2 & 0xff);

			tmp4 = instr_util_ustuffu8(tmp6, tmp5, tmp4, tmp3);
			break;
		case arm_par_uhadd8:
			tmp6 = ((tmp1 >> 24) & 0xff) + ((tmp2 >> 24) & 0xff);
			tmp5 = ((tmp1 >> 16) & 0xff) + ((tmp2 >> 16) & 0xff);
			tmp4 = ((tmp1 >> 8) & 0xff) + ((tmp2 >> 8) & 0xff);
			tmp3 = (tmp1 & 0xff) + (tmp2 & 0xff);

			tmp4 = instr_util_ustuffu8(tmp6 >> 1, tmp5 >> 1, tmp4 >> 1, tmp3 >> 1);
			break;
		case arm_par_uhsub8:
			tmp6 = ((tmp1 >> 24) & 0xff) - ((tmp2 >> 24) & 0xff);
			tmp5 = ((tmp1 >> 16) & 0xff) - ((tmp2 >> 16) & 0xff);
			tmp4 = ((tmp1 >> 8) & 0xff) - ((tmp2 >> 8) & 0xff);
			tmp3 = (tmp1 & 0xff) - (tmp2 & 0xff);

			tmp4 = instr_util_ustuffu8(tmp6 >> 1, tmp5 >> 1, tmp4 >> 1, tmp3 >> 1);
			break;
		case arm_par_uqadd8:
			tmp6 = ((tmp1 >> 24) & 0xff) + ((tmp2 >> 24) & 0xff);
			tmp6 = instr_util_usat((int)tmp6, 8);

			tmp5 = ((tmp1 >> 16) & 0xff) + ((tmp2 >> 16) & 0xff);
			tmp5 = instr_util_usat((int)tmp5, 8);

			tmp4 = ((tmp1 >> 8) & 0xff) + ((tmp2 >> 8) & 0xff);
			tmp4 = instr_util_usat((int)tmp4, 8);

			tmp3 = (tmp1 & 0xff) + (tmp2 & 0xff);
			tmp3 = instr_util_usat((int)tmp3, 8);

			tmp4 = instr_util_ustuffu8(tmp6, tmp5, tmp4, tmp3);
			break;
		case arm_par_uqsub8:
			tmp6 = ((tmp1 >> 24) & 0xff) - ((tmp2 >> 24) & 0xff);
			tmp6 = instr_util_usat((int)tmp6, 8);

			tmp5 = ((tmp1 >> 16) & 0xff) - ((tmp2 >> 16) & 0xff);
			tmp5 = instr_util_usat((int)tmp5, 8);

			tmp4 = ((tmp1 >> 8) & 0xff) - ((tmp2 >> 8) & 0xff);
			tmp4 = instr_util_usat((int)tmp4, 8);

			tmp3 = (tmp1 & 0xff) - (tmp2 & 0xff);
			tmp3 = instr_util_usat((int)tmp3, 8);

			tmp4 = instr_util_ustuffu8(tmp6, tmp5, tmp4, tmp3);
			break;
		case arm_par_usub8:
			tmp6 = ((tmp1 >> 24) & 0xff) - ((tmp2 >> 24) & 0xff);
			tmp5 = ((tmp1 >> 16) & 0xff) - ((tmp2 >> 16) & 0xff);
			tmp4 = ((tmp1 >> 8) & 0xff) - ((tmp2 >> 8) & 0xff);
			tmp3 = (tmp1 & 0xff) - (tmp2 & 0xff);

			tmp4 = instr_util_ustuffu8(tmp6, tmp5, tmp4, tmp3);
			break;
		case arm_par_uasx:
			tmp3 = (tmp1 & 0xffff) + ((tmp2 >> 16) & 0xffff);
			tmp4 = ((tmp1 >> 16) & 0xffff) - (tmp2 & 0xffff);

			tmp4 = instr_util_ustuffu16(tmp3, tmp4);
			break;
		case arm_par_uhasx:
			tmp3 = (tmp1 & 0xffff) + ((tmp2 >> 16) & 0xffff);
			tmp4 = ((tmp1 >> 16) & 0xffff) - (tmp2 & 0xffff);

			tmp4 = instr_util_ustuffu16(tmp3 >> 1, tmp4 >> 1);
			break;
		case arm_par_uhsax:
			tmp3 = ((tmp1 >> 16) & 0xffff) + (tmp2 & 0xffff);
			tmp4 = (tmp1 & 0xffff) - ((tmp2 >> 16) & 0xffff);

			tmp4 = instr_util_ustuffu16(tmp3 >> 1, tmp4 >> 1);
			break;
		case arm_par_uqasx:
			tmp3 = (tmp1 & 0xffff) + ((tmp2 >> 16) & 0xffff);
			tmp3 = instr_util_usat((int)tmp3, 16);

			tmp4 = ((tmp1 >> 16) & 0xffff) - (tmp2 & 0xffff);
			tmp4 = instr_util_usat((int)tmp4, 16);

			tmp4 = instr_util_ustuffu16(tmp3, tmp4);
			break;
		case arm_par_uqsax:
			tmp3 = ((tmp1 >> 16) & 0xffff) + (tmp2 & 0xffff);
			tmp3 = instr_util_usat((int)tmp3, 16);

			tmp4 = (tmp1 & 0xffff) - ((tmp2 >> 16) & 0xffff);
			tmp4 = instr_util_usat((int)tmp4, 16);

			tmp4 = instr_util_ustuffu16(tmp3, tmp4);
			break;
		case arm_par_usax:
			tmp3 = ((tmp1 >> 16) & 0xffff) + (tmp2 & 0xffff);
			tmp4 = (tmp1 & 0xffff) - ((tmp2 >> 16) & 0xffff);

			tmp4 = instr_util_ustuffu16(tmp3, tmp4);
			break;
		default:
			// shouldn't get here
			break;
		}
		retval = set_arm_addr(tmp4);
		retval = set_unpred_addr(retval);
	}
	else
	{
		retval = set_addr_lin();
	}
	return retval;
}

instr_next_addr_t arm_core_data_sat(unsigned int instr, ARM_decode_extra_t extra)
{
	// if d == 15 || n == 15 || m == 15 then UNPREDICTABLE
	// Rn = bits 19-16, Rm = bits 3-0, Rd = bits 15-12
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3;
	int stmp1, stmp2;
	long long int sltmp;

	retval = set_undef_addr();

	if (bitrng(instr, 15, 12) == 15) // Rd = PC
	{
		if (bitrng(instr, 24, 23) == 2)
		{
			// QADD/QDADD/QSUB/QDSUB
			// if d == 15 || n == 15 || m == 15 then UNPREDICTABLE
			// Rn = bits 19-16, Rm = bits 3-0, Rd = bits 15-12
			tmp3 = bitrng(instr, 19, 16); // Rn
			tmp1 = rpi2_reg_context.storage[tmp3]; // (Rn)
			if (tmp3 == 15) tmp1 += 8; // PC runs 2 words ahead
			tmp3 = bitrng(instr, 3, 0); // Rm
			tmp2 = rpi2_reg_context.storage[tmp3]; // (Rm)
			if (tmp3 == 15) tmp2 += 8; // PC runs 2 words ahead
			switch(extra)
			{
			case arm_sat_qadd:
				sltmp = ((long long int)tmp2) + ((long long int)tmp1);
				sltmp = instr_util_lssat(sltmp, 32);
				tmp3 = (unsigned int)(sltmp & 0xffffffffL);
				break;
			case arm_sat_qdadd:
				sltmp = instr_util_lssat(2 * ((long long int)tmp1), 32);
				sltmp = instr_util_lssat(((long long int)tmp2) - sltmp, 32);
				tmp3 = (unsigned int)(sltmp & 0xffffffffL);
				break;
			case arm_sat_qdsub:
				sltmp = instr_util_lssat(2 * ((long long int)tmp1), 32);
				sltmp = instr_util_lssat(((long long int)tmp2) - sltmp, 32);
				tmp3 = (unsigned int)(sltmp & 0xffffffffL);
				break;
			case arm_sat_qsub:
				sltmp = ((long long int)tmp2) + ((long long int)tmp1);
				sltmp = instr_util_lssat(sltmp, 32);
				tmp3 = (unsigned int)(sltmp & 0xffffffffL);
				break;
			default:
				// shouldn't get here
				break;
			}
		}
		else
		{
			// SSAT/SSAT16/USAT/USAT16
			// if d == 15 || n == 15 then UNPREDICTABLE;
			// Rn = bits 3-0, Rd = bits 15-12, imm = bits 11-6
			// sat_imm = bits 20/19 - 16, shift = bit 6
			tmp3 = bitrng(instr, 3, 0); // Rn
			tmp1 = rpi2_reg_context.storage[tmp3]; // (Rn)
			if (tmp3 == 15) tmp1 += 8; // PC runs 2 words ahead
			switch (extra)
			{
			case arm_sat_ssat:
			case arm_sat_usat:
				// make operand
				tmp2 = bitrng(instr, 11, 7); // shift count
				stmp1 = (int)tmp1;
				if (bit(instr, 6))
				{
					// signed int used to make '>>' ASR
					if (tmp2 == 0) // ASR #32
					{
						stmp1 >>= 31;
					}
					else // ASR imm
					{
						stmp1 >>= tmp2;
					}
				}
				else
				{
					if (tmp2 != 0)
					{
						stmp1 <<= tmp2; // LSL imm
					}
					// else LSL #0
				}
				if (extra == arm_sat_ssat)
				{
					stmp2 = instr_util_ssat(stmp1, bitrng(instr, 20, 16) -1);
				}
				else // arm_sat_usat
				{
					stmp2 = (int)instr_util_usat(stmp1, bitrng(instr, 20, 16));
				}
				tmp3 = (unsigned int)stmp2;
				break;
			case arm_sat_ssat16:
				stmp1 = instr_util_ssat(instr_util_shgetlo(tmp1),
						bitrng(instr, 19, 16));
				stmp2 = instr_util_ssat(instr_util_shgethi(tmp1),
						bitrng(instr, 19, 16));
				tmp3 = instr_util_ustuffs16(stmp2, stmp1);
				break;
			case arm_sat_usat16:
				stmp1 = instr_util_usat(instr_util_shgetlo(tmp1),
						bitrng(instr, 19, 16));
				stmp2 = instr_util_usat(instr_util_shgethi(tmp1),
						bitrng(instr, 19, 16));
				tmp3 = instr_util_ustuffs16(stmp2, stmp1);
				break;
			default:
				// shouldn't get here
				break;
			}
		}
		retval = set_arm_addr(tmp3);
		retval = set_unpred_addr(retval);
	}
	else
	{
		retval = set_addr_lin();
	}
	return retval;
}

instr_next_addr_t arm_core_data_bit(unsigned int instr, ARM_decode_extra_t extra)
{
	// Rd = bits 15-12, Rm = bits 3-0, imm = bits 11-7
	// Rd = bits 15-12, Rm = bits 11-8, Rn = bits 3-0
	// shift type = bits 6-5: 0=LSL, 1=LSR, 2=ASR, 3=ROR/RRX
	// exception returning:
	// find out operation result, set cpsr = spsr, pc = result (jump)
	// TODO: add check for T-bit, return thumb address if set (SPSR)
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4;
	int stmp1;

	retval = set_undef_addr();
	if (bitrng(instr, 15, 12) == 15) // Rd = PC
	{
		tmp3 = bitrng(instr, 3, 0); // Rn / Rm if imm
		tmp1 = rpi2_reg_context.storage[tmp3]; // (Rn)
		if (tmp3 == 15) tmp1 += 8; // PC runs 2 words ahead
		switch (extra)
		{
		case arm_ret_asr_imm:
		case arm_cdata_asr_imm:
			tmp2 = bitrng(instr, 11, 7); // imm
			stmp1 = (int) tmp1; // for '>>' to act as ASR instead of LSR
			tmp3 = (unsigned int) (stmp1 >> tmp2);
			break;
		case arm_ret_lsr_imm:
		case arm_cdata_lsr_imm:
			tmp2 = bitrng(instr, 11, 7); // imm
			tmp3 = tmp1 >> tmp2;
			break;
		case arm_ret_lsl_imm:
		case arm_cdata_lsl_imm:
			tmp2 = bitrng(instr, 11, 7); // imm
			tmp3 = tmp1 << tmp2;
			break;
		case arm_ret_mov_pc:
		case arm_cdata_mov_r:
			tmp3 = tmp1;
			break;
		case arm_ret_ror_imm:
		case arm_cdata_ror_imm:
			tmp2 = bitrng(instr, 11, 7); // imm
			tmp4 = bitrng(tmp1, tmp2, 0); // catch the dropping bits
			tmp4 <<= (32 - tmp2); // prepare for putting back in the top
			tmp3 = tmp1 >> tmp2;
			tmp2 = (~0) << tmp2; // make mask
			tmp3 = (tmp3 & (~tmp2)) || (tmp4 & tmp2); // add dropped bits into result
			break;
		case arm_ret_rrx_pc:
		case arm_cdata_rrx_r:
			tmp2 = bit(rpi2_reg_context.reg.cpsr, 29); // carry-flag
			tmp3 = (tmp1 >> 1) | (tmp2 << 31);
			break;
		case arm_cdata_asr_r:
			// if d == 15 || n == 15 || m == 15 then UNPREDICTABLE;
			tmp3 = bitrng(instr, 11, 8); // Rm
			tmp2 = rpi2_reg_context.storage[tmp3]; // (Rm)
			if (tmp3 == 15) tmp2 += 8; // PC runs 2 words ahead
			tmp2 &= 0x1f; // we don't need shifts more than 31 bits
			stmp1 = (int) tmp1; // for '>>' to act as ASR instead of LSR
			tmp3 = (unsigned int) (stmp1 >> tmp2);
			break;
		case arm_cdata_lsl_r:
			// if d == 15 || n == 15 || m == 15 then UNPREDICTABLE;
			tmp3 = bitrng(instr, 11, 8); // Rm
			tmp2 = rpi2_reg_context.storage[tmp3]; // (Rm)
			if (tmp3 == 15) tmp2 += 8; // PC runs 2 words ahead
			if (tmp2 > 31) // to get rid of warning about too long shift
			{
				tmp3 = 0;
			}
			else
			{
				tmp2 &= 0x1f; // we don't need shifts more than 31 bits
				tmp3 = tmp1 << tmp2;
			}
			break;
		case arm_cdata_lsr_r:
			// if d == 15 || n == 15 || m == 15 then UNPREDICTABLE;
			tmp3 = bitrng(instr, 11, 8); // Rm
			tmp2 = rpi2_reg_context.storage[tmp3]; // (Rm)
			if (tmp3 == 15) tmp2 += 8; // PC runs 2 words ahead
			if (tmp2 > 31) // to get rid of warning about too long shift
			{
				tmp3 = 0;
			}
			else
			{
				tmp2 &= 0x1f; // we don't need shifts more than 31 bits
				tmp3 = tmp1 >> tmp2;
			}
			break;
		case arm_cdata_ror_r:
			// if d == 15 || n == 15 || m == 15 then UNPREDICTABLE;
			tmp3 = bitrng(instr, 11, 8); // Rm
			tmp2 = rpi2_reg_context.storage[tmp3]; // (Rm)
			if (tmp3 == 15) tmp2 += 8; // PC runs 2 words ahead
			tmp2 &= 0x1f; // we don't need shifts more than 31 bits
			if (tmp2 == 0) // to get rid of warning about too long shift
			{
				tmp3 = tmp1;
			}
			else
			{
				tmp4 = bitrng(tmp1, tmp2, 0); // catch the dropping bits
				tmp4 <<= (32 - tmp2); // prepare for putting back in the top
				tmp3 = tmp1 >> tmp2;
				tmp2 = (~0) << tmp2; // make mask
				tmp3 = (tmp3 & (~tmp2)) || (tmp4 & tmp2); // add dropped bits into result
			}
			break;
		default:
			// shouldn't get here
			break;
		}
	} // if Rd = PC
	// check for UNPREDICTABLE and UNDEFINED
	switch (extra)
	{
	// none of these if Rd != PC
	case arm_ret_asr_imm:
	case arm_ret_lsr_imm:
	case arm_ret_lsl_imm:
	case arm_ret_ror_imm:
	case arm_ret_rrx_pc:
	case arm_ret_mov_pc:
		tmp1 = rpi2_reg_context.reg.cpsr;
		if (!bit(instr, 20)) // is not 's' (= return from exception)
		{
			// normal jump
			if (tmp3 & 1) retval = set_thumb_addr(tmp3);
			else if (!(tmp3 & 3)) retval = set_arm_addr(tmp3);
			else
			{
				retval = set_thumb_addr(tmp3);
				retval = set_unpred_addr(retval);
			}
			break;
		}
		// return from exception
		// if CurrentModeIsHyp() then UNDEFINED;
		// if CurrentModeIsUserOrSystem() then UNPREDICTABLE;
		// if executed in Debug state then UNPREDICTABLE
		// TODO: check other state restrictions too
		// UNPREDICTABLE due to privilege violation might cause
		// UNDEFINED or SVC exception. Let's guess SVC for now.
		switch (tmp1 & 0x1f) // current mode
		{
		case 0x10: // usr
		case 0x1f: // sys
			retval = set_arm_addr(0x8); // SVC-vector
			retval = set_unpred_addr(retval);
			break;
		case 0x1a: // hyp
			retval = set_undef_addr();
			break;
		default:
			// retval = set_arm_addr(tmp3);
			tmp1 = rpi2_reg_context.reg.spsr;
			if (bit(tmp1, 5)) // 'T'-bit in SPSR
			{
				retval = set_thumb_addr(tmp3);
			}
			break;
		}
		break;
	// here Rd may be PC, but no return from exception
	case arm_cdata_asr_r:
	case arm_cdata_lsl_r:
	case arm_cdata_lsr_r:
	case arm_cdata_ror_r:
		// if d == 15 || n == 15 || m == 15 then UNPREDICTABLE;
		if (bitrng(instr, 15, 12) == 15) // Rd = PC
		{
			if (tmp3 & 1) retval = set_thumb_addr(tmp3);
			else if (!(tmp3 & 3)) retval = set_arm_addr(tmp3);
			else
			{
				retval = set_thumb_addr(tmp3);
			}
			retval = set_unpred_addr(retval);
		}
		else
		{
			retval = set_addr_lin();
		}
		break;
	default:
		retval = set_addr_lin();
		break;
	}
	return retval;
}

instr_next_addr_t arm_core_data_std_r(unsigned int instr, ARM_decode_extra_t extra)
{
	// Rd = bits 15-12, Rn = bits 19-16, Rm = bits 3-0
	// imm = bits 11-7, shift = bits 6-5
	// TODO: add check for T-bit, return thumb address if set (CPSR)
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4;
	int stmp1;

	retval = set_undef_addr();

	// These don't have Rd
	if ((extra == arm_cdata_cmn_r) || (extra == arm_cdata_cmp_r)
			|| (extra == arm_cdata_teq_r) || (extra == arm_cdata_tst_r))
	{
		// these don't affect program flow
		retval = set_addr_lin();
	}
	else if (bitrng(instr, 15, 12) == 15) // Rd = PC
	{
		tmp3 = bitrng(instr, 19, 16); // Rn
		tmp1 = rpi2_reg_context.storage[tmp3]; // (Rn)
		if (tmp3 == 15) tmp1 += 8; // PC runs 2 words ahead
		// calculate operand2
		tmp3 = bitrng(instr, 3, 0); // Rm
		tmp2 = rpi2_reg_context.storage[tmp3]; // (Rm)
		if (tmp3 == 15) tmp2 += 8; // PC runs 2 words ahead
		tmp3 = bitrng(instr, 11, 7); // shift-immediate
		switch (bitrng(instr, 6, 5))
		{
		case 0: // LSL
			tmp2 <<= tmp3;
			break;
		case 1: // LSR
			if (tmp3 == 0)
				tmp2 = 0;
			else
				tmp2 >>= tmp3;
			break;
		case 2: // ASR
			if (tmp3 == 0) tmp3 = 31;
			stmp1 = tmp2; // for ASR instead of LSR
			tmp2 = (unsigned int)(stmp1 >> tmp3);
			break;
		case 3: // ROR
			if (tmp3 == 0)
			{
				// RRX
				tmp3 = bit(rpi2_reg_context.reg.cpsr, 29); // carry-flag
				tmp2 = (tmp2 >> 1) | (tmp3 << 31);
			}
			else
			{
				tmp4 = tmp2;
				tmp4 <<= (32 - tmp3); // prepare for putting back in the top
				tmp2 = tmp2 >> tmp3;
				tmp1 = (~0) << tmp3; // make mask
				tmp2 = (tmp2 & (~tmp1)) || (tmp4 & tmp1); // add dropped bits into result
			}
			break;
		default:
			// shouldn't get here
			break;
		}

		switch (extra)
		{
		case arm_cdata_adc_r:
		case arm_ret_adc_r:
			tmp3 = tmp1 + tmp2 + bit(rpi2_reg_context.reg.cpsr, 29);
			retval = set_arm_addr(tmp3);
			break;
		case arm_cdata_add_r:
		case arm_cdata_add_r_sp:
		case arm_ret_add_r:
			retval = set_arm_addr(tmp1 + tmp2);
			break;
		case arm_cdata_and_r:
		case arm_ret_and_r:
			retval = set_arm_addr(tmp1 & tmp2);
			break;
		case arm_cdata_bic_r:
		case arm_ret_bic_r:
			retval = set_arm_addr(tmp1 & (~tmp2));
			break;
		case arm_cdata_eor_r:
		case arm_ret_eor_r:
			retval = set_arm_addr(tmp1 ^ tmp2);
			break;
		case arm_cdata_mvn_r:
		case arm_ret_mvn_r:
			retval = set_arm_addr(~tmp2);
			break;
		case arm_cdata_orr_r:
		case arm_ret_orr_r:
			retval = set_arm_addr(tmp1 & tmp2);
			break;
		case arm_cdata_rsb_r:
		case arm_ret_rsb_r:
			retval = set_arm_addr(tmp2 - tmp1);
			break;
		case arm_cdata_rsc_r:
		case arm_ret_rsc_r:
			tmp3 = bit(rpi2_reg_context.reg.cpsr, 29);
			retval = set_arm_addr((~tmp1) + tmp2 + tmp3);
			break;
		case arm_cdata_sbc_r:
		case arm_ret_sbc_r:
			tmp3 = bit(rpi2_reg_context.reg.cpsr, 29);
			retval = set_arm_addr(tmp1 + (~tmp2) + tmp3);
			break;
		case arm_cdata_sub_r:
		case arm_cdata_sub_r_sp:
		case arm_ret_sub_r:
			retval = set_arm_addr(tmp1 - tmp2);
			break;
		default:
			// shouldn't get here
			break;
		}

		// check for UNPREDICTABLE and UNDEFINED
		switch (extra)
		{
		case arm_ret_adc_r:
		case arm_ret_add_r:
		case arm_ret_and_r:
		case arm_ret_bic_r:
		case arm_ret_eor_r:
		case arm_ret_mvn_r:
		case arm_ret_orr_r:
		case arm_ret_rsb_r:
		case arm_ret_rsc_r:
		case arm_ret_sbc_r:
		case arm_ret_sub_r:
			tmp1 = rpi2_reg_context.reg.cpsr;
			if (!(bit(instr, 20)))
			{
				// not return from exception
				tmp3 = retval.address;
				if (tmp3 & 1) retval = set_thumb_addr(tmp3);
				else if (!(tmp3 & 3)) retval = set_arm_addr(tmp3);
				else
				{
					retval = set_thumb_addr(tmp3);
					retval = set_unpred_addr(retval);
				}
				break;
			}
			// if CurrentModeIsHyp() then UNDEFINED;
			// if CurrentModeIsUserOrSystem() then UNPREDICTABLE;
			// if executed in Debug state then UNPREDICTABLE
			// UNPREDICTABLE due to privilege violation might cause
			// UNDEFINED or SVC exception. Let's guess SVC for now.
			// TODO: check other stare restrictions too
			switch (tmp1 & 0x1f) // current mode
			{
			case 0x10: // usr
			case 0x1f: // sys
				retval = set_arm_addr(0x8);
				retval = set_unpred_addr(retval); // SVC-vector
				break;
			case 0x1a: // hyp
				retval = set_undef_addr();
				break;
			default:
				tmp1 = rpi2_reg_context.reg.spsr;
				if (bit(tmp1, 5)) // 'T'-bit in SPSR
				{
					tmp3 = retval.address;
					retval = set_thumb_addr(tmp3);
				}
				break;
			}
		}
	}
	else
	{
		retval = set_addr_lin();
	}
	return retval;
}

instr_next_addr_t arm_core_data_std_sh(unsigned int instr, ARM_decode_extra_t extra)
{
	// if d == 15 || n == 15 || m == 15 || s == 15 then UNPREDICTABLE
	// Rd = bits 15-12, Rn = bits 19-16, Rm = bits 3-0
	// Rs = bits 11-8, shift = bits 6-5
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4;
	int stmp1;

	retval = set_undef_addr();

	// These don't have Rd
	if ((extra == arm_cdata_cmn_rshr) || (extra == arm_cdata_cmp_rshr)
			|| (extra == arm_cdata_teq_rshr) || (extra == arm_cdata_tst_rshr))
	{
		// these don't affect program flow
		retval = set_addr_lin();
	}
	else if (bitrng(instr, 15, 12) == 15) // Rd = PC
	{
		tmp4 = bitrng(instr, 19, 16); // Rn
		tmp1 = rpi2_reg_context.storage[tmp4]; // (Rn)
		if (tmp4 == 15) tmp1 += 8; // PC runs 2 words ahead

		// calculate operand2
		tmp4 = bitrng(instr, 3, 0); // Rm
		tmp2 = rpi2_reg_context.storage[tmp4]; // (Rm)
		if (tmp4 == 15) tmp2 += 8; // PC runs 2 words ahead
		tmp4 = bitrng(instr, 11, 8); // Rs
		tmp3 = rpi2_reg_context.storage[tmp4]; // (Rs)
		if (tmp4 == 15) tmp3 += 8; // PC runs 2 words ahead
		tmp3 &= 0x1f; // we don't need any longer shifts here

		if (tmp3 != 0) // with zero shift count there's nothing to do
		{
			switch (bitrng(instr, 6, 5))
			{
			case 0: // LSL
				tmp2 <<= tmp3;
				break;
			case 1: // LSR
				tmp2 >>= tmp3;
				break;
			case 2: // ASR
				stmp1 = tmp2; // for ASR instead of LSR
				tmp2 = (unsigned int)(stmp1 >> tmp3);
				break;
			case 3: // ROR
				tmp4 = tmp2;
				tmp4 <<= (32 - tmp3); // prepare for putting back in the top
				tmp2 = tmp2 >> tmp3;
				tmp1 = (~0) << tmp3; // make mask
				tmp2 = (tmp2 & (~tmp1)) || (tmp4 & tmp1); // add dropped bits into result
				break;
			default:
				// shouldn't get here
				break;
			}
		}

		switch (extra)
		{
	  	case arm_cdata_adc_rshr:
			tmp3 = tmp1 + tmp2 + bit(rpi2_reg_context.reg.cpsr, 29);
			retval = set_arm_addr(tmp3);
			break;
		case arm_cdata_add_rshr:
			retval = set_arm_addr(tmp1 + tmp2);
			break;
		case arm_cdata_and_rshr:
			retval = set_arm_addr(tmp1 & tmp2);
			break;
		case arm_cdata_bic_rshr:
			retval = set_arm_addr(tmp1 & (~tmp2));
			break;
		case arm_cdata_eor_rshr:
			retval = set_arm_addr(tmp1 ^ tmp2);
			break;
		case arm_cdata_mvn_rshr:
			retval = set_arm_addr(~tmp2);
			break;
		case arm_cdata_orr_rshr:
			retval = set_arm_addr(tmp1 & tmp2);
			break;
		case arm_cdata_rsb_rshr:
			retval = set_arm_addr(tmp2 - tmp1);
			break;
		case arm_cdata_rsc_rshr:
			tmp3 = bit(rpi2_reg_context.reg.cpsr, 29);
			retval = set_arm_addr((~tmp1) + tmp2 + tmp3);
			break;
		case arm_cdata_sbc_rshr:
			tmp3 = bit(rpi2_reg_context.reg.cpsr, 29);
			retval = set_arm_addr(tmp1 + (~tmp2) + tmp3);
			break;
		case arm_cdata_sub_rshr:
			retval = set_arm_addr(tmp1 - tmp2);
			break;
		default:
			// shouldn't get here
			break;
		}
		// due to Rd = PC
		retval = set_unpred_addr(retval);
	}
	else
	{
		retval = set_addr_lin();
	}
	return retval;
}

instr_next_addr_t arm_core_data_std_i(unsigned int instr, ARM_decode_extra_t extra)
{
	// if d == 15 || n == 15 || m == 15 || s == 15 then UNPREDICTABLE
	// Rd = bits 15-12, Rn = bits 19-16, imm = bits 11-0
	// Rs = bits 11-8, shift = bits 6-5
	// TODO: add check for T-bit, return thumb address if set (CPSR)
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4;

	retval = set_undef_addr();

	// These don't have Rd
	if ((extra == arm_cdata_cmn_imm) || (extra == arm_cdata_cmp_imm)
			|| (extra == arm_cdata_teq_imm) || (extra == arm_cdata_tst_imm))
	{
		// these don't affect program flow
		retval = set_addr_lin();
	}
	else if (bitrng(instr, 15, 12) == 15) // Rd = PC
	{
		tmp4 = bitrng(instr, 19, 16); // Rn
		tmp1 = rpi2_reg_context.storage[tmp4]; // (Rn)
		if (tmp4 == 15) tmp1 += 8; // PC runs 2 words ahead

		// calculate operand2
		// imm12: bits 11-8 = half of ror amount, bits 7-0 = immediate value
		tmp2 = bitrng(instr, 7, 0); // immediate
		tmp3 = bitrng(instr, 11, 8) << 1; // ROR-amount
		tmp3 &= 0x1f;
		if (tmp3 != 0) // with zero shift count there's nothing to do
		{
			// always ROR
			tmp4 = tmp2;
			tmp4 <<= (32 - tmp3); // prepare for putting back in the top
			tmp2 = tmp2 >> tmp3;
			tmp1 = (~0) << tmp3; // make mask
			tmp2 = (tmp2 & (~tmp1)) || (tmp4 & tmp1); // add dropped bits into result
		}

		switch (extra)
		{
		case arm_cdata_adc_imm:
		case arm_ret_adc_imm:
			tmp3 = tmp1 + tmp2 + bit(rpi2_reg_context.reg.cpsr, 29);
			retval = set_arm_addr(tmp3);
			break;
		case arm_cdata_add_imm:
		case arm_cdata_add_imm_sp:
		case arm_ret_add_imm:
			retval = set_arm_addr(tmp1 + tmp2);
			break;
		case arm_cdata_adr_lbla:
			// result = (Align(PC,4) + imm32);
			// Align(x, y) = y * (x DIV y)
			tmp3 = rpi2_reg_context.reg.r15 & ((~0) << 2);
			retval = set_arm_addr(tmp3 + tmp2);
			break;
		case arm_cdata_adr_lblb:
			// result = (Align(PC,4) - imm32);
			tmp3 = rpi2_reg_context.reg.r15 & ((~0) << 2);
			retval = set_arm_addr(tmp3 - tmp2);
			break;
		case arm_cdata_and_imm:
			retval = set_arm_addr(tmp1 & tmp2);
			break;
		case arm_cdata_bic_imm:
		case arm_ret_bic_imm:
			retval = set_arm_addr(tmp1 & (~tmp2));
			break;
		case arm_cdata_eor_imm:
		case arm_ret_eor_imm:
			retval = set_arm_addr(tmp1 ^ tmp2);
			break;
		case arm_cdata_mov_imm:
		case arm_ret_mov_imm:
			retval = set_arm_addr(tmp2);
			break;
		case arm_cdata_mvn_imm:
		case arm_ret_mvn_imm:
			retval = set_arm_addr(~tmp2);
			break;
		case arm_cdata_orr_imm:
			retval = set_arm_addr(tmp1 & tmp2);
			break;
		case arm_cdata_rsb_imm:
		case arm_ret_rsb_imm:
			retval = set_arm_addr(tmp2 - tmp1);
			break;
		case arm_cdata_rsc_imm:
		case arm_ret_rsc_imm:
			tmp3 = bit(rpi2_reg_context.reg.cpsr, 29);
			retval = set_arm_addr((~tmp1) + tmp2 + tmp3);
			break;
		case arm_cdata_sbc_imm:
		case arm_ret_sbc_imm:
			tmp3 = bit(rpi2_reg_context.reg.cpsr, 29);
			retval = set_arm_addr(tmp1 + (~tmp2) + tmp3);
			break;
		case arm_cdata_sub_imm:
		case arm_cdata_sub_imm_sp:
		case arm_ret_sub_imm:
			retval = set_arm_addr(tmp1 - tmp2);
			break;
		default:
			// shouldn't get here
			break;
		}

		// check for UNPREDICTABLE and UNDEFINED
		switch (extra)
		{
		case arm_ret_adc_imm:
		case arm_ret_add_imm:
		case arm_ret_bic_imm:
		case arm_ret_eor_imm:
		case arm_ret_mov_imm:
		case arm_ret_mvn_imm:
		case arm_ret_rsb_imm:
		case arm_ret_rsc_imm:
		case arm_ret_sbc_imm:
		case arm_ret_sub_imm:
			tmp1 = rpi2_reg_context.reg.cpsr;
			if (!(bit(instr, 20)))
			{
				// not return from exception
				tmp3 = retval.address;
				if (tmp3 & 1) retval = set_thumb_addr(tmp3);
				else if (!(tmp3 & 3)) retval = set_arm_addr(tmp3);
				else
				{
					retval = set_thumb_addr(tmp3);
					retval = set_unpred_addr(retval);
				}
				break;
			}
			// if CurrentModeIsHyp() then UNDEFINED;
			// if CurrentModeIsUserOrSystem() then UNPREDICTABLE;
			// if executed in Debug state then UNPREDICTABLE
			// UNPREDICTABLE due to privilege violation might cause
			// UNDEFINED or SVC exception. Let's guess SVC for now.
			// TODO: check other stare restrictions too
			switch (tmp1 & 0x1f) // current mode
			{
			case 0x10: // usr
			case 0x1f: // sys
				retval = set_arm_addr(0x8); // SVC-vector
				retval = set_unpred_addr(retval);
				break;
			case 0x1a: // hyp
				retval = set_undef_addr();
				break;
			default:
				tmp1 = rpi2_reg_context.reg.spsr;
				if (bit(tmp1, 5)) // 'T'-bit in SPSR
				{
					tmp3 = retval.address;
					retval = set_thumb_addr(tmp3);
				}
				break;
			}
		}
	}
	else
	{
		retval = set_addr_lin();
	}
	return retval;
}

// here we take some shortcuts. We assume ARM or Thumb instruction set
// and we don't go further into more complicated modes, like hyp, debug
// or secure monitor
// TODO: check what the PC value could be
// TODO: add check for T-bit, return thumb address if set (CPSR/SPSR)
// See execution of exception handler as the functionality of SW exception
// instruction - consider the program flow as linear
instr_next_addr_t arm_core_exc(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3;

	retval = set_undef_addr();

	switch (extra)
	{
	case arm_exc_eret:
		if (check_proc_mode(INSTR_PMODE_HYP, 0, 0, 0))
		{
			retval = set_arm_addr(get_elr_hyp());
			retval = set_unpred_addr(retval); // we don't support hyp
		}
		else if (check_proc_mode(INSTR_PMODE_USR, INSTR_PMODE_SYS, 0, 0))
		{
			retval = set_undef_addr();
		}
		else
		{
			retval = set_arm_addr(rpi2_reg_context.reg.r14); // lr
		}
		break;
	case arm_exc_bkpt:
		// if user code contains bkpt, address from low vector?
		// maybe not - single-stepping and suddenly ending a debugging session
		// might leave breakpoints in rpi_stub code
		// better play linear...
		// retval = set_arm_addr(0x0c); // PABT
		//retval = set_unpred_addr(retval); // may cause oddities
		retval = set_addr_lin();
		break;
	case arm_exc_hvc:
		// UNPREDICTABLE in Debug state.
		if (get_security_state())
		{
			retval = set_undef_addr();
		}
		else if(check_proc_mode(INSTR_PMODE_USR, 0, 0, 0))
		{
			retval = set_undef_addr();
		}
		else
		{
			if (!(get_SCR() & (1 <<8))) // HVC disabled
			{
				if (check_proc_mode(INSTR_PMODE_USR, 0, 0, 0))
				{
					//retval = set_arm_addr(0x14); // HVC-vector
					//retval = set_unpred_addr(retval);
					retval = set_addr_lin();
				}
				else
					retval = set_undef_addr();
			}
			else
			{
				//retval = set_arm_addr(0x14); // HVC-vector
				retval = set_addr_lin();
			}
		}
		break;
	case arm_exc_smc:
		if (check_proc_mode(INSTR_PMODE_USR, 0, 0, 0))
		{
			retval = set_undef_addr();
		}
		else if ((get_HCR() & (1 << 19)) && (!get_security_state())) // HCR.TSC
		{
				//retval = set_arm_addr(0x14); // hyp trap
				//retval = set_unpred_addr(retval); // we don't support hyp
				retval = set_addr_lin();
		}
		else if (get_SCR() & (1 << 7)) // SCR.SCD
		{
			if (!get_security_state()) // non-secure
			{
				retval = set_undef_addr();
			}
			else
			{
				//retval = set_arm_addr(0x08); // smc-vector?
				//retval = set_unpred_addr(retval);
				retval = set_addr_lin();
			}
		}
		else
		{
			//retval = set_arm_addr(0x08); // smc-vector?
			retval = set_addr_lin();
		}
		break;
	case arm_exc_svc:
		if (get_HCR() & (1 << 27)) // HCR.TGE
		{
			if ((!get_security_state()) // non-secure
					&& check_proc_mode(INSTR_PMODE_USR, 0, 0, 0))
			{
				//retval = set_arm_addr(0x14); // hyp trap
				//retval = set_unpred_addr(retval); // we don't support hyp
				retval = set_addr_lin();
			}
			else
			{
				//retval = set_arm_addr(0x08); // svc-vector
				retval = set_addr_lin();
			}
		}
		else
		{
			//retval = set_arm_addr(0x08); // svc-vector
			retval = set_addr_lin();
		}
		break;
	case arm_exc_udf:
		retval = set_undef_addr();
		break;
	case arm_exc_rfe:
		if (check_proc_mode(INSTR_PMODE_HYP, 0, 0, 0))
			retval = set_undef_addr();
		else
		{
			// bit 24 = P, 23 = Y, 22 = W
			// bits 19-16 = Rn
			// if n == 15 then UNPREDICTABLE
			tmp1 = bitrng(instr, 19, 16); // Rn
			tmp2 = rpi2_reg_context.storage[tmp1]; // (Rn)
			tmp3 = bits(instr, 0x01800000); // P and U
			// wback = (W == '1'); increment = (U == '1');
			// wordhigher = (P == U);
			// address = if increment then R[n] else R[n]-8;
			// if wordhigher then address = address+4;
			// new_pc_value = MemA[address,4];
			// spsr_value = MemA[address+4,4];
			switch (tmp3)
			{
			case 0: // DA
				// wordhigher
				tmp3 = *((unsigned int *)(tmp2 - 4));
				break;
			case 1: // IA
				// increment
				tmp3 = *((unsigned int *)tmp2);
				break;
			case 2: // DB
				tmp2 -= 4;
				tmp3 = *((unsigned int *)(tmp2 - 8));
				break;
			case 3: // IB
				// increment, wordhigher
				tmp2 += 4;
				tmp3 = *((unsigned int *)(tmp2 + 4));
				break;
			default:
				// shouldn't get here
				break;
			}

			retval = set_arm_addr(tmp3);

			// if encoding is Thumb or ARM, how the heck instruction set
			// can be ThumbEE?
			if (check_proc_mode(INSTR_PMODE_USR, 0, 0, 0)
					&& (rpi2_reg_context.reg.cpsr & 0x01000020 == 0x01000020)) // ThumbEE
			{
				retval = set_unpred_addr(retval);
			}
		}
		break;
	case arm_exc_srs:
		if (check_proc_mode(INSTR_PMODE_HYP, 0, 0, 0))
		{
			retval = set_undef_addr();
		}
		else
		{
			// doesn't affect program flow (?)
			retval = set_addr_lin();
			if (check_proc_mode(INSTR_PMODE_USR, INSTR_PMODE_SYS, 0, 0))
			{
				retval = set_unpred_addr(retval);
			}
			else if (bitrng(instr, 4, 0) == INSTR_PMODE_HYP)
			{
				retval = set_unpred_addr(retval);
			}
			else if (check_proc_mode(INSTR_PMODE_MON, 0, 0, 0)
					&& (!get_security_state()))
			{
				retval = set_unpred_addr(retval);
			}
			else if (check_proc_mode(INSTR_PMODE_FIQ, 0, 0, 0)
					&& check_coproc_access(16)
					&& (!get_security_state()))
			{
				retval = set_unpred_addr(retval);
			}
		}
		break;
	default:
		// shouldn't get here
		break;
	}
	return retval;
}

instr_next_addr_t arm_core_ldst(unsigned int instr, ARM_decode_extra_t extra)
{
	// Rn = bits 19-16, Rt = bits 15-12, Rm= bits 3-0
	// imm = bits 11-0, shiftcount = bits 11-7, shift type = bits 6-5
	// P = bit 24, U = bit 23, B = bit 22, W = bit 21, L = bit 20
	// register
	// if m == 15 then UNPREDICTABLE;
	// if wback && (n == 15 || n == t) then UNPREDICTABLE;
	// immediate
	// if wback && n == t then UNPREDICTABLE
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4;
	int stmp1; // for making '>>' to work like ASR
	int unp = 0;

	(void) extra;

	retval = set_undef_addr();

	tmp1 = bitrng(instr, 19, 16); // Rn
	tmp2 = bitrng(instr, 15, 12); // Rt
	tmp3 = (bit(instr, 21)); // W

	if (tmp3 && (tmp1 == tmp2)) unp++; // wback && n = t

	if ((tmp1 == 15) || (tmp2 == 15)) // if Rn or Rt is PC
	{
		// operand2
		if (bit(instr, 25)) // reg or imm
		{
			// register
			if (tmp3 && (tmp2 == 15)) unp++; // reg, wback && n = 15

			tmp1 = bitrng(instr, 3, 0); // Rm
			if (tmp1 == 15) unp++; // m = 15

			// calculate operand2
			tmp2 = rpi2_reg_context.storage[tmp1]; // Rm
			tmp3 = bitrng(instr, 11, 7); // shift-immediate
			switch (bitrng(instr, 6, 5))
			{
			case 0: // LSL
				tmp2 <<= tmp3;
				break;
			case 1: // LSR
				if (tmp3 == 0)
					tmp2 = 0;
				else
					tmp2 >>= tmp3;
				break;
			case 2: // ASR
				if (tmp3 == 0) tmp3 = 31;
				stmp1 = tmp2; // for ASR instead of LSR
				tmp2 = (unsigned int)(stmp1 >> tmp3);
				break;
			case 3: // ROR
				if (tmp3 == 0)
				{
					// RRX
					tmp3 = rpi2_reg_context.reg.cpsr;
					tmp3 = bit(tmp3, 29); // carry-flag
					tmp2 = (tmp2 >> 1) | (tmp3 << 31);
				}
				else
				{
					tmp4 = tmp2;
					tmp4 <<= (32 - tmp3); // prepare for putting back in the top
					tmp2 = tmp2 >> tmp3;
					tmp1 = (~0) << tmp3; // make mask
					tmp2 = (tmp2 & (~tmp1)) || (tmp4 & tmp1); // add dropped bits into result
				}
				break;
			default:
				// shouldn't get here
				break;
			}
		}
		else
		{
			// immediate
			tmp2 = bitrng(instr, 11, 0);
		}
		// now offset in tmp2

		tmp3 = bitrng(instr, 19, 16); // Rn
		tmp1 = bitrng(instr, 15, 12); // Rt

		// P = 0 always writeback, when W = 0: normal, W = 1: user mode
		switch (bits(instr, 0x01200000)) // P and W
		{
		case 0: // postindexing
			// rt = (Rn); rn = rn + offset
			if (tmp3 == 15) // Rn
			{
				tmp4 = rpi2_reg_context.storage[tmp3];
				// final Rn is returned
				if (bit(instr, 23)) // U-bit
				{
					tmp4 += tmp2;
				}
				else
				{
					tmp4 -= tmp2;
				}
			}
			else
			{
				// U, B, L
				// Rt is returned
				if (bit(instr, 20)) // L-bit
				{
					tmp3 = rpi2_reg_context.storage[tmp3];
					if (bit(instr, 22)) // B-bit
					{
						// byte
						tmp4 = (unsigned int)(*((unsigned char *)(tmp3)));
					}
					else
					{
						// word
						tmp4 = (*((unsigned int *)(tmp3)));
					}
				}
				else
				{
					// store doesn't change the register contents
					tmp4 = rpi2_reg_context.storage[tmp1];
				}
			}
			retval = set_arm_addr(tmp4);
			break;
		case 1: // usermode access, offset
		case 2: // offset
			// rt = (Rn + offset)
			if (tmp1 == 15) // if Rt = PC
			{
				if (bit(instr, 20)) // L-bit
				{
					if (bit(instr, 23)) // U-bit
					{
						tmp3 = rpi2_reg_context.storage[tmp3] + tmp2;
						if (bit(instr, 22)) // B-bit
						{
							// byte
							tmp4 = (unsigned int)(*((unsigned char *)(tmp3)));
						}
						else
						{
							// word
							tmp4 = (*((unsigned int *)(tmp3)));
						}
					}
					else
					{
						tmp3 = rpi2_reg_context.storage[tmp3] - tmp2;
						if (bit(instr, 22)) // B-bit
						{
							// byte
							tmp4 = (unsigned int)(*((unsigned char *)(tmp3)));
						}
						else
						{
							// word
							tmp4 = (*((unsigned int *)(tmp3)));
						}
					}
				}
				else
				{
					// store doesn't change the register contents
					tmp4 = rpi2_reg_context.storage[tmp1];
				}
				retval = set_arm_addr(tmp4);
			}
			else
			{
				retval = set_addr_lin();
			}
			break;
		case 3: // preindexing
			// rn = rn + offset; rt = (Rn)
			// address
			if (bit(instr, 23)) // U-bit
			{
				tmp3 = rpi2_reg_context.storage[tmp3] + tmp2;
			}
			else
			{
				tmp3 = rpi2_reg_context.storage[tmp3] - tmp2;
			}
			// set up return value - updated if load
			if (tmp3 == 15) // if Rn = PC
			{
				tmp4 = tmp3;
			}
			else
			{
				tmp4 = rpi2_reg_context.storage[tmp1];
			}

			// load/store
			if (tmp1 == 15) // if Rt = PC
			{
				if (bit(instr, 20)) // L-bit
				{
						if (bit(instr, 22)) // B-bit
						{
							// byte
							tmp4 = (unsigned int)(*((unsigned char *)(tmp3)));
						}
						else
						{
							// word
							tmp4 = (*((unsigned int *)(tmp3)));
						}
				}
				// else return value already in tmp4
				retval = set_arm_addr(tmp4);
			}
			if (tmp4 == rpi2_reg_context.storage[tmp1])
			{
				// The instruction didn't affect program flow
				retval = set_addr_lin();
			}
			else
			{
				retval = set_arm_addr(tmp4);
			}
			break;
		default:
			// shouldn't get here
			break;
		}
	}
	else
	{
		retval = set_addr_lin();
	}
	if (retval.address != 0xffffffff) // not lin
	{
		tmp1 = retval.address;
		if (bit(tmp1,0)) retval = set_thumb_addr(tmp1);
		else if ((tmp1 & 3) == 0) retval = set_arm_addr(tmp1);
		else
		{
			retval = set_thumb_addr(tmp1);
			unp++;
		}
	}
	if (unp) retval = set_unpred_addr(retval);
	return retval;
}

instr_next_addr_t arm_core_ldstm(unsigned int instr, ARM_decode_extra_t extra)
{
	// if n == 15 || BitCount(registers) < 1 then UNPREDICTABLE;
	// if wback && registers<n> == '1' && ArchVersion() >= 7 then UNPREDICTABLE;
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4;

	retval = set_undef_addr();

	if (extra == arm_cldstm_pop_r)
	{
		// pop one register from stack, Rt = bits 15-12
		// if t == 13 then UNPREDICTABLE
		// LDM{<c>}{<q>} SP!, <registers>
		if (bitrng(instr, 15, 12) == 15) // pop PC
		{
			tmp1 = rpi2_reg_context.reg.r13; // SP
			tmp2 = *((unsigned int *)tmp1);
			retval = set_arm_addr(tmp2);
		}
		else
		{
			// The instruction doesn't affect program flow
			retval = set_addr_lin();
			if (bitrng(instr, 15, 12) == 13) // pop SP
			{
				retval = set_unpred_addr(retval);
			}
		}
	}
	else if (extra == arm_cldstm_push_r)
	{
		// push one register to stack
		// The instruction doesn't affect program flow
		retval = set_addr_lin();
		if (bitrng(instr, 15, 12) == 13) // push SP
		{
			retval = set_unpred_addr(retval);
		}
	}
	else
	{
		/*
		 * bits 24 - 20: B I M W L
		 * B 1=before, 0=after
		 * I 1=increment, 0=decrement
		 * M 1=other mode, 0=current mode
		 * W 1=writeback, 0=no writeback
		 * L 1=load, 0=store
		 */
		tmp1 = bitrng(instr, 19, 16); // base register

		// bit count in register-list
		tmp4 = 0;
		for (tmp3 = 0; tmp3 < 16; tmp3++)
		{
			if (instr & (1 << tmp3)) tmp4++;
		}

		// if L == 0 || (M == 1 && bit 15 == 0) - stm or ldm-usr
		if ((bit(instr, 20) == 0) || (bit(instr, 22) && (bit(instr, 15) == 0)))
		{
			if ((tmp1 == 15) && bit(instr, 21)) // ((n == 15) && W)
			{
				// new n - B-bit doesn't make a difference
				if (bit(instr, 23)) // I-bit
				{
					tmp3 = rpi2_reg_context.reg.r15 + 4 * tmp4;
				}
				else
				{
					tmp3 = rpi2_reg_context.reg.r15 - 4 * tmp4;
				}
				retval = set_arm_addr(tmp3);
			}
			else
			{
				// no effect on program flow - store or user registers
				retval = set_addr_lin();
			}

			if (tmp1 == 15)
			{
				retval = set_unpred_addr(retval);
			}
			if ((tmp1 == 13) && (bit(instr, 13) || tmp4 < 2))
			{
				retval = set_unpred_addr(retval);
			}
			if (bit(instr, 22))
			{
				if (check_proc_mode(INSTR_PMODE_USR, INSTR_PMODE_SYS, 0, 0))
				{
					// UNPREDICTABLE if mode is usr or sys
					retval = set_unpred_addr(retval);
				}
				if (check_proc_mode(INSTR_PMODE_HYP, 0, 0, 0))
				{
					// UNDEFINED if hyp mode
					retval = set_undef_addr();
				}
			}
		}
		else // (L==1 && (M==0 || bit 15 == 1)) - ldm or pop-ret
		{
			if ((tmp1 == 15) && bit(instr, 21)) // ((n == 15) && W)
			{
				// new n - B-bit doesn't make a difference
				// the writeback overwrites the popped value
				if (bit(instr, 23)) // I-bit
				{
					tmp3 = rpi2_reg_context.reg.r15 + 4 * tmp4;
				}
				else
				{
					tmp3 = rpi2_reg_context.reg.r15 - 4 * tmp4;
				}
				retval = set_arm_addr(tmp3);
			}
			else if (bit(instr, 15)) // bit 15 == 1
			{
				// new PC
				tmp3 = rpi2_reg_context.storage[tmp1]; // base register
				switch (bitrng(instr, 24, 23)) // B and I
				{
				case 0: // decrement after
					tmp3 -= 4 * (tmp4 - 1);
					break;
				case 1: // increment after
					tmp3 += 4 * (tmp4 - 1);
					break;
				case 2: // decrement before
					tmp3 -= 4 * tmp4;
					break;
				case 3: // increment before
					tmp3 += 4 * tmp4;
					break;
				default:
					// shouldn't get here
					break;
				}
				tmp3 = *((unsigned int *) tmp3);
				if (tmp3 & 1)
				{
					retval = set_thumb_addr(tmp3);
				}
				else if (!(tmp3 & 3))
				{
					retval = set_arm_addr(tmp3);
				}
				else
				{
					retval = set_thumb_addr(tmp3);
					retval = set_unpred_addr(retval);
				}
				// if L == 1 || M == 1 - ldm exception return
				if (bit(instr, 20) || bit(instr, 22))
				{
					tmp1 = rpi2_reg_context.reg.spsr;
					if (bit(tmp1, 5))
					{
						tmp3 = retval.address;
						retval = set_thumb_addr(tmp3);
					}
					else
					{
						tmp3 = (retval.address) & (~3);
						retval = set_arm_addr(tmp3);
					}
				}
			}
			else
			{
				retval = set_addr_lin();
			}

			if (tmp1 == 15)
			{
				retval = set_unpred_addr(retval);
			}
			if (tmp1 == 13 && (bit(instr, 13) || tmp4 < 2))
			{
				retval = set_unpred_addr(retval);
			}
			else if (tmp4 < 1)
			{
				retval = set_unpred_addr(retval);
			}
			if (bit(instr, 22) && bit(instr, 15)) // (M=1 && bit 15 == 1)
			{
				if (check_proc_mode(INSTR_PMODE_USR, INSTR_PMODE_SYS, 0, 0))
				{
					// UNPREDICTABLE if mode is usr or sys
					retval = set_unpred_addr(retval);
				}
				if (check_proc_mode(INSTR_PMODE_HYP, 0, 0, 0))
				{
					// UNDEFINED if hyp mode
					retval = set_undef_addr();
				}
			}
		}
	}
	return retval;
}

instr_next_addr_t arm_core_ldstrd(unsigned int instr, ARM_decode_extra_t extra)
{
	// bits 24 - 21: PUIW (I = immediate)
	// bit 5: 0=load, 1=store
	// if Rn == '1111' then SEE (literal);
	// if Rt<0> == '1' then UNPREDICTABLE;
	// t2 = t+1; imm32 = ZeroExtend(imm4H:imm4L, 32);
	// if P == '0' && W == '1' then UNPREDICTABLE;
	// if wback && (n == t || n == t2) then UNPREDICTABLE;
	// if t2 == 15 then UNPREDICTABLE;

	// reg: additional restrictions
	// if t2 == 15 || m == 15 || m == t || m == t2 then UNPREDICTABLE;
	// if wback && (n == 15 || n == t || n == t2) then UNPREDICTABLE;

	// Rn = bits 19-16, Rt = bits 15-12, Rm = bits 3-0
	// imm = bits 11-8 and bits 3-0
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4;
	int unp = 0; // unpredictable

	(void) extra;

	retval = set_undef_addr();

	tmp1 = bitrng(instr, 19, 16); // Rn
	tmp2 = bitrng(instr, 15, 12); // Rt
	tmp3 = bit(instr, 21); // W
	if (tmp2 & 1) unp++; // Rt<0> == '1'
	if (tmp1 == 14) unp++; // t2 == 15
	// wback && (n == t || n == t2)
	if (tmp3 & ((tmp1 == tmp2) || (tmp1 == tmp2 + 1))) unp++;
	// if Rn, Rt or Rt2 is PC
	if ((tmp1 == 14) || (tmp1 == 15) || (tmp2 == 15))
	{
		// operand2
		if (bit(instr, 22)) // reg or imm
		{
			// register
			if (tmp3 && (tmp1 == 15)) unp++; // reg, wback && n = 15
			tmp4 = bitrng(instr, 3, 0); // Rm
			if (tmp4 == 15) unp++; // m = 15
			// (m == t || m == t2)
			if ((tmp4 == tmp1) || (tmp4 == tmp1 + 1)) unp++;
			// calculate operand2
			tmp2 = rpi2_reg_context.storage[tmp4]; // Rm
		}
		else
		{
			// immediate
			tmp2 = bits(instr, 0x00000f0f);
		}
		// now offset in tmp2

		tmp4 = bitrng(instr, 15, 12); // Rt
		if (tmp1 == 15) // Rn is PC
		{
			tmp3 = rpi2_reg_context.storage[tmp1]; // (Rn)
			// bits 24 - 21: PUIW (I = immediate)
			// bit 5: 0=load, 1=store
			// P = 0 always writeback, when W = 0: normal, W = 1: user mode
			// address = (Align(PC,4) +/- imm32);
			switch (bits(instr, 0x01200000)) // P and W
			{
			case 0: // postindexing
				// rt = (Rn); rn = rn + offset
				// if rn == 15, PC will eventually have new rn
				tmp3 = (tmp3 & ((~0) << 2));
				if (bit(instr, 23)) // U-bit
					tmp3 += tmp2;
				else
					tmp3 -= tmp2;
				tmp3 = (tmp3 & ((~0) << 2));
				retval = set_arm_addr(tmp3);
				break;
			case 1: // user mode access: UNDEFINED
				retval = set_undef_addr();
				break;
			case 2: // offset
				// rt = (Rn + offset)
				if(bit(instr, 5)) // STR
				{
					// STR doesn't affect Rt or RT2
					retval = set_addr_lin();
				}
				else
				{
					// address
					tmp3 = (tmp3 & ((~0) << 2));
					if (bit(instr, 23)) // U-bit
						tmp3 += tmp2;
					else
						tmp3 -= tmp2;
					tmp3 = (tmp3 & ((~0) << 2));

					if (tmp4 == 15) // Rt is PC
					{
						tmp3 = *((unsigned int *) tmp3);
						retval = set_arm_addr(tmp3);
					}
					else if (tmp4 == 14) // Rt2 is PC
					{
						tmp3 = *((unsigned int *) (tmp3 + 4));
						retval = set_arm_addr(tmp3);
					}
					else
					{
						// PC wasn't affected
						retval = set_addr_lin();
					}
				}
				break;
			case 3: // preindexing
				// rn = rn + offset; rt = (Rn)
				// if rn == 15, PC will eventually have new rn
				tmp3 = (tmp3 & ((~0) << 2));
				if (bit(instr, 23)) // U-bit
					tmp3 += tmp2;
				else
					tmp3 -= tmp2;
				tmp3 = (tmp3 & ((~0) << 2));
				retval = set_arm_addr(tmp3);
				break;
			default:
				// shouldn't get here
				break;
			}
		}
		else
		{
			// Rn is not PC, but either Rt or Rt2 is
			tmp3 = rpi2_reg_context.storage[tmp1]; // (Rn)
			// STR doesn't affect Rt or RT2
			if (bit(instr, 5)) // LDR
			{
				// bits 24 - 21: PUIW (I = immediate)
				// bit 5: 0=load, 1=store
				// P = 0 always writeback, when W = 0: normal, W = 1: user mode
				switch (bits(instr, 0x01200000)) // P and W
				{
				case 0: // postindexing
					// rt = (Rn); rn = rn + offset
					// address
					tmp3 = (tmp3 & ((~0) << 2));

					if (tmp4 == 15) // Rt is PC
					{
						tmp3 = *((unsigned int *) tmp3);
						retval = set_arm_addr(tmp3);
					}
					else if (tmp4 == 14) // Rt2 is PC
					{
						tmp3 = *((unsigned int *) (tmp3 + 4));
						retval = set_arm_addr(tmp3);
					}
					else
					{
						// PC wasn't affected
						retval = set_addr_lin();
					}
					break;
				case 1: // user mode access: UNDEFINED
					retval = set_undef_addr();
					break;
				case 2: // offset
					// rt = (Rn + offset)
				case 3: // preindexing
					// rn = rn + offset; rt = (Rn)
					// address
					if (bit(instr, 23)) // U-bit
						tmp3 += tmp2;
					else
						tmp3 -= tmp2;
					tmp3 = (tmp3 & ((~0) << 2));

					if (tmp4 == 15) // Rt is PC
					{
						tmp3 = *((unsigned int *) tmp3);
						retval = set_arm_addr(tmp3);
					}
					else if (tmp4 == 14) // Rt2 is PC
					{
						tmp3 = *((unsigned int *) (tmp3 + 4));
						retval = set_arm_addr(tmp3);
					}
					else
					{
						// PC wasn't affected - shouldn't get here
						retval = set_addr_lin();
					}
					break;
				default:
					// shouldn't get here
					break;
				}
			}
			else
			{
				// STR
				if (bits(instr, 0x01200000) == 1) // user mode: UNDEFINED
				{
					retval = set_undef_addr();
				}
				else
				{
					// PC wasn't affected
					retval = set_addr_lin();
				}
			}
		}
	}
	else
	{
		// PC not involved
		retval = set_addr_lin();
	}

	// check if UNPREDICTABLE
	if (retval.flag != INSTR_ADDR_UNDEF)
	{
		if (unp)
		{
			retval = set_unpred_addr(retval);
		}
	}
	return retval;
}

instr_next_addr_t arm_core_ldstrex(unsigned int instr, ARM_decode_extra_t extra)
{
	// STR: Rn = bits 19-16, Rd (status) = bits 15-12, Rt = bits 3-0
	// LDR: Rn = bits 19-16, Rt = bits 15-12
	// bits 22 - 20: S H L
	// S = size: 0 = double, 1 = half word
	// H = half: 0 = size, 1 = half-size
	// L = load: 0 = store, 1 = load
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3;
	int unp = 0; // unpredictable

	retval = set_undef_addr();

	tmp1 = bitrng(instr, 19, 16); // Rn
	if (tmp1 == 15) unp++; // n == 15

	if (bit(instr, 20))
	{
		// load
		tmp2 = bitrng(instr, 15, 12); // Rt
		// if Rt == PC or Rt2 == PC
		if (tmp2 == 15 || ((tmp2 == 14) && (extra == arm_sync_ldrexd)))
		{
			unp++;
			tmp1 = rpi2_reg_context.storage[tmp1]; // (Rn)
			tmp1 &= (~0) << 2; // word aligned - also ldrexd
			switch (extra)
			{
			case arm_sync_ldrex:
				tmp3 = *((unsigned int *) tmp1);
				break;
			case arm_sync_ldrexb:
				tmp3 = (unsigned int)(*((unsigned char *) tmp1));
				break;
			case arm_sync_ldrexh:
				tmp3 = (unsigned int)(*((unsigned short *) tmp1));
				break;
			case arm_sync_ldrexd:
				if (tmp2 == 15)
				{
					tmp3 = *((unsigned int *) tmp1);
				}
				else
				{
					tmp3 = *((unsigned int *) (tmp1 + 4));

				}
				break;
			default:
				// shouldn't get here
				break;
			}
			retval = set_arm_addr(tmp3);
		}
		else
		{
			// doesn'r affect PC
			retval = set_addr_lin();
		}
	}
	else
	{
		// store
		// if d == 15 || t == 15 || n == 15 then UNPREDICTABLE;
		// if strexd && (Rt<0> == '1' || t == 14) then UNPREDICTABLE;
		// if d == n || d == t || d == t2 then UNPREDICTABLE;
		if (tmp1 == 15) unp++; // n == 15
		tmp2 = bitrng(instr, 15, 12); // Rd
		if (tmp2 == tmp1) unp++; // d == n
		tmp3 = bitrng(instr, 3, 0); // Rt
		if (tmp3 == 15) unp++; // t == 15
		if (tmp2 == tmp1) unp++; // d == t
		if (tmp2 == 15)
		{
			unp++; // d == 15
		}
		else if (extra == arm_sync_strexd)
		{
			if (tmp3 == 14) unp++; // strexd && t+1 == 15
			else if (tmp3 & 1) unp++; // Rt<0> == '1'
		}
		retval = set_arm_addr(0); // assume success
	}

	if (retval.flag != INSTR_ADDR_UNDEF)
	{
		if (unp)
		{
			retval = set_unpred_addr(retval);
		}
	}
	return retval;
}

instr_next_addr_t arm_core_ldstrh(unsigned int instr, ARM_decode_extra_t extra)
{
	// bits 24 - 20: PUIWL (I = immediate)
	// Rn = bits 19-16, Rt = bits 15-12, Rm = bits 3-0
	// imm = bits 11-8 and bits 3-0
	// if P == '0' && W == '1' then SEE LDRHT;
	// imm32 = ZeroExtend(imm4H:imm4L, 32);
	// if t == 15 || (wback && n == t) then UNPREDICTABLE;
	// if Rn == PC && P == W then UNPREDICTABLE
	// if m == 15 then UNPREDICTABLE
	// if LDRHT && (t == 15 || n == 15 || n == t) then UNPREDICTABLE;
	// LDRHT and STRHT are UNPREDICTABLE in Hyp mode.
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4;
	int unp = 0;

	(void) extra;

	retval = set_undef_addr();

	tmp1 = bitrng(instr, 15, 12); // Rt
	tmp2 = bitrng(instr, 19, 16); // Rn
	if (bit(instr, 21) && (tmp1 == tmp2)) unp++;
	// offset
	if (tmp1 == 15)
	{
		unp++; // Rt == 15
		// offset
		if (bit(instr, 22)) // imm
		{
			tmp3 = bits(instr, 0xf0f);
		}
		else // reg
		{
			if (bit(instr, 21) && (tmp1 == tmp2)) unp++;
			tmp3 = bitrng(instr, 3, 0); // Rm
			if (tmp3 == 15) unp++;
			tmp3 = rpi2_reg_context.storage[tmp3]; // (Rm)
		}
	}
	else if (tmp2 == 15) // Rn == 15
	{
		// offset
		if (bit(instr, 22)) // imm
		{
			tmp3 = bits(instr, 0xf0f);
		}
		else // reg
		{
			if (bit(instr, 21)) unp++;
			tmp3 = bitrng(instr, 3, 0); // Rm
			if (tmp3 == 15) unp++;
			tmp3 = rpi2_reg_context.storage[tmp3]; // (Rm)
		}
	}

	retval = set_addr_lin(); // if PC is not involved

	// bits 24 - 20: PUIWL
	if (bit(instr, 20)) // load
	{
		if (tmp1 == 15) // Rt == PC
		{
			// P & W
			switch (bits(instr, (1<<24 | 1<<21)))
			{
			case 1: // ldrht - always postindexed
				if (check_proc_mode(INSTR_PMODE_HYP, 0, 0, 0))
				{
					unp++;
				}
				// no break
			case 0: // postindexing Rt = (Rn), Rn += offset
				if (tmp2 == 15) // assume that writeback is done after load
				{
					// loaded value becomes overwritten by writeback
					tmp4 = rpi2_reg_context.storage[tmp3];
					if (bit(instr, 23)) // U
					{
						tmp4 += tmp3;
					}
					else
					{
						tmp4 -= tmp3;
					}
				}
				else // load only
				{
					tmp4 = rpi2_reg_context.storage[tmp3];
					if (bit(instr, 23)) // U
					{
						tmp4 += tmp3;
					}
					else
					{
						tmp4 -= tmp3;
					}
					tmp4 = (unsigned int)(* ((unsigned short *) tmp4));
				}
				break;
			case 2: // offset Rt = (Rn + offset)
				tmp4 = rpi2_reg_context.storage[tmp3];
				if (bit(instr, 23)) // U
				{
					tmp4 += tmp3;
				}
				else
				{
					tmp4 -= tmp3;
				}
				tmp4 = (unsigned int)(* ((unsigned short *) tmp4));

				break;
			case 3:	// preindexing Rn += offset, Rt = (Rn)
				if (tmp2 == 15) // assume that writeback is done after load
				{
					// loaded value becomes overwritten by writeback
					tmp4 = rpi2_reg_context.storage[tmp3];
					if (bit(instr, 23)) // U
					{
						tmp4 += tmp3;
					}
					else
					{
						tmp4 -= tmp3;
					}
				}
				else
				{
					tmp4 = rpi2_reg_context.storage[tmp3];
					if (bit(instr, 23)) // U
					{
						tmp4 += tmp3;
					}
					else
					{
						tmp4 -= tmp3;
					}
					tmp4 = (unsigned int)(* ((unsigned int *) tmp4));
				}
				break;
			default:
				// shouldn't get here
				break;
			}
			retval = set_arm_addr(tmp4);
		}
		else if (tmp2 == 15) // Rn == PC
		{
			// P & W
			switch (bits(instr, (1<<24 | 1<<21)))
			{
			case 1: // ldrht - always postindexed
				if (check_proc_mode(INSTR_PMODE_HYP, 0, 0, 0))
				{
					unp++;
				}
				// no break
			case 0: // postindexing
				// writeback
				tmp4 = rpi2_reg_context.storage[tmp3];
				if (bit(instr, 23)) // U
				{
					tmp4 += tmp3;
				}
				else
				{
					tmp4 -= tmp3;
				}
				break;
			case 2: // offset
				tmp4 = tmp3;
				break;
			case 3:	// preindexing
				// writeback
				tmp4 = rpi2_reg_context.storage[tmp3];
				if (bit(instr, 23)) // U
				{
					tmp4 += tmp3;
				}
				else
				{
					tmp4 -= tmp3;
				}
				break;
			default:
				// shouldn't get here
				break;
			}
			retval = set_arm_addr(tmp4);
		}
	}
	else // store - only writeback can change PC
	{
		if (tmp2 == 15) // Rn == PC
		{
			// P & W
			switch (bits(instr, (1<<24 | 1<<21)))
			{
			case 0: // postindexing
			case 1: // ldrht - always postindexed
				// writeback
				tmp4 = rpi2_reg_context.storage[tmp3];
				if (bit(instr, 23)) // U
				{
					tmp4 += tmp3;
				}
				else
				{
					tmp4 -= tmp3;
				}
				break;
			case 2: // offset
				tmp4 = tmp3;
				break;
			case 3:	// preindexing
				// writeback
				tmp4 = rpi2_reg_context.storage[tmp3];
				if (bit(instr, 23)) // U
				{
					tmp4 += tmp3;
				}
				else
				{
					tmp4 -= tmp3;
				}
				break;
			default:
				// shouldn't get here
				break;
			}
			retval = set_arm_addr(tmp4);
		}
	}

	if (retval.flag != INSTR_ADDR_UNDEF)
	{
		if (unp)
		{
			retval = set_unpred_addr(retval);
		}
	}
	return retval;
}

instr_next_addr_t arm_core_ldstrsb(unsigned int instr, ARM_decode_extra_t extra)
{
	// bits 24 - 21: PUIW (there is no store)
	// Rn = bits 19-16, Rt = bits 15-12, Rm = bits 3-0
	// imm = bits 11-8 and bits 3-0
	// if P == '0' && W == '1' then SEE LDRHT;
	// imm32 = ZeroExtend(imm4H:imm4L, 32);
	// if t == 15 || (wback && n == t) then UNPREDICTABLE;
	// if Rn == PC && P == W then UNPREDICTABLE
	// if m == 15 then UNPREDICTABLE
	// if LDRSBT && (t == 15 || n == 15 || n == t) then UNPREDICTABLE;
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4;
	int unp = 0;

	(void) extra;

	retval = set_undef_addr();

	tmp1 = bitrng(instr, 15, 12); // Rt
	tmp2 = bitrng(instr, 19, 16); // Rn
	if (bit(instr, 21) && (tmp1 == tmp2)) unp++;
	// offset
	if (tmp1 == 15)
	{
		unp++; // Rt == 15
		// imm
		if (bit(instr, 22)) // imm
		{
			tmp3 = bits(instr, 0xf0f);
		}
		else // reg
		{
			if (bit(instr, 21) && (tmp1 == tmp2)) unp++;
			tmp3 = bitrng(instr, 3, 0); // Rm
			if (tmp3 == 15) unp++;
			tmp3 = rpi2_reg_context.storage[tmp3]; // (Rm)
		}
	}
	else if (tmp2 == 15) // Rn == 15
	{
		// imm
		if (bit(instr, 22)) // imm
		{
			tmp3 = bits(instr, 0xf0f);
		}
		else // reg
		{
			if (bit(instr, 21)) unp++;
			tmp3 = bitrng(instr, 3, 0); // Rm
			if (tmp3 == 15) unp++;
			tmp3 = rpi2_reg_context.storage[tmp3]; // (Rm)
		}
	}

	retval = set_addr_lin(); // if PC is not involved

	if (tmp1 == 15) // Rt == PC
	{
		// P & W
		switch (bits(instr, (1<<24 | 1<<21)))
		{
		case 1: // ldrsbt - always postindexed
		case 0: // postindexing Rt = (Rn), Rn += offset
			if (tmp2 == 15) // assume that writeback is done after load
			{
				// loaded value becomes overwritten by writeback
				tmp4 = rpi2_reg_context.storage[tmp3];
				if (bit(instr, 23)) // U
				{
					tmp4 += tmp3;
				}
				else
				{
					tmp4 -= tmp3;
				}
			}
			else // load only
			{
				tmp4 = rpi2_reg_context.storage[tmp3];
				if (bit(instr, 23)) // U
				{
					tmp4 += tmp3;
				}
				else
				{
					tmp4 -= tmp3;
				}
				tmp4 = (unsigned int)(* ((unsigned short *) tmp4));
			}
			break;
		case 2: // offset Rt = (Rn + offset)
			tmp4 = rpi2_reg_context.storage[tmp3];
			if (bit(instr, 23)) // U
			{
				tmp4 += tmp3;
			}
			else
			{
				tmp4 -= tmp3;
			}
			tmp4 = (unsigned int)(* ((unsigned short *) tmp4));

			break;
		case 3:	// preindexing Rn += offset, Rt = (Rn)
			if (tmp2 == 15) // assume that writeback is done after load
			{
				// loaded value becomes overwritten by writeback
				tmp4 = rpi2_reg_context.storage[tmp3];
				if (bit(instr, 23)) // U
				{
					tmp4 += tmp3;
				}
				else
				{
					tmp4 -= tmp3;
				}
			}
			else
			{
				tmp4 = rpi2_reg_context.storage[tmp3];
				if (bit(instr, 23)) // U
				{
					tmp4 += tmp3;
				}
				else
				{
					tmp4 -= tmp3;
				}
				tmp4 = (unsigned int)(* ((unsigned int *) tmp4));
			}
			break;
		default:
			// shouldn't get here
			break;
		}
		tmp4 = (unsigned int) instr_util_signx_byte(tmp4);
		retval = set_arm_addr(tmp4);
	}
	else if (tmp2 == 15) // Rn == PC
	{
		// P & W
		switch (bits(instr, (1<<24 | 1<<21)))
		{
		case 1: // ldrsbt - always postindexed
		case 0: // postindexing
			// writeback
			tmp4 = rpi2_reg_context.storage[tmp3];
			if (bit(instr, 23)) // U
			{
				tmp4 += tmp3;
			}
			else
			{
				tmp4 -= tmp3;
			}
			break;
		case 2: // offset
			tmp4 = tmp3;
			break;
		case 3:	// preindexing
			// writeback
			tmp4 = rpi2_reg_context.storage[tmp3];
			if (bit(instr, 23)) // U
			{
				tmp4 += tmp3;
			}
			else
			{
				tmp4 -= tmp3;
			}
			break;
		default:
			// shouldn't get here
			break;
		}
		retval = set_arm_addr(tmp4);
	}

	if (retval.flag != INSTR_ADDR_UNDEF)
	{
		if (unp)
		{
			retval = set_unpred_addr(retval);
		}
	}
	return retval;
}

instr_next_addr_t arm_core_ldstsh(unsigned int instr, ARM_decode_extra_t extra)
{
	// bits 24 - 21: PUIW (there is no store)
	// Rn = bits 19-16, Rt = bits 15-12, Rm = bits 3-0
	// imm = bits 11-8 and bits 3-0
	// if P == '0' && W == '1' then SEE LDRHT;
	// imm32 = ZeroExtend(imm4H:imm4L, 32);
	// if t == 15 || (wback && n == t) then UNPREDICTABLE;
	// if Rn == PC && P == W then UNPREDICTABLE
	// if m == 15 then UNPREDICTABLE
	// if LDRSHT && (t == 15 || n == 15 || n == t) then UNPREDICTABLE;
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4;
	int unp = 0;

	(void) extra;

	retval = set_undef_addr();

	tmp1 = bitrng(instr, 15, 12); // Rt
	tmp2 = bitrng(instr, 19, 16); // Rn
	if (bit(instr, 21) && (tmp1 == tmp2)) unp++;
	// offset
	if (tmp1 == 15)
	{
		unp++; // Rt == 15
		// imm
		if (bit(instr, 22)) // imm
		{
			tmp3 = bits(instr, 0xf0f);
		}
		else // reg
		{
			if (bit(instr, 21) && (tmp1 == tmp2)) unp++;
			tmp3 = bitrng(instr, 3, 0); // Rm
			if (tmp3 == 15) unp++;
			tmp3 = rpi2_reg_context.storage[tmp3]; // (Rm)
		}
	}
	else if (tmp2 == 15) // Rn == 15
	{
		// imm
		if (bit(instr, 22)) // imm
		{
			tmp3 = bits(instr, 0xf0f);
		}
		else // reg
		{
			if (bit(instr, 21)) unp++;
			tmp3 = bitrng(instr, 3, 0); // Rm
			if (tmp3 == 15) unp++;
			tmp3 = rpi2_reg_context.storage[tmp3]; // (Rm)
		}
	}

	retval = set_addr_lin(); // if PC is not involved

	if (tmp1 == 15) // Rt == PC
	{
		// P & W
		switch (bits(instr, (1<<24 | 1<<21)))
		{
		case 1: // ldrsht - always postindexed
		case 0: // postindexing Rt = (Rn), Rn += offset
			if (tmp2 == 15) // assume that writeback is done after load
			{
				// loaded value becomes overwritten by writeback
				tmp4 = rpi2_reg_context.storage[tmp3];
				if (bit(instr, 23)) // U
				{
					tmp4 += tmp3;
				}
				else
				{
					tmp4 -= tmp3;
				}
			}
			else // load only
			{
				tmp4 = rpi2_reg_context.storage[tmp3];
				if (bit(instr, 23)) // U
				{
					tmp4 += tmp3;
				}
				else
				{
					tmp4 -= tmp3;
				}
				tmp4 = (unsigned int)(* ((unsigned short *) tmp4));
			}
			break;
		case 2: // offset Rt = (Rn + offset)
			tmp4 = rpi2_reg_context.storage[tmp3];
			if (bit(instr, 23)) // U
			{
				tmp4 += tmp3;
			}
			else
			{
				tmp4 -= tmp3;
			}
			tmp4 = (unsigned int)(* ((unsigned short *) tmp4));

			break;
		case 3:	// preindexing Rn += offset, Rt = (Rn)
			if (tmp2 == 15) // assume that writeback is done after load
			{
				// loaded value becomes overwritten by writeback
				tmp4 = rpi2_reg_context.storage[tmp3];
				if (bit(instr, 23)) // U
				{
					tmp4 += tmp3;
				}
				else
				{
					tmp4 -= tmp3;
				}
			}
			else
			{
				tmp4 = rpi2_reg_context.storage[tmp3];
				if (bit(instr, 23)) // U
				{
					tmp4 += tmp3;
				}
				else
				{
					tmp4 -= tmp3;
				}
				tmp4 = (unsigned int)(* ((unsigned int *) tmp4));
			}
			break;
		default:
			// shouldn't get here
			break;
		}
		tmp4 = (unsigned int) instr_util_signx_short(tmp4);
		retval = set_arm_addr(tmp4);
	}
	else if (tmp2 == 15) // Rn == PC
	{
		// P & W
		switch (bits(instr, (1<<24 | 1<<21)))
		{
		case 1: // ldrsht - always postindexed
		case 0: // postindexing
			// writeback
			tmp4 = rpi2_reg_context.storage[tmp3];
			if (bit(instr, 23)) // U
			{
				tmp4 += tmp3;
			}
			else
			{
				tmp4 -= tmp3;
			}
			break;
		case 2: // offset
			tmp4 = tmp3;
			break;
		case 3:	// preindexing
			// writeback
			tmp4 = rpi2_reg_context.storage[tmp3];
			if (bit(instr, 23)) // U
			{
				tmp4 += tmp3;
			}
			else
			{
				tmp4 -= tmp3;
			}
			break;
		default:
			// shouldn't get here
			break;
		}
		retval = set_arm_addr(tmp4);
	}

	if (retval.flag != INSTR_ADDR_UNDEF)
	{
		if (unp)
		{
			retval = set_unpred_addr(retval);
		}
	}
	return retval;
}

instr_next_addr_t arm_core_misc(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4;
	int unp = 0;

	retval = set_undef_addr();

	switch (extra)
	{
	case arm_misc_sev:
	case arm_misc_dbg:
	case arm_misc_setend:
	case arm_misc_clrex:
	case arm_misc_dmb:
	case arm_misc_dsb:
	case arm_misc_isb:
	case arm_misc_pld_imm:
	case arm_misc_pld_lbl:
	case arm_misc_pli_lbl:
		retval = set_addr_lin();
		break;
	case arm_misc_pld_r:
		if (bit(instr, 22) && (bitrng(instr, 19, 16) == 15)) // pldw && n == 15
		{
			retval = set_addr_lin();
			retval = set_unpred_addr(retval);
		}
		// no break
	case arm_misc_pli_r:
		if (bitrng(instr, 3, 0) == 15) // m == 15
		{
			retval = set_addr_lin();
			retval = set_unpred_addr(retval);
		}
		break;
	case arm_misc_swp:
		// tmp <- (Rn); Rt2 -> (Rn); Rt <- tmp
		// bit 22 == 0 => swpb
		// Rn = bits 19-16, Rt = bits 15-12, Rt2 = bits 3-0
		// if t == 15 || t2 == 15 || n == 15 || n == t || n == t2 then UNPREDICTABLE;
		tmp1 = bitrng(instr, 19, 16);
		tmp2 = bitrng(instr, 15, 12);
		tmp3 = bitrng(instr, 3, 0);
		retval = set_addr_lin();
		if ((tmp1 == 15) || (tmp2 == 15) || (tmp3 == 15)
			|| (tmp1 == tmp2) || (tmp1 == tmp3))
		{
			unp++;
		}
		if (tmp2 == 15) // if PC is affected
		{
			tmp4 = rpi2_reg_context.storage[tmp1];
			if (bit(instr, 22)) // B
			{
				// swp
				tmp1 = *((unsigned int *)tmp4);
			}
			else
			{
				// swpb
				tmp1 = (unsigned int)(*((unsigned char *)tmp4));
			}
			retval = set_arm_addr(tmp1);
		}
		if (unp)
		{
			retval = set_unpred_addr(retval);
		}
		break;
	default:
		// just in case...
		break;
	}
	return retval;
}

instr_next_addr_t arm_core_status(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;
	unsigned int proc_mode;
	unsigned int secure_state;
	int unp = 0;
	unsigned int tmp1, tmp2, tmp3, tmp4;

	retval = set_undef_addr();

	if (extra == arm_cstat_cps)
	{
		// imod = bits 19-18, M = bit 17, AIF = bits 8-6
		// mode = bits 4-0
		// if mode != '00000' && M == '0' then UNPREDICTABLE;
		// if (imod<1> == '1' && A:I:F == '000') || (imod<1> == '0' && A:I:F != '000')
		//   then UNPREDICTABLE;
		// if (imod == '00' && M == '0') || imod == '01' then UNPREDICTABLE;

		retval = set_addr_lin(); // in all cases
		if (check_proc_mode(INSTR_PMODE_USR, 0, 0, 0))
		{
			 // NOP
			return retval;
		}

		tmp1 = bitrng(instr, 4, 0); // mode
		if ((bit(instr, 17) == 0) && (tmp1 != 0))
		{
			retval = set_unpred_addr(retval);
			return retval;
		}
		tmp2 = bitrng(instr, 19, 18); // imod
		if ((tmp2 == 1) || ((tmp2 = 0) && (bit(instr, 17) == 0)))
		{
			retval = set_unpred_addr(retval);
			return retval;
		}
		if (bitrng(instr, 8, 6) == 0)
		{
			if (bit(instr, 19))
			{
				retval = set_unpred_addr(retval);
				return retval;
			}
		}
		else
		{
			if (!bit(instr, 19))
			{
				retval = set_unpred_addr(retval);
				return retval;
			}
		}

		if (bit(instr, 17))
		{
			switch (tmp1)
			{
			case INSTR_PMODE_MON:

				if (!get_security_state())
				{
					unp++;
				}
				break;
			case INSTR_PMODE_FIQ:
				tmp2 = get_security_state();
				tmp3 = get_NSACR();
				if (bit(tmp3, 19) && (tmp2 == 0))
				{
					unp++;
				}
				break;
			case INSTR_PMODE_HYP:
				if (get_security_state() && (!check_proc_mode(INSTR_PMODE_MON, 0, 0, 0)))
				{
					unp++;
				}
				else if (check_proc_mode(INSTR_PMODE_MON, 0, 0, 0))
				{
					if((get_SCR() & 1) == 0)
					{
						unp++;
					}
				}
				else
				{
					if (!check_proc_mode(INSTR_PMODE_HYP, 0, 0, 0))
					{
						unp++;
					}
				}
				break;
			case INSTR_PMODE_IRQ:
			case INSTR_PMODE_SVC:
			case INSTR_PMODE_ABT:
			case INSTR_PMODE_UND:
			case INSTR_PMODE_SYS:
				break;
			default:
				unp++;
				break;
			}
		}
	}
	else // MSR/MRS (banked reg)
	{
		proc_mode = get_proc_mode();
		secure_state = get_security_state();
		tmp1 = bitrng(instr, 19, 16); // accessed mode
		tmp1 |= bit(instr, 8) << 4;
		tmp2 = bitrng(tmp1, 2, 0); // accessed register code
		// 'normalize' access mode to match processor mode
		switch (bitrng(tmp1, 4, 3))
		{
		case 0: // USR
			tmp1 = INSTR_PMODE_USR;
			tmp2 = tmp2 + 8; // register
			if (tmp2 == 15) // illegal -> UNPREDICTABLE
			{
				tmp2 = 0;
				unp++;
			}
			break;
		case 1: // FIQ
			tmp1 = INSTR_PMODE_FIQ;
			tmp2 = tmp2 + 8; // register
			if (tmp2 == 15) // illegal -> UNPREDICTABLE
			{
				tmp2 = 0;
				unp++;
			}
			break;
		case 2: // IRQ, SVC, ABT, UND
			switch (tmp2)
			{
			case 0:
				tmp1 = INSTR_PMODE_IRQ;
				tmp2 = 14; // LR
				break;
			case 1:
				tmp1 = INSTR_PMODE_IRQ;
				tmp2 = 13; // SP
				break;
			case 2:
				tmp1 = INSTR_PMODE_SVC;
				tmp2 = 14; // LR
				break;
			case 3:
				tmp1 = INSTR_PMODE_SVC;
				tmp2 = 13; // SP
				break;
			case 4:
				tmp1 = INSTR_PMODE_ABT;
				tmp2 = 14; // LR
				break;
			case 5:
				tmp1 = INSTR_PMODE_ABT;
				tmp2 = 13; // SP
				break;
			case 6:
				tmp1 = INSTR_PMODE_UND;
				tmp2 = 14; // LR
				break;
			case 7:
				tmp1 = INSTR_PMODE_UND;
				tmp2 = 13; // SP
				break;
			default:
				// logically impossible to get here
				break;
			}
			break;
		case 3: // MON, HYP
			switch (tmp2)
			{
			case 0:
			case 1:
			case 2:
			case 3:
				tmp2 = 0;
				unp++;
				break;
			case 4:
				tmp1 = INSTR_PMODE_MON;
				tmp2 = 14; // LR
				break;
			case 5:
				tmp1 = INSTR_PMODE_MON;
				tmp2 = 13; // SP
				break;
			case 6:
				tmp1 = INSTR_PMODE_HYP;
				tmp2 = 14; // LR
				break;
			case 7:
				tmp1 = INSTR_PMODE_HYP;
				tmp2 = 13; // SP
				break;
			default:
				// not logically possible to get here
				break;
			}
			break;
		default:
			// not logically possible to get here
			break;
		}
		if ((tmp2 == 14) && bit(instr, 22)) tmp2 = 16; // spsr

		if (tmp2 == 0)
		{
			// bad register
			retval = set_addr_lin();
			retval = set_unpred_addr(retval);
			return retval;
		}

		tmp4 = bitrng(instr, 15, 12); // Rd

		switch (proc_mode)
		{
		case INSTR_PMODE_USR:
			unp++;
			retval = set_addr_lin(); // just a guess...
			break;
		case INSTR_PMODE_FIQ:
			tmp3 = get_NSACR();
			if (bit(tmp3, 19) && (secure_state == 0))
			{
				unp++;
				retval = set_addr_lin();
				break;
			}
			switch(tmp1)
			{
			case INSTR_PMODE_FIQ:
			case INSTR_PMODE_HYP:
			case INSTR_PMODE_MON:
				unp++;
				retval = set_addr_lin();
				break;
			default:
				if (extra == arm_cstat_msr_b)
				{
					retval = set_addr_lin();
				}
				else // arm_cstat_mrs_b
				{
					if (tmp4 == 15)
					{
						unp++;
						tmp4 = get_mode_reg(tmp1, tmp2);
						retval = set_arm_addr(tmp4);
					}
					else
					{
						retval = set_addr_lin();
					}
				}
				break;
			}
			break;
		case INSTR_PMODE_IRQ:
		case INSTR_PMODE_SVC:
		case INSTR_PMODE_ABT:
		case INSTR_PMODE_UND:
			if (proc_mode == tmp1) // current mode registers
			{
				unp++;
				retval = set_addr_lin();
			}
			else if (tmp1 == INSTR_PMODE_HYP)
			{
				unp++;
				retval = set_addr_lin();
			}
			else if ((tmp1 == INSTR_PMODE_MON) && (secure_state == 0))
			{
				unp++;
				retval = set_addr_lin();
			}
			else if (tmp1 == INSTR_PMODE_FIQ)
			{
				tmp3 = get_NSACR();
				// FIQ mode and the FIQ Banked registers are accessible in
				// Secure security state only.
				if (bit(tmp3, 19) && (secure_state == 0))
				{
					unp++;
					retval = set_addr_lin();
				}
				else
				{
					// access FIQ
					if (extra == arm_cstat_msr_b)
					{
						retval = set_addr_lin();
					}
					else // arm_cstat_mrs_b
						{
						if (tmp4 == 15)
						{
							unp++;
							tmp4 = get_mode_reg(tmp1, tmp2);
							retval = set_arm_addr(tmp4);
						}
						else
						{
							retval = set_addr_lin();
						}
					}
				}
			}
			else
			{
				if (extra == arm_cstat_msr_b)
				{
					retval = set_addr_lin();
				}
				else // arm_cstat_mrs_b
				{
					if (tmp4 == 15)
					{
						unp++;
						tmp4 = get_mode_reg(tmp1, tmp2);
						retval = set_arm_addr(tmp4);
					}
					else
					{
						retval = set_addr_lin();
					}
				}
			}
			break;
		case INSTR_PMODE_MON:
			if (extra == arm_cstat_msr_b)
			{
				retval = set_addr_lin();
			}
			else // arm_cstat_mrs_b
			{
				if (tmp4 == 15)
				{
					unp++;
					tmp4 = get_mode_reg(tmp1, tmp2);
					retval = set_arm_addr(tmp4);
				}
				else
				{
					retval = set_addr_lin();
				}
			}
			break;
		case INSTR_PMODE_HYP:
			if ((tmp1 == proc_mode) || (tmp1 == INSTR_PMODE_MON)
				|| ((tmp1 == INSTR_PMODE_FIQ) && (get_NSACR() & (1 << 19))))
			{
				unp++;
				retval = set_addr_lin();
			}
			else
			{
				if (extra == arm_cstat_msr_b)
				{
					retval = set_addr_lin();
				}
				else // arm_cstat_mrs_b
				{
					if (tmp4 == 15)
					{
						unp++;
						tmp4 = get_mode_reg(tmp1, tmp2);
						retval = set_arm_addr(tmp4);
					}
					else
					{
						retval = set_addr_lin();
					}
				}
			}
			break;
		case INSTR_PMODE_SYS:
			if ((tmp1 == proc_mode) || (tmp1 == INSTR_PMODE_HYP)
				|| (tmp1 == INSTR_PMODE_MON))
			{
				unp++;
				retval = set_addr_lin();
			}
			else
			{
				if (extra == arm_cstat_msr_b)
				{
					retval = set_addr_lin();
				}
				else // arm_cstat_mrs_b
				{
					if (tmp4 == 15)
					{
						unp++;
						tmp4 = get_mode_reg(tmp1, tmp2);
						retval = set_arm_addr(tmp4);
					}
					else
					{
						retval = set_addr_lin();
					}
				}
			}
			break;
		default:
			// As user
			unp++;
			retval = set_addr_lin();
			break;
		}
	}

	if (retval.flag != INSTR_ADDR_UNDEF)
	{
		if (unp)
		{
			retval = set_unpred_addr(retval);
		}
	}
	return retval;
}

instr_next_addr_t arm_fp(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;

/*
arm_fp_vcmp_r
arm_fp_vcmp_z
arm_fp_vcvt_f3216
arm_fp_vcvt_f6432
arm_fp_vcvt_f6432_bc
arm_fp_vcvt_sf6432
arm_fp_vdiv
arm_fp_vfnm

 */
	(void) instr;
	(void) extra;

	retval = set_addr_lin();
	return retval;
}

instr_next_addr_t arm_v_bits(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;
/*
arm_vbits_vmov_6432_i - lin
arm_vbits_vmov_6432_r - lin
arm_vbits_vand
arm_vbits_vbic
arm_vbits_vbif
arm_vbits_vbit
arm_vbits_vbsl
arm_vbits_veor
arm_vbits_vmvn
arm_vbits_vorn
 */
	(void) instr;
	(void) extra;
	retval = set_addr_lin();
	return retval;
}

instr_next_addr_t arm_v_comp(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;

/*
arm_vcmp_vacge
arm_vcmp_vacgt
arm_vcmp_vceq
arm_vcmp_vceq_dt
arm_vcmp_vceq_z
arm_vcmp_vcge
arm_vcmp_vcge_dt
arm_vcmp_vcge_z
arm_vcmp_vcgt_dt
arm_vcmp_vcgt
arm_vcmp_vcgt_z
arm_vcmp_vcle_z
arm_vcmp_vclt_z
arm_vcmp_vtst
 */
	(void) instr;
	(void) extra;
	retval = set_addr_lin();
	return retval;
}

instr_next_addr_t arm_v_mac(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;

	(void) instr;
	(void) extra;
	retval = set_addr_lin();
	return retval;
}

instr_next_addr_t arm_v_misc(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;

	(void) instr;
	(void) extra;
	retval = set_addr_lin();
	return retval;
}

instr_next_addr_t arm_v_par(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;

	(void) instr;
	(void) extra;
	retval = set_addr_lin();
	return retval;
}

instr_next_addr_t arm_v_shift(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;

	(void) instr;
	(void) extra;
	retval = set_addr_lin();
	return retval;
}

instr_next_addr_t arm_vfp_ldst_elem(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4;
	unsigned int und = 0;
	unsigned int unp = 0;

/*
arm_vldste_vld1_all
arm_vldste_vld1_one
arm_vldste_vld2_all
arm_vldste_vld2_one
arm_vldste_vld3_all
arm_vldste_vld3_one
arm_vldste_vld4_all
arm_vldste_vld4_one
arm_vldste_vst1_one
arm_vldste_vst2_one
arm_vldste_vst3_one
arm_vldste_vst4_one
arm_vldste_vld1_mult
arm_vldste_vld2_mult
arm_vldste_vld3_mult
arm_vldste_vld4_mult
arm_vldste_vst1_mult
arm_vldste_vst2_mult
arm_vldste_vst3_mult
arm_vldste_vst4_mult
*/
	// bits 19-16 = Rn
	// bits 3 - 0 = rm
	// wback = (m != 15); register_index = (m != 15 && m != 13);
	// if n == 15 || d+regs > 32 then UNPREDICTABLE
	// Rn is updated by this instruction: Rn = Rn + Rm
	// [<Rn>{:<align>}] Encoded as Rm = 0b1111.
	// [<Rn>{:<align>}]! Encoded as Rm = 0b1101.
	// [<Rn>{:<align>}], <Rm> Encoded as Rm = Rm. Rm must not be encoded
	//   as 0b1111 or 0b1101, the PC or the SP.
	retval = set_undef_addr();

	//tmp1 = bitrng(instr, 19, 16); // accessed mode
	//tmp1 |= bit(instr, 8) << 4;
	// tmp2 = rpi2_reg_context.storage[tmp2]
	tmp1 = bitrng(instr, 19, 16); // Rn
	tmp2 = bitrng(instr, 3, 0); // Rm

	switch (extra)
	{
	case arm_vldste_vld1_mult:
	case arm_vldste_vst1_mult:
		tmp3 = bitrng(instr, 5, 4); // align
		switch (bitrng(instr, 11, 8)) // type
		{
		case 2:
			tmp4 = 4; // regs
			break;
		case 6:
			if (tmp3 & 2) und++;
			tmp4 = 3;
			break;
		case 7:
			if (tmp3 & 2) und++;
			tmp4 = 1;
			break;
		case 10:
			if (tmp3 == 3) und++;
			tmp4 = 2;
			break;
		}
		tmp4 *= 8; // Rn advancement if not Rm
		break;
	case arm_vldste_vld2_mult:
	case arm_vldste_vst2_mult:
		tmp3 = bitrng(instr, 5, 4); // align
		if (bitrng(instr, 7, 6) == 3) und++; // size
		switch (bitrng(instr, 11, 8)) // type
		{
		case 3:
			tmp4 = 2; // regs
			break;
		case 8:
			if (tmp3 == 3) und++;
			tmp4 = 1;
			break;
		case 9:
			if (tmp3 == 3) und++;
			tmp4 = 1;
			break;
		}
		tmp4 *= 16; // Rn advancement if not Rm
		break;
	case arm_vldste_vld3_mult:
	case arm_vldste_vst3_mult:
		if (bit(instr, 5)) und++; // align<1>
		if (bitrng(instr, 7, 6) == 3) und++; // size
		tmp4 = 24; // Rn advancement if not Rm
		break;
	case arm_vldste_vld4_mult:
	case arm_vldste_vst4_mult:
		if (bitrng(instr, 7, 6) == 3) und++; // size
		tmp4 = 32; // Rn advancement if not Rm
		break;
	case arm_vldste_vld1_one:
	case arm_vldste_vst1_one:
		tmp3 = bitrng(instr, 7, 4); // index_align
		// size == 3 => single element to all lanes
		switch (bitrng(instr, 11, 10)) // size
		{
		case 0:
			if (tmp3 & 1) und++;
			tmp4 = 1; // Rn advancement if not Rm
			break;
		case 1:
			if (tmp3 & 2) und++;
			tmp4 = 2; // Rn advancement if not Rm
			break;
		case 2:
			if (tmp3 & 4) und++;
			if (((tmp3 & 3) == 1) || ((tmp3 & 3) == 2)) und++;
			tmp4 = 4; // Rn advancement if not Rm
			break;
		}
		break;
	case arm_vldste_vld2_one:
	case arm_vldste_vst2_one:
		tmp3 = bitrng(instr, 7, 4); // index_align
		// size == 3 => single element to all lanes
		switch (bitrng(instr, 11, 10)) // size
		{
		case 0:
			tmp4 = 1; // ebytes
			break;
		case 1:
			tmp4 = 2;
			break;
		case 2:
			if (tmp3 & 2) und++;
			tmp4 = 4;
			break;
		}
		tmp4 *= 2; // Rn advancement if not Rm (ebytes * 2)
		break;
	case arm_vldste_vld3_one:
	case arm_vldste_vst3_one:
		tmp3 = bitrng(instr, 7, 4); // index_align
		// size == 3 => single element to all lanes
		switch (bitrng(instr, 11, 10)) // size
		{
		case 0:
			if (tmp3 & 1) und++;
			tmp4 = 1; // ebytes
			break;
		case 1:
			if (tmp3 & 1) und++;
			tmp4 = 2;
			break;
		case 2:
			if (tmp3 & 3) und++;
			tmp4 = 4;
			break;
		}
		tmp4 *= 3; // Rn advancement if not Rm (ebytes * 3)
		break;
	case arm_vldste_vld4_one:
	case arm_vldste_vst4_one:
		tmp3 = bitrng(instr, 7, 4); // index_align
		// size == 3 => single element to all lanes
		switch (bitrng(instr, 11, 10)) // size
		{
		case 0:
			tmp4 = 1; // ebytes
			break;
		case 1:
			tmp4 = 2;
			break;
		case 2:
			if (tmp3 & 3) und++;
			tmp4 = 4;
			break;
		}
		tmp4 *= 4; // Rn advancement if not Rm (ebytes * 4)
		break;
	case arm_vldste_vld1_all:
		tmp3 = bitrng(instr, 7, 6); // size
		if (tmp3 == 3) und++;
		if ((tmp3 == 0) &&  bit(instr, 4)) und++;
		tmp4 = 1 << tmp3; // Rn advancement if not Rm (ebytes)
		break;
	case arm_vldste_vld2_all:
		tmp3 = bitrng(instr, 7, 6); // size
		if (tmp3 == 3) und++;
		tmp4 = 1 << tmp3; // ebytes
		tmp4 *= 2; // Rn advancement if not Rm (ebytes * 2)
		break;
	case arm_vldste_vld3_all:
		tmp3 = bitrng(instr, 7, 6); // size
		if ((tmp3 == 3) ||  bit(instr, 4)) und++;
		tmp4 = 1 << tmp3; // ebytes
		tmp4 *= 3; // Rn advancement if not Rm (ebytes * 3)
		break;
	case arm_vldste_vld4_all:
		tmp3 = bitrng(instr, 7, 6); // size
		if ((tmp3 == 3) && !bit(instr, 4)) und++;
		if (tmp3 == 3) tmp3 = 4; // ebytes
		else tmp4 = 1 << tmp3;
		tmp4 *= 4; // Rn advancement if not Rm (ebytes * 4)
		break;

		if (tmp1 == 15) // Rn
		{
			unp++;
			if (tmp2 == 15) // [<Rn>{:<align>}] => no writeback
			{
				retval = set_addr_lin();
				break;
			}
			else if (tmp2 == 13) // [<Rn>{:<align>}]! => Rn += transfer size
			{
				tmp1 = rpi2_reg_context.reg.r15 + tmp4;
				retval = set_arm_addr(tmp1);
			}
			else // [<Rn>{:<align>}], <Rm> => Rn += Rm
			{
				tmp4 = rpi2_reg_context.storage[tmp2];
				tmp1 = rpi2_reg_context.reg.r15 + tmp4;
				retval = set_arm_addr(tmp1);
			}
		}
		else
		{
			retval = set_addr_lin();
			break;
		}
		break;
	default:
		break;
	}
	if (und) retval = set_undef_addr();
	else if (unp) retval = set_unpred_addr(retval);
	return retval;
}

instr_next_addr_t arm_vfp_ldst_ext(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4;
	unsigned int und = 0;
	unsigned int unp = 0;
/*
arm_vldstx_vldm32
arm_vldstx_vldm64
arm_vldstx_vstm32
arm_vldstx_vstm64
arm_vldstx_vldr_d_imm
arm_vldstx_vldr_s_imm
arm_vldstx_vstr_d_imm
arm_vldstx_vstr_s_imm
arm_vldstx_vpop32
arm_vldstx_vpop64
arm_vldstx_vpush32
arm_vldstx_vpush64
*/
	retval = set_undef_addr();
	switch (extra)
	{
	case arm_vldstx_vldr_d_imm:
	case arm_vldstx_vldr_s_imm:
	case arm_vldstx_vstr_d_imm:
	case arm_vldstx_vstr_s_imm:
		// doesn't change the base register
		retval = set_addr_lin();
		break;
	case arm_vldstx_vpop32:
	case arm_vldstx_vpop64:
	case arm_vldstx_vpush32:
	case arm_vldstx_vpush64:
		if ((instr & 0xff) == 0) unp++; // reglist = 0
		// base register is always SP
		retval = set_addr_lin();
		break;
	case arm_vldstx_vldm32:
	case arm_vldstx_vldm64:
	case arm_vldstx_vstm32:
	case arm_vldstx_vstm64:
		tmp3 = (0xd << 21); // mask for PUW-bits
		tmp1 = bits(instr, tmp3); // PUW
		if (bitrng(instr, 19, 16) == 15) // Rn == PC
		{
			tmp2 = bitrng(instr, 7, 0); // registers
			// TODO: check FLDMX if vldm64/vstm64
			tmp2 <<= 2;
			tmp3 = rpi2_reg_context.reg.r15;
			switch (tmp1)
			{
			case 2: // IA without writeback
				tmp4 = tmp3;
				break;
			case 3: // IA with writeback
				tmp4 = tmp3 + tmp2;
				break;
			case 5: // DB with writeback
				tmp4 = tmp3 - tmp2;
				break;
			case 1:
			case 7:
				tmp4 = tmp3; // just in case...
				und++;
				break;
			/*
			case 0: 64-bit VMOV
			case 4, 6: VLDR imm/VSTR imm
			*/
			}
			retval = set_arm_addr(tmp4);
		}
		else
		{
			// PC not involved
			if ((tmp1 == 1) || (tmp1 == 7)) und++;
			retval = set_addr_lin();
			break;
		}
		break;
	}
	if (und) retval = set_undef_addr();
	else if (unp) retval = set_unpred_addr(retval);
	return retval;
}

instr_next_addr_t arm_vfp_xfer_reg(unsigned int instr, ARM_decode_extra_t extra)
{
	instr_next_addr_t retval;
	unsigned int tmp1, tmp2, tmp3, tmp4;
	unsigned int *ptmp;
	unsigned int und = 0;
	unsigned int unp = 0;
/*
arm_vfpxfer_vmov_d
arm_vfpxfer_vmov_ss
arm_vfpxfer_vdup
arm_vfpxfer_vmov_dt_dx
arm_vfpxfer_vmov_dx
arm_vfpxfer_vmov_s
arm_vfpxfer_vmrs_fpscr
arm_vfpxfer_vmrs_r
arm_vfpxfer_vmsr_fpscr
arm_vfpxfer_vmsr_r
*/
	retval = set_undef_addr();

	switch (extra)
	{
	case arm_vfpxfer_vmov_d:
	case arm_vfpxfer_vmov_ss:
		// if bit 20 = 1: Neon -> arm else arm -> Neon
		// ss:
		// to_arm_registers = (op == 1); t = UInt(Rt); t2 = UInt(Rt2); m = UInt(Vm:M);
		// if t == 15 || t2 == 15 || m == 31 then UNPREDICTABLE;
		// if to_arm_registers && t == t2 then UNPREDICTABLE;
		// d:
		// to_arm_registers = (op == 1); t = UInt(Rt); t2 = UInt(Rt2); m = UInt(M:Vm);
		// if t == 15 || t2 == 15 then UNPREDICTABLE;
		// if to_arm_registers && t == t2 then UNPREDICTABLE;
		tmp1 = bitrng(instr, 15, 12); // Rt
		tmp2 = bitrng(instr, 19, 16); // Rt2
		if (bit(instr, 20))
		{
			if (tmp1 == tmp2) unp++;
		}
		if (extra == arm_vfpxfer_vmov_ss)
		{
			tmp3 = (instr & 0xff) << 1;
			tmp3 |= bit(instr, 5); // Neon-register
			if (tmp3 == 31)
			{
				unp++;
				tmp3 = 30;
			}
		}
		else
		{
			tmp3 = (instr & 0xff);
			tmp3 |= bit(instr, 5) << 4;	// Neon-register
		}
		if ((tmp1 == 15) && (tmp2 == 15))
		{
			unp++;
			if (bit(instr, 20))
			{
				if (extra == arm_vfpxfer_vmov_ss)
				{
					// S[m+1] probably overwrites S[m]
					tmp4 = ((unsigned int *)rpi2_neon_context.storage)[tmp3+1];
				}
				else
				{
					// D[m]<63:32> probably overwrites D[m]<31:0>
					tmp4 = ((unsigned int *)rpi2_neon_context.storage)[tmp3*2+1];
				}
			}
			else
			{
				// ARM -> Neon
				retval = set_addr_lin();
				break;
			}
		}
		else if (tmp1 == 15)
		{
			unp++;
			if (bit(instr, 20))
			{
				if (extra == arm_vfpxfer_vmov_ss)
				{
					// S[m+1] probably overwrites S[m]
					tmp4 = ((unsigned int *)rpi2_neon_context.storage)[tmp3];
				}
				else
				{
					// D[m]<63:32> probably overwrites D[m]<31:0>
					tmp4 = ((unsigned int *)rpi2_neon_context.storage)[tmp3*2];
				}
			}
			else
			{
				// ARM -> Neon
				retval = set_addr_lin();
				break;
			}
		}
		else if (tmp2 == 15)
		{
			unp++;
			if (bit(instr, 20))
			{
				if (extra == arm_vfpxfer_vmov_ss)
				{
					// S[m+1] probably overwrites S[m]
					tmp4 = ((unsigned int *)rpi2_neon_context.storage)[tmp3+1];
				}
				else
				{
					// D[m]<63:32> probably overwrites D[m]<31:0>
					tmp4 = ((unsigned int *)rpi2_neon_context.storage)[tmp3*2+1];
				}
			}
			else
			{
				// ARM -> Neon
				retval = set_addr_lin();
				break;
			}
		}
		else
		{
			// PC not involved
			retval = set_addr_lin();
		}
		break;
	case arm_vfpxfer_vmrs_fpscr:
		// PC can't be involved: if Rt = 15, target is APSR_nzcv
		retval = set_addr_lin();
		break;
	case arm_vfpxfer_vmsr_fpscr:
		// if Rt == PC unp
		if (bitrng(instr, 15, 12) == 15) unp++;
		// ARM -> Neon
		retval = set_addr_lin();
		break;
	case arm_vfpxfer_vmrs_r:
		// if t == 15 && reg != '0001' then UNPREDICTABLE;
		tmp1 = bitrng(instr, 15, 12); // Rt
		if (tmp1 == 15) // PC
		{
			tmp2 = bitrng(instr, 19, 16); // reg
			switch (tmp2)
			{
			case 0:
				asm volatile(
						"vmrs %[retreg], FPSID\n\t"
						: [retreg] "=r" (tmp3)::
				);
				retval = set_arm_addr(tmp3);
				retval = set_unpred_addr(retval);
				break;
			case 1:
				asm volatile(
						"vmrs %[retreg], FPSCR\n\t"
						: [retreg] "=r" (tmp3)::
				);
				retval = set_arm_addr(tmp3);
				break;
			case 6:
				asm volatile(
						"vmrs %[retreg], MVFR1\n\t"
						: [retreg] "=r" (tmp3)::
				);
				retval = set_arm_addr(tmp3);
				retval = set_unpred_addr(retval);
				break;
			case 7:
				asm volatile(
						"vmrs %[retreg], MVFR0\n\t"
						: [retreg] "=r" (tmp3)::
				);
				retval = set_arm_addr(tmp3);
				retval = set_unpred_addr(retval);
				break;
			case 8:
				asm volatile(
						"vmrs %[retreg], FPEXC\n\t"
						: [retreg] "=r" (tmp3)::
				);
				retval = set_arm_addr(tmp3);
				retval = set_unpred_addr(retval);
				break;
			default:
				retval = set_undef_addr();
				break;
			}
		}
		else
		{
			// PC not involved
			retval = set_addr_lin();
		}
		break;
	case arm_vfpxfer_vmsr_r:
		// if Rt == PC unp
		if (bitrng(instr, 15, 12) == 15) unp++;
		// ARM -> Neon
		retval = set_addr_lin();
		break;
	case arm_vfpxfer_vdup: // linear
		// B=bit22, E=bit 5
		if ((bit(instr, 22) == 1) && (bit(instr, 5) == 1)) und++;
		// ARM -> Neon
		retval = set_addr_lin();
		break;
	case arm_vfpxfer_vmov_dx: // linear
		// opc1 = bits 22-21, opc2 = bits 6-5
		if ((bit(instr, 22) == 0) && (bitrng(instr, 6, 5) == 2)) und++;
		// ARM -> Neon
		retval = set_addr_lin();
	case arm_vfpxfer_vmov_dt_dx: // scalar to ARM
		// Rt = 15, unp
		tmp1 = bitrng(instr, 15, 12); // Rt
		tmp2 = bitrng(instr, 22, 21); // opc1
		tmp3 = bitrng(instr, 6, 5); // opc2
		if (tmp2 & 2)
		{
			// esize = 8; index = UInt(opc1<0>:opc2);
			tmp4 = (tmp2 & 1) << 2; // index
			tmp4 |= tmp3;
			tmp3 = bitrng(instr, 19, 16)*2; // Dn
			// &rpi2_neon_context.storage[Dn*2] points to beginning of
			// the lower word of Dn
			// it is used as an array of the 8-bytes of Dn
			ptmp = (unsigned int *)rpi2_neon_context.storage;
			tmp3 = (unsigned int) ((unsigned char *) &(ptmp[tmp3]))[tmp4];
			if (bit(instr, 23)) // unsigned?
			{
				retval = set_arm_addr(tmp3);
			}
			else
			{
				tmp3 = (unsigned int)instr_util_signx_byte(tmp3);
				retval = set_arm_addr(tmp3);
			}

		}
		else
		{
			if (tmp3 & 1)
			{
				// esize = 16; index = UInt(opc1<0>:opc2<1>);
				tmp4 = (tmp2 & 1) << 1; // index
				tmp4 |= tmp3 >> 1;
				tmp3 = bitrng(instr, 19, 16)*2; // Dn
				// &rpi2_neon_context.storage[Dn*2] points to beginning of
				// the lower word of Dn
				// it is used as an array of the 8-bytes of Dn
				ptmp = (unsigned int *)rpi2_neon_context.storage;
				tmp3 = (unsigned int) ((unsigned char *) &(ptmp[tmp3]))[tmp4];
				if (bit(instr, 23)) // unsigned?
				{
					retval = set_arm_addr(tmp3);
				}
				else
				{
					tmp3 = (unsigned int)instr_util_signx_short(tmp3);
					retval = set_arm_addr(tmp3);
				}
			}
			else
			{
				if (tmp3 & 2)
				{
					und++;
				}
				else
				{
					if (bit(instr, 23)) // U
					{
						und++;
					}
					else
					{
						// esize = 32; index = UInt(opc1<0>);
						tmp4 = (tmp2 & 1);
						tmp3 = bitrng(instr, 19, 16)*2; // Dn
						// &rpi2_neon_context.storage[Dn*2] points to beginning of
						// the lower word of Dn it is used as an array of the
						// 8-bytes of Dn
						ptmp = (unsigned int *)rpi2_neon_context.storage;
						tmp3 = (unsigned int) ((unsigned char *) &(ptmp[tmp3]))[tmp4];
						retval = set_arm_addr(tmp3);
					}
				}
			}
		}
		if (und == 0)
		{
			if (tmp1 == 15) // PC
			{
				unp++;
			}
			else
			{
				// PC not involved
				retval = set_addr_lin();
			}
		}
		else
		{
			retval = set_undef_addr();
		}
		break;
	case arm_vfpxfer_vmov_s: // ARM <-> Neon
		if (bit(instr, 20)) // op
		{
			if (bitrng(instr, 15, 12) == 15) // PC
			{
				tmp1 = bitrng(instr, 19, 16); // Vn
				tmp2 = ((unsigned int *)rpi2_neon_context.storage)[tmp1];
				retval = set_arm_addr(tmp2);
			}
			else
			{
				// PC not involved
				retval = set_addr_lin();
			}
		}
		else
		{
			// ARM -> Neon
			retval = set_addr_lin();
		}
		break;
	default:
		retval = set_undef_addr();
		break;
	}

	if (und) retval = set_undef_addr();
	else if (unp) retval = set_unpred_addr(retval);
	return retval;
}
