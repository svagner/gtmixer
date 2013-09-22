/*
 *	This is mixer program for FreeBSD
 *
 * (C) Stanislav Putrya.
 *
 * You may do anything you wish with this program.
 *
 */

#include <cairo/cairo.h>
#include <pango/pango.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/sysctl.h>

#include <sys/param.h>
#include <sys/lock.h>

#include "gtmixer.h"
#include "config.h"

static gint	is_tray=FALSE;
static size_t   len = sizeof(sndunit);
static int	mixer_desc=0, onlyget_sysctl=FALSE;
static int      vol_ischanged=FALSE, pcm_ischanged=FALSE;
extern struct	mixerhash *mixerunits;

void
print_version(char *myname)
{
	fprintf(stderr, "[INFO] [%s:%d] %s(): %s-%s tag:%s\n", __FILE__, __LINE__, __func__, myname, VER, GIT_VERSION);
	exit(0);
}

int
gui_init(int * ac, char *** av) {
    return gtk_init_check(ac, av);
}

static void destroy (GtkWidget *window, gpointer data)
{
	gtk_main_quit ();
}

void 
set_app_font (const char *fontname)
{
	GtkSettings *settings;

	settings = gtk_settings_get_default();
	g_object_set(G_OBJECT(settings), "gtk-font-name", fontname,
			NULL);
}

void
cb_digits_scale_pcm(GtkWidget *widget, gpointer window)
{
	pcmstate=gtk_range_get_value(GTK_RANGE (hscalePcm));
	DPRINT("pcm state now: %d\n", pcmstate);
	set_mixer_state("pcm", pcmstate);
	pcm_ischanged=TRUE;
}

void
cb_digits_scale_global(GtkWidget *widget, gpointer unitname)
{
	int state, unit;
	unit = get_mixer_unit_by_name((char *)unitname);
	DPRINT("name - %s; unit - %d\n", (char *)unitname, unit);
	state=gtk_range_get_value(GTK_RANGE (mixer_hscale[unit]));
	DPRINT("new %s state is %d\n", (char *)unitname, state);
	set_mixer_state(unitname, state);
}

void
cb_digits_scale_vol(GtkWidget *widget, gpointer window)
{
	volstate=gtk_range_get_value(GTK_RANGE (hscaleVol));
	DPRINT("new PCM state is %d\n", volstate);
	set_mixer_state("vol", volstate);
	vol_ischanged=TRUE;
}

gboolean
*TimerFunc (gpointer trayIcon)
{
	int old_volstate, old_pcmstate;

	old_volstate = volstate;
	old_pcmstate = pcmstate;
	volstate=get_mixer_state("vol");
	pcmstate=get_mixer_state("pcm");
	if (GTK_WIDGET(mixer_window))
	    	get_mixer_state_all();

	if ((volstate != old_volstate || pcmstate != old_pcmstate) || (vol_ischanged==TRUE || pcm_ischanged==TRUE))
	{
		gtk_range_set_value(GTK_RANGE (hscaleVol), volstate);
		gtk_range_set_value(GTK_RANGE (hscalePcm), pcmstate);
		if (volstate > 50 && pcmstate > 50)
			gtk_status_icon_set_from_file (trayIcon, TRAY_INMUTE);
		if ((volstate > 0 && volstate <= 50) || (pcmstate > 0 && pcmstate <= 50))
			gtk_status_icon_set_from_file (trayIcon, TRAY_DEMUTE);
		if (volstate == 0 || pcmstate == 0)
		{
			gtk_status_icon_set_from_file (trayIcon, TRAY_VOLMUTE);
		}
		vol_ischanged=FALSE;
		pcm_ischanged=FALSE;
	}
	return (gboolean *)1;
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
	DPRINT("Focus out = %d\n", 1);
	gtk_widget_hide (window);
	gtk_window_iconify(GTK_WINDOW(window));
	is_tray=FALSE;
}

void checkphone_toogle_signal(GtkWidget *widget, gpointer checkphone_toogle)
{
	if(onlyget_sysctl)
		return;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkphone)))
	{
		sndunitnw=1;
		DPRINT("Phonehead enable = %d\n", sndunitnw);
		if (sysctlbyname("hw.snd.default_unit", &sndunit, &len, &fconfig.punit, sizeof(sndunitnw)) < 0) {
			perror("Sysctl hw.snd.default_unit");
			dialog = gtk_message_dialog_new (GTK_WINDOW(window),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_CLOSE,
					"Sysctl hw.snd.default_unit: %s",
					g_strerror (errno));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
		}
	}
	else
	{
		sndunitnw=0;
		DPRINT("Phonehead disable = %d\n", sndunitnw);
		if (sysctlbyname("hw.snd.default_unit", &sndunit, &len, &fconfig.ounit, sizeof(&sndunitnw)) < 0) {
			perror("Sysctl hw.snd.default_unit");
			dialog = gtk_message_dialog_new (GTK_WINDOW(window),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_CLOSE,
					"Sysctl hw.snd.default_unit: %s",
					g_strerror (errno));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
		}
	}
}

static
int save_config(gpointer set_window)
{
	FILE *conf;
	int status;

	conf = fopen(fconfig.directory, "wb");

	bzero(fconfig.device, strlen(fconfig.device));
	strcpy(fconfig.device,gtk_entry_get_text(GTK_ENTRY(devEntry)));
	fconfig.ounit = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(outEntry));
	fconfig.punit = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(phoneEntry));
	fconfig.phonesysctl = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(phoneCb));
	gtk_color_selection_get_current_color (GTK_COLOR_SELECTION(ColorSelect), &fconfig.ncolor);
	bzero(fconfig.nfont, strlen(fconfig.nfont));
	strcpy(fconfig.nfont, gtk_font_selection_get_font_name(GTK_FONT_SELECTION(FontSelect)));
	fprintf(conf,"[general]\ndevice = %s\n\n[mixer]\nphonehead = %d\nphoneviasysctl = %d\nout = %d\n\n[window]\npixel = %d\nred = %d\ngreen = %d\nblue = %d\nfont = %s\n", fconfig.device, fconfig.punit, fconfig.phonesysctl, fconfig.ounit, fconfig.ncolor.pixel, fconfig.ncolor.red, fconfig.ncolor.green, fconfig.ncolor.blue, fconfig.nfont);

	status = fclose(conf);

	gtk_widget_modify_bg(GTK_WIDGET(settings_window), GTK_STATE_NORMAL, &fconfig.ncolor);
	gtk_widget_modify_bg(GTK_WIDGET(window), GTK_STATE_NORMAL, &fconfig.ncolor);
	set_app_font(fconfig.nfont);
	if (sysctlbyname("hw.snd.default_unit", &sndunit, &len, &fconfig.ounit, sizeof(&sndunitnw)) < 0) {
		perror("Sysctl hw.snd.default_unit");
		dialog = gtk_message_dialog_new (GTK_WINDOW(settings_window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				"Sysctl hw.snd.default_unit: %s",
				g_strerror (errno));

		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
	return status;
}

static
void SettingsActivated (GObject *trayicon, gpointer window)
{

	GtkWidget *             settings_table;
	GtkWidget *             settings_table_p2;
	GtkWidget *             settings_table_p3;
	GtkWidget *             devLabel;
	GtkWidget *             phoneLabel;
	GtkWidget *             phoneCbLabel;

	GtkWidget *             outLabel;
	GtkWidget *             FPLabel;
	GtkWidget *             colorLabel;
	GtkWidget *             persLabel;
	GtkWidget *             fontLabel;
	GtkWidget *             GeneralPageLabel;
	GtkWidget *             GeneralPage;
	GtkWidget *             CloseButton;
	GtkWidget *             SaveButton;
	GtkWidget *             SaveFixed;
	GtkWidget *             CloseFixed;
	GtkWidget *		HeadNote;
	GtkWidget *		PersNote1Label;
	GtkWidget *		PersNote2Label;
	GtkWidget *		pVBox;
	GtkWidget *		pHBox;

	DPRINT("[general] ; device = %s ; [mixer] ; phonehead = %d ; out = %d\n", fconfig.device, fconfig.punit, fconfig.ounit);

	settings_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (settings_window), "GTMixer - Settings");
	gtk_container_set_border_width (GTK_CONTAINER (settings_window), 10);
	gtk_window_set_resizable(GTK_WINDOW (settings_window), FALSE);
	gtk_widget_set_size_request (settings_window, 725, 370);
	gtk_window_set_position (GTK_WINDOW (settings_window),GTK_WIN_POS_CENTER);

	gtk_widget_modify_bg(GTK_WIDGET(settings_window), GTK_STATE_NORMAL, &fconfig.ncolor);


	GeneralPageLabel = gtk_label_new("General");
	PersNote1Label = gtk_label_new("Window");
	PersNote2Label = gtk_label_new("Font");	

	pVBox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(settings_window), pVBox);

	/* Creation du GtkNotebook */
	HeadNote = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(pVBox), HeadNote, TRUE, TRUE, 0);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(HeadNote), TRUE);

	settings_table = gtk_table_new(7, 6, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(settings_table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(settings_table), 5);

	gtk_notebook_append_page(GTK_NOTEBOOK(HeadNote), settings_table, GeneralPageLabel);

	devLabel = gtk_label_new("Mixer device: ");
	gtk_table_attach_defaults(GTK_TABLE(settings_table), devLabel, 0, 1, 0, 1);

	devEntry = gtk_entry_new_with_max_length(55);
	gtk_entry_set_text(GTK_ENTRY(devEntry), fconfig.device);
	gtk_table_attach_defaults(GTK_TABLE(settings_table), devEntry, 1, 6, 0, 1);
	
	phoneCbLabel = gtk_label_new("Head Phone via sysctl: ");
	gtk_table_attach_defaults(GTK_TABLE(settings_table), phoneCbLabel, 0, 1, 1, 2);
	
	phoneCb = gtk_check_button_new();
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(phoneCb), fconfig.phonesysctl);
	gtk_table_attach_defaults(GTK_TABLE(settings_table), phoneCb, 1, 2, 1, 2);


	FPLabel = gtk_label_new("Front Panel unit: ");
	gtk_table_attach_defaults(GTK_TABLE(settings_table), FPLabel, 0, 1, 2, 3);

	FPEntry = gtk_spin_button_new_with_range(0, 10, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(FPEntry), fconfig.fp);
	gtk_table_attach_defaults(GTK_TABLE(settings_table), FPEntry, 1, 2, 2, 3);


	phoneLabel = gtk_label_new("Head Phone Unit: ");
	gtk_table_attach_defaults(GTK_TABLE(settings_table), phoneLabel, 2, 3, 2, 3);

	phoneEntry = gtk_spin_button_new_with_range(0, 10, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(phoneEntry), fconfig.punit);
	gtk_table_attach_defaults(GTK_TABLE(settings_table), phoneEntry, 3, 4, 2, 3);

	outLabel = gtk_label_new("Sound out Unit: ");
	gtk_table_attach_defaults(GTK_TABLE(settings_table), outLabel, 4, 5, 2, 3);

	outEntry = gtk_spin_button_new_with_range(0, 10, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(outEntry), fconfig.ounit);
	gtk_table_attach_defaults(GTK_TABLE(settings_table), outEntry, 5, 6, 2, 3);

	settings_table_p2 = gtk_table_new(7, 6, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(settings_table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(settings_table), 5);

	gtk_notebook_append_page(GTK_NOTEBOOK(HeadNote), settings_table_p2, PersNote1Label);

	colorLabel = gtk_label_new("Window color: ");
	gtk_table_attach_defaults(GTK_TABLE(settings_table_p2), colorLabel, 0, 1, 1, 2);

	ColorSelect = gtk_color_selection_new();
	gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(ColorSelect), &fconfig.ncolor);
	gtk_table_attach(GTK_TABLE(settings_table_p2), ColorSelect, 1, 6, 2, 3, 
			GTK_FILL | GTK_EXPAND | GTK_SHRINK,
			GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);

	settings_table_p3 = gtk_table_new(7, 6, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(settings_table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(settings_table), 5);

	gtk_notebook_append_page(GTK_NOTEBOOK(HeadNote), settings_table_p3, PersNote2Label);

	fontLabel = gtk_label_new("Font: ");
	gtk_table_attach_defaults(GTK_TABLE(settings_table_p3), fontLabel, 0, 1, 5, 6);

	FontSelect = gtk_font_selection_new();
	DPRINT("Current Font=\"%s\"\n", fconfig.nfont);
	gtk_table_attach(GTK_TABLE(settings_table_p3), FontSelect, 1, 6, 5, 6, 
			GTK_FILL | GTK_EXPAND | GTK_SHRINK,
			GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
	gtk_font_selection_set_font_name(GTK_FONT_SELECTION(FontSelect), fconfig.nfont); 

	pHBox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(pVBox), pHBox, TRUE, TRUE, 0);

	SaveFixed = gtk_fixed_new();
	gtk_box_pack_start(GTK_BOX(pHBox), SaveFixed, TRUE, TRUE, 0);
	SaveButton = gtk_button_new_with_label("Save");
	gtk_button_set_alignment(GTK_BUTTON(SaveButton), 50, 5);
	gtk_widget_set_size_request(SaveButton, 80, 35);
	gtk_button_set_relief(GTK_BUTTON(SaveButton), GTK_RELIEF_HALF);
	gtk_fixed_put(GTK_FIXED(SaveFixed), SaveButton, 50, 25);


	CloseFixed = gtk_fixed_new();
	gtk_box_pack_start(GTK_BOX(pHBox), CloseFixed, TRUE, FALSE, 0);
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

void exit_mixer_function(gpointer window)
{
	gtk_widget_destroy(mixer_window);
	mixer_window=NULL;
}

static
void MixerActivated (GObject *trayicon, gpointer window)
{

	GtkWidget *             mixer_table;
	GtkWidget *             mixer_frame[100];
	GtkWidget *		VolImg;
	GtkWidget *		PcmImg;
	gdouble marks[3] = { 0, 75, 100 };
	struct mixerhash *mixerh;
	int tablesize;
	tablesize = 0;

	clear_mixer_hash();
	get_mixer_unit();

	mixer_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (mixer_window), "GTMixer - Mixer");
	gtk_container_set_border_width (GTK_CONTAINER (mixer_window), 5);
	gtk_window_set_resizable(GTK_WINDOW (mixer_window), FALSE);
	gtk_widget_set_size_request (mixer_window, 300, tablesize);
	gtk_window_set_position (GTK_WINDOW (mixer_window),GTK_WIN_POS_CENTER);

	gtk_widget_modify_bg(GTK_WIDGET(mixer_window), GTK_STATE_NORMAL, &fconfig.ncolor);

	mixer_table = gtk_table_new(1, 4, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(mixer_table), 1);
	gtk_table_set_col_spacings(GTK_TABLE(mixer_table), 1);
	gtk_container_add(GTK_CONTAINER(mixer_window), mixer_table);

	VolImg = gtk_image_new_from_file(VOLSET);
	PcmImg = gtk_image_new_from_file(PCMSET);
	gtk_image_set_pixel_size(GTK_IMAGE(VolImg), 5);
	gtk_image_set_pixel_size(GTK_IMAGE(PcmImg), 5);

	for (mixerh=mixerunits; mixerh != NULL; mixerh=(struct mixerhash*)mixerh->hh.next) {
			DPRINT("mixer name:%s mixer id: %d\n", mixerh->name, mixerh->id);
			mixer_frame[mixerh->id] = gtk_frame_new(mixerh->name);
			gtk_frame_set_shadow_type(GTK_FRAME(mixer_frame[mixerh->id]), GTK_SHADOW_ETCHED_IN);
			if (strncmp(mixerh->name,"vol",sizeof("vol"))==0)
				    gtk_table_attach_defaults(GTK_TABLE(mixer_table), VolImg, 0, 1, mixerh->id, (mixerh->id)+1);
			if (strncmp(mixerh->name,"pcm",sizeof("pcm"))==0)
				    gtk_table_attach_defaults(GTK_TABLE(mixer_table), PcmImg, 0, 1, mixerh->id, (mixerh->id)+1);
			gtk_table_attach_defaults(GTK_TABLE(mixer_table), mixer_frame[mixerh->id],1, 4, mixerh->id, (mixerh->id)+1);

			GdkColor color_scal;
			gdk_color_parse("#ECECEC", &color_scal);
			mixer_hscale[mixerh->id] = gtk_hscale_new_with_range(0, 100, 1); 
			gtk_scale_set_digits(GTK_SCALE(mixer_hscale[mixerh->id]), 0);
			gtk_widget_modify_bg(GTK_WIDGET(mixer_hscale[mixerh->id]), GTK_STATE_ACTIVE, &color_scal);
			gtk_scale_add_mark (GTK_SCALE (mixer_hscale[mixerh->id]), marks[0], GTK_POS_BOTTOM, labels[3]);
			gtk_scale_add_mark (GTK_SCALE (mixer_hscale[mixerh->id]), marks[1], GTK_POS_BOTTOM, labels[4]);
			gtk_scale_add_mark (GTK_SCALE (mixer_hscale[mixerh->id]), marks[2], GTK_POS_BOTTOM, labels[2]);
			gtk_scale_set_value_pos(GTK_SCALE(mixer_hscale[mixerh->id]), GTK_POS_LEFT);
			gtk_range_set_update_policy (GTK_RANGE (mixer_hscale[mixerh->id]),
					GTK_UPDATE_CONTINUOUS);

			gtk_scale_set_value_pos (GTK_SCALE(mixer_hscale[mixerh->id]), GTK_POS_LEFT);

			gtk_container_add(GTK_CONTAINER(mixer_frame[mixerh->id]), mixer_hscale[mixerh->id]);
			g_signal_connect (G_OBJECT (mixer_hscale[mixerh->id]), "value_changed",
				    G_CALLBACK (cb_digits_scale_global), (gpointer)mixerh->name);
			tablesize = tablesize+65;
	gtk_widget_set_size_request (mixer_window, 300, tablesize);
	}
	get_mixer_state_all();
	gtk_signal_connect (GTK_OBJECT (mixer_window), "delete-event",
				     G_CALLBACK(exit_mixer_function), (gpointer)mixer_window);

	gtk_widget_show_all (mixer_window);
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
	    DPRINT("is tray = %d\n", 1);
	}
	else
	{
	    gtk_window_deiconify(GTK_WINDOW(window));
	    is_tray=TRUE;
	    DPRINT("is tray = %d\n", 0);
	}
	if (fconfig.phonesysctl==1)
	{
		if (sysctlbyname("hw.snd.default_unit", &sndunit, &len, NULL, 0) < 0)
		{
			dialog = gtk_message_dialog_new (GTK_WINDOW(window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				"Sysctl hw.snd.default_unit: %s",
				g_strerror (errno));

			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
		}
		if (sndunit==fconfig.punit)
		{
			onlyget_sysctl=TRUE;
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkphone), 1);
			onlyget_sysctl=FALSE;	
		}
		else
		{
			onlyget_sysctl=TRUE;
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkphone), 0);
			onlyget_sysctl=FALSE;	
		}
	}
	DPRINT("Unit: %d activated\n", sndunit);
	g_signal_connect (window, "focus-out-event", G_CALLBACK (on_focus_out), NULL);
}

void
gui_loop()
{
	GParamSpec		*pspec;
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
	gdouble marks[3] = { 0, 75, 100 };

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

	gtk_widget_modify_bg(GTK_WIDGET(window), GTK_STATE_NORMAL, &fconfig.ncolor);

	table = gtk_table_new(3, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_container_add(GTK_CONTAINER(window), table);
	


	frame = gtk_frame_new("Volume");
	set_app_font(fconfig.nfont);
	g_signal_connect (frame, "focus-out-event", G_CALLBACK (on_focus_out), NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);

	frame2 = gtk_frame_new("Master");
	gtk_frame_set_shadow_type(GTK_FRAME(frame2), GTK_SHADOW_ETCHED_IN);




	VolImg = gtk_image_new_from_file(VOLSET);
	PcmImg = gtk_image_new_from_file(PCMSET);

	gtk_image_set_pixel_size(GTK_IMAGE(VolImg), 10);
	gtk_image_set_pixel_size(GTK_IMAGE(PcmImg), 10);


	gtk_table_attach_defaults(GTK_TABLE(table), VolImg, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(table), PcmImg, 0, 1, 1, 2);

	gtk_table_attach_defaults(GTK_TABLE(table), frame,1, 4, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(table), frame2, 1, 4, 1, 2);
	if (fconfig.phonesysctl==1)
	{
	    checkphone = gtk_check_button_new_with_label(labels[0]);
	    PhoneImg = gtk_image_new_from_file(PHONESET);
	    gtk_image_set_pixel_size(GTK_IMAGE(PhoneImg), 10);
	gtk_table_attach_defaults(GTK_TABLE(table), PhoneImg, 0, 1, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(table), checkphone, 1, 2, 2, 3);
	}
	else
	    gtk_widget_set_size_request (window, 280, 150);



	GdkColor color_scal;
	gdk_color_parse("#ECECEC", &color_scal);

	hscaleVol = gtk_hscale_new_with_range(0, 100, 1); 
	gtk_scale_set_digits(GTK_SCALE(hscaleVol), 0);
	gtk_widget_modify_bg(GTK_WIDGET(hscaleVol), GTK_STATE_ACTIVE, &color_scal);
	gtk_scale_add_mark (GTK_SCALE (hscaleVol), marks[0], GTK_POS_BOTTOM, labels[3]);
	gtk_scale_add_mark (GTK_SCALE (hscaleVol), marks[1], GTK_POS_BOTTOM, labels[4]);
	gtk_scale_add_mark (GTK_SCALE (hscaleVol), marks[2], GTK_POS_BOTTOM, labels[2]);
	gtk_scale_set_value_pos(GTK_SCALE(hscaleVol), GTK_POS_LEFT);
	gtk_range_set_update_policy (GTK_RANGE (hscaleVol),
			                                 GTK_UPDATE_CONTINUOUS);

	hscalePcm = gtk_hscale_new_with_range(0, 100, 1); 
	gtk_scale_set_digits(GTK_SCALE(hscalePcm), 0);
	gtk_widget_modify_bg(GTK_WIDGET(hscalePcm), GTK_STATE_ACTIVE, &color_scal);
	gtk_scale_add_mark (GTK_SCALE (hscalePcm), marks[0], GTK_POS_BOTTOM, labels[3]);
	gtk_scale_add_mark (GTK_SCALE (hscalePcm), marks[1], GTK_POS_BOTTOM, labels[4]);
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
	menuItemMix = gtk_menu_item_new_with_label ("Mixer");
	menuItemSet = gtk_menu_item_new_with_label ("Settings");
	Separator1 = gtk_separator_menu_item_new();
	menuItemExit = gtk_menu_item_new_with_label ("Exit");
	g_signal_connect (G_OBJECT (menuItemView), "activate", G_CALLBACK (trayView), window);
	g_signal_connect (G_OBJECT (menuItemMix), "activate", G_CALLBACK (MixerActivated), window);
	g_signal_connect (G_OBJECT (menuItemSet), "activate", GTK_SIGNAL_FUNC (SettingsActivated), window);
	g_signal_connect (G_OBJECT (menuItemExit), "activate", G_CALLBACK (trayExit), NULL);

	g_signal_connect(GTK_STATUS_ICON (trayIcon), "activate", GTK_SIGNAL_FUNC (trayIconActivated), window);
	g_signal_connect(GTK_STATUS_ICON (trayIcon), "popup-menu", GTK_SIGNAL_FUNC (trayIconPopup), menu);


	g_signal_connect (G_OBJECT (hscalePcm), "value_changed",
			                      G_CALLBACK (cb_digits_scale_pcm), NULL);
	g_signal_connect (G_OBJECT (hscaleVol), "value_changed",
			                      G_CALLBACK (cb_digits_scale_vol), NULL);


	g_signal_connect (table, "focus-out-event", G_CALLBACK (on_focus_out), NULL);
	
	if (fconfig.phonesysctl==1)
	{
		if (sysctlbyname("hw.snd.default_unit", &sndunit, &len, NULL, 0) < 0)
			printf("%d\n", errno);
		if (sndunit==fconfig.punit)
		{
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkphone), 1);
		}
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkphone), 0);
		g_signal_connect(G_OBJECT(checkphone), "clicked",
				G_CALLBACK(checkphone_toogle_signal), (gpointer) checkphone);
	}
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuItemView);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuItemMix);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuItemSet);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), Separator1);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuItemExit);
	gtk_widget_show_all (menu);

	/*timer*/
	g_timeout_add_seconds(1,(GSourceFunc)TimerFunc,trayIcon);

	gtk_main();
}

int
main(int argc, char *argv[], char *envp[])
{
	char	mixer[PATH_MAX];
	char	fconfdir[PATH_MAX];
	char	*name, *eptr;
	int	n;
	FILE *conf;
	int	ch, fd;

	while ((ch = getopt(argc, argv, "vd")) != -1) {
		switch (ch) {
			case 'd':
				debug = 1;
				break;
			case 'v':
				print_version(argv[0]);
				break;
			case '?':
			default:
				break;	
		}
	}
	argc -= optind;
	argv += optind;

	strcpy(fconfdir,getenv("HOME"));
	strcat(fconfdir,CONFIGFILE);
	strcpy(fconfig.directory,fconfdir);

	if(parseconfig(fconfdir))
		DPRINT("Config file %s is not valid\n", fconfdir);

	fconfig.punit=0;
	fconfig.ounit=0;
	fconfig.phonesysctl=0;

	// init config
	bzero(fconfig.device, MAXDEVLEN);
	bzero(fconfig.nfont, MAXFONTLEN);
	// check variables
	if (!get_variable("[general]", "device"))
	{
		DPRINT("not fount device in config - using %s device\n", DEFAULTDEV);
		//strcpy(device, DEFAULTDEV);
		strcpy(fconfig.device, DEFAULTDEV);
	}
	else
		strcpy(fconfig.device, get_variable("[general]", "device"));
	if (!get_variable("[window]", "font"))
	{
		printf("not fountd font in config - using %s font\n", DEFAULTFONT);
		//strcpy(font, DEFAULTFONT);
		strcpy(fconfig.nfont, DEFAULTFONT);
	}
	else
		strcpy(fconfig.nfont, get_variable("[window]", "font"));

	fconfig.ncolor.pixel = get_variable("[window]", "pixel")?atoi(get_variable("[window]", "pixel")):0;
	fconfig.ncolor.red=get_variable("[window]", "red")?atoi(get_variable("[window]", "red")):65535;
	fconfig.ncolor.green=get_variable("[window]", "green")?atoi(get_variable("[window]", "green")):65535;
	fconfig.ncolor.blue=get_variable("[window]", "blue")?atoi(get_variable("[window]", "blue")):65535;
	fconfig.punit = atoi(get_variable("[mixer]", "phonehead"));
	fconfig.ounit = atoi(get_variable("[mixer]", "out"));
	fconfig.phonesysctl = atoi(get_variable("[mixer]", "phoneviasysctl"));



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
		fprintf(stderr, gettext("gtmixer: cannot open display\n"));
		exit(1);
	}
	get_mixer_unit();

	gui_loop();

	return (0);
}
