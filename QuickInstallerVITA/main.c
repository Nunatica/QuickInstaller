#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <malloc.h>
#include <stdint.h>
#include <sys/time.h>

#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/types.h>
#include <psp2/moduleinfo.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>

#include "draw.h"
#include "file.h"
#include "global.h"

#define TEXTBUF_ROWS 24
#define TEXTBUF_COLS 0x100

#define print_textbuf(__format__, ...) \
do { \
	sprintf(textbuf_data + textbuf_index * TEXTBUF_COLS, __format__, ##__VA_ARGS__); \
	textbuf_index = (textbuf_index + 1) % TEXTBUF_ROWS; \
} while(0)

typedef struct {
	char* path;
	char header[4];
	int path_length;
} data_info;

static int textbuf_index = 0;
static char textbuf_data[TEXTBUF_COLS * TEXTBUF_ROWS] = {0};

static data_info* dest_data;
static int count_data = 0;
static int count_inst = 0;
static int progress_pkg = -1;
static int progress_pkg_cur = 0;
static int progress_pkg_max = 0;

static uint8_t hexchar2uint(char c)
{
	if(c >= '0' && c <= '9') return c - '0';
	if(c >= 'A' && c <= 'F') return c - 'A' + 10;
	if(c >= 'a' && c <= 'f') return c - 'a' + 10;
	return 0xFF;
}

static void load_meta(char* path) {

	dest_data = calloc(0x10000, sizeof(data_info));
	char* buf = calloc(0x1000000, 1);

	int len = ReadFile(path, buf, 0x1000000);
	char* p = buf;
	char* end = buf + len;

	print_textbuf("load_meta: %s, %d", path, len);

	int cnt = 0;
	while (1) {
		int u = 0;
		int step = 0;
		dest_data[cnt].path = calloc(0x100, 1);	
		char header_buf[8] = {0};

		while(1) {
			if (p >= end) {
				free(buf);
				return;
			}
			char c = *p++;
			if (c == '\r') continue;
			if (c == '\n') break;
			if (c == ' ') {
				dest_data[cnt].path_length = u;
				dest_data[cnt].path[u] = 0; //null terminate.
				step = 1;
				u = 0;
				continue;
			}

			if(step == 0)
				dest_data[cnt].path[u++] = c;
			else if(step == 1)
				header_buf[u++] = c;
		}

		for(u = 0; u < 4; ++u)
			dest_data[cnt].header[u] = (hexchar2uint(header_buf[u << 1]) << 4) | hexchar2uint(header_buf[(u << 1) + 1]);

		print_textbuf("# %s %02x%02x%02x%02x", dest_data[cnt].path,
			dest_data[cnt].header[0], dest_data[cnt].header[1], dest_data[cnt].header[2], dest_data[cnt].header[3]);
		++cnt;
	}
}

#if 0
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
#endif

int process_install_pkg(const char* path) {
	return -1;
}

static void main_worker_impl() {
	print_textbuf("main_worker_impl");
	FileList p, q;	
	memset(&p, 0, sizeof(FileList));
	memset(&q, 0, sizeof(FileList));
	//strcpy(p.path, HOME_PATH);
	strcpy(p.path, "ux0:video/");

	if (fileListGetEntries(&p, p.path)) {
		print_textbuf("Failed to get list.");
		return;
	}

	FileListEntry* m = p.head;
	if (p.length == 1) {
		print_textbuf("Files not found.");
		return;
	}

	//skip parent directory.
	m = m->next;

	while (1) {
		//print_textbuf("dir: %s", m->name);
	
		fileListEmpty(&q);
		strcpy(q.path, p.path);		
		strcpy(q.path + 10, m->name);
		//print_textbuf("dir-full: %s", q.path);

		if (!fileListGetEntries(&q, q.path) && q.length > 1) {
			FileListEntry* n = q.head;

			//skip parent directory.
			n = n->next;

			while(1) {
				char path[0x100] = {0};
				sprintf(path, "%s%s", q.path, n->name);
				print_textbuf("file: %s", path);

				if(!memcmp(n->name, "qmeta.mp4", 9)) {
					load_meta(path);
				}
				
				if(n == q.tail) break;
				n = n->next;
			}
		}

		if(m == p.tail) break;
		m = m->next;
	}

#if 0
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
	} while (h != p.tail);

	progress_pkg = 0;
	progress_pkg_max = count_inst;
	for (int u = 0; u < count_inst; ++u) {
		progress_pkg_cur = u;
		if (process_install_pkg(list_inst[u]) == 0) {
			print_textbuf("Installed. %s", list_inst[u]);
		} else {
			print_textbuf("Failed to install package. %s", list_inst[u]);
			return;
		}
	}
	progress_pkg = -1;

	for (int u = 0; u < count_data; ++u) {
		if (movePath(list_data[u], dest_data[u], MOVE_REPLACE, NULL) == 0) {
			print_textbuf("Moved %s", list_data[u]);
		} else {
			print_textbuf("Failed to move file. %s", list_data[u]);
			return;
		}
	}
#endif

	print_textbuf("Done.");
}

static int main_worker(SceSize args, void* p) {
	main_worker_impl();
	return sceKernelExitDeleteThread(0);
}

static void begin_working_thread() {
	SceUID thid = sceKernelCreateThread("mythread", main_worker, 0x10000100, 0x10000, 0, 0, NULL);
	if (thid >= 0) sceKernelStartThread(thid, 0, NULL);
}

static void rendering_loop() {

	/* Input variables */
	SceCtrlData pad;

	while (1) {
		clear_screen();

		/* Read controls and touchscreen */
		sceCtrlPeekBufferPositive(0, &pad, 1);

		font_draw_stringf(10, 10, RGBA8(0xFF, 0xFF, 0x00, 0xFF), "quick installer");

		if (progress_pkg >= 0) {
			font_draw_stringf(10, 30, RGBA8(0xFF, 0xFF, 0x00, 0x00), "progress_pkg: %d%, %d of %d", progress_pkg, progress_pkg_cur, progress_pkg_max);
		}

		for (int u = 0; u < TEXTBUF_ROWS; ++u) {
			int row = (u + textbuf_index) % TEXTBUF_ROWS;
			font_draw_string(10, 56 + u * 20, RGBA8(0xFF, 0xFF, 0xFF, 0xFF), textbuf_data + row * TEXTBUF_COLS);
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
