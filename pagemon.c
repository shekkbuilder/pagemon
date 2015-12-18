#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <libgen.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <ncurses.h>

WINDOW *mainwin;
static bool resized;

#define APP_NAME	"pagemon"
#define MAX_MMAPS	(8192)
#define PAGE_SIZE	(4096)

enum {
	WHITE_RED = 1,
	WHITE_BLUE,
	WHITE_YELLOW,
	WHITE_CYAN,
	WHITE_GREEN,
	WHITE_BLACK,
	BLACK_WHITE,
	CYAN_BLUE,
	RED_BLUE,
	YELLOW_BLUE,
	BLACK_GREEN,
	BLACK_YELLOW,
	YELLOW_RED,
	YELLOW_BLACK,
};

typedef struct {
	unsigned long begin;
	unsigned long end;
	char attr[5];
	char dev[6];
	char name[NAME_MAX + 1];
} map_t;

typedef struct {
	unsigned long addr;
	unsigned int  mapping;
} page_t;

int read_mmaps(const char *filename, map_t *maps, const unsigned int max, page_t **pages, unsigned long *npages)
{
	FILE *fp;
	unsigned long n = 0, i, j;
	char buffer[4096];
	page_t *page;

	*pages = NULL;
	*npages = 0UL;

	fp = fopen(filename, "r");
	if (fp == NULL)
		return 0;

	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		maps[n].name[0] = '\0';
		sscanf(buffer, "%lx-%lx %5s %*s %6s %*d %s",
			&maps[n].begin,
			&maps[n].end,
			maps[n].attr,
			maps[n].dev,
			maps[n].name);

		*npages += (maps[n].end - maps[n].begin) / PAGE_SIZE;
		maps[n].end--;
			
		n++;
		if (n >= max)
			break;
	}
	fclose(fp);

	*pages = page = malloc(*npages * sizeof(page_t));

	for (i = 0; i < n; i++) {
		unsigned long count = (maps[i].end - maps[i].begin) / PAGE_SIZE;
		unsigned long addr = maps[i].begin;

		for (j = 0; j < count; j++) {
			page->addr = addr;
			page->mapping = i;
			addr += PAGE_SIZE;
			page++;
		}
	}

	return i;
}

void handle_winch(int sig)
{
	(void)sig;

	resized = true;
}

void show_usage(void)
{
	printf(APP_NAME ", version " VERSION "\n\n"
		"Usage: " APP_NAME " [options] pid\n"
		" -h help\n");
}

int main(int argc, char **argv)
{
	unsigned long addr = 0;
	unsigned long page_size = PAGE_SIZE;
	unsigned long npages;
	unsigned long page_index = 0, tmp_index;
	bool do_run = true;
	pid_t pid = -1;
	char path_refs[PATH_MAX];
	char path_map[PATH_MAX];
	char path_mmap[PATH_MAX];
	map_t mmaps[MAX_MMAPS], *mmap;
	int tick = 0;
	int blink = 0;
	int xpos = 0, ypos = 0;
	bool page_view = false;
	page_t *pages;
	unsigned long zoom = 1;

	for (;;) {
		int c = getopt(argc, argv, "h");

		if (c == -1)
			break;
		switch (c) {
		case 'h':
			break;
		default:
			show_usage();
			exit(EXIT_FAILURE);
		}
	}

	if (optind < argc) {
		printf("%s\n", argv[optind]);
		pid = strtol(argv[optind], NULL, 10);
		if (errno) {
			fprintf(stderr, "Invalid pid value\n");
			exit(EXIT_FAILURE);
		}
	} else {
		show_usage();
		exit(EXIT_FAILURE);
	}

	snprintf(path_refs, sizeof(path_refs),
		"/proc/%i/clear_refs", pid);
	snprintf(path_map, sizeof(path_map),
		"/proc/%i/pagemap", pid);
	snprintf(path_mmap, sizeof(path_mmap),
		"/proc/%i/maps", pid);

	resized = false;

	initscr();
	start_color();
	cbreak();
	noecho();
	nodelay(stdscr, 1);
	keypad(stdscr, 1);
	curs_set(0);
	mainwin = newwin(LINES, COLS, 0, 0);

	init_pair(WHITE_RED, COLOR_WHITE, COLOR_RED);
	init_pair(WHITE_BLUE, COLOR_WHITE, COLOR_BLUE);
	init_pair(WHITE_YELLOW, COLOR_WHITE, COLOR_YELLOW);
	init_pair(WHITE_CYAN, COLOR_WHITE, COLOR_CYAN);
	init_pair(WHITE_GREEN, COLOR_WHITE, COLOR_GREEN);
	init_pair(WHITE_BLACK, COLOR_WHITE, COLOR_BLACK);

	init_pair(BLACK_WHITE, COLOR_BLACK, COLOR_WHITE);
	init_pair(CYAN_BLUE, COLOR_CYAN, COLOR_BLUE);
	init_pair(RED_BLUE, COLOR_RED, COLOR_BLUE);
	init_pair(YELLOW_BLUE, COLOR_YELLOW, COLOR_BLUE);
	init_pair(BLACK_GREEN, COLOR_BLACK, COLOR_GREEN);
	init_pair(BLACK_YELLOW, COLOR_BLACK, COLOR_YELLOW);
	init_pair(YELLOW_RED, COLOR_YELLOW, COLOR_RED);
	init_pair(YELLOW_BLACK, COLOR_YELLOW, COLOR_BLACK);

	signal(SIGWINCH, handle_winch);

	do {
		int ch;
		int i;
		int width_step = COLS - 17;
		int fd;

		read_mmaps(path_mmap, mmaps, MAX_MMAPS, &pages, &npages);

		if (resized) {
			delwin(mainwin);
			endwin();
			refresh();
			clear();
			if (COLS < 18)
				break;
			if (LINES < 5)
				break;
			width_step = COLS - 17;
			mainwin = newwin(LINES, COLS, 0, 0);
			resized = false;
		}

		wbkgd(mainwin, COLOR_PAIR(RED_BLUE));

		tick++;
		if (tick > 10) {
			tick = 0;
			fd = open(path_refs, O_RDWR);
			if (fd < 0)
				break;
			if (write(fd, "4", 1) < 0)
				break;
			(void)close(fd);
		}


		ch = getch();

		wattrset(mainwin, COLOR_PAIR(WHITE_BLUE) | A_BOLD);
		mvwprintw(mainwin, LINES - 1, 0, "KEY: ");

		wattrset(mainwin, COLOR_PAIR(WHITE_RED));
		wprintw(mainwin, "A");
		wattrset(mainwin, COLOR_PAIR(WHITE_BLUE));
		wprintw(mainwin, " Mapped anon/file, ");

		wattrset(mainwin, COLOR_PAIR(WHITE_YELLOW));
		wprintw(mainwin, "R");
		wattrset(mainwin, COLOR_PAIR(WHITE_BLUE));
		wprintw(mainwin, " in RAM, ");

		wattrset(mainwin, COLOR_PAIR(WHITE_CYAN));
		wprintw(mainwin, "D");
		wattrset(mainwin, COLOR_PAIR(WHITE_BLUE));
		wprintw(mainwin, " Dirty, ");

		wattrset(mainwin, COLOR_PAIR(WHITE_CYAN));
		wprintw(mainwin, "S");
		wattrset(mainwin, COLOR_PAIR(WHITE_BLUE));
		wprintw(mainwin, " Swap, ");

		wattrset(mainwin, COLOR_PAIR(BLACK_WHITE));
		wprintw(mainwin, ".");
		wattrset(mainwin, COLOR_PAIR(WHITE_BLUE));
		wprintw(mainwin, " not in RAM");

		wattrset(mainwin, COLOR_PAIR(BLACK_WHITE) | A_BOLD);
		fd = open(path_map, O_RDONLY);
		if (fd < 0)
			break;

		tmp_index = page_index;
		for (i = 1; i < LINES - 1; i++) {
			int j;
			uint64_t info;

			addr = pages[tmp_index].addr;
			wattrset(mainwin, COLOR_PAIR(BLACK_WHITE));
			mvwprintw(mainwin, i, 0, "%16.16lx %s", addr);

			for (j = 0; j < width_step; j++) {
				char state = '.';
			
				addr = pages[tmp_index].addr;
				lseek(fd, sizeof(uint64_t) * (addr / page_size), SEEK_SET);
				if (read(fd, &info, sizeof(info)) < 0)
					break;

				wattrset(mainwin, COLOR_PAIR(BLACK_WHITE));

				if (info & ((uint64_t)1 << 63)) {
					wattrset(mainwin, COLOR_PAIR(WHITE_YELLOW));
					state = 'R';
				}
				if (info & ((uint64_t)1 << 62)) {
					wattrset(mainwin, COLOR_PAIR(WHITE_GREEN));
					state = 'S';
				}
				if (info & ((uint64_t)1 << 61)) {
					wattrset(mainwin, COLOR_PAIR(WHITE_RED));
					state = 'M';
				}
				if (info & ((uint64_t)1 << 55)) {
					wattrset(mainwin, COLOR_PAIR(WHITE_CYAN));
					state = 'D';
				}
				mvwprintw(mainwin, i, 17 + j, "%c", state);
				
				tmp_index += zoom;
				if (tmp_index > npages) {
					tmp_index = 0;
				}
			}
		}
		tmp_index = page_index + (xpos + (ypos * width_step));
		if (tmp_index >= npages)
			tmp_index = 0;
		mmap = &mmaps[pages[tmp_index].mapping];
		
		wattrset(mainwin, COLOR_PAIR(WHITE_BLUE) | A_BOLD);
		mvwprintw(mainwin, 0, 0, "Pagemon 0x%16.16lx Zoom x %u ",
			pages[tmp_index].addr, zoom);
		wprintw(mainwin, "%s %s %-20.20s",
			mmap->attr,
			mmap->dev,
			mmap->name[0] == '\0' ?
				"[Anonymous]" :
				basename(mmaps[i].name));
		wattrset(mainwin, A_NORMAL);

		if (page_view) {
			uint64_t info;

			wattrset(mainwin, COLOR_PAIR(WHITE_BLUE) | A_BOLD);
			mvwprintw(mainwin, 3, 4, " Page:   0x%16.16x                  ", pages[tmp_index].addr);
			mvwprintw(mainwin, 4, 4, " Map:    0x%16.16lx-%16.16lx ",
				mmap->begin, mmap->end);
			mvwprintw(mainwin, 5, 4, " Device: %5.5s                               ", mmap->dev);
			mvwprintw(mainwin, 6, 4, " Prot:   %4.4s                                ", mmap->attr);
			mvwprintw(mainwin, 6, 4, " File:   %-20.20s ", basename(mmap->name));
			lseek(fd, sizeof(uint64_t) * (pages[tmp_index].addr / page_size), SEEK_SET);
			if (read(fd, &info, sizeof(info)) < 0)
				break;
			mvwprintw(mainwin, 7, 4, " Flag:   0x%16.16lx                  ", info);
			mvwprintw(mainwin, 8, 4, "   Present in RAM:      %3s                  ", 
				(info & ((uint64_t)1 << 63)) ? "Yes" : "No ");
			mvwprintw(mainwin, 9, 4, "   Present in Swap:     %3s                  ", 
				(info & ((uint64_t)1 << 62)) ? "Yes" : "No ");
			mvwprintw(mainwin, 10, 4, "   File or Shared Anon: %3s                  ", 
				(info & ((uint64_t)1 << 61)) ? "Yes" : "No ");
			mvwprintw(mainwin, 11, 4, "   Soft-dirty PTE:      %3s                  ", 
				(info & ((uint64_t)1 << 55)) ? "Yes" : "No ");
		}
		close(fd);

		blink++;
		wattrset(mainwin, A_BOLD | ((blink & 0x20) ? COLOR_PAIR(WHITE_YELLOW) : COLOR_PAIR(WHITE_RED)));
		mvwprintw(mainwin, ypos + 1, xpos + 17, " ");
		wattrset(mainwin, A_NORMAL);

		wrefresh(mainwin);
		refresh();

		switch (ch) {
		case 27:	/* ESC */
		case 'q':
		case 'Q':
			do_run = false;
			break;
		case '\t':
			page_view = !page_view;
			break;
		case '+':
			zoom++ ;
			if (zoom > 8)
				zoom = 8;
			break;
		case '-':
			zoom--;
			if (zoom < 1)
				zoom = 1;
			break;
		case KEY_DOWN:
			ypos++;
			break;
		case KEY_UP:
			ypos--;
			break;
		case KEY_LEFT:
			xpos--;
			break;
		case KEY_RIGHT:
			xpos++;
			break;
		case KEY_NPAGE:
			ypos += (LINES - 2) / 2;
			break;
		case KEY_PPAGE:
			ypos -= (LINES - 2) / 2;
			break;
		}
		if (xpos >= width_step) {
			xpos = 0;
			ypos++;
		}
		if (xpos < 0) {
			xpos = width_step - 1;
			ypos--;
		}
		if (ypos > LINES-3) {
			page_index += zoom * (width_step * (ypos - (LINES-3)));
			ypos = LINES-3;
		}
		if (ypos < 0) {
			page_index -= zoom * (width_step * (-ypos));
			ypos = 0;
		}
		if (page_index > npages) {
			page_index = 0;
		}
		free(pages);
		usleep(10000);
	} while (do_run);

	endwin();
	exit(EXIT_SUCCESS);
}

