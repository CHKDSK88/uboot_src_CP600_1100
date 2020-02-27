/*
 * (C) Copyright 2001
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __SPI_FLASH_H__
#define __SPI_FLASH_H__

#ifdef SOFAWARE_BOOT_VER
#include <common.h>
#include <command.h>
#endif
#include <linux/types.h>


#define SPI_EEPROM_PAGE_SIZE 528
#define SPI_EEPROM_MAX_PAGES 4096

int spi_get_page_size(void);
int spi_read(int pg_num, unsigned char buf[SPI_EEPROM_PAGE_SIZE]);
void spi_erase(void);
int spi_write_file(unsigned long from, unsigned long number);
//int spi_write_page(unsigned int page_num, unsigned char * buf);
void do_spi_init(void);

#endif // __SPI_FLASH_H__
