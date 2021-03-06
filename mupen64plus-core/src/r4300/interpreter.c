/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - interpreter.def                                         *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2002 Hacktarux                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Before #including this file, <stdint.h> and <inttypes.h> must be included,
 * and the following macros must be defined:
 *
 * PCADDR: Program counter (memory address of the current instruction).
 *
 * ADD_TO_PC(x): Increment the program counter in 'x' instructions.
 * This is only used for small changes to PC, so the new program counter
 * is guaranteed to fall in the current cached interpreter or dynarec block.
 *
 * DECLARE_INSTRUCTION(name)
 * Declares an instruction function which is not a jump.
 * Followed by a block of code.
 *
 * DECLARE_JUMP(name, destination, condition, link, likely, cop1)
 * name is the name of the jump or branch instruction.
 * destination is the destination memory address of the jump.
 * If condition is nonzero, the jump is taken.
 * link is a pointer to a variable where (PC+8) is written unconditionally.
 *     To avoid linking, pass &reg[0]
 * If likely is nonzero, the delay slot is only executed if the jump is taken.
 * If cop1 is nonzero, a COP1 unusable check will be done.
 *
 * CHECK_MEMORY(): A snippet to be run after a store instruction,
 *                 to check if the store affected executable blocks.
 *                 The memory address of the store is in the 'address' global.
 */

#include "fpu.h"

DECLARE_INSTRUCTION(NI)
{
   DebugMessage(M64MSG_ERROR, "NI() @ 0x%" PRIX32, PCADDR);
   DebugMessage(M64MSG_ERROR, "opcode not implemented: %" PRIX32 ":%" PRIX32, PCADDR, *fast_mem_access(PCADDR));
   stop=1;
}

DECLARE_INSTRUCTION(RESERVED)
{
   DebugMessage(M64MSG_ERROR, "reserved opcode: %" PRIX32 ":%" PRIX32, PCADDR, *fast_mem_access(PCADDR));
   stop=1;
}

/* R4300 */
DECLARE_JUMP(J,   (jinst_index<<2) | ((PCADDR+4) & UINT32_C(0xF0000000)), 1, &reg[0],  0, 0)
DECLARE_JUMP(JAL, (jinst_index<<2) | ((PCADDR+4) & UINT32_C(0xF0000000)), 1, &reg[31], 0, 0)
DECLARE_JUMP(BEQ,     PCADDR + (iimmediate+1)*4, irs == irt, &reg[0], 0, 0)
DECLARE_JUMP(BNE,     PCADDR + (iimmediate+1)*4, irs != irt, &reg[0], 0, 0)
DECLARE_JUMP(BLEZ,    PCADDR + (iimmediate+1)*4, irs <= 0,   &reg[0], 0, 0)
DECLARE_JUMP(BGTZ,    PCADDR + (iimmediate+1)*4, irs > 0,    &reg[0], 0, 0)

DECLARE_INSTRUCTION(ADDI)
{
   irt = SE32(irs32 + iimmediate);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(ADDIU)
{
   irt = SE32(irs32 + iimmediate);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(SLTI)
{
   if (irs < iimmediate) irt = 1;
   else irt = 0;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(SLTIU)
{
   if ((uint64_t) irs < (uint64_t) ((int64_t) iimmediate))
     irt = 1;
   else irt = 0;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(ANDI)
{
   irt = irs & (uint16_t) iimmediate;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(ORI)
{
   irt = irs | (uint16_t) iimmediate;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(XORI)
{
   irt = irs ^ (uint16_t) iimmediate;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(LUI)
{
   irt = SE32(iimmediate << 16);
   ADD_TO_PC(1);
}

DECLARE_JUMP(BEQL,    PCADDR + (iimmediate+1)*4, irs == irt, &reg[0], 1, 0)
DECLARE_JUMP(BNEL,    PCADDR + (iimmediate+1)*4, irs != irt, &reg[0], 1, 0)
DECLARE_JUMP(BLEZL,   PCADDR + (iimmediate+1)*4, irs <= 0,   &reg[0], 1, 0)
DECLARE_JUMP(BGTZL,   PCADDR + (iimmediate+1)*4, irs > 0,    &reg[0], 1, 0)

DECLARE_INSTRUCTION(DADDI)
{
   irt = irs + iimmediate;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DADDIU)
{
   irt = irs + iimmediate;
   ADD_TO_PC(1);
}

/* Assists unaligned memory accessors with making masks to preserve or apply
 * bits in registers and memory.
 *
 * BITS_BELOW_MASK32 and BITS_BELOW_MASK64 make masks where bits 0 to (x - 1)
 * are set.
 *
 * BITS_ABOVE_MASK32 makes masks where bits x to 31 are set.
 * BITS_ABOVE_MASK64 makes masks where bits x to 63 are set.
 *
 * e.g. x = 8
 * 0000 0000 0000 0000 0000 0000 1111 1111 <- BITS_BELOW_MASK32(8)
 * 1111 1111 1111 1111 1111 1111 0000 0000 <- BITS_ABOVE_MASK32(8)
 *
 * Giving a negative value or one that is >= the bit count of the mask results
 * in undefined behavior.
 */

#define BITS_BELOW_MASK32(x) ((UINT32_C(1) << (x)) - 1)
#define BITS_ABOVE_MASK32(x) (~((UINT32_C(1) << (x)) - 1))

#define BITS_BELOW_MASK64(x) ((UINT64_C(1) << (x)) - 1)
#define BITS_ABOVE_MASK64(x) (~((UINT64_C(1) << (x)) - 1))

DECLARE_INSTRUCTION(LDL)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   uint64_t word = 0;
   ADD_TO_PC(1);
   if ((lsaddr & 7) == 0)
   {
     address = lsaddr;
     rdword = (uint64_t*) lsrtp;
     read_dword_in_memory();
   }
   else
   {
     address = lsaddr & UINT32_C(0xFFFFFFF8);
     rdword = &word;
     read_dword_in_memory();
     if (address)
     {
       /* How many low bits do we want to preserve from the old value? */
       uint64_t old_mask = BITS_BELOW_MASK64((lsaddr & 7) * 8);
       /* How many bits up do we want to add the low bits of the new value in? */
       int new_shift = (lsaddr & 7) * 8;
       *lsrtp = (*lsrtp & old_mask) | (word << new_shift);
     }
   }
}

DECLARE_INSTRUCTION(LDR)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   uint64_t word = 0;
   ADD_TO_PC(1);
   address = lsaddr & UINT32_C(0xFFFFFFF8);
   if ((lsaddr & 7) == 7)
   {
     rdword = (uint64_t*) lsrtp;
     read_dword_in_memory();
   }
   else
   {
     rdword = &word;
     read_dword_in_memory();
     if (address)
     {
       /* How many high bits do we want to preserve from the old value? */
       uint64_t old_mask = BITS_ABOVE_MASK64(((lsaddr & 7) + 1) * 8);
       /* How many bits down do we want to add the high bits of the new value in? */
       int new_shift = (7 - (lsaddr & 7)) * 8;
       *lsrtp = (*lsrtp & old_mask) | (word >> new_shift);
     }
   }
}

DECLARE_INSTRUCTION(LB)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   ADD_TO_PC(1);
   address = lsaddr;
   rdword = (uint64_t*) lsrtp;
   read_byte_in_memory();
   if (address)
     *lsrtp = SE8(*lsrtp);
}

DECLARE_INSTRUCTION(LH)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   ADD_TO_PC(1);
   address = lsaddr;
   rdword = (uint64_t*) lsrtp;
   read_hword_in_memory();
   if (address)
     *lsrtp = SE16(*lsrtp);
}

DECLARE_INSTRUCTION(LWL)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   uint64_t word = 0;
   ADD_TO_PC(1);
   if ((lsaddr & 3) == 0)
   {
     address = lsaddr;
     rdword = (uint64_t*) lsrtp;
     read_word_in_memory();
     if (address)
       *lsrtp = SE32(*lsrtp);
   }
   else
   {
     address = lsaddr & UINT32_C(0xFFFFFFFC);
     rdword = &word;
     read_word_in_memory();
     if (address)
     {
       /* How many low bits do we want to preserve from the old value? */
       uint32_t old_mask = BITS_BELOW_MASK32((lsaddr & 3) * 8);
       /* How many bits up do we want to add the low bits of the new value in? */
       int new_shift = (lsaddr & 3) * 8;
       *lsrtp = SE32(((uint32_t) *lsrtp & old_mask) | ((uint32_t) word << new_shift));
     }
   }
}

DECLARE_INSTRUCTION(LW)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   ADD_TO_PC(1);
   address = lsaddr;
   rdword = (uint64_t*) lsrtp;
   read_word_in_memory();
   if (address)
     *lsrtp = SE32(*lsrtp);
}

DECLARE_INSTRUCTION(LBU)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   ADD_TO_PC(1);
   address = lsaddr;
   rdword = (uint64_t*) lsrtp;
   read_byte_in_memory();
}

DECLARE_INSTRUCTION(LHU)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   ADD_TO_PC(1);
   address = lsaddr;
   rdword = (uint64_t*) lsrtp;
   read_hword_in_memory();
}

DECLARE_INSTRUCTION(LWR)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   uint64_t word = 0;
   ADD_TO_PC(1);
   address = lsaddr & UINT32_C(0xFFFFFFFC);
   if ((lsaddr & 3) == 3)
   {
     rdword = (uint64_t*) lsrtp;
     read_word_in_memory();
     if (address)
       *lsrtp = SE32(*lsrtp);
   }
   else
   {
     rdword = &word;
     read_word_in_memory();
     if (address)
     {
       /* How many high bits do we want to preserve from the old value? */
       uint32_t old_mask = BITS_ABOVE_MASK32(((lsaddr & 3) + 1) * 8);
       /* How many bits down do we want to add the new value in? */
       int new_shift = (3 - (lsaddr & 3)) * 8;
       *lsrtp = SE32(((uint32_t) *lsrtp & old_mask) | ((uint32_t) word >> new_shift));
     }
   }
}

DECLARE_INSTRUCTION(LWU)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   ADD_TO_PC(1);
   address = lsaddr;
   rdword = (uint64_t*) lsrtp;
   read_word_in_memory();
}

DECLARE_INSTRUCTION(SB)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   ADD_TO_PC(1);
   address = lsaddr;
   cpu_byte = (uint8_t) *lsrtp;
   write_byte_in_memory();
   CHECK_MEMORY();
}

DECLARE_INSTRUCTION(SH)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   ADD_TO_PC(1);
   address = lsaddr;
   cpu_hword = (uint16_t) *lsrtp;
   write_hword_in_memory();
   CHECK_MEMORY();
}

DECLARE_INSTRUCTION(SWL)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   uint64_t old_word = 0;
   ADD_TO_PC(1);
   if ((lsaddr & 3) == 0)
   {
     address = lsaddr;
     cpu_word = (uint32_t) *lsrtp;
     write_word_in_memory();
     CHECK_MEMORY();
   }
   else
   {
     address = lsaddr & UINT32_C(0xFFFFFFFC);
     rdword = &old_word;
     read_word_in_memory();
     if (address)
     {
       /* How many high bits do we want to preserve from what was in memory
        * before? */
       uint32_t old_mask = BITS_ABOVE_MASK32((4 - (lsaddr & 3)) * 8);
       /* How many bits down do we need to shift the register to store some
        * of its high bits into the low bits of the memory word? */
       int new_shift = (lsaddr & 3) * 8;
       cpu_word = ((uint32_t) old_word & old_mask) | ((uint32_t) *lsrtp >> new_shift);
       write_word_in_memory();
       CHECK_MEMORY();
     }
   }
}

DECLARE_INSTRUCTION(SW)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   ADD_TO_PC(1);
   address = lsaddr;
   cpu_word = (uint32_t) *lsrtp;
   write_word_in_memory();
   CHECK_MEMORY();
}

DECLARE_INSTRUCTION(SDL)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   uint64_t old_word = 0;
   ADD_TO_PC(1);
   if ((lsaddr & 7) == 0)
   {
     address = lsaddr;
     cpu_dword = *lsrtp;
     write_dword_in_memory();
     CHECK_MEMORY();
   }
   else
   {
     address = lsaddr & UINT32_C(0xFFFFFFF8);
     rdword = &old_word;
     read_dword_in_memory();
     if (address)
     {
       /* How many high bits do we want to preserve from what was in memory
        * before? */
       uint64_t old_mask = BITS_ABOVE_MASK64((8 - (lsaddr & 7)) * 8);
       /* How many bits down do we need to shift the register to store some
        * of its high bits into the low bits of the memory word? */
       int new_shift = (lsaddr & 7) * 8;
       cpu_dword = (old_word & old_mask) | ((uint64_t) *lsrtp >> new_shift);
       write_dword_in_memory();
       CHECK_MEMORY();
     }
   }
}

DECLARE_INSTRUCTION(SDR)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   uint64_t old_word = 0;
   ADD_TO_PC(1);
   address = lsaddr & UINT32_C(0xFFFFFFF8);
   if ((lsaddr & 7) == 7)
   {
     cpu_dword = *lsrtp;
     write_dword_in_memory();
     CHECK_MEMORY();
   }
   else
   {
     rdword = &old_word;
     read_dword_in_memory();
     if (address)
     {
       /* How many low bits do we want to preserve from what was in memory
        * before? */
       uint64_t old_mask = BITS_BELOW_MASK64((7 - (lsaddr & 7)) * 8);
       /* How many bits up do we need to shift the register to store some
        * of its low bits into the high bits of the memory word? */
       int new_shift = (7 - (lsaddr & 7)) * 8;
       cpu_dword = (old_word & old_mask) | (*lsrtp << new_shift);
       write_dword_in_memory();
       CHECK_MEMORY();
     }
   }
}

DECLARE_INSTRUCTION(SWR)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   uint64_t old_word = 0;
   ADD_TO_PC(1);
   address = lsaddr & UINT32_C(0xFFFFFFFC);
   if ((lsaddr & 3) == 3)
   {
     cpu_word = (uint32_t) *lsrtp;
     write_word_in_memory();
     CHECK_MEMORY();
   }
   else
   {
     rdword = &old_word;
     read_word_in_memory();
     if (address)
     {
       /* How many low bits do we want to preserve from what was in memory
        * before? */
       int32_t old_mask = BITS_BELOW_MASK32((3 - (lsaddr & 3)) * 8);
       /* How many bits up do we need to shift the register to store some
        * of its low bits into the high bits of the memory word? */
       int new_shift = (3 - (lsaddr & 3)) * 8;
       cpu_word = ((uint32_t) old_word & old_mask) | ((uint32_t) *lsrtp << new_shift);
       write_word_in_memory();
       CHECK_MEMORY();
     }
   }
}

DECLARE_INSTRUCTION(CACHE)
{
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(LL)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   ADD_TO_PC(1);
   address = lsaddr;
   rdword = (uint64_t*) lsrtp;
   read_word_in_memory();
   if (address)
     {
    *lsrtp = SE32(*lsrtp);
    llbit = 1;
     }
}

DECLARE_INSTRUCTION(LWC1)
{
   const unsigned char lslfft = lfft;
   const uint32_t lslfaddr = (uint32_t) reg[lfbase] + lfoffset;
   uint64_t temp;
   if (check_cop1_unusable()) return;
   ADD_TO_PC(1);
   address = lslfaddr;
   rdword = &temp;
   read_word_in_memory();
   if (address)
     *((uint32_t*) reg_cop1_simple[lslfft]) = (uint32_t) *rdword;
}

DECLARE_INSTRUCTION(LDC1)
{
   const unsigned char lslfft = lfft;
   const uint32_t lslfaddr = (uint32_t) reg[lfbase] + lfoffset;
   if (check_cop1_unusable()) return;
   ADD_TO_PC(1);
   address = lslfaddr;
   rdword = (uint64_t*) reg_cop1_double[lslfft];
   read_dword_in_memory();
}

DECLARE_INSTRUCTION(LD)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   ADD_TO_PC(1);
   address = lsaddr;
   rdword = (uint64_t*) lsrtp;
   read_dword_in_memory();
}

DECLARE_INSTRUCTION(SC)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   ADD_TO_PC(1);
   if(llbit)
   {
      address = lsaddr;
      cpu_word = (uint32_t) *lsrtp;
      write_word_in_memory();
      CHECK_MEMORY();
      llbit = 0;
      *lsrtp = 1;
   }
   else
   {
      *lsrtp = 0;
   }
}

DECLARE_INSTRUCTION(SWC1)
{
   const unsigned char lslfft = lfft;
   const uint32_t lslfaddr = (uint32_t) reg[lfbase] + lfoffset;
   if (check_cop1_unusable()) return;
   ADD_TO_PC(1);
   address = lslfaddr;
   cpu_word = *((uint32_t*) reg_cop1_simple[lslfft]);
   write_word_in_memory();
   CHECK_MEMORY();
}

DECLARE_INSTRUCTION(SDC1)
{
   const unsigned char lslfft = lfft;
   const uint32_t lslfaddr = (uint32_t) reg[lfbase] + lfoffset;
   if (check_cop1_unusable()) return;
   ADD_TO_PC(1);
   address = lslfaddr;
   cpu_dword = *((uint64_t*) reg_cop1_double[lslfft]);
   write_dword_in_memory();
   CHECK_MEMORY();
}

DECLARE_INSTRUCTION(SD)
{
   const uint32_t lsaddr = irs32 + iimmediate;
   int64_t *lsrtp = &irt;
   ADD_TO_PC(1);
   address = lsaddr;
   cpu_dword = *lsrtp;
   write_dword_in_memory();
   CHECK_MEMORY();
}

/* COP0 */
DECLARE_INSTRUCTION(MFC0)
{
   switch(rfs)
   {
      case CP0_RANDOM_REG:
        DebugMessage(M64MSG_ERROR, "MFC0 instruction reading un-implemented Random register");
        stop=1;
        break;
      case CP0_COUNT_REG:
        cp0_update_count();
        /* fall through */
      default:
        rrt = SE32(g_cp0_regs[rfs]);
   }
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(MTC0)
{
  switch(rfs)
  {
    case CP0_INDEX_REG:
      g_cp0_regs[CP0_INDEX_REG] = rrt32 & UINT32_C(0x8000003F);
      if ((g_cp0_regs[CP0_INDEX_REG] & UINT32_C(0x3F)) > UINT32_C(31))
      {
        DebugMessage(M64MSG_ERROR, "MTC0 instruction writing Index register with TLB index > 31");
        stop=1;
      }
      break;
    case CP0_RANDOM_REG:
      break;
    case CP0_ENTRYLO0_REG:
      g_cp0_regs[CP0_ENTRYLO0_REG] = rrt32 & UINT32_C(0x3FFFFFFF);
      break;
    case CP0_ENTRYLO1_REG:
      g_cp0_regs[CP0_ENTRYLO1_REG] = rrt32 & UINT32_C(0x3FFFFFFF);
      break;
    case CP0_CONTEXT_REG:
      g_cp0_regs[CP0_CONTEXT_REG] = (rrt32 & UINT32_C(0xFF800000))
                                  | (g_cp0_regs[CP0_CONTEXT_REG] & UINT32_C(0x007FFFF0));
      break;
    case CP0_PAGEMASK_REG:
      g_cp0_regs[CP0_PAGEMASK_REG] = rrt32 & UINT32_C(0x01FFE000);
      break;
    case CP0_WIRED_REG:
      g_cp0_regs[CP0_WIRED_REG] = rrt32;
      g_cp0_regs[CP0_RANDOM_REG] = UINT32_C(31);
      break;
    case CP0_BADVADDR_REG:
      break;
    case CP0_COUNT_REG:
      cp0_update_count();
      interupt_unsafe_state = 1;
      if (next_interupt <= g_cp0_regs[CP0_COUNT_REG]) gen_interupt();
      interupt_unsafe_state = 0;
      translate_event_queue(rrt32);
      g_cp0_regs[CP0_COUNT_REG] = rrt32;
      break;
    case CP0_ENTRYHI_REG:
      g_cp0_regs[CP0_ENTRYHI_REG] = rrt32 & UINT32_C(0xFFFFE0FF);
      break;
    case CP0_COMPARE_REG:
      cp0_update_count();
      remove_event(COMPARE_INT);
      add_interupt_event_count(COMPARE_INT, rrt32);
      g_cp0_regs[CP0_COMPARE_REG] = rrt32;
      g_cp0_regs[CP0_CAUSE_REG] &= UINT32_C(0xFFFF7FFF); //Timer interupt is clear
      break;
    case CP0_STATUS_REG:
      if((rrt32 & UINT32_C(0x04000000)) != (g_cp0_regs[CP0_STATUS_REG] & UINT32_C(0x04000000)))
      {
          shuffle_fpr_data(g_cp0_regs[CP0_STATUS_REG], rrt32);
          set_fpr_pointers(rrt32);
      }
      g_cp0_regs[CP0_STATUS_REG] = rrt32;
      cp0_update_count();
      ADD_TO_PC(1);
      check_interupt();
      interupt_unsafe_state = 1;
      if (next_interupt <= g_cp0_regs[CP0_COUNT_REG]) gen_interupt();
      interupt_unsafe_state = 0;
      ADD_TO_PC(-1);
      break;
    case CP0_CAUSE_REG:
      if (rrt32!=0)
      {
         DebugMessage(M64MSG_ERROR, "MTC0 instruction trying to write Cause register with non-0 value");
         stop = 1;
      }
      else g_cp0_regs[CP0_CAUSE_REG] = rrt32;
      break;
    case CP0_EPC_REG:
      g_cp0_regs[CP0_EPC_REG] = rrt32;
      break;
    case CP0_PREVID_REG:
      break;
    case CP0_CONFIG_REG:
      g_cp0_regs[CP0_CONFIG_REG] = rrt32;
      break;
    case CP0_WATCHLO_REG:
      g_cp0_regs[CP0_WATCHLO_REG] = rrt32;
      break;
    case CP0_WATCHHI_REG:
      g_cp0_regs[CP0_WATCHHI_REG] = rrt32;
      break;
    case CP0_TAGLO_REG:
      g_cp0_regs[CP0_TAGLO_REG] = rrt32 & UINT32_C(0x0FFFFFC0);
      break;
    case CP0_TAGHI_REG:
      g_cp0_regs[CP0_TAGHI_REG] = 0;
      break;
    default:
      DebugMessage(M64MSG_ERROR, "Unknown MTC0 write: %d", rfs);
      stop=1;
  }
  ADD_TO_PC(1);
}

/* COP1 */
DECLARE_JUMP(BC1F,  PCADDR + (iimmediate+1)*4, (FCR31 & FCR31_CMP_BIT)==0, &reg[0], 0, 1)
DECLARE_JUMP(BC1T,  PCADDR + (iimmediate+1)*4, (FCR31 & FCR31_CMP_BIT)!=0, &reg[0], 0, 1)
DECLARE_JUMP(BC1FL, PCADDR + (iimmediate+1)*4, (FCR31 & FCR31_CMP_BIT)==0, &reg[0], 1, 1)
DECLARE_JUMP(BC1TL, PCADDR + (iimmediate+1)*4, (FCR31 & FCR31_CMP_BIT)!=0, &reg[0], 1, 1)

DECLARE_INSTRUCTION(MFC1)
{
   if (check_cop1_unusable()) return;
   rrt = SE32(*((int32_t*) reg_cop1_simple[rfs]));
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DMFC1)
{
   if (check_cop1_unusable()) return;
   rrt = *((int64_t*) reg_cop1_double[rfs]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(CFC1)
{  
   if (check_cop1_unusable()) return;
   if (rfs==31)
   {
      rrt32 = SE32(FCR31);
   }
   if (rfs==0)
   {
      rrt32 = SE32(FCR0);
   }
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(MTC1)
{
   if (check_cop1_unusable()) return;
   *((int32_t*) reg_cop1_simple[rfs]) = rrt32;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DMTC1)
{
   if (check_cop1_unusable()) return;
   *((int64_t*) reg_cop1_double[rfs]) = rrt;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(CTC1)
{
   if (check_cop1_unusable()) return;
   if (rfs==31)
   {
      FCR31 = rrt32;
      update_x86_rounding_mode(rrt32);
   }
   //if ((FCR31 >> 7) & 0x1F) printf("FPU Exception enabled : %x\n",
   //                 (int)((FCR31 >> 7) & 0x1F));
   ADD_TO_PC(1);
}

// COP1_D
DECLARE_INSTRUCTION(ADD_D)
{
   if (check_cop1_unusable()) return;
   add_d(reg_cop1_double[cffs], reg_cop1_double[cfft], reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(SUB_D)
{
   if (check_cop1_unusable()) return;
   sub_d(reg_cop1_double[cffs], reg_cop1_double[cfft], reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(MUL_D)
{
   if (check_cop1_unusable()) return;
   mul_d(reg_cop1_double[cffs], reg_cop1_double[cfft], reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DIV_D)
{
   if (check_cop1_unusable()) return;
   if((FCR31 & UINT32_C(0x400)) && *reg_cop1_double[cfft] == 0)
   {
      //FCR31 |= 0x8020;
      /*FCR31 |= 0x8000;
      Cause = 15 << 2;
      exception_general();*/
      DebugMessage(M64MSG_ERROR, "DIV_D by 0");
      //return;
   }
   div_d(reg_cop1_double[cffs], reg_cop1_double[cfft], reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(SQRT_D)
{
   if (check_cop1_unusable()) return;
   sqrt_d(reg_cop1_double[cffs], reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(ABS_D)
{
   if (check_cop1_unusable()) return;
   abs_d(reg_cop1_double[cffs], reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(MOV_D)
{
   if (check_cop1_unusable()) return;
   mov_d(reg_cop1_double[cffs], reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(NEG_D)
{
   if (check_cop1_unusable()) return;
   neg_d(reg_cop1_double[cffs], reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(ROUND_L_D)
{
   if (check_cop1_unusable()) return;
   round_l_d(reg_cop1_double[cffs], (int64_t*) reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(TRUNC_L_D)
{
   if (check_cop1_unusable()) return;
   trunc_l_d(reg_cop1_double[cffs], (int64_t*) reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(CEIL_L_D)
{
   if (check_cop1_unusable()) return;
   ceil_l_d(reg_cop1_double[cffs], (int64_t*) reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(FLOOR_L_D)
{
   if (check_cop1_unusable()) return;
   floor_l_d(reg_cop1_double[cffs], (int64_t*) reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(ROUND_W_D)
{
   if (check_cop1_unusable()) return;
   round_w_d(reg_cop1_double[cffs], (int32_t*) reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(TRUNC_W_D)
{
   if (check_cop1_unusable()) return;
   trunc_w_d(reg_cop1_double[cffs], (int32_t*) reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(CEIL_W_D)
{
   if (check_cop1_unusable()) return;
   ceil_w_d(reg_cop1_double[cffs], (int32_t*) reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(FLOOR_W_D)
{
   if (check_cop1_unusable()) return;
   floor_w_d(reg_cop1_double[cffs], (int32_t*) reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(CVT_S_D)
{
   if (check_cop1_unusable()) return;
   cvt_s_d(reg_cop1_double[cffs], reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(CVT_W_D)
{
   if (check_cop1_unusable()) return;
   cvt_w_d(reg_cop1_double[cffs], (int32_t*) reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(CVT_L_D)
{
   if (check_cop1_unusable()) return;
   cvt_l_d(reg_cop1_double[cffs], (int64_t*) reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_F_D)
{
   if (check_cop1_unusable()) return;
   c_f_d();
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_UN_D)
{
   if (check_cop1_unusable()) return;
   c_un_d(reg_cop1_double[cffs], reg_cop1_double[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_EQ_D)
{
   if (check_cop1_unusable()) return;
   c_eq_d(reg_cop1_double[cffs], reg_cop1_double[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_UEQ_D)
{
   if (check_cop1_unusable()) return;
   c_ueq_d(reg_cop1_double[cffs], reg_cop1_double[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_OLT_D)
{
   if (check_cop1_unusable()) return;
   c_olt_d(reg_cop1_double[cffs], reg_cop1_double[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_ULT_D)
{
   if (check_cop1_unusable()) return;
   c_ult_d(reg_cop1_double[cffs], reg_cop1_double[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_OLE_D)
{
   if (check_cop1_unusable()) return;
   c_ole_d(reg_cop1_double[cffs], reg_cop1_double[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_ULE_D)
{
   if (check_cop1_unusable()) return;
   c_ule_d(reg_cop1_double[cffs], reg_cop1_double[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_SF_D)
{
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]))
   {
     DebugMessage(M64MSG_ERROR, "Invalid operation exception in C opcode");
     stop=1;
   }
   c_sf_d(reg_cop1_double[cffs], reg_cop1_double[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_NGLE_D)
{
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]))
   {
     DebugMessage(M64MSG_ERROR, "Invalid operation exception in C opcode");
     stop=1;
   }
   c_ngle_d(reg_cop1_double[cffs], reg_cop1_double[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_SEQ_D)
{
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]))
   {
     DebugMessage(M64MSG_ERROR, "Invalid operation exception in C opcode");
     stop=1;
   }
   c_seq_d(reg_cop1_double[cffs], reg_cop1_double[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_NGL_D)
{
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]))
   {
     DebugMessage(M64MSG_ERROR, "Invalid operation exception in C opcode");
     stop=1;
   }
   c_ngl_d(reg_cop1_double[cffs], reg_cop1_double[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_LT_D)
{
   if (check_cop1_unusable()) return;
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]))
   {
     DebugMessage(M64MSG_ERROR, "Invalid operation exception in C opcode");
     stop=1;
   }
   c_lt_d(reg_cop1_double[cffs], reg_cop1_double[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_NGE_D)
{
   if (check_cop1_unusable()) return;
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]))
   {
     DebugMessage(M64MSG_ERROR, "Invalid operation exception in C opcode");
     stop=1;
   }
   c_nge_d(reg_cop1_double[cffs], reg_cop1_double[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_LE_D)
{
   if (check_cop1_unusable()) return;
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]))
   {
     DebugMessage(M64MSG_ERROR, "Invalid operation exception in C opcode");
     stop=1;
   }
   c_le_d(reg_cop1_double[cffs], reg_cop1_double[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_NGT_D)
{
   if (check_cop1_unusable()) return;
   if (isnan(*reg_cop1_double[cffs]) || isnan(*reg_cop1_double[cfft]))
   {
     DebugMessage(M64MSG_ERROR, "Invalid operation exception in C opcode");
     stop=1;
   }
   c_ngt_d(reg_cop1_double[cffs], reg_cop1_double[cfft]);
   ADD_TO_PC(1);
}

// COP1_L
DECLARE_INSTRUCTION(CVT_S_L)
{
   if (check_cop1_unusable()) return;
   cvt_s_l((int64_t*) reg_cop1_double[cffs], reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(CVT_D_L)
{
   if (check_cop1_unusable()) return;
   cvt_d_l((int64_t*) reg_cop1_double[cffs], reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

// COP1_S
DECLARE_INSTRUCTION(ADD_S)
{
   if (check_cop1_unusable()) return;
   add_s(reg_cop1_simple[cffs], reg_cop1_simple[cfft], reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(SUB_S)
{
   if (check_cop1_unusable()) return;
   sub_s(reg_cop1_simple[cffs], reg_cop1_simple[cfft], reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(MUL_S)
{
   if (check_cop1_unusable()) return;
   mul_s(reg_cop1_simple[cffs], reg_cop1_simple[cfft], reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DIV_S)
{  
   if (check_cop1_unusable()) return;
   if((FCR31 & UINT32_C(0x400)) && *reg_cop1_simple[cfft] == 0)
   {
     DebugMessage(M64MSG_ERROR, "DIV_S by 0");
   }
   div_s(reg_cop1_simple[cffs], reg_cop1_simple[cfft], reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(SQRT_S)
{
   if (check_cop1_unusable()) return;
   sqrt_s(reg_cop1_simple[cffs], reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(ABS_S)
{
   if (check_cop1_unusable()) return;
   abs_s(reg_cop1_simple[cffs], reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(MOV_S)
{
   if (check_cop1_unusable()) return;
   mov_s(reg_cop1_simple[cffs], reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(NEG_S)
{
   if (check_cop1_unusable()) return;
   neg_s(reg_cop1_simple[cffs], reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(ROUND_L_S)
{
   if (check_cop1_unusable()) return;
   round_l_s(reg_cop1_simple[cffs], (int64_t*) reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(TRUNC_L_S)
{
   if (check_cop1_unusable()) return;
   trunc_l_s(reg_cop1_simple[cffs], (int64_t*) reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(CEIL_L_S)
{
   if (check_cop1_unusable()) return;
   ceil_l_s(reg_cop1_simple[cffs], (int64_t*) reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(FLOOR_L_S)
{
   if (check_cop1_unusable()) return;
   floor_l_s(reg_cop1_simple[cffs], (int64_t*) reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(ROUND_W_S)
{
   if (check_cop1_unusable()) return;
   round_w_s(reg_cop1_simple[cffs], (int32_t*) reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(TRUNC_W_S)
{
   if (check_cop1_unusable()) return;
   trunc_w_s(reg_cop1_simple[cffs], (int32_t*) reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(CEIL_W_S)
{
   if (check_cop1_unusable()) return;
   ceil_w_s(reg_cop1_simple[cffs], (int32_t*) reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(FLOOR_W_S)
{
   if (check_cop1_unusable()) return;
   floor_w_s(reg_cop1_simple[cffs], (int32_t*) reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(CVT_D_S)
{
   if (check_cop1_unusable()) return;
   cvt_d_s(reg_cop1_simple[cffs], reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(CVT_W_S)
{
   if (check_cop1_unusable()) return;
   cvt_w_s(reg_cop1_simple[cffs], (int32_t*) reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(CVT_L_S)
{
   if (check_cop1_unusable()) return;
   cvt_l_s(reg_cop1_simple[cffs], (int64_t*) reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_F_S)
{
   if (check_cop1_unusable()) return;
   c_f_s();
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_UN_S)
{
   if (check_cop1_unusable()) return;
   c_un_s(reg_cop1_simple[cffs], reg_cop1_simple[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_EQ_S)
{
   if (check_cop1_unusable()) return;
   c_eq_s(reg_cop1_simple[cffs], reg_cop1_simple[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_UEQ_S)
{
   if (check_cop1_unusable()) return;
   c_ueq_s(reg_cop1_simple[cffs], reg_cop1_simple[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_OLT_S)
{
   if (check_cop1_unusable()) return;
   c_olt_s(reg_cop1_simple[cffs], reg_cop1_simple[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_ULT_S)
{
   if (check_cop1_unusable()) return;
   c_ult_s(reg_cop1_simple[cffs], reg_cop1_simple[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_OLE_S)
{
   if (check_cop1_unusable()) return;
   c_ole_s(reg_cop1_simple[cffs], reg_cop1_simple[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_ULE_S)
{
   if (check_cop1_unusable()) return;
   c_ule_s(reg_cop1_simple[cffs], reg_cop1_simple[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_SF_S)
{
   if (check_cop1_unusable()) return;
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]))
   {
     DebugMessage(M64MSG_ERROR, "Invalid operation exception in C opcode");
     stop=1;
   }
   c_sf_s(reg_cop1_simple[cffs], reg_cop1_simple[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_NGLE_S)
{
   if (check_cop1_unusable()) return;
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]))
   {
     DebugMessage(M64MSG_ERROR, "Invalid operation exception in C opcode");
     stop=1;
   }
   c_ngle_s(reg_cop1_simple[cffs], reg_cop1_simple[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_SEQ_S)
{
   if (check_cop1_unusable()) return;
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]))
   {
     DebugMessage(M64MSG_ERROR, "Invalid operation exception in C opcode");
     stop=1;
   }
   c_seq_s(reg_cop1_simple[cffs], reg_cop1_simple[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_NGL_S)
{
   if (check_cop1_unusable()) return;
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]))
   {
     DebugMessage(M64MSG_ERROR, "Invalid operation exception in C opcode");
     stop=1;
   }
   c_ngl_s(reg_cop1_simple[cffs], reg_cop1_simple[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_LT_S)
{
   if (check_cop1_unusable()) return;
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]))
   {
     DebugMessage(M64MSG_ERROR, "Invalid operation exception in C opcode");
     stop=1;
   }
   c_lt_s(reg_cop1_simple[cffs], reg_cop1_simple[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_NGE_S)
{
   if (check_cop1_unusable()) return;
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]))
   {
     DebugMessage(M64MSG_ERROR, "Invalid operation exception in C opcode");
     stop=1;
   }
   c_nge_s(reg_cop1_simple[cffs], reg_cop1_simple[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_LE_S)
{
   if (check_cop1_unusable()) return;
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]))
   {
     DebugMessage(M64MSG_ERROR, "Invalid operation exception in C opcode");
     stop=1;
   }
   c_le_s(reg_cop1_simple[cffs], reg_cop1_simple[cfft]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(C_NGT_S)
{
   if (check_cop1_unusable()) return;
   if (isnan(*reg_cop1_simple[cffs]) || isnan(*reg_cop1_simple[cfft]))
   {
     DebugMessage(M64MSG_ERROR, "Invalid operation exception in C opcode");
     stop=1;
   }
   c_ngt_s(reg_cop1_simple[cffs], reg_cop1_simple[cfft]);
   ADD_TO_PC(1);
}

// COP1_W
DECLARE_INSTRUCTION(CVT_S_W)
{  
   if (check_cop1_unusable()) return;
   cvt_s_w((int32_t*) reg_cop1_simple[cffs], reg_cop1_simple[cffd]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(CVT_D_W)
{
   if (check_cop1_unusable()) return;
   cvt_d_w((int32_t*) reg_cop1_simple[cffs], reg_cop1_double[cffd]);
   ADD_TO_PC(1);
}

/* regimm */
DECLARE_JUMP(BLTZ,    PCADDR + (iimmediate+1)*4, irs < 0,    &reg[0],  0, 0)
DECLARE_JUMP(BGEZ,    PCADDR + (iimmediate+1)*4, irs >= 0,   &reg[0],  0, 0)
DECLARE_JUMP(BLTZL,   PCADDR + (iimmediate+1)*4, irs < 0,    &reg[0],  1, 0)
DECLARE_JUMP(BGEZL,   PCADDR + (iimmediate+1)*4, irs >= 0,   &reg[0],  1, 0)
DECLARE_JUMP(BLTZAL,  PCADDR + (iimmediate+1)*4, irs < 0,    &reg[31], 0, 0)
DECLARE_JUMP(BGEZAL,  PCADDR + (iimmediate+1)*4, irs >= 0,   &reg[31], 0, 0)
DECLARE_JUMP(BLTZALL, PCADDR + (iimmediate+1)*4, irs < 0,    &reg[31], 1, 0)
DECLARE_JUMP(BGEZALL, PCADDR + (iimmediate+1)*4, irs >= 0,   &reg[31], 1, 0)

/* special */
DECLARE_INSTRUCTION(NOP)
{
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(SLL)
{
   rrd = SE32((uint32_t) rrt32 << rsa);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(SRL)
{
   rrd = SE32((uint32_t) rrt32 >> rsa);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(SRA)
{
   rrd = SE32((int32_t) rrt32 >> rsa);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(SLLV)
{
   rrd = SE32((uint32_t) rrt32 << (rrs32 & 0x1F));
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(SRLV)
{
   rrd = SE32((uint32_t) rrt32 >> (rrs32 & 0x1F));
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(SRAV)
{
   rrd = SE32((int32_t) rrt32 >> (rrs32 & 0x1F));
   ADD_TO_PC(1);
}

DECLARE_JUMP(JR,   irs32, 1, &reg[0], 0, 0)
DECLARE_JUMP(JALR, irs32, 1, &rrd,    0, 0)

DECLARE_INSTRUCTION(SYSCALL)
{
   g_cp0_regs[CP0_CAUSE_REG] = UINT32_C(8) << 2;
   exception_general();
}

DECLARE_INSTRUCTION(SYNC)
{
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(MFHI)
{
   rrd = hi;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(MTHI)
{
   hi = rrs;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(MFLO)
{
   rrd = lo;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(MTLO)
{
   lo = rrs;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DSLLV)
{
   rrd = rrt << (rrs32 & 0x3F);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DSRLV)
{
   rrd = (uint64_t) rrt >> (rrs32 & 0x3F);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DSRAV)
{
   rrd = (int64_t) rrt >> (rrs32 & 0x3F);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(MULT)
{
   int64_t temp;
   temp = rrs * rrt;
   hi = temp >> 32;
   lo = SE32(temp);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(MULTU)
{
   uint64_t temp;
   temp = (uint32_t) rrs * (uint64_t) ((uint32_t) rrt);
   hi = (int64_t) temp >> 32;
   lo = SE32(temp);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DIV)
{
   if (rrt32)
   {
     lo = SE32(rrs32 / rrt32);
     hi = SE32(rrs32 % rrt32);
   }
   else DebugMessage(M64MSG_ERROR, "DIV: divide by 0");
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DIVU)
{
   if (rrt32)
   {
     lo = SE32((uint32_t) rrs32 / (uint32_t) rrt32);
     hi = SE32((uint32_t) rrs32 % (uint32_t) rrt32);
   }
   else DebugMessage(M64MSG_ERROR, "DIVU: divide by 0");
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DMULT)
{
   uint64_t op1, op2, op3, op4;
   uint64_t result1, result2, result3, result4;
   uint64_t temp1, temp2, temp3, temp4;
   int sign = 0;
   
   if (rrs < 0)
     {
    op2 = -rrs;
    sign = 1 - sign;
     }
   else op2 = rrs;
   if (rrt < 0)
     {
    op4 = -rrt;
    sign = 1 - sign;
     }
   else op4 = rrt;
   
   op1 = op2 & UINT64_C(0xFFFFFFFF);
   op2 = (op2 >> 32) & UINT64_C(0xFFFFFFFF);
   op3 = op4 & UINT64_C(0xFFFFFFFF);
   op4 = (op4 >> 32) & UINT64_C(0xFFFFFFFF);
   
   temp1 = op1 * op3;
   temp2 = (temp1 >> 32) + op1 * op4;
   temp3 = op2 * op3;
   temp4 = (temp3 >> 32) + op2 * op4;
   
   result1 = temp1 & UINT64_C(0xFFFFFFFF);
   result2 = temp2 + (temp3 & UINT64_C(0xFFFFFFFF));
   result3 = (result2 >> 32) + temp4;
   result4 = (result3 >> 32);
   
   lo = result1 | (result2 << 32);
   hi = (result3 & UINT64_C(0xFFFFFFFF)) | (result4 << 32);
   if (sign)
     {
    hi = ~hi;
    if (!lo) hi++;
    else lo = ~lo + 1;
     }
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DMULTU)
{
   uint64_t op1, op2, op3, op4;
   uint64_t result1, result2, result3, result4;
   uint64_t temp1, temp2, temp3, temp4;
   
   op1 = rrs & UINT64_C(0xFFFFFFFF);
   op2 = (rrs >> 32) & UINT64_C(0xFFFFFFFF);
   op3 = rrt & UINT64_C(0xFFFFFFFF);
   op4 = (rrt >> 32) & UINT64_C(0xFFFFFFFF);
   
   temp1 = op1 * op3;
   temp2 = (temp1 >> 32) + op1 * op4;
   temp3 = op2 * op3;
   temp4 = (temp3 >> 32) + op2 * op4;
   
   result1 = temp1 & UINT64_C(0xFFFFFFFF);
   result2 = temp2 + (temp3 & UINT64_C(0xFFFFFFFF));
   result3 = (result2 >> 32) + temp4;
   result4 = (result3 >> 32);
   
   lo = result1 | (result2 << 32);
   hi = (result3 & UINT64_C(0xFFFFFFFF)) | (result4 << 32);
   
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DDIV)
{
   if (rrt)
   {
     lo = rrs / rrt;
     hi = rrs % rrt;
   }
   else DebugMessage(M64MSG_ERROR, "DDIV: divide by 0");
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DDIVU)
{
   if (rrt)
   {
     lo = (uint64_t) rrs / (uint64_t) rrt;
     hi = (uint64_t) rrs % (uint64_t) rrt;
   }
   else DebugMessage(M64MSG_ERROR, "DDIVU: divide by 0");
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(ADD)
{
   rrd = SE32(rrs32 + rrt32);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(ADDU)
{
   rrd = SE32(rrs32 + rrt32);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(SUB)
{
   rrd = SE32(rrs32 - rrt32);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(SUBU)
{
   rrd = SE32(rrs32 - rrt32);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(AND)
{
   rrd = rrs & rrt;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(OR)
{
   rrd = rrs | rrt;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(XOR)
{
   rrd = rrs ^ rrt;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(NOR)
{
   rrd = ~(rrs | rrt);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(SLT)
{
   if (rrs < rrt) rrd = 1;
   else rrd = 0;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(SLTU)
{
   if ((uint64_t) rrs < (uint64_t) rrt)
     rrd = 1;
   else rrd = 0;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DADD)
{
   rrd = rrs + rrt;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DADDU)
{
   rrd = rrs + rrt;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DSUB)
{
   rrd = rrs - rrt;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DSUBU)
{
   rrd = rrs - rrt;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(TEQ)
{
   if (rrs == rrt)
   {
     DebugMessage(M64MSG_ERROR, "trap exception in TEQ");
     stop=1;
   }
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DSLL)
{
   rrd = rrt << rsa;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DSRL)
{
   rrd = (uint64_t) rrt >> rsa;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DSRA)
{
   rrd = rrt >> rsa;
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DSLL32)
{
   rrd = rrt << (32 + rsa);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DSRL32)
{
   rrd = (uint64_t) rrt >> (32 + rsa);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(DSRA32)
{
   rrd = (int64_t) rrt >> (32 + rsa);
   ADD_TO_PC(1);
}

/* TLB */
#include <encodings/crc32.h>

DECLARE_INSTRUCTION(TLBR)
{
   int index;
   index = g_cp0_regs[CP0_INDEX_REG] & UINT32_C(0x1F);
   g_cp0_regs[CP0_PAGEMASK_REG] = tlb_e[index].mask << 13;
   g_cp0_regs[CP0_ENTRYHI_REG] = ((tlb_e[index].vpn2 << 13) | tlb_e[index].asid);
   g_cp0_regs[CP0_ENTRYLO0_REG] = (tlb_e[index].pfn_even << 6) | (tlb_e[index].c_even << 3)
     | (tlb_e[index].d_even << 2) | (tlb_e[index].v_even << 1)
       | tlb_e[index].g;
   g_cp0_regs[CP0_ENTRYLO1_REG] = (tlb_e[index].pfn_odd << 6) | (tlb_e[index].c_odd << 3)
     | (tlb_e[index].d_odd << 2) | (tlb_e[index].v_odd << 1)
       | tlb_e[index].g;
   ADD_TO_PC(1);
}

static void TLBWrite(unsigned int idx)
{
   if (r4300emu != CORE_PURE_INTERPRETER)
   {
      unsigned int i;
      if (tlb_e[idx].v_even)
      {
         for (i=tlb_e[idx].start_even>>12; i<=tlb_e[idx].end_even>>12; i++)
         {
            if(!invalid_code[i] &&(invalid_code[tlb_LUT_r[i]>>12] ||
               invalid_code[(tlb_LUT_r[i]>>12)+0x20000]))
               invalid_code[i] = 1;
            if (!invalid_code[i])
            {
                /*int j;
                md5_state_t state;
                md5_byte_t digest[16];
                md5_init(&state);
                md5_append(&state, 
                       (const md5_byte_t*)&g_rdram[(tlb_LUT_r[i]&0x7FF000)/4],
                       0x1000);
                md5_finish(&state, digest);
                for (j=0; j<16; j++) blocks[i]->md5[j] = digest[j];*/
                
                blocks[i]->adler32 = encoding_crc32(0, (void*)&g_rdram[(tlb_LUT_r[i]&0x7FF000)/4], 0x1000);
                
                invalid_code[i] = 1;
            }
            else if (blocks[i])
            {
               /*int j;
                for (j=0; j<16; j++) blocks[i]->md5[j] = 0;*/
               blocks[i]->adler32 = 0;
            }
         }
      }
      if (tlb_e[idx].v_odd)
      {
         for (i=tlb_e[idx].start_odd>>12; i<=tlb_e[idx].end_odd>>12; i++)
         {
            if(!invalid_code[i] &&(invalid_code[tlb_LUT_r[i]>>12] ||
               invalid_code[(tlb_LUT_r[i]>>12)+0x20000]))
               invalid_code[i] = 1;
            if (!invalid_code[i])
            {
               /*int j;
               md5_state_t state;
               md5_byte_t digest[16];
               md5_init(&state);
               md5_append(&state, 
                      (const md5_byte_t*)&g_rdram[(tlb_LUT_r[i]&0x7FF000)/4],
                      0x1000);
               md5_finish(&state, digest);
               for (j=0; j<16; j++) blocks[i]->md5[j] = digest[j];*/
                
               blocks[i]->adler32 = encoding_crc32(0, (void*)&g_rdram[(tlb_LUT_r[i]&0x7FF000)/4], 0x1000);
                
               invalid_code[i] = 1;
            }
            else if (blocks[i])
            {
               /*int j;
               for (j=0; j<16; j++) blocks[i]->md5[j] = 0;*/
               blocks[i]->adler32 = 0;
            }
         }
      }
   }

   tlb_unmap(&tlb_e[idx]);

   tlb_e[idx].g = (g_cp0_regs[CP0_ENTRYLO0_REG] & g_cp0_regs[CP0_ENTRYLO1_REG] & 1);
   tlb_e[idx].pfn_even = (g_cp0_regs[CP0_ENTRYLO0_REG] & UINT32_C(0x3FFFFFC0)) >> 6;
   tlb_e[idx].pfn_odd = (g_cp0_regs[CP0_ENTRYLO1_REG] & UINT32_C(0x3FFFFFC0)) >> 6;
   tlb_e[idx].c_even = (g_cp0_regs[CP0_ENTRYLO0_REG] & UINT32_C(0x38)) >> 3;
   tlb_e[idx].c_odd = (g_cp0_regs[CP0_ENTRYLO1_REG] & UINT32_C(0x38)) >> 3;
   tlb_e[idx].d_even = (g_cp0_regs[CP0_ENTRYLO0_REG] & UINT32_C(0x4)) >> 2;
   tlb_e[idx].d_odd = (g_cp0_regs[CP0_ENTRYLO1_REG] & UINT32_C(0x4)) >> 2;
   tlb_e[idx].v_even = (g_cp0_regs[CP0_ENTRYLO0_REG] & UINT32_C(0x2)) >> 1;
   tlb_e[idx].v_odd = (g_cp0_regs[CP0_ENTRYLO1_REG] & UINT32_C(0x2)) >> 1;
   tlb_e[idx].asid = (g_cp0_regs[CP0_ENTRYHI_REG] & UINT32_C(0xFF));
   tlb_e[idx].vpn2 = (g_cp0_regs[CP0_ENTRYHI_REG] & UINT32_C(0xFFFFE000)) >> 13;
   //tlb_e[idx].r = (g_cp0_regs[CP0_ENTRYHI_REG] & 0xC000000000000000LL) >> 62;
   tlb_e[idx].mask = (g_cp0_regs[CP0_PAGEMASK_REG] & UINT32_C(0x1FFE000)) >> 13;
   
   tlb_e[idx].start_even = tlb_e[idx].vpn2 << 13;
   tlb_e[idx].end_even = tlb_e[idx].start_even+
     (tlb_e[idx].mask << 12) + UINT32_C(0xFFF);
   tlb_e[idx].phys_even = tlb_e[idx].pfn_even << 12;
   

   tlb_e[idx].start_odd = tlb_e[idx].end_even+1;
   tlb_e[idx].end_odd = tlb_e[idx].start_odd+
     (tlb_e[idx].mask << 12) + UINT32_C(0xFFF);
   tlb_e[idx].phys_odd = tlb_e[idx].pfn_odd << 12;
   
   tlb_map(&tlb_e[idx]);

   if (r4300emu != CORE_PURE_INTERPRETER)
   {
      unsigned int i;
      if (tlb_e[idx].v_even)
      {    
         for (i=tlb_e[idx].start_even>>12; i<=tlb_e[idx].end_even>>12; i++)
         {
            /*if (blocks[i] && (blocks[i]->md5[0] || blocks[i]->md5[1] ||
                  blocks[i]->md5[2] || blocks[i]->md5[3]))
            {
               int j;
               int equal = 1;
               md5_state_t state;
               md5_byte_t digest[16];
               md5_init(&state);
               md5_append(&state, 
                  (const md5_byte_t*)&g_rdram[(tlb_LUT_r[i]&0x7FF000)/4],
                  0x1000);
               md5_finish(&state, digest);
               for (j=0; j<16; j++)
                 if (digest[j] != blocks[i]->md5[j])
                   equal = 0;
               if (equal) invalid_code[i] = 0;
               }*/
               if(blocks[i] && blocks[i]->adler32)
               {
                  if(blocks[i]->adler32 == encoding_crc32(0,(void*)&g_rdram[(tlb_LUT_r[i]&0x7FF000)/4],0x1000))
                     invalid_code[i] = 0;
               }
         }
      }

      if (tlb_e[idx].v_odd)
      {    
         for (i=tlb_e[idx].start_odd>>12; i<=tlb_e[idx].end_odd>>12; i++)
         {
            /*if (blocks[i] && (blocks[i]->md5[0] || blocks[i]->md5[1] ||
                  blocks[i]->md5[2] || blocks[i]->md5[3]))
              {
            int j;
            int equal = 1;
            md5_state_t state;
            md5_byte_t digest[16];
            md5_init(&state);
            md5_append(&state, 
                   (const md5_byte_t*)&g_rdram[(tlb_LUT_r[i]&0x7FF000)/4],
                   0x1000);
            md5_finish(&state, digest);
            for (j=0; j<16; j++)
              if (digest[j] != blocks[i]->md5[j])
                equal = 0;
            if (equal) invalid_code[i] = 0;
            }*/
            if(blocks[i] && blocks[i]->adler32)
            {
               if(blocks[i]->adler32 == encoding_crc32(0,(void*)&g_rdram[(tlb_LUT_r[i]&0x7FF000)/4],0x1000))
                  invalid_code[i] = 0;
            }
         }
      }
   }
}

DECLARE_INSTRUCTION(TLBWI)
{
   TLBWrite(g_cp0_regs[CP0_INDEX_REG] & UINT32_C(0x3F));
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(TLBWR)
{
   cp0_update_count();
   g_cp0_regs[CP0_RANDOM_REG] = (g_cp0_regs[CP0_COUNT_REG]/2 % (32 - g_cp0_regs[CP0_WIRED_REG]))
                              + g_cp0_regs[CP0_WIRED_REG];
   TLBWrite(g_cp0_regs[CP0_RANDOM_REG]);
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(TLBP)
{
   int i;
   g_cp0_regs[CP0_INDEX_REG] |= UINT32_C(0x80000000);
   for (i=0; i<32; i++)
   {
      if (((tlb_e[i].vpn2 & (~tlb_e[i].mask)) ==
         (((g_cp0_regs[CP0_ENTRYHI_REG] & UINT32_C(0xFFFFE000)) >> 13) & (~tlb_e[i].mask))) &&
        ((tlb_e[i].g) ||
         (tlb_e[i].asid == (g_cp0_regs[CP0_ENTRYHI_REG] & UINT32_C(0xFF)))))
      {
         g_cp0_regs[CP0_INDEX_REG] = i;
         break;
      }
   }
   ADD_TO_PC(1);
}

DECLARE_INSTRUCTION(ERET)
{
   cp0_update_count();
   if (g_cp0_regs[CP0_STATUS_REG] & UINT32_C(0x4))
   {
     DebugMessage(M64MSG_ERROR, "error in ERET");
     stop=1;
   }
   else
   {
     g_cp0_regs[CP0_STATUS_REG] &= ~UINT32_C(0x2);
     generic_jump_to(g_cp0_regs[CP0_EPC_REG]);
   }
   llbit = 0;
   check_interupt();
   last_addr = PCADDR;
   if (next_interupt <= g_cp0_regs[CP0_COUNT_REG]) gen_interupt();
}

