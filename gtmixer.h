#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>

#include <gtk/gtk.h>

#include "uthash.h"
#include "ini.h"

#include <sys/types.h>
#include <sys/soundcard.h>

#include <libgen.h>

#ifndef DEBUG
#define DEBUG 0
#endif

#define gettext(x)      (x)
#define MATCH(s, n)	strcmp(section, s) == 0 && strcmp(name, n) == 0

#define DPRINT(fmt, ...) do { if (DEBUG) fprintf(stderr, "[DEBUG] [%s:%d] %s(): " fmt, __FILE__, \
					    __LINE__, __func__, __VA_ARGS__); } while (0)

#define MAXMIXUNIT 	100
#define SHAREPATH	"/usr/local/share/gtmixer/"
#define CONFIGFILE	"/.gtmixerrc"
#define DEFAULTDEV	"/dev/mixer"
#define DEFAULTFONT	"Sans 8"
#define PANEL_Y_SIZE	25
#define TRAY_VOLMUTE	"/usr/local/share/gtmixer/icons/tray/1.png"
#define TRAY_DEMUTE	"/usr/local/share/gtmixer/icons/tray/2.png"
#define TRAY_INMUTE	"/usr/local/share/gtmixer/icons/tray/3.png"
#define VOLSET		"/usr/local/share/gtmixer/icons/tray/273.png"
#define PCMSET		"/usr/local/share/gtmixer/icons/tray/431.png"
#define PHONESET	"/usr/local/share/gtmixer/icons/tray/430.png"


int		sndunit, sndunitnw, volstate, pcmstate;
char		device[100];
char		font[100];
int		mixstate[MAXMIXUNIT];

typedef struct
{
	const char* version;
	const char* device;
	const char* font;
	GdkColor wincolor;
	int phone_unit;
	int out_unit;
	int fp;
	int phonesysctl;
} configuration;

struct 
{
	char directory[255];	/* store the config path		*/
	char device[55];	/* device for output sound (/dev/mixer)	*/
	int fp;			/* front panel unit output		*/
	int punit;		/* phonehead unit output		*/
	int ounit;		/* primary sound output unit		*/
	GdkColor ncolor;	/* declare colour for form		*/
	gchar nfont[100];	/* declare font for form		*/
	int phonesysctl;	/* switch to head phone via sysctl(8)	*/
} fconfig;			/* global config struct			*/

struct mixerhash {
	char name[30];		/* mixer value (vol, pcm, etc.) as the key  */        
	int id;            
	UT_hash_handle hh;	/* makes this structure hashable	    */
};

GtkWidget*		checkphone;
GtkWidget*		menu;
GtkWidget*		menuItemView;
GtkWidget*		menuItemMix;
GtkWidget*		menuItemSet;
GtkWidget*		Separator1;
GtkWidget*		menuItemExit;
GtkWidget*		hscaleVol; 
GtkWidget*		hscalePcm; 
GtkWidget*              mixer_hscale[MAXMIXUNIT];
GtkWidget*		devEntry;
GtkWidget*		FPEntry;
GtkWidget*		phoneEntry;
GtkWidget*              phoneCb;
GtkWidget*		outEntry;
GtkWidget*		ColorSelect;
GtkWidget*		FontSelect;
GtkWidget*		settings_window;
GtkWidget*		mixer_window;
GtkWidget*		window;
GtkWidget*		dialog;
GtkStatusIcon*		trayIcon;
GtkBuilder*		builder;
GtkWidget*		app;

const gchar *labels[4] = {
	"Head Phones enable",
	"<span size='small'>Baby bath</span>",
	"<span color='Blue' size='small'>Max</span>",
	"<span color='Red' size='small'>Mute</span>"
};


void usage(int devmask, int recmask);
int  gui_init();

void gui_loop();
void set_app_font (const char *fontname);
int get_mixer_state(char * mixprm);
void get_mixer_state_all ();
void get_mixer_unit ();
void clear_mixer_hash ();
int set_mixer_state(char * mixprm, int st);
void on_popup_clicked (GtkButton*, GtkWidget*);
void on_popup_clicked (GtkButton*, GtkWidget*);
void on_focus_out (GtkWidget* window);
void checkphone_toogle_signal(GtkWidget *widget, gpointer window);
gboolean * TimerFunc (gpointer);
void cb_digits_scale_vol(GtkWidget *widget, gpointer window);
void cb_digits_scale_pcm(GtkWidget *widget, gpointer window);
gboolean on_popup_window_event(GtkWidget*, GdkEventExpose*);
int get_mixer_unit_by_name(const char *mixname);

static void destroy (GtkWidget *window, gpointer data);
static void trayIconActivated(GObject *trayicon, gpointer window);
static void trayView(GtkMenuItem *item, gpointer window);
static void trayExit(GtkMenuItem *item, gpointer user_data);
static void trayIconPopup(GtkStatusIcon *status_icon, guint button, guint32 activate_time, gpointer popUpMenu);

static int      res_name(const char *name, int mask);
static void     print_recsrc(int recsrc, int recmask, int sflag);

int handler(void* user, const char* section, const char* name, const char* value);
