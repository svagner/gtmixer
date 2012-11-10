/*
 *	This is mixer program for FreeBSD
 *
 * (C) Stanislav Putrya.
 *
 * You may do anything you wish with this program.
 *
 */

#include <sys/cdefs.h>

#include <stdlib.h>
#include <gtk/gtk.h>
#include <cairo/cairo.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/sysctl.h>

#include <unistd.h>
#include <sys/soundcard.h>

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <pthread.h>

#define gettext(x)      (x)

#include "gtmixer.h"

static pthread_mutex_t mixer_mutex;

const char	*names[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_NAMES;

static void	usage(int devmask, int recmask);
static int	res_name(const char *name, int mask);
static void	print_recsrc(int recsrc, int recmask, int sflag);

int
gui_init(int * ac, char *** av) {
    return gtk_init_check(ac, av);
}

static void destroy (GtkWidget *window, gpointer data)
{
	gtk_main_quit ();
}

void
cb_digits_scale_pcm(GtkWidget *widget, gpointer window)
{
	pcmstate=gtk_range_get_value(GTK_RANGE (hscalePcm));
#ifdef DEBUG	
	printf("==> New PCM state is %d\n", pcmstate);
#endif
	    pthread_mutex_lock(&mixer_mutex);
	set_mixer_state("pcm", pcmstate);
	    pthread_mutex_unlock(&mixer_mutex);
}

void
cb_digits_scale_vol(GtkWidget *widget, gpointer window)
{
	volstate=gtk_range_get_value(GTK_RANGE (hscaleVol));
#ifdef DEBUG	
	printf("==> New PCM state is %d\n", volstate);
#endif
	pthread_mutex_lock(&mixer_mutex);
	set_mixer_state("vol", volstate);
	pthread_mutex_unlock(&mixer_mutex);
}

void *TimerFunc (GtkStatusIcon *trayIcon, gpointer window)
{
	
	pthread_mutex_lock(&mixer_mutex);
	volstate=get_mixer_state("vol");
	pcmstate=get_mixer_state("pcm");
	pthread_mutex_unlock(&mixer_mutex);
	gtk_range_set_value(GTK_RANGE (hscaleVol), volstate);
	gtk_range_set_value(GTK_RANGE (hscalePcm), pcmstate);
	if (volstate > 50 && pcmstate > 50)
	    gtk_status_icon_set_from_file (trayIcon, TRAY_INMUTE);
	if ((volstate < 50 && volstate > 0) && (pcmstate < 50 && pcmstate > 0))
	    gtk_status_icon_set_from_file (trayIcon, TRAY_DEMUTE);
	if (volstate == 0 || pcmstate == 0)
	    gtk_status_icon_set_from_file (trayIcon, TRAY_VOLMUTE);
}

static void
trayView(GtkMenuItem *item, gpointer window)
{
	gtk_widget_show_all (window);
	gtk_window_deiconify(GTK_WINDOW(window));
	is_tray=TRUE;
}

static void
trayExit(GtkMenuItem *item, gpointer user_data)
{
	gtk_main_quit();
}

static void trayIconPopup(GtkStatusIcon *status_icon, guint button, guint32 activate_time, gpointer popUpMenu)
{
	gtk_menu_popup(GTK_MENU(popUpMenu), NULL, NULL, gtk_status_icon_position_menu, status_icon, button, activate_time);
}

void on_focus_out (GtkWidget* window)
{
#ifdef DEBUG
	printf("Out\n");
#endif
	gtk_widget_hide (window);
	gtk_window_iconify(GTK_WINDOW(window));
	is_tray=FALSE;
}

void checkphone_toogle_signal(GtkWidget *widget, gpointer window)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkphone)))
	{
#ifdef DEBUG
		g_print("Mic Enable!\n");
#endif
		sndunitnw=1;
		if (sysctlbyname("hw.snd.default_unit", &sndunit, &len, &sndunitnw, sizeof(sndunitnw)) < 0)
			perror("Sysctl hw.snd.default_unit");
	}
	else
	{
#ifdef DEBUG
		g_print("Mic Disable!\n");
#endif
		sndunitnw=0;
		if (sysctlbyname("hw.snd.default_unit", &sndunit, &len, &sndunitnw, sizeof(&sndunitnw)) < 0)
			perror("Sysctl hw.snd.default_unit");
	}
}

/*void on_popup_clicked (GtkButton* button, GtkWidget* pWindow)
{
	GtkWidget *popup_window;
	popup_window = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_window_set_title (GTK_WINDOW (popup_window), "Pop Up window");
	gtk_container_set_border_width (GTK_CONTAINER (popup_window), 10);
	gtk_window_set_resizable(GTK_WINDOW (popup_window), FALSE);
	gtk_widget_set_size_request (popup_window, 150, 150);
	gtk_window_set_transient_for(GTK_WINDOW (popup_window),GTK_WINDOW (pWindow));
	gtk_window_set_position (GTK_WINDOW (popup_window),GTK_WIN_POS_CENTER);
	g_signal_connect (G_OBJECT (button), "event",
			G_CALLBACK (on_popup_window_event),NULL);

	GdkColor color;
	gdk_color_parse("#3b3131", &color);
	gtk_widget_modify_bg(GTK_WIDGET(popup_window), GTK_STATE_NORMAL, &color);


	gtk_widget_show_all (popup_window);
}

gboolean on_popup_window_event(GtkWidget *popup_window, GdkEventExpose *event)
{
	if(event->type == GDK_FOCUS_CHANGE)
		gtk_widget_hide (popup_window);

	return FALSE;
}*/

static void trayIconActivated(GObject *trayicon,  gpointer window) 
{
	gtk_widget_show_all (window);
	if (is_tray==TRUE)
	{
	gtk_widget_hide (window);
	    gtk_window_iconify(GTK_WINDOW(window));
	    is_tray=FALSE;
#ifdef DEBUG
	    printf("Is tray = TRUE\n");
#endif
	}
	else
	{
	    gtk_window_deiconify(GTK_WINDOW(window));
	    is_tray=TRUE;
#ifdef DEBUG
	    printf("Is tray = FALSE\n");
#endif
	}
	
	if (sysctlbyname("hw.snd.default_unit", &sndunit, &len, NULL, 0) < 0)
		printf("%d\n", errno);
	if (sndunit==1)
	{
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkphone), 1);
	}
	else
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkphone), 0);

#ifdef DEBUG
	printf("Unit: %d\n", sndunit);
	printf("Activated!\n");
#endif
	g_signal_connect (window, "focus-out-event", G_CALLBACK (on_focus_out), NULL);
}

void
gui_loop()
{
	GParamSpec		*pspec;

	GtkWidget *             window;
	GtkWidget *             main_vbox;
	GtkWidget *             top_graphic;
	GtkWidget *             table_scrolled_window;
	GtkWidget *             bottom_buttons;
	GtkWidget *             table;
	GtkWidget *             w;
	GtkWidget *             image;
	GtkWidget *		frame;
	GtkWidget *		frame2;
	GtkWidget *		frame3;
	GtkWidget *		frame4;
	GtkWidget *		frame5;
	GtkWidget *		VolImg;
	GtkWidget *		PcmImg;
	GtkWidget *		PhoneImg;

	gint			xOrigin,yOrigin;
	const gchar *labels[4] = { 
		"Head Phones enable", 
		"<span size='small'>Baby bath</span>", 
		"<span color='Blue' size='small'>Hight</span>", 
		"<span color='Red' size='small'>Mute</span>" 
	};
	gdouble marks[3] = { 0, 50, 100 };

	window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
	g_signal_connect (window, "focus-out-event", G_CALLBACK (on_focus_out), NULL);
	gtk_container_set_border_width (GTK_CONTAINER (window), 10);
	gtk_window_set_resizable(GTK_WINDOW (window), FALSE);
	gtk_window_set_decorated(GTK_WINDOW (window), FALSE);
	gtk_widget_set_size_request (window, 280, 200);
	gtk_window_set_transient_for(GTK_WINDOW (window), NULL);
	gtk_window_set_position (GTK_WINDOW (window),GTK_WIN_POS_MOUSE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW (window),TRUE);
	gtk_window_stick(GTK_WINDOW (window));

	table = gtk_table_new(3, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_container_add(GTK_CONTAINER(window), table);
	


	frame = gtk_frame_new("Volume");
	g_signal_connect (frame, "focus-out-event", G_CALLBACK (on_focus_out), NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);

	frame2 = gtk_frame_new("Master");
	gtk_frame_set_shadow_type(GTK_FRAME(frame2), GTK_SHADOW_ETCHED_IN);

	checkphone = gtk_check_button_new_with_label(labels[0]);


	VolImg = gtk_image_new_from_file(VOLSET);
	PcmImg = gtk_image_new_from_file(PCMSET);
	PhoneImg = gtk_image_new_from_file(PHONESET);
	gtk_image_set_pixel_size(GTK_IMAGE(VolImg), 10);
	gtk_image_set_pixel_size(GTK_IMAGE(PcmImg), 10);
	gtk_image_set_pixel_size(GTK_IMAGE(PhoneImg), 10);

	gtk_table_attach_defaults(GTK_TABLE(table), VolImg, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(table), PcmImg, 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(table), PhoneImg, 0, 1, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(table), frame,1, 2, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(table), frame2, 1, 2, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(table), checkphone, 1, 2, 2, 3);
	/*gtk_table_attach(GTK_TABLE(table), frame3, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach(GTK_TABLE(table), frame4, 0, 1, 1, 2, GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
	gtk_table_attach(GTK_TABLE(table), frame5, 0, 1, 2, 3, GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
	gtk_table_attach(GTK_TABLE(table), frame, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
	gtk_table_attach(GTK_TABLE(table), frame2, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
	gtk_table_attach(GTK_TABLE(table), checkphone, 1, 2, 2, 3, GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);*/

	hscaleVol = gtk_hscale_new_with_range(0, 100, 1); 
	gtk_scale_set_digits(GTK_SCALE(hscaleVol), 0);
	gtk_scale_add_mark (GTK_SCALE (hscaleVol), marks[0], GTK_POS_BOTTOM, labels[3]);
	gtk_scale_add_mark (GTK_SCALE (hscaleVol), marks[2], GTK_POS_BOTTOM, labels[2]);
	gtk_scale_set_value_pos(GTK_SCALE(hscaleVol), GTK_POS_LEFT);
	gtk_range_set_update_policy (GTK_RANGE (hscaleVol),
			                                 GTK_UPDATE_CONTINUOUS);

	hscalePcm = gtk_hscale_new_with_range(0, 100, 1); 
	gtk_scale_set_digits(GTK_SCALE(hscalePcm), 0);
	gtk_scale_add_mark (GTK_SCALE (hscalePcm), marks[0], GTK_POS_BOTTOM, labels[3]);
	gtk_scale_add_mark (GTK_SCALE (hscalePcm), marks[2], GTK_POS_BOTTOM, labels[2]);
	gtk_range_set_update_policy (GTK_RANGE (hscalePcm),
			                                 GTK_UPDATE_CONTINUOUS);


	gtk_scale_set_value_pos (GTK_SCALE(hscaleVol), GTK_POS_LEFT);
	gtk_scale_set_value_pos (GTK_SCALE(hscalePcm), GTK_POS_LEFT);

	gtk_container_add(GTK_CONTAINER(frame), hscaleVol);
	gtk_container_add(GTK_CONTAINER(frame2), hscalePcm);

	GtkStatusIcon *trayIcon = gtk_status_icon_new();
	gtk_status_icon_set_from_file (trayIcon, TRAY_VOLMUTE);

	menu = gtk_menu_new();

	menuItemView = gtk_menu_item_new_with_label ("View");
	Separator1 = gtk_separator_menu_item_new();
	menuItemExit = gtk_menu_item_new_with_label ("Exit");
	g_signal_connect (G_OBJECT (menuItemView), "activate", G_CALLBACK (trayView), window);
	g_signal_connect (G_OBJECT (menuItemExit), "activate", G_CALLBACK (trayExit), NULL);

	g_signal_connect(GTK_STATUS_ICON (trayIcon), "activate", GTK_SIGNAL_FUNC (trayIconActivated), window);
	g_signal_connect(GTK_STATUS_ICON (trayIcon), "popup-menu", GTK_SIGNAL_FUNC (trayIconPopup), menu);

	g_signal_connect(G_OBJECT(checkphone), "clicked",
			        G_CALLBACK(checkphone_toogle_signal), (gpointer) checkphone);
	g_signal_connect (G_OBJECT (hscalePcm), "value_changed",
			                      G_CALLBACK (cb_digits_scale_pcm), NULL);
	g_signal_connect (G_OBJECT (hscaleVol), "value_changed",
			                      G_CALLBACK (cb_digits_scale_vol), NULL);


	g_signal_connect (table, "focus-out-event", G_CALLBACK (on_focus_out), NULL);
	
	if (sysctlbyname("hw.snd.default_unit", &sndunit, &len, NULL, 0) < 0)
		printf("%d\n", errno);
	if (sndunit==1)
	{
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkphone), 1);
	}
	else
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkphone), 0);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuItemView);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), Separator1);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuItemExit);
	gtk_widget_show_all (menu);

	/*timer*/
	    g_timeout_add_seconds(1,TimerFunc,trayIcon);
	//gtk_widget_show_all(window);


/* hide main window */
	//gtk_window_iconify (GTK_WINDOW(window));
	//is_tray=TRUE;


	gtk_main();

}

static void
usage(int devmask, int recmask)
{
	int	i, n;

	printf("usage: mixer [-f device] [-s | -S] [dev [+|-][voll[:[+|-]volr]] ...\n"
	    "       mixer [-f device] [-s | -S] recsrc ...\n"
	    "       mixer [-f device] [-s | -S] {^|+|-|=}rec rdev ...\n");
	if (devmask != 0) {
		printf(" devices: ");
		for (i = 0, n = 0; i < SOUND_MIXER_NRDEVICES; i++)
			if ((1 << i) & devmask)  {
				if (n)
					printf(", ");
				printf("%s", names[i]);
				n++;
			}
	}
	if (recmask != 0) {
		printf("\n rec devices: ");
		for (i = 0, n = 0; i < SOUND_MIXER_NRDEVICES; i++)
			if ((1 << i) & recmask)  {
				if (n)
					printf(", ");
				printf("%s", names[i]);
				n++;
			}
	}
	printf("\n");
	exit(1);
}

static int
res_name(const char *name, int mask)
{
	int	foo;

	for (foo = 0; foo < SOUND_MIXER_NRDEVICES; foo++)
		if ((1 << foo) & mask && strcmp(names[foo], name) == 0)
			break;

	return (foo == SOUND_MIXER_NRDEVICES ? -1 : foo);
}

static void
print_recsrc(int recsrc, int recmask, int sflag)
{
	int	i, n;

	if (recmask == 0)
		return;

	if (!sflag)
		printf("Recording source: ");

	for (i = 0, n = 0; i < SOUND_MIXER_NRDEVICES; i++)
		if ((1 << i) & recsrc) {
			if (sflag)
				printf("%srec ", n ? " +" : "=");
			else if (n)
				printf(", ");
			printf("%s", names[i]);
			n++;
		}
	if (!sflag)
		printf("\n");
}

int
get_mixer_state(char * mixprm)
{
	char	mixer[PATH_MAX] = "/dev/mixer";
	char	lstr[5], rstr[5];
	char	*name, *eptr;
	int	devmask = 0, recmask = 0, recsrc = 0, orecsrc;
	int	dusage = 0, drecsrc = 0, sflag = 0, Sflag = 0;
	int	l, r, lrel, rrel;
	int	ch, foo, bar, baz, dev, m, n, t;

	if ((name = strdup(basename(mixer))) == NULL)
		err(1, "strdup()");
	if (strncmp(name, "mixer", 5) == 0 && name[5] != '\0') {
		n = strtol(name + 5, &eptr, 10) - 1;
		if (n > 0 && *eptr == '\0')
			snprintf(mixer, PATH_MAX - 1, "/dev/mixer%d", n);
	}

	free(name);
	name = mixer;

	n = 1;

	if (sflag && Sflag)
		dusage = 1;

	if ((baz = open(name, O_RDWR)) < 0)
		err(1, "%s", name);
	if (ioctl(baz, SOUND_MIXER_READ_DEVMASK, &devmask) == -1)
		err(1, "SOUND_MIXER_READ_DEVMASK");
	if (ioctl(baz, SOUND_MIXER_READ_RECMASK, &recmask) == -1)
		err(1, "SOUND_MIXER_READ_RECMASK");
	if (ioctl(baz, SOUND_MIXER_READ_RECSRC, &recsrc) == -1)
		err(1, "SOUND_MIXER_READ_RECSRC");
	orecsrc = recsrc;


	if (dusage == 0) {
		for (foo = 0, n = 0; foo < SOUND_MIXER_NRDEVICES; foo++) {
			if (!((1 << foo) & devmask))
				continue;
			if (ioctl(baz, MIXER_READ(foo),&bar) == -1) {
			   	warn("MIXER_READ");
				continue;
			}
			if (Sflag || sflag) {
				printf("%s%s%c%d:%d", n ? " " : "",
				    names[foo], Sflag ? ':' : ' ',
				    bar & 0x7f, (bar >> 8) & 0x7f);
				n++;
			} else
			{
				if (strcmp(names[foo], mixprm)==0)
				{
					close(baz);
					return bar & 0x7f;
				}
			}
		}
		close(baz);
		return (0);
	}
	close(baz);
	return (0);
}

int
set_mixer_state(char * mixprm, int st)
{
	char	mixer[PATH_MAX] = "/dev/mixer";
	char	lstr[5], rstr[5];
	char	*name, *eptr;
	int	devmask = 0, recmask = 0, recsrc = 0, orecsrc;
	int	dusage = 0, drecsrc = 0, sflag = 0, Sflag = 0;
	int	l, r, lrel, rrel;
	int	ch, foo, bar, baz, dev, m, n, t;

	if ((name = strdup("mixer")) == NULL)
		err(1, "strdup()");
	if (strncmp(name, "mixer", 5) == 0 && name[5] != '\0') {
		n = strtol(name + 5, &eptr, 10) - 1;
		if (n > 0 && *eptr == '\0')
			snprintf(mixer, PATH_MAX - 1, "/dev/mixer%d", n);
	}
	free(name);
	name = mixer;

	n = 1;

	if (sflag && Sflag)
		dusage = 1;

	if ((baz = open(name, O_RDWR)) < 0)
		err(1, "%s", name);
	if (ioctl(baz, SOUND_MIXER_READ_DEVMASK, &devmask) == -1)
		err(1, "SOUND_MIXER_READ_DEVMASK");
	if (ioctl(baz, SOUND_MIXER_READ_RECMASK, &recmask) == -1)
		err(1, "SOUND_MIXER_READ_RECMASK");
	if (ioctl(baz, SOUND_MIXER_READ_RECSRC, &recsrc) == -1)
		err(1, "SOUND_MIXER_READ_RECSRC");
	orecsrc = recsrc;

	n = 0;

	if ((t = sscanf(mixprm, "%d:%d", &l, &r)) > 0)
		dev = 0;
	else if ((dev = res_name(mixprm, devmask)) == -1) {
		warnx("unknown device: %s", mixprm);
		dusage = 1;
	}

	lrel = rrel = 0;

	m = sscanf(mixprm, "%7[^:]:%7s", lstr, rstr);
	if (m > 0) {
		if (*lstr == '+' || *lstr == '-')
			    lrel = rrel = 1;
			l = strtol(lstr, NULL, 10);
	}
	if (m > 1) {
		if (*rstr == '+' || *rstr == '-')
			    rrel = 1;
			r = strtol(rstr, NULL, 10);
	}

			if (ioctl(baz, MIXER_READ(dev), &bar) == -1) {
				warn("MIXER_READ");
			}

			if (lrel)
				l = (bar & 0x7f) + l;
			if (rrel)
				r = ((bar >> 8) & 0x7f) + r;

			if (l < 0)
				l = 0;
			else if (l > 100)
				l = 100;
			if (r < 0)
				r = 0;
			else if (r > 100)
				r = 100;
			if (!Sflag)
#ifdef DEBUG	
				printf("Setting the mixer %s from %d:%d to "
				    "%d:%d.\n", names[dev], bar & 0x7f,
				    (bar >> 8) & 0x7f, l, r);
#endif
			l=r=st;
			l |= r << 8;
			if (ioctl(baz, MIXER_WRITE(dev), &l) == -1)
				warn("WRITE_MIXER");


	if (orecsrc != recsrc) {
		if (ioctl(baz, SOUND_MIXER_WRITE_RECSRC, &recsrc) == -1)
			err(1, "SOUND_MIXER_WRITE_RECSRC");
		if (ioctl(baz, SOUND_MIXER_READ_RECSRC, &recsrc) == -1)
			err(1, "SOUND_MIXER_READ_RECSRC");
	}

	if (drecsrc)
		print_recsrc(recsrc, recmask, Sflag || sflag);

	close(baz);

	return (0);
}

int
main(int argc, char *argv[])
{
	char	mixer[PATH_MAX] = "/dev/mixer";
	char	lstr[5], rstr[5];
	char	*name, *eptr;
	int	devmask = 0, recmask = 0, recsrc = 0, orecsrc;
	int	dusage = 0, drecsrc = 0, sflag = 0, Sflag = 0;
	int	l, r, lrel, rrel;
	int	ch, foo, bar, baz, dev, m, n, t;

	if ((name = strdup(basename(argv[0]))) == NULL)
		err(1, "strdup()");
	if (strncmp(name, "mixer", 5) == 0 && name[5] != '\0') {
		n = strtol(name + 5, &eptr, 10) - 1;
		if (n > 0 && *eptr == '\0')
			snprintf(mixer, PATH_MAX - 1, "/dev/mixer%d", n);
	}
	free(name);
	name = mixer;

	if (!gui_init(&argc, &argv)) {
		fprintf(stderr, gettext("wifimgr: cannot open display\n"));
		exit(1);
	}

	gui_loop();

	n = 1;
	for (;;) {
		if (n >= argc || *argv[n] != '-')
			break;
		if (strlen(argv[n]) != 2) {
			if (strcmp(argv[n] + 1, "rec") != 0)
				dusage = 1;
			break;
		}
		ch = *(argv[n] + 1);
		if (ch == 'f' && n < argc - 1) {
			name = argv[n + 1];
			n += 2;
		} else if (ch == 's') {
			sflag = 1;
			n++;
		} else if (ch == 'S') {
			Sflag = 1;
			n++;
		} else {
			dusage = 1;
			break;
		}
	}
	if (sflag && Sflag)
		dusage = 1;

	argc -= n - 1;
	argv += n - 1;

	if ((baz = open(name, O_RDWR)) < 0)
		err(1, "%s", name);
	if (ioctl(baz, SOUND_MIXER_READ_DEVMASK, &devmask) == -1)
		err(1, "SOUND_MIXER_READ_DEVMASK");
	if (ioctl(baz, SOUND_MIXER_READ_RECMASK, &recmask) == -1)
		err(1, "SOUND_MIXER_READ_RECMASK");
	if (ioctl(baz, SOUND_MIXER_READ_RECSRC, &recsrc) == -1)
		err(1, "SOUND_MIXER_READ_RECSRC");
	orecsrc = recsrc;

	if (argc == 1 && dusage == 0) {
		for (foo = 0, n = 0; foo < SOUND_MIXER_NRDEVICES; foo++) {
			if (!((1 << foo) & devmask))
				continue;
			if (ioctl(baz, MIXER_READ(foo),&bar) == -1) {
			   	warn("MIXER_READ");
				continue;
			}
			if (Sflag || sflag) {
				printf("%s%s%c%d:%d", n ? " " : "",
				    names[foo], Sflag ? ':' : ' ',
				    bar & 0x7f, (bar >> 8) & 0x7f);
				n++;
			} else
				printf("Mixer %-8s is currently set to "
				    "%3d:%d\n", names[foo], bar & 0x7f,
				    (bar >> 8) & 0x7f);
		}
		if (n && recmask)
			printf(" ");
		print_recsrc(recsrc, recmask, Sflag || sflag);
		return (0);
	}

	argc--;
	argv++;

	n = 0;
	while (argc > 0 && dusage == 0) {
		if (strcmp("recsrc", *argv) == 0) {
			drecsrc = 1;
			argc--;
			argv++;
			continue;
		} else if (strcmp("rec", *argv + 1) == 0) {
			if (**argv != '+' && **argv != '-' &&
			    **argv != '=' && **argv != '^') {
				warnx("unknown modifier: %c", **argv);
				dusage = 1;
				break;
			}
			if (argc <= 1) {
				warnx("no recording device specified");
				dusage = 1;
				break;
			}
			if ((dev = res_name(argv[1], recmask)) == -1) {
				warnx("unknown recording device: %s", argv[1]);
				dusage = 1;
				break;
			}
			switch (**argv) {
			case '+':
				recsrc |= (1 << dev);
				break;
			case '-':
				recsrc &= ~(1 << dev);
				break;
			case '=':
				recsrc = (1 << dev);
				break;
			case '^':
				recsrc ^= (1 << dev);
				break;
			}
			drecsrc = 1;
			argc -= 2;
			argv += 2;
			continue;
		}

		if ((t = sscanf(*argv, "%d:%d", &l, &r)) > 0)
			dev = 0;
		else if ((dev = res_name(*argv, devmask)) == -1) {
			warnx("unknown device: %s", *argv);
			dusage = 1;
			break;
		}

		lrel = rrel = 0;
		if (argc > 1) {
			m = sscanf(argv[1], "%7[^:]:%7s", lstr, rstr);
			if (m > 0) {
				if (*lstr == '+' || *lstr == '-')
					lrel = rrel = 1;
				l = strtol(lstr, NULL, 10);
			}
			if (m > 1) {
				if (*rstr == '+' || *rstr == '-')
					rrel = 1;
				r = strtol(rstr, NULL, 10);
			}
		}

		switch (argc > 1 ? m : t) {
		case 0:
			if (ioctl(baz, MIXER_READ(dev), &bar) == -1) {
				warn("MIXER_READ");
				argc--;
				argv++;
				continue;
			}
			if (Sflag || sflag) {
				printf("%s%s%c%d:%d", n ? " " : "",
				    names[dev], Sflag ? ':' : ' ',
				    bar & 0x7f, (bar >> 8) & 0x7f);
				n++;
			} else
				printf("Mixer %-8s is currently set to "
				    "%3d:%d\n", names[dev], bar & 0x7f,
				    (bar >> 8) & 0x7f);

			argc--;
			argv++;
			break;
		case 1:
			r = l;
			/* FALLTHROUGH */
		case 2:
			if (ioctl(baz, MIXER_READ(dev), &bar) == -1) {
				warn("MIXER_READ");
				argc--;
				argv++;
				continue;
			}

			if (lrel)
				l = (bar & 0x7f) + l;
			if (rrel)
				r = ((bar >> 8) & 0x7f) + r;

			if (l < 0)
				l = 0;
			else if (l > 100)
				l = 100;
			if (r < 0)
				r = 0;
			else if (r > 100)
				r = 100;

			if (!Sflag)
				printf("Setting the mixer %s from %d:%d to "
				    "%d:%d.\n", names[dev], bar & 0x7f,
				    (bar >> 8) & 0x7f, l, r);

			l |= r << 8;
			if (ioctl(baz, MIXER_WRITE(dev), &l) == -1)
				warn("WRITE_MIXER");

			argc -= 2;
			argv += 2;
 			break;
		}
	}

	if (orecsrc != recsrc) {
		if (ioctl(baz, SOUND_MIXER_WRITE_RECSRC, &recsrc) == -1)
			err(1, "SOUND_MIXER_WRITE_RECSRC");
		if (ioctl(baz, SOUND_MIXER_READ_RECSRC, &recsrc) == -1)
			err(1, "SOUND_MIXER_READ_RECSRC");
	}

	if (drecsrc)
		print_recsrc(recsrc, recmask, Sflag || sflag);

	close(baz);

	return (0);
}
