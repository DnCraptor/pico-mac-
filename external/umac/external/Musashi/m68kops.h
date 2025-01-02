#ifndef M68KOPS__HEADER
#define M68KOPS__HEADER

/* ======================================================================== */
/* ============================ OPCODE HANDLERS =========================== */
/* ======================================================================== */


#if M68K_DYNAMIC_INSTR_TABLES
/* Build the opcode handler table */
void m68ki_build_opcode_table(void);

extern void (*m68ki_instruction_jump_table[0x10000])(void); /* opcode handler jump table */
#if M68K_CYCLE_COUNTING
extern unsigned char m68ki_cycles[][0x10000];
#endif
#else
/* Opcode handler table is already built at compile time */
static inline void m68ki_build_opcode_table(void) {}
extern void (*const m68ki_static_instruction_jump_table[0x10000])(void);

#if M68K_CYCLE_COUNTING
/* To support cycle counting in a static-tables scenario, extend
 * m68kmake to generate a static table.
 */
#error "CYCLE_COUNTING is only supported with DYNAMIC_INSTR_TABLES"
#endif
#endif

/* ======================================================================== */
/* ============================== END OF FILE ============================= */
/* ======================================================================== */

#endif /* M68KOPS__HEADER */


