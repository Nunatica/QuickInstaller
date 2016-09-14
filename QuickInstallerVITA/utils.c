/*
	VitaShell
	Copyright (C) 2015-2016, TheFloW

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "file.h"
#include "utils.h"
#include "bm.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <psp2/system_param.h>
#include <psp2/kernel/processmgr.h>

SceCtrlData pad;
uint32_t old_buttons, current_buttons, pressed_buttons, hold_buttons, hold2_buttons, released_buttons;

static int netdbg_sock = -1;
static void *net_memory = NULL;
static int net_init = -1;

static int lock_power = 0;

static uint64_t wallpaper_time_start = 0;
static int wallpaper_random_delay = 0;
static float wallpaper_alpha = 255.0f;

int power_tick_thread(SceSize args, void *argp) {
	while (1) {
		if (lock_power > 0) {
			sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
		}

		sceKernelDelayThread(10 * 1000 * 1000);
	}

	return 0;
}

void initPowerTickThread() {
	SceUID thid = sceKernelCreateThread("power_tick_thread", power_tick_thread, 0x10000100, 0x40000, 0, 0, NULL);
	if (thid >= 0)
		sceKernelStartThread(thid, 0, NULL);
}

void powerLock() {
	lock_power++;
}

void powerUnlock() {
	lock_power--;
	if (lock_power < 0) lock_power = 0;
}

void readPad() {
	static int hold_n = 0, hold2_n = 0;

	memset(&pad, 0, sizeof(SceCtrlData));
	sceCtrlPeekBufferPositive(0, &pad, 1);

	if (pad.ly < ANALOG_CENTER - ANALOG_THRESHOLD) {
		pad.buttons |= SCE_CTRL_LEFT_ANALOG_UP;
	} else if (pad.ly > ANALOG_CENTER + ANALOG_THRESHOLD) {
		pad.buttons |= SCE_CTRL_LEFT_ANALOG_DOWN;
	}

	if (pad.lx < ANALOG_CENTER - ANALOG_THRESHOLD) {
		pad.buttons |= SCE_CTRL_LEFT_ANALOG_LEFT;
	} else if (pad.lx > ANALOG_CENTER + ANALOG_THRESHOLD) {
		pad.buttons |= SCE_CTRL_LEFT_ANALOG_RIGHT;
	}

	if (pad.ry < ANALOG_CENTER - ANALOG_THRESHOLD) {
		pad.buttons |= SCE_CTRL_RIGHT_ANALOG_UP;
	} else if (pad.ry > ANALOG_CENTER + ANALOG_THRESHOLD) {
		pad.buttons |= SCE_CTRL_RIGHT_ANALOG_DOWN;
	}

	if (pad.rx < ANALOG_CENTER - ANALOG_THRESHOLD) {
		pad.buttons |= SCE_CTRL_RIGHT_ANALOG_LEFT;
	} else if (pad.rx > ANALOG_CENTER + ANALOG_THRESHOLD) {
		pad.buttons |= SCE_CTRL_RIGHT_ANALOG_RIGHT;
	}

	current_buttons = pad.buttons;
	pressed_buttons = current_buttons & ~old_buttons;
	hold_buttons = pressed_buttons;
	hold2_buttons = pressed_buttons;
	released_buttons = ~current_buttons & old_buttons;

	if (old_buttons == current_buttons) {
		hold_n++;
		if (hold_n >= 10) {
			hold_buttons = current_buttons;
			hold_n = 6;
		}

		hold2_n++;
		if (hold2_n >= 10) {
			hold2_buttons = current_buttons;
			hold2_n = 10;
		}
	} else {
		hold_n = 0;
		hold2_n = 0;
		old_buttons = current_buttons;
	}
}

int holdButtons(SceCtrlData *pad, uint32_t buttons, uint64_t time) {
	if ((pad->buttons & buttons) == buttons) {
		uint64_t time_start = sceKernelGetProcessTimeWide();

		while ((pad->buttons & buttons) == buttons) {
			sceKernelDelayThread(10 * 1000);
			sceCtrlPeekBufferPositive(0, pad, 1);

			if ((sceKernelGetProcessTimeWide() - time_start) >= time) {
				return 1;
			}
		}
	}

	return 0;
}

int hasEndSlash(char *path) {
	return path[strlen(path) - 1] == '/';
}

int removeEndSlash(char *path) {
	int len = strlen(path);

	if (path[len - 1] == '/') {
		path[len - 1] = '\0';
		return 1;
	}

	return 0;
}

int addEndSlash(char *path) {
	int len = strlen(path);
	if (len < MAX_PATH_LENGTH - 2) {
		if (path[len - 1] != '/') {
			strcat(path, "/");
			return 1;
		}
	}

	return 0;
}

void getSizeString(char *string, uint64_t size) {
	double double_size = (double)size;

	int i = 0;
	static char *units[] = { "B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB" };
	while (double_size >= 1024.0f) {
		double_size /= 1024.0f;
		i++;
	}

	sprintf(string, "%.*f %s", (i == 0) ? 0 : 2, double_size, units[i]);
}

int randomNumber(int low, int high) {
   return rand() % (high - low + 1) + low;
}

char *strcasestr(const char *haystack, const char *needle) {
	return boyer_moore(haystack, needle); 
}
