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

#define TEXTBUF_ROWS 24
#define TEXTBUF_COLS 0x100

#define PATHBUF_LEN 0x100

#define print_textbuf(__format__, ...) \
do { \
	sprintf(textbuf_data + textbuf_index * TEXTBUF_COLS, __format__, ##__VA_ARGS__); \
	textbuf_index = (textbuf_index + 1) % TEXTBUF_ROWS; \
} while(0)

typedef struct {
	char src[PATHBUF_LEN];
	char dst[PATHBUF_LEN];
	char header[4];
} data_info;

typedef struct {
	char buf[PATHBUF_LEN];
} path_info;

FileProcessParam progress_param;

static int textbuf_index = 0;
static char textbuf_data[TEXTBUF_COLS * TEXTBUF_ROWS] = {0};

static data_info* data_info_arr;
static int data_info_cnt = 0;

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
	char* buf = NULL;
	int len = allocateReadFile(path, (void**)&buf);
	//print_textbuf("load_meta: %s, %d", path, len);
	if(len < 0) return -1;

	int cnt = 0;
	int step = 0;
	int v = 0;
	char header_buf[8] = {0};
	for(int u = 0; u < len; ++u) {
		char c = buf[u];

		switch(c) {
		case '\r':
			continue;
		case '\n':
			for(v = 0; v < 4; ++v) {
				data_info_arr[cnt].header[v] = 
					(hexchar2uint(header_buf[v << 1]) << 4) |
					hexchar2uint(header_buf[(v << 1) + 1])
				;
			}
			step = 0;
			v = 0;
			++cnt;
			break;
		case ' ':
			data_info_arr[cnt].dst[v] = 0; //null terminate.
			step = 1;
			v = 0;
			break;
		default:
			if((uint32_t)c >= 0x80) continue;
			if(step == 0)
				data_info_arr[cnt].dst[v++] = c;
			else
				header_buf[v++] = c;			
			break;
		}
	}

	//for(int u = 0; u < cnt; ++u)
	//	print_textbuf("#%s %x", data_info_arr[u].dst, *(uint32_t*)data_info_arr[u].header);

	free(buf);
	return cnt;
}

static int process_data(char* path, uint32_t index) {
	if(data_info_arr[index].src[0]) return -1; //duplicated entry.

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
	data_info_arr = calloc(0x1000, sizeof(data_info));

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
					if(installPackage(path, &progress_param)) {
						print_textbuf("Failed to install package.");
						return;
					}
					*progress_param.value = progress_param.max;
					removePath(path, NULL);
				} else if(!memcmp(n->name, "qmeta.mp4", 9)) {
					print_textbuf("Loading meta from %s...", path);
					data_info_cnt = load_meta(path);
					if(data_info_cnt < 0) {
						print_textbuf("Failed to load meta.");
						return;
					}
					removePath(path, NULL);
				}
				
				if(n == q.tail) break;
				n = n->next;
			}
		}

		if(m == p.tail) break;
		m = m->next;
	}
				
	print_textbuf("Resolving meta...");
	for(int u = 0; u < data_info_cnt; ++u) {
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

		font_draw_string(10, 10, RGBA8(0x80, 0xFF, 0xFF, 0xFF), "Quick Installer");

		uint64_t prog = (*progress_param.value * 100) / progress_param.max;
		if(prog < 100) {
			font_draw_stringf(900, 10, RGBA8(0xFF, 0x80, 0x00, 0xFF), "%2d%%", prog);
		}

		for (int u = 0; u < TEXTBUF_ROWS; ++u) {
			int row = (u + textbuf_index) % TEXTBUF_ROWS;
			if(u == (TEXTBUF_ROWS - 1))
				font_draw_string(10, 56 + u * 20, RGBA8(0xFF, 0xFF, 0x80, 0xFF), textbuf_data + row * TEXTBUF_COLS);
			else
				font_draw_string(10, 56 + u * 20, RGBA8(0xFF, 0xFF, 0xFF, 0xFF), textbuf_data + row * TEXTBUF_COLS);
		}

		swap_buffers();
		sceDisplayWaitVblankStart();
	}
}

int main() {
	progress_param.value = malloc(sizeof(uint64_t));
	*progress_param.value = 1;
	progress_param.max = 1;
	progress_param.SetProgress = NULL;
	progress_param.cancelHandler = NULL;

	init_video();
	begin_working_thread();
	rendering_loop();
	end_video();
	sceKernelExitProcess(0);
	return 0;
}
