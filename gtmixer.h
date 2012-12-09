#define MAXMIXUNIT 100

gint	is_tray=FALSE;
int	sndunit, sndunitnw, volstate, pcmstate, mixer_desc=0;
size_t	len = sizeof(sndunit);
char	device[100];
char	font[100];
int	vol_ischanged=FALSE, pcm_ischanged=FALSE;
int	mixstate[MAXMIXUNIT];


typedef struct
{
	const char* version;
	const char* device;
	const char* font;
	GdkColor wincolor;
	int phone_unit;
	int out_unit;
} configuration;

struct 
{
	char directory[255];
	char device[55];
	int fp;
	int punit;
	int ounit;
	GdkColor ncolor;
	gchar nfont[100];
} fconfig;

struct mixerhash {
	char name[30];     /* we'll use this field as the key */        
	int id;            
	UT_hash_handle hh; /* makes this structure hashable */
};

struct mixerhash *mixerunits = NULL;

GtkWidget *		checkphone;
GtkWidget *		menu;
GtkWidget *		menuItemView;
GtkWidget *		menuItemMix;
GtkWidget *		menuItemSet;
GtkWidget *		Separator1;
GtkWidget *		menuItemExit;
GtkWidget*		hscaleVol; 
GtkWidget*		hscalePcm; 
GtkWidget *             mixer_hscale[MAXMIXUNIT];
GtkWidget*		devEntry;
GtkWidget*		FPEntry;
GtkWidget*		phoneEntry;
GtkWidget*		outEntry;
GtkWidget *		ColorSelect;
GtkWidget *		FontSelect;
GtkWidget *		settings_window;
GtkWidget *		mixer_window;
GtkWidget *             window;
GtkStatusIcon *		trayIcon;




extern int gui_init();
static void destroy (GtkWidget *window, gpointer data);
extern void gui_loop();
void set_app_font (const char *fontname);
extern int get_mixer_state(char * mixprm);
extern void get_mixer_state_all ();
extern int set_mixer_state(char * mixprm, int st);
static void trayIconActivated(GObject *trayicon, gpointer window);
static void on_popup_clicked (GtkButton*, GtkWidget*);
static void on_popup_clicked (GtkButton*, GtkWidget*);
extern void on_focus_out (GtkWidget* window);
extern void checkphone_toogle_signal(GtkWidget *widget, gpointer window);
static void trayView(GtkMenuItem *item, gpointer window);
static void trayExit(GtkMenuItem *item, gpointer user_data);
static void trayIconPopup(GtkStatusIcon *status_icon, guint button, guint32 activate_time, gpointer popUpMenu);
extern gboolean * TimerFunc (gpointer);
extern void cb_digits_scale_vol(GtkWidget *widget, gpointer window);
extern void cb_digits_scale_pcm(GtkWidget *widget, gpointer window);
static gboolean on_popup_window_event(GtkWidget*, GdkEventExpose*);

#define SHAREPATH "/usr/local/share/gtmixer/"
#define CONFIGFILE "/.gtmixerrc"
#define DEFAULTDEV "/dev/mixer"

#define DEFAULTFONT "Sans 8"

#define PANEL_Y_SIZE 25
#define TRAY_VOLMUTE "/usr/local/share/gtmixer/icons/tray/1.png"
#define TRAY_DEMUTE "/usr/local/share/gtmixer/icons/tray/2.png"
#define TRAY_INMUTE "/usr/local/share/gtmixer/icons/tray/3.png"
#define VOLSET "/usr/local/share/gtmixer/icons/tray/273.png"
#define PCMSET "/usr/local/share/gtmixer/icons/tray/431.png"
#define PHONESET "/usr/local/share/gtmixer/icons/tray/430.png"
