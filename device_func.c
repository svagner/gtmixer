/*
 *	This is mixer program for FreeBSD
 *
 * (C) Stanislav Putrya.
 *
 * You may do anything you wish with this program.
 *
 */
#include <sys/cdefs.h>
#include "gtmixer.h"

const char* names[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_NAMES;
struct mixerhash *mixerunits = NULL;

int
get_mixer_unit_by_name(const char *mixname)
{
	struct mixerhash *mixerh;
	mixerh = malloc(sizeof(struct mixerhash));
	HASH_FIND_STR(mixerunits, mixname, mixerh);
	if (mixerh)
	    return mixerh->id;
	return -1;
}

int 
handler(void* user, const char* section, const char* name, const char* value)
{
	configuration* pconfig = (configuration*)user;

	if (MATCH("glogal", "version")) {
		pconfig->version = strdup(value);
	} else if (MATCH("general", "device")) {
		pconfig->device = strdup(value);
	} else if (MATCH("mixer", "phonehead")) {
		pconfig->phone_unit = atoi(value);
	} else if (MATCH("mixer", "phoneviasysctl")) {
		pconfig->phonesysctl = atoi(value);
	} else if (MATCH("mixer", "out")) {
		pconfig->out_unit = atoi(value);
	} else if (MATCH("window", "pixel")) {
		fconfig.ncolor.pixel = atoi(value);
	} else if (MATCH("window", "red")) {
		fconfig.ncolor.red = atoi(value);
	} else if (MATCH("window", "green")) {
		fconfig.ncolor.green = atoi(value);
	} else if (MATCH("window", "blue")) {
		fconfig.ncolor.blue = atoi(value);
	} else if (MATCH("window", "font")) {
		pconfig->font = strdup(value);
	} else {
		return 0;  /* unknown section/name, error */
	}

	return 1;
}

void
clear_mixer_hash ()
{
	struct mixerhash *mixerh, *tmp;

	HASH_ITER(hh, mixerunits, mixerh, tmp) {
		HASH_DEL(mixerunits,mixerh);  /* delete it (users advances to next) */
		free(mixerh);            /* free it */
	}
}

void 
get_mixer_unit ()
{
	char	mixer[PATH_MAX];
	char	lstr[5], rstr[5];
	char	*name, *eptr;
	int	devmask = 0, recmask = 0, recsrc = 0, orecsrc;
	int	dusage = 0, drecsrc = 0, sflag = 0, Sflag = 0;
	int	frame_unit, foo, bar, baz, n;
	struct mixerhash *mixerh, *tmp;

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
	frame_unit=0;

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
			// add to hash table
			mixerh = malloc(sizeof(struct mixerhash));
			strcpy(mixerh->name, names[foo]);
			mixerh->id = frame_unit;
			HASH_ADD_STR( mixerunits, name, mixerh );
			frame_unit++;
		}
	}
	close(baz);
}

int
res_name(const char *name, int mask)
{
	int	foo;

	for (foo = 0; foo < SOUND_MIXER_NRDEVICES; foo++)
		if ((1 << foo) & mask && strcmp(names[foo], name) == 0)
			break;

	return (foo == SOUND_MIXER_NRDEVICES ? -1 : foo);
}

void
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

void
get_mixer_state_all()
{
	char	mixer[PATH_MAX];
	char	lstr[5], rstr[5];
	char	*name, *eptr;
	int	devmask = 0, recmask = 0, recsrc = 0, orecsrc;
	int	dusage = 0, drecsrc = 0, sflag = 0, Sflag = 0;
	int	l, r, lrel, rrel, mixu;
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
			mixu = get_mixer_unit_by_name(names[foo]);

			if (mixu != -1)
			{
			    gtk_range_set_value(GTK_RANGE (mixer_hscale[mixu]), bar & 0x7f);
			}
		}
	}
	close(baz);
}

int
set_mixer_state(char * mixprm, int st)
{
	char	mixer[PATH_MAX];
	char	lstr[5], rstr[5];
	char	*name, *eptr;
	int	devmask = 0, recmask = 0, recsrc = 0, orecsrc;
	int	dusage = 0, drecsrc = 0, sflag = 0, Sflag = 0;
	int	l, r, lrel, rrel;
	int	bar, baz, dev, m, n, t;

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
			    DPRINT("Setting the mixer %s to %d:%d"
				    ".\n", names[dev], bar & 0x7f,
				    (bar >> 8) & 0x7f);
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
