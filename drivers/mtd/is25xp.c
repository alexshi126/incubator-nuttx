/************************************************************************************
 * drivers/mtd/is25xp.c
 * Driver for SPI-based IS25LPxx parts 32MBit and larger.
 *
 *   Copyright (C) 2016 Marten Svanfeldt. All rights reserved.
 *
 *   Copyright (C) 2009-2011, 2013 Gregory Nutt. All rights reserved.
 *   Author: Ken Pettit <pettitkd@gmail.com>
 *
 *   Copied from / based on m25px.c and sst25.c drivers written by
 *   Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ************************************************************************************/

/************************************************************************************
 * Included Files
 ************************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/kmalloc.h>
#include <nuttx/fs/ioctl.h>
#include <nuttx/spi/spi.h>
#include <nuttx/mtd/mtd.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/
/* Configuration ********************************************************************/
/* Per the data sheet, IS25xP parts can be driven with either SPI mode 0 (CPOL=0 and
 * CPHA=0) or mode 3 (CPOL=1 and CPHA=1).  So you may need to specify
 * CONFIG_IS25XP_SPIMODE to select the best mode for your device.  If
 * CONFIG_IS25XP_SPIMODE is not defined, mode 0 will be used.
 */

#ifndef CONFIG_IS25XP_SPIMODE
#  define CONFIG_IS25XP_SPIMODE SPIDEV_MODE0
#endif

/* SPI Frequency.  May be up to 50MHz. */

#ifndef CONFIG_IS25XP_SPIFREQUENCY
#  define CONFIG_IS25XP_SPIFREQUENCY 20000000
#endif


/* IS25 Registers *******************************************************************/
/* Indentification register values */

#define IS25_MANUFACTURER         0x9d
#define IS25_MEMORY_TYPE          0x60

/*  IS25LP064 capacity is 8,388,608 bytes:
 *  (2,048 sectors) * (4,096 bytes per sector)
 *  (32,768 pages) * (256 bytes per page)
 */
#define IS25_IS25LP064_CAPACITY       0x17
#define IS25_IS25LP064_SECTOR_SHIFT   12    /* Sector size 1 << 12 = 4,096 */
#define IS25_IS25LP064_NSECTORS       2048
#define IS25_IS25LP064_PAGE_SHIFT     8     /* Page size 1 << 8 = 256 */
#define IS25_IS25LP064_NPAGES         32768

/*  IS25LP128 capacity is 16,777,216 bytes:
 *  (4,096 sectors) * (4,096 bytes per sector)
 *  (65,536 pages) * (256 bytes per page)
 */
#define IS25_IS25LP128_CAPACITY       0x18
#define IS25_IS25LP128_SECTOR_SHIFT   12    /* Sector size 1 << 12 = 4,096 */
#define IS25_IS25LP128_NSECTORS       4096
#define IS25_IS25LP128_PAGE_SHIFT     8     /* Page size 1 << 8 = 256 */
#define IS25_IS25LP128_NPAGES         65536


/* Instructions */
/*      Command        Value      N Description             Addr Dummy  Data   */
#define IS25_WREN      0x06    /* 1 Write Enable              0   0     0     */
#define IS25_WRDI      0x04    /* 1 Write Disable             0   0     0     */
#define IS25_RDID      0x9f    /* 1 Read Identification       0   0     1-3   */
#define IS25_RDSR      0x05    /* 1 Read Status Register      0   0     >=1   */
//#define IS25_EWSR      0x50    /* 1 Write enable status       0   0     0     */
#define IS25_WRSR      0x01    /* 1 Write Status Register     0   0     1     */
#define IS25_READ      0x03    /* 1 Read Data Bytes           3   0     >=1   */
#define IS25_FAST_READ 0x0b    /* 1 Higher speed read         3   1     >=1   */
#define IS25_PP        0x02    /* 1 Page Program              3   0     1-256 */
#define IS25_SE        0x20    /* 1 Sector Erase              3   0     0     */
#define IS25_BE32      0x52    /* 2 32K Block Erase           3   0     0     */
#define IS25_BE64      0xD8    /* 2 64K Block Erase           3   0     0     */
#define IS25_CER       0xC7    /* 1 Chip Erase                0   0     0     */

/* NOTE 1: All parts.
 * NOTE 2: In IS25XP terminology, 0x52 and 0xd8 are block erase and 0x20
 *         is a sector erase.  Block erase provides a faster way to erase
 *         multiple 4K sectors at once.
 */

/* Status register bit definitions */

#define IS25_SR_WIP              (1 << 0)                /* Bit 0: Write in progress bit */
#define IS25_SR_WEL              (1 << 1)                /* Bit 1: Write enable latch bit */
#define IS25_SR_BP_SHIFT         (2)                     /* Bits 2-5: Block protect bits */
#define IS25_SR_BP_MASK          (15 << IS25_SR_BP_SHIFT)
#  define IS25_SR_BP_NONE        (0 << IS25_SR_BP_SHIFT) /* Unprotected */
#  define IS25_SR_BP_UPPER128th  (1 << IS25_SR_BP_SHIFT) /* Upper 128th */
#  define IS25_SR_BP_UPPER64th   (2 << IS25_SR_BP_SHIFT) /* Upper 64th */
#  define IS25_SR_BP_UPPER32nd   (3 << IS25_SR_BP_SHIFT) /* Upper 32nd */
#  define IS25_SR_BP_UPPER16th   (4 << IS25_SR_BP_SHIFT) /* Upper 16th */
#  define IS25_SR_BP_UPPER8th    (5 << IS25_SR_BP_SHIFT) /* Upper 8th */
#  define IS25_SR_BP_UPPERQTR    (6 << IS25_SR_BP_SHIFT) /* Upper quarter */
#  define IS25_SR_BP_UPPERHALF   (7 << IS25_SR_BP_SHIFT) /* Upper half */
#  define IS25_SR_BP_ALL         (8 << IS25_SR_BP_SHIFT) /* All sectors */
#define IS25_SR_QE               (1 << 6)                /* Bit 6: Quad (QSPI) enable bit */
#define IS25_SR_SRWD             (1 << 7)                /* Bit 7: Status register write protect */

#define IS25_DUMMY     0xa5

/************************************************************************************
 * Private Types
 ************************************************************************************/

/* This type represents the state of the MTD device.  The struct mtd_dev_s
 * must appear at the beginning of the definition so that you can freely
 * cast between pointers to struct mtd_dev_s and struct is25xp_dev_s.
 */

struct is25xp_dev_s
{
  struct mtd_dev_s mtd;      /* MTD interface */
  FAR struct spi_dev_s *dev; /* Saved SPI interface instance */
  uint8_t  sectorshift;      /* 12 */
  uint8_t  pageshift;        /* 8 */
  uint16_t nsectors;         /* 2,048 or 4,096 */
  uint32_t npages;           /* 32,768 or 65,536 */
  uint8_t  lastwaswrite;     /* Indicates if last operation was write */
};

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

/* Helpers */

static void is25xp_lock(FAR struct spi_dev_s *dev);
static inline void is25xp_unlock(FAR struct spi_dev_s *dev);
static inline int is25xp_readid(struct is25xp_dev_s *priv);
static void is25xp_waitwritecomplete(struct is25xp_dev_s *priv);
static void is25xp_writeenable(struct is25xp_dev_s *priv);
static inline void is25xp_sectorerase(struct is25xp_dev_s *priv, off_t offset, uint8_t type);
static inline int  is25xp_bulkerase(struct is25xp_dev_s *priv);
static inline void is25xp_pagewrite(struct is25xp_dev_s *priv, FAR const uint8_t *buffer,
                                  off_t offset);

/* MTD driver methods */

static int is25xp_erase(FAR struct mtd_dev_s *dev, off_t startblock, size_t nblocks);
static ssize_t is25xp_bread(FAR struct mtd_dev_s *dev, off_t startblock,
                          size_t nblocks, FAR uint8_t *buf);
static ssize_t is25xp_bwrite(FAR struct mtd_dev_s *dev, off_t startblock,
                           size_t nblocks, FAR const uint8_t *buf);
static ssize_t is25xp_read(FAR struct mtd_dev_s *dev, off_t offset, size_t nbytes,
                         FAR uint8_t *buffer);
#ifdef CONFIG_MTD_BYTE_WRITE
static ssize_t is25xp_write(FAR struct mtd_dev_s *dev, off_t offset, size_t nbytes,
                         FAR const uint8_t *buffer);
#endif
static int is25xp_ioctl(FAR struct mtd_dev_s *dev, int cmd, unsigned long arg);

/************************************************************************************
 * Private Data
 ************************************************************************************/

/************************************************************************************
 * Private Functions
 ************************************************************************************/

/************************************************************************************
 * Name: is25xp_lock
 ************************************************************************************/

static void is25xp_lock(FAR struct spi_dev_s *dev)
{
  /* On SPI busses where there are multiple devices, it will be necessary to
   * lock SPI to have exclusive access to the busses for a sequence of
   * transfers.  The bus should be locked before the chip is selected.
   *
   * This is a blocking call and will not return until we have exclusiv access to
   * the SPI buss.  We will retain that exclusive access until the bus is unlocked.
   */

  (void)SPI_LOCK(dev, true);

  /* After locking the SPI bus, the we also need call the setfrequency, setbits, and
   * setmode methods to make sure that the SPI is properly configured for the device.
   * If the SPI buss is being shared, then it may have been left in an incompatible
   * state.
   */

  SPI_SETMODE(dev, CONFIG_IS25XP_SPIMODE);
  SPI_SETBITS(dev, 8);
  (void)SPI_SETFREQUENCY(dev, CONFIG_IS25XP_SPIFREQUENCY);
}

/************************************************************************************
 * Name: is25xp_unlock
 ************************************************************************************/

static inline void is25xp_unlock(FAR struct spi_dev_s *dev)
{
  (void)SPI_LOCK(dev, false);
}

/************************************************************************************
 * Name: is25xp_readid
 ************************************************************************************/

static inline int is25xp_readid(struct is25xp_dev_s *priv)
{
  uint16_t manufacturer;
  uint16_t memory;
  uint16_t capacity;

  fvdbg("priv: %p\n", priv);

  /* Lock the SPI bus, configure the bus, and select this FLASH part. */

  is25xp_lock(priv->dev);
  SPI_SELECT(priv->dev, SPIDEV_FLASH, true);

  /* Send the "Read ID (RDID)" command and read the first three ID bytes */

  (void)SPI_SEND(priv->dev, IS25_RDID);
  manufacturer = SPI_SEND(priv->dev, IS25_DUMMY);
  memory       = SPI_SEND(priv->dev, IS25_DUMMY);
  capacity     = SPI_SEND(priv->dev, IS25_DUMMY);

  /* Deselect the FLASH and unlock the bus */

  SPI_SELECT(priv->dev, SPIDEV_FLASH, false);
  is25xp_unlock(priv->dev);

  fvdbg("manufacturer: %02x memory: %02x capacity: %02x\n",
        manufacturer, memory, capacity);

  /* Check for a valid manufacturer and memory type */

  if (manufacturer == IS25_MANUFACTURER && memory == IS25_MEMORY_TYPE)
    {
      /* Okay.. is it a FLASH capacity that we understand? */

      if (capacity == IS25_IS25LP064_CAPACITY)
        {
           /* Save the FLASH geometry */

           priv->sectorshift = IS25_IS25LP064_SECTOR_SHIFT;
           priv->nsectors    = IS25_IS25LP064_NSECTORS;
           priv->pageshift   = IS25_IS25LP064_PAGE_SHIFT;
           priv->npages      = IS25_IS25LP064_NPAGES;
           return OK;
        }
      else if (capacity == IS25_IS25LP128_CAPACITY)
        {
           /* Save the FLASH geometry */

           priv->sectorshift = IS25_IS25LP128_SECTOR_SHIFT;
           priv->nsectors    = IS25_IS25LP128_NSECTORS;
           priv->pageshift   = IS25_IS25LP128_PAGE_SHIFT;
           priv->npages      = IS25_IS25LP128_NPAGES;
           return OK;
        }
    }

  return -ENODEV;
}

/************************************************************************************
 * Name: is25xp_waitwritecomplete
 ************************************************************************************/

static void is25xp_waitwritecomplete(struct is25xp_dev_s *priv)
{
  uint8_t status;

  /* No need to check if no write / erase was done */

#if 0
  if (!priv->lastwaswrite)
    return;
#endif

  /* Are we the only device on the bus? */

#ifdef CONFIG_SPI_OWNBUS

  /* Select this FLASH part */

  SPI_SELECT(priv->dev, SPIDEV_FLASH, true);

  /* Send "Read Status Register (RDSR)" command */

  (void)SPI_SEND(priv->dev, IS25_RDSR);

  /* Loop as long as the memory is busy with a write cycle */

  do
    {
      /* Send a dummy byte to generate the clock needed to shift out the status */

      status = SPI_SEND(priv->dev, IS25_DUMMY);
    }
  while ((status & IS25_SR_WIP) != 0);

  /* Deselect the FLASH */

  SPI_SELECT(priv->dev, SPIDEV_FLASH, false);

#else

  /* Loop as long as the memory is busy with a write cycle */

  do
    {
      /* Select this FLASH part */

      SPI_SELECT(priv->dev, SPIDEV_FLASH, true);

      /* Send "Read Status Register (RDSR)" command */

      (void)SPI_SEND(priv->dev, IS25_RDSR);

      /* Send a dummy byte to generate the clock needed to shift out the status */

      status = SPI_SEND(priv->dev, IS25_DUMMY);

      /* Deselect the FLASH */

      SPI_SELECT(priv->dev, SPIDEV_FLASH, false);

      /* Given that writing could take up to few tens of milliseconds, and erasing
       * could take more.  The following short delay in the "busy" case will allow
       * other peripherals to access the SPI bus.
       */

      if ((status & IS25_SR_WIP) != 0)
        {
          is25xp_unlock(priv->dev);
          usleep(1000);
          is25xp_lock(priv->dev);
        }
    }
  while ((status & IS25_SR_WIP) != 0);

#endif

  priv->lastwaswrite = false;

  fvdbg("Complete\n");
}

/************************************************************************************
 * Name:  is25xp_writeenable
 ************************************************************************************/

static void is25xp_writeenable(struct is25xp_dev_s *priv)
{
  /* Select this FLASH part */

  SPI_SELECT(priv->dev, SPIDEV_FLASH, true);

  /* Send "Write Enable (WREN)" command */

  (void)SPI_SEND(priv->dev, IS25_WREN);

  /* Deselect the FLASH */

  SPI_SELECT(priv->dev, SPIDEV_FLASH, false);
  fvdbg("Enabled\n");
}

/************************************************************************************
 * Name: is25xp_unprotect
 ************************************************************************************/

static void is25xp_unprotect(struct is25xp_dev_s *priv)
{
  /* Make writeable */

  is25xp_writeenable(priv);

  /* Send "Write status (WRSR)" */

  SPI_SELECT(priv->dev, SPIDEV_FLASH, true);
  SPI_SEND(priv->dev, IS25_WRSR);

  /* Followed by the new status value */

  SPI_SEND(priv->dev, 0);

  SPI_SELECT(priv->dev, SPIDEV_FLASH, false);
}

/************************************************************************************
 * Name:  is25xp_sectorerase
 ************************************************************************************/

static void is25xp_sectorerase(struct is25xp_dev_s *priv, off_t sector, uint8_t type)
{
  off_t offset;

  offset = sector << priv->sectorshift;

  fvdbg("sector: %08lx\n", (long)sector);

  /* Wait for any preceding write to complete.  We could simplify things by
   * perform this wait at the end of each write operation (rather than at
   * the beginning of ALL operations), but have the wait first will slightly
   * improve performance.
   */

  is25xp_waitwritecomplete(priv);

  /* Send write enable instruction */

  is25xp_writeenable(priv);

  /* Select this FLASH part */

  SPI_SELECT(priv->dev, SPIDEV_FLASH, true);

  /* Send the "Sector Erase (SE)" or Sub-Sector Erase (SSE) instruction
   * that was passed in as the erase type.
   */

  (void)SPI_SEND(priv->dev, type);

  /* Send the sector offset high byte first.  For all of the supported
   * parts, the sector number is completely contained in the first byte
   * and the values used in the following two bytes don't really matter.
   */

  (void)SPI_SEND(priv->dev, (offset >> 16) & 0xff);
  (void)SPI_SEND(priv->dev, (offset >> 8) & 0xff);
  (void)SPI_SEND(priv->dev, offset & 0xff);
  priv->lastwaswrite = true;

  /* Deselect the FLASH */

  SPI_SELECT(priv->dev, SPIDEV_FLASH, false);
  fvdbg("Erased\n");
}

/************************************************************************************
 * Name:  is25xp_bulkerase
 ************************************************************************************/

static inline int is25xp_bulkerase(struct is25xp_dev_s *priv)
{
  fvdbg("priv: %p\n", priv);

  /* Wait for any preceding write to complete.  We could simplify things by
   * perform this wait at the end of each write operation (rather than at
   * the beginning of ALL operations), but have the wait first will slightly
   * improve performance.
   */

  is25xp_waitwritecomplete(priv);

  /* Send write enable instruction */

  is25xp_writeenable(priv);

  /* Select this FLASH part */

  SPI_SELECT(priv->dev, SPIDEV_FLASH, true);

  /* Send the "Chip Erase (CER)" instruction */

  (void)SPI_SEND(priv->dev, IS25_CER);

  /* Deselect the FLASH */

  SPI_SELECT(priv->dev, SPIDEV_FLASH, false);
  is25xp_waitwritecomplete(priv);

  fvdbg("Return: OK\n");
  return OK;
}

/************************************************************************************
 * Name:  is25xp_pagewrite
 ************************************************************************************/

static inline void is25xp_pagewrite(struct is25xp_dev_s *priv, FAR const uint8_t *buffer,
                                  off_t page)
{
  off_t offset = page << priv->pageshift;

  fvdbg("page: %08lx offset: %08lx\n", (long)page, (long)offset);

  /* Wait for any preceding write to complete.  We could simplify things by
   * perform this wait at the end of each write operation (rather than at
   * the beginning of ALL operations), but have the wait first will slightly
   * improve performance.
   */

  is25xp_waitwritecomplete(priv);

  /* Enable the write access to the FLASH */

  is25xp_writeenable(priv);

  /* Select this FLASH part */

  SPI_SELECT(priv->dev, SPIDEV_FLASH, true);

  /* Send "Page Program (PP)" command */

  (void)SPI_SEND(priv->dev, IS25_PP);

  /* Send the page offset high byte first. */

  (void)SPI_SEND(priv->dev, (offset >> 16) & 0xff);
  (void)SPI_SEND(priv->dev, (offset >> 8) & 0xff);
  (void)SPI_SEND(priv->dev, offset & 0xff);

  /* Then write the specified number of bytes */

  SPI_SNDBLOCK(priv->dev, buffer, 1 << priv->pageshift);
  priv->lastwaswrite = true;

  /* Deselect the FLASH: Chip Select high */

  SPI_SELECT(priv->dev, SPIDEV_FLASH, false);
  fvdbg("Written\n");
}

/************************************************************************************
 * Name:  is25xp_bytewrite
 ************************************************************************************/

#ifdef CONFIG_MTD_BYTE_WRITE
static inline void is25xp_bytewrite(struct is25xp_dev_s *priv, FAR const uint8_t *buffer,
                                     off_t offset, uint16_t count)
{
  fvdbg("offset: %08lx  count:%d\n", (long)offset, count);

  /* Wait for any preceding write to complete.  We could simplify things by
   * perform this wait at the end of each write operation (rather than at
   * the beginning of ALL operations), but have the wait first will slightly
   * improve performance.
   */

  is25xp_waitwritecomplete(priv);

  /* Enable the write access to the FLASH */

  is25xp_writeenable(priv);

  /* Select this FLASH part */

  SPI_SELECT(priv->dev, SPIDEV_FLASH, true);

  /* Send "Page Program (PP)" command */

  (void)SPI_SEND(priv->dev, IS25_PP);

  /* Send the page offset high byte first. */

  (void)SPI_SEND(priv->dev, (offset >> 16) & 0xff);
  (void)SPI_SEND(priv->dev, (offset >> 8) & 0xff);
  (void)SPI_SEND(priv->dev, offset & 0xff);

  /* Then write the specified number of bytes */

  SPI_SNDBLOCK(priv->dev, buffer, count);
  priv->lastwaswrite = true;

  /* Deselect the FLASH: Chip Select high */

  SPI_SELECT(priv->dev, SPIDEV_FLASH, false);
  fvdbg("Written\n");
}
#endif

/************************************************************************************
 * Name: is25xp_erase
 ************************************************************************************/

static int is25xp_erase(FAR struct mtd_dev_s *dev, off_t startblock, size_t nblocks)
{
  FAR struct is25xp_dev_s *priv = (FAR struct is25xp_dev_s *)dev;
  size_t blocksleft = nblocks;

  fvdbg("startblock: %08lx nblocks: %d\n", (long)startblock, (int)nblocks);

  /* Lock access to the SPI bus until we complete the erase */

  is25xp_lock(priv->dev);
  while (blocksleft > 0)
    {
      size_t sectorboundry;
      size_t blkper;

      /* We will erase in either 4K sectors or 32K or 64K blocks depending
       * on the largest unit we can use given the startblock and nblocks.
       * This will reduce erase time (in the event we have partitions
       * enabled and are doing a bulk erase which is translated into
       * a block erase operation).
       */

      /* Test for 64K alignment */

      blkper = 64 / 4;
      sectorboundry = (startblock + blkper - 1) / blkper;
      sectorboundry *= blkper;

      /* If we are on a sector boundry and have at least a full sector
       * of blocks left to erase, then we can do a full sector erase.
       */

      if (startblock == sectorboundry && blocksleft >= blkper)
        {
          /* Do a 64k block erase */

          is25xp_sectorerase(priv, startblock, IS25_BE64);
          startblock += blkper;
          blocksleft -= blkper;
          continue;
        }

      /* Test for 32K block alignment */

      blkper = 32 / 4;
      sectorboundry = (startblock + blkper - 1) / blkper;
      sectorboundry *= blkper;

      if (startblock == sectorboundry && blocksleft >= blkper)
        {
          /* Do a 32k block erase */

          is25xp_sectorerase(priv, startblock, IS25_BE32);
          startblock += blkper;
          blocksleft -= blkper;
          continue;
        }
      else
        {
          /* Just do a sector erase */

          is25xp_sectorerase(priv, startblock, IS25_SE);
          startblock++;
          blocksleft--;
          continue;
        }
    }

  is25xp_unlock(priv->dev);
  return (int)nblocks;
}

/************************************************************************************
 * Name: is25xp_bread
 ************************************************************************************/

static ssize_t is25xp_bread(FAR struct mtd_dev_s *dev, off_t startblock, size_t nblocks,
                             FAR uint8_t *buffer)
{
  FAR struct is25xp_dev_s *priv = (FAR struct is25xp_dev_s *)dev;
  ssize_t nbytes;

  fvdbg("startblock: %08lx nblocks: %d\n", (long)startblock, (int)nblocks);

  /* On this device, we can handle the block read just like the byte-oriented read */

  nbytes = is25xp_read(dev, startblock << priv->pageshift, nblocks << priv->pageshift, buffer);
  if (nbytes > 0)
    {
        return nbytes >> priv->pageshift;
    }

  return (int)nbytes;
}

/************************************************************************************
 * Name: is25xp_bwrite
 ************************************************************************************/

static ssize_t is25xp_bwrite(FAR struct mtd_dev_s *dev, off_t startblock, size_t nblocks,
                              FAR const uint8_t *buffer)
{
  FAR struct is25xp_dev_s *priv = (FAR struct is25xp_dev_s *)dev;
  size_t blocksleft = nblocks;
  size_t pagesize = 1 << priv->pageshift;

  fvdbg("startblock: %08lx nblocks: %d\n", (long)startblock, (int)nblocks);

  /* Lock the SPI bus and write each page to FLASH */

  is25xp_lock(priv->dev);
  while (blocksleft-- > 0)
    {
      is25xp_pagewrite(priv, buffer, startblock);
      buffer += pagesize;
      startblock++;
   }

  is25xp_unlock(priv->dev);
  return nblocks;
}

/************************************************************************************
 * Name: is25xp_read
 ************************************************************************************/

static ssize_t is25xp_read(FAR struct mtd_dev_s *dev, off_t offset, size_t nbytes,
                            FAR uint8_t *buffer)
{
  FAR struct is25xp_dev_s *priv = (FAR struct is25xp_dev_s *)dev;

  fvdbg("offset: %08lx nbytes: %d\n", (long)offset, (int)nbytes);

  /* Wait for any preceding write to complete.  We could simplify things by
   * perform this wait at the end of each write operation (rather than at
   * the beginning of ALL operations), but have the wait first will slightly
   * improve performance.
   */

  if (priv->lastwaswrite)
    {
      is25xp_waitwritecomplete(priv);
    }

  /* Lock the SPI bus and select this FLASH part */

  is25xp_lock(priv->dev);
  SPI_SELECT(priv->dev, SPIDEV_FLASH, true);

  /* Send "Read from Memory " instruction */

  (void)SPI_SEND(priv->dev, IS25_READ);

  /* Send the page offset high byte first. */

  (void)SPI_SEND(priv->dev, (offset >> 16) & 0xff);
  (void)SPI_SEND(priv->dev, (offset >> 8) & 0xff);
  (void)SPI_SEND(priv->dev, offset & 0xff);

  /* Then read all of the requested bytes */

  SPI_RECVBLOCK(priv->dev, buffer, nbytes);

  /* Deselect the FLASH and unlock the SPI bus */

  SPI_SELECT(priv->dev, SPIDEV_FLASH, false);
  is25xp_unlock(priv->dev);
  fvdbg("return nbytes: %d\n", (int)nbytes);
  return nbytes;
}

/************************************************************************************
 * Name: is25xp_write
 ************************************************************************************/

#ifdef CONFIG_MTD_BYTE_WRITE
static ssize_t is25xp_write(FAR struct mtd_dev_s *dev, off_t offset, size_t nbytes,
                         FAR const uint8_t *buffer)
{
  FAR struct is25xp_dev_s *priv = (FAR struct is25xp_dev_s *)dev;
  int    startpage;
  int    endpage;
  int    count;
  int    index;
  int    pagesize;
  int    bytestowrite;

  fvdbg("offset: %08lx nbytes: %d\n", (long)offset, (int)nbytes);

  /* We must test if the offset + count crosses one or more pages
   * and perform individual writes.  The devices can only write in
   * page increments.
   */

  startpage = offset / (1 << priv->pageshift);
  endpage = (offset + nbytes) / (1 << priv->pageshift);

  if (startpage == endpage)
    {
      /* All bytes within one programmable page.  Just do the write. */

      is25xp_bytewrite(priv, buffer, offset, nbytes);
    }
  else
    {
      /* Write the 1st partial-page */

      count = nbytes;
      pagesize = (1 << priv->pageshift);
      bytestowrite = pagesize - (offset & (pagesize-1));
      is25xp_bytewrite(priv, buffer, offset, bytestowrite);

      /* Update offset and count */

      offset += bytestowrite;
      count -=  bytestowrite;
      index = bytestowrite;

      /* Write full pages */

      while (count >= pagesize)
        {
          is25xp_bytewrite(priv, &buffer[index], offset, pagesize);

          /* Update offset and count */

          offset += pagesize;
          count -= pagesize;
          index += pagesize;
        }

      /* Now write any partial page at the end */

      if (count > 0)
        {
          is25xp_bytewrite(priv, &buffer[index], offset, count);
        }

      priv->lastwaswrite = true;
    }

  return nbytes;
}
#endif /* CONFIG_MTD_BYTE_WRITE */

/************************************************************************************
 * Name: is25xp_ioctl
 ************************************************************************************/

static int is25xp_ioctl(FAR struct mtd_dev_s *dev, int cmd, unsigned long arg)
{
  FAR struct is25xp_dev_s *priv = (FAR struct is25xp_dev_s *)dev;
  int ret = -EINVAL; /* Assume good command with bad parameters */

  fvdbg("cmd: %d \n", cmd);

  switch (cmd)
    {
      case MTDIOC_GEOMETRY:
        {
          FAR struct mtd_geometry_s *geo = (FAR struct mtd_geometry_s *)((uintptr_t)arg);
          if (geo)
            {
              /* Populate the geometry structure with information need to know
               * the capacity and how to access the device.
               *
               * NOTE: that the device is treated as though it where just an array
               * of fixed size blocks.  That is most likely not true, but the client
               * will expect the device logic to do whatever is necessary to make it
               * appear so.
               */

              geo->blocksize = (1 << priv->pageshift);
              geo->erasesize    = (1 << priv->sectorshift);
              geo->neraseblocks = priv->nsectors;

              ret = OK;

              fvdbg("blocksize: %d erasesize: %d neraseblocks: %d\n",
                    geo->blocksize, geo->erasesize, geo->neraseblocks);
            }
        }
        break;

      case MTDIOC_BULKERASE:
        {
            /* Erase the entire device */

            is25xp_lock(priv->dev);
            ret = is25xp_bulkerase(priv);
            is25xp_unlock(priv->dev);
        }
        break;

      case MTDIOC_XIPBASE:
      default:
        ret = -ENOTTY; /* Bad command */
        break;
    }

  fvdbg("return %d\n", ret);
  return ret;
}

/************************************************************************************
 * Public Functions
 ************************************************************************************/

/************************************************************************************
 * Name: is25xp_initialize
 *
 * Description:
 *   Create an initialize MTD device instance.  MTD devices are not registered
 *   in the file system, but are created as instances that can be bound to
 *   other functions (such as a block or character driver front end).
 *
 ************************************************************************************/

FAR struct mtd_dev_s *is25xp_initialize(FAR struct spi_dev_s *dev)
{
  FAR struct is25xp_dev_s *priv;
  int ret;

  fvdbg("dev: %p\n", dev);

  /* Allocate a state structure (we allocate the structure instead of using
   * a fixed, static allocation so that we can handle multiple FLASH devices.
   * The current implementation would handle only one FLASH part per SPI
   * device (only because of the SPIDEV_FLASH definition) and so would have
   * to be extended to handle multiple FLASH parts on the same SPI bus.
   */

  priv = (FAR struct is25xp_dev_s *)kmm_zalloc(sizeof(struct is25xp_dev_s));
  if (priv)
    {
      /* Initialize the allocated structure. (unsupported methods were
       * nullified by kmm_zalloc).
       */

      priv->mtd.erase  = is25xp_erase;
      priv->mtd.bread  = is25xp_bread;
      priv->mtd.bwrite = is25xp_bwrite;
      priv->mtd.read   = is25xp_read;
#ifdef CONFIG_MTD_BYTE_WRITE
      priv->mtd.write  = is25xp_write;
#endif
      priv->mtd.ioctl  = is25xp_ioctl;
      priv->dev        = dev;
      priv->lastwaswrite = false;

      /* Deselect the FLASH */

      SPI_SELECT(dev, SPIDEV_FLASH, false);

      /* Identify the FLASH chip and get its capacity */

      ret = is25xp_readid(priv);
      if (ret != OK)
        {
          /* Unrecognized! Discard all of that work we just did and return NULL */

          fdbg("Unrecognized\n");
          kmm_free(priv);
          priv = NULL;
        }
      else
        {
          /* Make sure that the FLASH is unprotected so that we can write into it */

          is25xp_unprotect(priv);
        }
    }

  /* Register the MTD with the procfs system if enabled */

#ifdef CONFIG_MTD_REGISTRATION
  mtd_register(&priv->mtd, "is25xp");
#endif

  /* Return the implementation-specific state structure as the MTD device */

  fvdbg("Return %p\n", priv);
  return (FAR struct mtd_dev_s *)priv;
}
