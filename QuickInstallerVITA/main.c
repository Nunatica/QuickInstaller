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
#include <psp2/io/fcntl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>

#include "global.h"
#include "draw.h"
#include "file.h"
#include "package_installer.h"

#define REMOVE_INSTALLER_FILES 1

#define TEXTBUF_ROWS 24
#define TEXTBUF_COLS 0x100

#define PATHBUF_LEN 0x100

#define print_textbuf(__format__, ...) \
do { \
	sprintf(textbuf_data + textbuf_index * TEXTBUF_COLS, __format__, ##__VA_ARGS__); \
	textbuf_index = (textbuf_index + 1) % TEXTBUF_ROWS; \
} while(0)

typedef struct {
	char* src;
	char* dst;
	char header[4];
} data_info;

static int textbuf_index = 0;
static char textbuf_data[TEXTBUF_COLS * TEXTBUF_ROWS] = {0};

static data_info* data_info_arr;
static int data_info_count = 0;

static uint8_t hexchar2uint(char c) {
	if(c >= '0' && c <= '9') return c - '0';
	if(c >= 'A' && c <= 'F') return c - 'A' + 10;
	if(c >= 'a' && c <= 'f') return c - 'a' + 10;
	return 0xFF;
}

static uint32_t read_data_index(char* filename) {
	return
		(hexchar2uint(filename[3]) << 24) |
		(hexchar2uint(filename[4]) << 16) |
		(hexchar2uint(filename[5]) << 8) |
		(hexchar2uint(filename[6]) << 0);
}

static int load_meta(char* path) {
	char* buf = calloc(0x1000000, 1);

	int len = ReadFile(path, buf, 0x1000000);
	char* p = buf;
	char* end = buf + len;

	//print_textbuf("load_meta: %s, %d", path, len);

	int cnt = 0;
	while (1) {
		int u = 0;
		int step = 0;
		data_info_arr[cnt].dst = calloc(PATHBUF_LEN, 1);	
		char header_buf[8] = {0};

		while(1) {
			if (p >= end) {
				free(buf);
				return cnt;
			}
			char c = *p++;
			if (c >= 0x80) continue;
			if (c == '\r') continue;
			if (c == '\n') break;
			if (c == ' ') {
				//data_info_arr[cnt].dst_length = u;
				data_info_arr[cnt].dst[u] = 0; //null terminate.
				step = 1;
				u = 0;
				continue;
			}

			if(step == 0)
				data_info_arr[cnt].dst[u++] = c;
			else if(step == 1)
				header_buf[u++] = c;
		}

		for(u = 0; u < 4; ++u)
			data_info_arr[cnt].header[u] = (hexchar2uint(header_buf[u << 1]) << 4) | hexchar2uint(header_buf[(u << 1) + 1]);

		//print_textbuf("# %s %02x%02x%02x%02x", data_info_arr[cnt].path, 
		//	data_info_arr[cnt].header[0], data_info_arr[cnt].header[1], data_info_arr[cnt].header[2], data_info_arr[cnt].header[3]);

		++cnt;
	}
}

static int process_data(char* path, uint32_t index) {
	if(data_info_arr[index].src) return -1; //duplicated entry.

	data_info_arr[index].src = calloc(PATHBUF_LEN, 1);
	strcpy(data_info_arr[index].src, path);
	return 0;
}

static int overwrite_file(char *file, void *buf, int size) {
	SceUID fd = sceIoOpen(file, SCE_O_WRONLY, 0777);
	if (fd < 0) return -1;

	sceIoLseek(fd, 0, SEEK_SET);
	int written = sceIoWrite(fd, buf, size);
	sceIoClose(fd);

	return written;
}

static int resolve_meta(int index) {
	data_info* t = data_info_arr + index;

	if(overwrite_file(t->src, t->header, 4) == -1) {
		print_textbuf("Failed to overwrite header.");
		return -1;
	}
	
	if(sceIoRename(t->src, t->dst)) {
		print_textbuf("Failed to move file.");
		print_textbuf("src: %s", t->src);
		print_textbuf("dst: %s", t->dst);
		return -1;
	}
	
	return 0;
}

static void main_worker_impl() {
	data_info_arr = calloc(0x10000, sizeof(data_info));

	FileList p, q;	
	memset(&p, 0, sizeof(FileList));
	memset(&q, 0, sizeof(FileList));
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

	print_textbuf("Searching files...");	
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
				char path[PATHBUF_LEN] = {0};
				sprintf(path, "%s%s", q.path, n->name);
				//print_textbuf("file: %s", path);

				if(!memcmp(n->name, "qd_", 3)) {
					//print_textbuf("Found data file: %s", path);
					if(process_data(path, read_data_index(n->name))) {						
						print_textbuf("Duplicated file is found.");
						return;
					}
				} else if(!memcmp(n->name, "qinst_", 6)) {
					print_textbuf("Installing %s...", path);
					if(installPackage(path)) {
						print_textbuf("Failed to install package.");
						return;
					}
#if REMOVE_INSTALLER_FILES
					removePath(path, NULL);
#endif
				} else if(!memcmp(n->name, "qmeta.mp4", 9)) {
					print_textbuf("Loading meta from %s...", path);
					data_info_count = load_meta(path);
#if REMOVE_INSTALLER_FILES
					removePath(path, NULL);
#endif
				}				
				
				if(n == q.tail) break;
				n = n->next;
			}
		}

		if(m == p.tail) break;
		m = m->next;
	}
				
	print_textbuf("Resolving meta...");	
	for(int u = 0; u < data_info_count; ++u) {
		if(resolve_meta(u)) {
			print_textbuf("Failed to resolve %s.", data_info_arr[u].src);
			return;
		}
	}

	print_textbuf("Done.");
}

#if 0
static void main_worker_impl() {
	print_textbuf("main_worker_impl");
	print_textbuf("Installing VPK...");
	int result = installPackage("ux0:homebrew/mgba.vpk");
	if(result)
		print_textbuf("Failed with code: %d.", result);
	else
		print_textbuf("Done.");
}
#endif

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
