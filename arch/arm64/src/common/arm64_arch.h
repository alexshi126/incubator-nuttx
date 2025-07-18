/****************************************************************************
 * arch/arm64/src/common/arm64_arch.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#ifndef ___ARCH_ARM64_SRC_COMMON_ARM64_ARCH_H
#define ___ARCH_ARM64_SRC_COMMON_ARM64_ARCH_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <arch/arch.h>

/* Unsigned integer with bit position n set (signed in
 * assembly language).
 */
#ifndef __ASSEMBLY__
  #include <stdint.h>
  #include <nuttx/nuttx.h>
#endif

#include <sys/param.h>
#include <nuttx/bits.h>

#include <arch/barriers.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BIT64(n)        ((1ULL) << (n))

/* Bit mask with bits 0 through n-1 (inclusive) set,
 * or 0 if n is 0.
 */
#define BIT_MASK(n)     (BIT(n) - 1)
#define BIT64_MASK(n)   (BIT64(n) - 1ULL)

#define DAIFSET_FIQ_BIT     BIT(0)
#define DAIFSET_IRQ_BIT     BIT(1)
#define DAIFSET_ABT_BIT     BIT(2)
#define DAIFSET_DBG_BIT     BIT(3)

#define DAIFCLR_FIQ_BIT     BIT(0)
#define DAIFCLR_IRQ_BIT     BIT(1)
#define DAIFCLR_ABT_BIT     BIT(2)
#define DAIFCLR_DBG_BIT     BIT(3)

#define DAIF_FIQ_BIT        BIT(6)
#define DAIF_IRQ_BIT        BIT(7)
#define DAIF_ABT_BIT        BIT(8)
#define DAIF_DBG_BIT        BIT(9)

#define DAIF_MASK           (0xf << 6)

/* Arm® Architecture Registers Armv8, for Armv8-A architecture profile
 * ( DDI 0595, ID121321 ), defined:
 *
 * SCTLR_EL1: System Control Register (EL1)
 * SCTLR_EL2: System Control Register (EL2)
 * SCTLR_EL3: System Control Register (EL3)
 *
 */

#define SCTLR_EL3_RES1      (BIT(29) | BIT(28) | BIT(23) | \
                             BIT(22) | BIT(18) | BIT(16) | \
                             BIT(11) | BIT(5)  | BIT(4))

#define SCTLR_EL2_RES1      (BIT(29) | BIT(28) | BIT(23) | \
                             BIT(22) | BIT(18) | BIT(16) | \
                             BIT(11) | BIT(5)  | BIT(4))

#define SCTLR_EL1_RES1      (BIT(29) | BIT(28) | BIT(23) | \
                             BIT(22) | BIT(20) | BIT(11))

#define SCTLR_M_BIT         BIT(0)
#define SCTLR_A_BIT         BIT(1)
#define SCTLR_C_BIT         BIT(2)
#define SCTLR_SA_BIT        BIT(3)
#define SCTLR_I_BIT         BIT(12)

/* Controls the impact of tag check faults
 * due to loads and stores in EL0 EL1.
 */

#define SCTLR_TCF0_BIT       BIT(38)
#define SCTLR_TCF1_BIT       BIT(40)

/* Controlling EL0 EL1 access to assigned tags */

#define SCTLR_ATA0_BIT       BIT(42)
#define SCTLR_ATA_BIT        BIT(43)

#define ACTLR_AUX_BIT        BIT(9)
#define ACTLR_CLPORTS_BIT    BIT(8)
#define ACTLR_CLPMU_BIT      BIT(7)
#define ACTLR_TESTR1_BIT     BIT(6)
#define ACTLR_CDBG_BIT       BIT(5)
#define ACTLR_PATCH_BIT      BIT(4)
#define ACTLR_BPRED_BIT      BIT(3)
#define ACTLR_POWER_BIT      BIT(2)
#define ACTLR_DIAGNOSTIC_BIT BIT(1)
#define ACTLR_REGIONS_BIT    BIT(0)

/* SPSR M[3:0] define
 *
 * Arm® Architecture Registers Armv8, for Armv8-A architecture profile
 * ( DDI 0595, ID121321 ), defined:
 * SPSR_EL1: Saved Program Status Register (EL1)
 * SPSR_EL2: Saved Program Status Register (EL2)
 * SPSR_EL3: Saved Program Status Register (EL3)
 *
 * reference to Programmer’s Guide for ARMv8-A
 * (ARM DEN0024A, ID050815 ), 4.1.2 Stack pointer
 *
 * The T suffix, indicates use of the SP_EL0 stack pointer.
 * The H suffix, indicates use of the SP_ELx stack pointer.
 *
 */

#define SPSR_DAIF_SHIFT     (6)
#define SPSR_DAIF_MASK      (0xf << SPSR_DAIF_SHIFT)

#define SPSR_MODE_EL0T      (0x0)
#define SPSR_MODE_EL1T      (0x4)
#define SPSR_MODE_EL1H      (0x5)
#define SPSR_MODE_EL2T      (0x8)
#define SPSR_MODE_EL2H      (0x9)
#define SPSR_MODE_EL3T      (0xc)
#define SPSR_MODE_EL3H      (0xd)
#define SPSR_MODE_MASK      (0xf)

#define RGSR_EL1_TAG_MASK   0xfUL
#define RGSR_EL1_SEED_SHIFT 8
#define RGSR_EL1_SEED_MASK  0xffffUL

#define TTBR_CNP_BIT        BIT(0)

/* CurrentEL: Current Exception Level */

#define MODE_EL_SHIFT       (0x2)
#define MODE_EL_MASK        (0x3)

#define MODE_EL3            (0x3)
#define MODE_EL2            (0x2)
#define MODE_EL1            (0x1)
#define MODE_EL0            (0x0)

#ifndef __ASSEMBLY__

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define GET_EL(mode)  (((mode) >> MODE_EL_SHIFT) & MODE_EL_MASK)

#define MPIDR_ID_MASK (0xff00ffffff)

#define MPIDR_AFFLVL(mpidr, aff_level) \
  (((mpidr) >> MPIDR_AFF ## aff_level ## _SHIFT) & MPIDR_AFFLVL_MASK)

#define CORE_TO_MPID(core, aff_level) \
  ({ \
    uint64_t __mpidr = GET_MPIDR(); \
    __mpidr &= ~(MPIDR_AFFLVL_MASK << MPIDR_AFF ## aff_level ## _SHIFT); \
    __mpidr |= ((core) << MPIDR_AFF ## aff_level ## _SHIFT); \
    __mpidr &= MPIDR_ID_MASK; \
    __mpidr; \
  })

/* System register interface to GICv3 */

#define ICC_IGRPEN1_EL1             S3_0_C12_C12_7
#define ICC_SGI1R                   S3_0_C12_C11_5
#define ICC_SRE_EL1                 S3_0_C12_C12_5
#define ICC_SRE_EL2                 S3_4_C12_C9_5
#define ICC_SRE_EL3                 S3_6_C12_C12_5
#define ICC_CTLR_EL1                S3_0_C12_C12_4
#define ICC_CTLR_EL3                S3_6_C12_C12_4
#define ICC_PMR_EL1                 S3_0_C4_C6_0
#define ICC_RPR_EL1                 S3_0_C12_C11_3
#define ICC_IGRPEN1_EL3             S3_6_C12_C12_7
#define ICC_IGRPEN0_EL1             S3_0_C12_C12_6
#define ICC_HPPIR0_EL1              S3_0_C12_C8_2
#define ICC_HPPIR1_EL1              S3_0_C12_C12_2
#define ICC_IAR0_EL1                S3_0_C12_C8_0
#define ICC_IAR1_EL1                S3_0_C12_C12_0
#define ICC_EOIR0_EL1               S3_0_C12_C8_1
#define ICC_EOIR1_EL1               S3_0_C12_C12_1
#define ICC_SGI0R_EL1               S3_0_C12_C11_7

/* register constants */
#define ICC_SRE_ELX_SRE_BIT         BIT(0)
#define ICC_SRE_ELX_DFB_BIT         BIT(1)
#define ICC_SRE_ELX_DIB_BIT         BIT(2)
#define ICC_SRE_EL3_EN_BIT          BIT(3)

/* ICC SGI macros */
#define SGIR_TGT_MASK               (0xffff)
#define SGIR_AFF1_SHIFT             (16)
#define SGIR_AFF2_SHIFT             (32)
#define SGIR_AFF3_SHIFT             (48)
#define SGIR_AFF_MASK               (0xf)
#define SGIR_INTID_SHIFT            (24)
#define SGIR_INTID_MASK             (0xf)
#define SGIR_IRM_SHIFT              (40)
#define SGIR_IRM_MASK               (0x1)
#define SGIR_IRM_TO_AFF             (0)
#define SGIR_IRM_TO_ALL             (1)

#define GICV3_SGIR_VALUE(_aff3, _aff2, _aff1, _intid, _irm, _tgt) \
  ((((uint64_t)(_aff3) & SGIR_AFF_MASK) << SGIR_AFF3_SHIFT) |     \
   (((uint64_t)(_irm) & SGIR_IRM_MASK) << SGIR_IRM_SHIFT) |       \
   (((uint64_t)(_aff2) & SGIR_AFF_MASK) << SGIR_AFF2_SHIFT) |     \
   (((_intid) & SGIR_INTID_MASK) << SGIR_INTID_SHIFT) |           \
   (((_aff1) & SGIR_AFF_MASK) << SGIR_AFF1_SHIFT) |               \
   ((_tgt) & SGIR_TGT_MASK))

/* CPTR_EL2, Architectural Feature Trap Register (EL2) */

#define CPTR_EZ_BIT                 BIT(8)
#define CPTR_TFP_BIT                BIT(10)
#define CPTR_TTA_BIT                BIT(20)
#define CPTR_TCPAC_BIT              BIT(31)
#define CPTR_EL2_RES1               BIT(13) | BIT(12) | BIT(9) | (0xff)

/* CPACR_EL1, Architectural Feature Access Control Register */
#define CPACR_EL1_FPEN_NOTRAP       (0x3 << 20)

/* SCR_EL3, Secure Configuration Register */
#define SCR_NS_BIT                  BIT(0)
#define SCR_IRQ_BIT                 BIT(1)
#define SCR_FIQ_BIT                 BIT(2)
#define SCR_EA_BIT                  BIT(3)
#define SCR_SMD_BIT                 BIT(7)
#define SCR_HCE_BIT                 BIT(8)
#define SCR_RW_BIT                  BIT(10)
#define SCR_ST_BIT                  BIT(11)
#define SCR_RES1                    (BIT(4) | BIT(5))

/* HCR_EL2, Hypervisor Configuration Register */

#define HCR_FMO_BIT                 BIT(3)
#define HCR_IMO_BIT                 BIT(4)
#define HCR_AMO_BIT                 BIT(5)
#define HCR_RW_BIT                  BIT(31)
#define HCR_ATA_BIT                 BIT(56)

/* CNTHCTL_EL2 bits definitions */

#define CNTHCTL_EL2_EL1PCEN_EN      BIT(1)
#define CNTHCTL_EL2_EL1PCTEN_EN     BIT(0)

/* CNTV_CVAL, Counter-timer Virtual Timer CompareValue register
 * CNTV_CTL, Counter-timer Virtual Timer Control register
 */

#define CNTV_CTL_ENABLE_BIT         BIT(0)
#define CNTV_CTL_IMASK_BIT          BIT(1)

/*  Maximum numbers of translation tables
 *      This option specifies the maximum numbers of translation tables
 *  excluding the base translation table. Based on this, translation
 *  tables are allocated at compile time and used at runtime as needed.
 *  If the runtime need exceeds preallocated numbers of translation
 *  tables, it will result in assert. Number of translation tables
 *  required is decided based on how many discrete memory regions
 *  (both normal and device memory) are present on given platform and
 *  how much granularity is required while assigning attributes
 *  to these memory regions.
 */

#define CONFIG_MAX_XLAT_TABLES      10

#define L1_CACHE_SHIFT              (6)
#define L1_CACHE_BYTES              BIT(L1_CACHE_SHIFT)

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name:
 *   getreg8/16/32/64
 *   putreg8/16/32/64
 *
 * Description:
 *   We need to use explicit assembler instruction there, because with
 *   classic "volatile pointer" approach compiler might generate
 *   instruction with immediate value like
 *
 *   str     w4, [x1], #4
 *
 *   Such instructions produce invalid syndrome in HSR register,
 *   so hypervisor can't emulate MMIO  when it traps memory access.
 *
 ****************************************************************************/

static inline uint8_t getreg8(unsigned long addr)
{
  uint8_t val;

  __asm__ volatile ("ldrb %w0, [%1]" : "=r" (val) : "r" (addr));

  UP_DMB();
  return val;
}

static inline void putreg8(uint8_t data, unsigned long addr)
{
  UP_DMB();
  __asm__ volatile ("strb %w0, [%1]" : : "r" (data), "r" (addr));
}

static inline uint16_t getreg16(unsigned long addr)
{
  uint16_t val;

  __asm__ volatile ("ldrh %w0, [%1]" : "=r" (val) : "r" (addr));

  UP_DMB();
  return val;
}

static inline void putreg16(uint16_t data, unsigned long addr)
{
  UP_DMB();
  __asm__ volatile ("strh %w0, [%1]" : : "r" (data), "r" (addr));
}

static inline uint32_t getreg32(unsigned long addr)
{
  uint32_t val;

  __asm__ volatile ("ldr %w0, [%1]" : "=r" (val) : "r" (addr));

  UP_DMB();
  return val;
}

static inline void putreg32(uint32_t data, unsigned long addr)
{
  UP_DMB();
  __asm__ volatile ("str %w0, [%1]" : : "r" (data), "r" (addr));
}

static inline uint64_t getreg64(unsigned long addr)
{
  uint64_t val;

  __asm__ volatile ("ldr %x0, [%1]" : "=r" (val) : "r" (addr));

  UP_DMB();
  return val;
}

static inline void putreg64(uint64_t data, unsigned long addr)
{
  UP_DMB();
  __asm__ volatile ("str %x0, [%1]" : : "r" (data), "r" (addr));
}

static inline void arch_nop(void)
{
  __asm__ volatile ("nop");
}

/****************************************************************************
 * Name:
 *   arm64_current_el()
 *
 * Description:
 *
 *   Get current execution level
 *
 ****************************************************************************/

#define arm64_current_el()                \
  ({                                      \
    uint64_t __el;                        \
    int      __ret;                       \
    __asm__ volatile ("mrs %0, CurrentEL" \
                      : "=r" (__el));     \
    __ret = GET_EL(__el);                 \
    __ret;                                \
  })

#define modreg8(v,m,a)  putreg8((getreg8(a) & ~(m)) | ((v) & (m)), (a))
#define modreg16(v,m,a) putreg16((getreg16(a) & ~(m)) | ((v) & (m)), (a))
#define modreg32(v,m,a) putreg32((getreg32(a) & ~(m)) | ((v) & (m)), (a))

/* Atomic modification of registers */

void modifyreg8(unsigned int addr, uint8_t clearbits, uint8_t setbits);
void modifyreg16(unsigned int addr, uint16_t clearbits, uint16_t setbits);
void modifyreg32(unsigned int addr, uint32_t clearbits, uint32_t setbits);

/****************************************************************************
 * Name:
 *   arch_get_exception_depth
 *
 * Description:
 *   tpidrro_el0 is used to record exception depth, it's used for fpu trap
 * happened at exception context (like IRQ).
 *
 ****************************************************************************/

static inline int arch_get_exception_depth(void)
{
  return read_sysreg(tpidrro_el0);
}

void arch_cpu_idle(void);

/****************************************************************************
 * Name: arm64_cpu_disable
 *
 * Description:
 *   Called from CPU0 to make sure that all other CPUs are in the disabled
 *   state.  This is a formality because the other CPUs are actually running
 *   then we have probably already crashed.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

#ifdef CONFIG_SMP
void arm64_cpu_disable(void);

#else
#  define arm64_cpu_disable()
#endif

/****************************************************************************
 * Name: arm64_cpu_enable
 *
 * Description:
 *   Called from CPU0 to enable all other CPUs.  The enabled CPUs will start
 *   execution at __cpuN_start and, after very low-level CPU initialization
 *   has been performed, will branch to arm_cpu_boot()
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

#ifdef CONFIG_SMP
void arm64_cpu_enable(void);

#else
#  define arm64_cpu_enable()
#endif

/****************************************************************************
 * Name: arm64_get_mpid
 *
 * Description:
 *   The function from cpu index to get cpu mpid which is reading
 * from mpidr_el1 register. Different ARM64 Core will use different
 * Affn define, the mpidr_el1 value is not CPU number, So we need
 * to change CPU number to mpid and vice versa
 *
 ****************************************************************************/

#ifdef CONFIG_SMP
uint64_t arm64_get_mpid(int cpu);
#else
#  define arm64_get_mpid(cpu) ((void)(cpu), (GET_MPIDR() & MPIDR_ID_MASK))
#endif /* CONFIG_SMP */

/****************************************************************************
 * Name: arm64_get_cpuid
 *
 * Description:
 *   The function from mpid to get cpu id
 *
 ****************************************************************************/

#ifdef CONFIG_SMP
int arm64_get_cpuid(uint64_t mpid);
#endif

/****************************************************************************
 * Name: arm64_mte_init
 *
 * Description:
 *   Initialize MTE settings and enable memory tagging
 *
 ****************************************************************************/

void arm64_mte_init(void);

#endif /* __ASSEMBLY__ */

#endif /* ___ARCH_ARM64_SRC_COMMON_ARM64_ARCH_H */
