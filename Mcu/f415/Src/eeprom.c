#include "eeprom.h"
#include "targets.h"

#include <string.h>
#include <stdbool.h>

//#pragma GCC optimize("O0")

#include "at32f415_flash.h"

/*
  the F415 can be either 1k or 2k sector size. The 256k flash part has
  2k sector size. We only support ESCs with 128k flash or less, so 1k
  sector size, but it is useful to work with 2k sector size when using
  a F415 dev board like the AT-Start F415, so we detect here.
 */
static inline uint32_t sector_size()
{
    const uint16_t *F_SIZE = (const uint16_t *)0x1FFFF7E0;
    if (*F_SIZE <= 128) {
	// 1k sectors for 128k flash or less
	return 1024;
    }
    // 256k flash is 2k sectors
    return 2048;
}

bool save_flash_nolib(const uint8_t* data, uint32_t length, uint32_t add)
{
  if ((add & 0x3) != 0 || (length & 0x3) != 0) {
    return false;
  }
  /*
    we need the data to be 32 bit aligned
   */
  const uint32_t word_length = length / 4;
  const uint32_t sector_sz = sector_size();
  const uint32_t end_addr = add + length;

  // unlock flash
  flash_unlock();

  // Erase all sectors that the write will cover
  // Calculate the first sector address
  uint32_t first_sector_addr = (add / sector_sz) * sector_sz;
  // Calculate the last sector address
  uint32_t last_sector_addr = ((end_addr - 1) / sector_sz) * sector_sz;

  // Erase all sectors in the range
  for (uint32_t sector_addr = first_sector_addr; sector_addr <= last_sector_addr; sector_addr += sector_sz) {
    // Check if sector is already erased (all 0xFF)
    const uint32_t *sector_ptr = (const uint32_t *)sector_addr;
    bool needs_erase = false;
    for (uint32_t i = 0; i < sector_sz / 4; i++) {
      if (sector_ptr[i] != 0xFFFFFFFF) {
        needs_erase = true;
        break;
      }
    }

    if (needs_erase) {
      flash_status_type erase_status = flash_sector_erase(sector_addr);
      if (erase_status != FLASH_OPERATE_DONE) {
        flash_lock();
        return false;
      }
    }
  }

  uint32_t index = 0;
  while (index < word_length) {
    uint32_t word;
    memcpy(&word, &data[index*4], sizeof(word));
    flash_status_type program_status = flash_word_program(add + (index * 4), word);
    if (program_status != FLASH_OPERATE_DONE) {
      flash_lock();
      return false;
    }
    flash_flag_clear(FLASH_PROGRAM_ERROR | FLASH_EPP_ERROR | FLASH_OPERATE_DONE);
    index++;
  }
  flash_lock();

  // ensure data is correct
  return memcmp(data, (const void *)add, length) == 0;
}

void read_flash_bin(uint8_t* data, uint32_t add, int out_buff_len)
{
    memcpy(data, (void*)add, out_buff_len);
}
