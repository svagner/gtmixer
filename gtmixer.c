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
#include <stdlib.h>
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

#include "ini.h"
#include "gtmixer.h"

static pthread_mutex_t mixer_mutex;

const char	*names[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_NAMES;

static void	usage(int devmask, int recmask);
static int	res_name(const char *name, int mask);
static void	print_recsrc(int recsrc, int recmask, int sflag);

pthread_t tid[2];

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

gboolean
*TimerFunc (gpointer trayIcon)
//*TimerFunc (GtkStatusIcon * trayIcon)
{
	
//	pthread_mutex_lock(&mixer_mutex);
	volstate=get_mixer_state("vol");
	pcmstate=get_mixer_state("pcm");
//	pthread_mutex_unlock(&mixer_mutex);
	gtk_range_set_value(GTK_RANGE (hscaleVol), volstate);
	gtk_range_set_value(GTK_RANGE (hscalePcm), pcmstate);
	if (volstate > 50 && pcmstate > 50)
	    gtk_status_icon_set_from_file (trayIcon, TRAY_INMUTE);
	if ((volstate < 50 && volstate > 0) && (pcmstate < 50 && pcmstate > 0))
	    gtk_status_icon_set_from_file (trayIcon, TRAY_DEMUTE);
	if (volstate == 0 || pcmstate == 0)
	{
	    gtk_status_icon_set_from_file (trayIcon, TRAY_VOLMUTE);
	}
	return 1;
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
		if (sysctlbyname("hw.snd.default_unit", &sndunit, &len, &fconfig.punit, sizeof(sndunitnw)) < 0)
			perror("Sysctl hw.snd.default_unit");
	}
	else
	{
#ifdef DEBUG
		g_print("Mic Disable!\n");
#endif
		sndunitnw=0;
		if (sysctlbyname("hw.snd.default_unit", &sndunit, &len, &fconfig.ounit, sizeof(&sndunitnw)) < 0)
			perror("Sysctl hw.snd.default_unit");
	}
}

static
int save_config(gpointer settings_window)
{
	FILE *conf;
	int status;
	conf = fopen(CONFIGFILE, "wb");

	bzero(fconfig.device, sizeof(fconfig.device));
	strcpy(fconfig.device, gtk_entry_get_text(GTK_ENTRY(devEntry)));
	fconfig.ounit = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(outEntry));
	fconfig.punit = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(phoneEntry));
	fprintf(conf,"[general]\ndevice = %s\n\n[mixer]\nphonehead = %d\nout = %d\n", fconfig.device, fconfig.punit, fconfig.ounit);

	status = fclose(conf);
	return status;
}

static 
int handler(void* user, const char* section, const char* name,
		const char* value)
{
	configuration* pconfig = (configuration*)user;

#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
	if (MATCH("glogal", "version")) {
		pconfig->version = strdup(value);
	} else if (MATCH("global", "device")) {
		pconfig->device = strdup(value);
	} else if (MATCH("mixer", "phonehead")) {
		pconfig->phone_unit = atoi(value);
	} else if (MATCH("mixer", "out")) {
		pconfig->out_unit = atoi(value);
	} else {
		return 0;  /* unknown section/name, error */
	}

	return 1;
}

static
void SettingsActivated (GObject *trayicon, gpointer window)
{
	GtkWidget *		settings_window;
	GtkWidget *             settings_table;
	GtkWidget *             devLabel;
	GtkWidget *             phoneLabel;
	GtkWidget *             outLabel;
	GtkWidget *             CloseButton;
	GtkWidget *             SaveButton;
	GtkWidget *             SaveFixed;
	GtkWidget *             CloseFixed;

#ifdef DEBUG
	printf("[general]\ndevice = %s\n\n[mixer]\nphonehead = %d\nout = %d\n\n", fconfig.device, fconfig.punit, fconfig.ounit);
#endif

	settings_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (settings_window), "GTMixer - Settings");
	gtk_container_set_border_width (GTK_CONTAINER (settings_window), 10);
	gtk_window_set_resizable(GTK_WINDOW (settings_window), FALSE);
	gtk_widget_set_size_request (settings_window, 325, 175);
	gtk_window_set_position (GTK_WINDOW (settings_window),GTK_WIN_POS_CENTER);

	GdkColor color;
	gdk_color_parse("#3b3131", &color);
	gtk_widget_modify_bg(GTK_WIDGET(settings_window), GTK_STATE_NORMAL, &color);

	settings_table = gtk_table_new(6, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(settings_table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(settings_table), 5);
	gtk_container_add(GTK_CONTAINER(settings_window), settings_table);


	devLabel = gtk_label_new("Mixer device: ");
	gtk_table_attach_defaults(GTK_TABLE(settings_table), devLabel, 0, 1, 0, 1);

	devEntry = gtk_entry_new_with_max_length(15);
	gtk_entry_set_text(GTK_ENTRY(devEntry), fconfig.device);
	gtk_table_attach_defaults(GTK_TABLE(settings_table), devEntry, 1, 2, 0, 1);

	phoneLabel = gtk_label_new("Head Phone Unit: ");
	gtk_table_attach_defaults(GTK_TABLE(settings_table), phoneLabel, 0, 1, 1, 2);

	phoneEntry = gtk_spin_button_new_with_range(0, 10, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(phoneEntry), fconfig.punit);
	gtk_table_attach_defaults(GTK_TABLE(settings_table), phoneEntry, 1, 2, 1, 2);

	outLabel = gtk_label_new("Sound out Unit: ");
	gtk_table_attach_defaults(GTK_TABLE(settings_table), outLabel, 0, 1, 2, 3);

	outEntry = gtk_spin_button_new_with_range(0, 10, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(outEntry), fconfig.ounit);
	gtk_table_attach_defaults(GTK_TABLE(settings_table), outEntry, 1, 2, 2, 3);

	SaveFixed = gtk_fixed_new();
	gtk_table_attach_defaults(GTK_TABLE(settings_table), SaveFixed, 0, 1, 3, 4);
	SaveButton = gtk_button_new_with_label("Save");
	gtk_button_set_alignment(GTK_BUTTON(SaveButton), 50, 5);
	gtk_widget_set_size_request(SaveButton, 80, 35);
	gtk_button_set_relief(GTK_BUTTON(SaveButton), GTK_RELIEF_HALF);
	gtk_fixed_put(GTK_FIXED(SaveFixed), SaveButton, 50, 25);

	CloseFixed = gtk_fixed_new();
	gtk_table_attach_defaults(GTK_TABLE(settings_table), CloseFixed, 1, 2, 3, 4);
	CloseButton = gtk_button_new_with_label("Close");
	gtk_button_set_alignment(GTK_BUTTON(CloseButton), 50, 5);
	gtk_widget_set_size_request(CloseButton, 80, 35);
	gtk_button_set_relief(GTK_BUTTON(CloseButton), GTK_RELIEF_HALF);
	gtk_fixed_put(GTK_FIXED(CloseFixed), CloseButton, 50, 25);

	// start event
	g_signal_connect(G_OBJECT(SaveButton), "clicked", G_CALLBACK (save_config), (gpointer) settings_window);
	g_signal_connect_swapped(G_OBJECT(CloseButton), "clicked", G_CALLBACK (gtk_widget_destroy), (gpointer) settings_window);

	gtk_widget_show_all (settings_window);
}

static 
void trayIconActivated(GObject *trayicon,  gpointer window) 
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

	trayIcon = gtk_status_icon_new();
	gtk_status_icon_set_from_file (trayIcon, TRAY_VOLMUTE);

	menu = gtk_menu_new();

	menuItemView = gtk_menu_item_new_with_label ("View");
	menuItemSet = gtk_menu_item_new_with_label ("Settings");
	Separator1 = gtk_separator_menu_item_new();
	menuItemExit = gtk_menu_item_new_with_label ("Exit");
	g_signal_connect (G_OBJECT (menuItemView), "activate", G_CALLBACK (trayView), window);
	g_signal_connect (G_OBJECT (menuItemSet), "activate", GTK_SIGNAL_FUNC (SettingsActivated), window);
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
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuItemSet);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), Separator1);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuItemExit);
	gtk_widget_show_all (menu);

	/*timer*/
	g_timeout_add_seconds(1,(GSourceFunc)TimerFunc,trayIcon);

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
	//char	mixer[PATH_MAX] = "/dev/mixer";
	char	mixer[PATH_MAX];
	char	lstr[5], rstr[5];
	char	*name, *eptr;
	int	devmask = 0, recmask = 0, recsrc = 0, orecsrc;
	int	dusage = 0, drecsrc = 0, sflag = 0, Sflag = 0;
	int	l, r, lrel, rrel;
	int	ch, foo, bar, baz, dev, m, n, t;

	strcpy(mixer, fconfig.device);

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
//	char	mixer[PATH_MAX] = "/dev/mixer";
	char	mixer[PATH_MAX];
	char	lstr[5], rstr[5];
	char	*name, *eptr;
	int	devmask = 0, recmask = 0, recsrc = 0, orecsrc;
	int	dusage = 0, drecsrc = 0, sflag = 0, Sflag = 0;
	int	l, r, lrel, rrel;
	int	ch, foo, bar, baz, dev, m, n, t;

	strcpy(mixer, fconfig.device);

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
main(int argc, char *argv[], char *envp[])
{
//	char	mixer[PATH_MAX] = "/dev/mixer";
	char	mixer[PATH_MAX];
	char	lstr[5], rstr[5];
	char	*name, *eptr;
	int	devmask = 0, recmask = 0, recsrc = 0, orecsrc;
	int	dusage = 0, drecsrc = 0, sflag = 0, Sflag = 0;
	int	l, r, lrel, rrel;
	int	ch, foo, bar, baz, dev, m, n, t;
	configuration config;
	configuration * pconfig = &config;
	FILE *conf;

	config.device = device;
	config.phone_unit=0;
	config.out_unit=0;

	if (ini_parse(CONFIGFILE, handler, &config) < 0) {
		conf = fopen(CONFIGFILE, "wb");
		fclose(conf);
	}

	// check variables
	if (strlen(pconfig->device)==0)
	{
		strcpy(device, DEFAULTDEV);
	}
	// init global variables
	strcpy(fconfig.device, config.device);
	fconfig.punit = config.phone_unit;
	fconfig.ounit = config.out_unit;

	strcpy(mixer, fconfig.device);

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
