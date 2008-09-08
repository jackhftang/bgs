/* See LICENSE file for copyright and license details.
 *
 * To understand bgs , start reading main().
 */
#include <stdlib.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <Imlib2.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

/* macros */
#define MIN(a, b)       ((a) < (b) ? (a) : (b))
#define LENGTH(x)       (sizeof x / sizeof x[0])

/* typedefs */
typedef struct {
	int x, y, w, h;
} Monitor;

/* function declarations */
static void cleanup(void);
static void die(const char *errstr);
static void drawbg(void);
static void run(void);
static void setup(char *images[], int c);
static void updategeom(void);

/* variables */
static int sx, sy, sw, sh;
static int depth, screen;
static Bool center  = False;
static Bool running = False;
static Display *dpy;
static Window root;
static Visual *vis;
static Colormap cm;
static int nmonitor, nimage;
static Monitor monitors[8];
static Imlib_Image images[LENGTH(monitors)];

/* function implementations */
void
cleanup(void) {
	int i;

	for(i = 0; i < nimage; i++) {
		imlib_context_set_image(images[i]);
		imlib_free_image_and_decache();
	}
}

void
die(const char *errstr) {
	fputs(errstr, stderr);
	exit(EXIT_FAILURE);
}

void
drawbg(void) {
	int i, w, h, tmp;
	Pixmap pm;
	Imlib_Image tmpimg, buffer;

	pm = XCreatePixmap(dpy, root, sw, sh, depth);
	if(!(buffer = imlib_create_image(sw, sh)))
		die("Error: Cannot allocate buffer.\n");
	imlib_context_set_blend(1);
	for(i = 0; i < nmonitor; i++) {
		imlib_context_set_image(images[i % nimage]);
		w = imlib_image_get_width();
		h = imlib_image_get_height();
		if(!(tmpimg = imlib_clone_image()))
			die("Error: Cannot clone image.\n");
		imlib_context_set_image(tmpimg);
		if((monitors[i].w > monitors[i].h && w < h) || (monitors[i].w < monitors[i].h && w > h)) {
			imlib_image_orientate(1);
			tmp = w;
			w = h;
			h = tmp;
		}
		imlib_context_set_image(buffer);
		if(center)
			imlib_blend_image_onto_image(tmpimg, 0, 0, 0, w, h,
					(monitors[i].w - w ) / 2, (monitors[i].h - h) / 2, w, h);
		else
			imlib_blend_image_onto_image(tmpimg, 0, 0, 0, w, h,
					monitors[i].x, monitors[i].y, monitors[i].w, monitors[i].h);
		imlib_context_set_image(tmpimg);
		imlib_free_image();
	}
	imlib_context_set_blend(0);
	imlib_context_set_image(buffer);
	imlib_context_set_drawable(root);
	imlib_render_image_on_drawable(0, 0);
	imlib_context_set_drawable(pm);
	imlib_render_image_on_drawable(0, 0);
	XSetWindowBackgroundPixmap(dpy, root, pm);
	imlib_context_set_image(buffer);
	imlib_free_image_and_decache();
	XFreePixmap(dpy, pm);
}

void
run(void) {
	XEvent ev;

	drawbg();
	while(running) {
		XNextEvent(dpy, &ev);
		if(ev.type == ConfigureNotify) {
			sw = ev.xconfigure.width;
			sh = ev.xconfigure.height;
			updategeom();
			drawbg();
		}
	}
}

void
setup(char *paths[], int c) {
	int i;

	/* Loading images */
	for(nimage = i = 0; i < c && i < LENGTH(images); i++) {
		if((images[nimage] = imlib_load_image(paths[i])))
			nimage++;
		else {
			fprintf(stderr, "Warning: Cannot load file `%s`. Ignoring.\n", paths[nimage]);
			continue;
		}
	}
	if(nimage == 0)
		die("Error: No image to draw.\n");

	/* set up X */
	screen = DefaultScreen(dpy);
	vis = DefaultVisual(dpy, screen);
	depth = DefaultDepth(dpy, screen);
	cm = DefaultColormap(dpy, screen);
	root = RootWindow(dpy, screen);
	XSelectInput(dpy, root, StructureNotifyMask);
	sx = sy = 0;
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	updategeom();

	/* set up Imlib */
	imlib_context_set_display(dpy);
	imlib_context_set_visual(vis);
	imlib_context_set_colormap(cm);
}

void
updategeom(void) {
#ifdef XINERAMA
	int i;
	XineramaScreenInfo *info = NULL;

	/* window area geometry */
	if(XineramaIsActive(dpy) && (info = XineramaQueryScreens(dpy, &nmonitor))) {
		nmonitor = MIN(nmonitor, LENGTH(monitors));
		for(i = 0; i < nmonitor; i++) {
			monitors[i].x = info[i].x_org;
			monitors[i].y = info[i].y_org;
			monitors[i].w = info[i].width;
			monitors[i].h = info[i].height;
		}
		XFree(info);
	}
	else
#endif
	{
		nmonitor = 1;
		monitors[0].x = sx;
		monitors[0].y = sy;
		monitors[0].w = sw;
		monitors[0].h = sh;
	}
}

int
main(int argc, char *argv[]) {
	int i;

	for(i = 1; i < argc && argv[i][0] == '-' && argv[i][0] != '\0' &&
			argv[i][2] == '\0'; i++)
		switch(argv[i][1]) {
		case 'c':
			center = True; break;
		case 'x':
			running = True; break;
		case 'v':
			die("bgs-"VERSION", © 2008 bgs engineers, see LICENSE for details\n");
		default:
			die("usage: bgs [-v] [-c] [-x] [IMAGE]...\n");
		}
	if(!(dpy = XOpenDisplay(0)))
		die("bgs: cannot open display\n");
	setup(&argv[i], argc - i);
	run();
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
