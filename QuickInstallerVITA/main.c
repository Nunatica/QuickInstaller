#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <stdint.h>
#include <sys/time.h>

#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/types.h>
#include <psp2/moduleinfo.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>

#include "draw.h"
#include "file.h"

#define HW_BUTTON_THRESHOLD 0x10000

#define TEXTBUF_ROWS 24
#define TEXTBUF_COLS 0x100

static int display_screen = HW_BUTTON_THRESHOLD + 1;
static int textbuf_index = 0;
static char textbuf_data[TEXTBUF_ROWS][TEXTBUF_COLS];

static char* textbuf_meta;
static char* list_data[0x100];
static char* list_inst[0x100];
static char* dest_data[0x100];
static int count_data = 0;
static int count_inst = 0;
static int progress_pkg = -1;
static int progress_pkg_cur = 0;
static int progress_pkg_max = 0;

static void print_textbuf(const char* format, ...) {
	va_list arglist;
	sprintf(textbuf_data[textbuf_index], format, arglist);

	if (textbuf_index == (TEXTBUF_ROWS - 1))
		textbuf_index = 0;	
	else
		textbuf_index += 1;
}

static void load_meta(FileListEntry* f) {
	textbuf_meta = calloc(0x1000000, 1);
	int len = ReadFile(f->name, textbuf_meta, 0x1000000);

	char* p = textbuf_meta;
	char* end = textbuf_meta + len;

	int cnt = 0;
	while (true) {
		int u = 0;
		dest[cnt] = calloc(0x100, 1);	
		while(true) {
			if (p >= end) return;
			char c = *p++;
			if (c == '\r') continue;
			if (c == '\n') break;			
			dest[cnt][u++] = c;
		}
		++cnt;
	}
}

static void load_inst(FileListEntry* f) {
	list_inst[count_inst] = malloc(f->name_length + 1);
	memcpy(list_inst[count_inst], f->name, f->name_length + 1);
	++count_inst;
}

static void load_data(FileListEntry* f) {
	list_data[count_data] = malloc(f->name_length + 1);
	memcpy(list_data[count_data], f->name, f->name_length + 1);
	++count_data;
}

int process_install_pkg(const char* path) {
	return -1;
}

static void main_worker_impl() {
	FileList p;

	if (fileListGetEntries(&p, "ux0:/video")) {
		print_textbuf("%s", "Failed to get list.");
		return;
	}

	FileListEntry* h = p.head;
	if (p.length == 0) {
		print_textbuf("%s", "Files not found.");
		return;
	}

	do {
		FileList q;
		fileListGetEntries(&q, h->name);
		if (q.length == 0) continue;

		FileListEntry* f = q.head;
		if (memcmp("qmeta.mp4", f->name + f->name_length - 9, 9) == 0) {
			load_meta(f);
		}
		else if (memcmp("qinst_", f->name + f->name_length - 6, 6) == 0) {
			load_inst(f);
		}
		else if (memcmp("qdata_", f->name + f->name_length - 6, 6) == 0) {
			load_data(f);
		}
		h = h->next;
	} while (h != files.tail);

	progress_pkg = 0;
	progress_pkg_max = count_inst;
	for (int u = 0; u < count_inst; ++u) {
		progress_pkg_cur = u;
		if (process_install_pkg(list_inst[u]) == 0) {
			print_textbuf("%s %s", "Installed. " list_inst[u]);
		} else {
			print_textbuf("%s %s", "Failed to install package.", list_inst[u]);
			return;
		}
	}
	progress_pkg = -1;

	for (int u = 0; u < count_data; ++u) {
		if (movePath(list_data[u], dest_data[u], MOVE_REPLACE, NULL) == 0) {
			print_textbuf("%s %s", "Moved. " list_data[u]);
		} else {
			print_textbuf("%s %s", "Failed to move file.", list_data[u]);
			return;
		}
	}

	print_textbuf("Done.");
}

static int main_worker(SceSize args, void* p) {
	main_worker_impl();
	return sceKernelExitDeleteThread(0);
}

static void begin_working_thread() {
	sceKernelCreateThread("mythread", main_worker, 0x10000100, 0x10000, 0, 0, NULL)
}

static void rendering_loop() {

	/* Input variables */
	SceCtrlData pad;

	memset(textbuf_data, 0, TEXTBUF_ROWS * TEXTBUF_COLS);

	while (1) {
		clear_screen();

		/* Read controls and touchscreen */
		sceCtrlPeekBufferPositive(0, &pad, 1);

		if (pad.buttons & SCE_CTRL_START) {
			if (display_screen > (HW_BUTTON_THRESHOLD << 1))
				display_screen = 0;
			else
				display_screen += 1;			
		}

		if (display_screen > HW_BUTTON_THRESHOLD) {
			if (progress_pkg >= 0) {
				font_draw_stringf(10, 10 + u * 20, RGBA8(0xFF, 0xFF, 0xFF, 0xFF), "progress_pkg: %d%, %d of %d", progress_pkg, progress_pkg_cur, progress_pkg_max);
			}
			for (int u = 0; u < TEXTBUF_ROWS; ++u) {
				int row = (u + textbuf_index) % TEXTBUF_ROWS;
				font_draw_string(10, 50 + u * 20, RGBA8(0xFF, 0xFF, 0xFF, 0xFF), textbuf_data[row]);
			}
		}

		swap_buffers();
		sceDisplayWaitVblankStart();
	}
}

int main() {
	init_video();
	begin_working_thread();
	rendering_loop();
	end_video();
	sceKernelExitProcess(0);
	return 0;
}
