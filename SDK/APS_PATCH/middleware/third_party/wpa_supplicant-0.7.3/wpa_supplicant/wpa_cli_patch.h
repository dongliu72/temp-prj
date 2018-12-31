/*
 * WPA Supplicant - command line interface for wpa_supplicant daemon
 * Copyright (c) 2004-2010, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#ifndef __WPA_CLI_PATCH_H__
#define __WPA_CLI_PATCH_H__

#ifdef FLAG_OF_CBS_READ_WRITE_INFO
int wpa_cli_get_manuf_name(u8 *name);
int wpa_cli_set_manuf_name(u8 *name);
#endif

#endif

